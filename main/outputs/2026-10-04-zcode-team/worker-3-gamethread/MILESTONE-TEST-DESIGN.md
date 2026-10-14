# MILESTONE-TEST-DESIGN — harmless engine/reflection query on the REAL game thread

Owner's milestone ladder (NEXT_ACTIONS.txt item 4): "Deliver real game-thread
calls before starting mods... First prove a harmless engine query and stable
object/reflection access." This document defines that test precisely, what
counts as PASS, what explicitly does NOT count, and the preconditions owed by
worker-1 (live-module facts) and worker-2 (validated UE5.6 layout profile).

## 1. Query candidate(s)

### Primary (must pass): read-only GUObjectArray element count on the game thread

One query, delivered through the EXISTING serialized Tick/ProcessEvent route
(INTEGRATION-MAP.md Stages 2-5):

1. The real `UGameEngine::Tick` hook fires (resolved from the live image, or
   via the serialized-ELF witness route under
   `CandidateSelectionPolicy::SelectUniqueAfterNoMatch` after a genuine
   original no-match).
2. On that callback's thread, the handler first records
   `RC::Unreal::IsInGameThreadRaw()` and `GetGameThreadIdRaw()` — the identity
   must be `Publication::Published` by this same first genuine Tick
   (`NativeGameThreadIdentity.hpp`).
3. The query body executes `UObjectArray::GetNumElements()` — a scalar,
   read-only access through the validated GUObjectArray facts (worker-2) and
   the live mapped image (worker-1). No writes, no spawning, no Lua mod code,
   no console/file mutation, no actor access.
4. The handler emits ONE structured log record (UE4SS writer device) with:
   engine major.minor actually used, the GUObjectArray element count, the
   published game-thread id, and the count read again on a second consecutive
   Tick (stability: two reads from consecutive Ticks, both on the published
   identity, no crash, no hang).

Why this candidate: it exercises every seam end-to-end (live scan source ->
engine address -> validated layout -> game-thread dispatch -> reflection
read) while being genuinely harmless, and its result is externally observable
without modifying any game state.

### Secondary (optional, same run): one FName conversion roundtrip

Convert one engine-supplied FName (e.g. from a known engine class name) to
string through the PRESERVED engine-owned Kismet `Conv_NameToString` fallback
(`FNameToStringMethod::Conv_NameToString`) and log the roundtrip equality.
This additionally validates the FName explicit-output ABI correction
(`void fn(ptr output, ptr text, i32 mode)`). Non-blocking: the old
SkySphereMesh anchor absence is saved and optional (mask 0x5d).

## 2. Required PASS evidence (what must be observed, from where)

All of the following, from the REAL Townfall process, collected by the
controller through the existing console evidence path (exact-build JSONL /
log capture; no re-dispatch of frozen historical artifacts):

1. **Game-thread identity**: the log record shows the handler ran on the
   thread published by the first genuine Tick; `IsInGameThreadRaw()==true` in
   the record; a pre-publication `GetGameThreadId()` attempt threw the honest
   "not yet initialized by UGameEngine::Tick" error (proves the guard was
   live, not stubbed).
2. **Affinity refusal evidence**: at least one dispatch attempt from a
   non-game thread (e.g. the init/update thread) was REFUSED by the existing
   ProcessEvent/action dispatch (unknown/other-thread refusal) and appears in
   the log or a debug counter — proving the dispatch guard, not just the
   happy path.
3. **Real reflection read**: element count `> 10000` (the initializer's own
   post-construction threshold, UnrealInitializer.cpp wait loop) in BOTH
   consecutive-Tick records; counts are plausible and stable (equal or
   monotonically non-decreasing).
4. **Version/layout facts used**: the log shows `Using engine version: 5.6`
   from the real version scan result, and the layout pair was published via
   `PublishStagedNativeLayoutMaps(5, 6, <resolved axes>)` with the staged
   identity agreeing (no `Native layout declarations disagree` refusal
   suppressed).
5. **Lifetime preserved**: cooperative stop after the query:
   `RuntimeOwner::stop()` from a loader/control thread returns true, joins,
   status reaches Stopped/Failed WITHOUT hanging; no crash during the
   following engine ticks (bounded observation window). The FName forwarding
   hook retirement/retention behavior is unchanged.
6. **Provenance chain**: the record's engine addresses (GUObjectArray, Tick)
   trace to the live-image scan (or the witness route with explicit
   provenance `TickSerializedElfWitness` and the original no-match recorded),
   and the used layout entries carry validated-profile origin, bounded width
   and confidence >= threshold — i.e. the two seam resolutions from the
   adapter contracts, not defaults.

## 3. Explicit NON-counting evidence (honesty bar)

None of the following may be reported as this milestone, alone or combined:

- Any host fixture, host harness, or host callback run (including this
  worker's seam_host_test and the 81-assertion tick-file-owner host suite).
- A guessed/hardcoded layout, PDB default, or an offset copied from another
  game/firmware; a caller-supplied boolean that "enables" engine facts.
- A successful `UE4SSProgram` constructor run (already proven on console,
  checkpoint `native_program_construction_console` — construction is NOT
  engine-bound startup).
- Plain Lua arithmetic, a log line from our own thread, init-array
  constructors (232 callbacks), or a compiled/loaded PRX/ELF.
- A static file-scan candidate (all `*_file_image` endpoints, status-0 file
  labels) — file candidates are not callable runtime functions.
- The serialized Tick witness composed object (117 MB .o) or any link/compose
  success; also the standalone DLL diagnostics (f72faea5, 6d295f56) —
  different payloads, explicitly not UE integration evidence.
- Item Spawner's unchanged host startup (its F7 failure:
  `ExecuteInGameThread` nil at main.lua:695 remains the marker that a REAL
  game-thread provider is absent; a host scheduling stub would not count).
- EngineInitialized observed from a thread we control (our init worker), or
  any callback delivered on an injected payload worker "pretending" to be the
  game thread (ProgramFactory.hpp's own prohibition).
- A game launch where the module loaded but no structured record of 2.1-2.4
  exists; screenshots or user impressions are not evidence.
- `ps_scan` returning mapped candidates WITHOUT the genuine live owner (the
  file-derived witness is provenance-labeled, bounded, and NOT live-wide
  uniqueness or a callable-ABI claim — by its own contract).

## 4. Preconditions from (a) worker-1 and (b) worker-2

### (a) Live-module facts (SEAM S1) — concretely

- `ModuleInput` with a genuine retained `lifetime` owner (process-lifetime
  retention; loaded-code references capacity 64 honored), `MainExe` image with
  real base/size and readable ranges from the LIVE process.
- `OwnedMappedImageFacts` with `format == PS_MAPPED_IMAGE_ELF64`,
  `text_base`/`image_base` of the live image, and complete `eh_frame`/
  `eh_frame_hdr` extents IF the serialized Tick witness route is needed
  (adapter refuses witness-without-EH).
- Bound `module_file_path`/`game_executable_path` (ValidateNativeProgramContext
  rules), and the mapping-to-path correspondence established by the supplier
  (a path/hash/base claim alone is the pretender pattern the adapter refuses).
- Result: `MakeNativeProgramContext` returns a context whose
  `scanner_source->source_owner() == module_source` so
  `UnrealInitializer::PreInitialize` admission passes on target.

### (b) Validated UE5.6 layout profile (SEAM S2) — concretely

- Member/vtable layout pair staged from the validated profile
  (`StageNativeLayoutFiles`), one loaded lifetime, same-facts retry only.
- Engine identity `major=5, minor=6` (matches the real version scan result)
  and BOTH axes resolved (non_shipping, stats) from detector outputs or
  explicit profile facts — never defaults.
- Required entries present with validated origin, bounded widths
  (`GetAccessorWidth` contract) and confidence >= threshold: at minimum
  `UObject`, `GUObjectArray`, `FName`, `FProperty`, `UFunction`
  (the adapter's `RequiredLayoutEntries()`).
- Controller wiring owed: `PublishStagedNativeLayoutMaps` currently has NO
  production caller (INTEGRATION-MAP.md Seam S2 GAP). It must be invoked with
  the resolved (major, minor, axes) before/at `InitializeVersionedContainer()`
  selecting `NativeProvidedVirtualVC`. This is a wire-up, not a rewrite; the
  refusal semantics (unstaged/axes-unknown/identity-disagreement) stay.

### Shared

- The gate (`GameThreadQueryGate`) stays DISARMED until both seam resolutions
  are ok — on the target this is enforced by the existing admission refusals;
  my host adapters mirror them for plug-in testing.

## 5. What already works and must be preserved (regression list for this test)

1. NameTypes conversion fallback (engine-owned Kismet Conv_NameToString);
   FNameToString/GameEngine::Tick are OPTIONAL scan slots (mask 0x5d).
2. FName explicit-output native constructor ABI
   (`void fn(ptr output, ptr text, i32 mode)`).
3. Initializer retry; FName wait cooperating with shutdown/deadline;
   StartupWaitControl cooperative cancellation (5-minute deadline, original
   retry intervals preserved in poll slices).
4. Cancellation/deadline/affinity guards: RuntimeOwner self-join refusal,
   late-ready terminal-state protection, honest "init returned without a
   stop request" failure, Identity publish/AlreadyCurrent/DifferentThread,
   ProcessEvent dispatch refusing unknown/other threads, no Tick leaves work
   pending. (All exercised on host by seam_host_test against the real
   sources.)
5. Serialized Tick witness route: original report authority verbatim,
   SelectUniqueAfterNoMatch-only admission, single read lease, bounded mapped
   RX candidate only.
6. Activation-owned Program/settings/Lua path: real settings/inventory/Lua
   entry/require/loadfile/dofile, immutable `ue4ss/` logical keys, serialized
   action close, callback retirement destroying callables before return,
   EngineInitialized fired only when not shutting down and after
   fire_unreal_init_for_cpp_mods + setup_unreal_properties + share_lua_functions.

## 6. Next executable test after this milestone

With the query PASS recorded: PlayerController/Pawn lookup on the game thread
plus ONE real reflected call (checkpoint `first_real_milestone`), then the
activation-owned start of one unchanged representative Lua mod (Item Spawner)
through the real `ExecuteInGameThread` provider — each with its own evidence
bar; none of them may be claimed from this milestone's record.
