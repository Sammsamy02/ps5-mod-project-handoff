#ifndef CAP13_WIN32_THREAD_IDENTITY_H
#define CAP13_WIN32_THREAD_IDENTITY_H

#include <cstdint>

namespace cap13 {

// Process-wide, non-reused DWORD identities. Zero signals exhaustion.
uint32_t win32_reserve_thread_id() noexcept;
bool win32_bind_current_thread_id(uint32_t id) noexcept;
uint32_t win32_current_thread_id() noexcept;

} // namespace cap13
#endif
