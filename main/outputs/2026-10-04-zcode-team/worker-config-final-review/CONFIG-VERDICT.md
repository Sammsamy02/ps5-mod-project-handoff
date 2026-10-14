# CONFIG-VERDICT.md — worker-config-final-review, 2026-10-04

Final config review before first UE4SS runtime start in Silent Hill Townfall
(PS5, UE 5.6.1 SHIPPING, 71959 GUA objects, engine `5.6.1-0+++UE5`, build_id
`04bb4330a7d71dcefe1cc2c4d19769d5`). Review-only; no staged-root or source
edits made by this worker.

**Overall: GO for all 8 staged files — no must-fix edits.** One operational
hard dependency and one user-expectation caveat are documented below (pad
provider availability; USB keyboard is not an input path).

Runtime source citations below use:
- `SM.cpp`  = `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/port/ue4ss/src/SettingsManager.cpp`
- `SM.hpp`  = `.../port/ue4ss/include/SettingsManager.hpp`
- `UP.cpp`  = `.../port/ue4ss/src/UE4SSProgram.cpp`
- `NMS.cpp` = `.../port/ue4ss/src/Platform/NativeModSource.cpp`
- `NLB.cpp` = `.../port/ue4ss/src/NativeLayoutBinding.cpp`
- `SCHEMA`  = `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc`
- `PAD`     = `.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp`
- `NPI.cpp` = `.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/NativePadInput.cpp`
- `KD.cpp`  = `.../port/first/src/Input/KeyDef.cpp`
- `IH.cpp`  = `.../port/first/src/Input/Handler.cpp`

---

## 1. Recovered staged bytes (Task 1)

Authoritative local copy: `outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/`
(`rehearsal-root` in `worker-final-staging-rehearsal/` verified `diff -r` IDENTICAL to it).

| File (root-relative)             | Bytes  | sha256 (full)                                                     | Cross-confirmed identical in |
|----------------------------------|--------|-------------------------------------------------------------------|------------------------------|
| `ue4ss/UE4SS-settings.ini`       | 779    | `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f` | worker-settings-bundle-merge/staged-package, worker-ue4ss-settings/staged, worker-final-staging-rehearsal/rehearsal-root |
| `ue4ss/MemberVariableLayout.ini` | 588    | `f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32` | worker-emit-final-pair/emission-final |
| `ue4ss/VTableLayout.ini`         | 186    | `9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86` | worker-emit-final-pair/emission-final |
| `ue4ss/PS5Pad.ini`               | 2960   | `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e` | rehearsal-root |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` | worker-itemspawner-bundle/bundle, worker-settings-bundle-merge/staged-package, worker-ue4ss-settings/staged |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty-file hash) | — |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7` | rehearsal-root |
| `loadlist.txt` (root)            | 446    | `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf` | rehearsal-root |

Every size matches the console-verified root inventory and every `file` row of
`loadlist.txt` exactly. `loadlist.txt` begins `modforge-loadlist\t1` and its
7 file rows sum to 35890 B (independently recomputed; matches
`worker-final-staging-rehearsal/REPORT.md` line 25; the harness log's "35897"
figure is a counting note superseded by that correction).
The older `worker-itemspawner-bundle/bundle` set (221 B settings, 326 B
loadlist) is OBSOLETE — do not stage it.

## 2. UE4SS-settings.ini — GO (Task 2)

### What our composed build actually reads
On `__PROSPERO__` the settings are consumed ONLY from the logical activation
key `ue4ss/UE4SS-settings.ini` through the mod source snapshot
(SM.cpp:58-68: `receiver.native_mod_source()->read("ue4ss/UE4SS-settings.ini")`,
NUL-byte refusal at SM.cpp:64-65); call site UP.cpp:224-236 — a parse failure
routes to `create_emergency_console_for_early_error` (PROSPERO: message stored,
no console, UP.cpp:837-847) and startup returns. The INI's header claims are
accurate.

Field-by-field (every key we set IS parsed; nothing set is ignored):

| Key | Parsed at | Semantic in our build |
|---|---|---|
| `[General] bUseUObjectArrayCache=false` | SM.cpp:146 | feeds `config.bUseUObjectArrayCache` (UP.cpp:979). Code default is `true` (SM.hpp:40); false is the deliberate retained value for the natively staged GUA layout. |
| `[General] SecondsToScanBeforeGivingUp=120` | SM.cpp:136-145 | 120 is also the code default (SM.hpp:39; legacy 30 coerced to 120). Harmless, explicit. |
| `[General] DefaultExecuteInGameThreadMethod=EngineTick` | SM.cpp:150-159 | selects `GameThreadExecutionMethod::EngineTick`. |
| `[General] DefaultFNameToStringMethod=Scan` | SM.cpp:160-169 | selects `FNameToStringMethod::Scan` (Conv_NameToString fallback remains available in Lua). |
| `[Debug] ConsoleEnabled=0` | SM.cpp:223 → `Debug.SimpleConsoleEnabled` | **required 0**: PROSPERO `create_simple_console` throws "Native simple console service is unavailable" if 1 (UP.cpp:888-890); `validate_native_services` throws too (UP.cpp:740-741). |
| `[Debug] GuiConsoleEnabled=0` | SM.cpp:224 | **required 0**: gate at UP.cpp:742-744 throws "Native debugging GUI service is unavailable". |
| `[Debug] GuiConsoleVisible=0` | SM.cpp:225 | **required 0**: same gate (UP.cpp:742-744). |
| `[Debug] RenderMode=ExternalThread` | SM.cpp:238-243 | **required exactly ExternalThread**: any other value trips the same gate (UP.cpp:742-744). |
| `[CrashDump] EnableDumping=0` | SM.cpp:268 | **required 0**: UP.cpp:738-739 throws "Native crash dumping service is unavailable". |

So the zeroed console/crashdump block is not cosmetic preference — it is the
exact conjunction `validate_native_services` (UP.cpp:736-762, called at
UP.cpp:250) demands for a native start.

Expected-but-missing: nothing required is missing.
- Hot reload: `EnableHotReloadSystem` / `EnableAutoReloadingLuaMods` are parsed
  (SM.cpp:117,132) but correctly ABSENT — code defaults are `false`
  (SM.hpp:33,35) and setting either true throws "Native hot reload and
  filesystem watcher services are unavailable" (UP.cpp:745-746). Absence is
  the only correct staging.
- Log verbosity: the retained parser has NO log-level/verbosity key at all;
  there is nothing to set and nothing silently ignored.
- `General.InputSource` (SM.hpp:42, default `"Default"`) is NOT read by the
  retained `deserialize`; with the injected pad source, `set_input_source("Default")`
  passes by design (IH.cpp:214-224: injected mode accepts `"Default"` or the
  provider name). `EnableDebugKeyBindings` default `false` (SM.hpp:38) — fine.
- The `bUseUObjectArrayCache` interplay at UP.cpp:1396-1400 only affects the
  (disabled) GUI live view; no effect with the GUI off.

Parser-trap compliance (measured findings, `worker-ue4ss-settings/REPORT.md:30-34`):
the staged file uses only full-line `;` comments (safe), no inline `;` after
values (would corrupt `get_bool`), and no `[Overrides]` section (an
empty/comments-only section wedges the retained IniParser).

### ModsFolderPath / how Mods is located (decisive answer)
- Our build does NOT support a root override: `[Overrides] ModsFolderPath` /
  `ControllingModsTxt` are parsed (SM.cpp:84,110) but native activation REFUSES
  non-empty values — throw "Native activation does not support filesystem mod
  path overrides" (SM.cpp:112-114). The staged file correctly has no
  `[Overrides]` section.
- Mods location on native: `setup_mod_directory_path` early-returns with the
  nominal `{std::filesystem::path("ue4ss/Mods")}` when a mod source exists
  (UP.cpp:851-856); filesystem scanning paths (UP.cpp:858-878, override /
  working-dir / legacy fallbacks) are dead code in this mode.
- Actual mod discovery is from the loader-built snapshot keys:
  `NativeModSource` enumerates `ue4ss/Mods/<Name>/Scripts/main.lua` (or
  `scripts`) as Lua mods (NMS.cpp:98-126), skipping `shared` (NMS.cpp:121),
  enabled by per-mod `enabled.txt` PRESENCE (NMS.cpp:178-179) or `mods.txt`
  `Name:1` rows (NMS.cpp:157-176); `start_mods` iterates
  `source->startup_mod_names()` (UP.cpp:1962-1969).
- Therefore the activation root IS reachable — reachability equals the loader
  successfully materializing the snapshot from `/data/modbridge/roots/PPSA33286/`
  (console-VERIFIED readable; rehearsed 46 PASS / 0 FAIL against the real
  `modforge_activation.c` in `worker-final-staging-rehearsal/REPORT.md`). No
  path override is needed, possible, or staged. `require("UEHelpers")` resolves
  via `ue4ss/Mods/shared/UEHelpers.lua` (LuaMod.cpp:867-874), and main.lua
  guards it with `pcall` (main.lua:15-17).

## 3. MemberVariableLayout.ini / VTableLayout.ini — GO (Task 3)

Consumed from keys `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini`
via `StageNativeLayoutTexts` (UP.cpp:913-926; both must exist or start throws).

### MemberVariableLayout.ini (588 B)
- `[NativeLayout]` header identity matches the promoted axes exactly:
  `Format=1`, `prospero-x86_64-sysv`, `PointerSize=8`, `5.6`,
  `NonShipping=0` (SHIPPING, margin 60), `Stats=0` (OFF, margin 12),
  `EngineRevision=5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`
  — all four promotion criteria C3/C4 corroborated in
  `.superpowers/native-observations/transport-experiments-progress.json`
  (`zcode_PROMOTION_SIGNED`, `zcode_v3_MAPPING_BREAKTHROUGH`).
- `[FUObjectArray]` = LAYOUT A keys, exactly as validated:
  ObjFirstGCIndex 0x0, ObjLastNonGCIndex 0x4, MaxObjectsNotConsideredByGC 0x8,
  OpenForDisregardForGC 0xC, ObjObjects 0x10 — matches the recorded template
  ("ObjFirstGCIndex@0x0, ObjLastNonGCIndex@0x4, MaxNotConsideredByGC@0x8,
  OpenForDisregard@0xC, then TUObjectArray@0x10") and the live snapshot
  invariants (first=35477=last+1, MaxNotConsideredByGC==ObjFirstGCIndex,
  OpenForDisregardForGC=0). Tail keys UObjectCreateListeners 0x68,
  UObjectDeleteListeners 0x78, PrimarySerialNumber 0xB0,
  bShouldRecycleObjectIndices 0xB4, UEP_TotalSize 0xB8 match the promoted
  pair and the "5.06 total 0xB8" divergence note in the same record. No entry
  in the progress JSON contradicts any staged offset.
- `[TUObjectArray]` = the 7 validated keys: Objects 0x0, PreAllocatedObjects
  0x8, MaxElements 0x10, NumElements 0x14, MaxChunks 0x18, NumChunks 0x1C,
  UEP_TotalSize 0x20 (2 pointers + 4×u32; consistent with 64Ki/chunk,
  live 2162688 = 33×65536 max, 2/33 chunks, FUObjectItem in-size 0x18).
- All 17 section/key pairs are accepted by the retained schema (SCHEMA:88-101,
  102-111); the binder requires positive `UEP_TotalSize` and offset+width fit
  (NLB.cpp:105-116) — satisfied (0xB0+4≤0xB8, 0xB4+4≤0xB8, etc.).
- Verification chain: ParsePair `ok:true`, 17 members, identity round-trips,
  negative controls exit 2/3 (`worker-emit-final-pair/emission-final/parsepair-result.json`,
  `emission-receipt.json`); pair byte-identical to staged
  (f291d552/9c20f7c9); promoted rung 1→2 with C1–C5 PASS
  (`zcode_PROMOTION_SIGNED`).

### VTableLayout.ini (186 B)
Header-only, zero class sections — BY DESIGN: promotion scope is explicitly
"FUObjectArray + TUObjectArray subset (rung 1→2)"; no vtable facts were
promoted (`tick_sco_status: EXPLICITLY UNRESOLVED`). An empty vtable set is
accepted (ParsePair `vtables: []`; binder publishes nothing and
`CheckEmpty` passes, NLB.cpp:119-126). The file must EXIST (UP.cpp:918-921
throws if missing) and it does. No fabricated vtable entries — correct.

## 4. PS5Pad.ini and main.lua keybinds — GO (Task 4)

Staged mapping (parsed by PAD:38-49 buttons, 99-114 modifiers/key, 123
"at least one binding"; read via NPI.cpp:13):

| Pad chord | Produced UE4SS key | Consumed by main.lua |
|---|---|---|
| OPTIONS | F7 | `RegisterKeyBind(Key.F7, ...)` main.lua:694 (menu toggle) |
| UP | UP_ARROW | main.lua:698 (selection previous) |
| DOWN | DOWN_ARROW | main.lua:703 (selection next) |
| CROSS | RETURN | main.lua:708 (activate/spawn) |
| LEFT | LEFT_ARROW | main.lua:713 (page previous) |
| RIGHT | RIGHT_ARROW | main.lua:722 (page next) |

- 6:6 exact coverage, no extra or missing binds. Every callback is
  `ExecuteInGameThread`-wrapped. main.lua registers nothing else
  (`RegisterLoadMapPreHook` at main.lua:731 is not a keybind).
- All 6 button names are in the parser's accepted set (PAD:41-46); all 6 key
  names resolve in `string_to_key` (KD.cpp:22 return, 44-47 arrows, 117 f7,
  lookup at 180-195). `User=-1` is valid (PAD:83-88). Single-button chords:
  no duplicates, no chord-containment conflicts (PAD:115-120).
- USB keyboard caveat (honest): the composed PROSPERO runtime has NO keyboard
  input provider — the only injected input source is the PS5 pad bridge
  (`make_native_ps5_pad_source`, NPI.cpp:17); no keyboard code exists anywhere
  under `native-runtime-startup/` or the pad-config unit. The user's USB
  keyboard will NOT trigger F7/arrows/RETURN inside UE4SS on this build; only
  the mapped pad buttons will. (Keyboard keys still reach the GAME via the OS;
  they just never reach UE4SS `RegisterKeyBind`.)
- Non-exclusivity: while the menu is open, the mapped pad buttons also reach
  the game; the mod compensates (SetIgnoreLookInput/SetIgnoreMoveInput) per the
  INI's documented note.

### Operational hard dependency (read before launch)
Because `ue4ss/PS5Pad.ini` is staged, `ConfigureRetainedPS5PadInput`
(NPI.cpp:10-26) will: parse it, REQUIRE a working pad provider
(`is_available()` false ⇒ runtime start THROWS with phase/error, NPI.cpp:19-24),
and reject any other supplied input source as conflict (NPI.cpp:15). Absent
file would mean "no pad bridge" (NPI.cpp:14) — but it IS staged, so first-run
success now includes pad-provider activation. This is designed behavior
(HARDWARE-TEST-PLAN failure modes), not a defect.

**Contingency must-fix ONLY if the pad provider is not ready for this first
launch** (keep the run otherwise pure):
1. Remove the row `file	config	ue4ss/PS5Pad.ini	2960` (line 9) from
   `loadlist.txt` and remove `ue4ss/PS5Pad.ini` from the staged root, then
   re-stage the smaller `loadlist.txt` (expected size 446 − 41 = 405 B; row
   sum 32930 B) and re-verify with the size-checked activation oracle.
2. Do NOT edit any other line; do not blank the row (size fields are checked
   against staged bytes, MFA_E_SIZE rehearsal control).

## 5. loadlist.txt / enabled.txt / UEHelpers.lua — GO

- `loadlist.txt` (446 B): header `modforge-loadlist\t1`; `title PPSA33286`
  matches the activation root; all 7 `file` rows match recovered bytes and
  sizes exactly; `mod TownfallItemSpawner 1.0.0` row present. Rehearsed
  end-to-end (46 PASS / 0 FAIL) including negative controls.
- `enabled.txt` (0 B): correct — enablement is PRESENCE-based
  (NMS.cpp:178-179); content is never read. Empty file is the canonical form.
- `UEHelpers.lua` (9962 B): reachable from the mod's native `require` search
  paths as `ue4ss/Mods/shared/UEHelpers.lua` (LuaMod.cpp:867-874); main.lua's
  use is pcall-guarded so even a resolution failure cannot abort the mod.

## 6. Verdict table

| File | Verdict | Must-fix |
|---|---|---|
| ue4ss/UE4SS-settings.ini | **GO** | none (values are native-start REQUIREMENTS; keep zeros and ExternalThread) |
| ue4ss/MemberVariableLayout.ini | **GO** | none (layout A keys = validated/promoted facts; identity matches runtime) |
| ue4ss/VTableLayout.ini | **GO** | none (header-only is the promoted, ParsePair-verified state) |
| ue4ss/PS5Pad.ini | **GO** | none, IF pad provider ready; else use §4 contingency (remove row+file, re-stage loadlist) |
| ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua | **GO** | none |
| ue4ss/Mods/TownfallItemSpawner/enabled.txt | **GO** | none (0 B presence marker) |
| ue4ss/Mods/shared/UEHelpers/UEHelpers.lua | **GO** | none |
| loadlist.txt | **GO** | none (all rows byte-verified) |

First-run expectation notes (not config defects): console/GUI/crashdump/
hot-reload services throw by design if ever enabled — current bytes keep them
off; a pad-provider failure at start surfaces as a runtime-start throw naming
`PS5Pad` phase/error — triage per FAILURE-TRIAGE, never a blind retry.
