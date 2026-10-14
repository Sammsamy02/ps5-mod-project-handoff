# FOLD1-READY.md — walkguard ACTIVE fold pre-verify for the L11 compose

worker-fold1-preverify, 2026-10-05. Host object compiles only; no composition run, no link,
no package, no console.

## Verdict

**FOLD-1 READY** — with one comparator correction the L11 compose must adopt
(see "Correction" below).

Both folds of the staged guarded ASMHelper (`worker-launch10-walkguard-prep/staging/asmhelper-guard/ASMHelper.guarded.cpp`,
sha 694af0163e273846..., re-verified unchanged) recompiled clean (rc=0, empty stderr) under the
CURRENT header set, through the exact launch-10-lineage unit argv:

- argv: `retained-asmhelper-asmhelper` (148 tokens) from
  `worker-launch10-walkguard-prep/ue4ss-native-supplier-composition-6-l10wg/run-a/commands.json`
  — the same CLANG_FLAGS + includes the packaging used for asmhelper-asmhelper.
- SEC32-3 equivalence asserted, not assumed: drift vs the shared
  `coherent-full-1/asmhelper-asmhelper.command.json` is exactly tokens {142 source, 144 -o, 147 -MF}.
- compiler: prospero-clang++ wrapper -> clang 21.1.8, target `x86_64-sie-ps5` (same LLVM-21.1.8
  tree and LLVM_CONFIG env as the Oct 5 06:57 flip probe).
- only source/-o/-MF redirected into this worker's directory; throwaway objects, never linked.

## Comparators for the L11 structural gate (bridge UND resolution vs fold mode)

| comparator | fold-0 (define ABSENT, shipped) | fold-1 (one-define flip, basename preserved) |
|---|---|---|
| object sha256 | `81dd832bd59fab43a57c0feb0c9c15b756b2e62f74e667e320e666a2c66b4bb6` | `6604ce2d423c5d0dee4de6a089bd8141126e0fb02e7538ae08a4076d9b5af8d5` |
| syscall instructions | 0 | **exactly 1** (`movl $0x4a,%eax; syscall` = inline mprotect 74, import-free) |
| object size | 534,408 B | 534,472 B |
| undefined (UND) surface | 60 symbols | 60 symbols — **set-identical** to fold-0 |
| defined surface | 784 symbols | 784 symbols — set-identical to fold-0 |
| RC::ASM exports | 9 (5 public resolvers + `WalkGuard::Core::enter/exit` + `ScopedRxGuard` ctor/dtor) | 9, list-identical |

Bridge consequence: fold-1 adds no UND symbol and removes none — the L11 compose's bridge UND
resolution gate sees an unchanged import surface across fold modes. The only text delta is the
guard body (zero syscall text -> exactly one inline mprotect syscall).

## Correction (before the L11 compose declares receipts)

The object bytes embed the source **basename** (measured: `strings` shows
`ASMHelper.guarded.cpp` in every basename-preserving object; `ASMHelper.guarded.fold1flip.cpp`
in probe2.o). Therefore:

- The composition-admissible route — flip the one fold-default token
  (`#define RC_ASM_WALK_RX_GUARD 0` -> `1`) in the staged file, **keep basename
  `ASMHelper.guarded.cpp`**, argc=148, no new edition — produces **`6604ce2d...`**.
- `f5627427131f53ac...` (walkguard-prep CLAIM L10WGP-5 PRIMARY "declared object") is only
  reachable with the probe's renamed source `ASMHelper.guarded.fold1flip.cpp`, which violates
  basename preservation. Declaring it for the basename-preserving route will fail the compose's
  receipts assertion late.
- Route convergence (measured): under basename preservation the source-flip route and the
  `-DRC_ASM_WALK_RX_GUARD=1` argv route produce byte-identical objects (`6604ce2d...`); the argv
  route remains edition-refused (argc 149), so the flip route is the activation path.

All other L10WGP-5 measurements reproduced exactly (rc=0, syscall counts, export surfaces);
only the declared sha for the basename-preserving flip route is corrected.

## Header drift reconciliation (the reason for this pre-verify)

- `NativeProgramContext.hpp` layout freshen (+0x30 shift): mtime Oct 4 21:41:25 — BEFORE the
  launch-10 build and the Oct 5 06:57 probe. The shipped object was built WITH the freshen in
  the tree, and this TU does not include that header at all (dependency-closure set-compared).
- Capture-capacity header `live_module_facts.hpp` `67170b21fc3b0094...`: lives in the controller's
  `launch10-build/staging/`, is not on the asmhelper -I closure; the spike-tree copy visible to
  the -I set (`f114f876...`, Oct 4 13:24) is unchanged.
- Freshness scan: 1606 headers under the 38 unique -I dirs, 0 modified after the probe cutoff.
- Fold-0 byte reproduction (`81dd832b...`, cmp-identical to probe0.o AND to the walkguard run-a
  composed `asmhelper-asmhelper.o`) proves header-state equivalence for this TU independently.

## Activation mechanics (unchanged, for the record)

Activation itself remains verdict-gated (WST-5: admissible exactly on a W_WORKS /
W_FAILS_R_WORKS IGW D2 verdict, seq5 `rx_result.mprotect_rc == 0`) and controller-owned. Runtime
fail-closure (mprotect rc != 0 -> loud refusal, no read) remains the mis-fold backstop. This
worker performed no activation and ships no ACTIVE candidate.

## Receipts

- `receipts/fold1_preverify_compile.sh` — the replay script (asserts argv drift set)
- `receipts/fold0/`, `receipts/fold1/` — objects, .d, compile.stderr, nm-defined.txt, nm-undefined.txt
- `receipts/evidence/preverify.console.log`, `receipts/evidence/preverify-measured.json`
- `receipts/RECEIPTS.txt` — sha pins
