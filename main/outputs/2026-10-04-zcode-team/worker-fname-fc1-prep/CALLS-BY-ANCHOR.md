# CALLS-BY-ANCHOR MATERIALIZATION — RUN RECORD + PRECISE BLOCKER (worker-fname-fc1-prep, 2026-10-04)

## Deliverable attempted
Materialize `fname_call::Candidate.calls_by_anchor` for the FName ctor call
candidate `0x23d590` by a bounded offline host re-run of the survey resolver
(UNMODIFIED crate
`.superpowers/native-dependency-spike/ue4ss-native-tick-relocation-resolver-1/crate`,
fname_call.rs sha256 `f481e6ddf86515dfc739ad0610c1b2518035df0340756086666a9b0b79e429a8`,
pinned by run-release-1/receipt.json). The survey FFI
`ps_fname_ctor_call_file_image` (lib.rs:299-304) writes only `Candidate.address`;
`calls_by_anchor` is dropped at the FFI boundary — hence a dedicated binary
calling `fname_call::resolve` directly.

## Result: BLOCKED — resolver call wedges in-kernel in MY binary only
Five build variants of the prepared binary
(`rerun-calls-by-anchor/`, all offline, pinned toolchain 1.90.0-aarch64,
pinned CARGO_HOME `ue4ss-native-mapped-candidates-1/tools/cargo`):
1. fs::read + pre-resolve annotation + iced-x86 `intel` feature
2. fs::read + pre-resolve annotation, resolver-exact iced features (no intel)
3. read-only mmap capture + pre-resolve annotation
4. read-only mmap + ALL annotation deferred to AFTER `resolve` (survey-identical call order)
5. rebuild without `RUSTC_BOOTSTRAP=1` (cargo fingerprint-identical to 4)

ALL variants: process enters `fname_call::resolve` and blocks uninterruptibly
(ps state U/UE) parked at a single PC (`resolve+36`, i.e. the
`executable_ranges` call at resolve entry — `sample` reports 100% of samples
on that one frame, no spin, CPU time frozen). Wall time >20 min each; SIGKILL
does not reap (kernel-held). PIDs 39417, 56779, 62111, 65627, 70722 were my
own task's children, individually killed at their bound; any still showing
UE are kernel-stuck remains of MY identified tasks only — no unrelated
process was touched.

## Proof the blocker is my binary, not host load or the resolver
Diagnostic re-run of the ORIGINAL frozen survey binary
(`ue4ss-native-file-core-survey-1/target/release/native_file_core_survey`)
on the SAME host at the SAME time as variant-1's wedge:
exit 0, all values byte-identical to the frozen receipt
(fname_ctor_call_candidate status=0 linked=0x23d590 elapsed_ms=502;
engine 5.6; SCO 0x490aa0; Tick 0x2fac8c0; GUA 0x93ba110...). So the resolver
algorithm, capture, disk, and machine were healthy mid-incident; the wedge is
specific to builds of my binary. Deltas examined and eliminated:
host swap pressure (10.5/11 GB used, but survey binary fine concurrently),
dirty-memory volume (mmap variant: 2.5 MB footprint, still wedged),
iced-x86 `intel` feature, pre-resolve annotation order, RUSTC_BOOTSTRAP.
Root cause NOT attributed — honestly unresolved; suspects remaining are
codegen/link-environment interactions of the locally-built binary
(e.g. dyld/rustc-version-specific layout) that no further in-task
experiment could separate within bounds.

## What DID materialize (bounded, read-only, capture verified unchanged)
From `run-1/stderr.txt` progress markers (before the resolver call in
variants 1–3) and a direct PT_LOAD walk:
- Anchor string candidates in file-backed PT_LOAD ranges: exactly ONE per
  anchor name — Engine, Renderer, AnimGraphRuntime, Landscape, RenderCore
  (whole-194MB UTF-16LE search, `\0NAME\0` form, +2 content-start semantics
  identical to fname_call.rs:61). No ambiguity for any of the five names.
- Exact bytes at 0x23d590 (PT_LOAD[0] vaddr 0x0 offset 0x4000 filesz
  0x7479b1c flags R+X; file offset 0x241590): 64-byte hex and 24-byte prefix
  recorded in FC-1-SPEC-UPDATE.md sections 1–2; decodes as a valid
  push-rbp-entry UTF-16 scan with a 0x7f clamp (S1–S3 there).
- Runtime address confirmed: 0x400000+0x23d590 = 0x63d590.
- Capture sha256 3b6cfad4…22fc verified identical before/after every access;
  MAP_PRIVATE PROT_READ mapping can never write back.

## NOT established
- calls_by_anchor table itself (blocked). Determinism double-run (blocked).
- No verdict numbers from the resolver beyond the frozen survey's address
  result, which was NOT re-derived by my binary (it never returned).

## Unblocking (controller decision, census A-3 note)
(a) re-dispatch `rerun-calls-by-anchor/` as prepared (build is complete and
instrumented; ≤90 s wall bound recommended via external guard), or
(b) owner-approved one-line extension of the pinned resolver FFI to
serialize `Candidate.calls_by_anchor` (lib.rs:303) + one survey re-run.
FC-1 sections 1–2 (exact target byte checks) are unblocked and shippable now.
