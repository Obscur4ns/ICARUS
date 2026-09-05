#include <icarus/core/session_state_protocol.hpp>
#include <icarus/ipc/session_state.hpp>

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
        L"Local\\ICARUS.SessionState.Test." + std::to_wstring(GetCurrentProcessId());

    constexpr std::uint64_t first_generation = 0x12345678ULL;
    constexpr std::uint64_t second_generation = 0x87654321ULL;

    icarus::ipc::SessionStateChannel arma(icarus::ipc::BridgeRole::arma, mapping_name);
    icarus::ipc::SessionStateChannel voice(icarus::ipc::BridgeRole::teamspeak, mapping_name);

    if(!arma.sync(first_generation))
    {
        return 1;
    }

    if(!voice.sync(first_generation))
    {
        return 2;
    }

    icarus::core::ArmaSessionState arma_state{};
    arma_state.session_id = first_generation;
    arma_state.position[0] = 10.0F;
    arma_state.position[1] = 20.0F;
    arma_state.position[2] = 30.0F;
    arma_state.head_direction[0] = 0.0F;
    arma_state.head_direction[1] = 1.0F;
    arma_state.head_direction[2] = 0.0F;
    arma_state.flags =
        static_cast<std::uint32_t>(icarus::core::ArmaSessionFlag::alive)
        | static_cast<std::uint32_t>(icarus::core::ArmaSessionFlag::in_vehicle);
    arma_state.voice_level = static_cast<std::uint32_t>(icarus::core::VoiceLevel::normal);
    arma_state.vehicle_role = static_cast<std::uint32_t>(icarus::core::VehicleRole::driver);
    copy_text(arma_state.player_uid, "76561198000000000");
    copy_text(arma_state.network_id, "2:41");
    copy_text(arma_state.vehicle_network_id, "2:77");

    if(!arma.publish_arma(arma_state))
    {
        return 3;
    }

    const auto received_arma = voice.read_arma();

    if(!received_arma.valid)
    {
        return 4;
    }

    if(received_arma.payload.session_id != first_generation)
    {
        return 5;
    }

    if(std::strcmp(received_arma.payload.player_uid, "76561198000000000") != 0)
    {
        return 6;
    }

    if(std::fabs(received_arma.payload.position[1] - 20.0F) > 0.001F)
    {
        return 7;
    }

    icarus::core::VoiceBackendSessionState voice_state{};
    voice_state.connection_id = 12;
    voice_state.acknowledged_session_id = received_arma.payload.session_id;
    voice_state.acknowledged_arma_sequence = received_arma.sequence;
    voice_state.local_client_id = 93;
    voice_state.connection_state =
        static_cast<std::uint32_t>(icarus::core::VoiceBackendConnectionState::established);
    voice_state.health =
        static_cast<std::uint32_t>(icarus::core::VoiceBackendHealth::ready);
    voice_state.state_protocol_major = icarus::core::session_state_protocol_major;
    voice_state.state_protocol_minor = icarus::core::session_state_protocol_minor;

    if(!voice.publish_voice_backend(voice_state))
    {
        return 8;
    }

    const auto received_voice = arma.read_voice_backend();

    if(!received_voice.valid)
    {
        return 9;
    }

    if(received_voice.payload.local_client_id != 93)
    {
        return 10;
    }

    if(received_voice.payload.acknowledged_arma_sequence != received_arma.sequence)
    {
        return 11;
    }

    if(!arma.sync(second_generation))
    {
        return 12;
    }

    if(voice.sync(first_generation))
    {
        return 13;
    }

    if(!voice.sync(second_generation))
    {
        return 14;
    }

    if(voice.read_arma().valid)
    {
        return 15;
    }

    return 0;
}
