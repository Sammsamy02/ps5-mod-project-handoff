# Agent GT — first-launch generator runtime TU (Claude round-5, 2026-10-08)

NEW FILES ONLY (no existing file touched — UnrealInitializer/LuaMod/drivers/
CHANGES untouched; root registers + wires at build staging):

| File | sha256 |
|---|---|
| `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include/FirstLaunchProfile.hpp` | `69279590ddeb21513f225bd36a2e03789ae76ff27a804fe7870f212a3545c16f` |
| `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/FirstLaunchProfile.cpp` | `fcb5628d2a8d9a5e7b12b3806eebd551eab4cf7ef1a88a671d60cf81f5608f17` |
| `outputs/2026-10-08-l25-prep/host-tests/firstlaunch-table-test.cpp` | `86fe7fd93b40f527b1029bb20f0ae993adcb391ffd25a3ef9faae04061d7f2c0` |

Design source: FIRST-LAUNCH-AUTOMATION.md (GI), GJ's
blade-prep/per-version-table.json laws block, GI's example packed cell
(tables/ue5.6-prospero-shipping-nostats...), FZ-GAME2-PROFILE-DRYRUN.md,
engine_match.py, NativePropertyReadBackend.inl, SeedTable.h,
PoolAdmissionEngine.hpp / ImageCoverage.hpp / CodeImageCache.hpp.

## 1. What the TU is

`FirstLaunchProfile.cpp` is a whole-TU `__PROSPERO__`-guarded
implementation of GI's first-launch ladder:

```
slot load/re-verify -> DETECT -> TABLE ROW -> GENERATE(+census arms)
  -> [verify REQUESTS to the initializer] -> COMMIT -> PERSIST
```

`FirstLaunchProfile.hpp` carries ONLY host-pure logic (no UE4SS/platform
includes) so the host test compiles it on the Mac:
- tier law mirroring engine_match.py thresholds (constants cited: triage
  min 3, refused max 8, tentative 4, confident 12, family-confident 25);
- bounded exception-free JSON reader + `parse_layout_table` accepting BOTH
  shipped forms: GI packed cell (`schema:"ue-layout-table/1"`) and GJ
  authoring (`schema:1`, version-keyed entries with per-field
  status/confidence, the measured 5.6 row's relative anchors);
- utf-16le needle encode + `scan_present` buffer primitive
  (engine_match `scan_markers` byte semantics);
- `derive_modbridge_root` — the GA law (strip `/ue4ss[/Mods]`, NO
  title-id default, Rule 10);
- C-ABI records `PinRecord` / `VerifyRequest` / `Outcome`.

Admission reuse (studied, mirrored, not duplicated by edit):
`fl_read_bytes` reproduces the `PropertyReadBytes` pattern from
NativePropertyReadBackend.inl (valid_range -> `admission::coverage` VQ
proof via `sceKernelVirtualQuery` -> memcpy -> budget check; per-op 1000ms
budget, whole-ladder under one `rc::pooladmission::kAdmissionBudgetMs`
deadline). The backend itself is a static inside SeedArming and is not
callable cross-TU, so the pattern is re-declared against the SAME
extern "C" seams the initializer uses: `bridge_readable_span_count/at`,
`bridge_code_cache_context`, `bridge_code_cache_signature`,
`bridge_image_range`. Linking note for staging: the TU needs those five
bridge symbols + `sceKernelVirtualQuery` (all already imported by the
PRX).

## 2. API (extern "C", declared in the header under __PROSPERO__)

```c
int  RC_first_launch_try_profile(const char* modbridge_root, uint64_t image_base);
int  RC_first_launch_verify_request_count(void);
int  RC_first_launch_verify_request_at(int index, rc::firstlaunch::VerifyRequest* out);
int  RC_first_launch_report_verify(const char* anchor_id, int passed);
int  RC_first_launch_commit(void);
int  RC_first_launch_pin_count(void);
int  RC_first_launch_pin_at(int index, rc::firstlaunch::PinRecord* out);
int  RC_first_launch_outcome(void);
```

Outcome codes: `0 Refused` (fail-closed, loud), `1 LoadedVerified`
(persisted slot re-verified; pins exposed with verified=1),
`2 PendingVerify` (candidates corroborated; behavioral verify REQUESTS
await the initializer), `3 Committed` (all requests passed; persisted),
`-1 Misuse`, `-2 NotRun`. One-shot per launch.

Protocol: try -> for each request {initializer runs its EXISTING probe
machinery; report(anchor, 1|0)} -> commit (persists
`<root>/profiles/<image-sha8>.profile.json`, no absolute addresses —
image_relative only, ASLR law) -> initializer retargets its seed
admission at `RC_first_launch_pin_at` records (prologue bytes included
for code pins, ready for the pin-registry route). A failed report
fail-closes the whole profile (wrong pin by definition, GI 2b).

## 3. WIRING REQUIREMENT (root, at build staging — exact slot)

Call site: in `UnrealInitializer.cpp` `SeedArming` init path,
**before** the pinned-profile admission
`consume_required_slot_seeds(...)` (the call at UnrealInitializer.cpp:1682
inside the Phase ladder), as that admission's input source:

```cpp
#if defined(__PROSPERO__)
extern "C" int RC_first_launch_try_profile(const char*, uint64_t);
{
    namespace fs = std::filesystem;
    // GA law (LuaMod.cpp prepare_mod): root from the RUNTIME-resolved
    // mods directory — never a title-id.
    fs::path root = fs::weakly_canonical(get_mods_directory-shaped path);
    if (root.filename() == "Mods") root = root.parent_path();
    root = root.parent_path(); // <root>/ue4ss -> <root>
    RC_first_launch_try_profile(root.string().c_str(), SeedArming::module_view_base());
}
#endif
```

Then: `PendingVerify` -> run the existing probes per request kind
(`behavioral_fnamector_byteproperty` = the Phase-4 slot-2 ctor
ByteProperty probe; `pool_words` = Phase-3 pool admission; `gmalloc_v1_v2`
= Phase-2b validators; `sco_empty_callback_abi` = Phase-5; shape checks
already ran in-TU) -> report -> commit -> arm from the exposed pins.
`Refused` -> keep today's compiled-in pinned profile path OR refuse per
the deployment policy (the generator never partially arms). NEW_UNITS
registration per GI 5c: `{'firstlaunch': {'source': .../FirstLaunchProfile.cpp,
'template': 'unreal-unrealinitializer', ...}}`. The shipped tables asset
must ride at `<modbridge-root>/tables/*.layout-table.json` (the loader
also accepts the GJ authoring filename `per-version-table.json` for
bring-up; the packer renames at ship).

## 4. [FL] marker family (for markers-declared.json; contiguous narrow
   literals in the TU, widened once via Unicode::DecodeUtf8 — st_send
   doctrine; utf-16le wide twins are the runtime-widened forms)

| Template | Fires |
|---|---|
| `[FL] profile slot=absent` | no persisted slot |
| `[FL] profile slot=regenerate sha8={8hex}` | slot failed re-verify |
| `[FL] profile loaded={sha8|none} verified={0|1} [pins=N / parse-error=.. / image-sha-mismatch / no-pins / anchor-..-reverify-failed / empty-pins]` | slot load/re-verify |
| `[FL] detect verdict={pass|refuse} tier={REFUSED|FAMILY-ONLY|TENTATIVE|CONFIDENT} markers={present}/{total} triage={hits}/{set}` | after detect |
| `[FL] table row={version} version={version} status=measured table={table_id}` | winning measured row |
| `[FL] table refused file={} error={}` | unparsable table file (non-fatal, skipped) |
| `[FL] verify request {anchor} probe={kind} addr=0x{X}` | per emitted request |
| `[FL] pin {anchor} addr=0x{X} verified=0 arms={n}/{min} census={refs|opt}` | per generated pin |
| `[FL] profile persisted={path} sha8={8hex} schema=1` | slot write ok |
| `[FL] persist failed session-armed=1 warn=persist-retry-next-launch` | GI 5b rung 5 |
| `[FL] commit outcome=COMMITTED` | commit ok |
| `[FL] FAIL-CLOSED reason={...}` | every refusal rung |

## 5. Syntax evidence (battery law: -fsyntax-only only, no builds, no console)

- TARGET TU: prospero-clang++ (sdk-v0.43), argv cloned from
  `2026-10-08-l26-build-variants/l26-B/pass1/objects/unreal-unrealinitializer.command.json`,
  `-ivfsoverlay` (+arg) stripped, `-c/-o/-MD/-MF` replaced by
  `-fsyntax-only`, `LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config`
  — **exit 0** (2026-10-10T21:39:14Z; sole stderr = benign
  'linker input unused' from the wrapper's -l additions).
- HOST TEST: `c++ -std=c++23 -fsyntax-only firstlaunch-table-test.cpp` —
  **exit 0** (AppleClang). NOT executed (battery); run command documented
  in the test header for the next powered session.

## 6. Host test coverage (logic-level; image-walk stays target-only)

- root-derivation law cases (incl. backslashes, trailing slash, empty);
- tier ladder: Astro path (0/16 -> REFUSED), floor (5 <8 -> REFUSED),
  FAMILY-ONLY (20), TENTATIVE (12, margin 4), CONFIDENT (54, margin 46 —
  Townfall numbers), table-threshold override agreement;
- utf16le_needle ascii + surrogate pair; synthetic image fixture with
  planted/absent markers through `scan_present`;
- token/pattern parsers (hex prose "0x0 (8B)", ?? wildcards,
  pack_version forms);
- GI example cell: axes, thresholds 54/12/4/8, envelope 64MiB/8,
  placeholder-marker detection (contributes nothing), 4 anchor ids,
  agreement/verify kinds, structures (ObjObjects@16, listeners@88 — the
  l25 correction), FMalloc::Malloc@48, and the NOT-YET stub assertion
  (no labels in packed cells -> runtime refuses);
- GJ authoring table: 7 bands, 5.6 measured/1.0, seed-band refusal via
  select_row (5.4/4.27/5.8 -> nullptr), every relative label (pool
  0x9365EC0, GUO 0x93BA110, ctor 0x23D590 + exact 32B prefix, SCO
  0x490AA0 + 25B, GMalloc 0x930ADD8, instance 0x93089B8, vtable
  0x7EC26E0, ToString 0x23FA00, add-listener 0x4841A0 + 16B), pool shape
  (8192 / 0x20000 / header 8/12/16), members, vtable slots;
  candidate arithmetic base 0x400000 reproduces SeedTable receipts
  (0x97BA110 / 0x63D590 / 0x970ADD8); required-anchor labeling;
- malformed-input refusals.

## 7. NOT-YET stubs (explicit; each REFUSES, never guesses a pin)

1. **Census-pool ranking for label-less cell anchors** — the packed GI
   cell ships no labels (labels are image-relative provenance per the
   laws); anchors without a table label fail-closed
   (`reason=anchor-<id>-no-table-label-census-pool-NOT-YET`). Today's
   generation is driven by GJ-form MEASURED rows carrying relative
   anchors (verified by the host test).
2. **Cost-solve margin** — the 30x3x2 gated-marker solve is not shippable
   from the example cell (marker pack is a placeholder). First-cut margin
   = present_count - refused_max, documented at `classify_detect`; the
   shipped thresholds (12/4/8/25) remain the ladder. Packer TODO: fill
   `detection.triage_markers` with the 16 real widest-coverage strings
   (offline via engine_match.triage_indices) — until then on-console
   detect fail-closes with `detect-no-markers`.
3. **Wildcard pattern-scan candidate generation** — parsed as ADVISORY
   probe_patterns only (GJ law: a probe miss is never a refusal; they
   went 0/9 and 0/14 image-wide).
4. **ueselftest auto-run / verdict.json / mod-per-type handoff** — out of
   this TU's scope (GB/GG seams); the commit hook is where 2d attaches.

## 8. Honest deviations & notes

- `bridge_code_cache_signature` positive = DECISIVE arm (+100); a zero
  return is treated as UNPROVEN (the owned-copyin capture may not cover
  unregistered addresses), not as a mismatch — the initializer's verify
  request remains the gate. Optional code pins require the decisive arm.
- LEA census counts `REX[WR] 8B/8D /r(rip)` sites targeting the label —
  corroboration only, never selection. On XOM consoles text is not
  CPU-readable so it contributes 0 there; data anchors then corroborate
  via shape arms + utf16 census (>=3 distinct module strings).
- Range class derives from the LIVE codecache metadata segments
  (PF_R&&!PF_X vs PF_X). `rc_seed::classify_seed_address` is deliberately
  NOT reused: its bounds are Townfall-pinned constants.
- GMalloc cell-form `agreement.arm_qualifier` string is not evaluated;
  `min_arms` is honored, and the V1/V2 word-pair check (table instance +
  vtable labels) is implemented as the decisive data arm.
- Persist failure after all verifies = session-arm + WARN (GI 5b rung 5);
  every other refusal is terminal, one-shot, zero partial state.
- The utf16 census set (Engine/Renderer/AnimGraphRuntime/Landscape/
  RenderCore/FMallocBinned3) is compiled in: engine-level facts, Rule 10
  clean (release_audit.py scope: no game names/title-ids anywhere in the
  three new files).

— agent GT, 2026-10-08 (battery session: code + -fsyntax-only only; no
builds, no --execute, no console contact; nothing herein authorizes any
console action).
