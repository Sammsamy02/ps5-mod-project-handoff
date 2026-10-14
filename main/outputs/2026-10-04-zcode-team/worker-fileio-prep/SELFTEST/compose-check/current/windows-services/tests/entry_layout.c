#include "../mb_win_entry.h"

const unsigned long mb_win_entry_layout[] = {
    sizeof(void *), sizeof(uintptr_t), sizeof(int32_t), sizeof(uint32_t),
    _Alignof(void *), _Alignof(uint32_t)
};

typedef int32_t (*native_entry_callback)(void *, void *, void *, uint32_t, void *);
native_entry_callback mb_win_entry_native_callback(void) {
    return mb_win_call_entry;
}
