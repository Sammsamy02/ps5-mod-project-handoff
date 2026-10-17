# INTEGRATION-PLAN — cumulative integration/build selection for #7 step 2, #10, #1/#3

Agent JD, 2026-10-13. INTEGRATION DESIGN ONLY — Mac-only, no builds, no console,
no source edits performed. Every fact below was read from the actual stagers,
drivers, recipes, and current tree bytes (verification receipts in the
Appendix). This plan implements AI-HANDOFF-2026-10-13 item 4: a coherent
cumulative integration/build selection with nonempty gates and one independent
runtime review per component, before any deployment/test approval is sought.

Paths (short names used throughout):

- `WHAT` = `/WORKSPACE/.codex/worktrees/translation-core/what`
- `L26`  = `WHAT/outputs/2026-10-08-l26-runtime`
- `CAPS` = `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work`
- Build driver = `L26/build_variants.py`; packaging driver = `L26/l26_packaging_driver.py`
- Static gates = `WHAT/outputs/2026-10-08-l25-prep/harvest/fs_throw_gate.py`, `no_detour_gate.py`

Current tree state assumed by this plan (all verified today, see Appendix A):
`typescan-r4/candidate/src/UnrealInitializer.cpp` = `04f02bc2…`, `LuaMod.cpp` =
`fd53a4f2…`, `UE4SSProgram.cpp` = `ad3ae621…`; `profile/units/CodeSignatureProfile.hpp`
= `a4cd6c04…`, `profile/units/CodeImageCache.hpp` = `22803051…`. Variant B is
dead (P2 tree patch applied: `MODBRIDGE_L25_SKIP_DELETE_LISTENER` count 0 in all
three files); variant C token present (UnrealInitializer 1, UObjectArray 9).

---

## 1. THE OVERLAP MAP

### 1.1 Per-file map (edited originals)

| File (l26 candidate path) | Consuming BASE unit | #7 step 2 | #10 | Main-tree staged (NOT yet in tree) | Driver-internal overlays | Status |
|---|---|---|---|---|---|---|
| `typescan-r4/candidate/src/UnrealInitializer.cpp` | `unreal-unrealinitializer` (also O_UNITS/G2t member; row original = `2026-10-08-l25-build-variants/staging-copyin-probe/UnrealInitializer.cpp`) | 3 hunks (lines 83-274, 885-1106, 3223-3308 of the current file); adds `RuntimePakMount.hpp` include + SeedArming adapter include + PakMountCodeProfile consumption | — | lookup-speed `04-findxof-name-cache-unrealinitializer.patch` (pin `6a9087b5…` — STALE vs current `04f02bc2…`) | Q copyin-probe second hop (`compose_copyin_probe` rebases agent-Q's pure-addition patch onto whatever the row candidate is); CTOKEN site (variant C Phase-6) | **#7 ↔ Q COMPATIBLE (verified, §6.2)**; lookup-speed must re-anchor onto post-#7 bytes |
| `typescan-r4/candidate/src/LuaMod.cpp` | `luamod` | 2 hunks (lines 1-105, 7268-7663); wires the producer trigger into `VmtEngineTickDrain` | — | group-b `01/02` (LuaMod.cpp), lua-alloc staged LuaMod.cpp — both staged against older bytes | — | serialize per-file: #7 first, then re-anchor group-b/lua-alloc |
| `typescan-r4/candidate/src/UE4SSProgram.cpp` | `program` | — | 3 anchors (after `#include <UE4SSProgram.hpp>`; before `auto UE4SSProgram::init() -> void`; the `m_native_context.on_engine_initialized` block). Stager pin `fe9fe7bc…` **DRIFTED**: current is `ad3ae621…`; all 3 anchors still exact-once; no Cap10 markers present | IP-shim-root `01-ue4ssprogram-shim-root.patch` — **CRASHED build, FORBIDDEN, never apply**; ET armor already landed (in base bytes) | — | #10 needs a one-line re-pin + fresh derived hash before integration |
| `profile/units/CodeSignatureProfile.hpp` | profile CHANGES row (vfs-aliased into every consuming TU; lineage overlay destination `lifecycle/CodeSignatureProfile.hpp`) | 3 hunks: +`#include "PakMountCodeProfile.hpp"`, prepend `rc::pakmountprofile::kPins[0..5]`, pin count 6→12 | — | — | packaging driver auto-derives the lineage overlay from `profile/CHANGES.json` candidates (name-keyed) | row refresh propagates to both compiled copies automatically |
| `profile/units/CodeImageCache.hpp` | profile CHANGES row (lineage overlay destination `CodeImageCache.hpp` at lineage root) | 1 hunk: `kMaxPins=8` → `kMaxPins=16` (**shared Metadata layout ABI change**) | — | — | same auto-derivation; G12b consumer cascade must recompile every consumer | THE review hold: every compiled provider/consumer copy must be byte-identical (see §4.1 G-C7-abi-uniform) |

### 1.2 Added files (no original to edit — new tree content)

| Component | File | Destination | Compiled by |
|---|---|---|---|
| #7 | `RuntimePakMount.cpp` | `typescan-r4/candidate/src/` | NEW TU (needs NEW_UNITS entry, §3) |
| #7 | `RuntimePakMount.hpp`, `PakRequestProducer.hpp`, `NativePakMountAdapter.inl`, `NativePakMountEntry.hpp`, `pak_mount_pins.inc` | `typescan-r4/include/` | included (adapter `.inl` from UnrealInitializer; producer from RuntimePakMount/LuaMod) |
| #7 | `PakMountCodeProfile.hpp` | `profile/units/` | included by CodeSignatureProfile.hpp — must resolve BOTH in the runtime build AND in the packaging lineage (needs EQ-pattern lineage/outer-headers extension, §3.3) |
| #10 | `Cap10InvocationObservation.hpp` | `typescan-r4/candidate/include/` (added_header) | included (unguarded within `#ifdef __PROSPERO__`) by the staged UE4SSProgram.cpp |
| #10 | `observer_fragment.inc` | consumed at stage time by the cap10 stager (inlined into UE4SSProgram.cpp; no tree placement required for compilation) | `program` TU |
| #10 | `MonoOwner.cpp/.hpp`, `InvocationBoundary.cpp/.hpp`, `ProbeImage.hpp` | `typescan-r4/candidate/{src,include}/` **DEFERRED** — they include `<mono/...>`; no Prospero Mono port exists | none until an admitted target port (§3.2) |
| #10 (loader) | `mono-owned-loader.patch` (3 anchors on `mono-dl.c`, base `b7ccf30b…`) | the matching Mono runtime's `mono/utils/mono-dl.c` | the port's own build — NEVER the PS5 PRX build |

### 1.3 Conflict and dependency inventory

Direct conflicts:

1. **#7 ↔ #10: NONE.** Disjoint edited-original sets ({UnrealInitializer, LuaMod,
   CodeSignatureProfile, CodeImageCache} ∩ {UE4SSProgram} = ∅) and disjoint
   added-file names. Confirmed from both stagers' source code.
2. **#7 ↔ Q copyin-probe second hop: compatible** (same file, disjoint anchors;
   clean rebase proven — §6.2). Dependency: the driver composes Q AFTER the row
   candidate is selected, so #7's derived file must be the row candidate before
   `--execute`; Q's rebase is verified by the driver's own G2q gates at plan time.
3. **#7 ↔ round5 staged patches (lookup-speed, group-b, lua-alloc): same files,
   deferred collisions.** Those patches are NOT in the tree and their unpatched
   pins are stale against the current tree already (`6a9087b5`/`63e6e057`/`a8e9b6f4`
   vs current `04f02bc2`/`d00a3dbd`/`84e863fa`). They must be re-anchored AFTER
   #7 lands (their base bytes shift again). This is a serialization dependency,
   not a conflict: each gets its own labelled gated build.
4. **#10 ↔ IP-shim-root: same file, EXCLUDED by rule.** IP-shim-root produced a
   crashed PRX (`1c48239d`) and is not part of any admissible selection. Its
   presence in round5-staging is historical.
5. **#7 profile pair ↔ shared CodeImageCache ABI: the only true cross-cutting
   dependency.** `kMaxPins` 8→16 changes the `Metadata` layout consumed by every
   TU that calls `named_entry` on the bridge's code cache. The runtime side is
   routed by the profile CHANGES row + vfs alias; the lineage side by the
   packaging driver's name-keyed overlay derivation. Both derive from the SAME
   candidate file, so one row refresh fixes both — provided G12b recompiles every
   header consumer in the same build (full mode does; `--incremental` waives it,
   so **no incremental builds for the #7 step**).
6. **Census/P8 accumulation.** Each new TU shifts the link-input census
   (G6: `391+len(NEW_UNITS)`) and possibly `.init_array` (P8, currently 240/2).
   Every step must declare its expected census before execute (§3.4).
7. **fs_throw baseline ratchet.** #7's `PakRequestProducer.hpp` is PS5-live C++
   using `std::filesystem::directory_iterator(directory)` in a range-for plus
   `is_symlink(symlink_status(...))`/`exists(file_status)` calls. The gate flags
   throwing forms (`directory_iterator(p)` construction, non-ec calls) as
   violations/new-site failures against the 1e49188b baseline. The `file_status`
   overloads are the documented false-positive class; the directory_iterator
   range-for is a REAL flag that must be reworked to an error_code form or
   review-approved with `[FS-THROW-APPROVED]` markers before `--execute`
   (gate G-C7-fs-throw-approved, §4.1).

Dependencies (must hold before each step): see the runbook §5 preconditions.

---

## 2. THE COMPOSITION ORDER

Principles:

- **Per-file edit chains serialize; cross-file work parallelizes.** Every edit
  changes the file's hash, and every stager/row is hash-bound, so edits to the
  same file must be ordered and each edit must refresh row hashes immediately
  (the CHANGES.json law already printed in the part scopes).
- **Land what binds the current tree first.** #7's four pins match the current
  bytes exactly (validated end-to-end today — Appendix A.2). #10's pin is stale
  and requires a re-pin regardless. The round5 patches are all stale regardless.
- **One labelled gated build per component** (Claude item-7 discipline, HANDOFF
  2026-10-12 "NEXT STEPS" 1), enforced additionally by the driver's build lock
  (`acquire_build_lock`, one build at a time).
- **Full builds only** for every step below (no `--incremental`): #7 changes a
  shared header (mixed-header ODR hazard), and every step is upload-grade.

Order:

```
S0  Preconditions (pins, snapshot, static gates on the clean tree)
S1  #7 step 2 source stage (read-only derived tree under cap07-step2/derived/)
S2  #7 tree composition + labelled build "cap07" + packaging        [serialize]
S3  #10 re-pin + tree composition + labelled build "cap10"          [serialize,
      independent of S2's files; ordered after S2 only for one-build-at-a-time]
S4  round5 staged items, each its own labelled build, AFTER re-anchoring:
      lookup-speed -> group-b -> shim-fix -> lua-alloc               [serialize]
S5  #1/#3: NO build content; fail-closed selection gate only (§4.3)
```

Independent vs serialized:

- **S2 (#7) and S3 (#10) are logically independent** (disjoint files, disjoint
  new units; #10's observer adds no link dependency while
  `MODBRIDGE_CAP10_ADMITTED_MONO_PORT` is off). They are serialized only by the
  build lock and label discipline. If ever reordered, #10's re-pin must target
  whatever UE4SSProgram.cpp bytes are current at staging time.
- **S2 before S4's lookup-speed/group-b/lua-alloc** (file-chain dependency:
  UnrealInitializer.cpp and LuaMod.cpp).
- **S4's shim-fix and lua-alloc's LuaMadeSimple files are independent of S2/S3**
  and could compose in any order relative to them after their own re-anchoring;
  keep them last anyway (each also touches LuaMod.cpp: group-b and lua-alloc).
- **Q's copyin-probe overlay is NOT a step**: the build driver composes it
  internally on every run (adopt-ready-candidate or patch-rebased modes); with
  #7 landed it takes the patch-rebased path (verified clean, §6.2).
- **The one independent runtime review per component** (handoff item 4) slots
  BETWEEN each component's tree composition and its `--execute`: #7's review
  must resolve the shared-cache/layout hold (§4.1 G-C7-abi-uniform) and the
  missing behavioral regressions listed in `cap07-step2/REVIEW.md` before its
  build; #10's review must confirm the re-pin and the deferred-port scope; the
  reviews are reading tasks and add no serialization beyond that.

---

## 3. THE NEW_UNITS REGISTRATION

### 3.1 #7 — register now (one entry)

In `L26/build_variants.py` `NEW_UNITS` (exact entry; source path relative to
`HERE` like the existing four):

```python
'pakmount': {'source': HERE/'typescan-r4/candidate/src/RuntimePakMount.cpp',
             'template': 'unreal-unrealinitializer',
             'note': 'cap07 step2 RuntimePakMount TU (pak mount adapter + request producer; no BASE counterpart; staged from caps-team-work/cap07-step2)'},
```

- **Template unit:** `unreal-unrealinitializer`. Rationale: the existing
  precedent uses `luamod` for Lua-facing TUs and `unreal-unrealinitializer` for
  TUs needing the full SDK/vendored include set (`buildstamp`, `tcselftest`).
  `RuntimePakMount.cpp` itself includes only `"RuntimePakMount.hpp"` + std
  headers, but the unit must resolve the typescan include scope and the vfs
  overlay uniformly; the template argv is a safe superset. The include-graph
  gate seeds the dep whitelist from the template's dep set — the new TU's actual
  deps are a subset, so no unapproved-add failure.
- **Mechanics already in the driver:** `make_new_units` clones the template
  argv, swaps the source, appends the object at the END of the BASE link list
  (zip-remap preserved), always-fresh in both modes.
- **Header routing:** the additions must be registered in
  `typescan-r4/CHANGES.json` `added_headers` (RuntimePakMount.hpp,
  PakRequestProducer.hpp, NativePakMountAdapter.inl, NativePakMountEntry.hpp,
  pak_mount_pins.inc) so their parent dir (`typescan-r4/include`) enters
  `include_dirs`; the four refreshed rows carry the new candidate hashes.

### 3.2 #10 — register NOTHING now (two deferred entries)

`MonoOwner.cpp` and `InvocationBoundary.cpp` include `<mono/mini/jit.h>`,
`<mono/metadata/…>`, `<mono/utils/…>`. There is no Prospero/PS5 Mono port: the
matching runtime in `caps-team-work/mono-host-research` is an Intel-Mac host
build under Rosetta. Registering either TU today would fail compilation of the
production PRX; the driver has no conditional-NEW_UNITS mechanism, and adding
one is out of scope for this plan. The UE4SSProgram.cpp stage compiles and
links WITHOUT them because:

- the `#include "MonoOwner.hpp"` line is guarded by
  `#if defined(MODBRIDGE_CAP10_ADMITTED_MONO_PORT) && … == 1`, and
- the only external reference, `Cap10Host::run_embedded_probe_once()`, sits
  under the same guard.

Deferred entries (recorded here so the future port admission has its exact
shape; template swap ALONE is insufficient — the argv additionally needs the
port's mono include dirs, which requires a driver extension with its own gate):

```python
# BLOCKED on an admitted target Mono port (see G-C10-port-admission):
'mono-owner':          {'source': HERE/'typescan-r4/candidate/src/MonoOwner.cpp',
                        'template': 'unreal-unrealinitializer',  # + mono -I dirs (argv surgery)
                        'note': 'cap10 MonoOwner TU — DEFERRED until admitted port'},
'invocation-boundary': {'source': HERE/'typescan-r4/candidate/src/InvocationBoundary.cpp',
                        'template': 'unreal-unrealinitializer',  # + mono -I dirs (argv surgery)
                        'note': 'cap10 InvocationBoundary TU — DEFERRED until admitted port'},
```

### 3.3 Packaging-side registration (the extension #7 actually needs)

`l26_packaging_driver.derive_manifest` maps `profile/CHANGES.json` candidates to
lineage overlays by NAME, so the refreshed `CodeSignatureProfile.hpp` and
`CodeImageCache.hpp` rows propagate automatically (destinations
`lifecycle/CodeSignatureProfile.hpp` and `CodeImageCache.hpp`, agent-EQ
admissions). The NEW `PakMountCodeProfile.hpp` is included by
`CodeSignatureProfile.hpp` and must therefore also be staged where the lineage
resolves it. Required extension (EQ pattern, one block):

- append a NEW lineage overlay
  `{'path': <profile/units/PakMountCodeProfile.hpp>, 'destination': 'lifecycle/PakMountCodeProfile.hpp'}`
  pinned to the profile CHANGES `added_header_hashes` row, staged AFTER the
  lineage copytree so it lands beside `lifecycle/CodeSignatureProfile.hpp`
  whose `#include "PakMountCodeProfile.hpp"` then resolves in the bridge TU;
- a missing manifest entry must be a HARD error (same law as the CopyinSink
  wiring: silently compiling the 12-pin profile without its include is exactly
  the silent-drift class this driver refuses).

For #10: `Cap10InvocationObservation.hpp` rides the existing
`outer_headers`-style admission pinned to `typescan-r4/CHANGES.json`
`added_header_hashes` (same shape as `EngineListenerOpsProfile.hpp`). Nothing
else of #10 enters the packaging manifest while the port is unadmitted; the
host `.config` files must never be staged (gate G-C10-no-host-config-leak).

### 3.4 Census impact (declare before every execute)

| Build | NEW_UNITS len | G6 link inputs (`391+len`) | P8 `.init_array`/`.fini_array` |
|---|---|---|---|
| current tree (1e49188b) | 4 | 395 | 240 / 2 |
| S2 cap07 | 5 (+`pakmount`) | **396** | 240 / 2 expected — `RuntimePakMount.cpp` has no namespace-scope objects (function-only TU; static scan). If the compiled census says +1, attribute it to this TU explicitly and re-baseline 240→241 with a receipt note (the l26 238→239→240 precedent requires attributable evidence, never a silent pass) |
| S3 cap10 | 5 (unchanged) | 396 | unchanged — the observer fragment defines function-local/thread_local state only; thread_local in a function scope adds no `.init_array` slot |
| future cap10 port | 7 | 398 | re-baseline with attribution (MonoOwner/InvocationBoundary statics) |

---

## 4. THE GATE DESIGN

Layered on the existing driver gates (G1, G2, G2q, G2r, G2s, G2t, G12, G12b,
G3, G4-ctoken, G5, G6, G7, G8, P0–P13, build stamp, build lock) plus the two
static gates. New gates below are named `G-C7-*`, `G-C10-*`, `G-C13-*` so they
read unambiguously in receipts. Gate law (2026-10-12) applies to every one: a
gate that checks zero things FAILS, verdicts print after the failed-gate check,
and every failure refuses before the receipt is written.

### 4.1 #7 step 2 — pak mount component

| Gate | Runs at | Check (nonempty by construction) |
|---|---|---|
| `G-C7-stage-pins` | stage (S1) + re-verified at S2 plan | The four originals match the recipe's SHA256+size (`04f02bc2`/`fd53a4f2`/`a4cd6c04`/`22803051`); every hunk anchor matches at its exact position; derived hashes match (`83df4c89`/`e4232f06`/`6d073dc6`/`3fa64af8`); the byte-inverse round-trips. Re-validated end-to-end today (Appendix A.2). Refusal on drift — never overwrite the integrator's newer work. |
| `G-C7-identity-pins` | S2 plan (static over `pak_mount_pins.inc` + `PakMountCodeProfile.hpp`) | Exactly 6 pakmount pin entries; IDs unique and disjoint from the existing 1–6 (profile total = 12, `kProfile` count field says 12); every pin carries label + linked address + 16-byte prefix + size; all bound to the pinned image sha `3b6cfad4…` (the seed-table image); the engine identities the adapter verifies (receiver/member/adjust/vtable/Execute) are the pinned ones; mounted-count correction is the +0x18 field (NOT the pending-encrypted +0x28 field). |
| `G-C7-abi-uniform` | S2 plan + packaging prepare | The review hold, made a gate: (a) runtime side — every compiled consumer of `CodeImageCache.hpp` is in the fresh set (G12b full-mode, no incremental waiver) and resolves to the `kMaxPins=16` candidate (G12 routing proof); (b) lineage side — the packaging manifest's `CodeImageCache.hpp` overlay carries the SAME candidate sha; (c) the two compiled copies are byte-identical (they are the same file — assert it anyway); (d) `CodeSignatureProfile.hpp` (12 pins) and `PakMountCodeProfile.hpp` staged in the same lineage scope (§3.3). Any mismatch refuses. |
| `G-C7-snapshot-abi` | S2 plan (host evidence replay) | The aggregate-return ABI remains corroborated: `tools/verify_pins.py --image <saved-image>` replays the six authenticated prefixes and ten windows / 8,235 selected bytes against the full saved-image hash (`saved-window-pass.json`, `current-source-bindings.json`, nonempty counts printed). This is explicitly HOST/source evidence, not target ABI execution — the gate records it as such and never promotes it. |
| `G-C7-producer-refusals` | S2 pre-execute (host suite) | Producer test suite green with NONEMPTY refusal coverage: missing `pakmods` disables; empty selection refuses; malformed entry refuses; >16 selection refuses; symlink root/entry refuses; non-regular/zero-size sidecars refuse; unsorted/discovery-not-all-or-nothing refuses. Also static: no fallback path to the step-1 disk installer exists in the composed tree (grep + review). |
| `G-C7-fs-throw-approved` | S2 pre-execute | `fs_throw_gate.py --files-from <SOURCE-SELECTION.json>` over the composed selection: every NEW `std::filesystem` site introduced by `PakRequestProducer.hpp`/`RuntimePakMount.*` is either a noexcept `file_status` overload (documented false-positive class — approve with `[FS-THROW-APPROVED] status-arg` markers so the count is visible) or an error_code form; the `directory_iterator(directory)` range-for MUST be reworked to an error_code form or explicitly review-approved. New unapproved sites fail the build (baseline ratchet). |
| `G-C7-no-detour` | S2 pre-execute | `no_detour_gate.py --root <candidate tree>` scans the composed tree including the new files: zero violations, nonempty file count printed. The pak mount adapter uses engine FString allocation/destruction and the already-bound MountPak delegate — function/data calls only; no exec-page writes are expected, and any hit is a design violation, not a gate problem. |
| `G-C7-census` | S2 execute | G6 link inputs == 396 exactly; P8 == 240/2 or an explicitly attributed re-baseline (§3.4). |
| `G-C7-runtime-review` | between S2 composition and execute | The interrupted independent review completed with a final verdict file; its two named holds (shared-cache layout; missing signed-boundary / patch-bonus-overflow / failed-post-call-snapshot / wrapper-descent regressions) either resolved with new evidence or explicitly accepted by the reviewer. Unresolved holds block `--execute`. |

### 4.2 #10 — embedded Mono probe component

| Gate | Runs at | Check |
|---|---|---|
| `G-C10-source-repin` | S3 stage | Current `UE4SSProgram.cpp` sha recorded (today `ad3ae621…`; the shipped `fe9fe7bc…` pin is STALE and must be refreshed by a fresh stager run, never blind-run); the 3 anchors each occur exactly-once (verified today: 1/1/1); zero pre-existing Cap10 markers (verified: 0); derived sha recomputed; exact inverse verified; `full_source_retained: false`. |
| `G-C10-observer-embed` | S3 plan | `Cap10InvocationObservation.hpp` present as an added_header with matching hash; staged UE4SSProgram.cpp contains the observer fragment exactly once, wholly inside `#ifdef __PROSPERO__`; `Cap10BridgeAdmissionScope` ctor/dtor defined in the fragment; `m_native_context.on_engine_initialized()` still called exactly once, now inside the scope. |
| `G-C10-probe-image` | S3 plan (packaging later) | `managed/Probe.dll`: sha256 `c328dca1…`, 3,584 bytes, references only mscorlib, compiled against the .NET Framework 4.7.2 reference (`evidence/COMPILE.json` binding). `ProbeImage.hpp`'s embedded array length and content hash equal Probe.dll's. If/when the owner TU is admitted: the registered assembly bytes in the runtime match this sha. |
| `G-C10-port-admission` | every build (fail-closed default) | `MODBRIDGE_CAP10_ADMITTED_MONO_PORT` is ABSENT from every production argv (gate scans the composed argv set); `MonoOwner.hpp` include and `run_embedded_probe_once()` therefore compile to nothing. Admission (only under explicit authorization) additionally requires: an admitted target Mono port identity; genuine corlib/config paths; `MONO_AOT_MODE_INTERP` execution; the `mono-dl.c` owned-name loader patch (3 anchors on base `b7ccf30b…`) compiled into that port; the deferred NEW_UNITS entries from §3.2 with mono include dirs. Absent any element, the define stays off and the gate passes as OFF — the probe is unreachable, by design. |
| `G-C10-boundary-exception-safety` | S3 pre-execute (static) + host receipt | Static over the composed observer: every probe/observation entry path terminates in `finish_probe` (success, refusal codes, or `catch (…) → finish_probe(-5, false)`); no exception escapes the admission scope; wrong-signature refusals present; serial/cookie/phase checks present; admission scope restores state on unwind. Runtime exception-safety evidence remains the matching-Mono host receipt (`MONO-HOST-CHECK.json`: two Run invocations, one deliberate managed exception recovered, two wrong-signature refusals, GC transitions, pinned boxed Int32) — recorded as HOST evidence until a port exists; the failed `System.Native` interpreter check is NOT evidence and stays diagnosed-not-repeated. |
| `G-C10-no-host-config-leak` | S3 packaging | No `*.config` from `cap10/managed/` or `cap10/host-check/` (Mac test-library mappings) is staged into any package dir. |
| `G-C10-runtime-review` | between S3 composition and execute | The interrupted #10 review completed with a final verdict confirming the helper receipt's narrow scope, the re-pin correctness, and the deferred-port boundaries. |

### 4.3 #1/#3 — kernel provider route (fail-closed selection gate)

| Gate | Runs at | Check |
|---|---|---|
| `G-C13-selection-empty` | every build | The integration selection for #1/#3 is EMPTY and must be: no kernel client/provider source, no kernel-resident code, no modified kstuff anywhere in the build inputs (scan the composed selection + added files). ANY nonempty #1/#3 content fails the gate. Basis: the pinned kstuff dispatcher (archive sha `b4fcf169…`, audited) exposes no generic kernel-VA copy operation and its private helpers are not userspace-callable; the possible loader inherited-context route (pipe/socket context + `kernel_copyin/kernel_copyout`) is UNVALIDATED (ABI/init, source↔resident identity, privilege/context compatibility unestablished; partial-I/O counts unreliable); no client exists. This gate encodes "do not integrate an admitted-nothing". |
| `G-C13-provider-identity` | future, only when a provider is admitted | Independent source↔resident-binary identity binding (archive hash, member shas, binary service inventory); redistribution/license establishment (the kstuff-lite archive has NO LICENSE member — redistribution permission unestablished); provider route identity and privilege sufficiency with no new gate/bypass. |
| `G-C13-readpatch-contract` | future | The read/patch contract validation from the acceptance doc: owned bounded targets; independent known answers; exact byte counts and partial-I/O semantics; compare-before-write; effect readback; exact restoration; revoke/drain; recovery proven through a controlled failure on the admitted interface; 3 clean passes across 3 separate restarts with fresh identities per boot (Phase B law), one console owner and one test per boot. |

---

## 5. THE BUILD SEQUENCE (runbook)

One command per step, with rollback. `SNAP` = the snapshot id printed by
`harvest/snapshot_tree.sh`. Every build step is FULL mode (no `--incremental`),
variant A only (never `L26_BUILD_B`/`L26_PACK_B`), under the driver's own build
lock. Each `--execute` refuses to overwrite existing variant dirs — archive the
previous `l26-A`/package dirs first when re-running a step.

**S0 — Preconditions (repeat before every step)**

```
# 0.1 verify the tree matches this plan's pins (refuse on drift)
python3 - <<'PY'
import hashlib,pathlib
W=pathlib.Path('/WORKSPACE/.codex/worktrees/translation-core/what')
pins={'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp':'04f02bc23bdd1701',
 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp':'fd53a4f295390677',
 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp':'ad3ae621a7c86d77',
 'outputs/2026-10-08-l26-runtime/profile/units/CodeSignatureProfile.hpp':'a4cd6c04eb2f9af0',
 'outputs/2026-10-08-l26-runtime/profile/units/CodeImageCache.hpp':'22803051e5406e1c'}
bad=[p for p,s in pins.items() if not hashlib.sha256((W/p).read_bytes()).hexdigest().startswith(s)]
print('DRIFT:',bad) if bad else print('PINS OK (5/5)')
PY
# 0.2 static gates on the clean tree (both must PASS with nonzero counts)
python3 WHAT/outputs/2026-10-08-l25-prep/harvest/fs_throw_gate.py --baseline <fs-baseline-1e49188b>
python3 WHAT/outputs/2026-10-08-l25-prep/harvest/no_detour_gate.py
# 0.3 snapshot + stamp env
bash WHAT/outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh   # prints SNAPSHOT-ID
export BRIDGE_SNAPSHOT_ID=<SNAPSHOT-ID>
```

Rollback S0: none needed (read-only).

**S1 — #7 read-only stage (writes only inside cap07-step2/derived/)**

```
python3 CAPS/cap07-step2/tools/stage_current.py \
  --root WHAT --out CAPS/cap07-step2/derived/INTEGRATION-A
```

Expected: `PASS: 4 exact sources/anchors/inverses; 11 derived files` + `STAGE.json`.
Rollback: `rm -rf CAPS/cap07-step2/derived/INTEGRATION-A`.

**S2 — #7 tree composition, review, build, package (label `cap07`)**

```
# 2.1 compose the four derived files into the tree as refreshed candidates and
#     add the seven additions (integrator source edits; see §1.2 destinations),
#     then refresh typescan-r4/profile CHANGES rows + added_headers +
#     added_header_hashes and add the NEW_UNITS 'pakmount' entry (§3.1) and the
#     PakMountCodeProfile lineage extension (§3.3).
# 2.2 independent runtime review gate (G-C7-runtime-review) — must be green
#     BEFORE 2.4; resolve the abi-uniform + regression holds.
# 2.3 plan + component gates (refuses on any drift/no-op row)
python3 L26/build_variants.py --plan
python3 CAPS/cap07-step2/tools/verify_pins.py --image <saved-image>   # G-C7-snapshot-abi
bash CAPS/cap07-step2/tests/run.sh CAPS/cap07-step2/tests/INTEGRATION-A.log  # G-C7-producer-refusals
python3 WHAT/outputs/2026-10-08-l25-prep/harvest/fs_throw_gate.py --files-from <pair-root>/l26-A/SOURCE-SELECTION.json
python3 WHAT/outputs/2026-10-08-l25-prep/harvest/no_detour_gate.py
# 2.4 build + package
python3 L26/build_variants.py --execute
python3 L26/l26_packaging_driver.py --plan
python3 L26/l26_packaging_driver.py --execute
```

Watch: G2q second-hop takes `patch-rebased-onto-revised-base` mode and MUST
stay clean (verified in design; if it ever fails, agent-Q/coordinator rebase —
never force). G12b lists every CodeImageCache/CodeSignatureProfile consumer
recompiled. G6 == 396. P7 == 207 imports; P8 == 240/2 or attributed
re-baseline.
Rollback S2: revert the four tree files to their S0 bytes (hash-pinned copies
or git), delete the seven additions, revert CHANGES.json + NEW_UNITS + the
packaging extension, `mv` the produced `l26-A`/package dirs to an archive name,
re-run `--plan` to prove the clean tree still selects the previous state.

**S3 — #10 re-pin, tree composition, review, build, package (label `cap10`)**

```
# 3.1 re-pin: update cap10/tools/stage_current.py SOURCE_SHA to the CURRENT
#     UE4SSProgram.cpp sha (ad3ae621… at plan time) and re-run it — it writes
#     native/current-owner.patch + prints the NEW derived sha (record both in
#     evidence/CURRENT-STAGE.json). Anchors were verified exact-once today.
python3 CAPS/cap10/tools/stage_current.py
# 3.2 compose the staged UE4SSProgram.cpp as the refreshed 'program' row
#     candidate; add Cap10InvocationObservation.hpp (+ observer_fragment.inc
#     provenance) as added_headers; do NOT add mono TUs; leave
#     MODBRIDGE_CAP10_ADMITTED_MONO_PORT undefined everywhere.
# 3.3 independent runtime review gate (G-C10-runtime-review) — green BEFORE 3.5.
# 3.4 plan + component gates
python3 L26/build_variants.py --plan
python3 WHAT/outputs/2026-10-08-l25-prep/harvest/fs_throw_gate.py --files-from <SOURCE-SELECTION>
python3 WHAT/outputs/2026-10-08-l25-prep/harvest/no_detour_gate.py
# 3.5 build + package (same flow as S2; census unchanged 396)
python3 L26/build_variants.py --execute
python3 L26/l26_packaging_driver.py --plan && python3 L26/l26_packaging_driver.py --execute
```

Rollback S3: revert UE4SSProgram.cpp + CHANGES + added header; archive build/
package dirs; `--plan` to confirm.

**S4 — round5 staged items (each its own labelled build, AFTER re-anchoring)**

Order: lookup-speed → group-b → shim-fix → lua-alloc. For each item: re-anchor
its patches onto the CURRENT bytes (their 2026-10-11 pins are stale: lookup-speed
expects `6a9087b5`/`63e6e057`/`a8e9b6f4`, tree has moved), refresh rows, then
the standard loop:

```
python3 L26/build_variants.py --plan        # gates first
python3 L26/build_variants.py --execute
python3 L26/l26_packaging_driver.py --execute
```

NEVER apply `IP-shim-root/01-ue4ssprogram-shim-root.patch` (crashed PRX
`1c48239d`; stale pin `3cc011d7` besides). The no-detour gate additionally
refuses the world-fix event-map-logger patch family unless it is the pure-VMT
redesign.
Rollback per item: revert the touched files + rows; archive dirs; `--plan`.

**S5 — #1/#3**: no command. `G-C13-selection-empty` is enforced by inspection of
every plan/execute receipt in S2–S4 (no kernel client/provider content appears
in any selection). Any future nonempty #1/#3 step requires the §4.3 future
gates plus explicit console authorization.

---

## 6. THE NON-CONFLICT GUARANTEE

### 6.1 #7 vs #10 (verified from the stagers' own code)

- #7's `stage_current.py` transforms exactly four files (recipe
  `files[]`: UnrealInitializer.cpp, LuaMod.cpp,
  profile/units/CodeSignatureProfile.hpp, profile/units/CodeImageCache.hpp) and
  adds seven files (§1.2). #10's `stage_current.py` transforms exactly one file
  (UE4SSProgram.cpp, three anchors) and its additions are six differently-named
  files. **Intersection of edited originals: empty. Intersection of added
  filenames: empty.**
- #10's engine-initialized callback anchor (`m_native_context.on_engine_initialized`)
  lives in UE4SSProgram.cpp only; #7's trigger rides `VmtEngineTickDrain` in
  LuaMod.cpp — different units (`program` vs `luamod`), no shared TU.

### 6.2 #7 vs the driver's Q second hop (verified by simulation, 2026-10-13)

Q's copyin-probe patch (`patches/unrealinitializer-copyin-probe-liveness.patch`,
hunks near the `bridge_image_range` declaration, the `shadow(admission::Budget&)`
region, and the Phase-6 cleanup notice) was applied with
`/usr/bin/patch --forward --no-backup-if-mismatch` onto the byte-exact #7-derived
UnrealInitializer.cpp (produced in-memory from the recipe against the current
tree): **rc=0, zero .rej files, no fuzz**. #7's three replaced regions and Q's
three anchors are textually disjoint. The driver therefore keeps composing
`row candidate (#7) → Q overlay → composed staging file` with its existing
G2q pin/order gates enforcing cleanliness at plan and execute time.

### 6.3 #7/#10 vs the main tree's staged patches

- lookup-speed / group-b / lua-alloc touch UnrealInitializer.cpp, LuaMod.cpp,
  UObjectGlobals.cpp, ClassListener.cpp, LuaUEnum.cpp, LuaMadeSimple.* —
  UE4SSProgram.cpp is touched by none of them, so **#10 has zero overlap with
  every staged patch except the forbidden IP-shim-root** (excluded by rule, and
  additionally stale-pinned).
- #7 overlaps lookup-speed (UnrealInitializer.cpp), group-b and lua-alloc
  (LuaMod.cpp) in FILE only; none of those patches are in the tree today, so
  #7's composition is against bytes they never touched. The serialization in §2
  (each later patch re-anchored onto the post-#7/post-#10 bytes inside its own
  labelled build) is the complete conflict-avoidance mechanism; every re-anchor
  is exact-pin-verified by its own stager/row law.
- Variant tokens: #7's derived UnrealInitializer.cpp preserves exactly one
  `MODBRIDGE_L25_ENGINE_ADD_LISTENER` site and contains zero
  `MODBRIDGE_L25_SKIP_DELETE_LISTENER` (matching the P2-patched tree), so
  `G4-ctoken-present` and the dead-B design are unaffected (verified on the
  derived bytes today).

### 6.4 Residual risks (declared, not hidden)

- Q's patch rebase is proven against TODAY'S #7 output; if the #7 review forces
  source changes, re-run the §6.2 simulation before execute (one command).
- The packaging lineage compile of the 12-pin profile is verified by §3.3's
  extension at packaging time, not by the runtime build; a missing extension
  fails loudly at prepare (hard error by design).
- UE4SSProgram.cpp is a moving file (Claude's teams work independently): S3's
  re-pin must be computed at composition time, not read from this document.

---

## Appendix A — verification receipts (2026-10-13, all read-only)

A.1 Current disk state vs every binding (sha256, first 16 hex):

| File | Disk | Binding | Verdict |
|---|---|---|---|
| UnrealInitializer.cpp | `04f02bc23bdd1701` | cap07 recipe `04f02bc2…` | MATCH |
| LuaMod.cpp | `fd53a4f295390677` | cap07 recipe + handoff baseline | MATCH |
| CodeSignatureProfile.hpp | `a4cd6c04eb2f9af0` | cap07 recipe | MATCH |
| CodeImageCache.hpp | `22803051e5406e1c` | cap07 recipe | MATCH |
| UE4SSProgram.cpp | `ad3ae621a7c86d77` | cap10 pin `fe9fe7bc…` | **DRIFT**; anchors 1/1/1 exact; 0 Cap10 markers; CHANGES row synced to disk |
| UObjectGlobals.cpp | `d00a3dbd0f3a4458` | lookup-speed pin `63e6e057…` | DRIFT (staged patch, needs re-anchor) |
| ClassListener.cpp | `84e863fa86b1db98` | lookup-speed pin `a8e9b6f4…` | DRIFT (staged patch) |
| UnrealInitializer.cpp | `04f02bc2…` | lookup-speed pin `6a9087b5…` | DRIFT (staged patch) |
| UE4SSProgram.cpp | `ad3ae621…` | IP-shim-root pin `3cc011d7…` | DRIFT + patch FORBIDDEN |

A.2 cap07 transform re-validation (read-only replication of
`stage_current.py`'s algorithm against the current tree): all four files
len_ok/sha_ok/anchors_ok TRUE and derived hashes reproduce
`83df4c894cda5948` (UnrealInitializer), `e4232f06ffd5eb0a` (LuaMod),
`6d073dc60d4fa534` (CodeSignatureProfile), `3fa64af86ea25543` (CodeImageCache).

A.3 Derived-content spot checks: UI derived keeps CTOKEN×1, B-token×0,
`NativePakMountAdapter`×1, `RuntimePakMount.hpp` include×1; LuaMod derived
keeps `VmtEngineTickDrain`×3; CSP derived has 6 `rc::pakmountprofile` rows and
pin count 12; CIC derived has `kMaxPins=16`.

A.4 Q-overlay simulation: applied to #7-derived UnrealInitializer.cpp in a
temp dir — rc=0, no `.rej`, no fuzz.

A.5 BASE facts: l25-A pass1 has 374 selected units; `luamod`, `program`,
`unreal-unrealinitializer` units consume LuaMod.cpp / UE4SSProgram.cpp /
UnrealInitializer.cpp respectively; current NEW_UNITS = 4 (`lua-lfb`,
`buildstamp`, `tcselftest`, `lua-pathshim`); G6 census law `391+len(NEW_UNITS)`;
P8 currently 240/2; thread part has zero file rows (placeholder).

A.6 #1/#3: `cap01-03/HANDOFF.md` — no admitted callable kernel provider; kstuff
archive `b4fcf169…` audited (no generic kernel-VA copy op; helpers need
privileged internal context; no LICENSE member); loader inherited-context route
unvalidated, no client written; acceptance requirements captured in §4.3.
