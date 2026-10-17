# REPORT.md — worker-loadlist-final, 2026-10-04

## Task

Produce the FINAL corrected stageable loadlist for the TownfallItemSpawner
activation: ALL rows present with correct sizes (settings 779 B, the staged
layout pair, PS5Pad.ini 2960 B, and the three mod files), total cross-checked
against actual file sizes, and host-tested through the vendored
`mfa_activate` with every file present.

## Deliverable: final-package/loadlist.txt

`outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/loadlist.txt`
(TAB-separated, UTF-8):

```
modforge-loadlist	1
title	PPSA33286
engine	unreal
framework	ue4ss-ps5
mod	TownfallItemSpawner	1.0.0
file	config	ue4ss/UE4SS-settings.ini	779
file	config	ue4ss/MemberVariableLayout.ini	588
file	config	ue4ss/VTableLayout.ini	186
file	config	ue4ss/PS5Pad.ini	2960
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0
file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	9962
```

Header rows unchanged from the merge worker's loadlist; the seven `file` rows
are exactly the required final rows from
`worker-post-promotion-runtime/RUNTIME-STAGING-PLAN.md` §4 ("Required loadlist
rows"), in the plan's order. Roles `config`/`lua_mod` are mfa's default
preload roles — the pair is preloaded through the loadlist, so the runtime's
layout gate (`UE4SSProgram.cpp:901-902`, "Native activation requires both
member and vtable layout files") is satisfied via the mod source, matching
`load_unreal_offsets_from_file()` reading both keys from the activation.

## Vendored package (all 7 content files, measured sizes + sha256)

| file | bytes | sha256 |
|---|---|---|
| `ue4ss/UE4SS-settings.ini` | 779 | `9f3fa1277218…30f7f` |
| `ue4ss/MemberVariableLayout.ini` | 588 | `f291d552418d…a63f32` |
| `ue4ss/VTableLayout.ini` | 186 | `9c20f7c9b1a1…4fbad86` |
| `ue4ss/PS5Pad.ini` | 2960 | `1d919fbfac49…55eca1e` |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a1ca5…59658` |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c44298fc…52b855` |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609e555…215a7` |
| `loadlist.txt` | 446 | `49ab7a3a40ee…1e3bf` |

Provenance (all `cmp` IDENTICAL, see host-verification.txt §2): settings +
mod files from `worker-settings-bundle-merge/staged-package/` (which itself
pins to worker-ue4ss-settings/intake/distro); `PS5Pad.ini` directly from
`worker-real-input-pad/`; layout pair directly from
`worker-emit-final-pair/emission-final/` (sha256 equal to the emission
receipt's `artifact_sha256` — byte-exact, no edits).

## Total verification (task 3)

- Loadlist `file`-row size sum: **35890**
- Actual sum of the 7 packaged files: **35890** (stat on disk, per-file
  mismatch count 0)
- `mfa_preloaded_bytes` measured: **35897** = 35890 payload + 7 NUL
  terminators (one per preloaded file; vendored `modforge_activation.c`
  pass-3 appends `data[size] == 0` per file — same accounting as the merge
  worker's 31598 + 4 = 31602 note).

HONESTY NOTE — discrepancy with the plan: RUNTIME-STAGING-PLAN.md §4 states
"Totals: 35390 preloaded bytes". The true payload total is **35890**
(779+588+186+2960+21415+0+9962); the plan's figure appears to be a
transposed-digit typo. Both of my numbers are measured, not asserted; the
loadlist rows themselves match the plan exactly.

## Host test through mfa_activate (task 4)

`host-test/test_activation_final.c` compiled against the vendored
`worker-7-installer/vendored/modforge_activation.c` + headers (cc -std=c11
-O2 -Wall -Wextra, no warnings) and run over the final package with all
files present, plus three mutated scratch roots. Host-only; no network, no
console, no Lua executed, nothing written outside this worker's directory.
The prior worker-itemspawner-bundle harness was NOT reused as-is because its
expectations are pinned to the old 4-row bundle (settings 221, layout pair
expected ABSENT); the final test flips those pins deliberately.

Results — **45 PASS / 0 FAIL, exit=0** (full log:
`host-test/host-verification.txt`):

- T1 loadlist parses; header consumed; exactly one
  title/engine/framework/mod; exactly seven file rows with exact
  role/path/size; zero order rows; zero unknown rows.
- T2 `mfa_activate(root, PPSA33286)` OK; title `PPSA33286`; engine
  `unreal` (activated_mod_source.cpp:27 gate); `wants_framework
  ue4ss-ps5` (:28 gate); load-order `[TownfallItemSpawner 1.0.0]`; 7
  files, all preloaded (roles lua_mod/config); preloaded 35897 bytes; no
  data-plugin order entries.
- T3 every activated file present at listed size with `data[size]==0`;
  enabled.txt empty presence-only; layout pair 588/186 NUL-terminated.
- T4 NativeModSource enumeration oracle: exactly one Lua mod
  `TownfallItemSpawner` with `Scripts/`; `shared/` never enumerated;
  startup list `[TownfallItemSpawner]` via enabled.txt; no mods.txt.
- T5 **layout pair PRESENT** via `mfa_find` at exact sizes 588/186 — the
  gap that made the merge worker's package "not stageable" is closed.
- T6 negative controls behave: wrong title → `MFA_E_TITLE` (-4);
  truncated MemberVariableLayout.ini (587 B) → `MFA_E_SIZE` (-8); missing
  loadlist → `MFA_E_NO_LOADLIST` (-2).

## Bounds honored

Writes only under `outputs/2026-10-04-zcode-team/worker-loadlist-final/`
(CLAIM.json, final-package/, host-test/ incl. scratch negative roots,
REPORT.md). Inputs read-only. No nested subagents, no network, no console.

## What this does NOT claim

This verifies the activation package stages and consumes correctly through
the vendored mfa path on the host. It does not run the game, execute the Lua
mod, publish staged layout maps (`PublishStagedNativeLayoutMaps` remains a
runtime-side gate on real engine facts), or validate any in-game behavior.
Console staging still goes through the installer route with readback hash
verification (RUNTIME-STAGING-PLAN.md §5); never hand-edit inside `roots/`.
