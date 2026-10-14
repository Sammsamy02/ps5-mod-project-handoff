# Shadow-copy scanner — UE4SS runtime integration design

Worker: worker-code-scan. Research + prototype only; no console contact, no game
claims. Everything below anchors on files that exist in this workspace.

## 0. Why: the exec-read wall

PC UE4SS's sig system scans the game module's mapped memory. On this PS5
target the game text (LOAD0, vaddr `[0, 0x7479b1c)`, runtime
`[0x400000, 0x787b1c)`) is mapped EXEC-ONLY: kernel VMMAP `prot=4`, no READ
bit. Data reads of those pages fault — proven in-game (igf4 `rc=3
covered_not_readable` at 0x33ac8c0 / 0x890aa0; igf6 died twice at the first
exec-span read). A live-page AOB scan can never run.

The shadow inverts the problem: scan a private readable copy (the retained
decrypted eboot, sha256 `3b6cfad4...`, already the pinned offline reference)
inside our PRX, then map hits to live addresses with the per-boot slide.

## 1. Where the shadow plugs in (existing code, exact files)

The port already has every seam needed. Nothing in the current seed/scan flow
has to be removed; the shadow is a new *ModuleSource* implementation.

### 1.1 The ModuleSource seam

`Unreal/PlatformModuleSnapshot.hpp` (lineage-overlay-2-l10r3-b) defines
`ModuleSnapshot` with `Views()`, `ReadableRanges()`, `IsMonolithic()`,
`FindExport()`. Its constructor REJECTS anything but one module extent
`[base, base+size)` containing sorted, non-overlapping readable ranges —
which is why `shadow_copy.cpp` uses a SINGLE contiguous allocation mirroring
the linked vaddr space (host test asserts the contract passes).

`UnrealInitializer.cpp` `SetupUnrealModules()` already consumes exactly this
interface on non-Windows:

```cpp
SigScannerStaticData::m_modules_info = source->Views();
SigScannerStaticData::m_is_modular   = !source->IsMonolithic();
```

### 1.2 The owned-range scan seam

`Signatures.cpp` (`ScanForGameFunctionsAndDataImpl` / `ScanForGUObjectArrayImpl`)
already calls the owned-range scan on non-Windows:

```cpp
SinglePassScanner::start_scan(signature_container_map, config.ModuleSource->ReadableRanges());
```

`SinglePassSigScanner.hpp` documents that overload: synchronous, borrowed
caller-owned bytes, no match crossing a range boundary — the shadow satisfies
it byte-for-byte (host-verified).

### 1.3 The ScanGame path

`UnrealInitializer.cpp:1040 ScanGame()` on `__PROSPERO__` currently:
1. SHAPE A gate — refuses unless the required seed table is complete;
2. `SeedArming::consume_required_slot_seeds(...)` binds slots 0/2/3/4 from
   `SeedTable.h`; the `ps_scan` arm is structurally dead; optional slots are
   "unbound by design" (fname_tostring, gnatives, fuobject_hash_tables_get).

The shadow becomes a THIRD arm, strictly additive:
`shadow_arm()` after seed consumption — optional/secondary slots resolve from
the shadow; REQUIRED slots stay seed-authoritative with shadow cross-check.

## 2. ShadowModuleSource (new unit, sketch)

```cpp
// ShadowModuleSource: ModuleSnapshot over the shadow copy.
// Built AFTER the bridge computed this boot's base (same-capture rule).
class ShadowModuleSource {
public:
    // 1. read the decrypted eboot (path from title config, e.g.
    //    /data/homebrew/<title>/decrypted/eboot.bin) via the PRX file core
    //    (sceKernelOpen/Read — proven live by the bridge probes);
    // 2. stream-hash it; REFUSE unless sha256 == SeedTable::kImageSha256;
    // 3. shadow_init(..., slide = bridge_observed_base);
    // 4. wrap the ranges in a ModuleSnapshot (monolithic, MainExe).
    static std::shared_ptr<const ModuleSnapshot> Create(
        const uint8_t* eboot, size_t size, uint64_t slide);
};

Platform::ModuleInput in{};
in.lifetime    = shadow_owner_shared_ptr;        // keeps ShadowImage alive
in.monolithic  = true;                           // single-image game
Platform::ModuleImage& img = in.images[ScanTarget::MainExe];
img.base         = shadow->base;                 // the "known base"
img.size         = shadow->backing_size;
for (each loaded segment s)
    img.readable_ranges.push_back({ s.backing, s.filesz }); // file-backed only
in.find_export  = [](std::string_view) -> void* { return nullptr; }; // no dynsym on this image
auto snapshot   = Platform::ModuleSnapshot::Create(std::move(in));
```

`Views()` then feeds `SetupUnrealModules()` unchanged; `ReadableRanges()`
feeds `Signatures.cpp` unchanged.

## 3. The match-address bifurcation (the one subtle contract)

On PC, `container.get_match_address()` is simultaneously (a) the live address
to store and (b) readable memory to decode around. On PS5 those split:

- the LIVE address (for `assign_address`, detours, calls) is
  `linked + slide`;
- the READABLE bytes are in the shadow; the live exec page may not be read.

Resolution: report_owned_match / the container wrapper must translate before
handlers run, and handlers that decode operands do so through a shadow
accessor. Concretely:

```cpp
namespace ShadowSigAdapter {
    // call first inside every on_match_found:
    inline uint8_t* live(SignatureContainer& c) {              // (a)
        uint64_t v; shadow_ptr_to_vaddr(shadow, c.get_match_address(), &v);
        return reinterpret_cast<uint8_t*>(v + shadow->slide);
    }
    inline const uint8_t* bytes(SignatureContainer& c) {       // (b)
        return c.get_match_address(); // still the shadow pointer — safe to read
    }
}
```

Data-seed handlers (GUA, GMalloc class) use the rip recipe proven in the host
test — match the xref sig, decode the disp32 on the shadow bytes, add slide:

```cpp
on_match_found = [&](SignatureContainer& c) {
    RipRefResult rr;
    resolve_rip_ref(shadow, site_vaddr(c), &rr);         // reads shadow bytes
    uint64_t linked_label = rr.target_vaddr;              // e.g. 0x93ba110
    uint64_t live_label   = linked_label + shadow->slide; // 0x97ba110
    UObjectArray::SetupGUObjectArrayAddress((void*)live_label);
};
```

(For the direct-function class — FName ctor, SCO — `live(c)` alone is the
answer; no operand decode.)

## 4. Policy: seeds stay authoritative

Preserves the existing SHAPE A doctrine and the studio's "never silently
override" rules:

1. **Identity gate.** The shadow is refused unless the file sha256 equals
   `SeedTable::kImageSha256`. A game update invalidates every sig at once;
   the loud refusal names both hashes. (Same reason the file-offset rules are
   re-derived per phdr, never assumed: `+0x4000` holds for LOAD0/1/3/7,
   `-0x324000` for LOAD8.)
2. **Required slots (0/2/3/4).** Seed-bound first (unchanged). The shadow
   scan runs as a cross-check: agreement → log confidence; disagreement →
   LOUD refusal listing seed and shadow values; never a silent shadow
   override. All four proven seeds cross-check green today (host evidence).
3. **Optional slots** currently "unbound by design" (fname_tostring,
   gnatives, fuobject_hash_tables_get, console_manager_singleton) become
   shadow-resolvable, each keeping its consumer's degradation path if the
   sig misses (the existing fallbacks — Conv_NameToString, FFrame null walk —
   stay).
4. **Address classes travel with the hit.** A hit in LOAD0 maps to a live
   exec-only address → call-only policy (assign + indirect call; never a
   data read, never a detour-verification read). A hit in LOAD7 BSS maps to
   data-readable-under-lease. The shadow never loosens the class rules — it
   only supplies the address.
5. **Slide provenance.** `slide` comes from the bridge's geometry
   (`live_module_facts` observed_base route: window anchors B/D/E verified
   inside readable VMMAP spans; validated uniform +0x400000). Same boot, same
   capture as the scan — never a cached slide.

## 5. PRX-side resource plan

- Source bytes: the decrypted eboot already ships with the homebrew staging
  (`/data/homebrew/<title>/decrypted/eboot.bin` — profile
  `identity.executable_console_path`). Read once at init through the PRX file
  core (sceKernelOpen/Read proven live; NID-routed fopen also proven).
- Memory: default policy (LOAD0+1+3+7) = 157,496,344-byte allocation
  (0x9633418 ≈ 150 MiB; 146.8 MiB of it file-backed). Text-only policy
  (LOAD0 alone) = 116.4 MiB if the heap is tight. LOAD8 stays opt-out
  (runtime mapping unproven → its addresses cannot be honestly mapped live).
- Zero-copy variant: where the PRX can `mmap` the file, per-PT_LOAD windows
  `[p_offset, p_offset+p_filesz)` can be mapped PROT_READ directly and used
  as the ranges (the single-allocation contract then needs a reserved
  contiguous VA span; fall back to the copy if not available). The copy-based
  prototype is the portable baseline.
- Cost: one-time ~0.4 s on the Mac host for read+build+all seed scans; the
  scan itself is a handful of ms. No per-frame work.

## 6. What this does NOT change

- No reads of live exec pages, ever (the whole point).
- No change to the seed table format, SeedValidation, or the lease/span rules.
- No new claims about in-game behavior: this is host-verified geometry +
  scan machinery. The in-game step (Phase F arming, shadow cross-check
  logging) remains future console work under the usual evidence rules.
