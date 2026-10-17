# APPLY — integrator steps for Team S1 (profile as data: B3, B5, B6, B7)

Mac-only staged work. Team S1 cannot build a PRX or touch the console; this is
what the integrator merges, builds (once), gates, and launches.

## 0. What is in this drop

| Part | Status | Where |
|---|---|---|
| Profile file format | complete | `PROFILE-FORMAT.md` |
| Runtime loader + data model | complete, host-tested | `new/ModProfile.hpp`, `new/ModProfileLoader.hpp` |
| Townfall profile (from today's headers) + equivalence proof | complete | `gen/townfall-profile.ini`, `gen/extract_townfall.py`, `host-tests/check_equivalence.py` (88/88 fields identical) |
| B5 pin split + ToString pin removal | complete | `new/.../CodeImageCache.hpp`, `new/.../CodeSignatureProfile.hpp` |
| B6 drop UE 5.6 numbers from checks | complete | `new/.../JoinedLayoutPreflight.hpp`, `new/.../BuildFactsChecks.hpp` (require_layout), `new/.../RuntimePakMount.cpp`, loader pak offsets |
| B7 engine version from profile | complete | `new/.../ue4ss/UE4SS-settings.ini` |
| B3 header→loader rewiring of the remaining data headers | recipe below (mechanical; integrator compiles) | §4 |

## 1. Apply the staged patch

Base copies are exact snapshots of the live files; verify before applying:

```
cd <repo>/outputs            # the 'outputs' working dir
shasum -a 256 -c <(sed 's#  #  #' sb-team-S1/base/BASE-SHA256SUMS)   # all 7 must be OK
# dry run, then apply (paths are relative to outputs/)
git apply --check --directory=outputs sb-team-S1/s1.patch     # or: patch -p1 < s1.patch from base/new roots
```

`s1.patch` is `diff -ruN base new`. It touches 6 existing files + adds
`ModProfile.hpp` / `ModProfileLoader.hpp`. Put the two new headers next to the
other profile units (same dir as `CodeImageCache.hpp`) so the quote-includes
resolve, and add them to the runtime source selection.

Base shas (must match the live tree before applying):
```
CodeImageCache.hpp              3fa64af86ea25543d1197b683b8ddc86a910e3b83500679395aa12bdcc8b00c1
CodeSignatureProfile.hpp        823bf228998bfe725aab01fdc695b3e706e48f74def3c874281ea7a19e127b3f
JoinedLayoutPreflight.hpp       1646a99e2184ea576eece3303e762132a7835b3782c3cee0545ba1fe03525a2c
RuntimeBuildProfile.hpp         09a14712c972ffe1a1e6967c5bc86eb88d918ebbc898727b12f4804c6d2af3aa
RuntimePakMount.cpp             f29af9ca0c75ce0aac2046875cfe1969073a54df1383d29a0eee2491a488c33d
UE4SS-settings.ini (fixture)    b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2
BuildFactsChecks.hpp            eb65ce9e9c2ff4834da2b8bb3ce7982bda9d595858e6c5c2d2d043cea3649a16
```

## 2. What the patched files do (self-contained; keep every gate's behaviour)

- **B5 — CodeImageCache.hpp**: `Pin`/`Entry` gain a `fatal` flag. Seed pins
  (fname ctor id1, SCO id2) stay fatal — a mismatch still refuses the whole
  image. Feature pins (thread id3, listener id5/6, pak 20-25) become non-fatal:
  a mismatch leaves them unverified so their feature refuses at its own gate,
  and a Lua-only launch still starts. The whole-image sha and the two seed pins
  are unchanged in behaviour. `named_entry` returns an entry only when verified.
- **B5 — CodeSignatureProfile.hpp**: the ToString pin (id 4) is removed (the
  bind it guarded was retired; the in-port decoder owns FName::ToString and its
  probe constants need no pin). Feature pins are wrapped `feature(...)` =
  `fatal=false`. Profile pin count 12 → 11. Positions 0/1 (the seed pins, read
  by `ExtendedCodeProfile`) are unchanged.
- **B6 — JoinedLayoutPreflight.hpp**: `evaluate` no longer compares each staged
  offset to a baked UE 5.6 number; it checks presence (required keys present)
  and keeps the key set + dispositions. A correct UE4 layout is no longer
  refused (D4). The baked `key.offset` is reference-only.
- **B6 — BuildFactsChecks.hpp `require_layout`**: the `.value` check no longer
  requires `staged.offset == pin.offset` (the baked 5.6 number); it reports the
  staged profile offset and accepts on presence + provenance + width. (D2)
- **B6 — RuntimePakMount.cpp `EnginePins::valid()`** and the loader: the pak
  slot/struct offsets (`slot_delegate_execute`, `slot_get_lower_level`,
  `pak_count_offset`) are profile data checked for plausibility (nonzero,
  8-aligned, bounded), not hard-wired to 0x60/0x68/0x18. `delegate_inst_off`/
  `delegate_size_off` stay the FDelegateBase C++ ABI (0/8). The live chain still
  re-proves everything before a mount.
- **B7 — UE4SS-settings.ini**: `[EngineVersionOverride] MajorVersion/MinorVersion
  = -1` (Detect). The deploy tool must emit -1 for every game; the profile's
  `[build] engine_major/engine_minor` then drives `Unreal::Version` at startup
  (BuildFactsChecks `settings.version` passes via `version_detect`). The live
  ROOT settings on the console get the same change from the deploy tool.

## 3. Wire the loader (B3 install)

The loader never opens a file; it parses bytes the activation preloaded. In
`runtime_start_bridge.cpp`, right AFTER `bridge_code_cache_prepare` returns a
ready `codecache::Metadata` (it has the authenticated `sha256`/`size`/`extent`/
`base`), and BEFORE the UE4SS init thread consumes seeds:

```cpp
#include "ModProfileLoader.hpp"
// codehash already links a streaming sha; adapt it to Sha256Ops:
struct BridgeSha { codehash::Sha256State st; };
static void bsha_init(void* v){ codehash::sha256_init(&((BridgeSha*)v)->st); }
static void bsha_update(void* v,const unsigned char* d,size_t n){ codehash::sha256_update(&((BridgeSha*)v)->st,d,n); }
static void bsha_final(void* v,char out[65]){ uint8_t dg[32]; codehash::sha256_final(&((BridgeSha*)v)->st,dg);
  static const char* h="0123456789abcdef"; for(int i=0;i<32;++i){out[i*2]=h[dg[i]>>4];out[i*2+1]=h[dg[i]&15];} out[64]=0; }

BridgeSha bs; rc::modprofile::Sha256Ops sha{bsha_init,bsha_update,bsha_final,&bs};
// rows = preloaded mfa files with role "profile" (see §5)
rc::modprofile::Profile prof; rc::modprofile::Diag diag;
auto r = rc::modprofile::Loader::select_and_parse(rows, row_count,
            g_code_cache.metadata.sha256, sha, prof, diag);
if (r == rc::modprofile::Result::Ok) {
    rc::modprofile::install(prof);          // active() now returns &prof
    rc::modprofile::install_all(prof);      // fill the loader-backed headers (§4)
} else {
    // fail closed: write image identity to runtime-status.txt via mfa_status_*,
    // run with NO mods (the identification launch). diag.detail says why.
}
```

`mfa_activate`'s default preload roles become `{lua_mod, config, script,
profile}` (one line in `runtime_module_entry.c` / the mfa defaults) so the
profile bytes are owned in memory. The interface struct Team S2 fills
(`ProfileSelectionInput`) is in `PROFILE-FORMAT.md §1`; the `rows` above are its
preloaded `profile`-role files.

## 4. B3 header→loader conversion recipe (mechanical; integrator compiles)

The loader + model + format + Townfall equivalence (88/88) prove the mechanism.
The remaining step converts each per-game data header from `inline constexpr
<literal>` to a loader-backed runtime global of the SAME name, filled by an
`install_from_active()` the install TU calls. Consumers are unchanged except the
few constexpr spots listed below. Pattern per header (keep the STRUCTURE — array
sizes, pin ids, which members are required, widths — compiled-generic; only the
VALUES load):

```cpp
// SeedTable.h (example)
namespace RC::Unreal::UnrealInitializer::SeedTable {
  inline uint64_t kBaseObserved = 0;           // was constexpr literal
  inline SlotSeed kGUObjectArray{};            // .name/.data_kind set in install (structural); .label/.present from active()
  ...
  inline char kImageShaBuf[65]{}; inline std::string_view kImageSha256{kImageShaBuf,0};
  inline void install_from_active() noexcept {
    const auto* p = rc::modprofile::active(); if(!p) return;
    kBaseObserved = p->image_base;
    kGUObjectArray = SlotSeed{p->guobject_present, true, p->guobject_array_label, "g_uobject_array", p->guobject_derivation};
    ... ; std::memcpy(kImageShaBuf,p->image_sha256,65);
  }
}
```

Do the same for: `FNameAbiProfile.hpp` (kPrefix bytes + case/bytes/align),
`EngineListenerOpsProfile.hpp`, `EngineToStringProfile.hpp` (constants; the pin
is already removed), `ThreadNameProfile.hpp` + `CandidateBindingData.hpp`,
`PakMountCodeProfile.hpp` + `pak_mount_pins.inc` (fill EnginePins from active()),
`CompiledNativeLayoutDeclarations.hpp`/`NativeLayoutDeclarations.hpp` (string_views
into `active()->layout_members/_vtables`), `RuntimeBuildProfile.hpp`
(`kProfile`, `kRequiredMembers`/`kRequiredVtables` offsets via
`rc::modprofile::layout_offset(active()->layout_members_or_vtables, class,
member)` — keep names+widths compiled), and `SeedValidation.hpp` `kPoolLayout`
(S3). Add `ModProfileInstall.cpp` that includes all of these and calls each
`install_from_active()` in dependency order (leaf arrays/labels first, then the
composite `codeprofile::kPins`, then `buildfacts`/`codecache` profiles).

Three CONSTEXPR spots that cannot read a runtime value — change them to the
port ABI constant (generic, not per-game), which the runtime still checks
against the profile at run time (`buildfacts settings.fname-shape`):
- `UnrealInitializer.cpp` `static_assert(sizeof(FName)==rc::fnameprofile::kOutputBytes)`
  and `static_assert(alignof(rc::fnameabi::Name)==rc::fnameprofile::kOutputAlignment)`
  → compare against `rc::fnameabi::kPortNameBytes`/`kPortNameAlign` (= 8/8).
- `SeedValidation.hpp` `kFNameBlockCapacityBytes = (1<<kPoolLayout.block_offset_bits)*2`
  → compute at first use from `active()->pool_block_offset_bits` (it is only
  used at runtime in `publish_readable_spans`), or keep a generic `16`-bit
  compiled default for the capacity floor (the floor only needs to be ≤ a real
  block, and 16 bits is the FNamePool invariant for UE ≥4.23).
- `NativeLayoutDeclarations.hpp` `static_assert(... profile_digest == kDigest)`
  → drop (both are runtime now; the digest equality is enforced at run time by
  `buildfacts layout.*` and `CandidateBindingData`).

Re-run the two layout gates with the generated shim (`pipeline-gates/NOTES.md`),
then `field_gate` / `layout_binder_gate` / `fs_throw` / `no_detour`, build once,
launch. The Townfall profile (`gen/townfall-profile.ini`) must load and the
run must behave exactly as R11 — that is the equivalence contract.

## 5. Deploy

- Generate the profile for the running image: `python3 -m tools.profilegen ...`
  then it emits `profile/<sha>.ini` in THIS format (generator team / B4).
- Place it under the title root, add the loadlist row
  `file<TAB>profile<TAB>profile/<sha>.ini<TAB><size>`, run the loadlist
  row-check (size+sha) before launch as usual.
- No profile for the running image sha ⇒ the runtime writes the image identity
  to `runtime-status.txt` and runs unmodded (the identification launch).

## 6. Pins that change
- `CodeSignatureProfile` pin count 12 → 11 (ToString pin removed). Any packaging
  assertion of 12 becomes 11.
- The per-game data moves from compiled-header overlays to the `profile/<sha>.ini`
  loadlist row (size+sha pinned there). See `pipeline-gates/NOTES.md §
  l26_packaging_driver.py`.
