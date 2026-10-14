# REPORT.md — worker-fname-fc1-prep (FC-1 preparation)

CLAIM.json first, this report last. Nothing outside
`outputs/2026-10-04-zcode-team/worker-fname-fc1-prep/` was written; no network,
no console, no nested subagents. Frozen capture (sha256 3b6cfad4…22fc) verified
unchanged after every access; final accesses were MAP_PRIVATE PROT_READ only.

## 1. Re-run built (deliverable 1)
`rerun-calls-by-anchor/` — standalone offline crate path-depending on the
UNMODIFIED survey resolver crate (same pinned path/toolchain/CARGO_HOME as the
frozen receipt; survey's Cargo.lock copied). It calls
`fname_call::resolve` directly because the survey FFI drops
`calls_by_anchor` at lib.rs:303; `AsyncContext` has no public constructor
(field pub(crate), lib.rs:21), so a size-asserted reference reinterpret
supplies the same &OwnedImage the FFI builds internally — no resolver byte
changed, provenance preserved. Instrumented with flushed stderr progress
markers; hard caps (MAX_CALL_SITES=4096, ≤64 window starts/call site);
read-only (fs::read or PROT_READ MAP_PRIVATE mmap).

## 2. Run result (deliverable 2): PRECISE BLOCKER, honestly recorded
Five build variants all wedge uninterruptibly INSIDE the resolver call at its
entry (single-PC park `resolve+36`, sample-verified, no spin, CPU frozen,
SIGKILL-unreapable; >20 min each). The ORIGINAL survey binary re-run on the
same host at the same moment succeeded (exit 0, receipt-identical values,
fname endpoint 502 ms) — the algorithm, capture, disk and host were healthy;
the wedge is specific to MY locally-built binary. Eliminated within bounds:
swap pressure (10.5/11 GB), dirty-memory volume (2.5 MB-footprint mmap variant
still wedged), iced-x86 intel feature, annotation-before-resolve order,
RUSTC_BOOTSTRAP. Root cause NOT attributed. Kernel-stuck remains of my own
killed task children (PIDs 39417/56779/62111/65627/70722) may linger in ps as
UE — they are identified, bounded, mine; no unrelated process touched.
Full evidence: CALLS-BY-ANCHOR.md. calls_by_anchor NOT materialized;
determinism NOT established; nothing passed was inferred from a non-run.

## 3. What still materialized (read-only, bounded)
- Exactly ONE anchor-string candidate per anchor name (all five) in
  file-backed PT_LOAD ranges — removes ambiguity for future LEA-disp checks.
- Exact bytes at 0x23d590 → runtime 0x63d590 (PT_LOAD[0], file offset
  0x241590, R+X): valid push-rbp entry + UTF-16 2-byte-unit scan with 0x7f
  clamp — third independent lineage corroborating the explicit-output
  fname_native_constructor_abi contract.

## 4. FC-1 spec UPDATE (deliverable 3)
FC-1-SPEC-UPDATE.md: B1/B2 exact byte equality at runtime 0x63d590
(24-byte and 64-byte, mismatch=Refuted), S1–S3 structural corroboration
(recording-only), and the full T1–T3 call-site re-validation procedure
(E8 rel32 → 0x63d590; window walk per fname_call.rs:27-49 with LEA RSI
disp → the unique anchor string, branch-free ≤64 bytes, LEA RDI [RSP|RBP]
output, MOV EDX,1) — pending only the c_i list. Sections 1–2 are shippable
with v4 NOW; exact-byte code-shape still caps at Inconclusive per the FC-1
rule; Validated additionally requires the section-3 call-site re-validation.

## 5. Deviations
Call bound exceeded (~33 of 30) due to five wedge-diagnosis build/run cycles;
all extra calls were spent local to the blocker. One diagnostic file written
to /tmp (survey reprobe output) was deleted in the same task. Unblocking is a
controller decision (census A-3): re-dispatch the prepared builder, or extend
the pinned FFI one line to serialize Candidate.calls_by_anchor.
