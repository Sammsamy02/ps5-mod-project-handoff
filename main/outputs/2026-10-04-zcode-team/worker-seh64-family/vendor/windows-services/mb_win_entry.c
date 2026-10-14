#include "mb_win_entry.h"

#if !defined(__clang__) && !defined(__GNUC__)
#error "Windows entry bridge requires a compiler with ms_abi support"
#endif

typedef int32_t (__attribute__((ms_abi)) *mb_win_entry_function)(void *, uint32_t,
    void *);

_Static_assert(sizeof(mb_win_entry_function) == sizeof(uintptr_t),
    "AMD64 entry function addresses must fit native pointers");

int32_t mb_win_call_entry(void *unused, void *entry_address, void *image_base,
    uint32_t reason, void *reserved) {
    (void)unused;
    mb_win_entry_function entry = (mb_win_entry_function)(uintptr_t)entry_address;
    return entry(image_base, reason, reserved);
}
