#include <icarus/core/version.hpp>
#include <icarus/ipc/bridge.hpp>

#include <teamspeak/public_definitions.h>
#include <ts3_functions.h>

namespace
{
constexpr int plugin_api_version = 26;

struct TS3Functions ts3_functions
{
};

icarus::ipc::BridgeEndpoint bridge(icarus::ipc::BridgeRole::teamspeak);

void log_message(const char* message, LogLevel level)
{
    if(ts3_functions.logMessage != nullptr)
    {
        ts3_functions.logMessage(message, level, "ICARUS", 0);
    }
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

    if(!bridge.start())
    {
        log_message("ICARUS process bridge failed to start.", LogLevel_ERROR);
        return 1;
    }

    log_message("ICARUS process bridge service started.", LogLevel_INFO);
    return 0;
}

ICARUS_TS3_EXPORT void ts3plugin_shutdown()
{
    bridge.stop();
    ts3_functions = {};
}
