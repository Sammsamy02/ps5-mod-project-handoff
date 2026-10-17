#pragma once
#include <atomic>
#include <cstdint>
namespace NativeThreadAudit {
struct EngineSample {std::uint8_t ready;std::uint32_t game_id;};
struct ReadOps {void* context{};bool(*snapshot)(void*,EngineSample&){};bool(*current_id)(void*,std::int32_t&){};};
enum class Admission {Accepted,Cancelled,ReadFailed,NotReady,InvalidId,SnapshotChanged,NativeForeign,InitConflict,AffinityForeign};
class StrictThreadPolicy {public:void cancel() noexcept {} Admission admit(ReadOps){return Admission::NotReady;}};
}
