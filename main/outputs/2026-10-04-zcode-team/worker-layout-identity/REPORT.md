# REPORT — worker-layout-identity (2026-10-04-zcode-team)

Task: resolve the staged pair's identity fields `engine_revision` and `build_id`
(the explicit refusal points of NativeLayoutBinding.cpp). Bounded: ~29 tool calls,
no nested subagents, no network/console, writes only in this directory.

## What was done (coordinator-corrected order)

1. Binding consumer traced FIRST (per controller correction): read the pinned
   `ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp` throw conditions.
   Stage gate refuses only empty strings; publish gate runtime-compares ONLY
   major/minor/non_shipping/stats. EngineRevision/BuildId are staging provenance,
   never runtime-compared, never map keys. Parser (`ue4ss-native-layout-parser-1`)
   defines them as opaque nonempty printable ASCII ≤512 bytes; numeric conversion
   is not a mechanism (both are `std::string`). Assembler
   (`assemble_staged_pair.py`) demands nonempty printable ASCII from
   controller-supplied identity_facts ("never defaults").
2. Identity-source semantics located in the port: pinned resolver
   `engine_version.rs` (four scan strategies incl. FEngineVersionBase store scan
   with Changelist at +8) and `build_properties.rs` (axes fingerprint — other
   worker's field). Retained `MemberVariableLayout.ini` fixtures use placeholder
   text, proving format-only acceptance; real values must be evidence-derived.
3. Survey metadata checked: run-release-1 stdout has `engine_version status=0
   major=5 minor=6` only — no revision/build-id facts existed before this worker.
4. THEN the image was searched (194,241,424-byte retained decrypted-eboot.bin):
   - Full multi-encoding string scans (section 5 negatives — branch strings,
     version strings, UE BUILD_ID value strings are all absent).
   - ELF parsed: 14 program headers, GNU build-id note found.
   - FEngineVersionBase init sequence located and hand-decoded.

## Result

Deliverable: `IDENTITY-RESOLUTION.md` (values, evidence bytes, read paths,
acceptance matrix). Headline:

- **engine_revision = `5.6.1-0+++UE5`** — new static evidence: store site file
  0x1F37DE gives Major=5 Minor=6 Patch=1; helpers decode to Changelist=0
  (return-0 stub at vaddr 0xE0) and Branch L"UE5" (accessor at vaddr 0xF0 →
  wide string at file 0x782A782). Patch=1 makes this 5.6.**1** — new fact
  vs the survey's major/minor-only 5.6.
- **build_id = `04bb4330a7d71dcefe1cc2c4d19769d5`** — ELF `NT_GNU_BUILD_ID`
  note (name "GNU", type 3, descsz 20) at file 0xB93B250 / vaddr 0xBC5F250.
  No UE FModuleManager BUILD_ID value string exists in the image (only the
  BuildIdOverride config machinery, UTF-16) — the GNU note is the only embedded
  build identity hash.
- Both proposed values pass every binding criterion; publish-time equality for
  major/minor (5/6) is corroborated by the survey. Build axes
  non_shipping/stats remain the other open gate (explicitly out of scope).

## Key caveats (honesty boundaries)

- `5.6.1-0+++UE5` is UE's ToString shape assembled from five verified components;
  the literal string exists only at runtime. Controller ratifies the assembly and
  the build_id := GNU-note mapping (decision points in IDENTITY-RESOLUTION.md §6).
- All addresses are file-image vaddr labels; runtime = load_base + vaddr, and
  load_base is unresolved in all saved evidence. Segment mapping is NOT uniform:
  +0x4000 for LOADs 1-4 but **+0x324000 for LOAD 5** (where the note lives).
  Every identity read is in mapped R/RX spans → direct reads, no mprotect needed.
- Live in-process confirmation stays with the controller per its own assembler
  contract; a bounded caller trace from vaddr 0x1EF7DE could locate the
  FEngineVersion singleton for a genuine live read (future work, distinct IDs).

## Files

- `CLAIM.json` (written first), `IDENTITY-RESOLUTION.md`, `REPORT.md` (this file, last).
- Read-only elsewhere; no executable source or engine files touched.
