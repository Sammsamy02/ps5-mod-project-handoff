#define MONO_GENERATING_OFFSETS 1
#include "/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap10-cross-aot/cross-work/mono/mini/mini-cross-helpers.c"

#undef DISABLE_JIT_OFFSETS
#define DISABLE_METADATA_OFFSETS

#define DECL_OFFSET(type,field) const unsigned long offset__##type##__##field = __builtin_offsetof(type,field);
#define DECL_OFFSET2(type,field,offset) unexpected_cross_offset
#define DECL_ALIGN2(type,size) unexpected_cross_alignment
#define DECL_SIZE(type) const unsigned long size__##type = sizeof(type);
#define DECL_SIZE2(type,size) unexpected_cross_size
#include <mono/metadata/object-offsets.h>
