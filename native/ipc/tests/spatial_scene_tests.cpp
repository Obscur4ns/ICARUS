#include <icarus/core/session_state_protocol.hpp>
#include <icarus/core/spatial_scene_protocol.hpp>
#include <icarus/ipc/spatial_scene.hpp>

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace
{
template<std::size_t Size>
void copy_text(char (&target)[Size], const char* value)
{
    const std::size_t length = std::min(std::strlen(value), Size - 1);
    std::memcpy(target, value, length);
    target[length] = '\0';
}
}

int main()
{
    const std::wstring mapping_name =
        L"Local\\ICARUS.SpatialScene.Test." + std::to_wstring(GetCurrentProcessId());

    constexpr std::uint64_t first_generation = 0x11223344ULL;
    constexpr std::uint64_t second_generation = 0x55667788ULL;

    icarus::ipc::SpatialSceneChannel arma(icarus::ipc::BridgeRole::arma, mapping_name);
    icarus::ipc::SpatialSceneChannel voice(icarus::ipc::BridgeRole::teamspeak, mapping_name);

    if(!arma.sync(first_generation) || !voice.sync(first_generation))
    {
        return 1;
    }

    icarus::core::SpatialSceneState scene{};
    scene.actor_count = 2;

    auto& first = scene.actors[0];
    copy_text(first.player_uid, "76561198000000001");
    copy_text(first.network_id, "2:41");
    first.position[0] = 10.0F;
    first.position[1] = 20.0F;
    first.position[2] = 30.0F;
    first.head_direction[1] = 1.0F;
    first.velocity[0] = 2.5F;
    first.flags = static_cast<std::uint32_t>(icarus::core::SpatialActorFlag::alive);
    first.voice_level = static_cast<std::uint32_t>(icarus::core::VoiceLevel::normal);

    auto& second = scene.actors[1];
    copy_text(second.player_uid, "76561198000000002");
    copy_text(second.network_id, "2:42");
    second.position[0] = 100.0F;
    second.flags = static_cast<std::uint32_t>(icarus::core::SpatialActorFlag::alive)
        | static_cast<std::uint32_t>(icarus::core::SpatialActorFlag::in_vehicle);
    second.voice_level = static_cast<std::uint32_t>(icarus::core::VoiceLevel::shout);

    if(!arma.publish_arma(scene))
    {
        return 2;
    }

    const auto received = voice.read_arma();

    if(!received.valid || received.payload.actor_count != 2)
    {
        return 3;
    }

    if(std::strcmp(received.payload.actors[0].network_id, "2:41") != 0)
    {
        return 4;
    }

    if(std::fabs(received.payload.actors[0].velocity[0] - 2.5F) > 0.001F)
    {
        return 5;
    }

    if(!icarus::core::has_flag(
           received.payload.actors[1].flags,
           icarus::core::SpatialActorFlag::in_vehicle
       ))
    {
        return 6;
    }

    icarus::core::VoiceBackendSpatialState acknowledgement{};
    acknowledgement.acknowledged_scene_sequence = received.sequence;
    acknowledgement.observed_actor_count = received.payload.actor_count;
    acknowledgement.matched_actor_count = 1;

    if(!voice.publish_voice_backend(acknowledgement))
    {
        return 7;
    }

    const auto received_ack = arma.read_voice_backend();

    if(!received_ack.valid
        || received_ack.payload.acknowledged_scene_sequence != received.sequence
        || received_ack.payload.observed_actor_count != 2
        || received_ack.payload.matched_actor_count != 1)
    {
        return 8;
    }

    if(!arma.sync(second_generation))
    {
        return 9;
    }

    if(voice.sync(first_generation))
    {
        return 10;
    }

    if(!voice.sync(second_generation))
    {
        return 11;
    }

    if(voice.read_arma().valid)
    {
        return 12;
    }

    return 0;
}
