#include <thread>
#include <pthread.h>
#include <type_traits>
#include <cstdint>
#include "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include/Unreal/Platform/NativeGameThreadIdentity.hpp"
extern "C" { extern const unsigned long long ThreadFacts[] = {
sizeof(void*),
sizeof(pthread_t),
alignof(pthread_t),
std::is_pointer_v<pthread_t>,
sizeof(std::thread::id),
alignof(std::thread::id),
std::is_same_v<decltype(pthread_self()),pthread_t>,
sizeof(std::uint32_t),
sizeof(int),
sizeof(std::uint32_t(*)())
}; }
std::thread::id CurrentStdThreadIdentity() { return std::this_thread::get_id(); }
bool CurrentStdThreadAffinity(RC::Unreal::NativeThreadDetail::Identity& state) { return state.in_current_raw(); }
