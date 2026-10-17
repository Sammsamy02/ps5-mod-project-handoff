// Shared per-host-thread Win32 error slot and the current guest x64 TEB view.
// The x64 TEB prefix used by this loader stores LastErrorValue at 0x68.
#ifndef CAP13_WIN32_LAST_ERROR_STATE_H
#define CAP13_WIN32_LAST_ERROR_STATE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cap13 {

constexpr size_t WIN32_TEB_LAST_ERROR_OFFSET = 0x68;

struct Win32LastErrorState
{
    uint32_t code = 0;
    uint8_t* active_teb = nullptr;
};

inline Win32LastErrorState& win32_last_error_state() noexcept
{
    static thread_local Win32LastErrorState state{};
    return state;
}

inline uint32_t win32_get_last_error() noexcept
{
    auto& state = win32_last_error_state();
    if (state.active_teb)
        std::memcpy(&state.code, state.active_teb + WIN32_TEB_LAST_ERROR_OFFSET,
                    sizeof(state.code));
    return state.code;
}

inline void win32_set_last_error(uint32_t code) noexcept
{
    auto& state = win32_last_error_state();
    state.code = code;
    if (state.active_teb)
        std::memcpy(state.active_teb + WIN32_TEB_LAST_ERROR_OFFSET, &code,
                    sizeof(code));
}

// Bind after the segment backend has accepted entry. Destroy before the
// backend leaves; direct guest writes are then copied to the host-thread
// slot and nested bindings restore their prior TEB view.
class ScopedGuestLastErrorBinding
{
  public:
    explicit ScopedGuestLastErrorBinding(void* guest_teb) noexcept
    {
        auto& state = win32_last_error_state();
        const uint32_t current = win32_get_last_error();
        previous_ = state.active_teb;
        state.active_teb = static_cast<uint8_t*>(guest_teb);
        if (state.active_teb)
            std::memcpy(state.active_teb + WIN32_TEB_LAST_ERROR_OFFSET,
                        &current, sizeof(current));
    }

    ~ScopedGuestLastErrorBinding() noexcept
    {
        auto& state = win32_last_error_state();
        const uint32_t current = win32_get_last_error();
        state.active_teb = previous_;
        if (previous_)
            std::memcpy(previous_ + WIN32_TEB_LAST_ERROR_OFFSET,
                        &current, sizeof(current));
    }

    ScopedGuestLastErrorBinding(const ScopedGuestLastErrorBinding&) = delete;
    ScopedGuestLastErrorBinding& operator=(const ScopedGuestLastErrorBinding&) = delete;

  private:
    uint8_t* previous_ = nullptr;
};

} // namespace cap13
#endif // CAP13_WIN32_LAST_ERROR_STATE_H
