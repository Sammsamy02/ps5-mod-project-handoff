# Agent Z12 — independent listener-instrumentation + ABI-fix verification (real compile + run)

Date: 2026-10-08. Subject: agent-O's L25 items 2/3/4 edits in
`outputs/2026-10-08-l25-runtime/typescan-r4/`. Harness: `/tmp/l25-verify-listener/`
(read-only against every source tree; all stubs/extracts live under /tmp).
Report-only mandate honored: the tree was NOT modified by Z12.

**HEADLINE: all five tasks PASS against the live tree. One real compile bug was
found by this harness (independently, on two compilers) and was fixed in-flight
by agent Z19 while verification ran; every result below reflects the final
post-fix state unless explicitly marked pre-fix.**

## 0. Concurrent-edit disclosure (read first)

At verification start (~14:50) the landed `candidate/src/UObjectArray.cpp` failed
real compilation (see BUG §6.1). At 14:57 — mid-verification — agent Z19 landed a
one-line fix (`const_cast`→`static_cast` at candidate line 610), refreshed the
CHANGES.json hash (668b2682… → 8f45e6ce…) and added a 4th `review_fixes` entry
citing Z13's E1. Z12 detected the change through its own hash pinning (pristine
extraction sha 9c8891ca… no longer matched), diffed it (exactly that one line),
and re-ran every affected step against the post-fix bytes. Z12 found the same
bug with BOTH host Apple clang and the real prospero-clang++ before reading
Z13/Z19's reports — fully independent cross-validation.

## 1. Task 1 — vtable layout proof (VERDICT: PASS, two independent tracks)

No agent-Z8 layout proof exists in the prep dir (checked; Z1–Z5, Z7, Z13
present, no Z8), so this is the only layout proof besides Z13's smoke matrix.

**Track A — real PS5 target ABI (prospero-clang++, compile-time, strongest).**
`task1_target.cpp` + the REAL `candidate/src/ClassListener.cpp` compiled with
the l24 COMPOSITION-RECEIPT argv for units `unreal-uobjectarray` /
`unreal-flat-classlistener`, with `typescan-r4/candidate/include` prepended so
`<Unreal/UObjectArray.hpp>` resolves to the LANDED edited header (selection
proven by the captured `-H` trace: `.. typescan-r4/candidate/include/Unreal/UObjectArray.hpp`),
l22 vfs overlay dropped (its 4 roots don't touch our files). Compiler-emitted
vtables parsed from the .o (llvm symbols + ELF relocation offsets, custom
parser `elf_vtable_check.py`):

| class | vtable size | slots | slot contents (reloc order) |
|---|---|---|---|
| FClassCreateListener | 0x30 | **4** | D2, D0, NotifyUObjectCreated, OnUObjectArrayShutdown — NO slot 4 |
| FClassDeleteListener | 0x38 | **5** | D2, D0, NotifyUObjectDeleted, OnUObjectArrayShutdown, **slot4 = `__ZNK2RC6Unreal22FUObjectDeleteListener16GetAllocatedSizeEv`** |
| FShutdownDeleteListener | 0x38 | **5** | same shape, slot4 = same base GetAllocatedSize |
| FLuaObjectDeleteListener (verbatim 11-line replica of LuaUObject.hpp:31-41, header sha 89cb7390…) | 0x38 | **5** | same shape, slot4 = same base GetAllocatedSize |

Slot 4 in all three delete-listener deriveds resolves to the BASE
`FUObjectDeleteListener::GetAllocatedSize` (deriveds add no virtuals, nothing
shifted; sizeof all listeners == 8, single vptr, data-less — static_asserted on
the target compiler). Engine order cross-checked against the vendored official
5.6.1 source (UObjectArray.h:929-954: dtor/Notify/Shutdown/GetAllocatedSize;
FUObjectCreateListener has NO GetAllocatedSize) — exact match. Note: the real
`LuaUObject.hpp` needs the LuaMadeSimple/lua stack absent from the port root, so
the Lua listener is proven via a byte-verbatim replica (limitation disclosed;
the other two deriveds use their REAL header, which itself includes the REAL
edited UObjectArray.hpp).

**Track B — host runtime (Apple clang 17, arm64, Itanium ABI, ASan+UBSan).**
`task1_host.cpp` compiles verbatim sed-extracted regions of the landed header
(lines 23-63, extract sha fba3dc8a…) and port ClassListener.hpp (lines 20-44,
sha 529f85d7…). 18/18 checks PASS in both asan and plain modes:
member-pointer slot encodings (Notify=2, Shutdown=3, GetAllocatedSize=4 on base
and on all three deriveds; identical pmf words base↔derived including slot 4),
`!HasGetAllocatedSize<FUObjectCreateListener>` static_assert, and raw
`vptr[4]` calls (the engine's exact dispatch shape at UObjectArray.h:1471)
returning 0 without crash on all three delete-listener instances. Encoding
decoder handles both Itanium pmf variants (x86-64 offset+1; AAPCS64/Apple
adj-flag), validated against a toy 5-slot class.

Bonus target compiles of the real landed TUs with candidate/include first:
`ClassListener.cpp` exit 0 (0 errors), `UObjectArray.cpp` exit 0 post-fix
(pre-fix: 1 error, §6.1).

## 2. Task 2 — registration harness (VERDICT: PASS 34/34, asan+ubsan and plain)

Extraction: candidate/src/UObjectArray.cpp lines 378-625 (sed, post-fix sha
d0f5bece…; pre-fix pristine 9c8891ca… preserved), compiled byte-verbatim inside
stub deps (recording Output, engine-shaped TArray with Data/Num/Max + hook for
simulated concurrent writers, layout-throw shim, GMalloc stand-in). Checks:

- sane header: accepts, Add-enter + `header pre: Data=` + `header post:` +
  `path=fresh-alloc`/`path=no-realloc` recorded, listener actually registered,
  no anomaly (7 checks)
- insanity cases ALL refuse cleanly with REFUSED (3), no exception escapes
  (init continues): null Data with Num>0, Num>Max, Max>256, misaligned Data,
  Data+Max*8 address wrap, negative Num (6)
- null GUObjectArray → REFUSED (1), no throw
- layout-missing throw from GetUObjectDeleteListeners() → caught, logged as
  REFUSED (2), does not escape
- duplicate registration → REFUSED (4), **no longer throws**, Num unchanged
- post-write ANOMALY fires when Num advances by 3 instead of 1
  (simulated concurrent engine writer)
- RISK `capacity full (Num==Max=2)` fires exactly at the bound, registration
  still completes, `path=realloc` recorded; GMalloc-unbound-for-growth variant
  refuses with REFUSED (5) and leaves the header untouched
- Remove path: `Remove enter` / `Remove header pre:` / `Remove header post:`
  + `was_present=1`, absent removal `was_present=0`, no throw
- file-local mutex: two concurrent Adds NEVER overlap inside Append
  (overlap detector inside the critical region, 5 ms hold), both land (Num==2)

## 3. Task 3 — variant-B switch (VERDICT: PASS 9/9 clean + 6/6 variant-B)

Extraction: candidate/src/UnrealInitializer.cpp lines 544-579 (sha
5c0aaa37…, file sha efdef253… — matches agent-O and CHANGES.json), compiled
twice from the same bytes (region's scope-closing brace wrapped):

- clean build: both AddUObjectDeleteListener calls live (count==2),
  create-listener unaffected (count==1), all four markers present
  (`class-delete-listener registration begin/returned in`,
  `shutdown-listener registration begin/returned in`,
  historical `Adding shutdown listener` directly before the call), all four
  `l25_dl_class_begin/done`, `l25_dl_shutdown_begin/done` diag tags emitted,
  no SKIPPED line.
- `-DMODBRIDGE_L25_SKIP_DELETE_LISTENER`: mandated line
  `delete-listener registration SKIPPED (variant B; unsafe long-term: stale
  object pointers not cleared)` prints, **zero** AddUObjectDeleteListener calls
  remain, historical `Adding shutdown listener` line kept, no pre-markers,
  create listener still registered. Both binaries exit 0 under asan+ubsan and
  plain.

## 4. Task 4 — marker audit (VERDICT: PASS, 43/43 checks)

Every grep string from agent-O's section 5 exists at the claimed file:line
(one calibration note: the `engine called FShutdownDeleteListener::
OnUObjectArrayShutdown` send-line is 243, inside the report-claimed :239-247
block — on-line). All 16 `l25_dl_*` diag tags present. The Prospero-guarded
`extern "C" void diagnostic_event(const char* tag, unsigned int value);`
declarations are byte-identical to the tree's established idiom (candidate
LuaMod.cpp:96, candidate UE4SSProgram.cpp:13, coexist PS5PadSource.cpp:9,
Handler.cpp:15; defined in-link by lineage diagnostic.c:145). Structural
claims verified: mutex at :425, bound 256 at :433, sanity gate at :445,
Add lock at :487, Remove lock at :583, duplicate Contains :518, GMalloc check
:530; AddUObjectCreateListener/RemoveUObjectCreateListener region
(candidate :466-479) is BYTE-IDENTICAL to the port original (port :364-377).

## 5. Task 5 — CHANGES sweep (VERDICT: PASS, 129 checks, 0 failures, 2 notes)

- All 18 entries × 3 hashes recomputed for BOTH the `changes` and `files`
  arrays (108 hash verifications) — every original/candidate/copied_original
  path exists and matches its recorded sha256. Arrays are content-identical.
- The 3 new overlay entries' `original=` paths point at the pinned port-root
  files (UObjectArray.hpp b01bf4ef…, UObjectArray.cpp 171560a9…, ClassListener.cpp
  a579b272…), read-only verified; each `copied_original` is a byte-copy of the
  port file; port-root files themselves untouched.
- All 4 added_header_hashes verified.
- NOTE 1: agent-O's REPORTED hash for candidate/src/UObjectArray.cpp (668b2682…)
  is stale vs the live file (8f45e6ce…) — the concurrent Z19 fix; CHANGES.json
  itself matches the live file (self-consistent).
- NOTE 2: `review_fixes` now has 4 entries (agent-O described 3) — the 4th is
  Z19's documented E1 fix; all dated 2026-10-08.

## 6. Discrepancies found (report-only; nothing fixed by Z12)

1. **BUG (found by this harness, pre-fix, confirmed on two compilers; FIXED
   in-tree by Z19 at 14:57, re-verified clean by Z12):**
   `candidate/src/UObjectArray.cpp:610`
   `const_cast<void*>(DeleteListeners.GetData())` where GetData() returns
   `FUObjectDeleteListener**` (non-const) — ill-formed (const_cast cannot
   convert T**→void*). Both `prospero-clang++` (real target compile,
   `error: const_cast from 'ElementType *' (aka 'RC::Unreal::FUObjectDeleteListener **') to 'void *' is not allowed`)
   and host Apple clang rejected it; the landed file as agent-O left it did NOT
   compile. Sibling lines were legal (they const_cast a `const void*` local).
   Z19's `static_cast<void*>` fix is value-preserving and correct; post-fix
   target compile is 0-error and the Remove-post dump behaves identically in
   the harness.
2. **Agent-O report staleness (not a tree error):** UObjectArray.cpp hash and
   the "3 review_fixes" description predate the Z19 fix (see §5 notes).
3. **Corroborated routing hazard (outside Z12's five tasks but load-bearing for
   the ABI fix; Z13's E3, independently consistent with Z12's method):** Z12's
   proof deliberately prepends `candidate/include` (per task instructions). If
   the parent's rebuild routes like `target_compile.py` (which does NOT add
   that include dir) without applying the header entry as a file-copy/vfs
   mapping at the original port path, the slot-4 repair silently drops and
   listener vtables revert to 4 slots. Agent-O's section-0 ODR requirement
   (tree-wide application) must be honored by the rebuild.
4. Minor: FLuaObjectDeleteListener proof uses a byte-verbatim replica (real
   header requires the Lua stack not in the port root) — disclosed limitation,
   mitigated by the exact 11-line shape + header sha pin.
5. Minor: no agent-Z8 layout proof exists in the prep dir despite the task
   brief suggesting it might; Z12's §1 is the independent proof.

## 7. Compile + run counts (final state)

- prospero-clang++ (real PS5 toolchain) compiles: 6 invocations total → final:
  3/3 clean, 0 errors (layout TU; real ClassListener.cpp; real
  UObjectArray.cpp post-fix). Pre-fix UObjectArray.cpp failure is §6.1.
- Host clang++ compiles: 9 (task1 asan+plain, task2 asan+plain, task3
  clean/variantB asan + clean/variantB plain, pmf-encoding calibration) —
  all clean; plus 1 negative-control mini repro (intentional fail→diagnosis).
- Host runs: 9 binaries — task1 (2 modes, 18/18), task2 (2 modes, 34/34),
  task3 (2 variants × 2 modes, 9/9 + 6/6), pmf calibration — all exit 0.
- Audit scripts: task4 43/43 PASS; task5 129 PASS / 0 FAIL / 2 NOTE.

## 8. Harness inventory (/tmp/l25-verify-listener/)

`task1_layout.cpp→task1_target.cpp` (target), `task1_host.cpp`, `build_task1_target.py`,
`build_task1_real_units.py`, `elf_vtable_check.py`, `task2_registration.cpp`,
`extract_region_378_625.inc` (+ `.pristine.inc` pre-fix), `extract_listeners_hpp.txt`,
`extract_classlistener_structs.txt`, `task3_callsite.cpp`, `extract_region_544_579.inc`,
`task4_marker_audit.py`, `task5_hash_sweep.py`, `pmf_debug.cpp`, binaries + traces
(`*.trace.txt`, `task5_out.txt`), and objects `task1_target.o`,
`classlistener_landed.o`, `uobjectarray_landed.o`.
