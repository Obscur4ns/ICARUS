#include <icarus/core/session_state_protocol.hpp>
#include <icarus/core/version.hpp>
#include <icarus/ipc/bridge.hpp>
#include <icarus/ipc/session_state.hpp>

#include <teamspeak/public_definitions.h>
#include <teamspeak/public_errors.h>
#include <ts3_functions.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace
{
constexpr int plugin_api_version = 26;
constexpr std::chrono::milliseconds state_publish_interval{100};

struct TS3Functions ts3_functions
{
};

icarus::ipc::BridgeEndpoint bridge(icarus::ipc::BridgeRole::teamspeak);
icarus::ipc::SessionStateChannel session_state(icarus::ipc::BridgeRole::teamspeak);

std::mutex voice_state_mutex;
icarus::core::VoiceBackendSessionState voice_state{};
std::jthread state_worker;

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

void set_health(icarus::core::VoiceBackendHealth health)
{
    std::scoped_lock lock(voice_state_mutex);
    voice_state.health = static_cast<std::uint32_t>(health);
}

void update_connection(std::uint64_t connection_id, int status)
{
    std::scoped_lock lock(voice_state_mutex);

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

void run_state_worker(std::stop_token stop_token)
{
    while(!stop_token.stop_requested())
    {
        const auto bridge_status = bridge.status();

        if(bridge_status.generation == 0)
        {
            session_state.reset();
            std::this_thread::sleep_for(state_publish_interval);
            continue;
        }

        if(session_state.sync(bridge_status.generation))
        {
            icarus::core::VoiceBackendSessionState state{};

            {
                std::scoped_lock lock(voice_state_mutex);
                state = voice_state;
            }

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

            static_cast<void>(session_state.publish_voice_backend(state));
        }

        std::this_thread::sleep_for(state_publish_interval);
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

    session_state.reset();
    bridge.stop();
    ts3_functions = {};
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
}
