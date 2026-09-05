#pragma once

#include <cstdint>
#include <type_traits>

namespace icarus::core
{
inline constexpr std::uint32_t bridge_magic = 0x49434152U;
inline constexpr std::uint16_t bridge_protocol_major = 1;
inline constexpr std::uint16_t bridge_protocol_minor = 0;

inline constexpr std::uint64_t bridge_heartbeat_interval_ms = 250;
inline constexpr std::uint64_t bridge_peer_timeout_ms = 1500;

enum class BridgeLifecycle : std::uint64_t
{
    offline = 0,
    online = 1,
    stopping = 2,
};

struct alignas(8) BridgeHeader
{
    std::uint32_t ready;
    std::uint32_t magic;
    std::uint16_t protocol_major;
    std::uint16_t protocol_minor;
    std::uint32_t struct_size;
    std::uint64_t generation;
};

struct alignas(8) BridgeEndpointSlot
{
    std::uint64_t pid;
    std::uint64_t started_tick;
    std::uint64_t heartbeat_tick;
    std::uint64_t sequence;
    std::uint64_t lifecycle;
};

struct alignas(8) BridgePage
{
    BridgeHeader header;
    BridgeEndpointSlot arma;
    BridgeEndpointSlot teamspeak;
};

static_assert(std::is_standard_layout_v<BridgePage>);
static_assert(std::is_trivially_copyable_v<BridgePage>);
static_assert(alignof(BridgePage) == 8);
static_assert(sizeof(BridgeHeader) == 24);
static_assert(sizeof(BridgeEndpointSlot) == 40);
static_assert(sizeof(BridgePage) == 104);
}
