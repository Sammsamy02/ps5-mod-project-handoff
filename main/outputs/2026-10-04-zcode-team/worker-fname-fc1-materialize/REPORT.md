# REPORT — worker-fname-fc1-materialize (2026-10-04)

Task: materialize `calls_by_anchor` for the FName ctor call-site candidate vaddr
`0x23d590` (runtime `0x63d590`) by running the survey's resolver offline on the frozen
capture, then derive the FC-1 byte-shape checks (`FC1-CHECKS.md`).

## Outcome: TASK COMPLETE (offline path), native re-run BLOCKED and documented

### 1. Resolver run — what happened

- Found the prepared native emitter `fc1_calls_by_anchor` (worker-fname-fc1-prep,
  `rerun-calls-by-anchor/target/release/`), which calls the UNMODIFIED resolver crate
  (`ue4ss-native-tick-relocation-resolver-1/crate/src/fname_call.rs::resolve`) over an
  mmap'd read-only capture. Its own run-1 had already died mid-resolve with empty stdout.
- My re-run (nice'd, 300 s alarm) reproduced the wedge exactly: stderr stops at
  "calling fname_call::resolve" (`run1.stderr.txt`, 247 B; `calls_by_anchor.raw.json`, 0 B).
- Diagnosis: host memory crisis — swap 11.18/12.28 GB used, ~67 MB free pages. `ps`
  showed SIX sibling `fc1_calls_by_anchor` processes from 1:21PM–2:47PM all wedged
  state `UE` (~1 s CPU, RSS ~0.4–1.3 MB) inside resolve, SIGKILL-pending but
  unkillable (uninterruptible kernel path). I stopped only MY task (identified, mine);
  the other workers' processes were left to their owner sessions (their sessions were
  already issuing kill -9).
- Per task item 4, the blocker is documented and the deliverable was produced from the
  resolver contract + expected-bytes extraction (plus a faithful offline port, below).

### 2. calls_by_anchor — materialized via bounded offline port

`fc1_offline_scan.py` mirrors the resolver exactly (read-only, mmap, capstone 5.0.7,
`ulimit -t 240`): `resolve_with_ranges` + `call_after` (fname_call.rs) and
`scan_xrefs`/`targets_with_indirection` (xrefs.rs), same NAMES order, same caps
(strings<=64, xrefs<=16384, calls<=4096), same 64-byte branch-free window, same
`E8 rel32` in-exec-range validation, same five-anchor intersection uniqueness rule.

Result (`calls_by_anchor.materialized.json`, capture sha256
`3b6cfad4...3622fc` — matches worker-expected-bytes):

- address `0x23d590` (== frozen survey run-release-1), runtime `0x63d590`.
- Engine: 0x2f07077, 0x3a17b91; Renderer: 0x174d6af, 0x174d6d8, 0x2f85635, 0x3a17bbc;
  AnimGraphRuntime: 0x3a17be7; Landscape: 0x3a17c50; RenderCore: 0x3a17ca6 (9 sites).
- Revalidation: all call bytes `E8`, all rel32 == candidate — TRUE.
- Each anchor: exactly 1 string candidate, exactly 1 distinct call target; the five
  anchor sites cluster at 0x3a17b7e..0x3a17ca6 (one registration function).
- Windows: every site starts at `48 8d 35 <disp32>` hitting the anchor UTF-16LE string
  content vaddr; all show `lea rdi,[rbp-disp]` (`48 8d 7d xx`) + `mov edx,1`
  (`ba 01 00 00 00`) before the branch-free `E8` — the full ABI contract observed.
- Candidate bytes at 0x23d590 (64 B) match worker-expected-bytes byte-for-byte
  (first 32: `55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74 33 0f b7 38 48 89 c6 31 c9 66 85 ff 74 2a 66 90`).

Fidelity caveat (honest labeling): this is a faithful PORT of the resolver contract,
not a native run. It is corroborated three ways: identical candidate address vs the
frozen native survey run, byte-identical expected bytes from an independent worker, and
internally consistent window/xref evidence. The native `calls_by_anchor` field remains
unverified natively while the binary wedges.

### 3. FC1-CHECKS.md (deliverable)

Defines, with exact hex, pass criteria, and refutation conditions:
- FC1-A: 32-byte prologue equality at runtime 0x63d590 (+ optional 64-byte strict).
- FC1-B: per-call-site window shape (LEA RSI rip-rel to anchor string; single
  `lea rdi,[rbp|rsp+disp]`; `mov edx,1`; no branch/clobber; `E8 rel32 -> 0x63d590`),
  with three exact reference windows dumped.
- FC1-C: uniqueness guard, fail-closed semantics.
- FC1-D: gated UTF-16LE anchor-string checks beyond the exec span (refutes the uniform
  +0x400000 data bias, not the code candidate).
- FC1-E: explicit refutation catalogue (incl. overload mismatch if `mov edx,1` absent).

Compatible with worker-anchor-validation's frozen facts (uniform load bias 0x400000,
exec span [0x400000,0x787c000), probe style: bytes-first, gated derefs, per-check
records). No live/engine execution was performed; these are the acceptance checks for
the probe/composed runtime, not evidence of runtime success.

### Files (this worker's scope only)

- `CLAIM.json` (first), `REPORT.md` (this, last)
- `FC1-CHECKS.md` — the FC-1 acceptance contract
- `calls_by_anchor.materialized.json` — full materialization (diagnostics, per-site windows)
- `fc1_offline_scan.py` — the bounded read-only port (provenance headers inside)
- `run1.stderr.txt` + `calls_by_anchor.raw.json` (0 B) — wedge evidence of the native binary
