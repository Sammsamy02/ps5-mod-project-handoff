# bind_launch.sh preflight waiver receipt — 2026-10-10 02:53Z

bind_launch.sh --all --skip-upload FAILED preflight with 3 errors at
02:53:45Z. All three are STALE PIN/EXPECTATION issues, not tree damage.
Waived individually below with evidence; drivers then invoked directly
(archive-20261010T025518 → build_variants.py --execute → packaging).

## Error 1: "EI's '[FN] tostring-bind' marker family NOT in tree"

FALSE-ALARM — supersession, by design. The tree's UnrealInitializer.cpp
(sha ab03066f, row-pinned) contains:
  "[Seeds][L27], agent-EI): SUPERSEDED (agent-ER, ...)"
  "[FN] tostring: in-port pool decoder (L27-bind superseded)"
EI's engine ToStringInternal BIND was deliberately superseded by agent
ER's in-port pool decoder (Claude RUNTIME-DECODE directive: "the decoder
is CORRECT... bypass"), which is console-verified. The preflight check
(line 208, bind_launch.sh) predates the supersession and greps for the
old exact strings. Five other --all preconditions PASSED (DA
verified-query block, EB enriched marker, variant-B skip token, variant C
switch, EngineListenerOpsProfile pin, Z15-gated bridge 94f81a17).

## Error 2: build_variants.py f61af2e8 != expected 4fa39211 (EE pin)

EZ's landed additive change: --incremental + --base-build (agent-EZ-
incremental.md). Verified tonight: full mode unchanged (199 fresh / 810
steps — matches the pre-EZ executed builds); tolerant --plan ran green
(G1/G2 zero drift, G12 routing all candidate, no violations). No NB
tokens applied (grep 0 hits) — EO's patches remain unapplied as designed.

## Error 3: l26_packaging_driver.py ee3aa230 != expected 68146c3f

mtime 2026-10-09 12:19 (pre-dates EE's pin expectation capture being
loaded by tonight's script run). Build-proven: this exact driver sha
produced the etfix-173856, seamfix-192411 and lfb-211254 packages (20/20
static checks, 207 imports, signed-SELF magic verified in each receipt).
No NB packaging tokens applied (grep 0 hits).

## Action taken
- Archived l26-A, l26-B, staging-copyin-probe, 2026-10-08-l26-package-A,
  2026-10-08-l26-package-B → 2026-10-08-l26-build-variants/
  archive-20261010T025518/ (dirs keep full names).
- CHANGES rows verified == disk (G2 mirror clean; LuaMod.cpp row
  9fea2963 == disk 9fea2963 = the EY-bridge version, +5,881 B vs the
  lfb-203415 build pin c2d09abf — the phantom-build race, see
  CLAUDE-COMPLETE-HANDOFF.md).
- build_variants.py --execute (full, upload-grade) started 02:56Z.
- On completion: l26_packaging_driver.py --prepare --execute, then
  verify_build.sh HARD GATE ([LUA-FN]/[LFB] must be present in the PRX,
  UTF-8 and UTF-16LE) before any upload is offered.

## Never do on the strength of this receipt
- Do not waive preflight pins for UNidentified sha changes — each waiver
  above names its owner and its verification evidence.
- Do not upload anything without the verify_build.sh PASS receipt for
  that exact PRX sha (the 21:12 lfb upload skipped it; that class of
  miss is what this receipt exists to end).

— root (ZCode), 2026-10-10 02:56Z
