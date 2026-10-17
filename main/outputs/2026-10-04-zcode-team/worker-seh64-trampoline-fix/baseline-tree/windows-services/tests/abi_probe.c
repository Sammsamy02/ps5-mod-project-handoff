/* Compile for both frontend targets: exact constant values and full LLVM data
 * layout must match before the Windows-ABI IR enters the PS5 backend. */
#include "../mb_win_crt_bind.h"

const unsigned long mb_win_crt_layout[] = {
    sizeof(void *), sizeof(size_t), sizeof(uintptr_t), sizeof(int),
    sizeof(mb_pe_string), _Alignof(mb_pe_string),
    offsetof(mb_pe_string, data), offsetof(mb_pe_string, size),
    sizeof(mb_pe_import), _Alignof(mb_pe_import),
    offsetof(mb_pe_import, library), offsetof(mb_pe_import, name),
    offsetof(mb_pe_import, iat_rva), offsetof(mb_pe_import, ordinal),
    offsetof(mb_pe_import, hint), offsetof(mb_pe_import, by_ordinal),
    sizeof(uint16_t), _Alignof(uint16_t)
};

mb_pe_binder mb_win_crt_native_callback(void) {
    return mb_win_crt_bind;
}
