#pragma once

#include <icarus/core/spatial_scene_protocol.hpp>
#include <icarus/ipc/bridge.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace icarus::ipc
{
inline constexpr wchar_t default_spatial_scene_mapping_name[] = L"Local\\ICARUS.SpatialScene.1";

template<typename Payload>
struct SpatialSnapshot
{
    bool valid{};
    std::uint64_t sequence{};
    std::uint64_t updated_tick{};
    Payload payload{};
};

class SpatialSceneChannel
{
public:
    explicit SpatialSceneChannel(
        BridgeRole role,
        std::wstring mapping_name = default_spatial_scene_mapping_name
    );
    ~SpatialSceneChannel();

    SpatialSceneChannel(const SpatialSceneChannel&) = delete;
    SpatialSceneChannel& operator=(const SpatialSceneChannel&) = delete;
    SpatialSceneChannel(SpatialSceneChannel&&) = delete;
    SpatialSceneChannel& operator=(SpatialSceneChannel&&) = delete;

    [[nodiscard]] bool sync(std::uint64_t generation);
    void reset();

    [[nodiscard]] std::uint64_t generation() const;

    [[nodiscard]] bool publish_arma(const core::SpatialSceneState& state);
    [[nodiscard]] bool publish_voice_backend(const core::VoiceBackendSpatialState& state);

    [[nodiscard]] SpatialSnapshot<core::SpatialSceneState> read_arma() const;
    [[nodiscard]] SpatialSnapshot<core::VoiceBackendSpatialState> read_voice_backend() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
