#include <icarus/core/direct_voice_acoustics.hpp>

#include <algorithm>
#include <cmath>

namespace icarus::core
{
float direct_voice_gain(VoiceLevel level, float distance_m) noexcept
{
    if(!std::isfinite(distance_m))
    {
        return 0.0F;
    }

    const float distance = std::max(distance_m, 0.0F);
    const float effective_distance = std::max(distance - direct_voice_near_field_m, 0.0F);
    const DirectVoiceAcousticProfile profile = direct_voice_profile(level);

    const float geometric =
        profile.reference_distance_m / (profile.reference_distance_m + effective_distance);
    const float far_field = 1.0F
        / (1.0F
           + effective_distance
               / (profile.reference_distance_m * profile.far_field_scale));

    return std::clamp(geometric * far_field, 0.0F, 1.0F);
}
}
