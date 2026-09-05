#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace icarus::ipc
{
inline constexpr wchar_t default_bridge_mapping_name[] = L"Local\\ICARUS.Bridge.1";

enum class BridgeRole
{
    arma,
    teamspeak,
};

enum class BridgeConnectionState
{
    stopped,
    waiting_for_peer,
    connected,
    protocol_mismatch,
    owner_conflict,
    transport_error,
};

struct BridgeStatus
{
    BridgeConnectionState state{BridgeConnectionState::stopped};
    std::uint16_t protocol_major{};
    std::uint16_t protocol_minor{};
    std::uint64_t generation{};
    std::uint64_t local_pid{};
    std::uint64_t peer_pid{};
    std::uint64_t peer_heartbeat_age_ms{};
};

[[nodiscard]] const char* to_string(BridgeConnectionState state) noexcept;

class BridgeEndpoint
{
public:
    explicit BridgeEndpoint(BridgeRole role, std::wstring mapping_name = default_bridge_mapping_name);
    ~BridgeEndpoint();

    BridgeEndpoint(const BridgeEndpoint&) = delete;
    BridgeEndpoint& operator=(const BridgeEndpoint&) = delete;
    BridgeEndpoint(BridgeEndpoint&&) = delete;
    BridgeEndpoint& operator=(BridgeEndpoint&&) = delete;

    [[nodiscard]] bool start();
    void stop();

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] BridgeStatus status() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
