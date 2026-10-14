#define MONO_GENERATING_OFFSETS 1
#include "/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap10-cross-aot/cross-work/mono/metadata/metadata-cross-helpers.c"

#undef DISABLE_METADATA_OFFSETS
#define DISABLE_JIT_OFFSETS
const unsigned long align__gint8 = __alignof__(gint8);
const unsigned long align__gint16 = __alignof__(gint16);
const unsigned long align__gint32 = __alignof__(gint32);
const unsigned long align__gint64 = __alignof__(gint64);
const unsigned long align__float = __alignof__(float);
const unsigned long align__double = __alignof__(double);
const unsigned long align__gpointer = __alignof__(gpointer);

#define DECL_OFFSET(type,field) const unsigned long offset__##type##__##field = __builtin_offsetof(type,field);
#define DECL_OFFSET2(type,field,offset) unexpected_cross_offset
#define DECL_ALIGN2(type,size) unexpected_cross_alignment
#define DECL_SIZE(type) const unsigned long size__##type = sizeof(type);
#define DECL_SIZE2(type,size) unexpected_cross_size
#include <mono/metadata/object-offsets.h>
