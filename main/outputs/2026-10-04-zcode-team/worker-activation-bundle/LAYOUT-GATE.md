# LAYOUT-GATE.md — can this activation bundle reach engine init without layout files?

**STATUS: BLOCKED-ON-LAYOUT-DATA.** Short answer: NO. An activation bundle
without validated `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini`
hard-stops during `UE4SSProgram::init()` BEFORE the engine scan, engine init,
`on_engine_initialized` and any Lua mod start. This worker did not synthesize
layout content and did not execute the runtime; every statement below is traced
to source read this session.

## 1. What the runtime requires, and where it stops

Startup order on `__PROSPERO__` with a native `mod_source`:

1. `UE4SSProgram` constructor: settings deserialize reads
   `ue4ss/UE4SS-settings.ini` — missing key throws here
   (SettingsManager.cpp:60-63). Our bundle supplies it (BUNDLE-SPEC 1.1).
2. `init()` calls `setup_unreal()` (UE4SSProgram.cpp:619).
3. `setup_unreal()` FIRST calls `load_unreal_offsets_from_file()`
   (UE4SSProgram.cpp:936 -> 896), which reads both layout keys from
   `mod_source` and throws
   `"Native activation requires both member and vtable layout files"` when
   either is absent (UE4SSProgram.cpp:899-904), then calls
   `Platform::NativeLayouts::StageNativeLayoutTexts(members->bytes, virtuals->bytes)`
   (UE4SSProgram.cpp:905).
4. Only AFTER that does the unreal initializer config/PreInitialize/Initialize
   (scan, version axes, object-array waits) run
   (UE4SSProgram.cpp:946-947 per INTEGRATION-MAP Stage 4; UnrealInitializer.cpp:940-947).
5. Engine readiness and Lua mod start come last
   (UE4SSProgram.cpp:648-661, 1947-1953).

So the hard stop for a layout-less bundle is step 3 — an exception inside
`setup_unreal()`, before `ScanGame()`. Nothing downstream (fire_unreal_init_for_cpp_mods,
setup_unreal_properties, share_lua_functions, on_engine_initialized, start_mods,
the Item Spawner main.lua) is reached. The strict staged root
(`build/stage-root`, built without layout keys) demonstrates the input side of
this stop: `mfa_activate` succeeds (the activation layer does not know about
layout keys) while `mfa_find` returns NULL for both — test T5.

## 2. What StageNativeLayoutTexts/StageNativeLayoutFiles require exactly

`StageNativeLayoutFiles(directory)` reads `<dir>/MemberVariableLayout.ini` and
`<dir>/VTableLayout.ini` as regular files <= 8 MiB each and delegates to the
same bundle path (NativeLayoutBinding.cpp:131-138). The activation route uses
the text form with the bundle's bytes. `StageNativeLayoutBundle` then enforces
(NativeLayoutBinding.cpp:139-164):

- Identity: `format == 1`, `target == "prospero-x86_64-sysv"`, `pointer_size == 8`,
  `engine_major >= 4`, non-empty `engine_revision` and `build_id`
  (NativeLayoutBinding.cpp:143-146). Any file without a valid identity header
  throws `"Invalid native layout identity"` — a comment-only placeholder cannot
  pass this check by construction (no identity record at all).
- Members: every section::key must exist in the compiled schema
  (`NativeLayoutSchema.inc`; unknown member throws, :72); bitfield tuples only
  for bitfield members, length < 64 (:73-78); duplicate member/alias throws
  (:81-82); every non-bitfield member (except `UEP_TotalSize`) needs a compiled
  accessor width from `GetAccessorWidth` — unknown width throws
  `"Native accessor width unavailable"` (:87-91).
- Per member class with entries: positive `UEP_TotalSize` required (:107-109);
  every offset in [0, total) and width within the declared size (:110-115).
- Vtables: known class sections only (:95-96); offsets <= INT32_MAX, 8-byte
  aligned, never bitfields (:156-158); aliases sharing a physical map must agree
  (:100-103).
- One staging per loaded lifetime: restaging throws
  `"Native layout bundle is already staged for this loaded lifetime"` (:161).

After staging, publication additionally requires resolved axes:
`SetResolvedNativeLayoutAxes(non_shipping, stats)` (wired after the first scan
pass, UnrealInitializer.cpp:832) and `PublishStagedNativeLayoutMaps(major, minor,
non_shipping, stats)` which refuses when axes are unresolved, when staged
identity disagrees with the resolved engine facts (major/minor/axes,
:181-182), and reuses immutable maps on same-bundle retry (:183-185).

## 3. The second half of the gate (not fixable by the bundle alone)

Even WITH validated layout bytes, `PublishStagedNativeLayoutMaps` currently has
NO production caller — only Stage and SetResolvedNativeLayoutAxes are wired
(UE4SSProgram.cpp:905; UnrealInitializer.cpp:832). The publication call must be
invoked with `Version::Major/Minor` from the scan result when
`InitializeVersionedContainer()` selects `NativeProvidedVirtualVC` on Prospero.
That wiring is the controller's recorded GAP (INTEGRATION-MAP SEAM 2). So the
gate is doubly blocked:

1. **blocked-on-layout-data** — no validated Townfall member/vtable pair with
   identity + axes exists in any retained source (worker-2's deliverable). This
   worker's bundle records both keys with `"blocked_on_layout_data": true` and
   NO bytes (`build/bundle.json`); the placeholder variant exists solely to
   exercise the mfa load path and is explicitly marked NOT layout facts
   (test T5 assertion).
2. **blocked-on-publication-wiring** — the controller must connect
   `PublishStagedNativeLayoutMaps` to the resolved version facts; until then,
   staged maps never reach the accessor maps the versioned container needs.

## 4. What would be dishonest to claim

- That the placeholder INI files "work": they fail identity validation by
  construction (no identity record). They were never passed to
  `StageNativeLayoutTexts` (the C++ runtime was not executed in this task).
- Any Townfall member offsets, vtable offsets, `UEP_TotalSize` values, engine
  revision/build ids: none are retained as validated facts; synthesizing them
  is prohibited and none were written.

## 5. Unblocking path (no new runtime code required by this worker)

1. worker-2 delivers the validated layout pair contents + identity
   (engine major/minor + non_shipping/stats axes) that satisfy section 2.
2. The controller wires the publish call per INTEGRATION-MAP SEAM 2.
3. This bundle's builder then replaces the two blocked entries with the
   validated bytes (same keys, role `config`); every other key, record and
   host-tested behavior in BUNDLE-SPEC.md is unchanged by that substitution.
