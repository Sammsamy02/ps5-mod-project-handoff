# Resource/source audit (Mac-only)

The legacy work is at outputs/2026-10-08-l25-prep/round6-prep-20261012/cap12-resourceindex/. It materially advances the earlier cap12 candidate census. Its object-relative signed32-bit field at212 has strong map-local slot-index evidence. The earlier prefix scan covered only80bytes, so it could not evaluate this field. Further scans for arbitrary small integers would repeat weaker work.

This audit verified all11 files listed in the legacy directory's own MANIFEST.sha256. Independently recomputing from its saved shader-objects-summary.json and the original small .stinfo metadata reproduced:

| Check | Result |
|---|---:|
| Shader-object rows | 14,645 |
| Sections with dense zero-based slot indices | 492 /492 |
| Map-local slots | 9,414 |
| Conflicting key/target values within one slot | 0 |
| Unique32-bit sort keys and sidecar entries | 9,388 each |
| Positional type-set matches | 9,388 /9,388 |
| Exact within-entry type-order matches | 9,376 /9,388 |
| Type references in each representation | 9,496 |

This recomputation validates saved metadata consistency and sidecar correspondence, not the original object-byte decoder anew. The original4.55MB shader cache was not read, and the multi-minute codec/negative-control jobs were not rerun. The receipt records this distinction and binds every inspected input hash. The legacy byte-identical roundtrip and negative controls remain prior receipts, not fresh results.

The relevant legacy implementation references are gsc_codec_ri.py:7–10 (empirical provenance/descriptive naming), :12–50 (layout assumptions and remaining raw subclass bytes), :101–108 (scalar offsets), :503–521 (slot/key order observations), :576–592 (same-slot and dense-index consistency). ri_crosscheck.py:68–86 assigns predicted library numbers from first occurrence of32-bit keys, :98–110 checks sidecar type sets, and :149–163 explicitly marks future manifest/hash checks. Field-local mutation and a byte-identical rebuild establish byte structure; they cannot independently establish the engine meaning of a field. Zero objects_raw_hex does not mean all object semantics are decoded: subclass fields remain opaque hex.

The licensed local retained definitions inspected were the three Core serialization headers listed with SHA256 and exact line ranges in audit-receipt.json. MemoryLayout.h:92–145 describes field/type descriptors; :316–323 exposes target-layout-dependent field inclusion/alignment/hash operations; :798–845 distinguishes initialization, pointer width, base alignment, editor fields and maximum alignment. MemoryImageWriter.h:21–43 and :80–114 makes frozen writing/unfreezing depend on target type layout. MemoryImage.h:201–213 carries target layout parameters and pointer/name patch tables through its archive interface. These constrain how a justified shader layout would need to be established; they do not contain shader declarations or a ResourceIndex accessor. Conditional on these headers matching the producer, layout flag1 denotes initialization without the other displayed flags; that fact alone cannot locate any shader field.

The inspected official-source roots have no Shader.h, Shader.cpp, ShaderMapResource or shader-library/archive implementation. The legacy codec explicitly says it used no Unreal source. No known full source root was supplied; home was not scanned. Therefore no source-known FShader field layout, assignment accessor or serialization chain to a library/code identity is established by this audit. No engine code or snippets were copied into outputs.

A useful scope correction is: object+212 is an empirically constrained per-map index, rather than an unexamined candidate. It is not demonstrated to be a global code-entry index bounded by9,388. The32-bit sort key has9,388 unique values here, but equality with a prefix of a full shader-code hash remains an explicit legacy hypothesis. A sidecar type-membership/order match supplies no full code hash, code offset or shader instruction boundary. The legacy first-occurrence numbering rule remains a prediction until a real library manifest confirms it.

The next prerequisite that can advance integration is actual source-bound type8 manifest bytes, admitted through the existing cap12 intake tool. Use the legacy per-map resource hashes, slot counts and predicted associations as check-only expectations. A real manifest must independently establish its resource/map identity, slot-to-entry mapping and full code identity before associating anything with an admitted type9 chunk. Do not require exact first-occurrence numbering merely because the legacy predictor used it: reject that predictor if the manifest disproves it, and use the actual supported manifest mapping. A manifest count or32-bit key match alone is insufficient. Source-known shader declarations/assignment and matching target-layout evidence are still required before shipping object+212 as a generic field offset. No code payload, shader decode, translation or compatibility has been proven.

## Root raw-byte follow-up

After this saved-summary/source audit, verify_raw_fields.py read the hash-bound original cache metadata and used only the retained M1a section/reference reader. Direct reads at object+208/+212/+216 match all14,645 saved reference rows over14,583 distinct object locations. All492 maps have dense zero-based indices, totaling9,414 slots with no same-slot target/key disagreement. Three original/source hashes remain unchanged. raw-field-replay.json binds the exact verifier and inputs. This freshly validates scalar byte correspondence; the full object codec/roundtrip/mutation controls were not rerun, and semantic/ABI/library/code claims remain unproved.
