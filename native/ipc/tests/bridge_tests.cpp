#include <icarus/ipc/bridge.hpp>

#include <Windows.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>

namespace
{
bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while(std::chrono::steady_clock::now() < deadline)
    {
        if(predicate())
        {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    return predicate();
}
}

int main()
{
    const std::wstring mapping_name =
        L"Local\\ICARUS.Bridge.Test." + std::to_wstring(GetCurrentProcessId());

    icarus::ipc::BridgeEndpoint arma(icarus::ipc::BridgeRole::arma, mapping_name);
    icarus::ipc::BridgeEndpoint teamspeak(icarus::ipc::BridgeRole::teamspeak, mapping_name);

    if(!arma.start())
    {
        return 1;
    }

    if(!teamspeak.start())
    {
        return 2;
    }

    const bool initially_connected = wait_until(
        [&]() {
            return arma.status().state == icarus::ipc::BridgeConnectionState::connected
                && teamspeak.status().state == icarus::ipc::BridgeConnectionState::connected;
        },
        std::chrono::seconds(3)
    );

    if(!initially_connected)
    {
        return 3;
    }

    teamspeak.stop();

    const bool disconnect_observed = wait_until(
        [&]() {
            return arma.status().state == icarus::ipc::BridgeConnectionState::waiting_for_peer;
        },
        std::chrono::seconds(3)
    );

    if(!disconnect_observed)
    {
        return 4;
    }

    if(!teamspeak.start())
    {
        return 5;
    }

    const bool reconnected = wait_until(
        [&]() {
            return arma.status().state == icarus::ipc::BridgeConnectionState::connected
                && teamspeak.status().state == icarus::ipc::BridgeConnectionState::connected;
        },
        std::chrono::seconds(3)
    );

    teamspeak.stop();
    arma.stop();

    return reconnected ? 0 : 6;
}
