// boundaries.S — explicit init/fini array boundaries for the facts PRX.
//
// The packager writes DT_INIT=module_start / DT_FINI=module_stop and the
// loader never owns the arrays (explicitLifecycleOwnsArrays). This module is
// the ONLY contributor of .init_array/.fini_array content, so the boundary
// symbols defined here bracket every entry deterministically:
//   __init_array_start .. __init_array_end   (1 sentinel slot)
//   __fini_array_start  .. __fini_array_end  (1 sentinel slot)
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
