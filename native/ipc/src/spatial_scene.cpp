#include <icarus/ipc/spatial_scene.hpp>

#include <Windows.h>

#include <cstring>
#include <mutex>
#include <utility>

namespace
{
std::uint64_t tick_now() noexcept
{
    return GetTickCount64();
}

std::uint64_t read_u64(const std::uint64_t& value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG64*>(const_cast<std::uint64_t*>(&value));
    return static_cast<std::uint64_t>(InterlockedCompareExchange64(pointer, 0, 0));
}

std::uint64_t increment_u64(std::uint64_t& value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG64*>(&value);
    return static_cast<std::uint64_t>(InterlockedIncrement64(pointer));
}

std::uint32_t read_u32(const std::uint32_t& value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG*>(const_cast<std::uint32_t*>(&value));
    return static_cast<std::uint32_t>(InterlockedCompareExchange(pointer, 0, 0));
}

void write_u32(std::uint32_t& value, std::uint32_t next) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG*>(&value);
    InterlockedExchange(pointer, static_cast<LONG>(next));
}

template<typename Payload>
bool write_snapshot(
    icarus::core::SpatialStateSlot<Payload>& slot,
    const Payload& payload
) noexcept
{
    std::uint64_t sequence = increment_u64(slot.sequence);

    if((sequence & 1U) == 0U)
    {
        sequence = increment_u64(slot.sequence);
    }

    static_cast<void>(sequence);

    MemoryBarrier();
    slot.updated_tick = tick_now();
    std::memcpy(&slot.payload, &payload, sizeof(Payload));
    MemoryBarrier();

    increment_u64(slot.sequence);
    return true;
}

template<typename Payload>
icarus::ipc::SpatialSnapshot<Payload> read_snapshot(
    const icarus::core::SpatialStateSlot<Payload>& slot
) noexcept
{
    for(int attempt = 0; attempt < 5; ++attempt)
    {
        const std::uint64_t sequence_before = read_u64(slot.sequence);

        if(sequence_before == 0 || (sequence_before & 1U) != 0U)
        {
            continue;
        }

        MemoryBarrier();

        icarus::ipc::SpatialSnapshot<Payload> snapshot{};
        snapshot.sequence = sequence_before;
        snapshot.updated_tick = slot.updated_tick;
        std::memcpy(&snapshot.payload, &slot.payload, sizeof(Payload));

        MemoryBarrier();

        const std::uint64_t sequence_after = read_u64(slot.sequence);

        if(sequence_before == sequence_after && (sequence_after & 1U) == 0U)
        {
            snapshot.valid = true;
            return snapshot;
        }
    }

    return {};
}
}

namespace icarus::ipc
{
class SpatialSceneChannel::Impl
{
public:
    Impl(BridgeRole role, std::wstring mapping_name)
        : role_(role),
          mapping_name_(std::move(mapping_name))
    {
    }

    ~Impl()
    {
        reset();
    }

    bool sync(std::uint64_t generation)
    {
        std::scoped_lock lock(mutex_);

        if(generation == 0)
        {
            reset_unlocked();
            return false;
        }

        if(page_ != nullptr && generation_ == generation && page_compatible(generation))
        {
            return true;
        }

        reset_unlocked();

        if(role_ == BridgeRole::arma)
        {
            return create_arma_mapping(generation);
        }

        return open_voice_mapping(generation);
    }

    void reset()
    {
        std::scoped_lock lock(mutex_);
        reset_unlocked();
    }

    std::uint64_t generation() const
    {
        std::scoped_lock lock(mutex_);
        return generation_;
    }

    bool publish_arma(const core::SpatialSceneState& state)
    {
        std::scoped_lock lock(mutex_);

        if(role_ != BridgeRole::arma || !page_current())
        {
            return false;
        }

        return write_snapshot(page_->arma, state);
    }

    bool publish_voice_backend(const core::VoiceBackendSpatialState& state)
    {
        std::scoped_lock lock(mutex_);

        if(role_ != BridgeRole::teamspeak || !page_current())
        {
            return false;
        }

        return write_snapshot(page_->voice_backend, state);
    }

    SpatialSnapshot<core::SpatialSceneState> read_arma() const
    {
        std::scoped_lock lock(mutex_);

        if(!page_current())
        {
            return {};
        }

        return read_snapshot(page_->arma);
    }

    SpatialSnapshot<core::VoiceBackendSpatialState> read_voice_backend() const
    {
        std::scoped_lock lock(mutex_);

        if(!page_current())
        {
            return {};
        }

        return read_snapshot(page_->voice_backend);
    }

private:
    bool create_arma_mapping(std::uint64_t generation)
    {
        mapping_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(sizeof(core::SpatialScenePage)),
            mapping_name_.c_str()
        );

        if(mapping_ == nullptr)
        {
            return false;
        }

        page_ = static_cast<core::SpatialScenePage*>(
            MapViewOfFile(
                mapping_,
                FILE_MAP_ALL_ACCESS,
                0,
                0,
                sizeof(core::SpatialScenePage)
            )
        );

        if(page_ == nullptr)
        {
            reset_unlocked();
            return false;
        }

        write_u32(page_->header.ready, 0);
        MemoryBarrier();

        std::memset(page_, 0, sizeof(core::SpatialScenePage));

        page_->header.magic = core::spatial_scene_magic;
        page_->header.protocol_major = core::spatial_scene_protocol_major;
        page_->header.protocol_minor = core::spatial_scene_protocol_minor;
        page_->header.struct_size = static_cast<std::uint32_t>(sizeof(core::SpatialScenePage));
        page_->header.generation = generation;

        MemoryBarrier();
        write_u32(page_->header.ready, 1);

        generation_ = generation;
        return true;
    }

    bool open_voice_mapping(std::uint64_t generation)
    {
        mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapping_name_.c_str());

        if(mapping_ == nullptr)
        {
            return false;
        }

        page_ = static_cast<core::SpatialScenePage*>(
            MapViewOfFile(
                mapping_,
                FILE_MAP_ALL_ACCESS,
                0,
                0,
                sizeof(core::SpatialScenePage)
            )
        );

        if(page_ == nullptr)
        {
            reset_unlocked();
            return false;
        }

        if(!page_compatible(generation))
        {
            reset_unlocked();
            return false;
        }

        generation_ = generation;
        return true;
    }

    bool page_compatible(std::uint64_t generation) const noexcept
    {
        if(page_ == nullptr || read_u32(page_->header.ready) != 1)
        {
            return false;
        }

        MemoryBarrier();

        return page_->header.magic == core::spatial_scene_magic
            && page_->header.protocol_major == core::spatial_scene_protocol_major
            && page_->header.struct_size >= sizeof(core::SpatialScenePage)
            && read_u64(page_->header.generation) == generation;
    }

    bool page_current() const noexcept
    {
        return generation_ != 0 && page_compatible(generation_);
    }

    void reset_unlocked() noexcept
    {
        if(page_ != nullptr)
        {
            UnmapViewOfFile(page_);
            page_ = nullptr;
        }

        if(mapping_ != nullptr)
        {
            CloseHandle(mapping_);
            mapping_ = nullptr;
        }

        generation_ = 0;
    }

    BridgeRole role_;
    std::wstring mapping_name_;

    HANDLE mapping_{};
    core::SpatialScenePage* page_{};
    std::uint64_t generation_{};

    mutable std::mutex mutex_;
};

SpatialSceneChannel::SpatialSceneChannel(BridgeRole role, std::wstring mapping_name)
    : impl_(std::make_unique<Impl>(role, std::move(mapping_name)))
{
}

SpatialSceneChannel::~SpatialSceneChannel() = default;

bool SpatialSceneChannel::sync(std::uint64_t generation)
{
    return impl_->sync(generation);
}

void SpatialSceneChannel::reset()
{
    impl_->reset();
}

std::uint64_t SpatialSceneChannel::generation() const
{
    return impl_->generation();
}

bool SpatialSceneChannel::publish_arma(const core::SpatialSceneState& state)
{
    return impl_->publish_arma(state);
}

bool SpatialSceneChannel::publish_voice_backend(const core::VoiceBackendSpatialState& state)
{
    return impl_->publish_voice_backend(state);
}

SpatialSnapshot<core::SpatialSceneState> SpatialSceneChannel::read_arma() const
{
    return impl_->read_arma();
}

SpatialSnapshot<core::VoiceBackendSpatialState> SpatialSceneChannel::read_voice_backend() const
{
    return impl_->read_voice_backend();
}
}
