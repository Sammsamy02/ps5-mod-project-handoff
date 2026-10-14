# STAGING-COMMANDS.md — placing the merged activation package on the console

worker-settings-bundle-merge, 2026-10-04. Controller-executed; this worker ran none of it
(no network, no console). Conventions follow worker-e2e-console-staging/STEP-BY-STEP.md and
worker-ue4ss-settings/STAGING-COMMANDS.md: `HTTP=http://ps5-host-not-configured.invalid:8084`,
`FTP=ftp://ps5-host-not-configured.invalid:2121` (anonymous). Only `/data/modbridge/**` is ever written.
Honest channel status: FTP RETR readback is proven; **FTP STOR write is NOT evidenced
anywhere in workspace records** (PREREQS G3) — GATE 0 below must pass before any STOR.

Local package root (all six files, verified):

```
outputs/2026-10-04-zcode-team/worker-settings-bundle-merge/staged-package/
```

## 0. Local staged bytes (verified: gate-tested + hash-pinned)

| console path (under root `/data/modbridge/PPSA33286/`) | local file (under `staged-package/`) | bytes | sha256 |
|---|---|---|---|
| `ue4ss/UE4SS-settings.ini` | `ue4ss/UE4SS-settings.ini` | 779 | `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f` |
| `ue4ss/PS5Pad.ini` | `ue4ss/PS5Pad.ini` | 2960 | `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e` |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7` |
| `loadlist.txt` | `loadlist.txt` | 360 | `9eef4dbba3130b8f46c41da4c30479cd5758904bc9eb8971bc7c41c85fe5170f` |

Provenance of each file:

- `UE4SS-settings.ini` — **the CORRECTED canonical settings** from
  `worker-ue4ss-settings/staged/ue4ss/` (`ConsoleEnabled=0` + all five native gates pass,
  `settings_parse_test` → PASS "all gates + expected values OK"). The prior bundle's
  221-byte ini (`f6d06cad…`, `ConsoleEnabled=1`) would ABORT startup
  (validate_native_services, UE4SSProgram.cpp:719-746) and is intentionally NOT staged.
- `main.lua`, `enabled.txt`, `UEHelpers.lua` — byte-identical copies from
  `worker-itemspawner-bundle/bundle/`, hashes equal to their `bundle.json` pins
  (`immutable_mod_bytes` preserved).
- `PS5Pad.ini` — byte-identical copy from `worker-real-input-pad/PS5Pad.ini`.
- `loadlist.txt` — merged manifest (see §2 merge deltas).

## 1. External dependency — the staged layout pair (NOT in this package)

`ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini` are REQUIRED at runtime
(UE4SSProgram.cpp:899-904 hard-throw "Native activation requires both member and vtable
layout files"; without them the run ends inside `setup_unreal()` BEFORE any Lua), but they
come from the staged-pair worker, not this package:

- Current candidate output: `outputs/2026-10-04-zcode-team/worker-staged-pair-live/emission-live/`
  (today: MemberVariableLayout.ini 628 B `e62d441585abc3e51fa45493c3666a9987d7a941800abf05692c562e86d60a26`;
  VTableLayout.ini 226 B `91b2e30c3e36d61f8edac2e187f81364c1e11f4f80baebe7d842034f5dd7dd0d`).
- **That emission is NOT stageable today**: its receipt (`emission-receipt.json`) marks
  EngineRevision/BuildId as explicit GAP markers (not scan facts) and sets
  `staging_rule = NEVER stage or ship this pair until the identity gaps close`.
- Therefore this package's `loadlist.txt` deliberately contains NO rows for the pair. The
  package as staged will activate through `mfa_activate` but the composed runtime will stop
  in `setup_unreal()` — honestly, with no placeholders.
- **When a gap-closed pair lands**: re-measure its sizes, upload both files (same STOR/RETR
  pattern as below, into `ue4ss/`), then append exactly two TAB-separated rows to
  `loadlist.txt` and re-stage it LAST (per AWAITING-STAGED-PAIR.md steps 2-3):
  `file\tconfig\tue4ss/MemberVariableLayout.ini\t<size>`
  `file\tconfig\tue4ss/VTableLayout.ini\t<size>`

## 2. What this merge changed vs the frozen bundle manifest

Two deltas in `loadlist.txt` (everything else byte-identical to
`worker-itemspawner-bundle/bundle/loadlist.txt`, original sha256
`cbe301368f4a8108ccca16f9739a1c74a8e3242ebc875a9e40b8d21d3846746e`):

1. Settings row size `221` → `779` (corrected settings). Required: size mismatch is
   `MFA_E_SIZE` and aborts the whole activation (verified: the unmodified bundle loadlist
   against these settings fails with "file is longer than the load list says").
2. Added `file\tconfig\tue4ss/PS5Pad.ini\t2960` — the pad config is read from the activated
   mod source (NativePadInput.cpp:10-26), so it must be preloaded via a loadlist row;
   present-but-unlisted would never reach the runtime, and a listed-but-wrong hash blocks
   the whole start (any pad-config failure THROWS).

The merged package re-passed the vendored activation host test: `mfa_activate` OK, all five
files preloaded with exact-size verification, mod discovery intact (see REPORT.md for the
four superseded frozen-bundle assertions).

## 3. Commands

```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
PKG=outputs/2026-10-04-zcode-team/worker-settings-bundle-merge/staged-package
ROOT=/data/modbridge/PPSA33286

# GATE 0 — bounded STOR write-probe (PREREQS G3/P1), once, into our own intake dir:
curl -s -T /etc/hostname "$FTP/data/modbridge/intake/.writeprobe" && \
  curl -s "$FTP/data/modbridge/intake/.writeprobe" | head -c 16   # STOR+RETR roundtrip
# No roundtrip -> STOP: no evidenced write route; report; do not improvise.

# Create the directory tree (single flat command set; --ftp-create-dirs handles nesting):
curl -s --ftp-create-dirs -T "$PKG/ue4ss/UE4SS-settings.ini" \
  "$FTP$ROOT/ue4ss/UE4SS-settings.ini"
curl -s --ftp-create-dirs -T "$PKG/ue4ss/PS5Pad.ini" \
  "$FTP$ROOT/ue4ss/PS5Pad.ini"
curl -s --ftp-create-dirs -T "$PKG/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" \
  "$FTP$ROOT/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
curl -s --ftp-create-dirs -T "$PKG/ue4ss/Mods/TownfallItemSpawner/enabled.txt" \
  "$FTP$ROOT/ue4ss/Mods/TownfallItemSpawner/enabled.txt"
curl -s --ftp-create-dirs -T "$PKG/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua" \
  "$FTP$ROOT/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"

# Readback identity after EACH file (RETR is the proven direction) — run one line per file:
for f in ue4ss/UE4SS-settings.ini ue4ss/PS5Pad.ini \
         ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua \
         ue4ss/Mods/TownfallItemSpawner/enabled.txt \
         ue4ss/Mods/shared/UEHelpers/UEHelpers.lua; do
  printf '%s  %s\n' "$(curl -s "$FTP$ROOT/$f" | shasum -a 256 | cut -d' ' -f1)" "$f"
done
# Expected (in order): 9f3fa127…, 1d919fbf…, f586043a…, e3b0c442…, 5a8f6609…  (§0 table).
# ANY mismatch -> STOP: re-verify local bytes, re-stage that file only; never continue.

# LOADLIST LAST — its presence is the install-commit truth (FORMATS.md:418):
curl -s -T "$PKG/loadlist.txt" "$FTP$ROOT/loadlist.txt"
curl -s "$FTP$ROOT/loadlist.txt" | shasum -a 256   # must equal 9eef4dbb…f170f
curl -s "$FTP$ROOT/loadlist.txt"                    # eyeball: header + 5 file rows, TAB-separated
```

Never stage `loadlist.txt` before its files: activation validates every listed file at
first open (module_start, `mfa_activate`) — a listed-but-missing file aborts activation and
the game runs unmodded (all-or-nothing).
