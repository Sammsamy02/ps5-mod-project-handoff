# REPORT.md — worker-diagnostic-sink (r-s1, 2026-10-04/05)

LOCAL ONLY delivery: no web, no console, no nested subagents. Nothing outside
`outputs/2026-10-04-zcode-team/worker-diagnostic-sink/` was modified.

## Delivered

1. **Staged source** `staging/diagnostic.c` (sha256
   `f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818`): the
   comp6 packaging unit `diagnostic.c` + ONE additive sink. Every
   `diagnostic_event(tag, value)` additionally appends
   `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,"tag":"…","value":D,"value_hex":"0x…","t_ms":MS}`
   to **`/data/modbridge-supplier-diag.jsonl`**. xwp tolerant-IO pattern:
   lazy `sceKernelOpen(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600)`, bounded
   partial-write retry (64), every failure tolerated, never fatal,
   open-failures retried (cap 64) with errno self-reported on the first
   recovered line. `t_ms` = CLOCK_MONOTONIC ms (same domain as
   runtime-status.txt `t=`). Signature and call sites unchanged; entry
   channel byte-unchanged.
2. **DIFF.md** — annotated unified diff + the exact unit
   (packaging bootstrap unit `diagnostic.c` of the pinned
   `worker-prx-link-gaps/package_prx.py`) + exact driver wiring (bounded
   `build_runtime_composition6.py` revision, `--diagnostic-source`
   substitution in the same in-memory `BOOTSTRAP_UNITS` injection class as
   the existing entry swap; the pinned fixed script and pinned package_prx.py
   stay byte-identical). Raw diff also at
   `staging/diagnostic.supplier-sink.diff`.
3. **ANALYSIS.md** — definition/linkage map (single definition: lineage
   `diagnostic.c` via the overlay symlink; 34 bridge r13 call sites; entry
   r4 call sites) and the four launch-9 loss mechanisms (pinned-marker
   collision, dead-channel silent drop, pre-open event drop, non-parseable
   mixed 2.34 MB append-only log with no supplier jsonl).
4. **Fixtures + PASS receipt** — `fixtures/run_host_tests.sh` →
   `receipts/host-test-pass.txt`: **23/23 C checks + strict python
   validation, PASS**. Scenarios: A forced startup open-failure (no file,
   bounded attempts, recovery, errno self-report, no repeats), B order +
   content + additive entry channel (205/205 legacy lines intact, same
   order), C 200-event burst (207 lines total, ordered), D write failure +
   partial-short-write bounded retry (line lands complete), E 200-char tag
   (bounded ≤96, never invalid JSON), s2b REAL chmod-0555 directory (80
   events, exit 0, no file, attempts capped at 64). Every produced line
   strict-parsed (json.loads): seq ascending with exactly one by-design gap
   (dropped write consumes its seq), t_ms nondecreasing, tags in call order.
   Four earlier failed attempts kept as raw provenance under
   `fixtures/receipts/host-test-fail-*.txt` (harness path bug; scenario-order
   bug; a REAL design flaw the fixtures caught — the original 8-attempt open
   cap permanently locked the sink after a transient failure, fixed to 64
   with rationale in the source; seq-gap expectation fix).
5. **Compile-gate rehearsal** — the EXACT packaging compile argv (SDK
   `ps5-payload-sdk@v0.43` prospero-clang wrapper, frozen CLANG_FLAGS,
   lineage include set, `-target x86_64-sie-ps5`) compiles the staged file
   `-Werror` clean: `fixtures/receipts/diagnostic-console-gate.o` (sha256
   `d14d0ac2…`). Global-symbol delta vs the shipped `diagnostic.o`:
   exactly `{__error, clock_gettime, getpid}` — all three in the frozen
   191-import contract AND already undefined in the shipped candidate's
   `combined-supplier.o`, so the §3.3 unresolved-set pre-check expectation is
   unchanged. Zero new imports, zero contract edits.

## COMPILE GATE THE CONTROLLER MUST RUN (launch-10 rebuild)

- The rehearsal proves the TU compiles under the exact flags/includes TODAY;
  the real pass must re-prove it inside the composition (this is the gate the
  RETURN-SESSION brief conditioned inclusion on: "compile-gated → include;
  else proceed").
- Exact argv pattern (identical class to the one in
  `worker-launch10-gmalloc-integration` receipts and package_prx.py
  `link_pipeline`): `prospero-clang -Wall -Wextra -Werror -O1 -fPIC
  -fno-stack-protector -fvisibility=hidden -ffreestanding -fno-builtin
  -I<overlay>/generated -I<overlay>/probe -I<overlay>/lifecycle -I<overlay>
  -I<lua_include> -c diagnostic.c -o build/diagnostic.o`.
- **Known environment trap (pre-existing, NOT introduced by this file):** the
  SDK's `bin/clang` is a broken COPY of the LLVM_BINDIR dispatcher; a wrapper
  run fails with `/llvm-clang: No such file or directory` unless
  `LLVM_BINDIR` points at a dir exposing `llvm-clang`/`llvm-config` names.
  My rehearsal shimmed a TEMP bin dir (created, used, deleted — the shared
  SDK was not touched). If the real packaging pass hits this, reconcile the
  environment exactly as the prior successful packaging run did — never by
  editing the shared SDK. The recovery argv uses
  `-target x86_64-sie-ps5 -isysroot <sdk> -isystem <sdk>/target/include`.

## Honest limits / flags

- **No console claims.** Host logic + compile rehearsal only. The xwp
  precedent proves the same open/append/O_NOFOLLOW pattern works in
  `/data` on console, but MY file has not executed there.
- **Total-loss surface remains:** if all 64 open attempts fail on console,
  the values are lost silently (tolerated by design; the task forbade a
  mirror into the shared channels). A UE4SS.log mirror variant can be added
  later if launch-10 shows the file absent.
- `diagnostic_missing()` (missing-import events) does NOT go to the jsonl —
  task scoped the sink to `diagnostic_event`; extending it is a one-call
  change in the same pattern if wanted.
- Baseline `diagnostic_event(NULL, …)` would crash in the legacy line builder
  exactly as before (unchanged, out of scope; my added path is NULL-safe).
- The entry channel's own quirks (negative-fd caching, no retry) are
  deliberately NOT "fixed" — the shared channel must stay byte-compatible
  with the frozen entry behavior; my sink is the additive replacement path
  for durability.
- The staged sha (f5e96c41…) is pinned in DIFF.md and CLAIM.json; the driver
  revision re-checks it at run time so a drifted copy cannot ship silently.

## Receipts summary

| receipt | result |
|---|---|
| `fixtures/receipts/host-test-pass.txt` | attempt 5: PASS (23 C checks + strict JSON validation) |
| `fixtures/receipts/host-test-fail-harness-path.txt` | attempt 1: FAIL (harness path bug — raw provenance) |
| `fixtures/receipts/host-test-fail-scenario-order.txt` | attempt 2: FAIL (scenario-order bug — raw provenance) |
| `fixtures/receipts/host-test-fail-cap-lockout.txt` | attempt 3: FAIL (caught real cap-lockout design flaw — raw provenance) |
| `fixtures/receipts/host-test-fail-seq-gap-expectation.txt` | attempt 4: FAIL (validator expectation — raw provenance) |
| `fixtures/receipts/diagnostic-console-gate.o` | compile-gate rehearsal object (PASS, sha pinned in DIFF.md) |
