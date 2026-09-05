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
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
constexpr int plugin_api_version = 26;
constexpr std::chrono::milliseconds state_publish_interval{100};

struct TS3Functions ts3_functions
{
};

icarus::ipc::BridgeEndpoint bridge(icarus::ipc::BridgeRole::teamspeak);
icarus::ipc::SessionStateChannel session_state(icarus::ipc::BridgeRole::teamspeak);
icarus::ipc::SpatialSceneChannel spatial_scene(icarus::ipc::BridgeRole::teamspeak);

std::mutex voice_state_mutex;
icarus::core::VoiceBackendSessionState voice_state{};
std::jthread state_worker;

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
    }
    catch(...)
    {
        set_health(icarus::core::VoiceBackendHealth::fault);
        bridge.stop();
        log_message("ICARUS session state worker failed to start.", LogLevel_ERROR);
        return 1;
    }

    log_message("ICARUS process bridge service started.", LogLevel_INFO);
    log_message("ICARUS session state service started.", LogLevel_INFO);
    log_message("ICARUS direct voice state service started.", LogLevel_INFO);
    log_message("ICARUS spatial scene service started.", LogLevel_INFO);
    return 0;
}

ICARUS_TS3_EXPORT void ts3plugin_shutdown()
{
    set_health(icarus::core::VoiceBackendHealth::stopping);

    state_worker.request_stop();

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
