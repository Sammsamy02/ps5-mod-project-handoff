# STAGING-MANIFEST-D1REL.md — console E2E staging, D1-fixed merged-chain payload

Origin: worker-e2e-console-dry, 2026-10-04. This manifest supersedes
`worker-e2e-console-staging/STAGING-MANIFEST.md` row #1 (payload only; the
supersession receipt is `worker-d1-chain-relink/STAGING-MANIFEST-REPIN.md`,
F-R3). Rows #2-#4 (corpus) and the not-staged table are carried over UNCHANGED;
every sha below was recomputed by THIS worker with `shasum -a 256` on
2026-10-04. Console = ps5-host-not-configured.invalid (pldmgr HTTP :8084, anonymous FTP :2121 per
`outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md`).

## Files to stage on console

| # | File (local, workspace-relative) | sha256 (recomputed this worker) | Bytes | Console destination | Purpose |
|---|---|---|---|---|---|
| 1 | `outputs/2026-10-04-zcode-team/worker-d1-chain-relink/composition/build/console/modbridge-installer.payload.elf` | `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436` | 681528 | `/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf` (upload alias `MODBRIDGE_INST_E2E.elf` via `POST /manage:upload?filename=MODBRIDGE_INST_E2E.elf`; destination/alias UNCHANGED from the staged manifest) | The installer payload for stages 0 / 1-owner / 2a / 2b / 3 / 4-5 / 6. Merged 51-tuple chain (35 base + 6 e2e + 4 zb + 4 uf + 2 r2), D1 fixed BEFORE F-R2 fixed. 116 dynamic imports, missing=[] (`worker-d1-chain-relink/composition/receipts/console-import-providers.json`); import NAME set and per-name providers byte-identical to the previously staged payload. 6 `mi_bridge_*` symbols in the link map. Suites green on this exact chain: bridge 75/75, F-R2 87/87, fault 186/186 |
| 2 | `.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip` | `3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad` | (zip; size not pinned — sha only, per manifest #2 convention) | `/data/modbridge/intake/movspeed.zip` | Stage 1-owner / 2a owner-placed archive. Role-bearing corpus proven through the full bridge flow (zb-pak pins this exact sha; merged-chain zb-pak transcript PASS) |
| 3 | `outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb` | `7b9888b9c647fc19ecd1631c17981ec320f69472260646b6b8c000e5cd9b70af` | 311 | `/data/modbridge/intake/mm-skin.mfb` | Stage 2b bundle flow, pak-role fixture (unchanged) |
| 4 | `outputs/2026-10-04-zcode-team/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb` | `eefc774a589d412d827c486e6d08a4f3eaee5adb4b87f8df6f9a70bc7bd639ec` | 337 | `/data/modbridge/intake/mm-lua.mfb` | Stage 2b bundle flow, lua_mod-role fixture (unchanged) |

Note: row #2's sha was recomputed twice this session and equals the original
manifest pin byte-for-byte
(`3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad`); the
controller MUST still recompute before upload (gate G5).

Optional alternative archive (only if #2 unavailable): `.superpowers/native-observations/nexus-real-mods-2026-10-01/infinite-stamina-v1.zip`, sha `61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5` (recomputed this session; zb-lua corpus, full-bridge-flow proven; merged-chain zb-lua transcript PASS).

## What changed vs `worker-e2e-console-staging/STAGING-MANIFEST.md`

- Row #1 ONLY: new local file (worker-d1-chain-relink build output), new sha
  `e3489297…`, new size 681528 (-104 B vs the old `cfa96ab5…` / 681632).
  The size delta is recorded and explained in
  `worker-d1-chain-relink/REPORT.md` §5 note 1 (net text shrink from the
  uf-order-commit pair). Staging acceptance for this payload is size law +
  identity receipts (below), never full-hash equality with a previous payload.
- Corpus rows #2-#4 and the optional row: UNCHANGED (same files, same pins,
  recomputed and confirmed byte-identical this session). Same console
  destinations. Same intake filenames.

## F-R3 / R-FTP-01 acceptance law for payload row #1 (applies verbatim)

Readback identity = staged-fself size law + ELF magic + import-set identity,
with the sha re-pinned DELIBERATELY by `worker-d1-chain-relink/STAGING-MANIFEST-REPIN.md`:

- readback bytes == 681528, ELF magic `\x7fELF` at offset 0;
- phdr-covered prefix = 606256 bytes (3 PT_LOAD segments) — the freshness
  oracle stays the runtime aligned-log mtime;
- import receipt `composition/receipts/console-import-providers.json`:
  116 imports, missing=[], provider sets identical to the previously staged
  payload's receipt;
- link map `composition/build/console/modbridge-installer.payload.map`
  contains the 6 `mi_bridge_*` symbols;
- per-tuple patch evidence `composition/receipts/patches.json` (51 patches,
  occurrences == expected; sole non-1 is the pre-existing base
  `empty-root-open`==2, also ==2 in the delivered base chain).

## Not staged (carried over, unchanged)

| File | sha256 | Bytes | Why not staged |
|---|---|---|---|
| `worker-ps5io-rdwr/native-link/modbridge-installer.download.elf` | `fbd355568ccc615f0eaba479a7d1a244a8876226bd0eb2fdb1e9ed640677c8e9` | 1197880 | Stage-1 NATIVE download stays DEFERRED (READINESS §5: ps5_io RDWR + §1A relink + owner session). Different build, different worker; staging invites wrong-ELF dispatch. This payload's `download` answers `bad_request download transport not linked into this flavor` |
| UE4SS module / runtime-ack patched runtime | n/a | n/a | Deferred (READINESS §3 items 4-5). Stage 4-5 uses `simulate-runtime`; a REAL `running=1` is not claimable this run |
| Any e2e-cmd.json payload revision | n/a | n/a | The G1-fallback config-entry installer does not exist yet — see `E2E-CMD-SCHEMA.md`. If G1 resolves NO-GO for argv, the run waits for that new revision (new sha re-pin), never improvises |

## Console state tree created during the run (owned by this run; reset = delete these)

- `/data/modbridge/state/` — records.txt, e2e-status.jsonl, downloads/, bundles/, journal/
- `/data/modbridge/roots/` — install root tree (`PPSA33286`, `.modforge-root`, `loadlist.txt`)
- `/data/modbridge/intake/` — the staged zip/.mfb files + quarantine dirs `*.q` + `e2e-cmd.json` / `e2e-cmd.json.consumed` (config-entry dispatch files, one per dispatch)

No game files are read or written by any stage in scope; no Townfall launch is
required (stage 4-5 simulated ack); no pre-existing payload is overwritten
(only the new `MODBRIDGE_INST_E2E` alias).
