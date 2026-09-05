#include <icarus/core/version.hpp>
#include <icarus/ipc/bridge.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
icarus::ipc::BridgeEndpoint bridge(icarus::ipc::BridgeRole::arma);

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
        return bridge.start() ? "started" : icarus::ipc::to_string(bridge.status().state);
    }

    if(function == "bridge_stop")
    {
        bridge.stop();
        return "stopped";
    }

    if(function == "bridge_status")
    {
        return format_bridge_status();
    }

    return "unsupported";
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
    const std::string_view function_name = function == nullptr ? std::string_view{} : std::string_view{function};
    const std::string result = dispatch(function_name);
    write_output(output, output_size, result);
}

__declspec(dllexport) int __stdcall RVExtensionArgs(
    char* output,
    unsigned int output_size,
    const char* function,
    const char**,
    unsigned int)
{
    const std::string_view function_name = function == nullptr ? std::string_view{} : std::string_view{function};
    const std::string result = dispatch(function_name);
    write_output(output, output_size, result);
    return 0;
}
}
