# Shared DDS module implementation evidence

Worktree: `/WORKSPACE/.codex/worktrees/translation-core/what`.

## Delivered scope

Added `include/modbridge/dds.hpp`, `src/dds.cpp`, `tests/dds_tests.cpp` and the independent test-only `tests/dds_fixture.hpp` under `outputs/ps5-mod-bridge`. Updated `core.hpp`, binary inspection, inventory, Makefile and inventory/planner regressions. No native SDK/probe files, staging/index, commits or console connections were touched by this implementer.

The dependency-free C++20 reader preserves exact raw header fields, DXGI/FourCC encoding, alpha bits, dimensions and array/cube/volume declarations. It calculates tight BC layout with checked arithmetic, bounds subresources before generating their list, hashes only available whole ranges and separates a capped prefix from true payload truncation. Normalization owns exact header, ordered BC payloads and opaque trailer; const accessors expose immutable views and lossless reconstruction reproduces source bytes. Normalization rejects input above 256 MiB before reading its header or allocating retained storage. Every DDS retains unresolved PS5 writer and game/build acceptance requirements.

Canonical header sizes are strict. Unsupported aliases, numeric/unknown FourCC, non-BC DXGI, BC/1D, partial legacy cubes and reserved alpha semantics remain unsupported or ineligible. Contradictory topology or compressed/uncompressed pixel declarations are rejected conservatively. The latter interpretation follows the distinct compressed/custom versus uncompressed declaration meanings in [Microsoft DDS_PIXELFORMAT](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-pixelformat); it is not permissive header repair.

FourCC is binary evidence: `fourcc_bytes` and `fourcc_value` retain exact raw values; `fourcc` contains printable ASCII only, otherwise null. This avoids invalid UTF-8 entering JSON. Header bytes retained by a normal form stay exact.

Inspection uses a 256 MiB hash-work bound in addition to the 65,536-subresource bound. Over-budget full spans may establish bounded layout, but do not receive fabricated payload/whole-input hashes, complete inspection or normal-form eligibility. Inventory's independent streamed whole-file digest is unchanged.

## Actual RED evidence

Commands ran from `outputs/ps5-mod-bridge`.

First behavioral RED, before production DDS changes:

```text
$ c++ -Iinclude -Itests -isystem third_party -isystem third_party/picosha2 -std=c++20 tests/main.cpp tests/dds_tests.cpp src/hash.cpp src/inspection.cpp -o build/test-dds-red && build/test-dds-red
FAIL dds_magic_dispatch_and_edge_block_pitches: tests/dds_tests.cpp:11: j.at("kind") == "dds_texture"
  actual: "lua"
expected: dds_texture
FAIL dds_cube_array_order_and_volume_depth: [json.exception.out_of_range.403] key 'subresources' not found
FAIL dds_unsupported_and_invalid_declarations: tests/dds_tests.cpp:37: j.at("kind") == "dds_texture"
  actual: "data"
expected: dds_texture
FAIL dds_exact_bc_format_identity: [json.exception.out_of_range.403] key 'format' not found
FAIL dds_layout_limits_and_checked_overflow: [json.exception.out_of_range.403] key 'reason' not found
5 tests, 5 failures
```

After the reader passed those contracts, the owning form and conflicting topology regressions were run against the incomplete implementation:

```text
$ make test-dds
FAIL dds_neutral_owns_immutable_lossless_bytes: tests/dds_tests.cpp:100: check failed: n.has_value()
FAIL dds_conflicting_topology_flags_rejected: tests/dds_tests.cpp:117: inspect_dds(b,"x",b.size()).at("facts").at("reason") == "cube_declaration_conflict"
  actual: "complete"
expected: cube_declaration_conflict
8 tests, 2 failures
```

The preceding attempt could not compile because this repository's equality reporter cannot stream vectors. The fixture assertion was corrected to boolean vector equality, and the independent literal trailer digest typo was corrected using Python `hashlib`; neither compile error nor typo is counted as behavioral RED evidence.

Self-review then caught a missing luminance declaration conflict:

```text
$ make test-dds
FAIL dds_conflicting_luminance_declaration_rejected: tests/dds_tests.cpp:124: inspect_dds(b,"x",b.size()).at("facts").at("reason") == "pixel_format_declaration_conflict"
  actual: "complete"
expected: pixel_format_declaration_conflict
9 tests, 1 failures
```

Root review identified unchecked binary FourCC serialization; the independent regression reproduced it before the fix:

```text
$ make test-dds
FAIL dds_binary_fourcc_serializes_as_opaque_evidence: [json.exception.type_error.316] invalid UTF-8 byte at index 0: 0xFF
13 tests, 1 failures
```

## Actual GREEN verification

The synthetic fixtures supply independently hand-calculated payload sizes, pitches and offsets. Coverage includes all 21 accepted DXGI enums and 11 legacy aliases; edge dimensions; zero/impossible mip chains; cube-array face order; full and partial legacy cubes; 2D arrays; volume depth halving; reserved alpha; unsupported encodings; contradictory declarations; canonical/short/capped headers; prefix versus actual truncation; trailer range/hash; overflow and work limits; immutable ownership and exact round trip. The byte-cap test maps inaccessible sparse virtual storage, so a read before the cap check would fail. The FourCC regression exercises all 256 byte values in opaque identifiers.

```text
$ make test-dds test-inventory test-planner
8 tests, 0 failures
22 tests, 0 failures
35 tests, 0 failures
```

After expanded edge/cap coverage, complete C++ core verification:

```text
$ make test-core > build/dds-core-verification.log 2>&1
96 tests, 0 failures
```

That complete-core checkpoint preceded the root FourCC fix. The latest focused normal DDS result includes that fix:

```text
$ make test-dds
13 tests, 0 failures
```

Focused ASan/UBSan verification before the FourCC fix:

```text
$ make BUILD_DIR=build-dds-sanitize CXXFLAGS='-std=c++20 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test-dds test-inventory test-planner > build/dds-sanitizer-verification.log 2>&1
12 tests, 0 failures
22 tests, 0 failures
35 tests, 0 failures
```

No compiler warnings or sanitizer diagnostics appeared in these completed checks. Latest DDS sanitizer verification is recorded below when complete. The controller owns the final complete host/account/UI regression and independent review checkpoint after all reviewed changes.

## Remaining product limits

This is a reusable preparation-core architectural subsystem and host evidence. It does not decode/recompress pixels, assign channels/color primaries, emit PS5 texture tiling, execute on PS5, install a mod or prove acceptance by any game. The private manager's broader Nexus formats/engines, PS5-native runtime, verified target writer and game/build acceptance remain separate work. Unsupported inputs and missing evidence cannot become installation permission because parsing or a lossless host round trip succeeded.

Latest focused sanitizer checkpoint, after the FourCC fix (exit 0; no compiler warnings or sanitizer diagnostics):

```text
$ make BUILD_DIR=build-dds-sanitize CXXFLAGS='-std=c++20 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test-dds > build/dds-sanitizer-fourcc-verification.log 2>&1
13 tests, 0 failures
```

`git diff --check` also completed with exit 0.

Root final regression checkpoint after the FourCC fix: `make test-all` using bundled Python3.12/Node completed with exit0: 97 C++ tests, 96 Python tests, UI tests and actual-host browser integration passed. This is host verification; no native DDS execution or PS5 writer/game acceptance was asserted.
