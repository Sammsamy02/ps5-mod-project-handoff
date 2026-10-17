// boundaries.S — VERBATIM VENDOR from worker-ingame-probe-v3/src/
// boundaries.S (lineage: native-full-runtime-aligned-entry-1). Explicit
// init/fini array boundaries for the pad probe PRX: the packager writes
// DT_INIT=module_start / DT_FINI=module_stop and the loader never owns the
// arrays; this module is the sole .init_array/.fini_array contributor, so
// the boundaries deterministically bracket every entry (1 sentinel slot).
.section .init_array,"aw",@init_array
.balign 8
.globl __init_array_start
.hidden __init_array_start
__init_array_start:
.quad ig_ctor_sentinel
.globl __init_array_end
.hidden __init_array_end
__init_array_end:

.section .fini_array,"aw",@fini_array
.balign 8
.globl __fini_array_start
.hidden __fini_array_start
__fini_array_start:
.quad ig_fini_sentinel
.globl __fini_array_end
.hidden __fini_array_end
__fini_array_end:
