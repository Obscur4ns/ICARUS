#pragma once

#include <icarus/core/session_state_protocol.hpp>

namespace icarus::core
{
struct DirectVoiceAcousticProfile
{
    float reference_distance_m;
    float far_field_scale;
};

inline constexpr float direct_voice_near_field_m = 0.5F;
inline constexpr float direct_voice_far_field_scale = 8.0F;

[[nodiscard]] constexpr bool is_direct_voice_level(VoiceLevel level) noexcept
{
    return level >= VoiceLevel::whisper && level <= VoiceLevel::shout;
}

[[nodiscard]] constexpr DirectVoiceAcousticProfile direct_voice_profile(
    VoiceLevel level
) noexcept
{
    switch(level)
    {
        case VoiceLevel::whisper:
            return {0.75F, direct_voice_far_field_scale};
        case VoiceLevel::quiet:
            return {1.5F, direct_voice_far_field_scale};
        case VoiceLevel::raised:
            return {6.0F, direct_voice_far_field_scale};
        case VoiceLevel::shout:
            return {12.0F, direct_voice_far_field_scale};
        case VoiceLevel::unknown:
        case VoiceLevel::normal:
        default:
            return {3.0F, direct_voice_far_field_scale};
    }
}

[[nodiscard]] float direct_voice_gain(VoiceLevel level, float distance_m) noexcept;
}
