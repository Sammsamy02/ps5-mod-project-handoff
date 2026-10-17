# RUNBOOK — next console test: fixed PRX + zzdiag v3 ("MOD MENU UP" run)

Written 2026-10-08 by agent-CK. Exact console-file sequence for the next
launch. Companion docs: ACCEPTANCE-LADDER.md (pass/fail criteria + failure
owners), zzdiag-v3/ (main.lua + LOADLIST-ROW.txt sidecar + staged
loadlist-zzdiag-v3.txt), harvest/harvest.py (now auto-reports `diag-zz`).

Console facts this runbook relies on:
- FTP: `ftp://ps5-host-not-configured.invalid:2121` (ftpsrv; text-file pulls/pushes only —
  executables ALWAYS need `-Q "SITE SELF"` decrypt-OFF handling, PS5 rules).
- Mod root: `/data/modbridge/roots/PPSA33286/`
  - `loadlist.txt` (remote, admission manifest; currently 9 rows, zzdiag v2)
  - `ue4ss/UE4SS-settings.ini` (1,947 B original, HookLoadMap=true)
  - `ue4ss/Mods/zzdiag/Scripts/main.lua` (currently v2, 1,107 B)
  - `ue4ss/Mods/zzdiag/enabled.txt` (0 B, stays as-is)
- Current state per evidence/UFUNCTION-NULLPTR-FINDING.md: variant A
  resident, cache-on settings, loadlist 9 rows, console REBOOTED after the
  v2 crash (CE-108255-1) — do NOT re-run v2's F1/F2 probes; v3 has none.

## Phase 0 — Mac prep (before touching the console)

1. Fixed PRX (the l26 UFunction/FName-resolution fix) built and
   fake-signed: `python3 make_fself.py in.elf out.self` per PS5 rule 3
   (sdk at outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/...).
   Local gate BEFORE upload: magic must be a SIGNED self
   (`4f153d1d`/`5414f5ee`); plain ELF `7f454c46` = STOP.
2. Record fixed-PRX sha256 (it goes into the receipt).
3. zzdiag v3 staged in `outputs/2026-10-08-l25-prep/zzdiag-v3/`:
   - `main.lua` = 2,314 bytes,
     sha256 2670e493d1c89d1f1499d82aad5f9dbd7d1f0fc85aa8798d5e9983f639fa8228
   - re-measure after any edit (`wc -c main.lua`) and update
     LOADLIST-ROW.txt + loadlist-zzdiag-v3.txt together.
4. Decide the optional PRE-SWAP run (v3 on the CURRENT composition to
   re-baseline the nullptr text with the new C-probe vocabulary). Default
   recommendation: SKIP it — the console already needs one clean cycle and
   the v2 evidence pins the baseline; spend the launch on the fixed PRX.
   v3 works identically before and after the swap (same chain), so it
   serves as cross-evidence either way.

## Phase 1 — console prep (one command + two file pushes)

1. **Fixed PRX via the SAME prep flow** (backup, upload, decrypt-OFF
   readback, settings restore, ploader arm, klog tap, ebuild monitor):
   ```
   cd outputs/2026-10-08-l25-prep/harvest
   python3 prep_modded_launch.py --variant A --prx-path <fixed-prx.self>
   ```
   Abort on ANY gate failure (the tool writes LAUNCH-PREP-FAILED-*.md;
   never bypass, never re-dispatch the loadpayload twice).
2. **Upload zzdiag v3** (size-pinned; binary mode, default curl):
   ```
   curl -sS --max-time 20 -T ../zzdiag-v3/main.lua \
     ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua
   ```
3. **Update the loadlist row** (admission is size-exact; v2 1107 -> v3 2314):
   ```
   curl -sS --max-time 20 -T ../zzdiag-v3/loadlist-zzdiag-v3.txt \
     ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/loadlist.txt
   ```
4. **Verify readback** (all three must pass before launch; readbacks are
   RECEIPTS — since 2026-10-10 (agent-HA evidence law) write them under
   `outputs/2026-10-08-l25-prep/evidence/runtime/zzdiag-v3/` instead of /tmp;
   the /tmp paths below are legacy, kept for history):
   ```
   curl -sS --max-time 20 -o /tmp/zz3-readback.lua \
     ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua
   cmp /tmp/zz3-readback.lua ../zzdiag-v3/main.lua        # byte-exact
   curl -sS --max-time 20 \
     ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/loadlist.txt \
     -o /tmp/zz3-loadlist-readback.txt
   cmp /tmp/zz3-loadlist-readback.txt ../zzdiag-v3/loadlist-zzdiag-v3.txt
   grep -c $'\t' /tmp/zz3-loadlist-readback.txt            # 9 file rows
   ```
   Keep v3 DURING the test (recommendation): every L3+R3 press then
   produces BOTH the real mod's markers AND the diag's stepwise twin in
   the same drain — cross-evidence at zero extra presses. Removal is
   Phase 5.

## Phase 2 — launch (user actions, DualSense)

Per agent-AI-spawn-procedure.md section 5 (now with the ladder in mind):

| # | Action | Watch for (ACCEPTANCE-LADDER rung) |
|---|---|---|
| 1 | Launch game; stay in MAIN MENU 90 s | Rung 0: `[l22] Lua mod started: TownfallItemSpawner`, `[TownfallItemSpawner] Loaded v1.0 ...`, `[zz] Z0 v3` |
| 2 | Load the SAME save; wait 30 s | no crash; subsystem now exists (zz A3 can pass) |
| 3 | Hold L3, tap R3 (ONE chord) | Rung 1+2: panel top-right + `<N> spawnable pickups found. Pages=<P>` + `Menu created.` + zz `A3 ok/B1 ok/B2 id=/C1 ok/C2` |
| 4 | Tap DOWN x3 (to first pickup row) | row highlight moves (>> + red) |
| 5 | Press CROSS once on the pickup row | Rung 3: pickup appears ~1.5 m ahead; `Spawned <name> x1 ID=<id>` |
| 6 | Hold L3, tap R3 (close) | panel disappears, sticks re-activate |
| 7 | Clean exit via in-game quit | Rung 4: klog clean, clean-exit state for the next cycle |

Expect BOTH mods to fire on the same chord: the spawner's toggle AND the
zz probe (v2 precedent 19:09:53 — dual-F7 coexistence proven, refs 6/12).
The zz lines interleave with [l22] lines; chunked E/F tails survive —
that is by design.

One launch per boot unless the exit was clean; on ANY crash (expected
signature CE-108255-1) STOP, note the step, reboot before anything else.

## Phase 3 — harvest (auto-reports the diag outcome)

```
cd outputs/2026-10-08-l25-prep/harvest
python3 harvest.py --label l26-menu-1
```
The report's VERDICT section now includes `diag-zz?` (resident diag
version, presses, per-press chain state, reassembled E/F chunks with
truncation/gap notes). Score the run against ACCEPTANCE-LADDER.md — rung
by rung, user-visible items confirmed by the player, klog checked.

## Phase 4 — decision

- All rungs green: "MOD MENU UP" accepted; mission-proving ladder
  complete; proceed to removal (Phase 5) and composition restore.
- Any red: walk ACCEPTANCE-LADDER.md's failure table (F1-F9) — it names
  the owner (admission / resolution-still-null / marshaling / widget /
  pump) from the exact shortfall markers. Preserve all raw files.

## Phase 5 — zzdiag removal + restore (post-test)

1. Remove the two zzdiag rows from the console loadlist (9 -> 7 rows; use
   the l23 composition as the reference — zzdiag-v3/LOADLIST-ROW.txt
   documents both states), re-upload, cmp readback.
2. Delete the remote diag files (curl pre-transfer quote form `-DELE`;
   if the server rejects it, try without the leading dash):
   ```
   curl -sS -Q "-DELE /data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua" ftp://ps5-host-not-configured.invalid:2121/
   curl -sS -Q "-DELE /data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/enabled.txt" ftp://ps5-host-not-configured.invalid:2121/
   ```
   (If DELE fails on this ftpsrv, leaving the files is harmless once the
   rows are gone — unlisted lua files are never admitted; note it in the
   receipt.)
3. Restore the resident composition per the standing law (never leave
   variant B resident; variant A or the designated fixed build only),
   with decrypt-OFF magic + sha256 verify for any executable change.

## Receipt (every phase appends to it)

Label: `l26-menu-1-<UTC timestamp>`. Record: fixed-PRX sha256 + readback
result, v3 upload readback cmp result, loadlist readback + row count,
launch timestamps per table step, user-visual confirmations (panel, item),
harvest report path, removal readbacks. Raw logs are never edited.
