# SLOTS-FEASIBILITY — can the REQUIRED ps_scan slots {0,2,3,4,6} resolve on the exec-only-text target?

Worker: worker-required-slots-feasibility, 2026-10-04. PROPOSALS ONLY — no frozen
or wiring file was modified; every diff below is a proposal for the controller.
Static source reading plus one local read-only measurement of the already-retained
`decrypted-eboot.bin` (sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,
194,241,424 bytes). No console contact, no text-read probes, no nested subagents.
Every claim cites `file:line`. Workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what` is abbreviated `W:`.
Consumed-copy provenance follows worker-tick-witness-design
(`W:outputs/2026-10-04-zcode-team/worker-composition-4-fix/build_runtime_composition4_fixed.py:27,75,94`):
the scan-source stack is the `ue4ss-native-tick-file-owner-1` copies compiled with
`-I ue4ss-native-ps-scan-provider-1/include`; the staged initializer is
`W:outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp`
(its :630 "PS scan timed out" throw matches the B4 premise).

---

## 0. Verdict up front

**The whole ps_scan stage is structurally dead on this target.** The required
slots {0,2,3,4,6} (`MappedRequiredSlots = 0x5d`,
`W:.superpowers/native-dependency-spike/ue4ss-native-mapped-candidate-selection-1/include/OwnedMappedResolverCollector.hpp:25`)
cannot resolve, and the stage cannot even run to completion — five stacked
gates, any one fatal:

- **G0 (facts):** `mapped.text_base` is never supplied and stays 0
  (`W:.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp:226-227`;
  contract comment
  `tick-file-owner-1/include/lmf_to_native_loader_facts.hpp:46-50`).
- **G1 (construction):** the consumed `OwnedMappedScanSource` ctor eagerly builds
  an `OwnedMappedImageCaller`
  (`tick-file-owner-1/src/OwnedMappedScanSource.cpp:10`; provider copy
  `W:.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/src/OwnedMappedScanSource.cpp:6-7`),
  which throws `"Mapped text base must be registered readable executable bytes"`
  unless `permits_code(text_base)` holds
  (`ps-scan-provider-1/include/OwnedMappedImageCaller.hpp:57`; `permits` returns
  false for address 0 at `:33`). The bridge registers readable spans with
  `PLH::R` only
  (`W:outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r11.cpp:483`;
  "Exec-only text is NOT registered readable" `:449-450`), and on this target no
  readable span is executable (measured below). So in the consumed composition
  `MakeNativeProgramContext`
  (`runtime_start_bridge_r11.cpp:623-624`) throws at Stage 2 → `MRS_ERR_CONTEXT`
  (`:658-661`) — **the scan is never reached**.
- **G2 (Rust view gate):** even with a supplied text_base and a forged X flag,
  `MappedImage::new` requires an EXECUTE-flagged view segment containing
  text_base
  (`W:.superpowers/native-dependency-spike/ue4ss-native-mapped-candidates-1/src/mapped_image.rs:48-52`),
  else every `ps_resolve_mapped_core_v1` call returns 2 = InvalidImage
  (`mapped_api.rs:46`, mapped to `image_error` in
  `ue4ss-native-mapped-candidate-selection-1/src/OwnedMappedResolverCollector.cpp:57`).
- **G3 (evidence — measured):** the required-slot anchors either do not exist in
  the pinned image at all or live in the unreadable span; every evidence step
  beyond a data anchor reads code bytes (`xrefs.rs:12-23`, disassemblers,
  `rip4`). Detail per slot in §2.
- **G4 (admission):** code-result slots 2 (fname ctor) and 4
  (StaticConstructObjectInternal) require `image.permits_code(result)` — a
  readable+executable span covering the result
  (`OwnedMappedResolverCollector.cpp:51`) — impossible on this target even for a
  perfectly correct constant. Slot 0/3 (data results) use
  `permits_pointer_slot` (readable+aligned, `:51`,
  `OwnedMappedImageCaller.hpp:71-73`) — that check alone is code-read-free.

Therefore the answer to the task question is **NO — the required slots can never
resolve through ps_scan on this target; the stage is dead structurally, not just
configurationally.** A "working ScanGame" here means one that completes without
ps_scan succeeding; §5 proposes exactly that and it is small.

---

## 1. Measured target geometry (corrects the tick-witness table)

Local read-only parse (script `offline_anchor_scan.py`, output
`anchor-measurement.json`, both in this directory). PT_LOAD table:

| i | file off | linked vaddr | filesz | static flags | live span (base 0x400000) | live (trusted records) |
|---|----------|--------------|--------|--------------|---------------------------|------------------------|
| 0 | 0x0004000 | 0x0000000 | 0x7479b1c (116.48 MiB) | **--X** | [0x400000, 0x7879b1c) | exec-only, unreadable |
| 1 | 0x7480000 | 0x747c000 | 0x0a3e918 (10.24 MiB) | **R--** | [0x787c000, 0x82ba918) | readable |
| 2 | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 (18.49 MiB) | RW- | [0x82bc000, 0x953a4e8) | readable |
| 3 | 0x9140000 | 0x913c000 | 0x1cc5f2 (1.80 MiB) | RW- | [0x953c000, 0x97085f2) | readable |
| 4 | 0x930f420 | 0x9633420 | 0x262cea8 (38.18 MiB) | --- | [0x9a33420, 0xc0602c8) | readable (bss-like) |

**Correction:** worker-tick-witness-design §2 lists LOAD0 as R-- and LOAD1 as
--X; the bytes say LOAD0 is --X and LOAD1 is R--. The trusted live records
(exec-only below 0x787c000 live, readable anchors ≥ 0x7c26782 live) match the
measured flags exactly: **the unreadable span IS the whole 116.48 MiB code
segment, and the readable window starts exactly at static-LOAD1.** The
tick-witness conclusions are unaffected (it fails under both readings), but this
correction matters here: all game code is unreadable; a large rodata/data window
is readable.

---

## 2. Task 1 — per required slot: what it scans, where the anchor lives, who consumes it

Slot↔id↔field mapping (authoritative):
`OwnedMappedResolverCollector.cpp:39` `provider_slots[]{6,0,2,1,3,4,10}` with
ids 1..7 (`:43-46`); Rust dispatch
`mapped-candidates-1/src/mapped_api.rs:47-51`; field clears/writes
`OwnedMappedResolverCollector.cpp:24-31,86-104`. Required mask bits {0,2,3,4,6}
(`hpp:25`), `succeeded()` ignores everything else (`hpp:23`).

### Slot 0 — `g_uobject_array` (resolver id 2)
- **Scans:** six rip-relative instruction-idiom patterns, two `mov edi, imm32`
  "linux" patterns, one three-capture stat-update pattern
  (`guobject_array.rs:10-16,32,47,52`); every candidate is extracted by reading
  the matched code bytes (`rip4`, `:96,61`; captures `:70-72`).
  Fallback `resolve_guobject_array` → `object_cfg::resolve_owned_file`
  (`guobject_array.rs:107-122`); the mapped-arm equivalent is
  `ps_guobject_array_cfg_mapped_v1` (`mapped_candidates.rs:22-27`) which needs
  an X-flagged view span to decode (`object_cfg.rs` `from_mapped` requires at
  least one executable span; `executable(ip)` decodes bytes) plus the two GC
  diagnostic strings (ALLOC `object_cfg.rs:8`, FREE `:9`).
- **Anchor location:** CODE only for the patterns (measured: **0/9 occur
  anywhere in the pinned 185.2 MiB image** — not an unreadability problem; the
  idioms do not exist in this clang/PS5 build). The diagnostic strings are
  split: ALLOC readable (linked 0x7a16c9a → live 0x7e16c9a), FREE unreadable
  (linked 0x77d3d0c → live 0x7bd3d0c) — and the CFG step reads code regardless.
- **Consumer:** `UObjectArray::SetupGUObjectArrayAddress(results.g_uobject_array)`
  (staged `UnrealInitializer.cpp:893-895`; upstream
  `W:.superpowers/native-dependency-spike/ue4ss-unreal-source-access-1/upstream/src/UObjectArray.cpp:344`)
  — the UObject iteration/GC foundation of the whole runtime.
- **Admission:** `permits_pointer_slot` (data) — `collector cpp:51`.

### Slot 2 — `fname_ctor_wchar` (resolver id 3)
- **Scans (ELF):** five UTF-16 module-name strings `"\0Engine\0"`,
  `"\0Renderer\0"`, `"\0AnimGraphRuntime\0"`, `"\0Landscape\0"`,
  `"\0RenderCore\0"` (`fname.rs:16-22`) → `scan_xrefs` over code (`:32`;
  `xrefs.rs:12-23` scans `lea reg,[rip+disp]` encodings plus pointer-byte
  patterns) → root-function groups intersected across ALL five (`:42-49`) →
  code pattern `"ba 01 00 00 00 e8 | ?? ?? ?? ??"` scanned inside the winning
  function (`:54,78-86`). Candidate endpoint
  `ps_fname_ctor_call_mapped_v1` (`mapped_candidates.rs:28-37`,
  `fname_call.rs:36-52`) similarly decodes instruction streams in X-flagged
  spans.
- **Anchor location:** DATA head + CODE body. Measured: Engine (live
  0x7c2706c), AnimGraphRuntime (0x79eefbe), Landscape (0x7a3151c) unreadable;
  only Renderer (0x7d9db10) and RenderCore (0x7df02e0) readable → the five-group
  intersection is impossible at runtime. All 8 raw occurrences of the code
  pattern sit in the unreadable text (linked ≤ 0x1650 + LOAD0; measured).
- **Consumer:** `FName::ConstructorInternal.assign_address`
  (staged `UnrealInitializer.cpp:885-888`) — FName creation hooks.
- **Admission:** `permits_code(result)` (`collector cpp:51`) — G4.

### Slot 3 — `gmalloc` (resolver id 5)
- **Scans:** 14 instruction-idiom patterns → `rip4` (`gmalloc.rs:22-37,39-43`);
  ELF string route: UTF-8 `"/proc/meminfo\0"` and
  `"Refusing to run with the root privileges.\n\0"` → xrefs → root functions →
  xcalls → disassembly (`gmalloc.rs:152-213`). Candidate endpoint
  `ps_gmalloc_deletefile_candidate_mapped_v1` (`mapped_candidates.rs:38-49`) →
  UTF-16 `"DeleteFile %s\0"` anchor then code-window decoding
  (`deletefile_candidate.rs:61-85`).
- **Anchor location:** CODE for the patterns (measured: **0/14 occur anywhere**).
  UTF-8 string anchors: **absent from the image entirely** (0 hits). The
  deletefile UTF-16 anchor IS readable (linked 0x79202d6 → live 0x7d202d6) but
  its xref/decode steps read code — dead.
- **Consumer:** `GMalloc = std::bit_cast<FMalloc**>(results.gmalloc)`
  (staged `UnrealInitializer.cpp:889-892`; consumed at upstream
  `upstream/src/UnrealInitializer.cpp:268-273` where a null GMalloc throws
  `"PostInitialize: GMalloc is uninitialized."`).
- **Admission:** `permits_pointer_slot` (data) — `collector cpp:51`.

### Slot 4 — `static_construct_object_internal` (resolver id 6)
- **Scans:** four call-site patterns (`static_construct_object.rs:24-41`) →
  `rip4` (`:45-47`); ELF string route: UTF-16
  `"NewObject with empty name can't be used to create default"` → `scan_xrefs`
  → disassembly of the first 130 bytes → paired `call` extraction
  (`:49-95`).
- **Anchor location:** CODE patterns — measured **0/4 occur anywhere**. The
  string anchor IS readable (linked 0x7999084 → live 0x7d99084) but the xref
  and disassembly steps read code bytes — dead at runtime.
- **Consumer:** `UObjectGlobals::SetupStaticConstructObjectInternalAddress`
  (staged `UnrealInitializer.cpp:812-815`; upstream
  `upstream/src/UObjectGlobals.cpp:23`).
- **Admission:** `permits_code(result)` (`collector cpp:51`) — G4.

### Slot 6 — `engine_version` (resolver id 1)
- **Scans:** (a) UTF-16 version strings `"d+.d+."` after NUL
  (`engine_version.rs:216-242`), (b) `mov rm32, imm32` store scan with
  iced-x86 disassembly of unreadable code and its successors
  (`:115-191`), (c) 37 code VERSION_PATTERNS (`:53-89,193-210`), (d) UTF-16
  branch names `"++UE4+Release-"` etc. (`:244-261`); precedence `:268-284`.
- **Anchor location (measured):** 1 raw VERSION_PATTERN match (VER#28) at
  linked 0x1ef884 → live 0x5ef884, unreadable (and its semantics need
  disassembly). 42 UTF-16 `"d.d."` regex hits; the only two in the readable
  window (live 0x9514d98, 0x96467fc) are false positives (character tables —
  dumped in §4 of the script run), and none of the unreadable ones
  (e.g. `1.0.0`, `236`, `60000fps`, GUID hex) parses as a plausible
  FEngineVersion (`plausible()`, `engine_version.rs:107-113`; `'-CL-'`/`+`
  requirement `:229-233`). Branch names: **0 hits image-wide** (both UTF-16 and
  UTF-8 measured). Store scan needs code disassembly — dead.
- **Consumer:** `Version::Major/Minor` (staged `UnrealInitializer.cpp:718-722`)
  → `InitializeVersionedContainer` (`:857`) → the Prospero
  `PublishStagedNativeLayoutMaps` publication and the S2 layout gate
  (`:863-876`).
- **Admission:** none needed (no address; `collector cpp:49,51` short-circuit).

---

## 3. Task 2 — non-code-reading routes per slot (the honest answers)

The tick's vtable fallback (staged `UnrealInitializer.cpp:1352-1364`, vtable
preference `:1365-1378`, lookup via live-object vtable pointer +
`ASM::resolve_function_address_from_potential_jmp`) works because UGameEngine is
a C++ object with a vtable in serviced memory. Analogues for the required slots:

| slot | vtable/exports route? | data-relative route? | honest answer |
|---|---|---|---|
| 0 GUObjectArray | No — plain global, no vtable. `ModuleSnapshot::FindExport` is unconfigured on this target and throws (`tick-file-owner-1/include/lmf_to_native_loader_facts.hpp:63-66`); PS5 games do not export these symbols | The RESULT is data; only the EVIDENCE needs code. A staged constant passes the code-read-free `permits_pointer_slot` admission | code-bytes-required **for ps_scan**; **constant-resolvable** (data slot) |
| 2 FName ctor | No — non-virtual free function/ctor | None: no data-side evidence identifies a code address | **code-bytes-required**, full stop; a constant must bypass mapped admission (G4) |
| 3 GMalloc | No — global `FMalloc*` variable | The pointer slot is data and live: `[addr]` holds a running FMalloc*. Data-side sanity probes (non-null, aligned, heap-range) are possible but are not identity proof | code-bytes-required **for ps_scan** (0/14 patterns, strings absent); **constant-resolvable** (data slot) with weak data-side validation |
| 4 StaticConstructObjectInternal | No — non-virtual | None | **code-bytes-required**; constant must bypass G4 |
| 6 engine_version | n/a — not an address | **YES: settings.** `[EngineVersionOverride] MajorVersion/MinorVersion` sets `Version::Major/Minor` directly and installs a no-op `version_finder` ScanOverride (`W:.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/src/UE4SSProgram.cpp:857-879`; ini keys `ue4ss-native-settings-defaults-1/src/SettingsManager.cpp:157-159`, fields `include/SettingsManager.hpp:54-60`), so `config.engine_version=false` (staged `UnrealInitializer.cpp:590`) removes bit 6 from `requested_mask` and thus from the required mask (`collector cpp:23,85`) | **resolvable without any scan** |

Reflection-based resolution: on Prospero the container/virtual-offset layer is
fed by `PublishStagedNativeLayoutMaps` from the validated profile — but that
publication happens *after* the required slots are consumed; it cannot feed
them. No reflection route exists upstream of ScanGame.

---

## 4. Task 3 — the resolution space, ranked by honesty and blast radius

### (a) Pattern re-derivation against the retained eboot offline — RANK 3, dead for the shipped resolver set
We hold the image, so patterns *can* be located offline — but the measurement
closes this route for the shipped resolvers:

- 27 instruction patterns across slots 0/3/4 have **zero occurrences** in the
  pinned image (`anchor-measurement.json`: guobjectarray 0/9, gmalloc 0/14,
  static_construct 0/4). These are upstream idioms (MSVC-flavored/other-title)
  that this clang/PS5 build simply does not contain. There is nothing to
  "re-derive" — the anchors do not exist.
- The string-anchored routes (FName, GMalloc-ELF, StaticConstruct) fail at their
  second step even when the string is readable: `scan_xrefs` scans code bytes
  (`xrefs.rs:12-23`) and every route then disassembles code. Readable data
  anchors measured: `"DeleteFile %s\0"` live 0x7d202d6, `"NewObject with empty
  name…"` live 0x7d99084, GUO-ALLOC live 0x7e16c9a — all stranded.
- The per-slot "what the runtime would then need is only the RESOLVED ADDRESS
  validation without reading code" idea is REAL for slots 0/3 (data results;
  `permits_pointer_slot` is exactly such a validation — alignment + readable 8
  bytes, `OwnedMappedImageCaller.hpp:71-73`), but slots 2/4 still die at G4, and
  ps_scan never runs anyway (G1).

Deeper offline RE of the pinned image *can* derive the four constants (we hold
the decrypted text!) — but that output is not a pattern fix, it is the input to
(b). Honest classification: (a) is dead as a runtime route; its useful residue
is offline constant derivation.

### (b) Staged address tables (offline-resolved constants + data-side validation) — RANK 1, the recommended core
- **Slots 0/3:** constants are data addresses; validation without code reads =
  the existing `permits_pointer_slot` semantics (readable + 8-aligned), plus
  optional bounded data probes (slot 3: `[addr]` non-null aligned pointer at
  scan time; slot 0: FUObjectArray header plausibility). Admission-grade, not
  identity-grade — must be labeled exactly like the existing candidate
  provenance strings ("identity and ABI unverified",
  `tick-file-owner-1/src/OwnedMappedScanSource.cpp:57-60`).
- **Slots 2/4:** constants are code addresses; NO data-side validation exists
  (reading them = code read). They must flow around the mapped pipeline's
  `permits_code` admission (G4) — see the seed diff below, which assigns them
  directly through the existing consumer call sites.
- **Slot 6:** settings route (§3), no constant needed.
- **Evidence standard (binding):** every constant is derived offline against the
  sha-pinned `decrypted-eboot.bin` (`3b6cfad4…`, also recorded as the profile's
  `build_id_source`, `W:outputs/2026-10-04-zcode-team/worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json:18-24`);
  the live base 0x400000 is proven by the in-game anchor
  (worker-mapping-recovery records); derivation notes (anchor bytes, rip
  arithmetic, uniqueness argument) are delivered next to the constants; nothing
  is guessed — an under-determined slot stays unseeded and blocks startup loudly
  rather than shipping a maybe.

**Exact proposed diff set (proposals only; all in the already-staged
initializer, which the composition replaces via the existing
`--replace-runtime-unit` mechanism,
`build_runtime_composition4_fixed.py:41,77-84`):**

```cpp
// staged UnrealInitializer.cpp, ScanGame(), after `PsScanResults results{};` (:607)
#ifdef __PROSPERO__
    // REQUIRED-SLOT SEEDS (proposal, worker-required-slots-feasibility):
    // offline-derived against decrypted-eboot.bin sha256 3b6cfad4… ; live base
    // 0x400000 proven by the in-game anchor. Identity/ABI UNVERIFIED — same
    // honesty class as the mapped candidate provenance strings.
    constexpr bool kSeedRequiredSlots = true;   // flip via settings later if desired
    if constexpr (kSeedRequiredSlots) {
        results.g_uobject_array                = reinterpret_cast<void*>(kSeedGUObjectArray);                // data addr
        results.gmalloc                        = reinterpret_cast<void*>(kSeedGMallocSlot);                  // data addr
        results.fname_ctor_wchar               = reinterpret_cast<void*>(kSeedFNameCtor);                    // code addr
        results.static_construct_object_internal = reinterpret_cast<void*>(kSeedStaticConstructObject);      // code addr
        // version: NOT seeded here; EngineVersionOverride settings route owns it.
    }
#endif
```

```cpp
// same file: gate the ps_scan arm so unseeded/unrequested slots are not required
// and a dead scan cannot time out the loop (:610-633). Proposal shape:
#if defined(__PROSPERO__)
    if constexpr (kSeedRequiredSlots) {
        config.g_uobject_array = false;              // was !ScanOverrides.guobjectarray (:585)
        config.fname_ctor_wchar = false;             // (:587)
        config.gmalloc = false;                      // (:588)
        config.static_construct_object_internal = false; // (:589)
    }
#endif
```

```cpp
// same file: the two Lua-override DoScan arms would otherwise run AOB scans that
// read code and fatal (:823-829, :906-915). Gate them on the seeds:
    if ((!ctx.config.fname_tostring_fstring ||
         !ctx.config.static_construct_object_internal ||
         !ctx.config.gameengine_tick)
#ifdef __PROSPERO__
        && !kSeedRequiredSlots   // seeds already assigned static_construct at :814's consumer
#endif
    ) { /* DoScan(ScanForGameFunctionsAndData) */ }
```

Important mechanical detail that makes the seeds survive the scan loop:
`unavailable_owned_mapped` clears only REQUESTED fields
(`OwnedMappedResolverCollector.cpp:24-29`) and `pointer()` writes only requested
slots (`:86-89`), so with the four config flags false the seeded
`results` fields are retained across every ps_scan retry and into the consumer
assignments (`:887-895`). With slot 6 handled by settings, the required mask
becomes empty regardless of the scan outcome.

Also proposed (diagnostics only): a one-line startup guard for the two data
seeds mirroring `permits_pointer_slot` (alignment + a bounded readable probe via
the already-registered lease), logged, never fatal beyond today's behavior.

### (c) Composition-level slot policy change — RANK 2, necessary companion, not sufficient alone
- `ScanOverrides` lambdas for the four slots (set by `setup_lua_scan_overrides`,
  `UE4SSProgram.cpp:883`) flip `config.X=false` (staged `:585-594`) — that alone
  removes the slots from the required mask but triggers the Lua DoScan arms
  (`:823-829`, `:906-915`) whose AOB containers read code and fatal after
  `SecondsToScanBeforeGivingUp` (`:685-713`, throw at `:656-658`), and it leaves
  the consumers assigning NULL (`:887-895`) → downstream nulls (GUObjectArray
  null, `GMalloc` null → the `:268-273` throw). Rejected standalone.
- Zeroing `MappedRequiredSlots` (`hpp:25`) is strictly worse: it declares
  empty-handed scans successful. Rejected.
- The collector also runs all 7 endpoints unconditionally
  (`collector cpp:41-46`), so while the view lacks an X segment
  (`mapped_image.rs:48-52`) every pass sets `image_error` (`cpp:56-57`) and
  `succeeded()` stays false (`hpp:23`) — no config-only change fixes that; the
  seed route's `config.X=false` does not touch `image_error`, so the ps_scan arm
  must be skipped or its failure tolerated when seeds are armed (hence the
  second diff above).
- Note G1 is orthogonal: in the consumed composition the Stage-2 ctor throw
  (`OwnedMappedImageCaller.hpp:57`) fires before any of this; the seed path's
  Stage-2 relevance is that it no longer depends on the scan source succeeding.

---

## 5. Task 4 — verdict table and the single recommended path

| slot | what it needs | resolvable-without-code-reads | via |
|---|---|---|---|
| 0 GUObjectArray | data address (FUObjectArray) | **constant-only: YES** / via ps_scan: NO | staged constant + `permits_pointer_slot`-class guard (data slot; measured 0/9 anchors) |
| 2 FName ctor | code address | **NO** (no data-side evidence class; G4 admission) | staged constant assigned directly at the consumer (`:887`), bypassing mapped admission |
| 3 GMalloc | data pointer slot (FMalloc\*) | **constant-only: YES** / via ps_scan: NO | staged constant + data probe (`[addr]` live pointer); measured 0/14 anchors, strings absent |
| 4 StaticConstructObjectInternal | code address | **NO** | staged constant assigned at the consumer (`:814`) |
| 6 engine_version | major/minor numbers | **YES — settings, zero scans** | `[EngineVersionOverride] Major=5 Minor=6` (`UE4SSProgram.cpp:857-879`; profile pins 5.6, `townfall-ps5-ue56.profile.json:13-14,31-37`) |
| 10 tick (optional, for completeness) | code address | YES — existing vtable fallback | staged `UnrealInitializer.cpp:1352-1364` (scan miss → vtable), already proven route B in TICK-WITNESS-DESIGN §6 |

**The single recommended path to a working ScanGame on this target** (full diff
set, all proposal-grade):

1. **Settings:** `UE4SS-settings.ini` → `[EngineVersionOverride]` `MajorVersion=5`,
   `MinorVersion=6` (zero diffs; kills the slot-6 requirement; the no-op
   `version_finder` override is installed by the existing code).
2. **Staged initializer (one unit, existing `--replace-runtime-unit` path):**
   the three seed diffs of §4(b) — constant table + config gates + DoScan-arm
   gates. No runtime code-byte read is introduced; the only runtime validation
   added is the optional data-probe guard for slots 0/3.
3. **Offline derivation task (bounded, new):** derive the four constants from
   the retained decrypted image with recorded derivations; deliver under the
   evidence standard of §4(b). Until a constant is derived, that seed stays
   absent and startup fails loudly exactly as today.
4. **Tick:** no diff — `bHookEngineTick` + existing vtable fallback
   (`:1352-1364`) once PostInitialize is reachable.
5. **Regression/acceptance:** the ps_scan arm's new "seeds armed, scan skipped"
   path needs its own Editor/PlayMode checks plus the existing query-harness
   layout gate (`:876`) as the downstream correctness witness; candidate
   addresses remain labeled identity-unverified (provenance strings,
   `OwnedMappedScanSource.cpp:57-60`) until behavioral evidence accrues.

Blast radius: one staged initializer unit + one ini file; no ABI, no Rust crate,
no frozen collector/caller changes, no new provider generations. The failure
mode without the constants is unchanged (loud early throw), never a wrong-but-silent
address.

---

## 6. Explicit non-claims

- No console contact, no text-read probes; the eboot measurement is a local
  read-only parse of an already-retained artifact (script + JSON in this
  directory).
- No constants are derived or claimed correct; the seed VALUES in §4 are
  placeholders for the proposed derivation task.
- The "exec-only/unreadable" premise rests on the trusted live records
  (r8:113-119, igf6) and is consistent with the measured static `--X` LOAD0;
  nothing here re-proves it.
- No claim about which exact failure shape the current binary exhibits at
  runtime (G1 predicts Stage-2 `MRS_ERR_CONTEXT` in the consumed composition;
  the B4 timeout shape presumes G0-G2 passed) — both are recorded, the verdict
  is identical under either.
- All diffs are proposals; no frozen file, ABI, OwnedMemory unit, or Rust crate
  was modified.
