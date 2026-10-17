# RESTORE-PATH — returning the console to a clean state, OR leaving the mod installed safely

Worker: worker-post-spawn-closure-prep, 2026-10-04. Read-only synthesis + spec. PROPOSAL:
every command class below is drafted from the cited on-disk evidence (rollback runbook,
pinned ploader source, FINAL-INI-LAYOUT, EVE-STATE); none has been executed by this
worker. Predecessor of record: `worker-rollback-runbook/ROLLBACK-RUNBOOK.md` (severities
A–D, the SHT-strip readback law, the recovery ritual). This document EXTENDS it from
"rollback after an incident" to "planned session closure", with the two end-states the
project owes after the acceptance + stability window.

Endpoints: `HTTP = http://ps5-host-not-configured.invalid:8084` (manager; never 9223);
`FTP = ftp://ps5-host-not-configured.invalid:2121` (anonymous). Slot `SLOT =
/data/plugins/modbridge_supplier_composition.prx`; activation root
`ROOT = /data/modbridge/roots/PPSA33286`.

## 0. Standing law (applies to every path)

1. **Game fully CLOSED** for every slot/ini/file operation; an ini edit takes effect only
   at the NEXT arm (pinned `main.c`: ini read once at arm time).
2. **Backup before overwrite** (rollback §7 pre-flight): RETR current bytes + sha256,
   STOR an on-console `.bak`, record BOTH hashes plus the incoming artifact identity
   BEFORE any destructive step.
3. **Readback law:** fself `.prx` readbacks are SHT-strip transformed — PASS = PT_LOAD
   equality vs the payload-of-record inner ELF (allowed classes only: `e_shnum=0` @60,
   last 24 B zeroed, bounded tail); whole-file sha vs the upload sha is MEANINGLESS.
   Plain files (`ploader.ini`, `.log`, `.txt`, `.jsonl`, loadlist, settings ini) read
   back byte-exact — whole-file sha256 IS the verification. Fresh connection per RETR.
4. **Never delete on-console `.bak` copies.** Never re-create the ini header, reorder,
   or "normalize" lines; the only verified ini edit is a single mode byte on an existing
   line (or the tool's content-anchored ensure ops inside `stage-ridealongs`).
5. **Destructive-step ordering = the journal.** On-console manual DELE has no journal and
   no undo; therefore the DOCUMENTED end-state (this section, per session) is written
   into the run receipt BEFORE the first deletion, and the ini is always finalized
   BEFORE any file it references is removed. (Lesson: worker-uninstall-fault-expansion
   D1 — an app-tracked uninstall that deleted its journal before committing records
   stranded an unrepairable half-state; the ini-first rule is our equivalent guard.)
6. **The user owns launch, power, and sign-in.** No automatic retries, no scheduled
   re-checks; crash mid-path → rollback §5 ritual (STOP; full restart; ONE reachability
   probe; plain-file sanity vs recorded shas; a mismatch = possible mid-STOR → treat the
   touched file as unverified and re-restore before the next launch).

## 1. Choose the end-state (decision table)

| Situation | End-state | Path |
|---|---|---|
| Stability gate `SG-PASS` recorded and the user wants the mod usable | **Leave-installed (safe)** | §B |
| No stability pass, or the user wants it dormant but re-enableable | **Dormant**: files stay, composition (+ optionally probes) flipped `= 0` | §B.4 |
| User wants the console as found (or as close as stock allows) | **Full teardown** | §A |
| Crash suspected to involve the mod, before the next launch | Disable first (single-byte flip), decide after triage | rollback §4.4 |

Both §A and §B are first-class outcomes; "leave-installed" is NOT the default — it is
earned by the stability gate (STABILITY-GATE §2 verdict rules).

---

## §A. Full teardown — ordered, verified, reversible-until-the-last-step

End-state options, in increasing depth. **Pick ONE, write it into the session receipt,
then execute top to bottom.** Each step's verification must pass before the next.

### A.1 Pre-flight (rollback §7, mandatory)

- Fresh-connection RETR + sha256 of: `ploader.ini` (expect the session's Step-0
  pre-image — EVE-STATE S3: 247 B `8d331b3f…`, INDEX Track 8 — unless the session's
  INI-BOOTSTRAPPED branch replaced it with the 400 B canonical `bbc34c16…`; the Step-0
  record is authoritative), the slot readback (law-aware), `ROOT/loadlist.txt`,
  `ROOT/ue4ss/UE4SS-settings.ini`.
- Inventory the on-console `.bak` set (`ploader.ini.<TAG>.bak` copies) — they survive
  teardown by law 4.
- Write the chosen end-state + expected readback identities into the run receipt BEFORE
  the first mutation.

### A.2 ploader.ini rollback (ALWAYS first)

Three documented end-states:

- **(a) Restore the pre-image / `.bak`** — the console as found:
  STOR the recorded pre-image bytes (or RNFR/RNTO the newest `ploader.ini.<TAG>.bak`
  back over `ploader.ini`), then fresh RETR + byte-exact diff. Expected sha: whichever
  the session's Step-0 recorded (`8d331b3f…` for the Track-8 era).
- **(b) Stock empty config** — pinned-source-proven: with the ini absent, `read_ini`'s
  fopen fails → clean exit, notification "Config empty", `g_plugin_count == 0`, nothing
  injects (rollback §2, `main.c` L652–653/L840–844). Achieve it by DELETING
  `/data/plugins/ploader.ini` ONLY AFTER (a)'s bytes are already preserved in `C/` and
  on-console `.bak`s exist. HONEST CAVEAT: ploader behavior is pinned-source-proven; any
  pldmgr-side expectation of the file's presence is NOT verified — prefer (a) or (c)
  unless stock truly demands (b), and record the first post-teardown arm's notification
  lines as the proof.
- **(c) Selective disable** — single-byte flips to `= 0` on chosen lines (§B.4
  mechanics). Mode 0 entries are skipped at parse: never registered, never attached, no
  FAIL, no timeout (pinned L672).

Verification for all three: fresh-connection RETR diff/sha (byte-exact law), plus the
rollback-runbook §4.4 diff discipline when editing.

### A.3 File removal (only after A.2 is verified)

With the ini no longer referencing them (or referencing them `= 0`), remove the session's
staged modules from `/data/plugins/`:

```
modbridge_exec_write_probe_armed.prx   modbridge_fc1pool.prx
modbridge_supplier_composition.prx     modbridge_qh_driver.prx
modbridge_runtime_aligned_observer.prx modbridge_runtime_aligned_entry.prx
modbridge_path_test.prx                (+ any *.disabled copies from rename fallbacks)
```

`modbridge_ingame_facts.prx` (mode 0 in the canonical layout) may stay or go; it is
inert at parse. **Never leave the ini pointing `= 2` at a deleted file** — that exact
shape is the proven ploader FAIL path: entry registers → up-to-25 s mode-2 wait →
PT_ATTACH + ucred jack → `access()` fails → FAIL notification → injector polls to the
120 s timeout, on EVERY subsequent launch (pinned §4.2). Rename-fallback copies
(`*.disabled`) must be renamed back or deleted only after a verified re-stage (§4.6).

### A.4 Activation root: settings v2 restore OR removal

- **Restore recorded v2** (the documented rollback inventory — what `stage-settings`
  itself freezes against): upload the v2 pair as ONE lockstep restage —
  `UE4SS-settings.ini` 834 B `9570ead3…` + `loadlist.txt` 446 B `3afd10ce…` →
  `ROOT/ue4ss/UE4SS-settings.ini` and `ROOT/loadlist.txt`; readback BOTH byte-exact
  before declaring done (lockstep law: never a partial pair).
- **Or remove the tree:** the E2E flow's own §7 reset law is the scope precedent —
  only `/data/modbridge/**` is ours to touch. DELE the staged mod tree
  (`ROOT/ue4ss/…`, `ROOT/Mods/…`), settings, loadlist; `runtime-status.txt` may remain
  (it `O_TRUNC`s per session; a stale one is harmless but record it).
- Game files are NEVER touched (policy: game files need backups; the staging surface is
  the slot + the root + the plugin dir + the log channels only).

### A.5 Slot: restore r11-era bytes or go stock

- **Stock:** with A.2(b) + A.4 removal, DELE `$SLOT` as well — nothing references it.
- **Restore r11 (composition-5):** STOR the preserved r11-era fself, then verify by the
  PT_LOAD readback-equality law — never by whole-file sha vs upload. Recorded r11-era
  identities (preserve the divergence, do not silently resolve it):
  - rollback runbook §0: composition-5 staged upload `04869ab3 (superseded r2-era upload — NOT the current slot; current slot = r11 0911af0c per reconciliation)…` 30,786,775 B
    (`worker-composition-5-rebuild/prx-candidate-composition5-2/…`, determinism twin
    `-1` byte-identical); console readback historically logged at 30,786,663 B — the
    runbook's flagged Step-15 discrepancy; fresh RETR is ground truth.
  - EVE-STATE S1: the **pldmgr payload slot** r11 identity `0911af0c…` 30,792,151 B
    (`/data/pldmgr/payloads/` — the dispatchable payload, a different artifact class
    from the staged plugin slot); receipts:
    `worker-fresh-r11-gate-receipt/live-r11-receipt.json` + `v2-r11-receipt.json`.
  - Step-0 of the restore is a fresh RETR of whatever the slot currently holds
    (possibly comp6 `76ee6af3…`), hashed into the run dir BEFORE the overwrite
    (pre-flight law 2).
- If the fself artifact were lost: the bare-readback STOR fallback exists (rollback
  §6.1) with its honest limit — no console proof a bare stripped ELF passes the kernel
  loader; the failure mode is the recoverable ploader FAIL path, not a crash risk.

### A.6 Payload catalog note (honest scope limit)

The session's game-closed flows upload payloads under fixed aliases
(`ALLFAM8_DLL_TEST.elf`, `MODBRIDGE_INST_E2E.elf`) into the pldmgr catalog. The
upload law forbids overwriting pre-existing payload names; a VERIFIED deletion path for
catalog entries is NOT documented in the sources read. Record what was uploaded this
session (aliases + shas) in the receipt; attempt catalog cleanup only if the manager's
own documented API provides it — never improvise an undocumented delete.

### A.7 Post-teardown verification launch

User re-arms ploader and launches Townfall ONCE, with the chosen end-state:
- (a)/(c): notification shows one-fewer-plugin inject than the session's launch (e.g.
  "Injecting N plugin(s)"), zero FAIL lines (read every line — any FAIL retries the
  whole list), game stable unmodded.
- (b): "Config empty", nothing injects, game stable unmodded.
Collect the notification text + `UE4SS.log` head ONCE, hash into the run dir, log
CONSOLE-LOG style. This launch is the teardown's receipt — without it the teardown is
"performed, unverified".

---

## §B. Leave-installed — the safety case

### B.1 What is already proven (the fail-open history)

- **Fail-open proven across eight launches (L1–L8):** every real launch that failed,
  failed LOUD and fail-open — `module_start` returns 0 on any wiring/activation failure,
  the kernel continues, the title runs unmodded (rollback §3; probe build REPORT §2;
  digest items 3/6d). No crash of game or console was ever attributable to the
  composition's own fail-open path. Severity A (loaded-but-runtime-failed) is a designed,
  do-nothing state.
- **Parser-law inertness:** mode 0 entries are skipped at parse (never registered,
  never attached, no FAIL, no timeout — pinned L672); a missing ini exits clean. The
  canonical layout holds 7 registered of MAX_PLUGINS=10 (headroom 3), inside the 120 s
  injector budget (~12–18 s of injection for 7 mode-2 entries; FINAL-INI-LAYOUT §1–§2).
- **Activation-root isolation:** everything the runtime owns lives in (i) its plugin
  slots under `/data/plugins/`, (ii) its config/content root `/data/modbridge/roots/
  PPSA33286/` (settings, loadlist, mod tree), (iii) its own log channels
  (`townfall_runtime_aligned_*.log`, `UE4SS.log`, the two probe verdict jsonl), and
  (iv) the ploader.ini lines. Game content is untouched; the installer family's only
  write scope is `/data/modbridge/**` (E2E §7 reset law). Nothing persists outside these
  surfaces, and the tick detour exists only inside the running game process (rollback
  §4.1: closing the game fully removes it).

### B.2 Residual risk rows (what leave-installed still owes vigilance for)

| Risk | Bound / mitigation |
|---|---|
| Tick detour wrong-but-plausible address (outcomes #6/#7) | Only reachable while the ladder runs; leave-installed is allowed only AFTER CP2/CP3 evidence + `SG-PASS` (never precede the stability gate) |
| ploader FAIL retry quirk (any FAIL mid-list retries the whole list) | Only matters if a staged file later goes missing; pre-flight hashes of all seven slots + ini are in the session record; §7-style inventory re-check after any console incident |
| A future game update | The tick address, layout A, axes, and class paths are build-pinned (0x33ac8c0 + base 0x400000, axes 5.6.1-0+++UE5). After ANY title update the mod is UNVALIDATED: flip the composition line to `= 0` BEFORE the first launch on the updated build; full re-acceptance is a NEW ladder |
| Ini drift (hand edits, lost `.bak`s) | Only content-anchored tool edits; `.bak` set never deleted; Step-0 RETR+sha at every session open |
| Disk/leak concerns during long residence | Exactly what SG-A/SG-B/SG-D measure; without a stability pass, use §B.4 dormancy |

### B.3 Leave-installed procedure (after `SG-PASS`)

1. Record the exact end-state in the session receipt: which of the 7 entries remain
   `= 2`, the ini sha, the seven slot shas, the root inventory (settings v3
   `d8faa4ec…`/`4d98ecd7…` or restored v2), and the `.bak` list.
2. Leave the ridealong probes exactly as the accepted launch had them — their windows
   are self-bounding (observer ~60 s; qdrv1 ~2 min) and read-only.
3. The standing kill switch is §4.4 of the rollback runbook: ONE byte on the composition
   line (`2`→`0`) with a fresh `.bak` and a byte-exact readback — usable at any time,
   game CLOSED, before the next arm.
4. Re-validation triggers (each forces dormancy + a NEW ladder, distinct revision IDs):
   title update, any PRX byte change, any ini layout change, any console crash whose
   triage names the mod.

### B.4 Dormancy (the no-stability-pass middle state)

If the session ends WITHOUT `SG-PASS` but the user prefers not to tear down: flip the
composition line (and, if desired, the probe lines) to `= 0` via the §4.4 procedure.
Files stay on disk untouched; zero injection occurs at any future arm; re-enabling is
the same one-byte edit with a fresh pre-image. This is the safest reversible state that
still preserves the accepted artifacts byte-for-byte.

---

## §C. Evidence to archive with EITHER path

In the closure run dir (plus `C/` backups): the chosen end-state declaration (written
pre-mutation), Step-0 pre-images + hashes, every readback proof, the A.7/B.3
verification-launch notification text, and the updated inventory (what remains on
console, with shas). The acceptance record (ACCEPTANCE-RECORD §5) indexes it.
