/* host_prelude.h — HOST TEST ONLY (worker-c2-probe). Force-included before
   mapscan.c on the macOS host: Darwin's sys/sysctl.h lacks KERN_PROC_VMMAP
   (a FreeBSD/Orbis constant). The value only feeds the never-called real
   ig_mapscan_run (the host harness renames the call site); any value works. */
#ifndef KERN_PROC_VMMAP
#define KERN_PROC_VMMAP 14
#endif
