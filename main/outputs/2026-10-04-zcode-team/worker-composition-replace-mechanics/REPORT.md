# REPORT — worker-composition-replace-mechanics (2026-10-04)

Scope honored: read-only analysis of the composition build + rehearsals strictly
inside `outputs/2026-10-04-zcode-team/worker-composition-replace-mechanics/`.
No console contact, no nested subagents, no frozen-tree writes, no script
modifications outside my dir (my script copy is byte-identical to the pinned
fixed script, sha `36cc8199…`).

## Deliverables

- `CLAIM.json` (first)
- `REPLACE-MECHANICS.md` — full map + proven procedure with line cites
- `REHEARSAL-RECEIPTS.json` + `rehearsal/` — raw receipts (run dirs, stdout/stderr, ld -r test objects)
- `REPORT.md` (this file, last)

## Findings

1. **Unit map.** Of the 368 composed units: 6 affected units are ALWAYS
   freshly recompiled (S:93–103) and only accept `--replace-runtime-unit` for
   3 of them (`unreal-unrealinitializer`, `native-psscanbinding`,
   `native-ownedmappedcandidateselection`) — the other 3
   (`native-ownedmappedscansource`, `native-loadercontext`,
   `native-programfactory`) are refused because S:75 pre-stages `W/src`
   defaults and S:83 rejects any target already in `new_sources`. Extras (6 in
   run-3, incl. `lmf-loader-adapter` = lmf_to_native_loader_facts.cpp) accept
   any path via `--extra-unit`. Bootstrap (3) accept `--replace-bootstrap-unit`.
   The other 353 units (incl. `program` = UE4SSProgram.cpp) have NO mechanism.

2. **ld -r semantics (proven T1–T6, same ld.lld invocation as S:165).**
   strong+strong = hard `duplicate symbol` error (no first-wins); strong beats
   weak in either order; weak+weak = first object wins. Therefore an extra unit
   can add symbols or override retained WEAK definitions (4,712 in program.o)
   but can never replace a retained strong definition (83 in program.o,
   including `UE4SSProgram::init/update`, ctors/dtors, and
   `get_game_executable_path` — the exact function worker-configured-roots
   needs to change).

3. **Per-target verdicts.** lmf_to_native_loader_facts.cpp: fully stageable
   today via argv-only change (proven end-to-end). NativeLoaderContext.cpp /
   NativeProgramFactory.cpp: require a script revision (Procedure B) — flags
   cannot reach them, and their angle-bracket includes make the staging-header
   trick unavailable. UE4SSProgram.cpp: double-locked (flag refusal proven in
   refusal-probe-run-1; extra-unit override blocked by strong symbols). Headers
   (PsMappedImageCaller.hpp et al.): frozen pins + -I precedence (S:94/118
   prepend W/include first); only quoted-include consumers co-staged with their
   TU can see modified header content.

4. **New-run feasibility (Phase A).** A fresh composition run (not reuse) with
   modified staging copies works WITHOUT touching frozen trees and WITHOUT
   modifying the fixed script — only the `--extra-unit`/`--replace-*-unit`
   paths in the argv change. The affected-set check (S:57–61) pins the base
   receipt inventory, not the flags; it cannot refuse additional valid flags.
   R2 proved a redirected staging source yields 12/13 fresh units bit-identical
   to run-3 (only diff: the redirected unit's embedded `__FILE__` string,
   8 bytes). R3 proved a real content change + same-basename staging header
   override compile-time (`#error` guard), auto-pinning, and marker export in
   the composed object, with no new undefined imports.

5. **Driver changes to leave run-3** are a short, explicit list (REPLACE-MECHANICS
   §2 table): RUN5/composition-run path, COMPOSE_ARGV_TAIL flags, checkpoint id
   bump, fresh --out and overlay dir, plus script-sha re-pin ONLY if Procedure B
   (script revision) is used. Receipt/checkpoint re-pinning is automatic: the
   driver reads the new run's receipt live and pins object hash/bytes from it.

## Honest limits

- Rehearsals exercised compile/link/nm gates only; no PRX packaging run was
  part of this task, and no claim is made about on-target behavior of any
  change. R2/R3 receipts attest gate passage, not game correctness.
- The strong/weak boundary for units other than `program` was not enumerated.
- The undefined-delta baseline compares against the notify-cancel base receipt
  (S:167), NOT against run-3; run-3 comparison was done out-of-band in this
  analysis and should be re-done for any real integration run.
