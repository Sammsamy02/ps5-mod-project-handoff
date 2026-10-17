# l23 container image/profile evidence

LOCAL ONLY. No console contact, game writes, source upload, Git commits, or native calls.

## Image result

Authenticated saved executable SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, 194241424 bytes. Measurements use ELF linked addresses with PT_LOAD file translation and DT_RELA bounds. `tools/measure_containers.py` replays 30 exact decoded instruction assertions, three named constructor bindings, nine executable relocation targets and four semantic strings. Full bytes and bounded disassembly are retained in `evidence/image-functions.json` and `evidence/image-disassembly.txt`.

| Field | Image byte offset | Independent classification |
|---|---:|---|
| FMapProperty::KeyProp | 0x70 | Named Map constructor binds vptr 0x7ef0ff0; copy-slot helper captures the +0x70/+0x78 pair. Callback 0x42cd10 loads the first helper property, forwards to its operation, and its return determines hash bucket placement in 0x32f0f0. TMAP formatter also places this property first. |
| FMapProperty::ValueProp | 0x78 | Same named Map copy body consumes this second nested property's ArrayDim, ElementSize, Offset_Internal and CopyValuesInternal. It is the second TMAP formatter argument; the other nested property independently supplies bucket selection. |
| FSetProperty::ElementProp | 0x70 | Named Set constructor binds vptr 0x7ef31d0. Its relocated copy/destroy bodies consume the sole nested property at +0x70 and dispatch established property operations. TSET formatter consumes the same sole nested property. |
| FArrayProperty::Inner | 0x78 | Fresh named Array constructor/vptr/relocated copy calibration agrees with retained subtype evidence. |

These image measurements alone do not authorize profile promotion. Official target compiler agreement is a separate prerequisite. The generator does not read generated PC offsets or official offsets when producing the image facts.

## Profile behavior

The copied candidate retains all 93 old facts byte-for-byte by record, and retains immutable copies of all four original profile/header files under `original/`. Array Inner is now startup-required. The joined preflight has 66 explicit entries: prior 63 plus Map key/value and Set element, with 50 required, 14 conditional, and two runtime-discovered. New nested discovery may be absent; present pinned conflicts refuse. This changes no initializer source. The parent must select the copied headers and new digest when integrating.

`CHANGES.json` is authoritative for promotion status, exact hashes and digest. While the official receipt is missing, all three new offsets remain absent from the compiled declarations and profile facts; their joined entries are conditional/unpinned. No zero value is promoted as an offset. The generator accepts all three new facts only when a named official target compiler receipt contains agreeing values and the Array calibration.

## Verification

`tests/run_tests.py` passes six ASan/UBSan host preflight scenarios: baseline, missing/mismatched Array Inner, three absent optional fields, two independently missing required fields, and present conditional mismatch. It verifies 66 entries, 50/14/2 classification and unchanged staged collections. The actual digest admission header accepts the matched pair and rejects mixed l22/l23 headers at compile time. Thirteen strict-join controls cover agreement, absent compiler evidence, wrong image/target/commit, missing facts/calibration/provenance, duplicate facts, conflicting widths/offsets and source-only status. Synthetic controls are never artifact-generation inputs. All 93 retained records and original file hashes are checked.

Host tests and static image evidence do not establish native execution, target packaging, or any menu/spawn success. Those remain parent integration/runtime work.

## Interim source status

Official worker reports that the original full-header PS5 compile stops at unavailable `PS5PlatformCompilerPreSetup.h`. A full-header probe with separately audited generic foundation wrappers is still being assessed; no value from that experiment has been promoted. The current candidate is usable as a 93-fact, optional-discovery-refusal profile and does not wait for the separate 711-key inventory. The three image-measured offsets remain unshipped pending qualifying independent compiler evidence.
