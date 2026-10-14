#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
namespace NativeNameThread {
struct Ops {void* context{};bool(*current)(void*,std::int32_t&){};bool(*name)(void*,std::uint32_t,const void*&){};};
enum class Result {Accepted,Cancelled,Unavailable,InvalidId,NotNamedGameThread,IdChanged,NativeInitConflict,AffinityForeign};
class Policy final {
 std::atomic<bool> cancelled_{false};std::mutex mutex_;
 std::uint32_t accepted_id_{};std::thread::id affinity_{};
public:
 void cancel() noexcept {cancelled_.store(true,std::memory_order_release);}
 Result authenticate(Ops,const void*) noexcept;
};
}
