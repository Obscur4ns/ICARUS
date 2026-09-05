#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace icarus::core
{
inline constexpr std::uint32_t spatial_scene_magic = 0x49435343U;
inline constexpr std::uint16_t spatial_scene_protocol_major = 1;
inline constexpr std::uint16_t spatial_scene_protocol_minor = 0;
inline constexpr std::size_t spatial_scene_max_actors = 256;

enum class SpatialActorFlag : std::uint32_t
{
    alive = 1U << 0U,
    in_vehicle = 1U << 1U,
};

struct alignas(8) SpatialSceneHeader
{
    std::uint32_t ready;
    std::uint32_t magic;
    std::uint16_t protocol_major;
    std::uint16_t protocol_minor;
    std::uint32_t struct_size;
    std::uint64_t generation;
};

struct alignas(8) SpatialActorState
{
    char player_uid[40];
    char network_id[64];
    float position[3];
    float head_direction[3];
    float velocity[3];
    std::uint32_t flags;
    std::uint32_t voice_level;
    std::uint32_t reserved[3];
};

struct alignas(8) SpatialSceneState
{
    std::uint32_t actor_count;
    std::uint32_t reserved;
    SpatialActorState actors[spatial_scene_max_actors];
};

struct alignas(8) VoiceBackendSpatialState
{
    std::uint64_t acknowledged_scene_sequence;
    std::uint32_t observed_actor_count;
    std::uint32_t matched_actor_count;
};

template<typename Payload>
struct alignas(8) SpatialStateSlot
{
    std::uint64_t sequence;
    std::uint64_t updated_tick;
    Payload payload;
};

struct alignas(8) SpatialScenePage
{
    SpatialSceneHeader header;
    SpatialStateSlot<SpatialSceneState> arma;
    SpatialStateSlot<VoiceBackendSpatialState> voice_backend;
};

[[nodiscard]] constexpr bool has_flag(
    std::uint32_t flags,
    SpatialActorFlag flag
) noexcept
{
    return (flags & static_cast<std::uint32_t>(flag)) != 0;
}

static_assert(std::is_standard_layout_v<SpatialScenePage>);
static_assert(std::is_trivially_copyable_v<SpatialScenePage>);
static_assert(alignof(SpatialScenePage) == 8);
static_assert(sizeof(SpatialSceneHeader) == 24);
static_assert(sizeof(SpatialActorState) == 160);
static_assert(sizeof(SpatialSceneState) == 40968);
static_assert(sizeof(VoiceBackendSpatialState) == 16);
static_assert(sizeof(SpatialStateSlot<SpatialSceneState>) == 40984);
static_assert(sizeof(SpatialStateSlot<VoiceBackendSpatialState>) == 32);
static_assert(sizeof(SpatialScenePage) == 41040);
}
