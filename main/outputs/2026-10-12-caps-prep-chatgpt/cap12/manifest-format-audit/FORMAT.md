# Type8 manifest format audit (Mac-only)

A concrete public decoder exists: trumank/retoc at revision d034ade1ae8117d4786eaf6b0418d4cf48474d7f, MIT. The previously retained subsets did not include shader_library.rs. The exact public files are now retained in public-retoc-source/ with LICENSE; public-source-receipt.json records original URLs, bytes and SHA256s. No Epic source was copied.

The [pinned reader](https://github.com/trumank/retoc/blob/d034ade1ae8117d4786eaf6b0418d4cf48474d7f/retoc/src/shader_library.rs#L235-L243) consumes type8 chunk bytes directly: little-endian u32 version1, followed by seven vectors in this order:

| Vector | Element bytes |
|---|---:|
| shader-map hashes |20 |
| shader hashes |20 |
| shader-group raw chunk IDs |12 |
| map entries: index-array offset, shader count |8 |
| packed shader entries |8 |
| group entries: index-array offset, shader count, uncompressed size, compressed size |16 |
| shared shader indices |4 |

Each vector starts with a little-endian u32 count. ser.rs:111–114,201–223 establishes count/scalar endianness; lib.rs:765–777,1105–1108 establishes raw-ID/hash widths. shader_library.rs:20–143 defines the records and ordering. The packed shader entry assigns4bits to frequency,30 to group index and30 to uncompressed offset within that group (:53–70).

There is no additional header or manifest decompression operation in this reader path. IoStore transport decompression is upstream; current local intake accepts stored blocks only. Type9 group compression is separate: the manifest supplies sizes, but no compression-method identity. The pinned reader attempts codec discovery when sizes differ (:249–265); a future metadata-only parser should report that uncertainty and avoid invoking it.

The map hash selects a map entry. Its index-array offset plus a map-local slot selects a library shader index, which selects a full20-byte shader hash and packed code/group location (:466–475). Thus actual manifest indices can replace legacy first-appearance numbering. The legacy object+212-to-map-slot relationship still needs comparison with real manifest data. The reader does not establish the object+216 field or its relationship to hash prefixes.

Type9 rows identify groups, potentially containing several shaders. The public writer derives group identity from member hashes/sizes and format, then truncates that group identity into a chunk ID (:1029–1057; lib.rs:885–889). A type9 chunk ID must not be equated with one shader's full hash. Manifest parsing alone does not validate group bytes, decompression, shader payload hashes or instruction boundaries.

Recommended future admission checks, independent of type9: validate the current intake receipt and exact payload IoHash/SHA; support only version1; cap counts before multiplication/allocation and require sufficient remaining bytes; compare map-hash/map-entry, shader-hash/shader-entry and group-ID/group-entry counts; validate every map/group index-array range and shader index; validate each packed group index and offset against its declared group size; verify group membership agrees with each referenced shader entry; reject ambiguous duplicate identities. Shared index-array ranges can be reused by maps/groups, so blanket range-disjointness would be unjustified. Preserve or refuse unsupported trailing bytes rather than silently consuming them. These are proposed defensive checks, not freshly validated target-format facts.

No manifest payload was inspected or decoded this turn. The exact missing input remains a provenanced type8 payload admitted by the existing local_shader_intake.py. A bounded metadata-only version1 parser now has enough source-backed structure to be implemented and fixture-tested; its applicability to the captured target must remain conditional until a real payload validates. The next semantic check is resource hash → manifest map → explicit map-local slot → full shader hash/group location, with object+216 checked only afterward. No compatibility or native shader result follows from that metadata chain.

Integration note: the source-specified bounded reader is now implemented in ../manifest-v1/ and passes15 synthetic tests plus CLI replay. Actual payload/target applicability and the frozen-field comparison remain pending; this note does not upgrade the format audit into target proof.
