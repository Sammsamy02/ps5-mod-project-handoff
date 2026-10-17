# STAGING-MANIFEST.md — console E2E staging, current payloads (bridge-linked build)

Origin: worker-e2e-console-staging, 2026-10-04. Workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`. Every sha256 below was
recomputed by this worker with `shasum -a 256` on 2026-10-04 (not copied from
receipts). Console = ps5-host-not-configured.invalid (pldmgr HTTP :8084, anonymous FTP :2121 per
`outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md`).

## Files to stage on console

| # | File (local, absolute) | sha256 (verified this worker) | Bytes | Console destination | Purpose |
|---|---|---|---|---|---|
| 1 | `outputs/2026-10-04-zcode-team/worker-zip-bridge-console-link/composition/build/console/modbridge-installer.payload.elf` | `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9` | 681632 | `/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf` (upload alias `MODBRIDGE_INST_E2E.elf` via `POST /manage:upload?filename=MODBRIDGE_INST_E2E.elf`; pldmgr creates the `<NAME>/<NAME>.elf` catalog entry, CONSOLE-LOG item 2) | The installer payload for stages 0 / 1-owner / 2a / 2b / 3 / 4-5 / 6. Bridge-linked build: includes the F-R1 archive→bundle bridge TU (`modinst_zip_bridge.ps5.o`), 116 dynamic imports, missing=[] (`composition/receipts/console-imports.stdout` has exactly 116 `U` lines, `wc -l` verified; provider check per bridge REPORT §1). sha and size match the dispatch brief and `worker-zip-bridge-console-link/REPORT.md` §1 exactly |
| 2 | `.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip` | `3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad` | (read via sha only) | `/data/modbridge/intake/movspeed.zip` | Stage 2a owner-placed archive. Role-bearing corpus proven through the FULL bridge flow in the 75/75 suite (`worker-zip-bridge-console-link/test/run/zb-pak/job.tsv` pins this exact sha; `test/bridge-e2e-summary.log` zb-pak passes). Re-verified by this worker end-to-end host-side (see REPORT §3 probe B) |
| 3 | `outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb` | `7b9888b9c647fc19ecd1631c17981ec320f69472260646b6b8c000e5cd9b70af` | 311 | `/data/modbridge/intake/mm-skin.mfb` | Stage 2b bundle flow, pak-role fixture (built by rehearsal `make-bundle`, `run_rehearsal.sh:104`; MFBUNDL1 verified by rehearsal/rehearsal-suite PASS lines) |
| 4 | `outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb` | `eefc774a589d412d827c486e6d08a4f3eaee5adb4b87f8df6f9a70bc7bd639ec` | 337 | `/data/modbridge/intake/mm-lua.mfb` | Stage 2b bundle flow, lua_mod-role fixture (`run_rehearsal.sh:106`) |

Optional alternative archive (only if #2 is unavailable): `.superpowers/native-observations/nexus-real-mods-2026-10-01/infinite-stamina-v1.zip`, sha `61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5` (verified; zb-lua corpus, also full-bridge-flow proven per `test/bridge-e2e-summary.log`).

NOT staged (kept out of this run deliberately):

| File | sha256 (verified) | Bytes | Why not staged |
|---|---|---|---|
| `outputs/2026-10-04-zcode-team/worker-ps5io-rdwr/native-link/modbridge-installer.download.elf` | `fbd355568ccc615f0eaba479a7d1a244a8876226bd0eb2fdb1e9ed640677c8e9` (matches `worker-ps5io-rdwr/NATIVE-DOWNLOAD-LINK.md` pin `fbd355568ccc615f…`) | 1197880 | Stage-1 NATIVE download stays DEFERRED per `worker-e2e-rehearsal/READINESS.md` §5 (needs ps5_io RDWR + §1A relink + owner session). It is a separate build from a different worker; staging it would invite accidental dispatch of the wrong ELF. The bridge-linked console payload (#1) has NO download transport: `download` answers `bad_request download transport not linked into this flavor` (READINESS §2) |
| UE4SS module / runtime-ack patched runtime | n/a | n/a | Deferred components (READINESS §3 items 4-5). Stage 4-5 uses the payload's own `simulate-runtime` verb; a REAL `running=1` is not claimable this run |

## Console state tree created during the run (owned by this run; reset = delete these)

- `/data/modbridge/state/` — records.txt, e2e-status.jsonl, downloads/, bundles/
- `/data/modbridge/roots/` — install root tree (`PPSA33286`, `.modforge-root`, `loadlist.txt`)
- `/data/modbridge/intake/` — the staged zip/.mfb files + quarantine dirs `*.q`

No game files are read or written by any stage in scope; no Townfall launch is
required (stage 4-5 is simulated ack, READINESS §1); no pre-existing payload is
overwritten (new alias `MODBRIDGE_INST_E2E`, CONSOLE-LOG only ever uploaded its
own two aliases).
