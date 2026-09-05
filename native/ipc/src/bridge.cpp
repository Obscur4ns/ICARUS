#include <icarus/ipc/bridge.hpp>

#include <icarus/core/bridge_protocol.hpp>

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
using icarus::core::BridgeEndpointSlot;
using icarus::core::BridgeLifecycle;
using icarus::core::BridgePage;
using icarus::ipc::BridgeConnectionState;
using icarus::ipc::BridgeRole;
using icarus::ipc::BridgeStatus;

constexpr std::uint64_t no_heartbeat_age = std::numeric_limits<std::uint64_t>::max();

std::uint64_t tick_now() noexcept
{
    return GetTickCount64();
}

std::uint64_t read_u64(const std::uint64_t& value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG64*>(const_cast<std::uint64_t*>(&value));
    return static_cast<std::uint64_t>(InterlockedCompareExchange64(pointer, 0, 0));
}

void write_u64(std::uint64_t& target, std::uint64_t value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG64*>(&target);
    InterlockedExchange64(pointer, static_cast<LONG64>(value));
}

std::uint64_t increment_u64(std::uint64_t& target) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG64*>(&target);
    return static_cast<std::uint64_t>(InterlockedIncrement64(pointer));
}

std::uint32_t read_u32(const std::uint32_t& value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG*>(const_cast<std::uint32_t*>(&value));
    return static_cast<std::uint32_t>(InterlockedCompareExchange(pointer, 0, 0));
}

void write_u32(std::uint32_t& target, std::uint32_t value) noexcept
{
    auto* pointer = reinterpret_cast<volatile LONG*>(&target);
    InterlockedExchange(pointer, static_cast<LONG>(value));
}

bool process_is_alive(std::uint64_t pid) noexcept
{
    if(pid == 0)
    {
        return false;
    }

    if(pid == GetCurrentProcessId())
    {
        return true;
    }

    const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));

    if(process == nullptr)
    {
        return GetLastError() == ERROR_ACCESS_DENIED;
    }

    const DWORD wait_result = WaitForSingleObject(process, 0);
    CloseHandle(process);

    return wait_result == WAIT_TIMEOUT;
}

std::uint64_t make_generation() noexcept
{
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);

    std::uint64_t value = static_cast<std::uint64_t>(counter.QuadPart);
    value ^= tick_now() << 1U;
    value ^= static_cast<std::uint64_t>(GetCurrentProcessId()) << 32U;
    value &= 0x7FFFFFFFFFFFFFFFULL;

    return value == 0 ? 1 : value;
}

void reset_slot(BridgeEndpointSlot& slot) noexcept
{
    write_u64(slot.lifecycle, static_cast<std::uint64_t>(BridgeLifecycle::offline));
    write_u64(slot.pid, 0);
    write_u64(slot.started_tick, 0);
    write_u64(slot.heartbeat_tick, 0);
    write_u64(slot.sequence, 0);
}

void register_slot(BridgeEndpointSlot& slot, std::uint64_t now) noexcept
{
    write_u64(slot.lifecycle, static_cast<std::uint64_t>(BridgeLifecycle::offline));
    write_u64(slot.pid, GetCurrentProcessId());
    write_u64(slot.started_tick, now);
    write_u64(slot.heartbeat_tick, now);
    write_u64(slot.sequence, 1);
    MemoryBarrier();
    write_u64(slot.lifecycle, static_cast<std::uint64_t>(BridgeLifecycle::online));
}

void heartbeat_slot(BridgeEndpointSlot& slot, std::uint64_t now) noexcept
{
    write_u64(slot.heartbeat_tick, now);
    increment_u64(slot.sequence);
}

void stop_slot(BridgeEndpointSlot& slot) noexcept
{
    write_u64(slot.lifecycle, static_cast<std::uint64_t>(BridgeLifecycle::stopping));
    MemoryBarrier();
    write_u64(slot.lifecycle, static_cast<std::uint64_t>(BridgeLifecycle::offline));
}

struct PeerSnapshot
{
    bool online{};
    std::uint64_t pid{};
    std::uint64_t heartbeat_age_ms{no_heartbeat_age};
};

PeerSnapshot read_peer(const BridgeEndpointSlot& slot, std::uint64_t now) noexcept
{
    const auto lifecycle_before = static_cast<BridgeLifecycle>(read_u64(slot.lifecycle));

    if(lifecycle_before != BridgeLifecycle::online)
    {
        return {};
    }

    const std::uint64_t pid = read_u64(slot.pid);
    const std::uint64_t heartbeat = read_u64(slot.heartbeat_tick);
    const auto lifecycle_after = static_cast<BridgeLifecycle>(read_u64(slot.lifecycle));

    if(lifecycle_after != BridgeLifecycle::online || pid == 0 || heartbeat == 0)
    {
        return {};
    }

    const std::uint64_t age = now >= heartbeat ? now - heartbeat : 0;

    return {
        .online = age <= icarus::core::bridge_peer_timeout_ms,
        .pid = pid,
        .heartbeat_age_ms = age,
    };
}
}

namespace icarus::ipc
{
const char* to_string(BridgeConnectionState state) noexcept
{
    switch(state)
    {
        case BridgeConnectionState::stopped:
            return "stopped";
        case BridgeConnectionState::waiting_for_peer:
            return "waiting_for_peer";
        case BridgeConnectionState::connected:
            return "connected";
        case BridgeConnectionState::protocol_mismatch:
            return "protocol_mismatch";
        case BridgeConnectionState::owner_conflict:
            return "owner_conflict";
        case BridgeConnectionState::transport_error:
            return "transport_error";
    }

    return "unknown";
}

class BridgeEndpoint::Impl
{
public:
    Impl(BridgeRole role, std::wstring mapping_name)
        : role_(role),
          mapping_name_(std::move(mapping_name))
    {
        status_.protocol_major = core::bridge_protocol_major;
        status_.protocol_minor = core::bridge_protocol_minor;
        status_.local_pid = GetCurrentProcessId();
        status_.peer_heartbeat_age_ms = no_heartbeat_age;
    }

    ~Impl()
    {
        stop();
    }

    bool start()
    {
        std::scoped_lock lock(start_stop_mutex_);

        if(running_)
        {
            return true;
        }

        if(role_ == BridgeRole::arma && !claim_arma_mapping())
        {
            return false;
        }

        running_ = true;

        try
        {
            worker_ = std::jthread([this](std::stop_token stop_token) {
                run(stop_token);
            });
        }
        catch(...)
        {
            running_ = false;
            close_mapping(false);
            set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
            return false;
        }

        return true;
    }

    void stop()
    {
        std::scoped_lock lock(start_stop_mutex_);

        if(!running_)
        {
            return;
        }

        worker_.request_stop();

        if(worker_.joinable())
        {
            worker_.join();
        }

        if(page_ != nullptr && generation_matches())
        {
            stop_slot(local_slot());
        }

        close_mapping(false);
        running_ = false;
        observed_generation_ = 0;
        set_status(BridgeConnectionState::stopped, 0, no_heartbeat_age);
    }

    bool running() const noexcept
    {
        return running_.load();
    }

    BridgeStatus status() const
    {
        std::scoped_lock lock(status_mutex_);
        return status_;
    }

private:
    bool claim_arma_mapping()
    {
        mapping_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(sizeof(core::BridgePage)),
            mapping_name_.c_str()
        );

        if(mapping_ == nullptr)
        {
            set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
            return false;
        }

        const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;

        page_ = static_cast<core::BridgePage*>(
            MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(core::BridgePage))
        );

        if(page_ == nullptr)
        {
            close_mapping(false);
            set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
            return false;
        }

        if(!existed)
        {
            initialise_new_page();
        }
        else
        {
            if(!header_compatible())
            {
                close_mapping(false);
                set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
                return false;
            }

            const std::uint64_t existing_pid = read_u64(page_->arma.pid);

            if(existing_pid != 0 && existing_pid != GetCurrentProcessId() && process_is_alive(existing_pid))
            {
                close_mapping(false);
                set_status(BridgeConnectionState::owner_conflict, existing_pid, no_heartbeat_age);
                return false;
            }

            begin_new_generation();
        }

        observed_generation_ = read_u64(page_->header.generation);
        set_status(BridgeConnectionState::waiting_for_peer, 0, no_heartbeat_age);
        return true;
    }

    void initialise_new_page() noexcept
    {
        write_u32(page_->header.ready, 0);
        page_->header.magic = core::bridge_magic;
        page_->header.protocol_major = core::bridge_protocol_major;
        page_->header.protocol_minor = core::bridge_protocol_minor;
        page_->header.struct_size = static_cast<std::uint32_t>(sizeof(core::BridgePage));

        reset_slot(page_->arma);
        reset_slot(page_->teamspeak);

        const std::uint64_t generation = make_generation();
        write_u64(page_->header.generation, generation);
        register_slot(page_->arma, tick_now());

        MemoryBarrier();
        write_u32(page_->header.ready, 1);
    }

    void begin_new_generation() noexcept
    {
        write_u32(page_->header.ready, 0);
        MemoryBarrier();

        reset_slot(page_->arma);
        reset_slot(page_->teamspeak);

        const std::uint64_t generation = make_generation();
        write_u64(page_->header.generation, generation);
        register_slot(page_->arma, tick_now());

        MemoryBarrier();
        write_u32(page_->header.ready, 1);
    }

    bool open_teamspeak_mapping()
    {
        mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapping_name_.c_str());

        if(mapping_ == nullptr)
        {
            const DWORD error = GetLastError();

            if(error == ERROR_FILE_NOT_FOUND)
            {
                set_status(BridgeConnectionState::waiting_for_peer, 0, no_heartbeat_age);
            }
            else
            {
                set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
            }

            return false;
        }

        page_ = static_cast<core::BridgePage*>(
            MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(core::BridgePage))
        );

        if(page_ == nullptr)
        {
            close_mapping(false);
            set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
            return false;
        }

        return true;
    }

    bool header_compatible() const noexcept
    {
        if(page_ == nullptr || read_u32(page_->header.ready) != 1)
        {
            return false;
        }

        MemoryBarrier();

        return page_->header.magic == core::bridge_magic
            && page_->header.protocol_major == core::bridge_protocol_major
            && page_->header.struct_size >= sizeof(core::BridgePage);
    }

    bool protocol_matches() const noexcept
    {
        return page_ != nullptr
            && page_->header.magic == core::bridge_magic
            && page_->header.protocol_major == core::bridge_protocol_major;
    }

    bool generation_matches() const noexcept
    {
        return page_ != nullptr
            && observed_generation_ != 0
            && read_u64(page_->header.generation) == observed_generation_;
    }

    BridgeEndpointSlot& local_slot() noexcept
    {
        return role_ == BridgeRole::arma ? page_->arma : page_->teamspeak;
    }

    BridgeEndpointSlot& peer_slot() noexcept
    {
        return role_ == BridgeRole::arma ? page_->teamspeak : page_->arma;
    }

    void run(std::stop_token stop_token)
    {
        while(!stop_token.stop_requested())
        {
            if(role_ == BridgeRole::teamspeak && page_ == nullptr)
            {
                open_teamspeak_mapping();
            }

            if(page_ != nullptr)
            {
                update();
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(core::bridge_heartbeat_interval_ms)
            );
        }
    }

    void update()
    {
        if(read_u32(page_->header.ready) != 1)
        {
            set_status(BridgeConnectionState::waiting_for_peer, 0, no_heartbeat_age);
            return;
        }

        MemoryBarrier();

        if(!protocol_matches())
        {
            set_status(BridgeConnectionState::protocol_mismatch, 0, no_heartbeat_age);
            return;
        }

        if(page_->header.struct_size < sizeof(core::BridgePage))
        {
            set_status(BridgeConnectionState::transport_error, 0, no_heartbeat_age);
            return;
        }

        const std::uint64_t generation = read_u64(page_->header.generation);

        if(generation == 0)
        {
            set_status(BridgeConnectionState::waiting_for_peer, 0, no_heartbeat_age);
            return;
        }

        if(generation != observed_generation_)
        {
            observed_generation_ = generation;
            register_slot(local_slot(), tick_now());
        }

        const std::uint64_t now = tick_now();
        heartbeat_slot(local_slot(), now);

        const PeerSnapshot peer = read_peer(peer_slot(), now);

        if(peer.online)
        {
            set_status(BridgeConnectionState::connected, peer.pid, peer.heartbeat_age_ms);
            return;
        }

        set_status(BridgeConnectionState::waiting_for_peer, peer.pid, peer.heartbeat_age_ms);

        if(peer.pid != 0 && !process_is_alive(peer.pid))
        {
            if(role_ == BridgeRole::arma)
            {
                reset_slot(peer_slot());
            }
            else
            {
                stop_slot(local_slot());
                close_mapping(false);
                observed_generation_ = 0;
            }
        }
    }

    void close_mapping(bool clear_local) noexcept
    {
        if(clear_local && page_ != nullptr && generation_matches())
        {
            stop_slot(local_slot());
        }

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
    }

    void set_status(
        BridgeConnectionState state,
        std::uint64_t peer_pid,
        std::uint64_t peer_heartbeat_age_ms
    )
    {
        std::scoped_lock lock(status_mutex_);

        status_.state = state;
        status_.protocol_major = core::bridge_protocol_major;
        status_.protocol_minor = core::bridge_protocol_minor;
        status_.generation = observed_generation_;
        status_.local_pid = GetCurrentProcessId();
        status_.peer_pid = peer_pid;
        status_.peer_heartbeat_age_ms = peer_heartbeat_age_ms;
    }

    BridgeRole role_;
    std::wstring mapping_name_;

    HANDLE mapping_{};
    core::BridgePage* page_{};
    std::uint64_t observed_generation_{};

    std::atomic_bool running_{false};
    std::jthread worker_;
    mutable std::mutex start_stop_mutex_;

    mutable std::mutex status_mutex_;
    BridgeStatus status_;
};

BridgeEndpoint::BridgeEndpoint(BridgeRole role, std::wstring mapping_name)
    : impl_(std::make_unique<Impl>(role, std::move(mapping_name)))
{
}

BridgeEndpoint::~BridgeEndpoint() = default;

bool BridgeEndpoint::start()
{
    return impl_->start();
}

void BridgeEndpoint::stop()
{
    impl_->stop();
}

bool BridgeEndpoint::running() const noexcept
{
    return impl_->running();
}

BridgeStatus BridgeEndpoint::status() const
{
    return impl_->status();
}
}
