# REPORT.md — worker-fold1-preverify (honest limits included)

worker-fold1-preverify, 2026-10-05. Workspace-local pre-verify for the launch-11 full-stack
variant's walkguard ACTIVE fold. Local only; no composition runs.

## What was done

1. **Include-set reconstruction (from launch10 candidate receipts, not memory).** The exact
   148-token `retained-asmhelper-asmhelper` argv (CLANG_FLAGS + 51 `-I` occurrences / 38 unique
   dirs) was taken from
   `worker-launch10-walkguard-prep/ue4ss-native-supplier-composition-6-l10wg/run-a/commands.json`
   (sha 805ee837781ef5cc...) — the packaging's real asmhelper-asmhelper compile. An in-script
   assertion proved it SEC32-3-equivalent to the shared
   `coherent-full-1/asmhelper-asmhelper.command.json` (sha 8d6f5d98f450f13f...): token delta
   exactly {142 source, 144 -o, 147 -MF}, argc 148 == 148.

2. **Both folds recompiled against the CURRENT header set.** prospero-clang++ (clang 21.1.8,
   target x86_64-sie-ps5; same LLVM-21.1.8 tree + LLVM_CONFIG env as the original probe). Only
   source/-o/-MF redirected into this worker's dir. Both rc=0, empty stderr:
   - fold-0 (staged guarded source, define ABSENT): `81dd832bd59fab43...66b4bb6`,
     **0** syscall instructions. cmp-identical to WGP probe0.o AND to the walkguard run-a
     composed `asmhelper-asmhelper.o`. The shipped object identity holds — no header drift
     reaches this TU.
   - fold-1 (ONE-token in-file flip `#define RC_ASM_WALK_RX_GUARD 0 -> 1`, basename
     `ASMHelper.guarded.cpp` preserved, no argv define, argc=148): `6604ce2d423c5d0d...b5af8d5`,
     **exactly 1** syscall instruction (`movl $0x4a,%eax; syscall` = inline mprotect 74,
     import-free). cmp-identical to WGP probe1.o and to WST's fold-1 measurement.

3. **Exactly-one-syscall + symbol-surface checks (llvm-nm / llvm-objdump).** fold-1 has exactly
   one `syscall`; fold-0 has zero. Defined surfaces 784 == 784 (set-equal); undefined surfaces
   60 == 60 (set-equal); RC::ASM exports 9 == 9 (5 public resolvers + 4 WalkGuard internals).
   Bridge UND resolution is unchanged between fold modes.

4. **Include-closure + freshness proof.** New .d sets: fold-0 == probe0.d (805 deps,
   set-equal); fold-1 == probe2.d except the TU source path token itself. 1606 headers under
   the 38 unique -I dirs: 0 modified after the probe cutoff (2026-10-05 06:57:52). The two
   coordinator-named changes are both OUTSIDE this TU's closure:
   - `NativeProgramContext.hpp` (+0x30 freshen): mtime Oct 4 21:41:25 — before the launch-10
     build and the probe; not included by this TU.
   - `live_module_facts.hpp` `67170b21...` (capture capacity): controller
     `launch10-build/staging/` copy; not on the asmhelper -I closure (the spike-tree copy
     `f114f876...`, Oct 4 13:24, unchanged, is what the -I set would resolve — and the TU does
     not include it).

## MATERIAL FINDING — comparator correction for the L11 compose

Object bytes embed the source **basename** (measured via `strings` + sizes + cmp):
- basename-preserving builds embed `ASMHelper.guarded.cpp`;
- probe2.o embeds `ASMHelper.guarded.fold1flip.cpp` (its renamed probe source).

Consequence: worker-launch10-walkguard-prep CLAIM **L10WGP-5's "exact_flip PRIMARY" declared
object `f5627427131f53ac...` is internally inconsistent** with its own "keep the basename
`ASMHelper.guarded.cpp`" instruction. The basename-preserving flip — the composition-admissible
route (argc=148, no new edition) — measures **`6604ce2d423c5d0d...`**, byte-identical to the
`-D` argv route at the same basename (the argv route stays edition-refused at argc=149).
`f5627427...` is only reachable by renaming the staged source, which violates the discipline.
The L11 compose must declare `6604ce2d...` as the fold-1 comparator or its receipts assertion
will discover this late. WGP's other L10WGP-5 measurements (rc=0, syscall counts, export
surfaces) all reproduced exactly here; only that one declared sha is corrected.

## Deliverables

- `CLAIM.json` (F1PV-1..F1PV-5 + verdict/comparators)
- `FOLD1-READY.md` (ready-state verdict + L11 comparator table)
- `receipts/fold1_preverify_compile.sh` (replayable; fails loudly on argv drift)
- `receipts/fold0/`, `receipts/fold1/` (objects, .d, compile.stderr, nm-defined.txt,
  nm-undefined.txt; fold1/ also holds the flipped source `ASMHelper.guarded.cpp`)
- `receipts/evidence/preverify.console.log`, `receipts/evidence/preverify-measured.json`
- `receipts/RECEIPTS.txt` (sha pins)

## Honest limits

- Host-level object compiles ONLY. No link, no package, no composed runtime, no PRX, no
  console contact, no runtime/kernel/verdict/gameplay/quality claim of any kind. The L11
  compose owns its own receipts and gate.
- The comparators are valid for the CURRENT header state of this tree. Any later change inside
  the TU's 805-dependency closure invalidates them; re-run
  `receipts/fold1_preverify_compile.sh` (it re-asserts the argv drift set and re-measures).
- The basename finding is a correction to a DECLARED sha in another worker's receipt; per
  studio discipline this worker does not edit worker-launch10-walkguard-prep's files — owner
  reconciliation is recommended before the L11 compose.
- The compile wrapper's `--version` only executes with the LLVM env set; toolchain identity is
  pinned by the LLVM-21.1.8-macOS-ARM64 tree path + byte-identical reproduction, not merely a
  version string.
- Shared-tree reads only: nothing outside `worker-fold1-preverify/` was written. The staged
  walkguard files, spike trees, run receipts and other workers' outputs are untouched.
