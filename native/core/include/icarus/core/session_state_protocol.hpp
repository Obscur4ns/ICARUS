#pragma once

#include <cstdint>
#include <type_traits>

namespace icarus::core
{
inline constexpr std::uint32_t session_state_magic = 0x49435353U;
inline constexpr std::uint16_t session_state_protocol_major = 1;
inline constexpr std::uint16_t session_state_protocol_minor = 0;

enum class ArmaSessionFlag : std::uint32_t
{
    alive = 1U << 0U,
    spectator = 1U << 1U,
    in_vehicle = 1U << 2U,
};

enum class VoiceLevel : std::uint32_t
{
    unknown = 0,
    whisper = 1,
    quiet = 2,
    normal = 3,
    raised = 4,
    shout = 5,
};

enum class VehicleRole : std::uint32_t
{
    none = 0,
    driver = 1,
    commander = 2,
    gunner = 3,
    turret = 4,
    cargo = 5,
    unknown = 6,
};

enum class VoiceBackendConnectionState : std::uint32_t
{
    disconnected = 0,
    connecting = 1,
    established = 2,
};

enum class VoiceBackendHealth : std::uint32_t
{
    starting = 0,
    ready = 1,
    degraded = 2,
    stopping = 3,
    fault = 4,
};

struct alignas(8) SessionStateHeader
{
    std::uint32_t ready;
    std::uint32_t magic;
    std::uint16_t protocol_major;
    std::uint16_t protocol_minor;
    std::uint32_t struct_size;
    std::uint64_t generation;
};

struct alignas(8) ArmaSessionState
{
    std::uint64_t session_id;
    float position[3];
    float head_direction[3];
    std::uint32_t flags;
    std::uint32_t voice_level;
    std::uint32_t vehicle_role;
    std::uint32_t reserved;
    char player_uid[40];
    char network_id[64];
    char vehicle_network_id[64];
};

struct alignas(8) VoiceBackendSessionState
{
    std::uint64_t connection_id;
    std::uint64_t acknowledged_session_id;
    std::uint64_t acknowledged_arma_sequence;
    std::uint64_t local_client_id;
    std::uint32_t connection_state;
    std::uint32_t health;
    std::uint16_t state_protocol_major;
    std::uint16_t state_protocol_minor;
    std::uint32_t reserved;
};

template<typename Payload>
struct alignas(8) SessionStateSlot
{
    std::uint64_t sequence;
    std::uint64_t updated_tick;
    Payload payload;
};

struct alignas(8) SessionStatePage
{
    SessionStateHeader header;
    SessionStateSlot<ArmaSessionState> arma;
    SessionStateSlot<VoiceBackendSessionState> voice_backend;
};

[[nodiscard]] constexpr bool has_flag(std::uint32_t flags, ArmaSessionFlag flag) noexcept
{
    return (flags & static_cast<std::uint32_t>(flag)) != 0;
}

[[nodiscard]] constexpr const char* to_string(VoiceLevel value) noexcept
{
    switch(value)
    {
        case VoiceLevel::unknown:
            return "unknown";
        case VoiceLevel::whisper:
            return "whisper";
        case VoiceLevel::quiet:
            return "quiet";
        case VoiceLevel::normal:
            return "normal";
        case VoiceLevel::raised:
            return "raised";
        case VoiceLevel::shout:
            return "shout";
    }

    return "unknown";
}

[[nodiscard]] constexpr const char* to_string(VehicleRole value) noexcept
{
    switch(value)
    {
        case VehicleRole::none:
            return "none";
        case VehicleRole::driver:
            return "driver";
        case VehicleRole::commander:
            return "commander";
        case VehicleRole::gunner:
            return "gunner";
        case VehicleRole::turret:
            return "turret";
        case VehicleRole::cargo:
            return "cargo";
        case VehicleRole::unknown:
            return "unknown";
    }

    return "unknown";
}

[[nodiscard]] constexpr const char* to_string(VoiceBackendConnectionState value) noexcept
{
    switch(value)
    {
        case VoiceBackendConnectionState::disconnected:
            return "disconnected";
        case VoiceBackendConnectionState::connecting:
            return "connecting";
        case VoiceBackendConnectionState::established:
            return "established";
    }

    return "unknown";
}

[[nodiscard]] constexpr const char* to_string(VoiceBackendHealth value) noexcept
{
    switch(value)
    {
        case VoiceBackendHealth::starting:
            return "starting";
        case VoiceBackendHealth::ready:
            return "ready";
        case VoiceBackendHealth::degraded:
            return "degraded";
        case VoiceBackendHealth::stopping:
            return "stopping";
        case VoiceBackendHealth::fault:
            return "fault";
    }

    return "unknown";
}

static_assert(std::is_standard_layout_v<SessionStatePage>);
static_assert(std::is_trivially_copyable_v<SessionStatePage>);
static_assert(alignof(SessionStatePage) == 8);
static_assert(sizeof(SessionStateHeader) == 24);
static_assert(sizeof(ArmaSessionState) == 216);
static_assert(sizeof(VoiceBackendSessionState) == 48);
static_assert(sizeof(SessionStateSlot<ArmaSessionState>) == 232);
static_assert(sizeof(SessionStateSlot<VoiceBackendSessionState>) == 64);
static_assert(sizeof(SessionStatePage) == 320);
}
