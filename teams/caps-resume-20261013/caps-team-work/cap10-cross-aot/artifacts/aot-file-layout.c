#define MONO_GENERATING_OFFSETS 1
#include "mono/mini/mini-cross-helpers.c"
#include "mono/mini/aot-runtime.h"
const unsigned long off_version=__builtin_offsetof(MonoAotFileInfo,version);
const unsigned long off_flags=__builtin_offsetof(MonoAotFileInfo,flags);
const unsigned long off_nmethods=__builtin_offsetof(MonoAotFileInfo,nmethods);
const unsigned long off_nextra_methods=__builtin_offsetof(MonoAotFileInfo,nextra_methods);
const unsigned long off_double_align=__builtin_offsetof(MonoAotFileInfo,double_align);
const unsigned long off_long_align=__builtin_offsetof(MonoAotFileInfo,long_align);
const unsigned long off_card_table_shift_bits=__builtin_offsetof(MonoAotFileInfo,card_table_shift_bits);
const unsigned long off_card_table_mask=__builtin_offsetof(MonoAotFileInfo,card_table_mask);
const unsigned long off_aotid=__builtin_offsetof(MonoAotFileInfo,aotid);
