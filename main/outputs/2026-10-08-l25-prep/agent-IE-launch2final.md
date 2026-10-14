# agent-IE — launch-2 FINAL manifest synthesis (2026-10-11)

Task: re-synthesize the launch-2 manifest for the Claude round-8 FINAL
STACK (round-4 base + P1 + P2 + world-fix + round-5 restore) + LADDER V2 +
self-test re-arm + zzdiag v11. Mac-only, battery-safe: no builds, no
console contact, no mod edits. Everything under
`outputs/2026-10-08-l25-prep/launch-manifests/` (+ /tmp fixture).

## Delivered

1. **`MANIFEST-LAUNCH-2-FINAL.md`** — the turnkey manifest: build-stack
   checklist S0..S4 (each labelled+gated, patch paths + post-P2 shas +
   never-apply-without laws pinned; markers 15→18 at S4; `<final-stack-prx>`
   placeholder); the 30-row loadlist; the pad + combo budget; upload order
   law + readback incl. the −6 lesson; prelaunch_rowcheck.py as the
   automatic pre-launch gate; GO/NO-GO + reconciled rollbacks; ST2/[MAP]/
   [FL-MAP]/PRB-7 expectations; user sequence + watch list.
2. **`launch2-final-loadlist.txt`** — 35 lines / **30 file rows** / 1766 B,
   sha b860dee1…; = launch-1 live composition + 8 ladder rows; zzdiag@4086
   + ueselftest-main@3987 (current measured; both RE-MEASURE-AT-DEPLOY).
3. **`launch2-final-loadlist-rollback.txt`** — byte-identical to the live
   launch-1 state (27 lines / 22 rows / 1292 B, 9883ed1c…).
4. **`launch2-final-PS5Pad.ini`** — FV regen over ALL 7 resident mods
   (incl. the four ladder mods): **1736 B, 7 bindings** (6 pinned + L1+R1
   =F12), `--check --expect-size 1736` = 0 violations / 0 warnings.
5. **`launch2-final-rowcheck-pins.json`** — 30-row `--expect` pins for
   ID's `harvest/prelaunch_rowcheck.py` (sha-pinned ladder mods + managed
   rows; size-only residents; remeasure annotations).
6. **`launch2-final-audit-receipt.txt`** — the offline receipts (below).
7. `MANIFEST-LAUNCH-2.md` updated with a SUPERSEDED pointer (body kept).

## Compact answers

- **Row count**: 35 tab-lines / **30 file rows** (4 config + 2 spawner +
  1 UEHelpers + 2 zzdiag + 13 ueselftest + 8 ladder). Rollback = 27/22.
- **Pad verdict**: **1736 B stands** for the full round-8 set. The four
  ladder mods add ZERO pad rows (generator-extractor verified: their only
  "keybinds" are engine ConsoleKeys, invisible to the pad by design). Combo
  budget: 1 of 10 pairs10 slots used, 9 free, ceiling 16 rows; the only
  co-fire stays the reviewed F7 spawner+zzdiag pair (`--allow-cofire`).
  Bindings identical to launch-1's pad; only the inputs-sha256 header
  moves (N3 law) — upload the manager's deploy-time regen, never
  cross-regenerate.
- **Offline validation**: prelaunch_rowcheck.py fixture run over the exact
  staged bytes = **30/30 PASS**; keymap --check = 0/0. The FIRST fixture
  run caught IC's concurrent edits as SIZE-MISMATCH (zzdiag 4094→4086,
  ueselftest main 3961→3987, landed 09:04-09:06 local) — the gate works
  and those two rows are marked RE-MEASURE.
- **Open re-measures**: (1) zzdiag-v11/main.lua — IC mid-prep edit added
  PRB-7 fname-eq; pin actual wc -c/sha at deploy (history: 4065→4094
  live→4086 current). (2) ueselftest/main.lua — IC's RUN-start line
  (3961→3987; ST2 expectations updated: `RUN start 1..167`). (3) Pad + its
  1736 row re-`wc -c` if the manager's regen header length ever shifts.
   (4) `round5-staging/world-fix/` (IA) + S3-declared markers + IB's
   writable-shim patch — not on disk when this was written; re-check at AC
   build. (5) `<final-stack-prx>` sha when S4 builds. (6) GQ's
   launch1-loadlist-rollback.txt zzdiag row (4065) is stale — re-pin at
   any rollback execution (documented in FINAL §5).
- **−6 lesson wired in**: every enabled.txt-class row is in the upload
   list, readback-cmp'd, and covered by the automatic
   `prelaunch_rowcheck.py --expect` gate (a declared-but-absent row = the
   -6 whole-module refusal; a size mismatch = code=-8).

Rollback reconciliation: launch-2 rollback = launch-1 final state (live,
re-measured rows carried); launch-1 rollback = round-4 resident state
(pad 3718 in place) with the stale zzdiag row re-pinned at execution.

— agent IE (ZCode), 2026-10-11. Zero console contact; /tmp/ie-l2f-fixture
disposable; read-only on all other agents' files.
