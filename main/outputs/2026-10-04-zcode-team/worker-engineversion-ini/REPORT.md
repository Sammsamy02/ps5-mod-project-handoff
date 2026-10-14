# REPORT.md — worker-engineversion-ini, 2026-10-04

Task: the settings-ini side of the slot-6 (engine_version) zero-scan
resolution — consumer reading, new staged settings bytes, loadlist delta +
restage plan, host verification. Complements worker-seed-table-derivation
(its scope untouched). PROPOSALS-ONLY; no console contact; nothing under
`/data/modbridge` modified (path is not mounted on this Mac — staging remains
a controller step; exact local bytes are delivered ready-to-upload).

## Verdict

**The settings route is real, exact, and fully verified at the byte level.**
`[EngineVersionOverride] MajorVersion=5 MinorVersion=6` is sufficient to make
required slot 6 resolve with zero scans, and the appended 55 bytes break none
of the GO constraints from worker-config-final-review. The complete restage
delta is two files: settings 779→834 B, loadlist one row 779→834 (file stays
446 B).

## Findings

1. **Accepted keys are exactly `MajorVersion`/`MinorVersion`** (int64, section
   `[EngineVersionOverride]`; SettingsManager.cpp:172-173 in both the
   settings-defaults copy and the config-review citation copy). Optional
   `DebugBuild`/`Stats` strings exist in the same section; we deliberately do
   not set them (axes already resolved and staged; scope stays slot 6).
   `Patch`/`Changelist`/`VersionString` keys do not exist in the retained
   parser — engine_revision/patch stay pinned in the staged
   `MemberVariableLayout.ini` identity (`5.6.1-0+++UE5`,
   `04bb4330a7d71dcefe1cc2c4d19769d5`).
2. **Consumer chain confirmed in the consumed staged initializer**
   (`worker-query-s2-arm/staging/UnrealInitializer.cpp`): ini override sets
   `Unreal::Version::Major/Minor` + no-op `version_finder` (UE4SSProgram.cpp
   :869-884) → `config.engine_version=false` (:590) → slot 6 leaves the
   required mask and the scan-assignment block (:718-722) is skipped →
   `PublishStagedNativeLayoutMaps(Major, Minor, NonShipping, DebugBuild)` and
   `ArmLayoutGate` (:856/:860-876) consume exactly those values. Override
   (5,6) equals the staged layout-pair identity `5.6`, so the binder's
   identity-agreement refusal cannot fire. Value-neutral today; the effect is
   admission-level + determinism.
3. **Honesty corrections to the task brief** (recorded, not silently fixed):
   - "Profile pins engine 5.6.1" — the profile JSON actually records
     patch/revision as null/unknown (`unresolved_list`); 5.6.1 comes from the
     later worker-layout-identity image evidence. Immaterial to the ini:
     Major=5/Minor=6 under both sources.
   - "Loadlist pins size+sha" — the t1 loadlist pins SIZE ONLY; mfa
     enforcement is the exact byte-count (`read_exact` → `MFA_E_SIZE`). Sha
     equality is our own provenance discipline.
   - Parser reality (measured): `get_int64` is backed by 32-bit `stoi`; an
     out-of-int32 value throws inside the parser and the override silently
     stays unset (probe V3). Effective window is int32 ∩ uint32; 5/6 trivial.
4. **New bytes** (`package-v2/ue4ss/UE4SS-settings.ini`): 834 B,
   sha256 `9570ead39ce8b9a2a2d96eddbcf04d43dd0225be0bbfd5b2cd417d618b8c6e0c`.
   Strict append-only on the pinned 779 B file (`9f3fa127…`): 55 bytes —
   `\n[EngineVersionOverride]\nMajorVersion=5\nMinorVersion=6\n`. All
   config-review GO constraints re-verified on the new bytes by the retained
   IniParser oracle (V1: PASS; trap probes still FAIL).
5. **Loadlist delta** (`package-v2/loadlist.txt`): still 446 B, sha256
   `3afd10cebca1a89c653d70fff453bdec121f2bc6c230c4768f0833b3d0032ff5`; one row
   changes, `…UE4SS-settings.ini\t779` → `\t834`. Payload 35945 B
   (+7 NUL = 35952 preloaded), was 35890/35897.
6. **Restage plan** (NEW-SETTINGS.md §3): freeze-check the staged root against
   the recorded inventory → upload BOTH files → FTP readback + sha256 verify
   → only then launch. The two uploads are lockstep: mfa refuses any
   asymmetry with `MFA_E_SIZE` on `ue4ss/UE4SS-settings.ini` (host-proven in
   the old-bytes-vs-new-row direction, V6; the symmetric branch is the same
   check). Rollback = re-upload the recorded old pair.

## Verification summary (VERIFICATION.md)

- V1 retained-parser oracle on new settings: **PASS** (all gates + expected
  values), negative probes still FAIL.
- V2 derived oracle (same vendored parser + `MajorVersion==5`,
  `MinorVersion==6`, UP.cpp:869-884 admission replay): **PASS**; 0 warnings
  from our translation units.
- V3 admission edge probes: out-of-range → parser throw; unset → all three EV
  expectations FAIL (discrimination proven).
- V5 real mfa activation over package-v2 with the new row: **46 PASS /
  0 FAIL** (incl. `settings.ini size==834, NUL-terminated`, preloaded 35952,
  negative controls MFA_E_TITLE/-4, MFA_E_SIZE/-8, MFA_E_NO_LOADLIST/-2).
- V6 lockstep negative: old 779 B settings vs new 834 row →
  `rc=-8 MFA_E_SIZE "file is shorter than the load list says"`.

## Scope boundaries honored

No console contact; no nested subagents; no editor/engine/source writes; no
files outside `outputs/2026-10-04-zcode-team/worker-engineversion-ini/` were
created or modified; worker-seed-table-derivation files untouched. The staged
`/data` root was not touched from this Mac. Everything delivered is
controller-ready proposal bytes + receipts.

## Deliverables

- `CLAIM.json` (written first, sha fields completed after composition)
- `NEW-SETTINGS.md` (bytes, deltas, restage plan)
- `VERIFICATION.md` + `host-test/` (oracle sources/binaries, logs, probes,
  mismatch root) + `package-v2/` (the exact new bytes for both files)
- `REPORT.md` (this file)
