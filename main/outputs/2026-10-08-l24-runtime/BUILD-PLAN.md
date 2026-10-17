# l24 BUILD PLAN — source edit to packaged PRX (Mac-only)

Created 2026-10-08. Scope of this infrastructure task: Mac-only, no console contact,
no runtime/mod-manager edits, and **the build is NOT run yet** — the design agent's
source changes must land in `typescan-r3/candidate` first.

## 0. Lineage

```
l19 full build (frozen composition, 374 units / 391 link inputs)
  -> l22-build-r3  (BASE of l23)
  -> l23-runtime   (parts: profile, typescan-r2, thread)   [completed, console-harvested]
  -> l23-build-r2  (BASE of l24)  runtime-l23.o twins, sha256 653307e6...
  -> l24-runtime   (parts: profile, typescan-r3, thread)   [THIS DIRECTORY, placeholder]
  -> l24-build-r2  (created by build_runtime.py when run)  -> l24-package -> PRX
```

l23 ran on console (PID127): preflight passed, then type scan refused with
`[PropertyPointer] unreadable field or unknown property FField class refused`.
The l24 design goal (from the l23 harvest review) is a local rework of
property-pointer validation/readability and FField class recognition during
type-system bootstrap. That work lands in `typescan-r3/` only.

## 1. What exists in this directory now

| Path | What it is |
|---|---|
| `build_runtime.py` | Build driver, copied from l23 and edited: `BASE=outputs/2026-10-08-l23-build-r2`, `OUT=outputs/2026-10-08-l24-build-r2`, parts `['profile','typescan-r3','thread']`, all receipt labels l23->l24. Logic is byte-equivalent to l23's proven driver. |
| `typescan-r3/CHANGES.json` | Placeholder, same schema as l23's typescan-r2 file. 12 rows; originals point at the **l23 typescan-r2 candidates** (the sources the BASE composition actually selected), candidates at the byte-identical copies in `typescan-r3/candidate`. All hashes verified. |
| `typescan-r3/candidate/` | 12 files, byte-identical copies of l23 typescan-r2 candidates (starting point for design edits). |
| `typescan-r3/original/` | Refreshed snapshots of the **current originals** (the l23-r2 candidates). Pre-l23 snapshots remain in l23's own `typescan-r2/original/`. |
| `typescan-r3/include/` | 4 helper headers copied from r2: `NativePropertyPointerGuard.hpp/.inl`, `NativePropertyReadBackend.inl`, `NativeTypeScanPolicy.hpp`. |
| `typescan-r3/tests/` | Host test suite copied from r2 (25 test entries). |
| `typescan-r3/target_compile.py` | Probe compiler, copied and re-pointed: part dir -> `2026-10-08-l24-runtime/typescan-r3`, composition -> `2026-10-08-l23-build-r2/pass1`. |
| `typescan-r3/results/inherited-l23-r2/` | Quarantined l23 r2 evidence (22 files: `.o/.d/.log`, TARGET-COMPILE.json, HOST-TESTS.json, RED logs). **Not l24 evidence** — do not present as such; regenerate. |
| `typescan-r3/REPORT.md` | Still the l23 r2 report; must be rewritten when l24 changes land. |
| `profile/CHANGES.json` | Placeholder: `files: []` (profile unchanged from l23; overlay already baked into BASE), declares `-I .../2026-10-08-l23-runtime/profile/units`. |
| `thread/CHANGES.json` | Placeholder: `files: []` (thread unchanged from l23), declares `-I .../2026-10-08-l23-runtime/thread/include` and `-I .../2026-10-08-l24-runtime/typescan-r3/include`. |

Verified during setup: BASE artifacts present (SOURCE-SELECTION.json with 11,946 pins,
200 sampled pins hash-verified; pass1/pass2 COMPOSITION-RECEIPT.json + undefined.stdout),
LLVM 21.1.8 toolchain at `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin`,
both scripts pass `py_compile`, and a dry-run of the driver's CHANGES parsing validates
all 12 rows + 4 added headers + 5 include dirs.

## 2. l23 typescan-r2/candidate file list (the actual sources, now copied to r3)

`candidate/generated_include/` (6, header replacements, PS5 `#ifdef __PROSPERO__` variants):
- `MemberVariableLayout_HeaderWrapper_FArrayProperty.hpp`
- `MemberVariableLayout_SrcWrapper_FArrayProperty.hpp`
- `MemberVariableLayout_HeaderWrapper_FMapProperty.hpp`
- `MemberVariableLayout_SrcWrapper_FMapProperty.hpp`
- `MemberVariableLayout_HeaderWrapper_FSetProperty.hpp`
- `MemberVariableLayout_SrcWrapper_FSetProperty.hpp`

`candidate/src/` (6, translation units):
- `src/Property/FArrayProperty.cpp`  -> unit `unreal-src-property-farrayproperty`
- `src/Property/FMapProperty.cpp`    -> unit `unreal-src-property-fmapproperty`
- `src/Property/FSetProperty.cpp`    -> unit `unreal-src-property-fsetproperty`
- `src/TypeChecker.cpp`              -> unit `unreal-flat-typechecker`
- `src/UnrealInitializer.cpp`        -> unit `unreal-unrealinitializer`
- `src/CustomProperty.cpp`           -> unit `lua-custom-property`

Mapping verified against `2026-10-08-l23-build-r2/pass1/COMPOSITION-RECEIPT.json`:
exactly 8 units are sourced from l23-runtime — the 6 above plus `luamod` and `program`
(thread/src/LuaMod.cpp, thread/src/UE4SSProgram.cpp, unchanged in l24).

## 3. How prepare.py transformed sources (historical, r1)

`../2026-10-08-l23-runtime/typescan/prepare.py` generated the **r1** overlay; the r2
candidates then carried reviewed fixes on top (r2 REPORT.md items 1-2). It is marked
"do not use to regenerate final CHANGES.json". What it did:

- Generated-include wrappers (Map KeyProp/ValueProp, Set ElementProp, Array Inner):
  on `__PROSPERO__`, getters return `FProperty*` by value instead of `FProperty*&`,
  reading the published offset via `MemberOffsets.find(...)` and returning
  `NativePropertyGuard::ReadPointer(this, offset, "...")`; non-PS5 keeps the original
  reference getter inside `#else`.
- `src/Property/F{Map,Set,Array}Property.cpp`: prepended
  `#ifdef __PROSPERO__ #include "NativePropertyPointerGuard.hpp" #endif`.
- `TypeChecker.cpp`: included `NativeTypeScanPolicy.hpp` + guard + `<atomic>`;
  `find_all_property_types` captured by reference; `GetFFieldClassUnsafe` routed
  through `NativePropertyGuard::ReadClass(field)`; field-chain traversal gained a
  `visited` cycle refusal and `missing_keys.attempt(...)` collect-and-continue
  isolation around nested `inner/element/key/value` probes and child scans; a
  `[TypeScan] scan-complete missing-keys=N core-ready=B` gate requires the core
  type list (UObject..UCanvasPanel, FField..FEnumProperty) before reflection
  consumers run; `NativePropertyPointerGuard.inl` included in-namespace on PS5.

The l24 design agent edits the r3 copies directly; prepare.py is reference only.

## 4. CHANGES.json schemas (as the driver consumes them)

Driver reads, per part: `d['files']` (required), `d.get('added_headers', [])`,
`d.get('include_directories', [])`. Each file row needs `original`,
`candidate`, `original_sha256`, `candidate_sha256` (driver asserts both live and
matching; `copied_original*` fields are provenance, kept for the packet).

- **typescan part (l24 `typescan-r3/CHANGES.json`)**: 12 rows as in l23, plus
  `changes` (duplicate list), `include_dirs`/`include_directories`
  (`typescan-r3/include`, `typescan-r3/candidate/generated_include`),
  `added_headers` + `added_header_hashes` (the 4 guard/policy headers),
  `scope`, `new_os_imports: []`, `parent_shutdown_requirement`, and
  `source_selection` pinning `2026-10-08-l23-build-r2/SOURCE-SELECTION.json`.
- **thread part**: rows were `original` = pre-l23 path (l22-runtime/diagnostics
  LuaMod.cpp, l19-profile-r4 UE4SSProgram.cpp) -> `candidate` = l23-runtime/thread
  src; `added_headers` = `L23GameThreadProbe.hpp`; include dirs thread/include +
  typescan include. l24 placeholder: empty files, include dirs only (see file).
- **profile part**: rows were l22 profile files -> l23 profile overlay files
  (townfall-build-facts.json, RuntimeBuildProfile.hpp,
  CompiledNativeLayoutDeclarations.hpp, JoinedLayoutPreflight.hpp) plus fact
  counters (93 prior facts, 0 promoted). l24 placeholder: empty files,
  `-I` on `2026-10-08-l23-runtime/profile/units` only.

Key l24 difference from l23: **originals now point at the l23-r2 candidate paths**
(what BASE actually selected), not the `.superpowers` source tree. This was
verified necessary: the 6 generated-header overlay entries in BASE
`headers.vfs.json` target the l23-r2 candidates, and the 6 cpp unit sources in
BASE's composition are the l23-r2 candidates, so replacement/alias/overlay-rewrite
all resolve through those paths. After any edit, recompute `candidate_sha256`
(and `added_header_hashes` if include/ headers change); originals' hashes must
stay frozen — if an original itself changes, BASE has drifted and must be
reconciled before building.

## 5. Source-edit phase (design agent, before any build)

1. Edit only under `typescan-r3/candidate/` and `typescan-r3/include/`
   (new headers go in `typescan-r3/include/`; add them to `added_headers` +
   `added_header_hashes` + `include_directories`).
2. Keep the no-new-offsets discipline: reuse published offsets/facts only
   (93 facts; Map/Set container keys stay conditional; no new OS imports —
   `new_os_imports` must remain `[]`, the driver enforces import equality).
3. Update host tests under `typescan-r3/tests/`; run them in plain and
   ASan/UBSan modes; mirror the l23 r2 evidence set (RED log for the refusal
   being fixed, HOST-TESTS.json, TARGET-COMPILE.json) into `typescan-r3/results/`
   (fresh files, never reuse the quarantined `inherited-l23-r2/` evidence).
4. Probe-compile the six target units:
   `python3 outputs/2026-10-08-l24-runtime/typescan-r3/target_compile.py`
   from the worktree root (`what/`). All must exit 0.
5. Update `typescan-r3/CHANGES.json` hashes and rewrite `typescan-r3/REPORT.md`.
6. Pre-build gates (mirror l23): NO-NEW-OFFSETS check against the 93 profile
   facts, an independent review packet + receipt, and explicit approval that
   candidate hashes differ from originals (identical rows at placeholder time
   mean the change has not landed — do not build).

## 6. Runtime build phase (after approval)

Run once from the worktree root (`what/`):
`python3 outputs/2026-10-08-l24-runtime/build_runtime.py`

What it does (l23-proven logic):
1. Refuses if `outputs/2026-10-08-l24-build-r2` exists; verifies every BASE pin
   (SOURCE-SELECTION source/receipt/artifact pins).
2. Loads parts `profile`, `typescan-r3`, `thread`; verifies original/candidate
   hashes; candidates must live under this directory; builds the replacements map.
3. Header aliasing: any dependency file in the BASE universe with a matching
   (basename, original sha) is aliased to the r3 candidate; inherited VFS overlay
   entries (from BASE argvs) are rewritten through the replacements into a new
   `l24-build-r2/headers.vfs.json`.
4. Affected units = sources replaced or depending (via .d files) on any aliased
   header; retained units keep their pinned objects.
5. Two independent passes (pass1, pass2): recompiles affected units (new argv =
   parent argv minus old `-ivfsoverlay`, plus new overlay, `-I` for the declared
   include dirs and the source parent; typescan-r3 dirs are inserted first so they
   shadow the baked l23 `-I .../typescan-r2/...` flags), rejects unapproved
   include-graph additions, then links the full 391-input runtime
   (`runtime-l24.o`), asserts `llvm-nm -u` symbol set equals the BASE pass's
   (raw import delta must be declared before packaging), and writes
   COMPILE-RECEIPT/DEPENDENCY-GRAPH/IMPORT-DELTA/COMPOSITION-RECEIPT.
6. Asserts the twin objects are byte-identical across passes and every unit's
   object hash matches; writes ACTUAL-DEPENDENCIES.json, SOURCE-SELECTION.json
   (grows BASE pins with the new actual/candidate/added pins), TWIN-RECEIPT.json
   (`MAC_ONLY_IDENTICAL_L24_RUNTIME_PAIR`).

Concurrency note: 16GB Mac — the driver already serializes to max_workers=2 and
the twin passes run sequentially in-process; do not run other heavy builds
alongside it.

## 7. Packaging phase (creates the PRX; mirror of l23-package)

Performed in a new `outputs/2026-10-08-l24-package/` directory (create when
reached; do not reuse l23-package — `pass-1`/`pass-2` refuse to overwrite).

Inputs manifest `BUILD-INPUTS.json` (see l23's for the exact shape):
- `scope: LOCAL_COMBINED_PACKAGE_ONLY`, `console_actions_authorized: false`.
- `runtime_objects_by_pass`: pins of `l24-build-r2/pass{1,2}/runtime-l24.o`
  (twins must be identical) + `runtime_source_selection` (l24 SOURCE-SELECTION),
  `runtime_independent_twin_receipt`, both composition receipts,
  `runtime_strong_undefined`/`runtime_weak_undefined` censuses (must match the
  new runtime's `llvm-readelf` undefined sets).
- Frozen bootstrap provenance: `outer_entry`/`outer_bridge`/`outer_headers`,
  `lineage_root` (`2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2`)
  + 74 lineage source pins + `lineage_overlays`, `SDK_captures` (sdknm
  libSceLibcInternal/libScePad/libkernel stdout captures), `import_evidence`,
  `import_check` (owned relocation check matching the typed shim order),
  `package_script` (`package_prx_l19.py`), `controller`, `historical_receipts`.
- Update the l23-specific rows (l23_profile_digest/l23_profile_changes ->
  l24 equivalents, parent_build_inputs, changed_runtime_units,
  final_import_baseline) to the l24 facts.

`package_combined.py --pass-id 1|2` (copy and re-point from l23):
1. `prep_guard.require_ready()` re-pins every manifest entry (including every
   row of the l24 runtime SOURCE-SELECTION) and refuses any console grant.
2. Loads the frozen package script, pins historical receipts, admits exactly the
   one previously-evidenced extra import (`scePthreadGetthreadid` ->
   sceKernelOpen) into the typed import table (order preserved, appended last),
   and checks the generated shim order matches the owned import-check source.
3. Stages the lineage tree once into `l24-package/lineage/`, writes the new
   outer entry/bridge/headers and import check into it, records
   STAGED-SOURCE-PINS.json (re-run refuses on BUILD-INPUTS sha drift).
4. Copies the pass runtime object in as the composition witness; verifies the
   strong/weak undefined census against the manifest.
5. Replaces the frozen bootstrap entry unit with `lifecycle/runtime_module_entry.c`,
   swaps in the l24 diagnostic source, appends `runtime_start_bridge.cpp` +
   `native_thread_import_check.c`, extends the supplier import shim with the
   import-count symbol, and attaches exact evidence to the new getter's contract
   row (native resolution explicitly not proven).
6. Runs the frozen link pipeline (`link_pipeline`) -> combined object -> PRX
   (`bare-sdk/<candidate>.prx`), then `verify_candidate` static checks,
   constructor/destructor array census (slots preserved), and
   `check_import_identity.verify` requiring exactly the 207 exact import
   identities (206 + the one admitted getter).
7. Emits BUILD-RECEIPT.json (`MAC_PACKAGE_CHECKS_PASS` only with zero gaps),
   PACKAGE-RESULT.json, STATIC-CHECKS.json, FINAL-RECEIPT.json, review zips.

Gates that blocked l23 packaging, to repeat for l24: import delta declared and
empty; twins identical; all static checks pass; independent review of the packet
before any delivery step. l23's PRX for reference: 31,324,743 bytes, SELF magic
`4f153d1d`, SDK `0x11000000`.

## 8. Explicitly out of scope (per task rules)

- No console contact, upload, arming, or launch — those are separate, explicitly
  authorized phases (PS5 rules in AGENTS.md: decrypt-off verify, fake-sign, no
  rapid-fire launches) and nothing here triggers them.
- No edits to runtime/ mod-manager sources; no changes under l23-* directories;
  `outputs/2026-10-08-l24-build-r2/` is created only by the build driver.
- The build must not run while CHANGES.json rows are still byte-identical
  placeholders.
