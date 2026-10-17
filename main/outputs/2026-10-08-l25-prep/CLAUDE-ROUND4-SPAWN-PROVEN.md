# ROUND-4 SPAWN PROVEN — launch-1 session verdict (2026-10-11)

PRX 59b0528b (round-4, snapshot a79b3655, stamp l26-20261011T030618Z —
confirmed in-log). Contents per the accepted declaration: r3 base + FR
parity revert + FW literal markers; shim/first-launch/KB TU set aside.

## THE RESULT
1. SPAWN: console-proven, receipted by the mod's own log:
   "Menu created" -> FIVE spawns receipted (Brick ID=5e, Shotgun ID=54, +3 more per
   the audit; the two cited were the first). The ItemId read resolves; configure-pickup passes; items
   spawn. THE BLOCKER OF THE ENTIRE PROJECT IS CLOSED on the shape we
   ship (userdata returns; bridge inside :ToString/__tostring).
2. FR PARITY REVERT: proven by the FJ pickup probe ladder (357 PRB
   lines, SIXTEEN completed runs per the IJ citation audit — 1 auto +
   8 F7 presses + 7 rerun-class; stronger than first receipted): PickupInventoryItem read ->
   type=userdata, :type()=UScriptStruct, .ItemId -> type=userdata,
   tostring(id) AND id:ToString() BOTH ok with real values (3f2...),
   actor control ok, [PRB] done x2. Objects pass back — the exact
   PC-mod shape Claude ordered the revert for.
3. DRIVER RATES (GY's question): cb=23/24 sustained across the whole
   session (25 cycles per the audit; ~24 ticks/6s = the 250ms LoopAsync cadence).
   The per-mod async thread is the sustained driver; rates steady in
   menu AND in-world. GY's return-false law held (the counter lives).
4. SELF-TEST: armed (boot v1 M=167 armed=true) but NOT FIRED (0 idx
   lines) — L1+R1 either not pressed or the combo didn't trigger.
   Open item; rides the next launch (Lua-side only).
5. [MAP]: absent — World lookup returned nil/"?" in the states visited
   (db/pc lookups fine, so class-shape-specific). #4's milestone
   question stays open pending a World-state visit or a revised lookup.
6. EXPECTED-WITHOUT-SHIM: progress-io unavailable -> resume disabled
   (admission blocks creating unlisted files) — degraded to SKIP-class
   by design, never FAIL. [CORRECTED per IJ audit: the io-roundtrip
   SKIP is design-derived — the self-test never fired this session, so
   only "progress-io unavailable" is log-receipted.] Round-5 shim or a
   pre-staged loadlisted progress file fixes it.

## DEPLOY NOTES (for the record)
- One real launch incident: MFA refused the whole module
  (mfa_activate_failed -6) because the loadlist declared
  ueselftest/enabled.txt which was never uploaded — all-or-nothing
  admission caught it exactly as designed; fixed (0 B marker uploaded),
  full 22-row readback verified, relaunch clean.
- GQ's re-measure step fired: zzdiag row re-pinned 4065->4094 after
  GY's cb edit. Live lesson recorded: FTP file reads return empty
  under game load — verify with game closed only.

## STATE
- Console: round-4 resident (verified decrypt-off), zzdiag-v11 +
  ueselftest deployed, pad 1736 B, all 22 rows verified.
- AC queue (order law): serial-equivalence proof, P1 fault-ledger,
  P2 variant-B, round-5 (shim + first-launch + KB restore + the
  set-aside restore procedure in ROUND4-CONTENTS-DECLARATION.md).
- Ladder v2 (GC's four type-mods) rides the next launch window with
  the self-test re-arm.

Evidence: evidence/round4-spawn-session-20261011{.log,.decoded.log}
(195,738 B raw, decoded via the GZ tool).
