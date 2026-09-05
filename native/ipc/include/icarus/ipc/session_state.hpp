#pragma once

#include <icarus/core/session_state_protocol.hpp>
#include <icarus/ipc/bridge.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace icarus::ipc
{
inline constexpr wchar_t default_session_state_mapping_name[] = L"Local\\ICARUS.SessionState.1";

template<typename Payload>
struct StateSnapshot
{
    bool valid{};
    std::uint64_t sequence{};
    std::uint64_t updated_tick{};
    Payload payload{};
};

class SessionStateChannel
{
public:
    explicit SessionStateChannel(
        BridgeRole role,
        std::wstring mapping_name = default_session_state_mapping_name
    );
    ~SessionStateChannel();

    SessionStateChannel(const SessionStateChannel&) = delete;
    SessionStateChannel& operator=(const SessionStateChannel&) = delete;
    SessionStateChannel(SessionStateChannel&&) = delete;
    SessionStateChannel& operator=(SessionStateChannel&&) = delete;

    [[nodiscard]] bool sync(std::uint64_t generation);
    void reset();

    [[nodiscard]] std::uint64_t generation() const;

    [[nodiscard]] bool publish_arma(const core::ArmaSessionState& state);
    [[nodiscard]] bool publish_voice_backend(const core::VoiceBackendSessionState& state);

    [[nodiscard]] StateSnapshot<core::ArmaSessionState> read_arma() const;
    [[nodiscard]] StateSnapshot<core::VoiceBackendSessionState> read_voice_backend() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
