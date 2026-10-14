# REPORT.md — worker-dll-8fam-combined

Worker: worker-dll-8fam-combined (ZCode team, 2026-10-04). Budget ~55 tool
calls of 100 used. No console contact, no target/PS5 execution, no nested
subagents, no network; writes only under `outputs/2026-10-04-zcode-team/
worker-dll-8fam-combined/`. Neither source worker tree
(worker-static-tls-7fam, worker-seh64-family) was written or modified; both
were read-only inputs, plus worker-combined-dll-allfam/vendor used ONLY as
the 3-way merge base.

Status: **EIGHT_FAMILY_COMBINED_HOST_AND_SDK_CANDIDATE_PASSED** —
`OWNED_CANDIDATE_HOST_CONTROLS_PASSED` twice (deterministic, full 8-family
`--host-only`) and `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED` (11.8 s,
600 s guard respected); the ELF was never executed anywhere.

## Deliverables (in write order)

1. `CLAIM.json` — bounds and claims, written first.
2. `vendor/` — fresh 8-family tree: worker-static-tls-7fam's vendor (7
   families; `windows-services/build` scratch excluded — absent from the
   seh64 tree which built fine without it, and nothing references it) plus
   byte-identical SEH64 grafts from worker-seh64-family:
   `windows-services/mb_win_seh64.{c,h}`, `mb_win_seh64_abi.{c,h}`,
   `windows-candidate/tests/worker-seh64.h`,
   `tests/worker-seh64-kernel.def`, `tests/worker-seh64-vcruntime.def`.
3. `merge-base/` — raw 3-way merge provenance: `.base` (allfam),
   `.ours` (static-tls-7fam), `.theirs` (seh64-family), `.merged`
   (resolved) for each double-edited file.
4. `out-host-validation/256e9b3a…/` and `out-host-validation/1fb208c5…/` —
   two complete, deterministic host-validation runs; logs
   `out-host-validation-run{1,2}.log`.
5. `out-sdk-composition/c3aba79f…/` — compile-only SDK ELF candidate; log
   `out-sdk-composition.log` (records the 600 s guard outcome).
6. `EXPECTED-DELTA-8FAM.md` — console-results expectations vs the 7-family
   plan.

## Merge method and cited conflict resolutions

Both worker trees descend from worker-combined-dll-allfam/vendor (verified
by diff), so every double-edited file was merged 3-way with
`git merge-file` (base=allfam, ours=static-tls-7fam, theirs=seh64-family).
`tests/worker-fixture.c` changed only on the 7fam side — no merge needed.
`pe_module.h` auto-merged clean (0 hunks). Nine files had true conflicts:

- `windows-execution/pe_module.c` (1 hunk): `admission()` — composed the
  TLS amendment (data directory 9 admitted only under the runtime
  `m->tls_admission` ticket, `MB_PE_REQUIRES_TLS` accepted only then) with
  the SEH64 amendment (directory 3 + `MB_PE_REQUIRES_EXCEPTION` only under
  `#ifdef MB_PE_SEH64`, plus the existing in-image range check). Kept both
  comment blocks and added a merge note; every other directory and the
  default refusal path stay byte-identical to the base.
- `windows-candidate/owned_runtime.h` (1): report struct keeps BOTH
  `static_tls_enabled` and `seh64_enabled`.
- `windows-candidate/result_report.c` (1): keep both key emitters;
  `,static_tls:{enabled:1}` then `,seh64:{enabled:1}`.
- `windows-candidate/fixtures/entry_consumer.c` (2): both sides shared one
  trailing `#endif`; resolution gives each family its own `#if/#endif`
  include block and exercise call, in family order (static TLS 7th, SEH64
  8th). Preprocessor balance re-verified (12/12).
- `windows-candidate/owned_runtime.c` (9): includes, session members and
  owner globals (both sides, own `#endif` each); bind order — static TLS
  block first, SEH64 block after it (newest binds last; one comment line
  rewritten to say so, claim text unchanged); retire order — SEH64 first
  ("newest family retires first", their comment kept verbatim), then
  static TLS; `service_begin` — 7fam's `if(i==0)` static-TLS session +
  admission-ticket block kept, binder gate takes seh64's extension
  `#if MB_OWNED_CLOCK || MB_OWNED_WORKERS || MB_OWNED_SEH64`; report flags
  (both); stage-55 owner opens (both, each own `#endif`); run_body import
  census — union term `+(MB_OWNED_SEH64?8u:0u)` appended after static
  TLS's ENTRY-gated +4. Note: stages 55/56 are now the open/close window
  of TWO owners (static TLS and SEH64), each with its own status enum as
  the detail code — both sites were already multi-call-site in the 7fam
  tree (open + directory parse share stage 55), so the contract "unique
  stage numbers per family" becomes "unique per family window"; recorded
  as a composition fact, not a code change.
- `windows-candidate/test_entry_candidate.c` (1) and
  `test_worker_candidate_failures.c` (1): import formulas union the +8
  SEH64 term with each side's authored gating preserved verbatim (static
  TLS keeps its `MB_OWNED_ENTRY&&` conjunct; SEH64 keeps the unconditioned
  form from its own validated tree). Both formulas were then proven
  empirically by the host runs (90 imports, normal and failure modes).
- `windows-candidate/build.py` (12): argparse (both flags), constraints
  (both), profile defines (both, STATIC_TLS then SEH64), native/abi source
  lists (both), sources manifest (both), receipt profiles (both), def-lib
  runs and consumer extra-libs — the seh64 tree shipped the
  `seh64-kernel-lib`/`seh64-vcruntime-lib` pair and the consumer
  `extra +=` line duplicated verbatim twice (verified in its source);
  resolution keeps ONE of each (identical commands, pure dedupe);
  fixture-compile line — 7fam's define list + `-DMB_OWNED_SEH64` + seh64's
  `*ehflags` form (unwind tables on when `--seh64`); unsupported-directory
  guard — seh64's list switch (`[7,8,…] if a.seh64 else [3,…]`, dropping
  admitted directory 3) with 7fam's directory-9 consumer allowance kept in
  the body; result-key assertions (all four lines); win64cc IR expected
  dict — union (`mb_win_static_tls_abi: 1`, `mb_win_seh64_abi: 7`).

## Evidence (all receipts in this worker dir)

Host validation, all eight family flags + `--entry --clock --workers --sync
--host-only`, both runs:

- Run 1 `out-host-validation/256e9b3a339a41719c34ab67ecdb6b05/`:
  `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`; 30 commands, all exit 0, zero
  hung-driver markers. `host-normal.jsonl` 4 records, `host-release.jsonl`
  5 records; results **[86,93]** in both; all family keys including
  `static_tls` and `seh64` present as `{"enabled":1}` from sequence 1
  onward. Prior evidence unshifted: worker `calls_started/returned` **4/4**
  (begin `[…,258,259]`, result `0x81234567`), entry attach **[7,7]** /
  detach **[0,0]**, `contract_errors` **0**, cleanup
  `{pending:0, retired:1}`; worker-cleanup-failure mode keeps worker 2/2
  and the first-failure contract. Consumer imports measured **exactly 90**
  from the PE metadata (KERNEL32 73, VCRUNTIME140 1, mbstatictls.dll 4,
  heap 2, synch 3, crt-runtime 7) = the 7-family 82 + 8 SEH64 names;
  helper 10. seh64_profile: `win64cc_entries: 7`, `exercised_imports: 8`;
  the full -130..-137 exercise (probe/lookup/virtual-unwind, same-frame
  raise, guarded restore, cross-frame `__finally`, dynamic
  register/remove over a bridge address, unhandled-filter round trip)
  completed inside the passing run.
- Run 2 `out-host-validation/1fb208c51d58440e9c13b3845a8ceb93/`: same
  status. Determinism: all three JSONL evidence streams byte-identical
  after removing only `build_id` (which must differ by design);
  `owned_consumer.dll`, `owned_helper.dll`, `owned_fixtures.h` SHA-256
  identical; the 30-command argv/exit sequence identical; the only receipt
  differences reduce to the uuid build id, its run paths, and hashes of
  id-bearing artifacts; `owned_profile.h` differs by exactly the
  `MB_OWNED_BUILD_ID` line. Driver binaries embed that id, hence their
  hashes differ — source-level evidence identical.

SDK composition (same family flags, no `--host-only`, `--report-path
/data/modbridge-owned-dll-candidate.jsonl`, `--fileio-root /data/modbridge`):

- `out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/`:
  `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`; elapsed **11.8 s**,
  inside the enforced 600 s wall guard. Artifact `owned-dll-candidate.elf`
  336,552 bytes, sha256 `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc
  985cf25e6d804fb`. The report path is compiled into the profile only
  (`target_path_opened_by_build: false`). ELF undefined imports: **55 —
  the identical symbol set and provider mapping as the 7-family ELF**
  (e72f531 receipt `import_providers` sets are equal; the standalone
  seh64 ELF's 36-symbol set is a subset): SEH64 adds ZERO SDK imports.
  win64cc IR expected dict passed with the union
  (`mb_win_static_tls_abi: 1`, `mb_win_seh64_abi: 7`). `structure_layout`
  byte-identical to the 7-family composition; `dynamic_layout` validated
  (21 entries, terminal DT_NULL). The composition run's own host controls
  also passed (4 normal + 5 release records).

## Honest limits

- Nothing console-proven; no PRX; the SDK ELF is a compiled candidate only
  (`target_executed=false`, `console_contact=false`, `prx_created=false`
  in every receipt). No Windows-loader parity claim; SEH64 semantic
  conformance covers the exercised MSVC-emitted fixtures only; the PS5
  trampoline route remains compile-verified only.
- Static-TLS divergences (no TEB emulation, `__tls_array` unresolved, lazy
  blocks) and SEH64 scope cuts (H2 RSP parity, signal delivery,
  resume-execution, vectored handlers, C++ EH) are inherited verbatim from
  the two source workers' reports.
- The stage 55/56 shared-window note above is a documentation-level
  composition change; no authored constant was altered.
- `merge-base/` retains the raw `.base/.ours/.theirs/.merged` copies as
  merge provenance; `scratch/`-style experiments were not needed — both
  host runs passed on the first attempt each.
