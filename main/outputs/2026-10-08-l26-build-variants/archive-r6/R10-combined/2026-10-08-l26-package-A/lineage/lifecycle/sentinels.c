#include "../probe/probe_api.h"
typedef void (*callback)(void);
static void ctor1(void) { probe_event("ctor-1"); }
static void ctor2(void) { probe_event("ctor-2"); }
static void fini1(void) { probe_event("fini-1"); }
static void fini2(void) { probe_event("fini-2"); }
/* Native ELF arrays: ascending init priority; cleanup walks reverse order. */
static callback const init1 __attribute__((used,section(".init_array.00101")))=ctor1;
static callback const init2 __attribute__((used,section(".init_array.00102")))=ctor2;
static callback const end1 __attribute__((used,section(".fini_array.00101")))=fini1;
static callback const end2 __attribute__((used,section(".fini_array.00102")))=fini2;
