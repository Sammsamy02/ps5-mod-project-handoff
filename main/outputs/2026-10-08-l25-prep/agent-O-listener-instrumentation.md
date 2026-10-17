# Agent O — L25 items 2/3/4: UObjectDeleteListeners instrumentation, hardening, ABI repair

All edits landed in the l25 overlay tree
`outputs/2026-10-08-l25-runtime/typescan-r4/` as CHANGES.json replacement
entries (see "Where the edits landed and why they reach the build"). No
builds, no console contact, no host suites re-run, no target compile. Brace/
string balance of every edited TU verified mechanically; review by eye; the
parent owns compile/rebuild.

REVISED 2026-10-08 (post agent-Z9 edit-path audit): row set confirmed
complete for every file actually changed; see section 0b for the final row
list, the scope check on ClassListener.hpp / LuaUObject.hpp (NO rows —
unchanged), and the Z9 cross-checks (original-key digests, cross-part
duplicate-original scan, variant-B token spelling, include-universe pins).

## 0. Where the edits landed and why they reach the build (coordinator question)

Mechanism verified from receipts, not assumed:

- The l24 full build consumed the PREVIOUS overlay tree: its
  `outputs/2026-10-08-l24-build-r2/CHANGE-SELECTION.json` is literally the
  l24 tree's `typescan-r3/CHANGES.json` content (original/candidate/hash
  entries), and the l24 `pass1/COMPOSITION-RECEIPT.json` composed
  `unreal-unrealinitializer` FROM `outputs/2026-10-08-l24-runtime/typescan-r3/candidate/src/UnrealInitializer.cpp`
  — i.e. the overlay candidate, not the port root. That is the sanctioned
  replacement channel: the parent rebuild applies each CHANGES entry
  (original → candidate) before compiling the closure.
- `unreal-uobjectarray` and `unreal-flat-classlistener` were composed from the
  pinned port root
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/`
  ONLY because no overlay entry existed for them until now. I added overlay
  entries with `original` = those exact port paths (matching the established
  "original keys point at the selected authoritative source" convention from
  the l23 report), `candidate` = my edited copies. The l25 parent rebuild will
  replace them exactly like it replaced UnrealInitializer.cpp in l24.
- The port root files themselves were NOT edited (frozen provenance; hashes in
  CHANGES.json pin them).

Files touched (candidate copies + original/ snapshots + CHANGES.json):

1. `typescan-r4/candidate/include/Unreal/UObjectArray.hpp` (NEW overlay;
   original: `port/unreal/include/Unreal/UObjectArray.hpp`)
2. `typescan-r4/candidate/src/UObjectArray.cpp` (NEW overlay; original:
   `port/unreal/src/UObjectArray.cpp`)
3. `typescan-r4/candidate/src/ClassListener.cpp` (NEW overlay; original:
   `port/unreal/src/ClassListener.cpp`)
4. `typescan-r4/candidate/src/UnrealInitializer.cpp` (existing overlay entry;
   hash recomputed)
5. `typescan-r4/CHANGES.json` (+3 entries, UnrealInitializer hash refreshed,
   dated review_fixes entry; other agents' concurrent entries preserved)

Forbidden files untouched: NativePropertyPointerGuard.*, TypeChecker.cpp,
ScanCollectionGuard.hpp, UE4SSProgram.cpp, LuaMod.cpp, ScriptHookThunks.cpp,
tests/, profile/, build scripts.

ODR note for the parent: the UObjectArray.hpp replacement is a HEADER — it
must be applied tree-wide (all TUs), not per-TU shadowed, so every listener
vtable in the closure (FClassDeleteListener, FShutdownDeleteListener,
FLuaObjectDeleteListener in LuaUObject.cpp) is compiled against slot-4 layout
identically. The CHANGES replacement model does exactly this (file copy at
the original location).

## 0b. Final row list + Z9 follow-up (supersedes Z9 checklist items 5-6)

Z9's audit (`outputs/2026-10-08-l25-prep/agent-Z9-edit-path.md`) marked
ClassListener.hpp and LuaUObject.hpp rows PENDING, but those were written as
a generic checklist before the implementation shape was known. SCOPE CHECK
RESOLVED — **no rows are added for either file because neither was edited**:

- `ClassListener.hpp` — unchanged. The first-call markers are TU-local in
  `ClassListener.cpp` (file-local anonymous-namespace `L25CallMarker` objects
  at :90-93); no declaration changes were needed, so no row. Verified: no
  `typescan-r4/{original,candidate}/include/Unreal/ClassListener.hpp` exists
  and no CHANGES row keys on it.
- `LuaUObject.hpp` — unchanged. The slot-4 ABI repair propagates to
  `FLuaObjectDeleteListener` through the BASE-class header (my
  `UObjectArray.hpp` row), which build_runtime.py aliases by (name, digest)
  across the whole dependency universe — including `lua-luauobject` and the
  other 32 units consuming the l22-diagnostics copy. The LuaUObject listener
  adds no virtuals, so nothing in it needs editing. This also sidesteps Z9's
  risk-1 trap entirely (no LuaUObject.hpp row exists to mis-key on the port
  root; had one been needed, `original` would have been the l22-diagnostics
  copy `outputs/2026-10-08-l22-runtime/diagnostics/include/LuaType/LuaUObject.hpp`
  sha 1b6e2ab7…, never the port root 89cb7390…).

Final row set (all four verified against Z9 section 2 and the l24 receipts;
`original_sha256` on disk == recorded == Z9's table):

| original (exactly what l24 consumed) | sha256 | candidate | sha256 |
|---|---|---|---|
| l24-r3 `typescan-r3/candidate/src/UnrealInitializer.cpp` | 78c16fb7… | `candidate/src/UnrealInitializer.cpp` | efdef253… |
| port root `…/port/unreal/include/Unreal/UObjectArray.hpp` | b01bf4ef… | `candidate/include/Unreal/UObjectArray.hpp` | 69a2391a… |
| port root `…/port/unreal/src/UObjectArray.cpp` | 171560a9… | `candidate/src/UObjectArray.cpp` | 668b2682… |
| port root `…/port/unreal/src/ClassListener.cpp` | a579b272… | `candidate/src/ClassListener.cpp` | a8e9b6f4… |

Z9 cross-checks performed after the audit:

1. Original-key digests all match (table above; `UnrealInitializer.cpp`
   stays keyed on the typescan-r3 candidate — never re-keyed to port root or
   an l25 path, per Z9 checklist item 4).
2. Cross-part duplicate-original scan across `profile/`, `typescan-r4/`,
   `thread/` CHANGES.json: NO conflicts (same original never maps to two
   different candidates — build_runtime.py line-45 assert safe).
3. Full hash sweep re-run: 54 hash pins across the 18 `files` rows (mine +
   other agents') ALL match disk; legacy `changes` key kept in sync with
   `files` (build_runtime reads only `files`, but sync prevents confusion);
   every candidate lives under `outputs/2026-10-08-l25-runtime/`.
4. Variant-B token spelling verified against the driver:
   `build_variants.py:39` `TOKEN='MODBRIDGE_L25_SKIP_DELETE_LISTENER'` —
   identical to both `#if !defined(...)` guards in UnrealInitializer.cpp
   (:546, :561).
5. Added includes are already-pinned universe headers, so the
   unapproved-include assert (build_runtime.py:115-116) holds:
   `Unreal/FMemory.hpp`, `DynamicOutput/Output.hpp`, `File/Macros.hpp`,
   `Helpers/String.hpp` all appear in many l24 `pass1/objects/*.d` files
   (e.g. asset_registry.d, csv_data_commands.d) and in
   unreal-unrealinitializer.d; `atomic/chrono/cstdint/mutex` are std headers
   already used by existing units. No new headers introduced → `added_headers`
   correctly unchanged.
6. Object provenance note for agent R: `unreal-uobjectarray` and
   `unreal-flat-classlistener` were INHERITED l22 objects in the l24 build
   (l24 receipt points at `2026-10-08-l22-build-r3/pass1/objects/*.o` with
   source pins 171560a9/a579b272 — equal to my rows' original hashes), so my
   rows will move both units into `affected_units` and force recompilation
   from the candidates; `unreal-unrealinitializer` (l24 object, source pin
   78c16fb7) and, via the header alias, `lua-luauobject` (l22-diagnostics
   LuaUObject.cpp d31f4e51) are equally covered. Z9's risk-8 checklist item
   (all four units must appear in the produced CHANGE-SELECTION
   affected_units) applies at build time.


## 1. ABI verdict and the fix (item 3)

Independent verification from the vendored official UE 5.6.1 source
(`outputs/2026-10-08-l23-layout-generator/official-source/`, Build.version
5.6.1 — identical copy under `2026-10-08-l23-layout-generic/`), corroborated
by `agent-V-listener-abi.md` (received mid-task):

- ENGINE `FUObjectDeleteListener` — vendored
  `Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h:929-954`:
  virtual dtor (:932 → Itanium slots 0 D1 / 1 D0),
  `NotifyUObjectDeleted(const UObjectBase*, int32) = 0` (:940 → slot 2),
  `OnUObjectArrayShutdown() = 0` (:945 → slot 3),
  `SIZE_T GetAllocatedSize() const { return 0; }` (:950-953 → slot 4).
- OUR base `RC::Unreal::FUObjectDeleteListener` — port
  `include/Unreal/UObjectArray.hpp:29-35` (overlay original): slots 0-3 only.
  **MISMATCH: slot 4 absent; our listener vtables ended at slot 3.** The
  engine dispatches slot 4 at `UObjectArray.h:1471`
  (`FUObjectArray::GetDeleteListenersAllocatedSize`, reached from
  `LogHashMemoryOverheadStatistics`, engine `UObjectHash.cpp:2416`) — an
  out-of-bounds vtable read + indirect call one word past our vtable object.
- Slots 0-3 exact match (order, signatures, constness; `int32`==`int32_t`,
  `SIZE_T`==`size_t` 8-byte on the PS5 x86-64 target). Engine
  `FUObjectCreateListener` (`UObjectArray.h:908-924`) has NO GetAllocatedSize
  — our create-listener base was already correct; no fix needed.
- Derived classes: `FClassDeleteListener`/`FShutdownDeleteListener`
  (`port/unreal/include/Unreal/ClassListener.hpp:30-44`) and
  `FLuaObjectDeleteListener` (`port/ue4ss/include/LuaType/LuaUObject.hpp:31-41`,
  registered from LuaMod.cpp:6896/lineage :7927) add NO new virtuals, so the
  base fix repairs all three vtables with no derived edits.

**FIX APPLIED** — `typescan-r4/candidate/include/Unreal/UObjectArray.hpp:62`:

```cpp
virtual size_t GetAllocatedSize() const { return 0; }
```

declared LAST (after OnUObjectArrayShutdown) so it occupies slot 4, with a
comment block (:21, :36-54) recording the ground truth, the dispatch site,
and the "never reorder / never declare derived virtuals before base slots"
rule. The header's stale "hasn't changed from 4.12" claim is corrected for
the delete listener (:21-23).

## 2. Item 2 — instrumented + hardened registration (call site + callee)

Call site `typescan-r4/candidate/src/UnrealInitializer.cpp` (PostInitialize):
- :544-558 — class-cache delete listener: pre-marker, timed call, post-marker
  with elapsed ms, diag events `l25_dl_class_begin`/`l25_dl_class_done`.
- :561-579 — shutdown listener (the exact stall point): pre-marker, the
  historical "Adding shutdown listener" line kept directly before the call,
  timed call, post-marker with elapsed ms, diag events
  `l25_dl_shutdown_begin`/`l25_dl_shutdown_done`.

Callee `typescan-r4/candidate/src/UObjectArray.cpp`:
- `AddUObjectDeleteListener` (:481-576): logs Add-enter with GUObjectArray +
  listener pointers; dumps the engine `UObjectDeleteListeners` TArray header
  (Data/Num/Max) BEFORE (:508) and AFTER (:554) registration; sane-header gate
  (:511) that refuses cleanly; duplicate refusal (:518) — behavior change:
  previously `throw std::runtime_error` (aborting init), now skip + loud log +
  diag event, init continues; growth analysis (no-realloc / fresh-alloc /
  realloc path string in the post-dump); post-write anomaly check (:557, Num
  must advance by exactly 1); whole body after the null-GUObjectArray check in
  try/catch (:498) because `GetUObjectDeleteListeners()` itself throws when
  the runtime member-offset table lacks the 'UObjectDeleteListeners' layout
  (generated wrapper `MemberVariableLayout_SrcWrapper_FUObjectArray.hpp:137-145`
  — port file, cited in the comment at :500-503).
- `RemoveUObjectDeleteListener` (:578-625): same hardening (engine calls this
  from our OnUObjectArrayShutdown callbacks on the game thread).
- Sanity gate `l25_listener_header_sane` (:445-454): Num/Max >= 0, Num <= Max,
  Max <= 256, Data==nullptr only when Num==Max==0, 8-byte alignment, no
  address-space wrap at Data+Max*8. Bound justification (:427-433): engine
  core registers a single-digit count of delete listeners and TArray growth
  slack stays small (4..64), so 256 separates plausible from garbage with
  >16x headroom; a garbage Max read through a mis-bound GUObjectArray offset
  lands in the millions or is negative. "Data readable" is NOT provable from
  this TU (the array is engine-heap memory; the port's image-span classifier
  and VirtualQuery bridge are staging headers not on this unit's include
  path) — the gate is arithmetic; the markers discriminate the rest.
- `AddUObjectCreateListener`/`RemoveUObjectCreateListener` deliberately left
  byte-identical to the port original (:466-479): the create array is a
  different member, completed before the observed stall point in every log,
  and item scope is the delete listener.

## 3. Item 3 — first-call markers (engine-dispatch proof)

`typescan-r4/candidate/src/ClassListener.cpp`:
- `L25CallMarker` (:42-88): atomics-based, logs first call then at most once
  per >=10 s (steady_clock); per-call cost is one fetch_add + one clock read
  so mass-GC dispatch stays cheap; also emits a one-shot-ish diagnostic event
  per logged call.
- Markers in `FClassDeleteListener::NotifyUObjectDeleted` (:173-178),
  `FClassDeleteListener::OnUObjectArrayShutdown` (:216-222),
  `FShutdownDeleteListener::NotifyUObjectDeleted` (:229-237 — body was empty
  before, still no behavioral work beyond the marker),
  `FShutdownDeleteListener::OnUObjectArrayShutdown` (:239-247).
- Diagnostic events: `l25_dl_first_notify_class`,
  `l25_dl_first_notify_shutdown`, `l25_dl_shutdown_cb_class`,
  `l25_dl_shutdown_cb_shutdown` (tags at :90-93), declared via the same
  in-link `extern "C" diagnostic_event` pattern PS5PadSource.cpp established
  (defined in-link by diagnostic.c — no new import, no new undefined symbol;
  Prospero-guarded).

## 4. Item 4 — registration path decision

**Chosen: hardened direct array write** (same write the port always did, now
gated/logged/serialized). The engine's own locked path and the game-thread
pump path were evaluated and are NOT reachable from this change set — the
full reasoning is in the block comment
`candidate/src/UObjectArray.cpp:378-422`:

1. Engine's `FUObjectArray::AddUObjectDeleteListener` (COREUOBJECT_API,
   vendored header `UObjectArray.h:1133`; Private impl not vendored) takes
   `UObjectDeleteListenersCritical` (member at :1434; THREADSAFE_UOBJECTS
   hard-1 per vendored `ObjectMacros.h:33`, so the lock exists in shipping).
   It cannot be called: the PS5 game image is statically linked with no
   runtime symbol table, this port has no signature pattern for it, and the
   seed table has no slot for it.
2. Taking `UObjectDeleteListenersCritical` ourselves: no published layout
   offset for the critical sections in the generated member layout, and
   `FTransactionallySafeCriticalSection` layout is feature-dependent;
   blind-locking a guessed offset would corrupt GUObjectArray. (The port's
   own `LockGUObjectArray` remains a TODO stub — and guards the wrong lock.)
3. Registering on the game thread via the R3 marshal pump: the game-thread
   action queue (`m_game_thread_actions` drained by `funcptr_marshal_hook`)
   lives in LuaMod.cpp, off-limits to this change set.

Safety applied to the unavoidable unlocked write: file-local
`std::mutex GUObjectDeleteListenerWriteMutex` (:425) serializing ALL of our
delete-listener writers (init-thread Add, game-thread self-Remove from
OnUObjectArrayShutdown, LuaMod's FLuaObjectDeleteListener Add — lock order is
engine-lock→ours on the callback path, ours alone on Add, so no cycle);
sane-header gate refusing cleanly; GMalloc-bound check before any growth
(:530) — note RC `FMemory.cpp:32-44` routes Realloc through the seed-bound
ENGINE GMalloc instance, so agent-V's allocator-mismatch hazard is NOT
present in this build (storage comes from and returns to the engine heap);
and explicit path logging, with the realloc case flagged RISK (:541) because
realloc swaps/frees the engine-owned Data buffer while the game thread may be
iterating it under the engine lock we cannot hold — that residual race is now
visible per-session in the log instead of silent.

**Variant B hook** (`UnrealInitializer.cpp:546/:556-558` and `:561/:576-579`):
both delete-listener registrations wrap in
`#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)` … `#else` logging the
mandated line `delete-listener registration SKIPPED (variant B; unsafe
long-term: stale object pointers not cleared)`. Variant B therefore yields a
process with ZERO delete listeners registered (both the class-cache and the
shutdown listener), with init otherwise unchanged.

## 5. Marker strings for the launch harvest (grep list)

UE4SS.log lines (all prefixed `[L25][DeleteListener]`):
1. `shutdown-listener registration begin (pre-marker)`
2. `Adding shutdown listener` (historical line, still directly before the call)
3. `Add enter (GUObjectArray=` (callee, both adds)
4. `header pre: Data=` (callee header dump before)
5. `REFUSED (` (callee clean refusal, code + reason; codes: 1 null
   GUObjectArray, 2 exception/layout-missing, 3 insane header, 4 duplicate,
   5 GMalloc unbound for growth)
6. `RISK: capacity full` (realloc path; engine-owned buffer swap imminent)
7. `fresh allocation (Num==Max==0)` (first alloc, no live old buffer)
8. `header post: Data=` (callee header dump after; `path=no-realloc` /
   `path=fresh-alloc` / `path=realloc`)
9. `ANOMALY: Num advanced by` (concurrent engine-side writer)
10. `shutdown-listener registration returned in` (post-marker + elapsed ms)
11. `class-delete-listener registration begin (pre-marker)`
12. `class-delete-listener registration returned in` (post-marker + elapsed)
13. `engine called FClassDeleteListener::NotifyUObjectDeleted`
14. `engine called FShutdownDeleteListener::NotifyUObjectDeleted`
15. `engine called FClassDeleteListener::OnUObjectArrayShutdown`
16. `engine called FShutdownDeleteListener::OnUObjectArrayShutdown`
17. `delete-listener registration SKIPPED (variant B; unsafe long-term: stale object pointers not cleared)`
18. `Remove enter` / `Remove header pre:` / `Remove header post:` /
    `Remove REFUSED` (deregistration path)

Diagnostic-event tags (supplier-diag jsonl): `l25_dl_add_begin`,
`l25_dl_pre`, `l25_dl_post`, `l25_dl_add_done`, `l25_dl_refused`,
`l25_dl_risk`, `l25_dl_remove_begin`, `l25_dl_remove_done`,
`l25_dl_class_begin`, `l25_dl_class_done`, `l25_dl_shutdown_begin`,
`l25_dl_shutdown_done`, `l25_dl_first_notify_class`,
`l25_dl_first_notify_shutdown`, `l25_dl_shutdown_cb_class`,
`l25_dl_shutdown_cb_shutdown`.

Reading the harvest: last line 1 with no 2/3 → hang inside Output or between
markers; 2 without 3 → hang entering the callee (call frame); 3 without 4 →
hang in logging or mutex; 4 without 8 → hang inside Contains/Append (engine
memory walk or GMalloc); 8 without 10 → hang on return path; any REFUSED →
root cause + code; 10 followed later by 13/14 → listener live and dispatched
(slot-4 fix then covers the memory-stats path); 10 but never 13/14 → engine
never dispatches into us (registration-visible-but-dead or timing); 17 →
variant B build confirmed.

## 6. Hashes (CHANGES.json updated; verify with shasum -a 256)

- `candidate/include/Unreal/UObjectArray.hpp` 69a2391aae35…
  (original port header b01bf4ef8463…, byte-identical copies at
  typescan-r4/original/…; pinned in the entry)
- `candidate/src/UObjectArray.cpp` 668b2682751e…
- `candidate/src/ClassListener.cpp` a8e9b6f4246d…
- `candidate/src/UnrealInitializer.cpp` efdef2535fe7… (recomputed from prior
  c4364dd73401…)

Full values in `typescan-r4/CHANGES.json` (18 file entries now; the three
other agents' concurrent entries — TypeChecker/UE4SSProgram/LuaMod/
ScriptHookThunks/CustomProperty — preserved untouched). Post-Z9 full sweep
re-run (section 0b item 3): all 54 hash pins across the 18 rows match disk,
`files` and legacy `changes` in sync, every candidate under l25-runtime, no
cross-part duplicate-original conflicts.

## 7. Not done / honest limits

- Nothing compiled: no target compile, no host suite, no console run. The
  parent owns the closure rebuild; the header replacement must be applied
  tree-wide (ODR, section 0).
- The sanity gate is arithmetic only (no VirtualQuery of the engine-heap Data
  pointer from this TU); a readable-but-stale Data pointer still passes and
  the markers exist to expose exactly that case.
- `MaxPlausibleDeleteListeners = 256` is a judgment bound, justified in code.
- Variant B leaves searcher pools un-invalidated for the session (stale
  object pointers) — the documented, accepted diagnostic tradeoff.
