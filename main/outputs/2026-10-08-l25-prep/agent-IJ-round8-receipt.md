# agent-IJ-round8-receipt.md — round-8 recording + round-4 citation audit

Agent IJ (ZCode), 2026-10-11. Docs only, battery-safe, no console/build
contact. Read: CLAUDE-ROUND4-SPAWN-PROVEN.md, WAKE-RUNBOOK-20261010.md
(§10 round-4 update + this round's edits), FROZEN-KNOWN-GOOD-20261010.md
(round-4 freeze section), ROUND4-CONTENTS-DECLARATION.md, and the round-4
decoded log (spot-verified).

## Deliverables

1. `CLAUDE-ROUND8-RECEIPT.md` — all 7 round-8 items → status → artifact →
   what launch-2 proves. Status summary: freeze DONE (root, FROZEN §ROUND-4
   FREEZE); fname-eq + self-test lines/trigger AUTHORED (GB ueselftest, call
   29/37 + EXPECTED-MARKERS.txt + 1736 B pad) but execution PENDING (armed
   not fired); World bug OPEN with FJ's [MAP] logger deployed; writable area
   + labelled-build queue STAGED-AT-AC (round4-setaside/ + WAKE-RUNBOOK
   §11/§12 order law; serial-equivalence proof NOT yet on disk); deploy gate
   DONE in-session (MFA refusal caught+fixed, laws standing).
2. `STATE-L26-WAVE.md` §10 appended — round-4 spawn-proven session receipts
   (one paragraph), round-8 map, console state (round-4 resident known-good,
   ladder-v2 queued on the final gated round-5 build).

## Citation audit of CLAUDE-ROUND4-SPAWN-PROVEN.md vs the decoded log

All greps `grep -a`, run over BOTH
`evidence/round4-spawn-session-20261011.decoded.log` (191,682 B) and the
raw `.log` (195,738 B — receipt's raw size EXACT).

VERIFIED EXACT:
- `[BUILD] id=l26-20261011T030618Z … snapshot=a79b3655` (L23) — the
  in-log stamp confirmation claim holds.
- "Menu created." (L995), "Spawned Brick x1 ID=5e" (L1070), "Spawned Shotgun
  x1 ID=54" (L1243) — verbatim.
- 357 PRB-family lines — EXACT (`grep -c "\[PRB"` = 339 `[PRB-*` + 18
  `[PRB]*` = 357, same in raw and decoded).
- cb=23/24 sustained — Lua T-lines carry 1x cb=23, 24x cb=24; the 250ms
  LoopAsync counter matches v11's design (n250 reset per 6s cycle).
- `[ST2] boot v1 M=167 armed=true pf=false` — verbatim (L824/826); 0 idx
  lines (0 `idx` matches in raw AND decoded) — armed-not-fired confirmed.
- `[MAP]` absent — 0 lines in raw and decoded; consistent with v11 main.lua
  (`lm` defaults "?", prints only on a resolved World:GetName change). The
  receipt's "db/pc lookups fine, so class-shape-specific" is supported:
  all T-lines db=v pc=v and `[FFO2] q='World' … exact=2` (L849).
- FR-parity detail lines — `[PRB-1] inv ok=1 type=userdata`, `udt ok=1
  t=UScriptStruct`, `[PRB-2] id ok=1 type=userdata`, `[PRB-3] tos ok=1
  val=3f2`, `[PRB-4] TS ok=1 val=3f2` (+[LFB-M] fstring-rev ok=1), `[PRB-6]
  actor ok=1` — all present verbatim.
- `[ST2] progress-io unavailable (resume disabled)` — verbatim (L820/822).
- 24 spawnable pickups / Loaded v1.0 lines present; K1 f7 = K2 q = 8
  (drain fine every press).
- PRX receipts (not log claims): package-A FINAL-RECEIPT sha
  59b0528b…4af61, 31,423,015 B, twins identical, symbols sidecar paired —
  verified on disk; zzdiag-v11 main.lua = 4094 B (matches the re-pin).

DRIFT (honest):
1. "[PRB] done x2" / "two full runs incl. F7 rerun" — ACTUAL: **16**
   completed `[PRB] done` runs (08:48:10→08:50:16), all full (PRB-1..6 each;
   PRB-5/6 also x16). Attribution: 1 auto @18s + 8 K1 f7 presses = 9
   accounted; the other 7 cluster within 1–2 s of others (key-repeat
   double-enqueues the likeliest cause; not established from the log).
   Understatement — the parity proof is STRONGER than receipted, but the
   "x2" figure is wrong as written.
2. "27 T-lines" — ACTUAL: 25 T-cycles (T1–T25, [Lua] lines); 27 is the
   line-count including 2 `[LFB-M] print-widen` duplicates of T1/T2.
   Direction of the claim (sustained cadence) unaffected.
3. "The io-roundtrip test SKIPs (no-io-path)" — NO SUCH LINE in the log (0
   `roundtrip` matches): the io-roundtrip call is one of the 167 self-test
   calls that never fired. The claim is design-derived (EXPECTED-MARKERS
   SKIP taxonomy); the log-receipted half is only the progress-io line.
4. Spawn receipts under-cited: the log shows 5 spawns total (plus 3x
   "Spawned Shotgun Ammo x1 ID=55"); the receipt cites 2 of 5. Selective,
   not false.
5. Deploy-note facts NOT in this log (deploy-time, root/klog side,
   unverifiable here): `mfa_activate_failed -6`, the 22-row readback, the
   4065→4094 re-pin event (the 4094 B file size itself IS confirmed on
   disk). Recorded as root-reported, not log-receipted.

Bottom line: the receipt's core claims (spawn proven, FR parity proven,
cb sustained, armed-not-fired, [MAP] absent, stamp in-log) all verify; the
run-count figures drift low (16 runs not 2; 25 cycles not 27) and one
io-roundtrip claim is design-derived rather than log-receipted.
