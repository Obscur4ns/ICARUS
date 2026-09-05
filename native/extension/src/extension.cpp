#include <icarus/core/session_state_protocol.hpp>
#include <icarus/core/version.hpp>
#include <icarus/ipc/bridge.hpp>
#include <icarus/ipc/session_state.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
icarus::ipc::BridgeEndpoint bridge(icarus::ipc::BridgeRole::arma);
icarus::ipc::SessionStateChannel session_state(icarus::ipc::BridgeRole::arma);

void write_output(char* output, unsigned int output_size, std::string_view value) noexcept
{
    if(output == nullptr || output_size == 0)
    {
        return;
    }

    const std::size_t capacity = static_cast<std::size_t>(output_size - 1);
    const std::size_t length = std::min(value.size(), capacity);

    std::memcpy(output, value.data(), length);
    output[length] = '\0';
}

std::string_view normalise_argument(const char* argument) noexcept
{
    if(argument == nullptr)
    {
        return {};
    }

    std::string_view value{argument};

    if(value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }

    return value;
}

template<typename Number>
bool parse_number(std::string_view value, Number& result) noexcept
{
    const char* begin = value.data();
    const char* end = begin + value.size();

    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

template<std::size_t Size>
void copy_text(char (&target)[Size], std::string_view value) noexcept
{
    const std::size_t length = std::min(value.size(), Size - 1);

    std::memset(target, 0, Size);
    std::memcpy(target, value.data(), length);
}

std::string format_bridge_status()
{
    const auto status = bridge.status();

    std::ostringstream stream;
    stream << "state=" << icarus::ipc::to_string(status.state)
           << ";protocol=" << status.protocol_major << '.' << status.protocol_minor
           << ";generation=" << status.generation
           << ";localPid=" << status.local_pid
           << ";peerPid=" << status.peer_pid
           << ";peerHeartbeatAgeMs=";

    if(status.peer_heartbeat_age_ms == std::numeric_limits<std::uint64_t>::max())
    {
        stream << "none";
    }
    else
    {
        stream << status.peer_heartbeat_age_ms;
    }

    return stream.str();
}

std::string format_session_status()
{
    const auto bridge_status = bridge.status();

    if(bridge_status.generation == 0 || !session_state.sync(bridge_status.generation))
    {
        return "transport=unavailable";
    }

    const auto arma = session_state.read_arma();
    const auto voice = session_state.read_voice_backend();

    std::ostringstream stream;
    stream << "transport=ready"
           << ";protocol=" << icarus::core::session_state_protocol_major
           << '.' << icarus::core::session_state_protocol_minor
           << ";generation=" << bridge_status.generation
           << ";armaValid=" << (arma.valid ? 1 : 0);

    if(arma.valid)
    {
        const auto voice_level =
            static_cast<icarus::core::VoiceLevel>(arma.payload.voice_level);
        const auto vehicle_role =
            static_cast<icarus::core::VehicleRole>(arma.payload.vehicle_role);

        stream << ";armaSeq=" << arma.sequence
               << ";playerUid=" << arma.payload.player_uid
               << ";networkId=" << arma.payload.network_id
               << ";position=[" << arma.payload.position[0]
               << ',' << arma.payload.position[1]
               << ',' << arma.payload.position[2] << ']'
               << ";alive="
               << (icarus::core::has_flag(
                       arma.payload.flags,
                       icarus::core::ArmaSessionFlag::alive
                   )
                       ? 1
                       : 0)
               << ";spectator="
               << (icarus::core::has_flag(
                       arma.payload.flags,
                       icarus::core::ArmaSessionFlag::spectator
                   )
                       ? 1
                       : 0)
               << ";inVehicle="
               << (icarus::core::has_flag(
                       arma.payload.flags,
                       icarus::core::ArmaSessionFlag::in_vehicle
                   )
                       ? 1
                       : 0)
               << ";voiceLevel=" << icarus::core::to_string(voice_level)
               << ";vehicleRole=" << icarus::core::to_string(vehicle_role);
    }

    stream << ";voiceValid=" << (voice.valid ? 1 : 0);

    if(voice.valid)
    {
        const auto connection_state =
            static_cast<icarus::core::VoiceBackendConnectionState>(
                voice.payload.connection_state
            );
        const auto health =
            static_cast<icarus::core::VoiceBackendHealth>(voice.payload.health);

        stream << ";voiceSeq=" << voice.sequence
               << ";voiceConnection=" << icarus::core::to_string(connection_state)
               << ";voiceConnectionId=" << voice.payload.connection_id
               << ";voiceClientId=" << voice.payload.local_client_id
               << ";voiceHealth=" << icarus::core::to_string(health)
               << ";ackSession=" << voice.payload.acknowledged_session_id
               << ";ackArmaSeq=" << voice.payload.acknowledged_arma_sequence;
    }

    return stream.str();
}

std::string dispatch(std::string_view function)
{
    if(function == "ping")
    {
        return "pong";
    }

    if(function == "version")
    {
        return std::string{icarus::core::version()};
    }

    if(function == "bridge_start")
    {
        if(!bridge.start())
        {
            return icarus::ipc::to_string(bridge.status().state);
        }

        const auto status = bridge.status();

        if(status.generation != 0)
        {
            static_cast<void>(session_state.sync(status.generation));
        }

        return "started";
    }

    if(function == "bridge_stop")
    {
        session_state.reset();
        bridge.stop();
        return "stopped";
    }

    if(function == "bridge_status")
    {
        return format_bridge_status();
    }

    if(function == "session_status")
    {
        return format_session_status();
    }

    return "unsupported";
}

int publish_session_state(
    char* output,
    unsigned int output_size,
    const char** arguments,
    unsigned int argument_count
)
{
    constexpr unsigned int expected_argument_count = 14;

    if(arguments == nullptr || argument_count != expected_argument_count)
    {
        write_output(output, output_size, "invalid_argument_count");
        return 1;
    }

    const auto bridge_status = bridge.status();

    if(bridge_status.generation == 0 || !session_state.sync(bridge_status.generation))
    {
        write_output(output, output_size, "transport_unavailable");
        return 2;
    }

    icarus::core::ArmaSessionState state{};
    state.session_id = bridge_status.generation;

    copy_text(state.player_uid, normalise_argument(arguments[0]));
    copy_text(state.network_id, normalise_argument(arguments[1]));

    for(std::size_t index = 0; index < 3; ++index)
    {
        if(!parse_number(normalise_argument(arguments[2 + index]), state.position[index]))
        {
            write_output(output, output_size, "invalid_position");
            return 3;
        }

        if(!parse_number(normalise_argument(arguments[5 + index]), state.head_direction[index]))
        {
            write_output(output, output_size, "invalid_head_direction");
            return 4;
        }
    }

    std::uint32_t alive{};
    std::uint32_t spectator{};
    std::uint32_t in_vehicle{};
    std::uint32_t vehicle_role{};
    std::uint32_t voice_level{};

    if(!parse_number(normalise_argument(arguments[8]), alive)
        || !parse_number(normalise_argument(arguments[9]), spectator)
        || !parse_number(normalise_argument(arguments[10]), in_vehicle))
    {
        write_output(output, output_size, "invalid_flags");
        return 5;
    }

    copy_text(state.vehicle_network_id, normalise_argument(arguments[11]));

    if(!parse_number(normalise_argument(arguments[12]), vehicle_role))
    {
        write_output(output, output_size, "invalid_vehicle_role");
        return 6;
    }

    const std::string_view voice_level_value = normalise_argument(arguments[13]);

    if(!parse_number(voice_level_value, voice_level))
    {
        write_output(output, output_size, "invalid_voice_level");
        return 7;
    }

    if(alive != 0)
    {
        state.flags |= static_cast<std::uint32_t>(icarus::core::ArmaSessionFlag::alive);
    }

    if(spectator != 0)
    {
        state.flags |= static_cast<std::uint32_t>(icarus::core::ArmaSessionFlag::spectator);
    }

    if(in_vehicle != 0)
    {
        state.flags |= static_cast<std::uint32_t>(icarus::core::ArmaSessionFlag::in_vehicle);
    }

    state.voice_level = voice_level;
    state.vehicle_role = vehicle_role;

    if(!session_state.publish_arma(state))
    {
        write_output(output, output_size, "publish_failed");
        return 8;
    }

    write_output(output, output_size, "ok");
    return 0;
}
}

extern "C"
{
__declspec(dllexport) void __stdcall RVExtensionVersion(char* output, unsigned int output_size)
{
    write_output(output, output_size, icarus::core::version());
}

__declspec(dllexport) void __stdcall RVExtension(char* output, unsigned int output_size, const char* function)
{
    const std::string_view function_name =
        function == nullptr ? std::string_view{} : std::string_view{function};
    const std::string result = dispatch(function_name);

    write_output(output, output_size, result);
}

__declspec(dllexport) int __stdcall RVExtensionArgs(
    char* output,
    unsigned int output_size,
    const char* function,
    const char** arguments,
    unsigned int argument_count
)
{
    const std::string_view function_name =
        function == nullptr ? std::string_view{} : std::string_view{function};

    if(function_name == "session_publish")
    {
        return publish_session_state(
            output,
            output_size,
            arguments,
            argument_count
        );
    }

    const std::string result = dispatch(function_name);
    write_output(output, output_size, result);
    return 0;
}
}
