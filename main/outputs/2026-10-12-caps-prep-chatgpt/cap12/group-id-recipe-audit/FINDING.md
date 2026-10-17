# Group-ID recipe: exact source contract

Mac-only source audit and synthetic arithmetic replay. No real manifest, group,
UCAS, shader code or target format was read or admitted. Existing readers and
identity-status flags are unchanged.

The pinned public writer feeds each member's stored 20-byte shader hash followed
by that member's **u32 little-endian uncompressed length**, in writer member order,
into SHA1. It then appends the exact UTF-8 shader-format bytes, without a length,
delimiter, NUL, case conversion or Unicode normalization. The chunk constructor
copies the **first 11 digest bytes** and appends the version-specific raw type.
The current v8/raw09 profile uses 09. It is not the common eight-byte-ID plus
zero-index constructor, nor the full 20-byte digest or internal versioned C9.

Source anchors: shader_library.rs13,361–365,1029–1057 establish SHA1, u32 widths,
member ordering and format bytes; lib.rs862–889,903–911,1302–1315 establish
version conversion and the distinct shader-code constructor. Sources and the
MIT license are hash-bound in SOURCE-RECEIPT.json.

For a future metadata crosscheck, the existing manifest's sorted spans can supply
the original writer's sizes only under a separately admitted writer profile:
membership order equals increasing offset order, first offset is zero, all sizes
are positive, and the final span ends at the declared uncompressed group size.
The ordinary reader intentionally allows disordered membership and a leading
prefix, so those valid reader cases cannot silently acquire writer attribution.
Compression does not change the metadata recipe; this does not decode a codec.

An exact shader-format declaration is still required. The manifest does not
contain its format string. A filename guess or matching truncated ID cannot
independently authenticate the format, target applicability, member hashes or
writer profile. Three synthetic vectors record full messages/digests/raw IDs;
five perturbations show why order, size width/endian, case and termination matter.
No production checker or compatibility promotion was added.

Next action: once provenance-complete real metadata and an independently admitted
format/writer profile arrive, check this exact recipe alongside the whole-buffer
gate. A recipe match is metadata consistency only; shader identity, translation,
packaging and rendered output still need their own evidence.
