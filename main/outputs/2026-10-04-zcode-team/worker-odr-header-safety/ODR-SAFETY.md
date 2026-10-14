# ODR-SAFETY.md — composition-6 header-change audit of the composition-4 run-3 unit set

Worker: worker-odr-header-safety · 2026-10-04 · read-only audit; write scope
`outputs/2026-10-04-zcode-team/worker-odr-header-safety/` only.

Baseline: `…/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/`
receipt status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`, 368 units
= 15 freshly compiled + 353 retained (objects pinned from base receipt
`ue4ss-native-notify-cancel-1/build-1`). Every unit entry carries `dependency_file`;
all 368 walks were read (0 unreadable). That is the authoritative per-unit include
closure — the run-3 `.d` files cover only the 15 fresh TUs, the retained TUs'
walks come from their original build receipts.

Headers audited:
- comp6 struct-layout: `Platform/NativeProgramContext.hpp` (port),
  `NativeLoaderContext.hpp` (W/include), `lmf_to_native_loader_facts.hpp` (W/include)
- D2 inline-guard: `PsMappedImageCaller.hpp` (ps-scan-abi-1),
  `OwnedMappedImageCaller.hpp` (ps-scan-provider-1 provider variant; read-lease
  variant is NOT in the composition include graph)

## 1. Complete hit matrix (all 368 units, from dependency walks)

24 units include at least one audited header. No other unit does — including all
remaining 344 retained units (0 hits, safe by construction).

### NativeProgramContext.hpp — 24 units

| Unit | Class in comp6 | .d evidence | Actual use (source grep + map) | Verdict |
|---|---|---|---|---|
| unreal-unrealinitializer | FRESH (affected, `--replace-runtime-unit`) | run-3 .d | constructs no caller; validates context chain | SAFE (recompiled) |
| native-ownedmappedscansource | FRESH (affected) | run-3 .d | **sole construction site of `OwnedMappedImageCaller`** (`OwnedMappedScanSource.cpp:10,15`) | SAFE only if it compiles against the NEW D2 headers (see FINDING-2) |
| native-psscanbinding | FRESH (affected) | run-3 .d | reference use only | SAFE |
| native-ownedmappedcandidateselection | FRESH (affected) | run-3 .d | reference use; emits weak `PsMappedImageCaller::view()`/`OwnedMappedImageCaller::permits_code()` (unchanged bodies) | SAFE |
| native-loadercontext | FRESH (affected) | run-3 .d | defines `MakeNativeProgramContext(NativeLoaderFacts)` **by value**; moves configured fields | SAFE if same header vintage as adapter+bridge (FINDING-1) |
| native-programfactory | FRESH (affected) | run-3 .d | NativeLoaderContext.hpp consumer | SAFE |
| native-ownedserializedelfsource | FRESH (extra) | run-3 .d | `const OwnedMappedImageCaller&` only | SAFE |
| lmf-loader-adapter | FRESH (extra) | run-3 .d | `LmfConversionOptions` by value internally; sret `LmfConversionResult` out | SAFE if same vintage (FINDING-1/3) |
| module-runtime-ack-supplier | FRESH (extra) | run-3 .d | NativeProgramContext.hpp via ue4ss_backend.hpp chain | SAFE |
| ue4ss_backend (bootstrap) | FRESH (bootstrap replace) | run-3 .d | `ue4ss_backend.hpp` includes `Platform/NativeProgramContext.hpp` | SAFE |
| **program** | **REPLACED-in-comp6** (retained-replace allow-list, design §3.2) | run-3 .d (historical) | `m_native_context` by value; UE4SSProgram ctor takes context **by value**; sole odr-user of offset-sensitive inlines (below) | MUST be replaced — the whole audit hinges on this (FINDING-4) |
| cppmod | RETAINED | historical .d via `UE4SSProgram.hpp` | **zero** textual use of NativeProgramContext/Validate/Make (grep: 0 hits) | BENIGN |
| cppmod-base | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| luamod | RETAINED | via UE4SSProgram.hpp | calls inline `native_mod_source()` (LuaMod.cpp:900) → emits weak copy | BENIGN — offset-safe, see §3 invariant |
| settings | RETAINED | via UE4SSProgram.hpp | calls `native_mod_source()` (SettingsManager.cpp:60,112) | BENIGN — offset-safe |
| program-signatures | RETAINED | via UE4SSProgram.hpp | calls `native_mod_source()` (Signatures.cpp:43,70,175) | BENIGN — offset-safe |
| program-object-to-string | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| generator-sdkgenerator-generator | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| generator-sdkgenerator-ueheadergenerator | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| generator-usmapgenerator-generator | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| lua-library | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| csv_data_commands | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| native-jmap | RETAINED | via UE4SSProgram.hpp | zero use | BENIGN |
| native-ownedmappedresolvercollector | RETAINED | via `PsMappedImageCaller.hpp` (which includes `Platform/NativeProgramContext.hpp`) | reference use of `OwnedMappedImageCaller` only; calls unchanged inline `view()/permits_code()/permits_pointer_slot()` (OwnedMappedResolverCollector.cpp:46,51,69) | BENIGN for comp6 layout; see §2 for D2 |

Transitive chains verified: `NativeProgramContext.hpp` is included directly only by
`bootstrap/ue4ss_backend.hpp`, `W/include/ue4ss_backend.hpp`, and
`port/ue4ss/include/UE4SSProgram.hpp` (grep over port + W + notify-cancel trees).
All 10 fresh .d walks and all retained walks resolve the PORT copy
(`native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp`) —
no flat staging of this header exists in the composition include path; the overlay
copy is packaging-only.

### NativeLoaderContext.hpp — 3 units, all FRESH

`native-loadercontext`, `lmf-loader-adapter`, `native-programfactory` (run-3 .d).
**Zero retained units include it** — the design claim "retained units never name
NativeLoaderFacts" is VERIFIED against all 353 retained walks. Cross-TU ABI surface:
`MakeNativeProgramContext(NativeLoaderFacts)` takes the struct BY VALUE (mangled
`_ZN2RC8Platform24MakeNativeProgramContextENS0_17NativeLoaderFactsE`, run-3 map:
emitted only by `native-loadercontext.o`); `TryMakeNativeLoaderFactsFromSupplier`
returns `LmfConversionResult` BY VALUE (sret). Callers live in packaging-time
bridge/entry units (overlay `lifecycle/`, compiled fresh each packaging) and
tests/owner_controls.cpp (not a composition unit).

### lmf_to_native_loader_facts.hpp — 1 unit, FRESH

`lmf-loader-adapter` only. `LmfConversionOptions` gains a bool (appended; size
grows). Passed by const-ref from the bridge; constructed by the bridge by value.
Both bridge and adapter must share one vintage.

### PsMappedImageCaller.hpp / OwnedMappedImageCaller.hpp (D2) — 9 units

8 FRESH (unreal-unrealinitializer, native-ownedmappedscansource,
native-ownedserializedelfsource, native-psscanbinding,
native-ownedmappedcandidateselection, native-loadercontext, native-programfactory,
lmf-loader-adapter) + 1 RETAINED (`native-ownedmappedresolvercollector`).
All 8 fresh walks resolve the frozen satellite copies (ps-scan-abi-1 / ps-scan-provider-1).

## 2. D2 inline-guard audit (Diff B: PsMappedImageCaller ctor `contains({text_base,1})`;
   Diff C: OwnedMappedImageCaller ctor `permits_code` gate; both code-only, NO layout change)

- Construction sites (the only odr-uses of the changed inline ctors): grep over all
  canonical sources finds exactly one in the composition —
  `W/src/OwnedMappedScanSource.cpp:10,15` → unit `native-ownedmappedscansource`
  (FRESH). `native-ownedserializedelfsource`, `native-psscanbinding`,
  `native-ownedmappedcandidateselection`, `unreal-unrealinitializer`,
  `native-loadercontext`, `native-programfactory`, `lmf-loader-adapter`: no
  construction (verified by grep; reference use only).
- RETAINED `native-ownedmappedresolvercollector`: no construction; calls only
  `view()` / `permits_code()` / `permits_pointer_slot()` — inline functions whose
  bodies Diff B/C do NOT touch. Its baked copies remain semantically identical to
  the new ones. Struct layouts unchanged → reference calls safe. **BENIGN.**
- run-3 runtime.map: the changed ctors appear as NO standalone symbols (fully
  inlined into native-ownedmappedscansource.o) — confirming the single bake site.

**FINDING-2 (LOUD — blocks D2's purpose in the current drybuild shape):** the
comp6-drybuild intercept chain (staging-d1 quoted-include override) reaches ONLY
the adapter TU. Dry1 run-2 `.d` evidence: `lmf-loader-adapter.d` resolves
`staging-d1/OwnedMappedImageCaller.hpp` + `staging-d1/PsMappedImageCaller.hpp`,
while ALL 7 other D2 users — including the ctor site `native-ownedmappedscansource.d`
— resolve the FROZEN satellite copies (shas `b4612648…`/`69db72d3…`, still the old
`text_base` gates on disk right now). The drybuild REPORT corroborates: 14/15 fresh
units bit-identical to run-3. Consequence in that shape: adapter compiles the
text_base=0-tolerant conversion (Diff A + Resolution B opt-in), but the ONLY ctor
site keeps the OLD throw — at runtime `MakeNativeProgramContext` →
`OwnedMappedScanSource` ctor throws ("Mapped metadata or spans exceed supplied
readable bytes" / "Mapped text base must be registered readable executable bytes").
Not memory-unsafe, but D2 option (c) silently never takes effect, and the program
contains two divergent definitions of the same inline functions (strict ODR
violation). **REMEDIATION: Diff B/C must land IN PLACE in
`ps-scan-abi-1/include/PsMappedImageCaller.hpp` and
`ps-scan-provider-1/include/OwnedMappedImageCaller.hpp` (with pin/receipt
regeneration), or the interception chain must be extended so the affected-unit argv
resolves the new bytes. An argv-only adapter redirection can never deliver Diff B/C
to the ctor site. Also land Diff C in the read-lease variant
(`ue4ss-native-read-lease-1/include/OwnedMappedImageCaller.hpp`, still old) for
satellite/test builds.**

## 3. comp6 layout-change audit (NativeProgramContext + NativeLoaderFacts)

Insertion point is load-bearing: both structs insert the two new
`std::filesystem::path` fields AFTER `module_file_path`/`game_executable_path`/
`module_source`/`scanner_source`/`mod_source` (NativeLoaderFacts: before `modules`;
NativeProgramContext: after `mod_source`, per design §2.1/§2.2 and the landed
copies). Offsets of everything BEFORE the insertion are unchanged.

- Retained `luamod.o` emits the only copy of inline `UE4SSProgram::native_mod_source()`
  (run-3 map, from `ue4ss-native-notify-cancel-1/build-1/luamod.o`). It reads
  `m_native_context.mod_source` — offset of `m_native_context` within UE4SSProgram
  unchanged (comp6 does not touch UE4SSProgram.hpp members); offset of `mod_source`
  within NativeProgramContext unchanged. **BENIGN, but this is a load-bearing
  invariant: any reorder that shifts `mod_source` (or `m_native_context`) turns
  luamod/settings/signatures into real divergence.** Post-build check: emitter set
  must remain {luamod.o, settings.o, program-signatures.o} with identical bodies
  (all read the same unchanged offsets).
- Offset-SENSITIVE in-class functions of UE4SSProgram (`request_shutdown`,
  `get_debugging_ui`, `can_process_events`, `is_event_loop_thread`,
  `get_event_loop_thread_id`, `are_mods_being_touched`, statics
  `get_current_imgui_*`): swept across port/notify-cancel/jmap/collector trees —
  every odr-use is in `UE4SSProgram.cpp` (the replaced unit). The size growth of
  `m_native_context` shifts post-:127 members ONLY inside code compiled from the
  new header; no retained TU touches them. **CONTAINED by the program replacement.**
- `ValidateNativeProgramContext` (inline, body rewritten by comp6): run-3 map shows
  exactly ONE emitter, `program.o` (weak, 0x4c2); no other TU references it.
  Replacement of `program` swaps in the new body. If `program` were NOT replaced,
  the stale body would hard-require a non-empty `game_executable_path` and
  re-validate with the old rules — startup would throw under Resolution B.
- 12 retained port TUs (cppmod … native-jmap): zero use of the type, zero inline
  calls, zero construction — the header content is baked only as unused class
  definition/debug info. **BENIGN.**
- Bootstrap `ue4ss_backend` + extras (module-runtime-ack-supplier, etc.): FRESH —
  recompiled.

**FINDING-4 (blocking, design already owns it):** `program` MUST be in the comp6
replace set. Without it the composed object has a NEW port header feeding a STALE
program.o: stale `ValidateNativeProgramContext` (single weak emitter = program.o),
stale `setup_paths`/banner derivations, stale by-value UE4SSProgram ctor + stale
by-value context argument sizing in the bridge call. This is the single largest
stale-layout contact in the whole set. Design §3.2 (retained-units-replaced =
{"program"}) covers it; rehearsal evidence exists
(`worker-program-unit-inventory/patched-staged.*`). Note `UE4SSProgram.cpp` on disk
is STILL the old vintage (mtime Oct 3, 0 hits for configured_game_directory) — the
port edit is not landed yet.

## 4. FINDING-1 / FINDING-3 (LOUD — mixed-vintage hazards in the current tree)

The landing is MID-FLIGHT (files changed at 21:41–21:42 during this audit):
- LANDED (new vintage): port `Platform/NativeProgramContext.hpp`
  (sha `0a068bb9…`, exe-optional validator), `W/include/NativeLoaderContext.hpp`
  (`d857bc0d…`, configured roots), `W/include/lmf_to_native_loader_facts.hpp`
  (`1c990d0b…`, option bool), `W/src/lmf_to_native_loader_facts.cpp` (Diff A +
  opt-in), `W/src/NativeLoaderContext.cpp` (configured moves).
- NOT LANDED: `UE4SSProgram.cpp` (old), both satellite D2 headers (old), read-lease
  variant (old). Frozen pins elsewhere still name the OLD vintages
  (`18c76cdc…`/`a5bc3b70…` in the composition-5/dry1 drivers;
  `66499615…`/`b4612648…`/`69db72d3…` in the dry1 overlay pins).

**FINDING-1:** `NativeLoaderFacts` is a cross-TU BY-VALUE parameter and
`LmfConversionResult`/`LmfConversionOptions` cross TU boundaries by value/const-ref
between the ADAPTER unit and the PACKAGING bridge unit. Any build that lets the
adapter TU and the bridge TU resolve DIFFERENT vintages of
`NativeLoaderContext.hpp`/`lmf_to_native_loader_facts.hpp` embeds an sret-size
mismatch (caller-allocated return slot too small → out-of-bounds write) and a
const-ref field-offset mismatch on the options bool. The dry1 PRX candidates
(`35ba028e…` wrapped / `0dede0ee…` bare) are EXACTLY this mixed state: adapter TU =
staging-d1 new headers; bridge TU = overlay old headers
(`18c76cdc…`/`a5bc3b70…` per dry1 overlay-manifest.json). Static gates cannot see
this; **those candidates must never be launched** (dry1 was offline-only,
`target_executed: False` — verified).
**REMEDIATION: one vintage rule — the composition W/include copies, the affected/
extra unit argv, the port tree, and the packaging overlay must ALL resolve the same
bytes in the one-pass build; regenerate every pin that names the old vintages.**

**FINDING-3:** same single-vintage rule applies between `native-loadercontext`
(defines the by-value `MakeNativeProgramContext`) and the adapter/bridge. In dry1
run-2 the adapter resolved staging-d1's new `NativeLoaderContext.hpp` while
`native-loadercontext.d` resolved the OLD W/include copy — i.e. the dry1 composed
object itself contains the by-value parameter mismatch (adapter's internal
LmfConversionResult is new-layout; loadercontext's parameter is old-layout). The
real build must recompile BOTH against the landed W/include vintage (they are both
FRESH units, so this follows automatically once the intercept hack is dropped and
the canonical W/include bytes are the single source).

## 5. Verdict table (summary)

| Group | Units | Class | Verdict |
|---|---|---|---|
| 6 affected units | unreal-unrealinitializer, ownedmappedscansource, psscanbinding, ownedmappedcandidateselection, loadercontext, programfactory | FRESH | SAFE — provided affected argv resolves the NEW header vintages (D2: FINDING-2; lmf/loader-context: FINDING-1/3) |
| program | UE4SSProgram.cpp | REPLACED-in-comp6 | REQUIRED — must join replace set (FINDING-4); port edit still unlanded |
| 8 extras/bootstrap | lmf-loader-adapter, lmf-base-scan, lmf-live-module-facts, lmf-sce-ops, native-query-harness, module-runtime-ack-supplier, runtime_owner, ue4ss_backend | FRESH | SAFE (adapter per FINDING-1/3) |
| 12 retained port TUs | cppmod, cppmod-base, luamod, settings, program-signatures, program-object-to-string, generator-sdkgenerator-generator, generator-sdkgenerator-ueheadergenerator, generator-usmapgenerator-generator, lua-library, csv_data_commands, native-jmap | RETAINED | BENIGN (zero use; luamod/settings/signatures inline use is offset-safe by insertion-point invariant) |
| 1 retained collector | native-ownedmappedresolvercollector | RETAINED | BENIGN (reference-only; unchanged inline bodies; D2 is layout-free) |
| 344 remaining retained | — | RETAINED | 0 header contact — safe by construction |
| dry1 PRX candidates | prx-candidate-composition6-dry1-a/-b | artifact | DO NOT LAUNCH (mixed-vintage sret/const-ref mismatch, FINDING-1) |

## 6. Remediation checklist BEFORE the one-pass build

1. Land `UE4SSProgram.cpp` Resolution B edit (program unit) — currently old vintage.
2. Land Diff B/C IN PLACE in `ps-scan-abi-1/include/PsMappedImageCaller.hpp` and
   `ps-scan-provider-1/include/OwnedMappedImageCaller.hpp` (ctor site
   `native-ownedmappedscansource` resolves these frozen paths; the drybuild
   intercept chain provably does not reach it). Land Diff C in the read-lease
   variant too. Drop the staging-d1 intercept hack (or reduce it to the `#error`
   resolution guards) once in-place lands.
3. One-vintage sweep: regenerate ALL pins naming old header vintages —
   composition driver `DEFAULT_STAGE_HEADERS` (`18c76cdc…`, `a5bc3b70…`), dry1
   overlay pins (`66499615…` port vintage, D2 `b4612648…`/`69db72d3…`), W/include
   glob snapshot pins, `:59` unit-source checks for `NativeLoaderContext.cpp`,
   `--replace-receipt`/`--extra-receipt` file maps, and the port `:51` pin for
   `NativeProgramContext.hpp`. Never bypass a pin; re-pin with new attested hashes.
4. Port copy vs overlay copy sha-equality gate for `Platform/NativeProgramContext.hpp`
   (design ODR gate) — current port `0a068bb9…` must be what packaging stages.
5. Extend the post-build `.d` resolution check from the adapter TU to ALL 15 fresh
   TUs: assert every resolution of the 5 audited headers is the new vintage and
   ZERO frozen/satellite old-vintage paths remain in any fresh `.d`.
6. Post-build symbol checks: `ValidateNativeProgramContext` emitter set =
   {program.o} (new body); `MakeNativeProgramContext` emitters = {native-loadercontext.o};
   `native_mod_source()` emitters = {luamod.o, settings.o, program-signatures.o}
   with unchanged bodies; changed D2 ctors remain absent as standalone symbols
   (inlined into scansource) — then disassemble the scansource ctor to prove the
   `text_base!=0` guard shape.
7. Quarantine the dry1 candidates (see FINDING-1); never execute them on target.
