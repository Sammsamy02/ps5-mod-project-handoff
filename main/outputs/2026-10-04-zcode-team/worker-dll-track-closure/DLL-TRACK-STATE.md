# DLL-TRACK-STATE — Windows-services DLL track, closure state 2026-10-04

Compiled by `worker-dll-track-closure` (read-only synthesis; no console
contact, no dispatch). Every hash below was re-computed on disk this session
and matches the owning receipt. Statuses are quoted verbatim from the owning
workers' receipts/reports; nothing here upgrades or re-rates evidence.

One sentence of state: **the track is host-complete across 8 families; 6 are
console-proven (four individually + all six again inside the combined
ALLFAM_DLL_TEST run); Static TLS and SEH64 are host-validated and await their
first console proof inside the single combined 8-family candidate (`c3aba79f`),
which is already wired into the batch tool's session order as `dll8`.**

---

## 1. Per-family provenance table

Substrate (pre-family, shared by every candidate build, console-proven
6d295f56 and earlier): entry/clock + owned worker threads + synchronization
(SRW/CV/address-wait slice). These are not counted among the 8 families.

| # | Family | Owning worker(s) | Host status | Console status | Console build / receipt |
|---|---|---|---|---|---|
| 1 | InitOnce (`InitOnceBeginInitialize`/`Complete`) | worker-4-win-services | VERIFIED (host, ASan/UBSan/TSan, red-stub control, native PS5 owner objects; win64cc i32 bridge) | PASSED individual + inside 6/7/8-family builds | `1a6b37b7` INITONCE_DLL_TEST.elf, 46/10 imports, all expected values matched (`worker-dll-console-stage-prep` cites `C/console/1a6b37b7-console-receipt.json`; superseded first attempt c49108fe logged to /tmp — controller error, not evidence) |
| 2 | Mutex (+ CreateMutexA adapter) | worker-mutex lineage (sdk-target/sdk-retry/fix-2/review) | VERIFIED | PASSED individual + inside 6/7/8-family builds | `6f5bf85a` MUTEX_DLL_TEST.elf, 51/10 imports (`C/console/mutex-console-receipt.json`; 43-symbol ELF import list preserved in `worker-mutex-sdk-retry/elf-undefined.stdout`) |
| 3 | File I/O | worker-fileio* lineage (apply = vendor base of every later tree) | VERIFIED | PASSED individual + inside 6/7/8-family builds | `664f83a5` FILEIO_DLL_TEST.elf, 61/10 imports, after the /data/modbridge namespace-root fix (×8 JSONL records incl. the fixed re-run) |
| 4 | CRT startup (`_initterm`/`_initterm_e`/atexit/onexit, `_seh_filter_dll` stub) | worker-crt-startup-implement / worker-crt-integrate | VERIFIED | PASSED individual + inside 6/7/8-family builds | `538085976d31464b9e2ed61cd84f3150`, 68/10 imports; console-lineage ELF sha256 `a6208aea…60379f` (re-hashed this session) |
| 5 | Module enumeration (14 census names, 5 loader-unique exercised) | worker-module-enum | VERIFIED (bind-order rule: binders after loader bridge) | Console-proven INSIDE the 6-family combined run only | rides `133c5e9e` ALLFAM_DLL_TEST (ledger match) |
| 6 | Virtual memory (VirtualAlloc/Free/Protect/Query/QueryEx; +5-thunk ABI glue authored in allfam) | worker-virtual-memory (+ worker-combined-dll-allfam glue) | VERIFIED (protection changes are documented bookkeeping-only emulation) | Console-proven INSIDE the 6-family combined run only | rides `133c5e9e` ALLFAM_DLL_TEST (ledger match) |
| 7 | Static TLS (dir-9 admission ticket, post-link TLS-directory patch, 4 PE imports, callbacks) | worker-static-tls → worker-static-tls-7fam | COMPLETE (host + TSan + native/bridge receipts; 7-family host validation `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`) | **NOT YET DISPATCHED** — first proof rides dll8; frozen fallback = 7-family candidate | none (candidate compiled only) |
| 8 | SEH64 (dir-3 EXCEPTION admission under `MB_PE_SEH64`, dynamic unwind registry, raise→dispatch→`__except`/`__finally`, unhandled-filter state) | worker-seh64-design → worker-seh64-family | COMPLETE (host validation incl. full −130..−137 exercise; 7 win64cc ABI definitions IR-verified) | **NOT YET DISPATCHED** — first proof rides dll8 | none (candidates compiled only) |

Family-level console evidence policy (worker-acceptance-evidence §0.2 applies
here too): a family is "console-proven" only by its target JSONL records
matching the frozen expected ledger; host receipts never substitute.

## 2. Candidate ELFs (all re-hashed this session, EXACT vs receipts)

| Candidate | build_id | sha256 | bytes | Status |
|---|---|---|---|---|
| 6-family combined (InitOnce+Mutex+FileIO+CRT+ModuleEnum+VM) | `133c5e9e59f24c2fb57e7e3422c665a1` | `96296c1c92850623781c0ecf8126f2307c21a1380cc95bb99d277a8d7a07e81c` | 300,456 | **Console PASSED** as ALLFAM_DLL_TEST — 4 records, ledger match (CONSOLE-LOG item 13; ledger `allfam_dll_console_run`) |
| 7-family (= 6 + Static TLS) | `e72f5318233845369a68556cd61e59ca` | `83ee2ae95268a9b7f7bc4d931a29b77d5c3c2b15afdbb4e4288ac5490a847cae` | 334,656 | Compiled, never dispatched. **Frozen fallback attribution candidate** (alias ALLFAM7_DLL_TEST.elf, plan+ledger frozen) |
| **8-family (= 7 + SEH64) — THE dll8 candidate** | `c3aba79f968a492b849e5ece2f009e8e` | `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb` | 336,552 | Compiled, never executed anywhere (`OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; console_contact/target_execution/prx_created false). ELF build-id 0224ee94363709e5114bb133231aa4a7 |
| Standalone SEH64 (component-only) | `5f7e67b36ce04bb1b0fffc0a20266240` | `9d74b27bd2882b30391c87d4d2c0dc26f990d99a2625717fcb27bc1ee6fbbcc3` | 222,408 | Compiled candidate; superseded for console purposes by the 8-family merge; kept as provenance |

Import-surface invariant (verified twice, by worker-8fam-console-prep and
worker-dll-7fam-stage-prep): the 55-symbol `nm -u` undefined-import sets of
the 6-, 7-, and 8-family ELFs are pairwise **byte-identical** — SEH64 and
Static TLS add ZERO SDK imports; their new names (4 TLS + 8 SEH64) live only
in the consumer PE fixture surface bound through def libs. Every SDK-side
provider was already target-proven by the console passes.

## 3. Plans and ledgers (all frozen; none edited after freeze)

| Plan | Where | Covers | State |
|---|---|---|---|
| CONSOLE-STAGE-PLAN.md + EXPECTED-RESULTS-LEDGER.md | worker-dll-console-stage-prep | 6-family ALLFAM_DLL_TEST (+ distills the 4 family-payload precedents: upload once / check / FTP readback sha law / single dispatch / append-only JSONL / game-closed) | EXECUTED — PASSED |
| CONSOLE-STAGE-PLAN-7FAM.md + EXPECTED-LEDGER-7FAM.md | worker-dll-7fam-stage-prep | 7-family fallback (static_tls key, 82 imports, memory.last 147456, −120..−127 triage) | FROZEN — fallback attribution path only |
| **CONSOLE-STAGE-PLAN-8FAM.md** + VERIFICATION.md | worker-8fam-console-prep | 8-family preferred run (alias **ALLFAM8_DLL_TEST.elf**, readback law = `79281191…`/336552 B EXACTLY, single dispatch, JSONL positions 37–40, pass-oracle vs BOTH priors, −130..−137 triage, stale-shape discriminators) | FROZEN — PREFERRED, awaiting user-authorized console window |
| EXPECTED-DELTA-8FAM.md | worker-dll-8fam-combined | The 8fam expected ledger (see §4) | FROZEN oracle (memory.last omission corrected in the plan, not in the delta doc — provenance preserved) |
| Batch tool | worker-console-batch-runner/console_batch.py, patched in place by worker-batch-runner-8fam-patch (diffs + pre-patch snapshots retained) | `DEFAULT_DLL="dll8"`; session order **dll8 → e2e → stage-qdrv1 → game-evidence**; `--dll 7` swaps the fallback in; exactly one DLL stage per session; same gate chain for both | TESTED (162 selftest + 31 fake-console checks green); **dll8 IS in the session order**; nothing dispatched |

## 4. The 8-family expected ledger (already prepped — summary and pointers)

Oracle sources: `worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md` (host-based
delta) + `worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md` (pass-oracle
table incl. the memory.last correction) + the batch tool's `verdict_8fam_records`
implementation. Headline expectations for the 4 appended records
(seq 0..3, phases [1,2,3,5], positions 37–40; 41–44 only after a dll7
fallback run; hard invariant: appended after ALL existing records, none modified):

- `seh64: {enabled:1}` from seq1 onward — 9 counted family keys (sync,
  init_once, mutex, fileio, crt_startup, module_enum, vm, static_tls, seh64)
  plus entry/clock/worker enablement fields = 12 enabled keys; seq0 census-zero.
- `counts.consumer_imports` **90** (6fam 78 → 7fam 82 → 8fam 90: +4 TLS
  symbols, +8 SEH64 names — 7×KERNEL32 + 1×VCRUNTIME140 `__C_specific_handler`);
  `helper_imports` stays 10.
- `results` [86,93]; worker 4/4, begin [5,1,258,259], result quadruple
  0x81234567 / cookie / 0xe17395ab / 0; entry attach [7,7] / detach [0,0];
  `contract_errors` 0; cleanup `{pending:0, retired:1}` (retire order: SEH64
  first, then static TLS, then 6..1).
- `memory.last.length` **114688** (seq2) → **163840** (seq3) — host-measured;
  the 6-family run proved this field matches host on target. (This corrects
  EXPECTED-DELTA-8FAM.md's silence on the field; the plan carries the values.)
- win64cc IR: union dict incl. `mb_win_static_tls_abi: 1` and
  `mb_win_seh64_abi: 7`.
- Console-only delivery delta to NOT misread: `stdout_error 32`,
  `stdout_records 1` (target has no stdout; delivery-channel status, not a
  failure).
- Failure triage: −130..−137 (SEH64 fixture assertions, source-verified) on
  top of inherited ranges −120..−127 (static TLS) and −1..−117; discriminators
  for partial SEH64 binds (imports 88/89) and stale payloads (82-shape = 7fam
  ELF under the new alias; 78 = ALLFAM).

## 5. What the dll8 run adds over the 6-family PASS

The 6-family PASS (ALLFAM_DLL_TEST) proved: the substrate + six families, in
one combined build, on target, with the 55-symbol SDK import surface and the
append-only record shape. The dll8 run adds exactly three things:

1. **First console proof of the 7th family (Static TLS).** The target
   loader's first consumption of a post-link-patched data directory 9
   (`.rdataT` struct, `.tls` region, `.rdataC` callbacks), the dir-9
   admission ticket in pe_module, the `@tls_import_thunk` bridge, and the
   −120..−127 fixture assertions — until now host-only evidence.
2. **First console proof of the 8th family (SEH64).** First target execution
   of the dir-3/EXCEPTION admission amendment, the dynamic function-table
   registry, the raise→capture→two-pass dispatch→`__C_specific_handler`→
   `__finally`→continue-trampoline path, and the −130..−137 fixture surface.
   The never-dispatched surface is narrow and named, so attribution stays
   clean against the 6-family PASSED baseline (and the frozen 7-family
   candidate exists purely to separate "SEH64 graft broke it" from a
   shared-surface regression if needed).
3. **The combined admission itself.** The 8-family build is a 3-way merge
   (git merge-file, base=allfam, ours=static-tls-7fam, theirs=seh64-family;
   raw merge provenance retained in worker-dll-8fam-combined/merge-base/)
   with composition facts no individual validation proves: bind order static
   TLS 7th then SEH64 8th (newest binds last), retire newest-first, the
   stage-55/56 window now owned by TWO owners, the union import census (90),
   the union win64cc IR dict (1+7), and the scaled memory.last values. A dll8
   PASS proves that merged composition executes on the target route — the
   superset in one upload/dispatch/readback budget, strictly more than the
   7-family run would prove.

Economy note: because the SDK import set is identical to the console-passed
ELF, dll8 carries no import-resolution risk; its only new risk surface is the
family surface named above.

## 6. Successor queue after dll8 (honest)

**Nothing in the queue is console-blocking.** A dll8 PASS closes console
evidence for all 8 families. What legitimately remains, none of it gating any
other track's milestone:

- **SEH64 PS5-trampoline pre-execution review (the one named open item).**
  Worker-seh64-family (REPORT + SEH64-IMPLEMENTATION §4.2): the naked
  trampolines take the freebsd9/PS5 backend's default entry with in-body
  register shuffles (only 7 win64cc C-level defines survive on the ELF
  route); those register contracts are **host-verified only** and the owning
  worker recorded that they "would need target-route review before any
  execution." A dll8 PASS exercises the bounded fixture census on target and
  does NOT discharge this review: before any *reliance* on SEH64 semantics
  beyond the exercised fixtures (real DLL mods raising exceptions), a
  dedicated review of the trampoline route must cover the documented
  divergences — H2 guard-frame RSP parity, signal→exception delivery,
  resume-execution, vectored handlers, C++ EH, integer-state-only restore.
- **Host-only caveats carried verbatim by families** (documented in-code and
  in the owning reports; they bound what a PASS means):
  - Static TLS: no TEB/`__tls_array` emulation (thunk + `__tls_index` are the
    adapter surface); lazy blocks for never-notified threads (the internal
    thread's NULL block is the honest observation — a NON-NULL block is
    fabrication-suspicious, not a pass); no loader lock.
  - Virtual memory: protection changes are bookkeeping-only (no host-enforced
    page protection).
  - CRT startup: `_seh_filter_dll` bound but not exercised; onexit exercised
    in its documented unbacked-table state.
  - Module enum: five loader-unique census names exercised; the shared
    loader names stay bound to the loader bridge.
- **Conditional 7-family fallback run** — only if dll8 trips a SEH64-specific
  surface and window budget remains (attribution aid, not a closure
  requirement).
- **Post-PASS bookkeeping** (controller): console receipt
  `allfam8-console-receipt.json` + result copy naming per the batch guide,
  CURRENT-BUILD notes, JSONL left append-only-intact.
- **Adjacent, not part of the 8:** MSVCP140 remains ASSESSED-only (bounded
  narrow-only owner recommended) — a possible future 9th family, explicitly
  not required for this track's completion.

## 7. UE4SS acceptance independence (documented, in both directions)

The UE4SS acceptance ladder is defined in
`worker-acceptance-evidence/EVIDENCE-SPEC.md`: rungs from genuine game-thread
callback through visible spawn through real input, proven only through the
composed-runtime PRX lineage (composition-5 r11 staged; composition-6 next)
in real Townfall launches via channels C1–C4. Independence facts:

1. **No rung requires any DLL-family result.** EVIDENCE-SPEC's own global
   rule — host fixtures and compile probes "never advance a rung" — applies
   symmetrically: DLL console passes (payload ELFs) are not rung evidence,
   and no rung's PASS/FAIL oracle references
   `/data/modbridge-owned-dll-candidate.jsonl` or any family key.
2. **Different mechanism, different artifacts, different console state.**
   DLL stages: pldmgr `/loadpayload`, `game_launched: false`, game CLOSED,
   report file `/data/modbridge-owned-dll-candidate.jsonl`, payload catalog
   under `/data/pldmgr/payloads/`. UE4SS acceptance: ploader arm + PRX slot
   (`/data/plugins/modbridge_supplier_composition.prx`) + running game,
   evidence in UE4SS.log / runtime-status.txt / facts JSONL / capture. The
   8fam plan and the 7fam plan both mandate: payload-only stages in one
   game-closed window, the game-launch session LAST, never interleaved.
3. **Different build lineages.** The 8-family vendor tree descends from
   worker-combined-dll-allfam/vendor (worker-fileio-apply base); the
   composition lineage is the bridge r2→r12 + entry + comp6 scaffold with
   its own gates. No staged artifact of either track is an input to the
   other; dll8 staging touches neither ploader.ini nor the PRX slot.
4. **Scheduling is order, not dependency.** The batch session order
   (dll8 → e2e → stage-qdrv1 → game-evidence) puts the DLL stage first
   because it is smallest and needs the game closed — a sequencing choice
   recorded in the plan, not a gate: the game-evidence stage can proceed
   after a dll8 FAILURE exactly as after a PASS (with the failure recorded
   and reconciled host-side).
5. **Motivation link, not acceptance link.** SEH64's design census reads
   UE4SS.dll/dwmapi.dll imports — that is forward-looking substrate for
   real DLL mods inside the runtime; the current acceptance path (Lua mods
   through composition) does not load owned PE DLLs, so the families are not
   on the UE4SS critical path in either direction.

## 8. Provenance discipline

All status text above is quoted or tightly paraphrased from the owning
workers' REPORTs/receipts; raw reports remain unchanged. Re-hashes this
session are the only newly generated evidence. Nothing in this file is
console evidence; the console checkpoint remains the console checkpoint's.
