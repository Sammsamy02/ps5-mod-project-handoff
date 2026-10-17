# NEW-SETTINGS.md — worker-engineversion-ini, 2026-10-04

PROPOSALS-ONLY. The exact bytes exist locally under `package-v2/` in this
directory; nothing was staged to the console. Complements
worker-seed-table-derivation (its scope: the initializer unit + seed table —
untouched by this worker).

## 1. What the port actually reads (Task 1)

**Accepted keys — the complete list.** The retained parser registers exactly
four keys in section `[EngineVersionOverride]`
(`ue4ss-native-settings-defaults-1/src/SettingsManager.cpp:171-199`, identical
at the config-review citation copy
`ue4ss-unreal-native-build-1/port/ue4ss/src/SettingsManager.cpp:171-199`):

| Key | Type | Default (SM.hpp) | Consumed by |
|---|---|---|---|
| `MajorVersion` | int64 (`REGISTER_INT64_SETTING`, :172) | `-1` | `UE4SSProgram.cpp:869-884` |
| `MinorVersion` | int64 (`:173`) | `-1` | `UE4SSProgram.cpp:869-884` |
| `DebugBuild` | string (`true`/`1`, `false`/`0`, `test`) | `Detect` | `UnrealConfig.DebugBuild` (build-configuration axis) |
| `Stats` | string (`true`/`1`, `false`/`0`) | `Detect` | `UnrealConfig.Stats` (STATS axis) |

**`Patch`, `Changelist`, `VersionString` keys DO NOT EXIST** in the retained
parser. There is nothing to set them with; engine_revision/patch stay pinned
where they already are — the staged `MemberVariableLayout.ini` identity header
(`EngineRevision=5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`).
We deliberately do NOT set `DebugBuild`/`Stats`: those axes are already
resolved and staged (SHIPPING margin 60, STATS OFF margin 12 —
worker-checkpoint-reconcile/CURRENT-STATE.md; worker-axes-detector), and
overriding them would bypass the validated detector. Scope stays slot 6 only.

**Admission semantics** (`ue4ss-native-layout-binding-1/src/UE4SSProgram.cpp:869-884`,
cited range 857-879): only when BOTH `MajorVersion != -1 && MinorVersion != -1`,
and both in `[0, UINT32_MAX]` (else startup throws `...must be in range of a
uint32`), it sets
`Unreal::Version::Major/Minor = static_cast<uint32_t>(value)` and installs the
no-op `config.ScanOverrides.version_finder` lambda. Parse-failure of a key
(macro catch) silently keeps the default `-1` → override not applied.

**Measured parser bound (host-verified, see VERIFICATION.md V3):** the retained
IniParser's `get_int64` reads through a 32-bit `stoi` — a value above
`2147483647` (e.g. `4294967296`) makes the parser itself throw
`stoi: out of range`, the macro swallows it, and the override silently stays
unset. Effective acceptance window through the real stack is int32, then the
uint32 check upstream. `5` and `6` are trivially inside every bound.

**Validated 5.6 facts and what our port reads downstream:**

- Validated profile `worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json`:
  `identity.engine_major=5`, `engine_minor=6`; EngineVersion fact
  `validated-static`, origin `E-survey-stdout` line
  `engine_version status=0 major=5 minor=6`.
- Image-level identity (worker-layout-identity/IDENTITY-RESOLUTION.md §3):
  `engine_revision="5.6.1-0+++UE5"` (Patch=1 store site 0x1F37E4 in the pinned
  capture), `build_id=04bb4330a7d71dcefe1cc2c4d19769d5`. Honesty note: the
  profile JSON itself still records patch/revision as `null` (its
  `unresolved_list` predates that worker); either way the ini override carries
  ONLY Major=5/Minor=6, identical under both sources.
- Downstream of the override, in the staged initializer actually consumed by
  the composition (`worker-query-s2-arm/staging/UnrealInitializer.cpp`):
  1. `:590` `config.engine_version = !UnrealConfig.ScanOverrides.version_finder;`
     → false. This is the payoff: slot 6 leaves the required mask
     (mapped collector `cpp:23,85` — worker-required-slots-feasibility), and
  2. `:718-722` `if (ctx.config.engine_version) { Version::Major = results...; }`
     is skipped, so the ini values stand unmodified by any scan;
  3. `:856` `InitializeVersionedContainer()` → on `__PROSPERO__` the
     NativeProvidedVirtualVC `set_virtual_offsets()` calls
     `PublishStagedNativeLayoutMaps((uint32_t)Version::Major,
     (uint32_t)Version::Minor, Version::NonShipping, Version::DebugBuild)`
     (comment block :860-871) — the S2 layout publication consumes exactly our
     two values;
  4. `:876` `ArmLayoutGate({Version::Major, Version::Minor, ...})` arms the
     harness gate with the same facts;
  5. layout-binding copy `:917` logs `Using engine version: 5.6`.
- Consistency requirement: the override (5,6) EQUALS the layout-pair identity
  `5.6` already staged in `MemberVariableLayout.ini`, so the binder's
  identity-agreement refusal cannot fire. Value-neutral today (the detector
  also yields 5/6); the change is admission-level + determinism, exactly as
  proposed by worker-required-slots-feasibility.

## 2. The new staged bytes (Task 2)

`package-v2/ue4ss/UE4SS-settings.ini` — **834 bytes**,
sha256 `9570ead39ce8b9a2a2d96eddbcf04d43dd0225be0bbfd5b2cd417d618b8c6e0c`.

Strictly append-only on the staged 779-byte file
(`9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f`, proven
byte-identical to `worker-loadlist-final/final-package/ue4ss/UE4SS-settings.ini`):
the old 779 bytes are a byte-prefix of the new file (`new.startswith(old)`
asserted during composition). Exactly 55 bytes appended:

```
\n[EngineVersionOverride]\nMajorVersion=5\nMinorVersion=6\n
```

New tail hex (offset 0x2f0..0x341):

```
000002f0: 4372 6173 6844 756d 705d 0a45 6e61 626c  CrashDump].Enabl
00000300: 6544 756d 7069 6e67 3d30 0a0a 5b45 6e67  eDumping=0..[Eng
00000310: 696e 6556 6572 7369 6f6e 4f76 6572 7269  ineVersionOverri
00000320: 6465 5d0a 4d61 6a6f 7256 6572 7369 6f6e  de].MajorVersion
00000330: 3d35 0a4d 696e 6f72 5665 7273 696f 6e3d  =5.MinorVersion=
00000340: 360a                                     6.
```

Unified diff of the whole file:

```diff
22a23,26
> 
> [EngineVersionOverride]
> MajorVersion=5
> MinorVersion=6
```

### Config-review constraint re-verification (worker-config-final-review)

Every GO constraint from `worker-config-final-review/CONFIG-VERDICT.md` §2 was
re-verified against the NEW bytes by the same retained-parser oracle lineage
(VERIFICATION.md V1-V4): `bUseUObjectArrayCache=false`,
`SecondsToScanBeforeGivingUp=120`, `DefaultExecuteInGameMethod=EngineTick`
unchanged, `DefaultFNameToStringMethod=Scan` unchanged, `ConsoleEnabled=0`,
`GuiConsoleEnabled=0`, `GuiConsoleVisible=0` (required zeros),
`RenderMode=ExternalThread` (exact), `CrashDump.EnableDumping=0`,
hot-reload keys `EnableHotReloadSystem`/`EnableAutoReloadingLuaMods` still
ABSENT (absence is the only correct staging; true would throw),
no `[Overrides]` section, full-line `;` comments only / no inline `;`
(trap probes still FAIL, i.e. the oracle still discriminates), no
comments-only section (trap), no NUL, no CR, ASCII/LF only.

## 3. Loadlist delta + restage plan (Task 3)

`package-v2/loadlist.txt` — **still 446 bytes** (779→834 is a 3-digit→3-digit
change), sha256 `3afd10cebca1a89c653d70fff453bdec121f2bc6c230c4768f0833b3d0032ff5`
(was `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf`).
One line changes:

```diff
6c6
< file	config	ue4ss/UE4SS-settings.ini	779
---
> file	config	ue4ss/UE4SS-settings.ini	834
```

(whitespace = single TABs, as in the staged file; `modforge-loadlist\t1`
header and all other rows byte-identical.)

The loadlist t1 format pins SIZE ONLY (modforge_loadlist.h `mf_parse_u64` on
field 4; no sha field exists). The mfa enforcement at activation is the exact
byte-count match: `read_exact` (modforge_activation.c:163-206, called at :481)
refuses with `MFA_E_SIZE` (-8), message `file is shorter/longer than the load
list says`, on ANY mismatch. Consequence: **the settings upload and the
loadlist upload must land as one lockstep restage** — host-verified both
directions (VERIFICATION.md V5: old-bytes-vs-new-row refused rc=-8 on path
`ue4ss/UE4SS-settings.ini`). The sha equality is OUR provenance discipline
(team records), not an mfa check.

Payload sums: 35945 B payload (+7 NUL terminators = 35952 preloaded), was
35890/35897. Verified against the real activation oracle.

### Restage plan (proposal for the controller — NO console contact from this worker)

1. Freeze: confirm the currently staged root state matches the recorded
   inventory (779/`9f3fa127…` settings, 446/`49ab7a3a…` loadlist) before
   touching anything; abort the restage on any divergence (never overwrite
   divergent/unowned content).
2. Upload `package-v2/ue4ss/UE4SS-settings.ini` →
   `/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini` (834 B) and
   `package-v2/loadlist.txt` →
   `/data/modbridge/roots/PPSA33286/loadlist.txt` (446 B). Either FTP order is
   safe ONLY because activation does not run mid-transfer; do not launch the
   title until step 4 passes.
3. Readback verify BOTH files byte-for-byte (FTP download + sha256):
   expected `9570ead39ce8b9a2a2d96eddbcf04d43dd0225be0bbfd5b2cd417d618b8c6e0c`
   and `3afd10cebca1a89c653d70fff453bdec121f2bc6c230c4768f0833b3d0032ff5`, plus
   sizes 834/446. A readback mismatch = stop, re-transfer, re-verify; never
   proceed on a partial upload (that is exactly the MFA_E_SIZE failure mode).
4. Only after readback passes: launch. At activation the mfa reads the
   loadlist during module_start (the /data readable window), buffers each
   `file` row with the size pin, and refuses with MFA_E_SIZE on any drift —
   there is no silent-failure path for a half-restaged root.
5. Expected first-log line from the initializer: `Using engine version: 5.6`,
   and NO `version_finder` scan attempt for slot 6 (required mask empty of
   bit 6 per the collector wiring).

Rollback: re-upload the recorded old pair (779 B / `9f3fa127…` settings,
446 B / `49ab7a3a…` loadlist) — the old pair is internally consistent and
was the GO-reviewed state.
