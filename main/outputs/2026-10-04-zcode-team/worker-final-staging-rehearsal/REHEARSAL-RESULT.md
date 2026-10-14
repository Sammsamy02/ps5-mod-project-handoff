# REHEARSAL-RESULT.md — worker-final-staging-rehearsal, 2026-10-04

Host-side rehearsal of `worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md`
(the 9-entry manifest). Every file, every hash, every command checked; the
loadlist was re-run through the vendored `mfa_activate` against a root
assembled from the manifest's exact §A source paths. Host-only: no network,
no console, no Lua executed; writes confined to this worker's directory.

## Verdict: **READY TO EXECUTE** — 0 hash mismatches, 0 missing files, 0 FAIL in the activation rehearsal. The only outstanding unknown is Gate A0 (FTP STOR write route), which is host-unverifiable by design and correctly gated as the first live command with a hard STOP.

---

## 1. Check-by-check results

| # | Check | Result | Evidence |
|---|---|---|---|
| C1 | Manifest file 1 — PRX `worker-composition-3/prx-candidate-composition3-1/modbridge_supplier_composition.prx` | **PASS** | on-disk sha256 `b8bfae77c8d147b54864a6fc5ac1f24355ba3fb13c79b0cd92e11bc7e36aab35`, 30756791 B — matches §A exactly; first 4 bytes `4f 15 3d 1d` (FSELF) as §H states; `build-receipt.json` `"gaps": []` confirmed |
| C2 | Manifest file 2 — loadlist `worker-console-staging-bundle/loadlist.txt` | **PASS** | sha `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf`, 446 B — matches §A; TAB (0x09) separators, LF (0x0a) endings confirmed by hexdump; header + 7 file rows exactly as §A quotes |
| C3 | Manifest file 3 — `UE4SS-settings.ini` | **PASS** | sha `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f`, 779 B |
| C4 | Manifest file 4 — `MemberVariableLayout.ini` (emission-final) | **PASS** | sha `f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32`, 588 B |
| C5 | Manifest file 5 — `VTableLayout.ini` (emission-final) | **PASS** | sha `9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86`, 186 B |
| C6 | Manifest file 6 — `PS5Pad.ini` | **PASS** | sha `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e`, 2960 B; byte-identical to `worker-real-input-pad/PS5Pad.ini` (same hash, re-measured) |
| C7 | Manifest file 7 — `main.lua` | **PASS** | sha `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`, 21415 B |
| C8 | Manifest file 8 — `enabled.txt` | **PASS** | sha `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`, 0 B |
| C9 | Manifest file 9 — `UEHelpers.lua` | **PASS** | sha `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7`, 9962 B |
| C10 | mfa_activate rehearsal (vendored, manifest's file set) | **PASS** | 46 PASS / 0 FAIL, exit=0 (`mfa-rehearsal-output.txt`); details §2 |
| C11 | ploader.ini edit spec | **PASS (spec-level)** | exact line matches the pinned `worker-prx-packaging/STAGING-CHECKLIST.md` §2; rules prevent any other change; live-file check only possible at Step 0 (§3 caveats) |
| C12 | Launch procedure + evidence order | **PASS** | internally consistent; simulation in §4 |
| C13 | Referenced docs exist | **PASS** | `RUNTIME-STAGING-PLAN.md`, `FAILURE-TRIAGE.md`, `worker-ue4ss-settings/STAGING-COMMANDS.md` (Route A), `STAGING-CHECKLIST.md` all present |
| C14 | §B Step 2/3 command ↔ §A path/hash consistency | **PASS** | all 9 STOR source paths and all 9 expected hash comments in Step 3 match the §A table verbatim |

Loadlist totals recomputed: 779+588+186+2960+21415+0+9962 = **35890** payload
(+7 NUL terminators = 35897 as mfa counts) — confirms the manifest's corrected
figure over the source plan's 35390 typo. Far below the 8 MiB/file and 64 MiB
`NativeModSource` limits.

## 2. mfa_activate rehearsal (T1–T6), manifest's exact file set

Root assembled strictly from §A source paths (`rehearsal-root/`): loadlist from
`worker-console-staging-bundle`, files 3/6/7/8/9 from
`worker-settings-bundle-merge/staged-package/ue4ss`, files 4/5 from
`worker-emit-final-pair/emission-final`. Harness:
`worker-loadlist-final/host-test/test_activation_final.c` compiled 0 warnings
(`cc -std=c11 -O2 -Wall -Wextra`) against the vendored
`worker-7-installer/vendored/modforge_activation.c` + headers. Binary:
`test_activation_final` (this directory). Log: `mfa-rehearsal-output.txt`.

- **T1** loadlist parses; header `modforge-loadlist 1` consumed; exactly one
  title/engine/framework/mod (`PPSA33286` / `unreal` / `ue4ss-ps5` /
  `TownfallItemSpawner 1.0.0`); exactly 7 file rows with exact role/path/size;
  zero order rows; zero unknown rows. **PASS**
- **T2** `mfa_activate(root, PPSA33286)` = MFA_OK; engine `unreal`,
  `wants_framework ue4ss-ps5`; 7 files all preloaded (roles `config`/`lua_mod`);
  preloaded bytes == 35897; no data-plugin order entries. **PASS**
- **T3** every file present at its listed size with `data[size]==0`;
  `enabled.txt` empty presence-only; layout pair NUL-terminated at 588/186. **PASS**
- **T4** NativeModSource enumeration oracle: exactly one Lua mod
  `TownfallItemSpawner` (`Scripts/`), `shared/` never enumerated as a mod;
  startup list `[TownfallItemSpawner]` via `enabled.txt`; no `mods.txt`. **PASS**
- **T5** required layout keys PRESENT via `mfa_find` at 588/186. **PASS**
- **T6** negative controls: wrong title → `MFA_E_TITLE` (rc −4); truncated
  layout file → `MFA_E_SIZE` (rc −8); missing loadlist → `MFA_E_NO_LOADLIST`
  (rc −2). **PASS**

Cross-checks in the same run:
- `diff -r rehearsal-root worker-loadlist-final/final-package` → **IDENTICAL**
  (worker-loadlist-final's delivery, which landed after the manifest was
  assembled, is byte-for-byte the manifest's set — no divergence).
- `worker-loadlist-final/final-package/loadlist.txt` sha ==
  `worker-console-staging-bundle/loadlist.txt` sha (`49ab7a3a…`).
- `parsepair-result.json`: `ok:true`; identity exactly as the manifest claims
  (`5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`,
  `prospero-x86_64-sysv`, PointerSize 8, NonShipping 0, Stats 0, 5.6); 
  FUObjectArray 10 members with `UEP_TotalSize` at 184 (0xB8); TUObjectArray
  7 members with `UEP_TotalSize` at 32 (0x20); both staged INIs carry the
  `[NativeLayout]` identity header. Emission receipt label
  `LIVE-EVIDENCE-EMITTED`; scope rung 1→2 only; code-anchor PENDING
  (declared non-blocking).

## 3. ploader.ini edit — verification and caveats

Spec verified as correct and minimal:
- Exact line to ADD (if absent), directly under the existing `[PPSA33286]`
  header: `/data/plugins/modbridge_supplier_composition.prx = 2` — byte-equal
  to the pinned `worker-prx-packaging/STAGING-CHECKLIST.md` §2 shape
  (`/data/plugins/<module>.prx = 2`, mode 2 = LATE).
- §C's prohibitions (nothing else changes; frozen aligned-entry line intact;
  no whitespace/comment/reorder drift; never recreate the header) match the
  procedure: RETR pre-image (Step 0) → edit LOCALLY → on-console `.bak` →
  STOR → readback eyeball. Rollback artifact defined.
- If the line already exists: change nothing — correct no-op path.

Caveat (not a defect): the actual console bytes of `/data/plugins/ploader.ini`
are not recorded anywhere host-side; exactness against the live file is only
provable at Step 0 (Gate A0) when the pre-image is RETR'd. The manifest's
workflow already enforces readback + backup; nothing to add.

## 4. Launch simulation — expected evidence order

1. **T+0 (before anything):** Gate A0 write-probe roundtrips (exactly once).
   No roundtrip → STOP; nothing else in this simulation happens.
2. **Arm + entry re-send:** USER arms `ploader.elf` (Payload Manager UI or
   `$HTTP/loadpayload:`), the mode-2 entry is re-sent for THIS launch
   (per-launch rule §C). Game fully CLOSED before this.
3. **USER launches Townfall.** ~1 min: ploader surface reports injection.
4. **First on-disk evidence — Tier A (placeholder PRX):**
   `/data/townfall_supplier_composition.log` appears with fresh mtime:
   `module_start_entered`, stack-alignment events, `constructor_count`,
   constructor begin/return pairs, then supplier/facts events
   (`sceKernelGetModuleInfo`). This is the ONLY file the composition-3
   placeholder as linked can produce (its symbol table has no
   `MakeRuntimeAckConfig`/`StartActivatedRuntimeWithAck`).
5. **Tier B — NOT expected this launch** (absence with healthy Tier A is the
   documented WIRING state, not a failure). If the §C composition-4
   contingency were ever taken instead, in order: `$R/runtime-status.txt`
   (fresh; `modforge-runtime-status 1` → `session <pid> <ms>` → `activated` →
   `engine-ready` → `mods-started`; `failed <code> <detail>` on any failure) →
   `/data/plugins/UE4SS.log` (fresh; default-config line → `Config:
   /data/plugins/UE4SS-settings.ini` → root/working dir `/data/plugins` →
   game-executable lines → `mods directories: [0] ue4ss/Mods` → engine-5.6
   scan lines) → `[TownfallItemSpawner]` Lua lines only after engine-ready.
6. **Failure shapes to expect, never blind-retry:** any missing/size-mismatched
   staged file ⇒ mfa all-or-nothing refusal ⇒ game runs UNMODDED (Tier A log
   may still appear — that is a "staged-wrong" outcome, not a crash);
   healthy Tier A + no UE4SS.log ⇒ startup died before log open; layout-gate
   refusal detail ⇒ triage via `worker-post-promotion-runtime/FAILURE-TRIAGE.md`.
7. **Collection (~2 min after launch):** FTP RETR of the three logs; record
   which items appeared, full readbacks, mtimes. ONE launch per staged build.

## 5. Gaps / findings (none blocking)

- **G1 (the real open risk, host-unverifiable):** no workspace record evidences
  an FTP STOR write to the console (PREREQS G3). The manifest gates this
  correctly as Gate A0, first, exactly once, hard STOP on failure. Nothing
  further can be rehearsed about it from the host.
- **G2 (stale manifest sentence, zero divergence):** the manifest header states
  "worker-loadlist-final was NOT delivered". That delivery HAS since landed and
  is byte-identical to the manifest's §A set (diff -r → IDENTICAL). The
  manifest's file choices stand; annotate the sentence at execution time if
  desired (existing files left untouched by this worker).
- **G3:** worker-composition-4 still has no PRX (CLAIM.json, compose-log.txt,
  REPORT.md only) ⇒ §C contingency NOT triggered; file 1 stays the
  placeholder; Tier B evidence is not expected from this launch.
- **G4:** layout-pair code-anchor confirmation remains PENDING (declared
  non-blocking by its task); pair itself parse-verified (§2).
- **G5 (cosmetic):** §A's quoted 7-row block carries 2-space markdown
  indentation; the actual loadlist rows have none. mfa parse confirms the real
  file is clean (zero unknown rows); no correction needed to the file.

## 6. Ready-to-execute command list (controller)

Execute exactly in this order; `set -e` discipline; abort the pass on ANY
failed command. All commands verified against §A (paths, hashes, order).

```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
TAG=$(date +%Y%m%d-%H%M%S)
W=/WORKSPACE/.codex/worktrees/translation-core/what
T=$W/outputs/2026-10-04-zcode-team
R=/data/modbridge/roots/PPSA33286
mkdir -p /tmp/ppsa33286-preimages

# 0. Gate A0 — STOR write-probe, FIRST, exactly once (STOP if no roundtrip)
curl -s -T /etc/hostname "$FTP/data/modbridge/intake/.writeprobe" && \
  curl -s "$FTP/data/modbridge/intake/.writeprobe" | head -c 16

# 0b. Pre-image RETR + hash-log of everything this pass overwrites
curl -s "$FTP/data/plugins/ploader.ini" -o /tmp/ppsa33286-preimages/ploader.ini.pre || echo "no prior ploader.ini"
shasum -a 256 /tmp/ppsa33286-preimages/ploader.ini.pre 2>/dev/null || true
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" -o /tmp/ppsa33286-preimages/prx.pre || echo "no prior module"
shasum -a 256 /tmp/ppsa33286-preimages/prx.pre 2>/dev/null || true
curl -s "$FTP$R/loadlist.txt" -o /tmp/ppsa33286-preimages/loadlist.pre || echo "no prior loadlist"
shasum -a 256 /tmp/ppsa33286-preimages/loadlist.pre 2>/dev/null || true
# on-console PRX backup only after its pre-image hash is recorded:
curl -s -T /tmp/ppsa33286-preimages/prx.pre "$FTP/data/plugins/modbridge_supplier_composition.$TAG.bak"

# 1. mkdir (5xx on existing is fine; real failures surface at STOR)
for d in $R/ue4ss $R/ue4ss/Mods $R/ue4ss/Mods/TownfallItemSpawner \
         $R/ue4ss/Mods/TownfallItemSpawner/Scripts $R/ue4ss/Mods/shared \
         $R/ue4ss/Mods/shared/UEHelpers ; do
  curl -s -Q "MKD $d" "$FTP/" || true
done

# 2. STOR — activation tree first (files 3-9), PRX 2nd-to-last, loadlist LAST
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini"   "$FTP$R/ue4ss/UE4SS-settings.ini"
curl -s --ftp-create-dirs -T "$T/worker-emit-final-pair/emission-final/MemberVariableLayout.ini"         "$FTP$R/ue4ss/MemberVariableLayout.ini"
curl -s --ftp-create-dirs -T "$T/worker-emit-final-pair/emission-final/VTableLayout.ini"                 "$FTP$R/ue4ss/VTableLayout.ini"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/PS5Pad.ini"           "$FTP$R/ue4ss/PS5Pad.ini"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" "$FTP$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/TownfallItemSpawner/enabled.txt"      "$FTP$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"
curl -s --ftp-create-dirs -T "$T/worker-settings-bundle-merge/staged-package/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"       "$FTP$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
curl -s -T "$T/worker-composition-3/prx-candidate-composition3-1/modbridge_supplier_composition.prx" "$FTP/data/plugins/modbridge_supplier_composition.prx"
curl -s -T "$T/worker-console-staging-bundle/loadlist.txt" "$FTP$R/loadlist.txt"

# 3. Readback-verify EVERY file (sha256 must equal C1-C9; PRX readback Gate A1 magic 7f45 4c46)
verify () { curl -s "$FTP$1" | shasum -a 256; }
verify "$R/ue4ss/UE4SS-settings.ini"                        # 9f3fa127…
verify "$R/ue4ss/MemberVariableLayout.ini"                  # f291d552…
verify "$R/ue4ss/VTableLayout.ini"                          # 9c20f7c9…
verify "$R/ue4ss/PS5Pad.ini"                                # 1d919fbf…
verify "$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" # f586043a…
verify "$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"      # e3b0c442…
verify "$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"       # 5a8f6609…
verify "$R/loadlist.txt"                                    # 49ab7a3a…
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" | shasum -a 256  # b8bfae77…
curl -s "$FTP/data/plugins/modbridge_supplier_composition.prx" | xxd -l 4       # MUST be 7f45 4c46
# ANY mismatch: halt, re-STOR that file once, re-verify; persisting → rollback §F + report.

# 4. ploader.ini: backup → local §C edit of the pre-image → STOR → readback
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.pre "$FTP/data/plugins/ploader.ini.$TAG.bak"
#    apply §C edit locally -> /tmp/ppsa33286-preimages/ploader.ini.new
curl -s -T /tmp/ppsa33286-preimages/ploader.ini.new "$FTP/data/plugins/ploader.ini"
curl -s "$FTP/data/plugins/ploader.ini"   # eyeball == §C shape, nothing else changed

# 5. Freshness reset (immediately before launch)
curl -s -Q "DELE /data/townfall_supplier_composition.log" "$FTP/" || true
curl -s "$FTP/data/plugins/UE4SS.log" -o /dev/null && curl -s -Q "DELE /data/plugins/UE4SS.log" "$FTP/" || true
curl -s "$FTP$R/runtime-status.txt" -o /dev/null && curl -s -Q "DELE $R/runtime-status.txt" "$FTP/" || true

# 6. Launch (§D): game CLOSED → arm ploader → RE-SEND mode-2 entry → USER launches.
#    ~2 min later collect §E evidence via FTP RETR; ONE launch per staged build.
```

No corrections to the manifest's commands were needed; the list above is the
manifest's §B/§C/§D verbatim, re-verified file-by-file by this rehearsal.
