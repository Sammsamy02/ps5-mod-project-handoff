# worker-code-scan — shadow-copy code scanner (design + host verification)

Task: make the game's code scannable for byte patterns (PC UE4SS AOB
capability) on the PS5 port. Host-side research + prototype only: no console
contact, no in-game claims.

- Image: retained decrypted eboot, 194,241,424 bytes,
  sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
  (`worker-game-version/console-decrypted-eboot.bin`; byte-identical pin in
  the game profile `identity.image_sha256`).
- Result: **35/35 host checks pass, clean under AddressSanitizer + UBSan**,
  all four proven seeds recovered exactly (`host_test/run-output.txt`).

## 1. Scanner design

### 1.1 The exec-read wall (why a shadow at all)

PC UE4SS AOB-scans the game module's mapped memory. On this target the game
text (LOAD0, linked `[0, 0x7479b1c)`) is EXEC-ONLY in the kernel VMMAP
(prot=4, no READ bit): data reads fault — proven in-game (igf4 `rc=3
covered_not_readable` at 0x33ac8c0/0x890aa0; igf6 died twice at the first
exec-span read). So the live-page scan is impossible; instead we scan our own
readable copy and map back.

### 1.2 The shadow (`shadow_copy.hpp/.cpp`)

`shadow_init(file_bytes, file_size, slide, policy, allocator)`:

1. Parses the ELF64 program headers with the same field layout the bridge
   uses (e_phoff@0x20, e_phentsize@0x36=56, e_phnum@0x38; 14 phdrs here).
2. Selects PT_LOADs by policy class — default = the runtime-proven set:

   | phdr | flags | linked vaddr | filesz | memsz | file-offset delta |
   |------|-------|--------------|--------|-------|-------------------|
   | 0 | --X | 0x0 | 0x7479b1c | 0x7479b1c | +0x4000 |
   | 1 | R-- | 0x747c000 | 0xa3e918 | 0xa3e918 | +0x4000 |
   | 3 | RW- | 0x7ebc000 | 0x127e4e8 | 0x1280000 | +0x4000 |
   | 7 | RW- | 0x913c000 | 0x1cc5f2 | 0x4f7418 | +0x4000 |
   | 8 | --- | 0x9633420 | 0x262cea8 | 0x262cea8 | **−0x324000** |

   LOAD8 (prot 0, runtime mapping unproven at the igf3 vintage) is opt-in
   only: addresses found there cannot be honestly mapped to live addresses.
   The per-phdr `p_offset/p_vaddr` deltas are used directly — this kills the
   documented "+0x4000 trap" (it is NOT uniform: LOAD8 maps `vaddr −
   0x324000`).
3. Allocates ONE contiguous PRX-owned buffer of
   `max(vaddr+memsz) − min(vaddr)` (default policy: 0x9633418 = 157,496,344
   bytes ≈ 150 MiB; 146.8 MiB of that file-backed), zeroes it, and memcpy's
   each selected PT_LOAD's file-backed prefix at its vaddr slot.
   Single-allocation is deliberate: it makes the whole shadow satisfy UE4SS's
   `ModuleSnapshot` extent contract (sorted, non-overlapping readable ranges
   inside one `[base, base+size)`), and it makes the "known base" arithmetic
   literal:

   `live_address == shadow_ptr + slide − shadow_base − min_vaddr`

   (LOAD0 vaddr starts at 0 on this image, so `min_vaddr == 0`.)
4. BSS tails stay zero-filled: translation remains total, but scanning is
   file-backed-only by construction (BSS bytes do not exist in the file).

### 1.3 The AOB scanner

`size_t scan_pattern(uint8_t *pattern, const char *mask, size_t len, uint64_t *results, size_t max_results)`
— exact task signature; operates on the module-active shadow installed by
`shadow_init`. UE4SS mask semantics (`'?'` wildcard, otherwise literal).
Results are LINKED vaddrs ascending; returns the full match count even beyond
capacity; `SIZE_MAX` refusal for all-wildcard masks, short masks, zero
length, null image. memchr-probe on the first literal byte; no allocations,
no exceptions, no OS calls (PRX/realtime-safe). Variants:
`scan_pattern_in` (explicit image + vaddr window), `scan_pattern_live`
(stores live addresses).

### 1.4 Live mapping + rip resolution

- `live = linked + slide`; slide is the per-boot load bias from the bridge
  geometry (`live_module_facts` observed_base; validated uniform
  **0x400000** across all PT_LOADs on observed boots). Host test also proves
  the math is slide-polymorphic (0x123400000 test → 0x12363d590).
- BSS data seeds have no file bytes at their address. They are found the way
  PC UE4SS finds them: scan a code xref, decode the rip-relative disp32,
  resolve the target. `resolve_rip_ref()` decodes the observed family
  (REX.W/REX.WR `48/4c`, opcodes `8d` lea / `8b/89` mov, modrm mod=00
  rm=101): `target = site + instr_len + disp32`.

## 2. Verification against the proven seeds (host results)

| Seed | Linked | Expected live | Sig (from pinned image) | Hits | Found | Live match |
|------|--------|---------------|--------------------------|------|-------|------------|
| fname_ctor_wchar | 0x23d590 | **0x63d590** | 16B prologue `55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74` | **1 (unique)** | 0x23d590 | YES |
| static_construct_object | 0x490aa0 | **0x890aa0** | 25B extension (16B prologue alone: 22,879 hits — NOT unique) | **1 (unique)** | 0x490aa0 | YES |
| g_uobject_array | 0x93ba110 | **0x97ba110** | 3 xref sigs (below) → rip resolve | 2 / 1 / 7 | sites 0x2b212a, 0x2b2399, 0x2b25f0, 0x2b1e57, 0x2b27fc all resolve → 0x93ba110 | YES |
| gmalloc | 0x930add8 | **0x970add8** | bootstrap store `48 89 1d ?? ?? ?? ?? 4c 8b 35 ?? ?? ?? ?? 0f b6` (both disps wildcarded) | **1 (unique)** at 0xb660 → rip resolve | 0x930add8 | YES |

GUA xref sigs (disp32 wildcarded):
- A `48 8d 1d ? ? ? ? bf 00 00 00 00 39 43 24 7e` → exactly the two
  documented sites 0x2b212a / 0x2b2399, both resolve to 0x93ba110.
- B `48 8d 1d ? ? ? ? 4c 89 e7 4c 89 fe ff 50 18` → unique at 0x2b25f0 →
  0x93ba110.
- C `4c 8d 2d ? ? ? ? 41 39 45 24 7e 18 49 8b` (REX.WR form) → 7 hits incl.
  documented 0x2b1e57 / 0x2b27fc (both → 0x93ba110); the 5 extra same-shape
  sites resolve elsewhere — the disambiguation rule is "group by resolved
  target", documented in the test.

Geometry checks also pass: all 4 loaded segments byte-identical to the
pinned phdr table; whole-image extent 0xbc602c8; the inter-LOAD gap vaddr
refuses translation; LOAD1 spot byte matches the file at vaddr+0x4000 (S-box
anchor); GUA label translates into zeroed LOAD7 BSS tail (filesz end
0x93085f2); the ModuleSnapshot extent contract holds; alternate-slide math
inverts. Refusals verified: all-wildcard, short mask, zero-length, null
image, absent pattern → 0 hits.

Performance (Mac, -O2): full run — 194 MB read + 143 MiB shadow build + every
seed scan — **0.46 s wall**; scan-only cost is milliseconds.

## 3. Integration plan (details in INTEGRATION.md)

Additive; the existing seams already fit:

1. `ShadowModuleSource` implements the port's `ModuleSnapshot` contract
   (`Unreal/PlatformModuleSnapshot.hpp`): monolithic MainExe image,
   `base = shadow->base`, `size = backing_size`, readable ranges = per-LOAD
   file-backed windows. `UnrealInitializer.cpp SetupUnrealModules()` already
   consumes `source->Views()` unchanged.
2. `Signatures.cpp` already calls
   `SinglePassScanner::start_scan(map, config.ModuleSource->ReadableRanges())`
   on non-Windows — the shadow ranges slot in unchanged (the owned-range
   contract: no match crosses a range boundary — matches the shadow's
   per-segment scan exactly).
3. `ScanGame()`: keep SHAPE A seed gate; add a `shadow_arm()` stage after
   `SeedArming::consume_required_slot_seeds`. Required slots stay
   seed-authoritative with a shadow CROSS-CHECK (agreement logs confidence;
   disagreement = loud refusal, never silent override — all four proven
   seeds cross-check green today). Optional slots currently "unbound by
   design" (fname_tostring, gnatives, fuobject_hash_tables_get,
   console_manager_singleton) become shadow-resolvable with their existing
   fallbacks intact.
4. Match-address bifurcation contract: on PC one pointer is both live
   address and readable bytes; on PS5 they split. Handlers get
   `live(container)` = linked+slide (store/call target) and
   `bytes(container)` = shadow pointer (operand decode). Data-seed handlers
   use the rip recipe from §1.4.
5. Guardrails: sha256 identity gate against `SeedTable::kImageSha256`
   before any scan (game update → loud refusal); slide from the bridge's
   same-capture geometry; address-class rules travel with the hit (LOAD0 hit
   → exec-only-call-only; LOAD7 hit → data-readable-under-lease); LOAD8
   stays excluded until its runtime mapping is proven.
6. PRX sourcing: read the decrypted eboot once at init via the PRX file core
   (sceKernelOpen/Read and NID-routed fopen are both proven live by bridge
   probes); ~143 MiB default or ~116 MiB text-only; optional zero-copy mmap
   variant where the sandbox allows it.

## 4. Deliverables in this directory

- `shadow_copy.hpp` / `shadow_copy.cpp` — the scanner (dual-target
  C++17, no OS deps in scan path; -Wall -Wextra -Werror clean).
- `host_test/host_test.cpp`, `host_test/Makefile` — 35-check Mac
  verification (`make run`, `make asan`); `host_test/run-output.txt` —
  captured passing run.
- `INTEGRATION.md` — the UE4SS runtime integration design.

## 5. Honest limits

- Host verification only: the shadow's PRX-side build, file read, sha gate,
  and ScanGame arming are designed but not console-executed. In-game seed
  arming (Phase F) remains pending exactly as the profile records.
- Sig quality is per-image: the SCO 16-byte prologue is NOT unique (22,879
  file-wide hits); uniqueness requires the 25-byte sig. Any future sig must
  be uniqueness-audited against the pinned image the same way (the scanner
  reports full counts, making the audit mechanical).
- GUA sig C's extra same-shape hits resolve to other labels; multi-hit sigs
  must disambiguate by resolved target (or the audit must lengthen the sig).
- The slide itself is validated as 0x400000 on observed boots; the code
  takes it as a parameter precisely because it is per-boot, not a constant.
