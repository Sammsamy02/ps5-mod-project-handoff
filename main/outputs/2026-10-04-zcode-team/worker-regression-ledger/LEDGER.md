# LEDGER — loading-regression forensics, 2026-10-05 18:40 → ~20:40 local

Worker: worker-regression-ledger · 2026-10-05 · READ-ONLY archaeology + this ledger.
No console contact, no source edits, no game claims. Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`.

Provenance classes used throughout:
- **ON-DISK** — a receipt/artifact exists in this workspace or `/tmp`; path given; hashes recomputed by this worker where stated.
- **DERIVED** — computed by this worker from ON-DISK bytes (arithmetic/byte-diff, no new console data).
- **PROMPT-ONLY** — recorded only in the controller's task brief of 2026-10-05; no on-disk receipt found. Flagged wherever used.

HEADLINE (byte-proven, DERIVED): **the "pre-image" 34eafe5d is NOT the console's stored
bytes — it is ftpsrv's RETR VIEW (decrypt + SHT/SIE-note strip + 2,016-byte tail
truncation) of the stored FSELF 07ac3e1e.** Consequently the task brief's no-strip
A/B chain links 2–4 are refuted, and every failing launch L1–L5 ran a NON-FSELF
composition PRX while the one good boot (pid 700) ran the FSELF. Details in §3.

---

## 1. Governed content states (sha256, bytes)

| file (console path) | pid700-good era (18:40) | after my staging (20:07:37) | after restores (~20:3x, pre-L5) |
|---|---|---|---|
| `/data/plugins/modbridge_supplier_composition.prx` | **STORED: FSELF `07ac3e1e`** 30,849,159 B (ON-DISK build receipt aside-1791247296; RETR VIEW seen as `34eafe5d` 30,849,624 B — see §3) | `8ede717f` bare ELF 30,839,416 B (ON-DISK 200730 l10.3+l10.4) | `34eafe5d` bare stripped ELF 30,849,624 B (PROMPT-ONLY STOR ~20:2x; bytes ON-DISK in both run dirs' `l10-composition.prx.pre`) — **NOT the pid700 stored artifact** |
| `/data/pldmgr/payloads/ploader/ploader.elf` | `edf61245` 142,104 B (ON-DISK `/tmp/ppsa33286-preimages-next/ploader.elf.pre`) | `4c339235` 05r 144,248 B (ON-DISK 200504 GO + phase-B readback) | `edf61245` (ON-DISK restore receipts `/tmp/ploader-old.elf` + readback `/tmp/ploader-readback.elf`, both `edf61245…`, mtimes 20:12) |
| `/data/modbridge/roots/PPSA33286/loadlist.txt` | `c656740a` 447 B (ON-DISK `/tmp/ppsa33286-preimages-next/loadlist.pre`) | `cbe30136` (200504 GO; CONTENT CHANGE: settings row 1947→221 B, Member/VTable/PS5Pad rows removed) | `c656740a` (PROMPT-ONLY STOR ~20:3x; source bytes ON-DISK) |
| `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini` | `b980cea7` 1,947 B (ON-DISK pre) | `f6d06cad` 221 B (200504 GO; CONTENT CHANGE: full config → minimal) | `b980cea7` (PROMPT-ONLY STOR ~20:3x; source bytes ON-DISK) |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | `f586043a` (ON-DISK pre) | `f586043a` re-STORed, content UNCHANGED | unchanged |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | `5a8f6609` (ON-DISK pre) | `5a8f6609` re-STORed, content UNCHANGED | unchanged |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | absent or empty (pre-image 0 B, ON-DISK; absence vs empty indistinguishable) | `e3b0c442…` (empty) re-STORed | unchanged |
| `/data/plugins/ploader.ini` | `1216aab6` 440 B — 2-plugin config (composition=2, path_test=2) | `1216aab6` — frozen byte-equal before/after in ALL staging passes (ON-DISK) | `1216aab6` (never written by any flow) |

The full `07ac3e1e` FSELF bytes SURVIVE on disk in three aside snapshots
(`launch11-combined/prx-candidate-l11-1-aside-{1791247296,1791243375}/` and
`prx-candidate-l11-2-aside-1791248590/`, each `modbridge_supplier_composition.prx`,
sha recomputed `07ac3e1e6c32580531bd4442a6817dd8cd36331c9500120ff72f04414704740c`).

## 2. Console byte-touch timeline, 18:40 → ~20:40

Reads (RETR/HTTP GET) omitted except where they are the only record of a state.
Writes (STOR) bold. All times local 2026-10-05.

| # | time | console write | content | receipt class |
|---|---|---|---|---|
| 0 | ~18:37 | **STOR** `/data/plugins/modbridge_supplier_composition.prx` (previous session's compose) | FSELF `07ac3e1e` 30,849,159 B | PROMPT-ONLY for the STOR act; the stored IDENTITY is ON-DISK-derivable (§3 derivation) — the pre-image 34eafe5d can only be served by ftpsrv from a stored FSELF wrapping bare-sdk `2b700bdd`, i.e. this build |
| 1 | 18:40:10 | (no write — boot, RAN FINE, pid 700: `runtime_start_ok` +831 ms, Lua harness green; archive `launch10-evidence/pid700-fullfix/`) | — | ON-DISK |
| 2 | 20:04:26 | run `20261005-200426-stage-next`: G0 fetch OK, then halted; `go-nogo.json` = NO-GO halted-before-completion. Halt point between the G0 fetch and the Gate-A0 readback step — the A0 `.writeprobe` STOR *may* have executed | possible `/data/modbridge/intake/.writeprobe` | ON-DISK receipts; write uncertainty flagged |
| 3 | 20:05:04–37 | run `20261005-200504-stage-next` (GO, phases A+B): **STOR** `.writeprobe` (=loadlist bytes cbe30136; 16-byte readback ON-DISK `gate-a0-readback.txt` = "modforge-loadlis"); **STOR** `ploader.elf.20261005-200504.bak` (=edf61245); **STOR** ×4 tree files (2 unchanged, settings f6d06cad, enabled.txt); **STOR** `loadlist.txt` cbe30136 (LAST); **STOR** `ploader.elf` 4c339235; readback of every upload passed (no mismatch artifacts exist; GO verdict ON-DISK) | see §1 | ON-DISK |
| 4 | 20:05:28 | run `20261005-200528-stage-c`: preflight FAIL (local candidate path missing) — **zero console writes** | — | ON-DISK |
| 5 | 20:05:35–48 | run `20261005-200535-stage-c`: RETR pre-image (`34eafe5d` 30,849,624 — the ftpsrv VIEW, §3); **STOR** `.prx.20261005-200539.bak` (=34eafe5d); **STOR** composition.prx = d6922807 FSELF 30,851,495 B; RETR readback `33a48f32` 30,837,400 B (≠ local FSELF → content-law FAIL); **STOR** composition.prx = 34eafe5d (restore); RETR restore-readback 34eafe5d byte-exact (ON-DISK `l10-restore-verdict.json`) | d6922807 → 34eafe5d | ON-DISK |
| 6 | 20:07:30–37 | run `20261005-200730-stage-c` (GO): RETR pre-image 34eafe5d again; **STOR** `.prx.20261005-200733.bak` (=34eafe5d); **STOR** composition.prx = 8ede717f bare ELF 30,839,416 B; RETR readback **byte-identical** (ON-DISK `l10-readback-verdict.json` ok=true) | 8ede717f | ON-DISK |
| 7 | ~20:09:13 (DERIVED, §5) | L1 launch: 4c339235 ploader + 8ede717f PRX → FAIL (user memory "error code") | — | PROMPT-ONLY (marker ON-DISK-corroborated class, §5) |
| 8 | 20:12 | **STOR** `ploader.elf` = edf61245 (restore); pre-image `/tmp/ploader-old.elf` + post-STOR readback `/tmp/ploader-readback.elf`, both sha `edf61245…` 142,104 B, mtimes 20:12 | edf61245 | ON-DISK (in /tmp) |
| 9 | ~20:16:12 (DERIVED) | L2 launch: edf61245 + 8ede717f → FAIL (no transcription) | — | PROMPT-ONLY |
| 10 | ~20:2x | **STOR** composition.prx = 34eafe5d (restore of the pre-image bytes) | 34eafe5d (stripped bare ELF) | PROMPT-ONLY (source bytes ON-DISK) |
| 11 | between L2 and L4 | L3 launch: FAIL; **no PATH_TEST marker** that boot | — | PROMPT-ONLY |
| 12 | ~20:28:07 (DERIVED) | L4 launch: FAIL | — | PROMPT-ONLY |
| 13 | ~20:3x | **STOR** `loadlist.txt` = c656740a; **STOR** `UE4SS-settings.ini` = b980cea7 (full pre-state restore) | c656740a / b980cea7 | PROMPT-ONLY (source bytes ON-DISK) |
| 14 | after ~20:3x | L5 launch: FAIL with verbatim transcription `ploader ppsa33286 fail m85 ms modbridge_supplier_composition.prx`; "second plugin said OK"; no rc, no restore/detach values (`incident-loading-regression/POPUP-TRANSCRIPTIONS.md`) | — | ON-DISK (transcription file) |

Additive console artifacts created in-window (all inert to the loader — the .ini
names explicit paths, and `.prx.<TAG>.bak` does not match any plugin name):
`/data/modbridge/intake/.writeprobe` (cbe30136 bytes; once or twice, #2 uncertain),
`/data/pldmgr/payloads/ploader/ploader.elf.20261005-200504.bak` (edf61245),
`/data/plugins/modbridge_supplier_composition.prx.20261005-200539.bak` (34eafe5d),
`/data/plugins/modbridge_supplier_composition.prx.20261005-200733.bak` (34eafe5d).
Note: the 200535/200730 restore-verdict JSONs record the console_bak with a
literal `<TAG>` placeholder — the actual TAGs above come from the events.logs.

## 3. The ftpsrv RETR-VIEW discovery (refutes the stated A/B chain)

Byte-derivation, all steps recomputed by this worker from ON-DISK bytes:

- **View identity.** `34eafe5d` (the 20:05/20:07 pre-image, 30,849,624 B) equals
  `2b700bdd` (the 18:36-era bare-sdk build, 30,851,640 B, from
  `prx-candidate-l11-1-aside-1791247296/bare-sdk/`) truncated by exactly 2,016
  tail bytes with EXACTLY 15 byte diffs: e_shnum 27→0 (offsets 60–61) and the
  24-byte SIE-note tail at 30,849,600–30,849,623 zeroed (local bytes contain
  `"SIE\0"` + an 8+8 build-id; view has zeros). 30,851,640 − 2,016 = 30,849,624.
  No other byte differs.
- **Same transform on the d6922807 attempt.** The 20:05:35 readback `33a48f32`
  (30,837,400 B) equals `8ede717f` (30,839,416 B) under the SAME delta:
  −2,016 bytes, e_shnum zeroed, SIE-note tail zeroed, 15 diffs. Two independent
  builds, identical transformation fingerprint.
- **ELF stores are NOT transformed.** The 8ede717f ELF STOR read back
  byte-identical (200730 l10.4, ON-DISK). The edf61245 ploader ELF restore read
  back byte-identical (/tmp, ON-DISK). So the channel is identity for plain-ELF
  stored content, and the transformation is a property of the SELF-decrypt RETR
  path (ftpsrv advertises "SELF/ELF auto-decryption").
- **Therefore** the console's STORED content before my staging was a FSELF
  wrapping bare-sdk `2b700bdd` — i.e. `07ac3e1e` (the 18:36 build receipt's
  final_prx; a stored bare ELF is excluded because an ELF store would have read
  back untransformed). pid 700 (18:40, RAN FINE) loaded THAT FSELF.
- The `07ac3e1e` FSELF bytes survive on the host in three aside snapshots (§1).

Consequences for the task brief's chain (task item 3), link by link:

| link (as stated) | verdict |
|---|---|
| 1. bare-sdk upload (8ede717f, 30,839,416) read back byte-identical | **VERIFIED** (ON-DISK) — but only proves identity for THIS plain-ELF store/retr pair |
| 2. ⇒ ftpsrv does not transform large files | **REFUTED as generalized**: the d6922807 FSELF upload read back 33a48f32 (transformed, ON-DISK). ftpsrv's RETR is identity for stored ELFs, decrypt+strip for stored SELF/FS ELFs |
| 3. ⇒ the 34eafe5d pre-image IS the console's true content | **REFUTED**: it is the decrypted+stripped RETR VIEW of the stored FSELF 07ac3e1e, not the stored bytes |
| 4. ⇒ my PRX restore is byte-exact to the pid-700-good content | **REFUTED at stored-bytes level**: the ~20:2x restore STORed the VIEW (a bare, section-less, SIE-note-stripped ELF). The good boot's stored artifact was the FSELF. Byte-exact only to the pid700-era *RETR view* |
| 5. ⇒ the file is exonerated | **INVERTED**: content/format is the LEADING suspect — see §4/§7 |

Gap flagged honestly: the ~18:37 STOR itself is PROMPT-ONLY; the stored-FSELF
identity is an inference from the deterministic derivation above (it excludes a
stored bare ELF, and the only 18:36-era FSELF wrapping 2b700bdd is 07ac3e1e).

## 4. Loader-instance history per launch + popup-format forensics

Lineage formats (strings extracted by this worker from the ON-DISK binaries):
- `edf61245` (baseline): `[ppsa33286] FAIL (%ldms): %s` — NO rc, NO restore/detach.
- `4c339235` (05r): `[ppsa33286] FAIL (%ldms, restore=%d, detach=%d): %s`
  (source `ploader-build/main.c:877`; structural checks receipts agree).

| launch | ploader ON DISK (provenance) | PRX stored | popup evidence | lineage that RAN |
|---|---|---|---|---|
| pid700 (18:40) | edf61245 | FSELF 07ac3e1e | boot OK (no popup) | edf61245 + FSELF = GOOD |
| L1 (~20:09) | 4c339235 (200504 GO readback) | 8ede717f bare ELF | user memory "error code" | **consistent-not-proven** with the 05r format (restore=/detach= numbers could be remembered as "error code"); no verbatim transcription |
| L2 (~20:16) | edf61245 (20:12 restore readback) | 8ede717f | FAIL, no transcription recorded | edf61245 (only instance on disk) |
| L3 (L2..L4) | edf61245 | 34eafe5d if the ~20:2x restore preceded it (prompt ordering suggests yes for L4; L3 uncertain) | FAIL; NO PATH_TEST marker that boot | edf61245 |
| L4 (~20:28) | edf61245 | 34eafe5d | FAIL, no transcription | edf61245 |
| L5 (post-20:3x) | edf61245 | 34eafe5d | verbatim: FAIL `(m85 ms)` format, NO restore=/detach=, NO rc; "second plugin said OK" | **edf61245 PROVEN** — format matches `FAIL (%ldms): %s` and is incompatible with the 05r string which would print literal `restore=N, detach=N` |

Discriminator inside L5: the SAME boot loaded `modbridge_path_test.prx` (enabled
`=2` in the frozen 2-plugin ini, ON-DISK) successfully — "The second plugin said
OK" — and produced the fresh PATH_TEST marker, while the composition PRX FAILED
in ~85 ms. One plugin loading and the other failing in the same boot, same
loader, same kernel, same game process ⇒ the rejection is SPECIFIC TO THE
COMPOSITION PRX BYTES/FORMAT, not a generic loader/kernel/game failure.

## 5. PATH_TEST markers, wall-clock anchors, reboot analysis

- Marker format (entry log `/data/townfall_runtime_aligned_entry.log`, extracted
  by worker-evidence-pull): `PATH_TEST: /data/plugins/UE4SS.log writable at <ns> monotonic-ns`.
- pid700 era marker `86785799451051` — ON-DISK
  (`worker-evidence-pull/dryrun-pid700/results.json`, from the archived chain).
- Fresh markers `92057819361968` (L1), `92476239711905` (L2), none (L3),
  `93190748570750` (L4 per the brief's slot ordering) — PROMPT-ONLY (the pulls
  are not archived under outputs/). The brief records L5's status as OK but its
  numeric marker is NOT in the brief — flagged gap.
- Reboot analysis (DERIVED): the four PATH_TEST stamps share one
  CLOCK_MONOTONIC-ns scale and are strictly increasing
  86.786e12 → 92.058e12 → 92.476e12 → 93.191e12 with no reset ⇒ **no console
  reboot occurred between the pid700 boot and L4** (L3 bracketed by L2/L4; L5
  only weakly bracketed — no number recorded). The pid700 sdiag/status ms clock
  (t_first 86,713,681 ms ↔ wall stamp 18:40:10 in `fullfix-ue4ss.log`) anchors
  wall time: pid700 marker ≈ 18:41:21; L1 ≈ **20:09:13**, L2 ≈ **20:16:12**,
  L4 ≈ **20:28:07** (±s of clock skew; DERIVED). These are consistent with the
  20:12 ploader restore sitting between L1 and L2.
- Console uptime at 18:40 was already ≈ 24.1 h (86,713 s) — long-running session
  (consistent with the user's keep-awake authorizations; no reboot record exists
  anywhere in outputs/).
- kstuff/etaHEN RUNNING versions: NO HTTP-endpoint capture is recorded anywhere
  in outputs/ (negative search). Only INSTALLED payload files are documented in
  the pldmgr listing (ON-DISK): `etaHEN-13.60.elf` (web_upload 2026-09-30),
  `kstuff.elf` v1.6.7, `kstuff-lite_v1.11` — installed ≠ injected; which payload
  set armed at each launch is not receipted.

## 6. Game-binary (eboot) identity

- Pinned sha (ON-DISK, `worker-l3-profile/game-profile.townfall-ps5-ue56-v1.json`):
  eboot.bin (decrypted) `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,
  194,241,424 B; S-image anchor recomputed at profile generation (2026-10-05)
  from the LOCAL retained copy `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`.
  RE notes (`worker-re-fname-pool/FNAME-POOL-RE.md:5`) and the branch-zero memo
  (`worker-blocks0-misread/DECISION-MEMO-BRANCH-ZERO.md` §2) pin all layout
  addresses to this sha.
- Console-accessible re-verification WITHOUT a game run: the profile records
  `executable_console_path: /data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin`
  — under `/data`, i.e. the ftpsrv-reachable tree. RETR + sha256 would re-verify
  the DUMP identity. Limits: (a) a 194 MB RETR is a size class never yet
  exercised in receipts (max proven-clean RETR = 30.84 MB); (b) this verifies
  the dump, not the running title-mount image (`kConfiguredGameDirectory=/app0`,
  profile S5; the running image's identity has NO non-game-run verification
  path in any recorded tooling).
- Indirect running-image consistency evidence (all ≤ 18:40, none after):
  in-game lmf spans, GMalloc V1/V2 exact-match, CurrentBlock=68 identical across
  boots 645/672/700 (`pid700-fullfix/EVIDENCE.md` §2/§5). L1–L5 produced no
  runtime address evidence (the composition module never loaded).
- igf tooling captures (`igf6-results.jsonl`, `anchor-validation-results.jsonl`,
  `ingame-v1/v2/v3-facts`, all Oct 4) contain no game-file hash pulls via FTP;
  no receipt anywhere shows ftpsrv browsing `/mnt/app0`, `/mnt/sandbox`, or
  `/user/app` (negative search).

## 7. Verdict table

| hypothesis | evidence FOR | evidence AGAINST | observation that would decide it |
|---|---|---|---|
| **PRX content/format (FSELF → bare/stripped ELF)** — leading | Every failing launch (L1–L5) stored a non-FSELF composition PRX (8ede717f or 34eafe5d — §2/§3); the only good boot stored the FSELF; same-boot L5: path_test plugin OK + composition FAIL ⇒ file-specific rejection; L2–L5 fail under the GOOD ploader edf61245 ⇒ ploader variable controlled; 85 ms FAIL = fast load-stage rejection | None observed. (Kernel acceptance of bare/stripped ELF vs FSELF is not independently documented in receipts — mechanism unproven, correlation total) | Restore the surviving `07ac3e1e` FSELF bytes (exist in 3 asides) as the composition PRX, game closed, launch: success confirms; also cheap zero-run check — RETR `modbridge_path_test.prx` and read its magic (expected FSELF-class under this hypothesis) |
| staged-content (loadlist/settings/tree) | loadlist c656740a→cbe30136 and settings b980cea7→f6d06cad were real content changes; L3/L4 ran with them in force | L5 ran with BOTH restored (PROMPT-ONLY STORs) and still failed; L1/L2 failed BEFORE those files were ever changed relative to L3/L4 state... L1/L2 ran with the NEW loadlist/settings too — but L2's failure is already explained by the PRX under the good ploader | None needed unless FSELF restore still fails; then diff the restored loadlist/settings readbacks (their restores are PROMPT-ONLY — see gaps) |
| loader-instance mix (05r vs baseline) | L1 ran 4c339235; its "error code" memory is format-compatible | L2–L5 all ran edf61245 (restore receipt ON-DISK for L2+; L5 format PROVEN baseline); L2–L5 still failed ⇒ loader lineage is not the discriminator | None — controlled by L2–L5 unless L1 alone had a second cause (unfalsifiable without a transcription) |
| kernel-state drift (no reboot) | Monotonic-ns continuity proves NO reboot pid700→L4 (§5); repeated failed injects could in principle drift state | Same kernel session ran pid700 GOOD; the file-specific discriminator (path_test OK / composition FAIL in one boot) makes global kernel drift unnecessary; drift unmeasured by any receipt | If FSELF restore still fails: arm/inspect kstuff/etaHEN state endpoints (none recorded today) or bounded console reboot, then FSELF retry — distinct observations, never replays |
| game updated (binary changed post-18:40) | No direct evidence either way; game auto-update is unmonitored | No running-image evidence after 18:40 exists to compare; hypothesis adds nothing the PRX hypothesis doesn't cover (L5's path_test OK shows the game process launched and accepted a plugin) | FTP RETR + sha256 of `/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin` vs `3b6cfad4…22fc` (game closed, no game run); running-title identity has no non-run verification path |

## 8. Gaps and honesty notes

1. 18:37 compose STOR: PROMPT-ONLY (act), ON-DISK-derivable (stored identity, §3).
2. Run 200426: possible unverified `.writeprobe` STOR (halt point documented §2 #2).
3. ~20:2x PRX restore and ~20:3x loadlist/settings restores: PROMPT-ONLY STORs;
   no post-restore readback receipts exist for them (unlike the ploader restore,
   which has /tmp readback receipts). L5's exact console state therefore rests on
   those prompt records + the 20:07:30 pre-image.
4. L3's PRX state (pre- vs post-~20:2x restore) unresolved; L5's PATH_TEST
   marker number unrecorded in the brief.
5. L1 popup is user memory, compatible-not-proven with the 05r format.
6. The 200535 content-law verdict ("cannot attribute the divergence to the
   SHT-strip class") was correct per its law but compared against the wrong
   reference: for a stored FSELF, the RETR view must be compared against the
   INNER bare ELF, not the FSELF container. The upload itself was likely fine.
7. `INDEX-pid700-fullfix.md` (2747 B, seen 19:30) was no longer present at read
   time — concurrent archiving by another worker; nothing in this ledger depends
   on it (MANIFEST + EVIDENCE.md carry the content).
8. The staging receipts' `console_bak` fields embed a literal `<TAG>` placeholder;
   actual .bak paths were reconstructed from events.logs (§2).

## 9. Receipt index (absolute paths)

- Staging runs: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-staging-next/runs/{20261005-200426-stage-next,20261005-200504-stage-next,20261005-200528-stage-c,20261005-200535-stage-c,20261005-200730-stage-c}/`
- Staging script (phase law): `.../worker-staging-next/run_stage_next.sh`; build/structural/dry-run receipts: `.../worker-staging-next/{receipts,ploader-build,structural-checks,patch-verify,dry-runs}/`
- ploader restore receipts: `/tmp/ploader-old.elf`, `/tmp/ploader-readback.elf` (20:12)
- pre-images: `/tmp/ppsa33286-preimages-next/` (ploader.elf.pre edf61245, ploader.ini.pre 1216aab6, UE4SS-settings.ini.pre b980cea7, loadlist.pre c656740a, main.lua.pre f586043a, UEHelpers.lua.pre 5a8f6609, enabled.txt.pre 0 B)
- pid700 evidence: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-evidence/pid700-fullfix/{MANIFEST.md,EVIDENCE.md,primary/,related-sessions/}`
- popup transcription: `.../2026-10-04-zcode-controller/incident-loading-regression/POPUP-TRANSCRIPTIONS.md`
- 18:36 build identity: `.../2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1-aside-1791247296/{artifacts-receipt.json,modbridge_supplier_composition.prx,bare-sdk/}`
- 20:03 rebuild identity: `.../2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1/{artifacts-receipt.json,modbridge_supplier_composition.prx,bare-sdk/}`
- eboot pin: `.../2026-10-04-zcode-team/worker-l3-profile/game-profile.townfall-ps5-ue56-v1.json`; `.../worker-blocks0-misread/DECISION-MEMO-BRANCH-ZERO.md`
- PATH_TEST semantics: `.../worker-evidence-pull/analyze.py`, `.../worker-evidence-pull/dryrun-pid700/results.json`
