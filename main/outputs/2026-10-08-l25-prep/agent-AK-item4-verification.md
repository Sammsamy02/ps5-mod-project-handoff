# Agent AK — independent verification of agent-AD item-4 edits (compile + run harness)

Date: 2026-10-08, ~15:27-15:40 CDT. Target:
`outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp`
sha256 `4f4afddfd24e213faa2e80bc2ac3d41e23e634446d5f27f23c97ea5b7ac78b09`
(confirmed on disk at session start, before harness build, and again at report
time — the edited TU did NOT move during this verification). Tree treated as
read-only; all AK artifacts under `/tmp/l25-verify-item4/` (harness, runners,
logs); Z12/Z13/Z19/AD artifacts untouched.

## Verdict

ALL 9 assigned verification tasks PASS on the 4f4afddf revision. Two tree-level
observations (not failures of AD's edit) and one numeric note are in
"Discrepancies & findings".

- Run harness: 92/92 checks PASS (plain build) and 104/104 PASS (Prospero-diag
  build = the same 92 + 12 diag-tag checks).
- Compile matrix: 6/6 exit 0.
- CHANGES.json sweep: 58/58 pin checks match disk (54 row pins + 4 added-header
  hashes; AD's count of "56" is off by two — see finding 3).
- Item-6 suite re-run: PASS 2/2 (plain + asan-ubsan), exit 0.

## Extracted regions (verbatim, sed line-ranged from 4f4afddf; shas recorded)

| region | file lines | sha256 |
|---|---|---|
| anon-namespace helpers (incl. all AD additions: L25CanonicalUserMax, l25_pointer_word_plausible, l25_read_word, L25GMallocSnapshot/l25_snapshot_gmalloc, l25_data_heap_plausible, code 6 enum) | 423-562 | `23a6fe9933d22a8b207bb2618dc9cbe447a29a469e330d9c31caca4b879f0966` |
| AddUObjectDeleteListener (hardened body under test) | 579-711 | `446012775b3ee4766140acbc065e4588689c3ede5e42cd3b6250b12bd3462bee` |
| RemoveUObjectDeleteListener (unchanged, exercised for codes 1-5) | 713-760 | `e2cef4eaa5a309611005452aae2d51d12953d73b725787705268289557526ddc` |
| landed listener-base header extract (reused Z12 fixture) | — | `fba3dc8a47d0f72796a471760356f07effb59e2abd259a4b2f5fb26b504a84c7` |

Harness: `/tmp/l25-verify-item4/ak_item4_harness.cpp` (Z12 task2 stub pattern:
STR/Output/ensure_str/TArray/GUObjectArray/GMalloc stand-ins; TArray stub adds
an Append call counter for the no-call assertions). Two builds:
`ak_harness_plain` (host semantics; l25_diag no-ops exactly like the real TU
off Prospero) and `ak_harness_diag` (`-D__PROSPERO__` + extern-C
diagnostic_event recorder mirroring the TU's declaration at UObjectArray.cpp:16).
Full logs: `logs/plain-run.log`, `logs/diag-run.log`.

## 1. growth_required computation — PASS (8/8)

`growth_required = (pre_num >= pre_max) || pre_data == nullptr` observed via the
pre-Append log field (computed before the sane gate, so every boundary is
observable):

- Num(1) < Max(4), Data non-null → `growth_required=0` (A1).
- Num(2) == Max(2) > 0, Data non-null → `growth_required=1` — first disjunct
  alone (A2).
- Num == Max == 0 (Data null) → `growth_required=1` + fresh-allocation branch,
  no code 6, no RISK (A3).
- Data == null with Num(0) < Max(5) → `growth_required=1` — second disjunct
  alone (Num>=Max is false); the same header is then refused (3) by the
  unconditional sane gate, pre-gate observation preserved (A4).

## 2. Guarded reads — PASS (25/25)

- `l25_pointer_word_plausible` truth table (B1): aligned real heap address
  true; 0 false; 0x100000004 (misaligned) false; `1<<47` false;
  `(1<<47)-8` (last aligned user address) true; `(1<<47)+8` false; aligned
  `0xFFFFFFFFFFFFFFF0` false.
- `l25_read_word` (B2): byte-wise reconstruction of 0x1122334455667788 and a
  high-byte-only pattern verified on the little-endian host (target x86-64 LE
  composes identically).
- Staged snapshot `l25_snapshot_gmalloc()` (B3-B9): full chain reads all four
  stages correctly (variable_address == &GMalloc; variable_value == slot
  address; instance == *variable through the guarded reader; vptr == first word
  of instance). Every failing stage short-circuits with the downstream fields
  left 0 and NO dereference: null variable (B4); misaligned variable value
  0x100000004 (B5 — would fault if read); `1<<47` variable value, aligned so
  only the canonical bound rejects it (B6); slot containing null (B7);
  misaligned instance 0x100000004 (B8); beyond-canonical instance (B9).
- `l25_data_heap_plausible` (B10): real aligned buffer + live Num true; null
  Data false; misaligned false; `1<<47` false; wrapping 0xFFFF...FFF0 false;
  negative Num (wrap term) false.

## 3. Refusal code 6 ImplausibleDataForGrowth — PASS (21/21)

- FIRES only on live-buffer growth (Num==Max>0) with implausible Data:
  `Data = 1<<47` (C1) and kernel-half `0xFFFF800000000000` (C2) each emit
  `REFUSED (6)` with the message `refusing instead of calling Realloc`, leave
  Num untouched, and — asserted through the instrumented Append stub — do NOT
  call Append (counter unchanged), and emit no `header post:`/`GMalloc post:`
  lines.
- Does NOT fire when Data is plausible: Num==Max=2 real buffer proceeds to
  RISK + exactly one Append, Num 2→3 (C3).
- Does NOT fire on the fresh-alloc path Num==Max==0: fresh marker kept, Append
  called, first registration lands (C4) — AD's fresh-path interpretation is
  behaviorally as documented.
- Does NOT fire when growth not required (Num<Max) (C5).
- Ordering preserved: GMalloc-unbound refusal (5) wins over 6 when both
  conditions hold (C6); Append not called.
- Reachability analysis (verified, C7/C8): misaligned and wrapping Data are
  intercepted EARLIER by the unconditional sane gate as REFUSED (3) — the sane
  gate already requires non-null/aligned/no-wrap-with-Max and Max >= Num makes
  the num-wrap term redundant. Post-gate, null Data requires Num==Max==0,
  excluded from the live path. So code 6's live-path trigger reduces exactly to
  the canonical-bound violation (Data > (1<<47)-1), which is the new coverage
  AD added.
- Scenario note (harness-faithful, disclosed): on the host, user addresses are
  all < 2^47, so the end-to-end code-6 cases run with the stub Contains
  address-guarded (modeling a 57-bit-VA machine on which a Data at/above 2^47
  is mapped; `g_contains_guard`, default off = Z12's dereferencing Contains).
  See finding 2 for why this matters on the real target.

## 4. Refusal codes 1-5 unchanged — PASS (24/24)

Z12's original sanity battery re-exercised on the NEW region (fixtures reused:
listener header extract + stub set): sane fresh add with pre/post dumps and
path tags (D1); duplicate → REFUSED (4), no throw, Num unchanged (D2); the six
insanity cases → REFUSED (3) each: null Data with Num>0, Num>Max, Max>256,
misaligned, Data+Max*8 wrap, negative Num (D3); null GUObjectArray → REFUSED
(1) (D4); layout-missing throw contained → REFUSED (2) (D5); RISK at the
capacity bound + GMalloc-unbound growth → REFUSED (5) with header untouched
and Append not called (D6); Remove enter/pre/post/was_present markers (D7);
concurrent Adds serialized by the file-local mutex, both land (D8).

## 5. Log lines — PASS (14/14)

- Pre line renders ALL fields in order Data → Num → Max → growth_required →
  GMallocVar → GMalloc → GMallocInst → vptr, and each rendered VALUE was
  checked against ground truth: GMallocVar == &GMalloc, GMalloc == slot
  address, GMallocInst == instance address, vptr == first word of instance
  (E1).
- `GMalloc post:` renders the same four values re-read post-Append on the
  success path (E2).
- Post lines (`header post:` and `GMalloc post:`) are emitted on success only:
  absent on refusals 3, 4, 5, and 6 (E3; codes 1-2 return before the pre dump
  by construction).

## 6. Prospero diag tags — PASS (12/12, diag build)

`l25_dl_gmalloc_pre` == `l25_dl_gmalloc_post` == low-32(instance) on success,
pre emitted before post (F1-F3); `l25_dl_pre` carries pre_num (F4); unbound
GMalloc → both gmalloc tags carry 0 (F5); the code-6 path emits
`l25_dl_refused` = 6 and no post tags (F6-F7); `l25_dl_refused` values 1-5
re-confirmed per cause (F8).

## 7. Compile matrix — PASS 6/6 (logs under /tmp/l25-verify-item4/logs/)

| run | compiler | result | log |
|---|---|---|---|
| unreal-uobjectarray, edited TU (real tree source, stub recipe argv, source swapped off the Z13 shim as AD did) | host /usr/bin/clang++ | `-fsyntax-only` exit 0, 0 errors | `ak-host-syntax-unreal-uobjectarray.log` |
| same, full codegen | host | `-c` exit 0, 0 errors, object `ak-unreal-uobjectarray.o` | `ak-host-obj-unreal-uobjectarray.log` |
| unreal-unrealinitializer variant A (default) | host | `-fsyntax-only` exit 0 | `ak-host-syntax-unreal-initializer-variantA.log` |
| unreal-unrealinitializer variant B (`-DMODBRIDGE_L25_SKIP_DELETE_LISTENER`) | host | `-fsyntax-only` exit 0 | `ak-host-syntax-unreal-initializer-variantB.log` |
| unreal-uobjectarray, edited TU | real `prospero-clang++` (receipt argv + l25 -I inserts, no stubs) | exit 0, 0 errors, only the 2 known inherited Atomic.hpp -Wswitch warnings | `ak-prospero-syntax-unreal-uobjectarray.log` |
| unreal-unrealinitializer A / B | prospero | exit 0, 0 errors, known port-header warnings only (Atomic.hpp x2, UnrealType.hpp deprecated-enum x2 per variant) | `ak-prospero-syntax-unreal-initializer-variant{A,B}.log` |

The UnrealInitializer.cpp runs compiled the CURRENT file — see finding 1.

## 8. CHANGES.json sweep — PASS 58/58 pin checks

Against the CURRENT `typescan-r4/CHANGES.json` (sha `24224e740a88...`): 18 rows
x 3 role pins in `changes[]` = 54, plus 4 `added_header_hashes` = 58 total
checks, every pinned hash recomputed and matching disk; `files[]` byte-equal to
`changes[]`; no duplicated path+sha pairs; UObjectArray.cpp rows pin
`4f4afddf...` (disk match) in both roles carrying it. Sweep log:
`logs/changes-sweep.log`. See findings 1 and 3 for the version history.

## 9. Item-6 suite re-run (Claude pre-build gate) — PASS 2/2

`python3 .../2026-10-08-l25-runtime/tests/run_tests.py` → exit 0; plain PASS +
asan-ubsan PASS, all four sub-checks (a)-(d) green in both modes, "PASS l25
item-6 host tests: plain + asan-ubsan". Exactly the expected 2/2; log:
`logs/item6-suite.log`.

## Discrepancies & findings (none is a failure of AD's edit)

1. TREE MOVED MID-VERIFICATION (parallel landing, not AD): at AK session start
   (~15:27 CDT) `candidate/src/UnrealInitializer.cpp` hashed `efdef253...` —
   AD's pinned value, confirming AD truly did not touch it during the item-4
   window. At 15:30:36 CDT a parallel agent (CHANGES.json `review_fixes` row:
   agent-K-l25-review findings F1/F2/F3) edited it to
   `cac95e4822709b790cd96718f7d1a3f43d7346fb899e155d81c48a0e675d268d` and
   re-pinned CHANGES.json at 15:31:22 (now `24224e740a88...`, replacing AD-era
   `9ec2f0bd...` which AK had observed on disk at 15:29). The new pin matches
   the new file, so the CURRENT tree is self-consistent. AK's variant A/B
   compiles above cover the current cac95e48 file; AD's efdef253 variant proof
   stands from AD's own logs plus AK's 15:27 observation. UObjectArray.cpp
   (4f4afddf) and the region shas were stable throughout.
2. Code-6 screen sits AFTER `Contains` (observation for the coordinator): the
   landed order is sane-gate → Contains → GMalloc-bound (5) → heap-plausibility
   (6) → RISK → Append. `Contains` dereferences Data, so on the real 4-level
   target a genuinely non-canonical live Data (the only reachable code-6
   trigger, per section 3) would fault inside Contains BEFORE the code-6
   refusal runs. Claude's rule as written ("refuse cleanly instead of calling
   Realloc") is still honored — Append/Realloc is never reached — and the
   Contains-first order is O's pre-existing structure, not an AD change; but
   code 6 is not a pre-read guard. If a pre-read guard is wanted, the
   plausibility screen (or at least the canonical-bound term) would need to
   move above `Contains`.
3. Pin-count nit: AD's report says "all 56 hash pins across the 18 rows + 4
   added-header hashes"; recomputed reality is 54 row pins + 4 added = 58 (0
   duplicated pairs). All 58 match disk, so the claim's substance holds; only
   the count "56" is off by two.
4. Prospero fidelity caveat inherited and reproduced unchanged: the prospero
   syntax run still resolves `<Unreal/UObjectArray.hpp>` through the l22 vfs
   overlay to the old port-tree header (the E3 routing gap owned by the
   parallel agent — the same agent-K/initializer work now in flight); AD's
   change is body-level in the .cpp.

## Artifacts

- Harness + regions + binaries: `/tmp/l25-verify-item4/` (`ak_item4_harness.cpp`,
  `ak_region{1,2,3}*.inc`, `ak_harness_plain`, `ak_harness_diag`,
  `ak_compile_matrix.py`, `ak_changes_sweep.py`, `ak-unreal-uobjectarray.o`)
- Logs: `/tmp/l25-verify-item4/logs/` (plain/diag build+run, 6 compile-matrix
  logs + summary, changes sweep, item-6 suite)
- Z12 fixture reused read-only: `/tmp/l25-verify-listener/extract_listeners_hpp.txt`
