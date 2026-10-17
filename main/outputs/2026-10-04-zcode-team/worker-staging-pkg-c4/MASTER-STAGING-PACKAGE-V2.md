# MASTER-STAGING-PACKAGE-V2.md — ONE controller execution pass (Townfall / PPSA33286)

Worker: worker-staging-pkg-c4, 2026-10-04. **V2 supersedes**
`outputs/2026-10-04-zcode-team/worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md`
(V1). Sole substantive changes from V1: file 1 is now the **delivered composition-4
PRX** (replacing the composition-3 placeholder), the **Tier B evidence expectations
are ACTIVE** (composition-4 carries the ack supplier), and the ploader entry is
confirmed **UNCHANGED** (§C). Everything else is inherited verbatim from V1.
Workspace root for every relative path:
`/WORKSPACE/.codex/worktrees/translation-core/what`. This is the single
document the controller executes for final staging. The composition-4 sha256/byte
size in §A was recomputed on disk by THIS worker on 2026-10-04 by reading the actual
file; all other rows inherit V1's on-disk recomputation.

Honest status line: this is a staging package, not a performed staging. FTP STOR write
to the console is NOT evidenced anywhere in workspace records (PREREQS G3) — Gate A0
below decides. Gameplay remains a prototype; no claim of runtime success exists. The
composition-4 ack-supplier wiring is evidenced by its checkpoint/verification records
(§A file 1 facts), NOT by a performed launch.

**Sources merged** (all read; see §H for verification method):
`worker-post-promotion-runtime/RUNTIME-STAGING-PLAN.md` (placement, working dir,
activation root, ploader entry) · `worker-settings-bundle-merge/staged-package/`
(6-file package) · `worker-emit-final-pair/emission-final/` (staged pair,
LIVE-EVIDENCE-EMITTED, parsepair OK) · `worker-real-input-pad/PS5Pad.ini` (byte-identical
to the staged-package copy, hash below) · loadlist: **worker-loadlist-final was NOT
delivered** — the corrected loadlist was computed by the V1 worker from actual file sizes
(§A file 2; the settings-bundle-merge copy lacks the two layout-pair rows and is NOT
stageable as-is) · PRX: **composition-4 IS DELIVERED** at
`$T/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx`
— verified on disk by THIS worker (sha `de372302…`, 30778567 B, §A file 1); the
composition-3 candidate `b8bfae77…` is superseded as placeholder (both hashes retained
in §C).

---

## 0. Console contact + hard gates (read before any command)

```
HTTP=http://ps5-host-not-configured.invalid:8084        # payload manager (ploader arming)
FTP=ftp://ps5-host-not-configured.invalid:2121          # anonymous FTP
```

- **Gate A0 — STOR write-probe, FIRST, exactly once:**
  ```sh
  curl -s -T /etc/hostname "$FTP/data/modbridge/intake/.writeprobe" && \
  curl -s "$FTP/data/modbridge/intake/.writeprobe" | head -c 16
  ```
  No roundtrip → **STOP**: no evidenced write route. Report; do not improvise.
- **Gate A1 — FSELF decrypt oracle check** (on the PRX readback, §B step 5): readback
  MUST start `7f 45 4c 46` (inner ELF — ftpsrv auto-decrypts FSELF; the local
  composition-4 file starts `4f 15 3d 1d`, verified below — same outer magic as the
  composition-3 candidate). A returned `4f 15 3d 1d` means the decrypt oracle is
  broken → **STOP**.
- **Ownership caveat (verbatim from the source plan):** "never mkdir inside `roots/`
  by hand" — the installer route (`register-mod`/`prepare-archive`/`install`) owns
  generation commits under `/data/modbridge/roots/`. The FTP mkdir+STOR sequence below
  is what the controller requested for this pass; if the installer service is
  reachable, prefer Route A of `worker-ue4ss-settings/STAGING-COMMANDS.md` with the
  SAME bytes/hashes as §A. Only `/data/modbridge/**` + the ploader-staging route
  (`/data/plugins`, backup + hash-verified pre-image) are ever written.
- ONE launch per staged build. Do not relaunch repeatedly without a changed
  implementation or a specific open question.

---

## A. Complete file manifest — EVERY file that goes on the console

Upload order group, exact local paths, verified sha256, byte size, console destination.
`$W` = `/WORKSPACE/.codex/worktrees/translation-core/what`;
`$T` = `$W/outputs/2026-10-04-zcode-team`;
`$R` = `/data/modbridge/roots/PPSA33286` (activation root; `mfa_config::root` exact string).

| # | order | local file (absolute) | bytes | sha256 | console destination |
|---|---|---|---|---|---|
| 1 | 2nd-to-last | `$T/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx` | 30778567 | `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f` | `/data/plugins/modbridge_supplier_composition.prx` |
| 2 | **LAST** | `$T/worker-console-staging-bundle/loadlist.txt` | 446 | `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf` | `/data/modbridge/roots/PPSA33286/loadlist.txt` |
| 3 | 1st | `$T/worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini` | 779 | `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f` | `$R/ue4ss/UE4SS-settings.ini` |
| 4 | 1st | `$T/worker-emit-final-pair/emission-final/MemberVariableLayout.ini` | 588 | `f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32` | `$R/ue4ss/MemberVariableLayout.ini` |
| 5 | 1st | `$T/worker-emit-final-pair/emission-final/VTableLayout.ini` | 186 | `9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86` | `$R/ue4ss/VTableLayout.ini` |
| 6 | 1st | `$T/worker-settings-bundle-merge/staged-package/ue4ss/PS5Pad.ini` | 2960 | `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e` | `$R/ue4ss/PS5Pad.ini` |
| 7 | 1st | `$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` | `$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` |
| 8 | 1st | `$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt` |
| 9 | 1st | `$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7` | `$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` |

Plus ONE in-place edit (not an upload of new content): `/data/plugins/ploader.ini` (§C —
with composition-4 packaged under the SAME module filename, the module line itself needs
NO change; only the per-launch re-send applies).

Manifest facts:
- Files 4+5 are the **CRITICAL PREREQUISITE staged layout pair** (LIVE-EVIDENCE-EMITTED,
  ParsePair OK, negative controls passed: stripped-BuildId refused exit 3, null identity
  refused exit 2). Identity embedded in both:
  `EngineRevision=5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`,
  `EngineMajor=5`, `EngineMinor=6`, `NonShipping=0`, `Stats=0`,
  `Target=prospero-x86_64-sysv`, `PointerSize=8`. Sections: FUObjectArray (10 members,
  `UEP_TotalSize=0xB8`) + TUObjectArray (7 members, `UEP_TotalSize=0x20`) — rung 1→2
  ONLY; code-anchor confirmation PENDING and non-blocking per its task. Without either
  file the runtime throws "Native activation requires both member and vtable layout
  files" inside `setup_unreal()` — no engine scan, no mods, no engine-ready.
- File 6 (`PS5Pad.ini`): `worker-real-input-pad/PS5Pad.ini` and the staged-package copy
  are byte-identical (same sha256, verified).
- **File 2 (loadlist) — why the staged-package copy is refused:** its 360-B loadlist
  (sha `9eef4dbba3130b8f46c41da4c30479cd5758904bc9eb8971bc7c41c85fe5170f`) lacks rows
  for `ue4ss/MemberVariableLayout.ini` and `ue4ss/VTableLayout.ini`; `mfa` preloads ONLY
  loadlisted files, so the pair would never activate and the layout gate always throws.
  The bundle copy in `worker-itemspawner-bundle` is also refused (old 221-B settings ini,
  sha `f6d06cad…`, hard-fails `validate_native_services`). The corrected 446-B loadlist
  (file 2) is TAB-separated, LF endings, header rows `modforge-loadlist 1` /
  `title PPSA33286` / `engine unreal` / `framework ue4ss-ps5` /
  `mod TownfallItemSpawner 1.0.0`, then exactly these 7 file rows (sizes = actual bytes
  of files 3–9 above):
  ```
  file	config	ue4ss/UE4SS-settings.ini	779
  file	config	ue4ss/MemberVariableLayout.ini	588
  file	config	ue4ss/VTableLayout.ini	186
  file	config	ue4ss/PS5Pad.ini	2960
  file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415
  file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0
  file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	9962
  ```
  Roles `config`/`lua_mod` are exactly mfa's default preload roles and exactly the roles
  `MakeActivatedUE4SSModSource` forwards. Total preloaded bytes: **35890** (recomputed;
  the source plan printed 35390 — arithmetic slip, corrected in V1) — far below the
  8 MiB/file and 64 MiB `NativeModSource` limits. `mfa` verifies each file against the
  loadlist-recorded size, all-or-nothing (`MFA_E_SIZE` on mismatch → game runs unmodded).
  File 1 (the PRX) is NOT loadlist-preloaded; it loads via the ploader route (§C).
- **File 1 is the composition-4 DELIVERED PRX (ack supplier present).**
  `build-receipt.json` gaps `[]` (verified by THIS worker). Local file starts
  `4f 15 3d 1d` (FSELF, read back directly) → Gate A1 readback expectation `7f 45 4c 46`
  unchanged. Provenance: ledger checkpoint **`zcode_supplier_composition_4`** (17/17
  checks per the checkpoint record; the on-disk
  `verification-evidence/lineage-verification.json` in the same candidate directory
  carries 33 checks, ALL pass, 0 fail, including
  `ledger_checkpoint_present = zcode_supplier_composition_4`,
  `composition_object_sha256_matches_checkpoint = 45dd9901…75af5` (11777955 B composed
  object), and frozen-base PRX `3b998f09…` at 30151623 B). Unlike composition-3 — whose
  symbol table measurably lacked `MakeRuntimeAckConfig`/`StartActivatedRuntimeWithAck` —
  composition-4 is the recomposition that adds the ack-supplier unit
  (`runtime_ack_supplier`), which is why §E Tier B is ACTIVE. Honesty note: the
  delivered FSELF is encrypted, so a plaintext symbol probe cannot independently
  re-confirm the ack symbols; the ack-supplier claim rests on the checkpoint/lineage
  verification records cited above, which THIS worker read and whose on-disk hashes,
  byte sizes, receipt gaps, and pass/fail tallies it re-verified.
- Deliberately NOT staged (never upload): `ue4ss/Mods/mods.txt`, `ue4ss/UE4SS_Signatures/*`,
  any `dlls/`, any `ue4ss/` file under `/data/plugins`, `UE4SS.log`,
  `runtime-status.txt`, `townfall_supplier_composition.log` (runtime-created).

---

## B. Exact FTP command sequence

Run from the workspace root. `set -e` discipline: abort the pass on ANY failed command.
Tag: `TAG=$(date +%Y%m%d-%H%M%S)`.

### Step 0 — Gate A0 write-probe (§0) + pre-image backup
```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
TAG=$(date +%Y%m%d-%H%M%S)
W=/WORKSPACE/.codex/worktrees/translation-core/what
T=$W/outputs/2026-10-04-zcode-team
mkdir -p /tmp/ppsa33286-preimages

# A0 (§0) — must roundtrip or STOP.

# Pre-image RETR of anything this pass will overwrite (do BEFORE any STOR):
curl -s "$FTP/data/plugins/ploader.ini" -o /tmp/ppsa33286-preimages/ploader.ini.pre || echo "no prior ploader.ini"
shasum -a 256 /tmp/ppsa33286-preimages/ploader.ini.pre 2>/dev/null || true
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" -o /tmp/ppsa33286-preimages/prx.pre || echo "no prior module"
shasum -a 256 /tmp/ppsa33286-preimages/prx.pre 2>/dev/null || true
curl -s "$FTP/data/modbridge/roots/PPSA33286/loadlist.txt" -o /tmp/ppsa33286-preimages/loadlist.pre || echo "no prior loadlist"
shasum -a 256 /tmp/ppsa33286-preimages/loadlist.pre 2>/dev/null || true
# Record these hashes in the controller log. If a prior PRX exists (it may be the
# composition-3 placeholder b8bfae77… from an earlier pass), back it up ON CONSOLE:
# (only after its pre-image hash is recorded)
curl -s -T /tmp/ppsa33286-preimages/prx.pre \
  "$FTP/data/plugins/modbridge_supplier_composition.$TAG.bak"
```

### Step 1 — mkdir (only the directories the manifest needs)
```sh
for d in \
  /data/modbridge/roots/PPSA33286/ue4ss \
  /data/modbridge/roots/PPSA33286/ue4ss/Mods \
  /data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner \
  /data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner/Scripts \
  /data/modbridge/roots/PPSA33286/ue4ss/Mods/shared \
  /data/modbridge/roots/PPSA33286/ue4ss/Mods/shared/UEHelpers ; do
  curl -s -Q "MKD $d" "$FTP/" || true   # 5xx on existing dir is fine; real failures show in Step 2 STOR
done
```

### Step 2 — STOR per file (activation tree first, PRX second-to-last, loadlist LAST)
```sh
R=/data/modbridge/roots/PPSA33286
# 1st wave — activation tree (files 3-9):
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini"    "$FTP$R/ue4ss/UE4SS-settings.ini"
curl -s --ftp-create-dirs -T "$T/worker-emit-final-pair/emission-final/MemberVariableLayout.ini"          "$FTP$R/ue4ss/MemberVariableLayout.ini"
curl -s --ftp-create-dirs -T "$T/worker-emit-final-pair/emission-final/VTableLayout.ini"                  "$FTP$R/ue4ss/VTableLayout.ini"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/PS5Pad.ini"            "$FTP$R/ue4ss/PS5Pad.ini"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" "$FTP$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/enabled.txt"      "$FTP$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"       "$FTP$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
# 2nd-to-last — PRX (file 1, composition-4):
curl -s -T "$T/worker-composition-4-fix/prx-candidate-composition4-1/modbridge_supplier_composition.prx" \
  "$FTP/data/plugins/modbridge_supplier_composition.prx"
# LAST — loadlist (file 2), ONLY after every other STOR succeeded:
curl -s -T "$T/worker-console-staging-bundle/loadlist.txt" "$FTP$R/loadlist.txt"
```

### Step 3 — readback verify EVERY file (RETR + sha256, must equal §A exactly)
```sh
R=/data/modbridge/roots/PPSA33286
verify () { curl -s "$FTP$1" | shasum -a 256; }   # compare output to the §A column
verify "$R/ue4ss/UE4SS-settings.ini"                                   # 9f3fa127…
verify "$R/ue4ss/MemberVariableLayout.ini"                             # f291d552…
verify "$R/ue4ss/VTableLayout.ini"                                     # 9c20f7c9…
verify "$R/ue4ss/PS5Pad.ini"                                           # 1d919fbf…
verify "$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"            # f586043a…
verify "$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"                 # e3b0c442…
verify "$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"                  # 5a8f6609…
verify "$R/loadlist.txt"                                               # 49ab7a3a…
# PRX readback: hash AND decrypt-oracle magic (Gate A1):
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" | shasum -a 256   # de372302…
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" | xxd -l 4        # MUST be: 7f45 4c46
```
ANY mismatch → halt, do not proceed to Step 4; re-STOR that file once, re-verify;
persisting mismatch → rollback §F and report.

### Step 4 — ploader.ini update (see §C for content rules)
```sh
# RETR (already in Step 0) -> edit LOCALLY (never improvise on-console) -> backup -> STOR:
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.pre "$FTP/data/plugins/ploader.ini.$TAG.bak"
# apply §C edit to /tmp/ppsa33286-preimages/ploader.ini.pre -> /tmp/ppsa33286-preimages/ploader.ini.new
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.new "$FTP/data/plugins/ploader.ini"
curl -s "$FTP/data/plugins/ploader.ini"        # eyeball final content == §C shape
```

### Step 5 — freshness reset (immediately before launch, §D step 1)
```sh
curl -s -Q "DELE /data/townfall_supplier_composition.log" "$FTP/" || true   # may not exist
curl -s "$FTP/data/plugins/UE4SS.log" -o /dev/null && \
  curl -s -Q "DELE /data/plugins/UE4SS.log" "$FTP/" || true                 # stale log from prior run
curl -s "$FTP$R/runtime-status.txt" -o /dev/null && \
  curl -s -Q "DELE $R/runtime-status.txt" "$FTP/" || true                   # stale ack from prior run
```
(These three are runtime-created; deleting stale copies makes §E freshness checks sound.)

---

## C. ploader.ini update

Console file `/data/plugins/ploader.ini`, section = title id, one line per module. The
section for this launch must read EXACTLY:

```ini
[PPSA33286]
/data/plugins/modbridge_supplier_composition.prx = 2
```

- **composition-4 does NOT change this entry:** the delivered composition-4 PRX is
  packaged under the SAME module filename (`modbridge_supplier_composition.prx`) as the
  composition-3 placeholder, so the ploader line's left side and destination are
  IDENTICAL; mode stays `2` = LATE load — the only mode shape evidenced for this line
  (worker-prx-packaging/STAGING-CHECKLIST.md §2). If the exact line already exists,
  change NOTHING. The only §C actions this pass are: ensure the line is present, the
  pre-image backup (Step 4), and the per-launch re-send (§D step 3).
- **Record of BOTH PRX hashes (per V1 contingency, now resolved):** superseded
  composition-3 placeholder `b8bfae77c8d147b54864a6fc5ac1f24355ba3fb13c79b0cd92e11bc7e36aab35`
  (30756791 B) → ACTIVE composition-4 `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f`
  (30778567 B). Same destination filename retained.
- **What NOT to change:** every other section and line; the frozen
  `modbridge_runtime_aligned_entry.prx` entry or any prior module's line; no extra
  whitespace, comments, or reordering; never improvise ini edits beyond this shape;
  never delete an existing `[PPSA33286]` header to "recreate" it.
- The pre-image backup `/data/plugins/ploader.ini.$TAG.bak` (Step 4) is the rollback
  artifact (§F).
- Per-launch rule: mode-2 payload entries must be RE-SENT for each game launch — a
  module sent for launch N is not present for launch N+1 without re-send (ploader arm +
  entry re-send, §D step 3).

---

## D. Launch procedure (controller prepares; USER launches)

1. **Freshness reset** (§B Step 5): `/data/townfall_supplier_composition.log` gone;
   `/data/plugins/UE4SS.log` and `$R/runtime-status.txt` absent or stale-deleted.
   ONE launch per staged build.
2. **Game fully CLOSED.** Verify no suspended Townfall application on the console.
3. **Arm ploader** (USER action, Payload Manager UI `ploader.elf`, or):
   `curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/ploader/ploader.elf"` — then
   RE-SEND the mode-2 entry for THIS launch (§C per-launch rule).
4. **USER launches Townfall.** Expect the ploader surface to report injection
   ~1 minute in.
5. **Wait ~2 minutes**, then collect evidence (§E) via FTP RETR
   (`ftp://ps5-host-not-configured.invalid:2121`, anonymous). Do not relaunch repeatedly without a
   changed implementation or a specific open question.
6. Record: which §E items appeared, full readbacks of the three logs, mtimes.

## E. Expected evidence checklist

Ordered by when it appears. Collection is FTP RETR against `ftp://ps5-host-not-configured.invalid:2121`.

**Tier A — module-load evidence (expected with the composition-4 PRX; necessary but
NOT sufficient this pass):**

- [ ] **`/data/townfall_supplier_composition.log` (fresh mtime)** — the module loaded
  and module_start ran: `module_start_entered`, stack-alignment events,
  `constructor_count`, constructor begin/return pairs, then supplier/facts events
  (live module facts via `sceKernelGetModuleInfo`). Stale mtime / absent = the module
  did NOT load this launch → check ploader arm, ploader.ini entry, Gate A1 magic.

**Tier B — ACTIVE this pass. The staged composition-4 PRX carries the ack-supplier
unit (§A file 1), so ALL of the following are EXPECTED on a healthy launch. Absence
of any Tier B item alongside a healthy Tier A log is now a FAILURE/WIRING state to
triage (`worker-post-promotion-runtime/FAILURE-TRIAGE.md`) — it is NOT excused by any
placeholder limitation (that excuse applied only to composition-3 and no longer
applies):**

- [ ] **`$R/runtime-status.txt` (fresh)** — ack sink, TAB-separated, append-only; a new
  session REPLACES the file. Expected content: header `modforge-runtime-status 1`, then
  `session <pid> <monotonic ms>`, then state lines in order
  `activated` → `engine-ready` → `mods-started` on success; `failed <code> <detail>`
  otherwise; `stopped` at most once at module_stop. The last `state` line is current.
- [ ] **`/data/plugins/UE4SS.log` (fresh)** — runtime log created NEXT TO THE PRX
  (requires `/data/plugins` writable; startup fails AT LOG OPEN if not). Expected
  startup lines on the native path: `No specific game configuration found, using
  default configuration file`, `Config: /data/plugins/UE4SS-settings.ini`,
  `root directory: /data/plugins`, `working directory: /data/plugins`,
  `game executable directory: …`, `game executable: … (N bytes)`,
  `mods directories: [0] ue4ss/Mods` (logical key, not a disk path), then
  `Using engine version: 5.6` engine-version/scan lines.
- [ ] **Lua mod lines** — `[TownfallItemSpawner] …` prefixed log lines in UE4SS.log
  once the mod's Lua runs (after engine init + deferred `start_native_mods` inventory,
  on engine-ready). No such lines with an otherwise healthy UE4SS.log → mod did not
  start; check `enabled.txt` presence (0 B, loadlisted) and `mods-started` ack.

**Always true (both tiers):**

- [ ] **All-or-nothing sanity:** if ANY staged file were missing/size-mismatched, mfa
  refuses activation and the game runs UNMODDED — unmodded gameplay + Tier A log is a
  valid "staged-wrong" outcome, never a crash to be retried blindly.
- [ ] **Negative evidence is evidence:** a `failed` ack line with detail; a missing
  UE4SS.log next to a healthy module log (startup died before log open); a
  layout-gate refusal detail — see `worker-post-promotion-runtime/FAILURE-TRIAGE.md`
  for the triage table. "No crash" and empty logs are never success; a supplier that
  cannot retain yields ZERO facts by design.

## F. Rollback procedure (restore pre-staging state)

Reverse of §B; run only with the game CLOSED and ploader NOT armed.

1. **Master switch (disables activation by itself):** `mfa` is all-or-nothing — with
   `loadlist.txt` removed or failing validation the game runs unmodded.
   ```sh
   curl -s -Q "DELE /data/modbridge/roots/PPSA33286/loadlist.txt" "$FTP/" || true
   ```
   If a prior loadlist pre-image existed (Step 0 hash recorded), restore it instead:
   `curl -s -T /tmp/ppsa33286-preimages/loadlist.pre "$FTP…/loadlist.txt"`.
2. **ploader.ini:** restore the Step 4 backup —
   `curl -s -T /tmp/ppsa33286-preimages/ploader.ini.pre "$FTP/data/plugins/ploader.ini"`
   (if there was NO prior ploader.ini: `DELE` it). Readback to confirm the added line
   is gone and the frozen aligned-entry module line is intact.
3. **PRX:** `curl -s -Q "DELE /data/plugins/modbridge_supplier_composition.prx" "$FTP/"`;
   if a prior module pre-image existed, restore
   `/tmp/ppsa33286-preimages/prx.pre` over the same name (hash-verify against the
   recorded pre-image hash — a prior module may be the composition-3 placeholder
   `b8bfae77…` or an even earlier aligned entry; the pre-image hash decides, never the
   filename); delete the console backup
   `/data/plugins/modbridge_supplier_composition.$TAG.bak`.
4. **Activation tree:** `DELE` files 3–9 at their §A destinations, then `RMD` the now
   empty dirs bottom-up (`…/Scripts`, `…/TownfallItemSpawner`, `…/UEHelpers`,
   `…/shared`, `…/Mods`, `ue4ss`) — RMD only succeeds when empty, which is the check.
   NOTE: if the tree was staged via the installer route instead of this FTP pass,
   rollback MUST go through the installer's uninstall/recovery journal — never manual
   `DELE` inside an installer-committed generation.
5. **Runtime-created artifacts** (safe to remove; they did not exist pre-staging):
   `DELE /data/plugins/UE4SS.log`, `DELE $R/runtime-status.txt`,
   `DELE /data/townfall_supplier_composition.log`.
6. **Verify rollback:** RETR `/data/plugins/` and `$R/` listings; confirm only
   pre-existing entries remain (match against Step 0 pre-image hashes). Final proof:
   relaunch the game WITHOUT arming ploader → must run unmodded, cleanly.

---

## H. Hash verification method

V1 (worker-console-staging-bundle, 2026-10-04): every sha256 in §A was computed via
`shasum -a 256` on the actual local file and byte size via `stat -f%z`, in a single
pass; the corrected loadlist (file 2) was written with `printf` (TAB separators, LF
endings, trailing newline) and hashed from disk (`446` B). Cross-checks PASSED against
the source plan §4: files 3–9 all match worker-post-promotion-runtime's recomputed
values exactly; `PS5Pad.ini` identical across `worker-real-input-pad/` and the staged
package; `UE4SS-settings.ini` identical across `worker-ue4ss-settings/staged/` and the
staged package.

V2 addition (worker-staging-pkg-c4, 2026-10-04): file 1 recomputed on disk by THIS
worker — `shasum -a 256` =
`de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f`, `stat` size =
`30778567`, first 4 bytes `4f 15 3d 1d` (FSELF) — matching checkpoint
`zcode_supplier_composition_4` (sha prefix `de372302`, 30778567 B, 17/17 checks).
Additionally verified in the same candidate directory: `build-receipt.json`
`gaps == []` and `verification-evidence/lineage-verification.json` with 33 checks,
ALL pass, 0 fail (including `ledger_checkpoint_present = zcode_supplier_composition_4`,
composed object sha `45dd9901…` at 11777955 B, frozen-base PRX `3b998f09…` at
30151623 B). Files 2–9 are unchanged from V1 and inherit its on-disk verification.

Re-verify ALL hashes at execution time (RETR + shasum, §B Step 3) before launch;
worker-emit-final-pair froze its pair while in flight, and the runtime plan itself
mandates re-verification at execution time.
