
#include "polyhook2/PolyHookOs.hpp"
#if !defined(POLYHOOK2_OS_PROSPERO)
#error "branch control: POLYHOOK2_OS_PROSPERO not selected"
#endif
#if !defined(POLYHOOK2_ARCH_X64)
#error "branch control: POLYHOOK2_ARCH_X64 not selected"
#endif
#include "polyhook2/PolyHookProsperoPlatform.hpp"
#include "polyhook2/MemAccessor.hpp"
#include "polyhook2/MemProtector.hpp"
#include "polyhook2/Misc.hpp"
// The header core is platform-independent: translation sanity at compile time.
static_assert(plh_prospero::translate_plh_to_prot(plh_prospero::kPlhRwx) == 7, "rwx");
static_assert(plh_prospero::translate_prot_to_plh(3) ==
              (plh_prospero::kPlhR | plh_prospero::kPlhW), "rw");
static_assert(plh_prospero::kPage16K == 0x4000u, "page");
static_assert(plh_prospero::kVmHdr == 136u, "vmhdr");
static_assert(plh_prospero::kKveSizeofCap == 1160u, "kvecap");
int branch_control() { return 0; }
