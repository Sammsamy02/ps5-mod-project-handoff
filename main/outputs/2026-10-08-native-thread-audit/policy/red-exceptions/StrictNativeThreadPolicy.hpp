#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
namespace NativeThreadAudit {
// Snapshot readers own image/address authentication and failure reporting.
// This policy never reads a game address or interprets an opaque thread ID.
struct EngineSample {std::uint8_t ready;std::uint32_t game_id;};
struct ReadOps {void* context{};bool(*snapshot)(void*,EngineSample&){};bool(*current_id)(void*,std::int32_t&){};};
enum class Admission {Accepted,Cancelled,ReadFailed,NotReady,InvalidId,SnapshotChanged,NativeForeign,InitConflict,AffinityForeign};
class StrictThreadPolicy final {
    std::atomic<bool> cancelled_{false};
    std::mutex mutex_;
    std::uint32_t engine_id_{};
    std::thread::id affinity_{};
    static bool positive(std::uint32_t id) noexcept {return id!=0&&id<=0x7fffffffU;}
public:
    void cancel() noexcept {cancelled_.store(true,std::memory_order_release);}
    Admission admit(ReadOps reader) {
        if(cancelled_.load(std::memory_order_acquire)) return Admission::Cancelled;
        if(!reader.snapshot||!reader.current_id) return Admission::ReadFailed;
        EngineSample before{},after{};
        if(!reader.snapshot(reader.context,before)) return Admission::ReadFailed;
        if(cancelled_.load(std::memory_order_acquire)) return Admission::Cancelled;
        if(before.ready!=1) return Admission::NotReady;
        if(!positive(before.game_id)) return Admission::InvalidId;
        std::int32_t current{};
        if(!reader.current_id(reader.context,current)) return Admission::ReadFailed;
        if(cancelled_.load(std::memory_order_acquire)) return Admission::Cancelled;
        if(current<=0) return Admission::InvalidId;
        if(!reader.snapshot(reader.context,after)) return Admission::ReadFailed;
        if(cancelled_.load(std::memory_order_acquire)) return Admission::Cancelled;
        if(before.ready!=after.ready||before.game_id!=after.game_id) return Admission::SnapshotChanged;
        if(static_cast<std::uint32_t>(current)!=before.game_id) return Admission::NativeForeign;
        const auto affinity=std::this_thread::get_id();
        std::lock_guard lock{mutex_};
        if(cancelled_.load(std::memory_order_acquire)) return Admission::Cancelled;
        if(engine_id_&&engine_id_!=before.game_id) return Admission::InitConflict;
        if(affinity_!=std::thread::id{}&&affinity_!=affinity) return Admission::AffinityForeign;
        engine_id_=before.game_id;
        affinity_=affinity;
        return Admission::Accepted;
    }
};
}
