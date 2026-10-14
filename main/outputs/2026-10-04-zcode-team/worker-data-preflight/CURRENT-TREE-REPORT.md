# CURRENT-TREE-REPORT — worker-data-preflight, 2026-10-05

## Verdict

**PREFLIGHT CLEAN** — 0 errors, 0 warnings, exit 0.

```
data_preflight: tree=.../worker-data-preflight/current-staged-tree
               loadlist=.../worker-data-preflight/current-staged-tree/loadlist.txt
               declared files hashed: 7
VERDICT: CLEAN (0 errors, 0 warnings)
```

Machine-readable: `current-staged-tree-report.json` (same directory).

## What was verified

Every file the loadlist declares, against the runtime's own checks
re-implemented from source (pins in `CLAIM.json` / the tool header):
mfa activation (`loadlist.txt` parse, presence, exact size, duplicates,
roles, path keys, preload limits), the UE4SS mod-source layer (engine /
framework gate, key canonicality, lua-mod discovery), and the full
PS5Pad.ini parser gate (ASCII/no-control bytes, 1..16384 bytes, sections,
chords, keys, modifiers, binding rules).

## The current staged tree state (provenance)

Mirror at `current-staged-tree/`, assembled byte-for-byte from:

| file | source | bytes | sha256 |
|---|---|---|---|
| `loadlist.txt` | `launch10-evidence/loadlist-sizefixed.txt` | 447 | (copy of evidence file) |
| `ue4ss/PS5Pad.ini` | `launch10-evidence/PS5Pad-sanitized.ini` | 2958 | `6ab119b3e8ae…848efe` |
| `ue4ss/UE4SS-settings.ini` | settings-v3 `package-v3` | 1448 | `d8faa4eca2ed…67038` |
| `ue4ss/MemberVariableLayout.ini` | settings-v3 `package-v3` | 588 | `f291d552418d…3f32` |
| `ue4ss/VTableLayout.ini` | settings-v3 `package-v3` | 186 | `9c20f7c9b1a1…ad86` |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | settings-v3 `package-v3` | 21415 | `f586043a1ca5…9658` |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | settings-v3 `package-v3` | 0 | `e3b0c44298fc…b855` (empty) |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | settings-v3 `package-v3` | 9962 | `5a8f6609e555…15a7` |

Four payload hashes (settings, layouts, main.lua, enabled.txt, UEHelpers.lua)
match the hashes published in `worker-settings-v3-package/REPORT.md`, i.e.
this is exactly the settings-v3 package payload with the two launch-10
corrections (sanitized pad, size-fixed loadlist) applied — the state that
passed both data gates in launch-10 run 3.

## LOUD WARNING — pre-staged landmine still on disk

The DEFAULT tree (`worker-settings-v3-package/package-v3`) still contains the
**ORIGINAL em-dash PS5Pad.ini** (2960 bytes, sha256 `1d919fbffac4…ca1e`) and
the **2960 loadlist row** (`4d98ecd7aba1…5d5d`). Running the tool on it:

```
[ERROR] PAD_ASCII ue4ss/PS5Pad.ini: byte 0xe2 at offset 19 -- runtime throws
        'PS5Pad.ini must be ASCII text without control bytes'
VERDICT: FAILED (1 error(s), 0 warning(s))
```

Anyone who re-stages from `package-v3/` unmodified will re-create the launch-10
run1 data-gate death. Either stage from `worker-data-preflight/current-staged-tree/`
or from `worker-data-preflight/demo-fix-from-package-v3/` (byte-identical
payloads; the latter also carries `FIXES-DIFF.txt`), or re-run
`data_preflight.py --fix` on whatever tree you intend to stage and stage the
`--fix-out` result. `package-v3/` itself was NOT modified (hashes above prove it).

## Non-data caveat (out of scope, for the record)

Launch-10 run 3 cleared BOTH data gates and then failed on
`PS5Pad input unavailable: phase=6, error=-2137915388` (pad provider, not a
file gate). That is a runtime/input issue, explicitly outside data-file
preflight scope; this tool does not and cannot clear it.

## How to re-run

```
python3 outputs/2026-10-04-zcode-team/worker-data-preflight/tools/data_preflight.py \
    --tree outputs/2026-10-04-zcode-team/worker-data-preflight/current-staged-tree
```
