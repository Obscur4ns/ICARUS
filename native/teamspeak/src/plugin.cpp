#include <icarus/core/direct_voice_acoustics.hpp>
#include <icarus/core/identity_protocol.hpp>
#include <icarus/core/session_state_protocol.hpp>
#include <icarus/core/spatial_scene_protocol.hpp>
#include <icarus/core/version.hpp>
#include <icarus/ipc/bridge.hpp>
#include <icarus/ipc/session_state.hpp>
#include <icarus/ipc/spatial_scene.hpp>

#include <teamspeak/public_definitions.h>
#include <teamspeak/public_errors.h>
#include <teamspeak/public_rare_definitions.h>
#include <ts3_functions.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
constexpr int plugin_api_version = 26;
constexpr std::chrono::milliseconds state_publish_interval{100};
constexpr std::chrono::milliseconds acoustic_update_interval{20};
constexpr std::chrono::milliseconds acoustic_scene_stale_timeout{750};
constexpr std::chrono::milliseconds acoustic_prediction_limit{250};
constexpr float acoustic_position_smoothing = 0.35F;
constexpr std::size_t teamspeak_client_slots = 65536;

struct TS3Functions ts3_functions
{
};

icarus::ipc::BridgeEndpoint bridge(icarus::ipc::BridgeRole::teamspeak);
icarus::ipc::SessionStateChannel session_state(icarus::ipc::BridgeRole::teamspeak);
icarus::ipc::SpatialSceneChannel spatial_scene(icarus::ipc::BridgeRole::teamspeak);

std::mutex voice_state_mutex;
icarus::core::VoiceBackendSessionState voice_state{};
std::jthread state_worker;
std::jthread acoustic_worker;

std::array<std::atomic<std::uint8_t>, teamspeak_client_slots> acoustic_voice_levels{};
std::atomic_bool acoustics_active{false};
std::atomic_bool acoustic_scene_fresh{false};
std::atomic<std::uint64_t> acoustic_connection_id{0};
std::atomic<std::uint64_t> acoustic_scene_sequence{0};

std::mutex plugin_id_mutex;
std::string plugin_id;

std::atomic_bool local_talking{false};
std::atomic_bool hello_pending{true};

struct ClientIdentity
{
    std::string teamspeak_uid;
    std::string player_uid;
    std::string network_id;
    bool talking{};
};

struct LocalIdentity
{
    std::uint64_t connection_id{};
    std::uint64_t session_id{};
    anyID client_id{};
    std::string teamspeak_uid;
    std::string player_uid;
    std::string network_id;
    bool valid{};
    bool announced{};
};

using IdentityKey = std::pair<std::uint64_t, unsigned int>;

std::mutex identity_mutex;
std::map<IdentityKey, ClientIdentity> identities;
LocalIdentity local_identity{};

IdentityKey identity_key(std::uint64_t connection_id, anyID client_id) noexcept
{
    return {connection_id, static_cast<unsigned int>(client_id)};
}

void log_message(const char* message, LogLevel level)
{
    if(ts3_functions.logMessage != nullptr)
    {
        ts3_functions.logMessage(message, level, "ICARUS", 0);
    }
}

icarus::core::VoiceBackendConnectionState map_connection_state(int status) noexcept
{
    switch(status)
    {
        case STATUS_CONNECTION_ESTABLISHED:
            return icarus::core::VoiceBackendConnectionState::established;

        case STATUS_CONNECTING:
        case STATUS_CONNECTED:
        case STATUS_CONNECTION_ESTABLISHING:
            return icarus::core::VoiceBackendConnectionState::connecting;

        case STATUS_DISCONNECTED:
        default:
            return icarus::core::VoiceBackendConnectionState::disconnected;
    }
}

icarus::core::VoiceBackendSessionState read_voice_state()
{
    std::scoped_lock lock(voice_state_mutex);
    return voice_state;
}

std::string read_client_string(
    std::uint64_t connection_id,
    anyID client_id,
    std::size_t property
)
{
    if(ts3_functions.getClientVariableAsString == nullptr
        || ts3_functions.freeMemory == nullptr)
    {
        return {};
    }

    char* value{};

    if(ts3_functions.getClientVariableAsString(
           connection_id,
           client_id,
           property,
           &value
       ) != ERROR_ok
        || value == nullptr)
    {
        return {};
    }

    std::string result{value};
    ts3_functions.freeMemory(value);
    return result;
}

void set_health(icarus::core::VoiceBackendHealth health)
{
    std::scoped_lock lock(voice_state_mutex);
    voice_state.health = static_cast<std::uint32_t>(health);
}

void clear_connection_identities(std::uint64_t connection_id)
{
    std::scoped_lock lock(identity_mutex);

    for(auto iterator = identities.begin(); iterator != identities.end();)
    {
        if(iterator->first.first == connection_id)
        {
            iterator = identities.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }

    if(local_identity.connection_id == connection_id)
    {
        local_identity = {};
    }
}

void remove_identity(std::uint64_t connection_id, anyID client_id)
{
    std::scoped_lock lock(identity_mutex);
    identities.erase(identity_key(connection_id, client_id));
}

void upsert_identity(
    std::uint64_t connection_id,
    anyID client_id,
    std::string_view teamspeak_uid,
    std::string_view player_uid,
    std::string_view network_id
)
{
    std::scoped_lock lock(identity_mutex);
    auto& identity = identities[identity_key(connection_id, client_id)];
    identity.teamspeak_uid = teamspeak_uid;
    identity.player_uid = player_uid;
    identity.network_id = network_id;
}

void set_identity_talking(
    std::uint64_t connection_id,
    anyID client_id,
    bool talking
)
{
    std::scoped_lock lock(identity_mutex);
    const auto iterator = identities.find(identity_key(connection_id, client_id));

    if(iterator != identities.end())
    {
        iterator->second.talking = talking;
    }
}

void update_connection(std::uint64_t connection_id, int status)
{
    std::uint64_t previous_connection{};

    {
        std::scoped_lock lock(voice_state_mutex);
        previous_connection = voice_state.connection_id;
        voice_state.connection_id = connection_id;
        voice_state.connection_state =
            static_cast<std::uint32_t>(map_connection_state(status));
        voice_state.local_client_id = 0;

        if(status == STATUS_CONNECTION_ESTABLISHED
            && connection_id != 0
            && ts3_functions.getClientID != nullptr)
        {
            anyID client_id{};

            if(ts3_functions.getClientID(connection_id, &client_id) == ERROR_ok)
            {
                voice_state.local_client_id = client_id;
            }
        }
    }

    if(previous_connection != 0 && previous_connection != connection_id)
    {
        clear_connection_identities(previous_connection);
    }

    if(status == STATUS_CONNECTION_ESTABLISHED)
    {
        hello_pending = true;
    }
    else if(connection_id != 0)
    {
        clear_connection_identities(connection_id);
    }
}

void refresh_current_connection()
{
    if(ts3_functions.getCurrentServerConnectionHandlerID == nullptr)
    {
        update_connection(0, STATUS_DISCONNECTED);
        return;
    }

    const std::uint64_t connection_id =
        ts3_functions.getCurrentServerConnectionHandlerID();

    if(connection_id == 0 || ts3_functions.getConnectionStatus == nullptr)
    {
        update_connection(connection_id, STATUS_DISCONNECTED);
        return;
    }

    int status = STATUS_DISCONNECTED;

    if(ts3_functions.getConnectionStatus(connection_id, &status) != ERROR_ok)
    {
        update_connection(connection_id, STATUS_DISCONNECTED);
        return;
    }

    update_connection(connection_id, status);
}

bool update_local_identity(
    const icarus::ipc::StateSnapshot<icarus::core::ArmaSessionState>& arma,
    const icarus::core::VoiceBackendSessionState& voice
)
{
    const std::string player_uid{arma.payload.player_uid};
    const std::string network_id{arma.payload.network_id};
    const anyID client_id = static_cast<anyID>(voice.local_client_id);
    const std::string teamspeak_uid =
        read_client_string(voice.connection_id, client_id, CLIENT_UNIQUE_IDENTIFIER);

    std::scoped_lock lock(identity_mutex);

    const bool changed =
        !local_identity.valid
        || local_identity.connection_id != voice.connection_id
        || local_identity.client_id != client_id
        || local_identity.session_id != arma.payload.session_id
        || local_identity.teamspeak_uid != teamspeak_uid
        || local_identity.player_uid != player_uid
        || local_identity.network_id != network_id;

    if(changed)
    {
        local_identity = {
            .connection_id = voice.connection_id,
            .session_id = arma.payload.session_id,
            .client_id = client_id,
            .teamspeak_uid = teamspeak_uid,
            .player_uid = player_uid,
            .network_id = network_id,
            .valid = true,
            .announced = false,
        };
    }

    auto& self = identities[identity_key(voice.connection_id, client_id)];
    self.teamspeak_uid = teamspeak_uid;
    self.player_uid = player_uid;
    self.network_id = network_id;
    self.talking = local_talking.load();

    return changed;
}

void invalidate_local_identity()
{
    std::scoped_lock lock(identity_mutex);
    local_identity.valid = false;
    local_identity.announced = false;
}

LocalIdentity read_local_identity()
{
    std::scoped_lock lock(identity_mutex);
    return local_identity;
}

void mark_local_identity_announced(const LocalIdentity& identity)
{
    std::scoped_lock lock(identity_mutex);

    if(local_identity.valid
        && local_identity.connection_id == identity.connection_id
        && local_identity.client_id == identity.client_id
        && local_identity.session_id == identity.session_id
        && local_identity.player_uid == identity.player_uid
        && local_identity.network_id == identity.network_id)
    {
        local_identity.announced = true;
    }
}

std::string read_plugin_id()
{
    std::scoped_lock lock(plugin_id_mutex);
    return plugin_id;
}

bool is_our_plugin(const char* name)
{
    if(name == nullptr)
    {
        return false;
    }

    const std::string id = read_plugin_id();
    return !id.empty() && id == name;
}

bool send_local_identity(
    icarus::core::IdentityMessageKind kind,
    int target_mode,
    const anyID* target_ids
)
{
    if(ts3_functions.sendPluginCommand == nullptr)
    {
        return false;
    }

    const LocalIdentity identity = read_local_identity();
    const std::string id = read_plugin_id();

    if(!identity.valid || id.empty())
    {
        return false;
    }

    const std::string command = icarus::core::build_identity_message(
        kind,
        identity.player_uid,
        identity.network_id
    );

    if(command.empty())
    {
        return false;
    }

    ts3_functions.sendPluginCommand(
        identity.connection_id,
        id.c_str(),
        command.c_str(),
        target_mode,
        target_ids,
        nullptr
    );

    return true;
}

bool local_identity_announced()
{
    std::scoped_lock lock(identity_mutex);
    return local_identity.valid && local_identity.announced;
}


std::uint32_t count_matched_actors(const icarus::core::SpatialSceneState& scene)
{
    std::scoped_lock lock(identity_mutex);
    std::uint32_t matched{};

    const std::uint32_t actor_count = std::min(
        scene.actor_count,
        static_cast<std::uint32_t>(icarus::core::spatial_scene_max_actors)
    );

    for(std::uint32_t actor_index = 0; actor_index < actor_count; ++actor_index)
    {
        const auto& actor = scene.actors[actor_index];

        for(const auto& entry : identities)
        {
            const auto& identity = entry.second;

            if(identity.player_uid == actor.player_uid
                && identity.network_id == actor.network_id)
            {
                ++matched;
                break;
            }
        }
    }

    return matched;
}


struct AcousticIdentity
{
    anyID client_id{};
    std::string player_uid;
    std::string network_id;
};

struct RenderedSource
{
    TS3_VECTOR position{};
    std::uint64_t seen_cycle{};
    bool initialised{};
};

std::vector<AcousticIdentity> read_remote_identities(
    std::uint64_t connection_id,
    anyID local_client_id
)
{
    std::vector<AcousticIdentity> result;
    std::scoped_lock lock(identity_mutex);
    result.reserve(identities.size());

    for(const auto& entry : identities)
    {
        if(entry.first.first != connection_id
            || entry.first.second == static_cast<unsigned int>(local_client_id))
        {
            continue;
        }

        const auto& identity = entry.second;

        if(identity.player_uid.empty() || identity.network_id.empty())
        {
            continue;
        }

        result.push_back({
            .client_id = static_cast<anyID>(entry.first.second),
            .player_uid = identity.player_uid,
            .network_id = identity.network_id,
        });
    }

    return result;
}

const icarus::core::SpatialActorState* find_spatial_actor(
    const icarus::core::SpatialSceneState& scene,
    std::string_view player_uid,
    std::string_view network_id
) noexcept
{
    const std::uint32_t actor_count = std::min(
        scene.actor_count,
        static_cast<std::uint32_t>(icarus::core::spatial_scene_max_actors)
    );

    for(std::uint32_t index = 0; index < actor_count; ++index)
    {
        const auto& actor = scene.actors[index];

        if(std::string_view{actor.player_uid} == player_uid
            && std::string_view{actor.network_id} == network_id)
        {
            return &actor;
        }
    }

    return nullptr;
}

TS3_VECTOR subtract_position(
    const icarus::core::SpatialActorState& source,
    const icarus::core::SpatialActorState& listener,
    float prediction_seconds
) noexcept
{
    const float source_x = source.position[0] + source.velocity[0] * prediction_seconds;
    const float source_y = source.position[1] + source.velocity[1] * prediction_seconds;
    const float source_z = source.position[2] + source.velocity[2] * prediction_seconds;

    const float listener_x = listener.position[0] + listener.velocity[0] * prediction_seconds;
    const float listener_y = listener.position[1] + listener.velocity[1] * prediction_seconds;
    const float listener_z = listener.position[2] + listener.velocity[2] * prediction_seconds;

    return {
        source_x - listener_x,
        source_z - listener_z,
        -(source_y - listener_y),
    };
}

float vector_length(const TS3_VECTOR& value) noexcept
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

TS3_VECTOR normalise_vector(const TS3_VECTOR& value, const TS3_VECTOR& fallback) noexcept
{
    const float length = vector_length(value);

    if(!std::isfinite(length) || length < 0.0001F)
    {
        return fallback;
    }

    return {value.x / length, value.y / length, value.z / length};
}

TS3_VECTOR cross_product(const TS3_VECTOR& left, const TS3_VECTOR& right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

TS3_VECTOR listener_forward(const icarus::core::SpatialActorState& actor) noexcept
{
    return normalise_vector(
        {
            actor.head_direction[0],
            actor.head_direction[2],
            -actor.head_direction[1],
        },
        {0.0F, 0.0F, -1.0F}
    );
}

TS3_VECTOR listener_up(const TS3_VECTOR& forward) noexcept
{
    constexpr TS3_VECTOR world_up{0.0F, 1.0F, 0.0F};
    const TS3_VECTOR right = normalise_vector(
        cross_product(forward, world_up),
        {1.0F, 0.0F, 0.0F}
    );

    return normalise_vector(
        cross_product(right, forward),
        world_up
    );
}

TS3_VECTOR smooth_vector(
    const TS3_VECTOR& current,
    const TS3_VECTOR& target,
    float factor
) noexcept
{
    return {
        current.x + (target.x - current.x) * factor,
        current.y + (target.y - current.y) * factor,
        current.z + (target.z - current.z) * factor,
    };
}

icarus::core::VoiceLevel acoustic_voice_level(std::uint32_t value) noexcept
{
    const auto level = static_cast<icarus::core::VoiceLevel>(value);
    return icarus::core::is_direct_voice_level(level)
        ? level
        : icarus::core::VoiceLevel::normal;
}

void reset_rendered_sources(
    std::uint64_t connection_id,
    std::map<unsigned int, RenderedSource>& rendered_sources
)
{
    constexpr TS3_VECTOR centred{};

    for(const auto& entry : rendered_sources)
    {
        const auto client_id = static_cast<anyID>(entry.first);
        acoustic_voice_levels[entry.first].store(0, std::memory_order_relaxed);

        if(connection_id != 0 && ts3_functions.channelset3DAttributes != nullptr)
        {
            static_cast<void>(ts3_functions.channelset3DAttributes(
                connection_id,
                client_id,
                &centred
            ));
        }
    }

    rendered_sources.clear();
}

void disable_acoustics(
    std::uint64_t connection_id,
    std::map<unsigned int, RenderedSource>& rendered_sources
)
{
    reset_rendered_sources(connection_id, rendered_sources);
    acoustics_active.store(false, std::memory_order_relaxed);
    acoustic_connection_id.store(0, std::memory_order_relaxed);
}

void run_acoustic_worker(std::stop_token stop_token)
{
    std::map<unsigned int, RenderedSource> rendered_sources;
    std::uint64_t rendered_connection{};
    std::uint64_t last_scene_sequence{};
    std::uint64_t render_cycle{};
    auto scene_observed_at = std::chrono::steady_clock::now();
    bool scene_observed{};
    TS3_VECTOR rendered_forward{0.0F, 0.0F, -1.0F};
    bool forward_initialised{};

    while(!stop_token.stop_requested())
    {
        const auto bridge_status = bridge.status();
        const auto voice = read_voice_state();
        const bool connection_ready =
            voice.connection_state == static_cast<std::uint32_t>(
                icarus::core::VoiceBackendConnectionState::established
            )
            && voice.connection_id != 0
            && voice.local_client_id != 0;

        if(!connection_ready
            || bridge_status.generation == 0
            || ts3_functions.systemset3DListenerAttributes == nullptr
            || ts3_functions.systemset3DSettings == nullptr
            || ts3_functions.channelset3DAttributes == nullptr)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            acoustic_scene_fresh.store(false, std::memory_order_relaxed);
            rendered_connection = 0;
            last_scene_sequence = 0;
            scene_observed = false;
            forward_initialised = false;
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        if(rendered_connection != voice.connection_id)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            rendered_connection = voice.connection_id;
            last_scene_sequence = 0;
            scene_observed = false;
            forward_initialised = false;

            if(ts3_functions.systemset3DSettings(
                   rendered_connection,
                   1.0F,
                   1.0F
               ) != ERROR_ok)
            {
                rendered_connection = 0;
                std::this_thread::sleep_for(acoustic_update_interval);
                continue;
            }
        }

        if(!spatial_scene.sync(bridge_status.generation))
        {
            disable_acoustics(rendered_connection, rendered_sources);
            acoustic_scene_fresh.store(false, std::memory_order_relaxed);
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        const auto scene = spatial_scene.read_arma();
        const auto now = std::chrono::steady_clock::now();

        if(!scene.valid)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            acoustic_scene_fresh.store(false, std::memory_order_relaxed);
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        if(!scene_observed || scene.sequence != last_scene_sequence)
        {
            scene_observed_at = now;
            last_scene_sequence = scene.sequence;
            scene_observed = true;
        }

        const auto scene_age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - scene_observed_at
        );

        if(scene_age > acoustic_scene_stale_timeout)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            acoustic_scene_fresh.store(false, std::memory_order_relaxed);
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        acoustic_scene_fresh.store(true, std::memory_order_relaxed);
        acoustic_scene_sequence.store(scene.sequence, std::memory_order_relaxed);

        const LocalIdentity local = read_local_identity();

        if(!local.valid || local.connection_id != rendered_connection)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        const auto* local_actor = find_spatial_actor(
            scene.payload,
            local.player_uid,
            local.network_id
        );

        if(local_actor == nullptr)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        const float prediction_seconds = static_cast<float>(std::min(
            scene_age,
            acoustic_prediction_limit
        ).count()) / 1000.0F;

        const TS3_VECTOR target_forward = listener_forward(*local_actor);
        rendered_forward = forward_initialised
            ? normalise_vector(
                smooth_vector(
                    rendered_forward,
                    target_forward,
                    acoustic_position_smoothing
                ),
                target_forward
            )
            : target_forward;
        forward_initialised = true;

        constexpr TS3_VECTOR listener_position{};
        const TS3_VECTOR up = listener_up(rendered_forward);

        if(ts3_functions.systemset3DListenerAttributes(
               rendered_connection,
               &listener_position,
               &rendered_forward,
               &up
           ) != ERROR_ok)
        {
            disable_acoustics(rendered_connection, rendered_sources);
            std::this_thread::sleep_for(acoustic_update_interval);
            continue;
        }

        ++render_cycle;
        const auto remote_identities = read_remote_identities(
            rendered_connection,
            local.client_id
        );

        for(const auto& identity : remote_identities)
        {
            const auto* actor = find_spatial_actor(
                scene.payload,
                identity.player_uid,
                identity.network_id
            );

            if(actor == nullptr)
            {
                continue;
            }

            const TS3_VECTOR target = subtract_position(
                *actor,
                *local_actor,
                prediction_seconds
            );
            auto& rendered = rendered_sources[static_cast<unsigned int>(identity.client_id)];
            rendered.position = rendered.initialised
                ? smooth_vector(
                    rendered.position,
                    target,
                    acoustic_position_smoothing
                )
                : target;
            rendered.initialised = true;
            rendered.seen_cycle = render_cycle;

            if(ts3_functions.channelset3DAttributes(
                   rendered_connection,
                   identity.client_id,
                   &rendered.position
               ) == ERROR_ok)
            {
                const auto level = acoustic_voice_level(actor->voice_level);
                acoustic_voice_levels[static_cast<unsigned int>(identity.client_id)].store(
                    static_cast<std::uint8_t>(level),
                    std::memory_order_relaxed
                );
            }
            else
            {
                acoustic_voice_levels[static_cast<unsigned int>(identity.client_id)].store(
                    0,
                    std::memory_order_relaxed
                );
            }
        }

        for(auto iterator = rendered_sources.begin(); iterator != rendered_sources.end();)
        {
            if(iterator->second.seen_cycle == render_cycle)
            {
                ++iterator;
                continue;
            }

            const auto client_id = static_cast<anyID>(iterator->first);
            acoustic_voice_levels[iterator->first].store(0, std::memory_order_relaxed);
            static_cast<void>(ts3_functions.channelset3DAttributes(
                rendered_connection,
                client_id,
                &listener_position
            ));
            iterator = rendered_sources.erase(iterator);
        }

        acoustic_connection_id.store(rendered_connection, std::memory_order_relaxed);
        acoustics_active.store(true, std::memory_order_relaxed);
        std::this_thread::sleep_for(acoustic_update_interval);
    }

    disable_acoustics(rendered_connection, rendered_sources);
    acoustic_scene_fresh.store(false, std::memory_order_relaxed);
    acoustic_scene_sequence.store(0, std::memory_order_relaxed);
}

void run_state_worker(std::stop_token stop_token)
{
    while(!stop_token.stop_requested())
    {
        const auto bridge_status = bridge.status();

        if(bridge_status.generation == 0)
        {
            spatial_scene.reset();
            session_state.reset();
            invalidate_local_identity();
            std::this_thread::sleep_for(state_publish_interval);
            continue;
        }

        if(session_state.sync(bridge_status.generation))
        {
            icarus::core::VoiceBackendSessionState state = read_voice_state();
            const auto arma = session_state.read_arma();

            if(arma.valid)
            {
                state.acknowledged_session_id = arma.payload.session_id;
                state.acknowledged_arma_sequence = arma.sequence;
            }
            else
            {
                state.acknowledged_session_id = 0;
                state.acknowledged_arma_sequence = 0;
            }

            state.state_protocol_major =
                icarus::core::session_state_protocol_major;
            state.state_protocol_minor =
                icarus::core::session_state_protocol_minor;
            state.flags = 0;

            if(local_talking.load())
            {
                state.flags |= static_cast<std::uint32_t>(
                    icarus::core::VoiceBackendSessionFlag::local_talking
                );
            }

            if(acoustics_active.load(std::memory_order_relaxed))
            {
                state.flags |= static_cast<std::uint32_t>(
                    icarus::core::VoiceBackendSessionFlag::direct_voice_acoustics_active
                );
            }

            if(acoustic_scene_fresh.load(std::memory_order_relaxed))
            {
                state.flags |= static_cast<std::uint32_t>(
                    icarus::core::VoiceBackendSessionFlag::spatial_scene_fresh
                );
            }

            const bool identity_ready =
                arma.valid
                && state.connection_state == static_cast<std::uint32_t>(
                    icarus::core::VoiceBackendConnectionState::established
                )
                && state.connection_id != 0
                && state.local_client_id != 0
                && arma.payload.player_uid[0] != '\0'
                && arma.payload.network_id[0] != '\0';

            if(identity_ready)
            {
                if(update_local_identity(arma, state))
                {
                    hello_pending = true;
                }

                state.flags |= static_cast<std::uint32_t>(
                    icarus::core::VoiceBackendSessionFlag::identity_ready
                );

                if(hello_pending.exchange(false))
                {
                    if(send_local_identity(
                           icarus::core::IdentityMessageKind::hello,
                           PluginCommandTarget_CURRENT_CHANNEL,
                           nullptr
                       ))
                    {
                        mark_local_identity_announced(read_local_identity());
                    }
                    else
                    {
                        hello_pending = true;
                    }
                }

                if(local_identity_announced())
                {
                    state.flags |= static_cast<std::uint32_t>(
                        icarus::core::VoiceBackendSessionFlag::identity_announced
                    );
                }
            }
            else
            {
                invalidate_local_identity();
                hello_pending = true;
            }

            static_cast<void>(session_state.publish_voice_backend(state));
        }

        if(spatial_scene.sync(bridge_status.generation))
        {
            const auto scene = spatial_scene.read_arma();

            if(scene.valid)
            {
                icarus::core::VoiceBackendSpatialState acknowledgement{};
                acknowledgement.acknowledged_scene_sequence = scene.sequence;
                acknowledgement.observed_actor_count = std::min(
                    scene.payload.actor_count,
                    static_cast<std::uint32_t>(icarus::core::spatial_scene_max_actors)
                );
                acknowledgement.matched_actor_count = count_matched_actors(scene.payload);

                static_cast<void>(
                    spatial_scene.publish_voice_backend(acknowledgement)
                );
            }
        }

        std::this_thread::sleep_for(state_publish_interval);
    }
}

void handle_client_move(std::uint64_t connection_id, anyID client_id)
{
    const auto state = read_voice_state();

    if(state.connection_id == connection_id
        && state.local_client_id == static_cast<std::uint64_t>(client_id))
    {
        clear_connection_identities(connection_id);
        hello_pending = true;
        return;
    }

    remove_identity(connection_id, client_id);
}
}

#define ICARUS_TS3_EXPORT extern "C" __declspec(dllexport)

ICARUS_TS3_EXPORT const char* ts3plugin_name()
{
    return "ICARUS";
}

ICARUS_TS3_EXPORT const char* ts3plugin_version()
{
    return icarus::core::version().data();
}

ICARUS_TS3_EXPORT int ts3plugin_apiVersion()
{
    return plugin_api_version;
}

ICARUS_TS3_EXPORT const char* ts3plugin_author()
{
    return "ICARUS Project";
}

ICARUS_TS3_EXPORT const char* ts3plugin_description()
{
    return "ICARUS communications integration for ArmA 3.";
}

ICARUS_TS3_EXPORT void ts3plugin_setFunctionPointers(const struct TS3Functions functions)
{
    ts3_functions = functions;
}

ICARUS_TS3_EXPORT int ts3plugin_init()
{
    log_message("ICARUS plugin loaded.", LogLevel_INFO);

    {
        std::scoped_lock lock(voice_state_mutex);
        voice_state = {};
        voice_state.health =
            static_cast<std::uint32_t>(icarus::core::VoiceBackendHealth::starting);
        voice_state.state_protocol_major =
            icarus::core::session_state_protocol_major;
        voice_state.state_protocol_minor =
            icarus::core::session_state_protocol_minor;
    }

    if(!bridge.start())
    {
        set_health(icarus::core::VoiceBackendHealth::fault);
        log_message("ICARUS process bridge failed to start.", LogLevel_ERROR);
        return 1;
    }

    refresh_current_connection();
    set_health(icarus::core::VoiceBackendHealth::ready);

    try
    {
        state_worker = std::jthread(run_state_worker);
        acoustic_worker = std::jthread(run_acoustic_worker);
    }
    catch(...)
    {
        acoustic_worker.request_stop();
        state_worker.request_stop();

        if(acoustic_worker.joinable())
        {
            acoustic_worker.join();
        }

        if(state_worker.joinable())
        {
            state_worker.join();
        }

        set_health(icarus::core::VoiceBackendHealth::fault);
        bridge.stop();
        log_message("ICARUS voice services failed to start.", LogLevel_ERROR);
        return 1;
    }

    log_message("ICARUS process bridge service started.", LogLevel_INFO);
    log_message("ICARUS session state service started.", LogLevel_INFO);
    log_message("ICARUS direct voice state service started.", LogLevel_INFO);
    log_message("ICARUS spatial scene service started.", LogLevel_INFO);
    log_message("ICARUS direct voice acoustics service started.", LogLevel_INFO);
    return 0;
}

ICARUS_TS3_EXPORT void ts3plugin_shutdown()
{
    set_health(icarus::core::VoiceBackendHealth::stopping);

    acoustic_worker.request_stop();
    state_worker.request_stop();

    if(acoustic_worker.joinable())
    {
        acoustic_worker.join();
    }

    if(state_worker.joinable())
    {
        state_worker.join();
    }

    spatial_scene.reset();
    session_state.reset();
    bridge.stop();

    {
        std::scoped_lock lock(identity_mutex);
        identities.clear();
        local_identity = {};
    }

    {
        std::scoped_lock lock(plugin_id_mutex);
        plugin_id.clear();
    }

    local_talking = false;
    hello_pending = true;
    acoustics_active.store(false, std::memory_order_relaxed);
    acoustic_scene_fresh.store(false, std::memory_order_relaxed);
    acoustic_connection_id.store(0, std::memory_order_relaxed);
    acoustic_scene_sequence.store(0, std::memory_order_relaxed);
    ts3_functions = {};
}

ICARUS_TS3_EXPORT void ts3plugin_registerPluginID(const char* id)
{
    {
        std::scoped_lock lock(plugin_id_mutex);
        plugin_id = id == nullptr ? "" : id;
    }

    hello_pending = true;
}

ICARUS_TS3_EXPORT void ts3plugin_onConnectStatusChangeEvent(
    uint64 server_connection_handler_id,
    int new_status,
    unsigned int error_number
)
{
    static_cast<void>(error_number);
    update_connection(server_connection_handler_id, new_status);
}

ICARUS_TS3_EXPORT void ts3plugin_currentServerConnectionChanged(
    uint64 server_connection_handler_id
)
{
    if(server_connection_handler_id == 0
        || ts3_functions.getConnectionStatus == nullptr)
    {
        update_connection(server_connection_handler_id, STATUS_DISCONNECTED);
        return;
    }

    int status = STATUS_DISCONNECTED;

    if(ts3_functions.getConnectionStatus(
           server_connection_handler_id,
           &status
       ) != ERROR_ok)
    {
        update_connection(server_connection_handler_id, STATUS_DISCONNECTED);
        return;
    }

    update_connection(server_connection_handler_id, status);
    hello_pending = true;
}

ICARUS_TS3_EXPORT void ts3plugin_onTalkStatusChangeEvent(
    uint64 server_connection_handler_id,
    int status,
    int is_received_whisper,
    anyID client_id
)
{
    static_cast<void>(is_received_whisper);

    const bool talking = status == STATUS_TALKING;
    set_identity_talking(server_connection_handler_id, client_id, talking);

    const auto state = read_voice_state();

    if(state.connection_id == server_connection_handler_id
        && state.local_client_id == static_cast<std::uint64_t>(client_id))
    {
        local_talking = talking;
    }
}

ICARUS_TS3_EXPORT void ts3plugin_onCustom3dRolloffCalculationClientEvent(
    uint64 server_connection_handler_id,
    anyID client_id,
    float distance,
    float* volume
)
{
    if(volume == nullptr
        || !acoustics_active.load(std::memory_order_relaxed)
        || acoustic_connection_id.load(std::memory_order_relaxed)
            != server_connection_handler_id)
    {
        return;
    }

    const std::uint8_t level_value =
        acoustic_voice_levels[static_cast<unsigned int>(client_id)].load(
            std::memory_order_relaxed
        );

    if(level_value < static_cast<std::uint8_t>(icarus::core::VoiceLevel::whisper)
        || level_value > static_cast<std::uint8_t>(icarus::core::VoiceLevel::shout))
    {
        return;
    }

    *volume = icarus::core::direct_voice_gain(
        static_cast<icarus::core::VoiceLevel>(level_value),
        distance
    );
}

ICARUS_TS3_EXPORT void ts3plugin_onClientSelfVariableUpdateEvent(
    uint64 server_connection_handler_id,
    int flag,
    const char* old_value,
    const char* new_value
)
{
    static_cast<void>(old_value);

    if(flag != CLIENT_FLAG_TALKING || new_value == nullptr)
    {
        return;
    }

    const auto state = read_voice_state();

    if(state.connection_id == server_connection_handler_id)
    {
        local_talking = std::string_view{new_value} == "1";
    }
}

ICARUS_TS3_EXPORT void ts3plugin_onPluginCommandEvent(
    uint64 server_connection_handler_id,
    const char* plugin_name,
    const char* plugin_command,
    anyID invoker_client_id,
    const char* invoker_name,
    const char* invoker_unique_identity
)
{
    static_cast<void>(invoker_name);

    if(!is_our_plugin(plugin_name) || plugin_command == nullptr)
    {
        return;
    }

    const auto message =
        icarus::core::parse_identity_message(plugin_command);

    if(!message.valid)
    {
        return;
    }

    upsert_identity(
        server_connection_handler_id,
        invoker_client_id,
        invoker_unique_identity == nullptr ? "" : invoker_unique_identity,
        message.player_uid,
        message.network_id
    );

    if(message.kind != icarus::core::IdentityMessageKind::hello)
    {
        return;
    }

    const auto state = read_voice_state();

    if(state.connection_id != server_connection_handler_id
        || state.local_client_id == static_cast<std::uint64_t>(invoker_client_id))
    {
        return;
    }

    const anyID targets[] = {invoker_client_id, 0};

    static_cast<void>(send_local_identity(
        icarus::core::IdentityMessageKind::identity,
        PluginCommandTarget_CLIENT,
        targets
    ));
}

ICARUS_TS3_EXPORT void ts3plugin_onClientMoveEvent(
    uint64 server_connection_handler_id,
    anyID client_id,
    uint64 old_channel_id,
    uint64 new_channel_id,
    int visibility,
    const char* move_message
)
{
    static_cast<void>(old_channel_id);
    static_cast<void>(new_channel_id);
    static_cast<void>(visibility);
    static_cast<void>(move_message);
    handle_client_move(server_connection_handler_id, client_id);
}

ICARUS_TS3_EXPORT void ts3plugin_onClientMoveMovedEvent(
    uint64 server_connection_handler_id,
    anyID client_id,
    uint64 old_channel_id,
    uint64 new_channel_id,
    int visibility,
    anyID mover_id,
    const char* mover_name,
    const char* mover_unique_identifier,
    const char* move_message
)
{
    static_cast<void>(old_channel_id);
    static_cast<void>(new_channel_id);
    static_cast<void>(visibility);
    static_cast<void>(mover_id);
    static_cast<void>(mover_name);
    static_cast<void>(mover_unique_identifier);
    static_cast<void>(move_message);
    handle_client_move(server_connection_handler_id, client_id);
}

ICARUS_TS3_EXPORT void ts3plugin_onClientKickFromChannelEvent(
    uint64 server_connection_handler_id,
    anyID client_id,
    uint64 old_channel_id,
    uint64 new_channel_id,
    int visibility,
    anyID kicker_id,
    const char* kicker_name,
    const char* kicker_unique_identifier,
    const char* kick_message
)
{
    static_cast<void>(old_channel_id);
    static_cast<void>(new_channel_id);
    static_cast<void>(visibility);
    static_cast<void>(kicker_id);
    static_cast<void>(kicker_name);
    static_cast<void>(kicker_unique_identifier);
    static_cast<void>(kick_message);
    handle_client_move(server_connection_handler_id, client_id);
}

ICARUS_TS3_EXPORT void ts3plugin_onClientKickFromServerEvent(
    uint64 server_connection_handler_id,
    anyID client_id,
    uint64 old_channel_id,
    uint64 new_channel_id,
    int visibility,
    anyID kicker_id,
    const char* kicker_name,
    const char* kicker_unique_identifier,
    const char* kick_message
)
{
    static_cast<void>(old_channel_id);
    static_cast<void>(new_channel_id);
    static_cast<void>(visibility);
    static_cast<void>(kicker_id);
    static_cast<void>(kicker_name);
    static_cast<void>(kicker_unique_identifier);
    static_cast<void>(kick_message);
    handle_client_move(server_connection_handler_id, client_id);
}
