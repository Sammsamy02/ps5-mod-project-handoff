// Validate current-thread guest stack outputs during an active guest TEB call.
#ifndef CAP13_GUEST_STACK_WRITE_H
#define CAP13_GUEST_STACK_WRITE_H

#include "win32_last_error_state.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#if defined(__PROSPERO__)
#include <pthread_np.h>
#endif

namespace cap13 {

// Only the current x86-64 thread's caller stack above this function's frame
// is eligible. An inactive guest binding, another thread's stack, wrapping
// pointer, or unavailable OS stack bounds refuses without writing.
inline bool write_current_guest_stack(void* address, const void* source,
                                      size_t length) noexcept
{
    if (!win32_last_error_state().active_teb || !address || !source || !length)
        return false;
    uintptr_t low = 0, high = 0;
#if defined(__APPLE__)
    const pthread_t self = pthread_self();
    const uintptr_t top = reinterpret_cast<uintptr_t>(
        pthread_get_stackaddr_np(self));
    const size_t size = pthread_get_stacksize_np(self);
    if (!top || !size || size > top) return false;
    low = top - size;
    high = top;
#elif defined(__PROSPERO__)
    pthread_attr_t attr{};
    if (pthread_attr_get_np(pthread_self(), &attr) != 0) return false;
    void* base = nullptr;
    size_t size = 0;
    const int status = pthread_attr_getstack(&attr, &base, &size);
    (void)pthread_attr_destroy(&attr);
    const uintptr_t stack_begin = reinterpret_cast<uintptr_t>(base);
    if (status != 0 || !stack_begin || !size ||
        size > UINTPTR_MAX - stack_begin)
        return false;
    low = stack_begin;
    high = stack_begin + size;
#else
    return false;
#endif
    volatile uint8_t frame_marker = 0;
    const uintptr_t frame = reinterpret_cast<uintptr_t>(&frame_marker);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(address);
    // Guest Win64 entry currently uses the same downward-growing x86-64
    // host stack. The caller's output can only be above this deeper frame.
    if (frame < low || frame >= high || begin < frame || begin >= high ||
        length > high - begin)
        return false;
    std::memmove(address, source, length);
    return true;
}

} // namespace cap13
#endif
