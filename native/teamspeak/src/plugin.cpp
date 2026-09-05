#include <icarus/core/version.hpp>

#include <teamspeak/public_definitions.h>
#include <ts3_functions.h>

namespace
{
constexpr int plugin_api_version = 26;
struct TS3Functions ts3_functions
{
};
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
    if(ts3_functions.logMessage != nullptr)
    {
        ts3_functions.logMessage("ICARUS plugin loaded.", LogLevel_INFO, "ICARUS", 0);
    }

    return 0;
}

ICARUS_TS3_EXPORT void ts3plugin_shutdown()
{
    ts3_functions = {};
}
