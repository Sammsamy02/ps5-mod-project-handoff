# l24 local package verification

Two independently rebuilt runtimes (l24-build-r2 twin pair, sha256
`c25130952d67f7b519cfa03df2143c2f9676f9ef469d23a1e866402c52152dec`,
119,827,176 bytes each, byte-identical) were packaged in two separate passes.
Both packaged PRXs are byte-identical and pass the retained 20 static gates,
preserve constructor/destructor arrays (238 init / 2 fini slots), and retain
exactly the 207 l23 provider import identities (no additions, no removals;
byte-for-byte equal to the l23 baseline including NID/module/library/version
rows and published long-form names). SELF magic is 4f153d1d; SDK stamp is
0x11000000.

PRX SHA256: `2e712a8b0e1ffc2b8c12af6e5b7a7a8fe63fe6e6a5c24d113655faebb79c92f4`
(31,331,783 bytes). Profile digest (unchanged retained93 overlay inherited
from l23): `53ed7c04591fc1e10a13209527c111abd75eb9f6a478bfb813d9c293fb6ded8f`.
Each pass recompiles 125 units and retains 249 of 374 selected units; 391
full link inputs. Counts come from the l24 twin build receipts.

## Import delta vs l23

None. The raw runtime strong-undefined census (211 strong, 1 weak
`kernel_mprotect`) is identical to l23's; the l24 build IMPORT-DELTA records
added=[] removed=[]; the final PRX carries the same 207 exact identities as
the l23 package baseline (verified row-by-row by check_import_identity.py,
require_getter on, and compared against
`2026-10-08-l23-package/pass-1/EXACT-IMPORT-IDENTITIES.json`).

## Changed facts in this edition (the whole source delta vs l23)

The l24 change is the typescan-r3 property-pointer/FField-class refusal
rework responding to the l23 console harvest (`[PropertyPointer] unreadable
field or unknown property FField class refused`). Exactly 4 files differ from
the l23 sources the BASE composition selected (12 tracked rows total; the
other 11 rows are byte-identical carried replacements):

- `typescan-r3/candidate/src/TypeChecker.cpp`
  (`d7b934a5...` -> `dcebc6e607a6effafff9bb5a75bd6395d49b86fe86dfcc1e9c3ea794947996cf`):
  scan-lifetime `NativePropertyGuard::RefusalLog`, `BeginScanCollection` /
  `EndScanCollection` around `find_all_object_types`, and post-scan
  `[TypeScan] refusals collected=N dropped=M` + per-record
  `reason/object/field/class` warning prints for harvest.
- `typescan-r3/include/NativePropertyPointerGuard.hpp`
  (`f64577fa...`): new refusal taxonomy enum (`SlotRead`, `ClassNull`,
  `ClassMisaligned`, `ClassIdentity`) with `reason_text`, `RefusalRecord`
  (bounded 50-entry `RefusalLog`).
- `typescan-r3/include/NativePropertyPointerGuard.inl`
  (`ede64b2c...`): scan-mode state; bootstrap-safe FField-class check
  (`property_readable`, `read_class_fname`, `fname_ends_in_property`)
  replacing the l23 identity gate; class checks routed through the verified
  FName pool without new interning.
- `typescan-r3/include/NativePropertyReadBackend.inl`
  (`96179b2d...`): `PropertyImageClassRange` now routes through the bridge's
  full spanpub policy (module-info when obtained, else PT_LOAD + this
  capture's VM records) instead of the module-info-only subset that made the
  l23 range check always fail with zero mappings.

`NativeTypeScanPolicy.hpp` and the profile/thread parts are unchanged from
l23 (placeholder CHANGES.json, files: []); profile digest stays
`53ed7c04...` (93 facts, Map KeyProp/ValueProp and Set ElementProp remain
conditional and absent from declarations; Array Inner required; no new
container offset shipped). 125 units recompiled (guard/header dependents),
249 retained.

Note: `typescan-r3/CHANGES.json` row hashes match the built candidates (the
build driver verified them), but its `scope` string still carries placeholder
wording ("changes not yet landed") — stale text only; the candidate hashes in
the same file are the recomputed, landed ones. No file under the runtime
directory was modified by this packaging task.

## Static gates (both passes, 20/20 each)

elf_magic_bare, fself_magic_wrapped (4f153d1d),
wrapped_embedded_elf_header_equal, fself_payload_digest_matches_bare,
wrapped_segments_equal, elf_module_type_flags (ET_SCE_RELEXEC 0xfe18),
elf64_x86_64_le, pt_sce_module_unique, sdk_stamp_0x11000000,
explicit_dt_init_dt_fini, loader_owns_no_arrays, init_fini_slots
(238/232 frozen + 2/2), init_fini_slots_present, .eh_frame/.eh_frame_hdr/
.gcc_except_table present, imports_equal_extended_contract (207),
linker_provided_dynamic_not_imported, imports_resolve_to_sdk_or_linker,
entry_alignment_evidence (force_align_arg_pointer bytes at module_start).
Full details in `pass-{1,2}/STATIC-CHECKS.json`; zero gaps in both
`pass-{1,2}/BUILD-RECEIPT.json` (MAC_PACKAGE_CHECKS_PASS). Checker unit
tests: 11/11 OK (4 import-identity tamper tests + 7 extended-sections).

This packet is local review material. No console action or upload occurred.
Native queue delivery, menu/spawn operation, runtime scan success and the
refusal-fix effect on the l23 harvest failure remain unproved; that is the
next explicitly authorized console phase's question. Original mod content is
unchanged. Official Epic source and its diagnostic excerpts are excluded;
local evidence paths/hashes remain in receipts.
