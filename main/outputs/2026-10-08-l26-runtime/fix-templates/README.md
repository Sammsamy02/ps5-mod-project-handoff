# l26-runtime fix-templates — walk-finds-nothing pre-staged fixes (agent CY)

Authored 2026-10-08. READ-ONLY pre-staging: nothing in this directory is
consumed by any build; every tree referenced was read read-only. The three
candidate root causes (A/B/C) each get a template directory:

| Dir | Candidate | One-line failure mode | Fed by audit |
|---|---|---|---|
| `candidate-A-objectflags/` | A | A UObjectBase/FUObjectItem member is read at a wrong offset in the walk's filter — garbage exclusion bits | CU (UObject/flags offsets), CW (walk mechanics) |
| `candidate-B-canonical-class/` | B | `IsA<UClass>`'s canonical class pointer resolves wrong — every object excluded as UClass | CR/CT (class resolution / type-scan), CW |
| `candidate-C-name-compare/` | C | The class-name `Equals` compare reads the wrong NamePrivate slot / number-form — nothing matches | CS (string compare + walk harness), CW |

CR/CT/CU/CW/CS reports had NOT landed under `outputs/2026-10-08-l25-prep/`
at authorship time (verified: no matching files). DECISION.md maps each
audit's possible VERDICT to a template variant; fill the report paths when
they land. CO's landed walk audit (`agent-CO-walk-audit.md`) is the shared
baseline and is cited throughout.

## How to use a template

1. Read DECISION.md, bind the landed audit report to a template variant.
2. Copy that template's `values.example.json` to `values.json` — the ONLY
   place numbers/strings are typed (decimal is the single authority; hex INI
   spellings are derived, AS/AM law).
3. Fill the `<<SLOT>>` markers in the template's diff blocks from values.json.
4. Land edits ONLY as l26 part candidates + CHANGES.json rows (below). Never
   edit the port, l25, or any frozen original in place.
5. Run the template's verification set; all must be green before any packet.

## The consumption law (build_runtime.py, verified in source)

`outputs/2026-10-08-l26-runtime/build_runtime.py` is the only consumer of
l26 changes. It reads EXACTLY `HERE/<part>/CHANGES.json` for
`part in ['profile','typescan','thread']` — actually
`['profile','typescan-r4','thread']` (line ~40). Consequences:

- New candidate files must live under one of those three part dirs (or the
  parts list in build_runtime.py must be widened — a driver-level change with
  precedent, but avoid it; A/B/C fixes all fit existing parts).
- Each row in `files[]` MUST carry, and the driver re-verifies:
  `original` (the path the BASE l25-A build actually consumed — PORT path for
  never-overlaid files, the prior tree's candidate for already-overlaid ones),
  `candidate` (under `outputs/2026-10-08-l26-runtime/`), `original_sha256`
  (frozen pin), `candidate_sha256` (recomputed after every edit).
  typescan-r4 rows additionally carry `copied_original` +
  `copied_original_sha256` (the frozen copy under `typescan-r4/original/`).
- Headers are redirected by (basename, original-sha) vfs aliasing across the
  base dependency universe — a candidate header can shadow a PORT header
  without touching include spellings (this is how
  `candidate/include/Unreal/NameTypes.hpp` already overlays the PORT header).
- Profile part rows additionally keep `profile_digest` = sha256 of the
  canonical JSON (`json.dumps(p, sort_keys=True, separators=(',',':'))`),
  mirrored in THREE places: `profile/CHANGES.json`,
  `profile/units/CompiledNativeLayoutDeclarations.hpp` string[3], and
  `profile/units/RuntimeBuildProfile.hpp` `kDigest`. The boot preflight
  (`candidate/src/UnrealInitializer.cpp:1384-1415`, `[BuildProfile]` /
  `[LayoutPreflight]`) REFUSES startup on any layer disagreeing — fail-closed,
  so a half-updated profile cannot ship quietly.
- Never re-run `profile/generate_overlay.py` after landing a profile edit
  (it regenerates from l23 and erases corrections; AS/AM §6 law).

## Shared provenance anchors (sha256 at authorship time)

| File | sha256 |
|---|---|
| PORT `unreal/include/Unreal/VersionedContainer/Flags.hpp` | `ecfed9560854e0658b77c20e05f2f4dbd4cb2023a704d72ce6436987875e0104` |
| PORT `unreal/src/VersionedContainer/UObject425.cpp` | `daa27fcc459c7224f5255b7366e262b68cc3e308a617dd7289d8a0b26d7a9fe9` |
| PORT `unreal/src/UObject.cpp` | `93825320514682d59fcd257173782446c3aca10dfdd4e3ab78cbb8fdb64813ec` |
| PORT `unreal/include/Unreal/NameTypes.hpp` | `5fa8d93e7c5d394f...` (full hash in ts4 CHANGES row) |
| CAND `typescan-r4/candidate/src/UObjectGlobals.cpp` | `12470d77b3a3ebcf39290aee366b828ec8fa99a16d0499bf6dba08aa52ed2228` |
| CAND `typescan-r4/candidate/src/TypeChecker.cpp` | `c05b06a7db9a50976f7b2d783c9f196b6b74698b872605e978c8af6f1f20ac7d` |
| CAND `typescan-r4/candidate/src/UObject.cpp` | `ee5607e1bdd43e66fcbd3f3eccc08fe1e992d70809e504f90268a4a3343018c0` |
| CAND `typescan-r4/candidate/src/UObjectArray.cpp` | `ea7221e8d7537e9b0dd36ddc0ac26a2c76050f343fdf7e8e2b8b03f297281f29` |
| CAND `typescan-r4/candidate/include/Unreal/NameTypes.hpp` | `3723dbe34353cca5f5f4c2d4f27d4afb61b3b0e857d89cc1f9976c6e1f1a6502` |
| `profile/profiles/townfall-build-facts.json` | `95d22dbe494a225b53e1df78864d66e34a4345adf962eee5e413909b9e49b6dd` |
| `profile/units/CompiledNativeLayoutDeclarations.hpp` | `bf9419dd3374817414566d6cddd2b54f34ecdeb0e76c6cad48d82ba0767c5d86` |
| `profile/units/RuntimeBuildProfile.hpp` | `09a14712c972ffe1a1e6967c5bc86eb88d918ebbc898727b12f4804c6d2af3aa` |
| `profile/units/JoinedLayoutPreflight.hpp` | `1646a99e2184ea576eece3303e762132a7835b3782c3cee0545ba1fe03525a2c` |

(PORT = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port`,
CAND = `outputs/2026-10-08-l26-runtime/typescan-r4`.)

## Shared verification assets

- Host walk harness (CD-built, verbatim port regions, sha-pinned):
  `/tmp/l26-ufunc-harness/` — rebuild+run via `python3 extract_and_build.py`.
  Regions already include `is_valid_object_for_findxof.inc`,
  `findfirstof_fname.inc`, `findfirstof_string.inc`, `fname_equals.inc`,
  `staticfindobject_slow.inc` — all three templates extend this harness.
- Profile suite: `python3 outputs/2026-10-08-l26-runtime/profile/tests/run_tests.py`
- Scan-refusal suite: `python3 outputs/2026-10-08-l26-runtime/tests/run_tests.py`
- Compile+link proof (two passes, full import graph):
  `python3 outputs/2026-10-08-l26-runtime/build_runtime.py`
  (writes `outputs/2026-10-08-l26-build-r2/`; fails loudly on any CHANGES
  row sha drift or unapproved include-graph addition).
- Console evidence for final acceptance remains the authorized one-launch
  ladder (CK acceptance ladder); templates prove host-side only.

## Authorization boundary

Templates are proposal artifacts. No console contact, no uploads, no engine
writes. All edits land as l26 candidates under the three part dirs with
CHANGES rows; every original stays byte-frozen. Fake-sign/upload/launch
rules per PS5-MODDING-MISSION.md remain in force.
