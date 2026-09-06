#include <icarus/core/direct_voice_acoustics.hpp>

#include <array>
#include <cmath>
#include <limits>

namespace
{
bool near(float left, float right, float tolerance = 0.0001F)
{
    return std::fabs(left - right) <= tolerance;
}
}

int main()
{
    using icarus::core::VoiceLevel;
    using icarus::core::direct_voice_gain;

    if(!near(direct_voice_gain(VoiceLevel::normal, 0.0F), 1.0F))
    {
        return 1;
    }

    if(!near(direct_voice_gain(VoiceLevel::normal, -5.0F), 1.0F))
    {
        return 2;
    }

    if(direct_voice_gain(
           VoiceLevel::normal,
           std::numeric_limits<float>::infinity()
       ) != 0.0F)
    {
        return 3;
    }

    constexpr std::array levels{
        VoiceLevel::whisper,
        VoiceLevel::quiet,
        VoiceLevel::normal,
        VoiceLevel::raised,
        VoiceLevel::shout,
    };

    for(const VoiceLevel level : levels)
    {
        const float near_gain = direct_voice_gain(level, 2.0F);
        const float middle_gain = direct_voice_gain(level, 20.0F);
        const float far_gain = direct_voice_gain(level, 100.0F);

        if(!(near_gain > middle_gain && middle_gain > far_gain && far_gain > 0.0F))
        {
            return 4;
        }
    }

    const float distance = 20.0F;

    if(!(direct_voice_gain(VoiceLevel::whisper, distance)
            < direct_voice_gain(VoiceLevel::quiet, distance)
        && direct_voice_gain(VoiceLevel::quiet, distance)
            < direct_voice_gain(VoiceLevel::normal, distance)
        && direct_voice_gain(VoiceLevel::normal, distance)
            < direct_voice_gain(VoiceLevel::raised, distance)
        && direct_voice_gain(VoiceLevel::raised, distance)
            < direct_voice_gain(VoiceLevel::shout, distance)))
    {
        return 5;
    }

    if(direct_voice_gain(VoiceLevel::shout, 10000.0F) <= 0.0F)
    {
        return 6;
    }

    return 0;
}
