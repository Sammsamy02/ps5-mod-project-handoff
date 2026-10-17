# Bounded Stellar Blade container audit

Read-only samples from the discovered content-directory candidate at `/data/Stellar Blade 01.014.001 PPSA13197/sb/content/paks/` show standard PAK v11 and IoStore v3 metadata. This gives concrete reusable container-reader requirements; it does not establish a texture writer, current runtime identity or mod acceptance.

The independent audit at 2026-09-30T14:37:43Z kept four samples totaling 1,875 bytes, each within a 3-second deadline. Ordinary FTP TYPE I/PASV/RETR and REST were used; sockets closed promptly at the sampling limit. No service command, console write, key/decryption operation, executable download or whole large archive download occurred. Binary samples were discarded after parsing.

| File | Sample offset | Sample bytes | SHA-256 of sampled range |
| --- | ---: | ---: | --- |
| global.utoc | 0 | 512 | 4707aa485931a4ca8d06e5b3eb7b3d4a6916df8e15b81ee681f530057f02f55d |
| pakchunk0-ps5.utoc | 0 | 512 | 6bb56e4481ad0f1345d3abc7d0acd49acb030ba5053f41ae0bdedf108859961e |
| pakchunk1-ps5.pak | 0 | 339 | 895d5c70c69bfa5c4317176b729578f9600d023153488383e974cf2fa46a592c |
| pakchunk0-ps5.pak | 1,273,877,883 | 512 | 59d4a3e7b3f75ca7a31a5297287d5ac5234cc0938f9c4766dbc95013417adac2 |

Prefix/tail digests identify only those ranges. The small 339-byte digest covers the full file size reported by the earlier LIST; large file bodies were not hashed.

Both UTOC headers have the standard 16-byte IoStore magic, raw version 3 (PartitionSize schema), 144-byte header, 12-byte compressed-block records, compression block size 65,536, zero compression names and zero encryption-key GUIDs. They declare one partition, partitionSize UINT64_MAX and no perfect-hash counts. Global declares three entries, 24 blocks, flags 0, no directory index and containerId UINT64_MAX. Chunk0 declares 164,325 entries, 734,118 blocks, flags 8 (Indexed only), directory index size 5,029,412 and containerId 10437857082156768155.

Header count arithmetic using the referenced 12-byte chunk IDs, 10-byte offset/length records, 12-byte block records and 33-byte legacy entry metadata matches the previously listed TOC sizes (597 and 22,876,847 bytes). This is size consistency, not chunk/index validation. The chunk0 compression table begins at 12,424,710, beyond the sampled prefix; zero declared compression names does not independently validate every entry's codec or payload.

Both PAK tails have magic 0x5a6f12e1, raw version 11 (Fnv64BugFix schema), a 221-byte footer, encryptedIndex=false, zero GUIDs and five blank 32-byte compression-name slots. The small file's footer begins at 118, index offset is 0 and index size 106, with declared SHA-1 aa6a986461366acd6ac94088833eaea3258bbd9a. The large footer begins at 1,273,878,174, index offset is 1,273,577,209 and index size 54,642, with declared SHA-1 8c3b85b64f6c9615ddac2d041db3bd1d186d4525. Declared index ranges fit the earlier file sizes; this initial four-sample checkpoint did not verify index hashes. The later primary-index sample below verifies the large PAK index.

Primary format references are pinned CUE4Parse commit 28f385e622468c4ab84997d596b46bb1c3adfcfa: [IoStore header](https://github.com/FabianFG/CUE4Parse/blob/28f385e622468c4ab84997d596b46bb1c3adfcfa/CUE4Parse/UE4/IO/Objects/FIoStoreTocHeader.cs), [TOC resource](https://github.com/FabianFG/CUE4Parse/blob/28f385e622468c4ab84997d596b46bb1c3adfcfa/CUE4Parse/UE4/IO/Objects/FIoStoreTocResource.cs), [entry metadata](https://github.com/FabianFG/CUE4Parse/blob/28f385e622468c4ab84997d596b46bb1c3adfcfa/CUE4Parse/UE4/IO/Objects/FIoStoreTocEntryMeta.cs), and [PAK footer/index schema](https://github.com/FabianFG/CUE4Parse/blob/28f385e622468c4ab84997d596b46bb1c3adfcfa/CUE4Parse/UE4/Pak/Objects/FPakInfo.cs).

Next reusable requirements: bounded PAK v11 path-hash index and IoStore v3 table/directory/chunk readers; verified backing payload/partition mapping; selected-entry integrity and codec semantics; platform texture/container writers; exact game/build acceptance. No .ucas was visible in the limited physical PAK directory listing. The later metadata samples below confirm an AMPR logical namespace with paired .ucas declarations; backing file/chunk/volume consistency remains unverified. These container schema numbers do not identify an Unreal engine/build version and supply no install permission.


## Verified large PAK primary index

A later normal FTP REST/RETR read at 2026-09-30T14:47:24Z obtained exactly 54,642 bytes at offset 1,273,577,209 (0.060 seconds). SHA-1 `8c3b85b64f6c9615ddac2d041db3bd1d186d4525` matches the sampled footer. Range SHA-256 is `184670522954ea8d7772dbeb2d78c3204e7f6bccbf40dce2314ed4bca8fed4d6`. A standard v10+ primary-index parse consumes exactly that range: mount `../../../`, file count 4,544, path-hash seed 4084654653, encoded entry bytes 54,528, nonencoded entries 0.

Secondary path-hash index declares offset 1,273,631,851, size 131,453, SHA-1 `394496661e179f2a774690380f5c6a03817392e9`. Full-directory index declares offset 1,273,763,304, size 114,870, SHA-1 `eb03bdc92474d46bbe821acd9c8e550fced1101a`. Ranges fit the listed file size; neither secondary was fetched or hash-verified. File inventory/entry semantics remain unresolved. The 54,642-byte index buffer was discarded. Primary schema: [pinned PAK reader](https://github.com/FabianFG/CUE4Parse/blob/28f385e622468c4ab84997d596b46bb1c3adfcfa/CUE4Parse/UE4/Pak/PakFileReader.cs#L296).

## Confirmed AMPR metadata and logical UCAS declarations

Three further ordinary FTP reads at 2026-09-30T14:55:11Z totaled 48,416 bytes, each 0.035–0.046 seconds under three-second deadlines. No asset body, volume data, reconstruction, key or executable was read. Raw buffers were discarded. These exact files are relative to `/data/Stellar Blade 01.014.001 PPSA13197`:

| Metadata | Sample | Range SHA-256 | Observed schema |
| --- | --- | --- | --- |
| ampr_emu.index | Complete 48,224 bytes, EOF confirmed | 83033304ae08ea0fd21a83dc59a0d6529760be014a1a13abc9b0eb9186a8b2be | AMPRIDX3, v3 |
| ampr_assets.index | First 128 bytes | 0a91c2cad258635ce2f2d03f296f1c3f2bfdaddf7bbc35fb5c9a67317e032bb9 | AMPRPAK4, v4 |
| ampr_assets.index.runtime | Complete 64 bytes, EOF confirmed | e66bbbcf33140ccd6a67c7014538a273564042fb7b35d8a855ab79ed233e1fe0 | AMPRCFG1, v1 |

The AMPRIDX3 header is 48 bytes, with 418 records of 24 bytes, path blob 21,748 bytes at offset 10,080, and 1,024 hash slots of 16 bytes at offset 31,840. Computed end is 48,224. All 418 sampled record path bounds, NUL/UTF-8/app0 constraints and hash-slot field bounds pass; 418 slots are occupied. Hash probing/lookup semantics are not verified. This index has no build-ID field.

AMPRPAK4 header fields match the public v4 schema: flags 0, endian tag 0x01020304, reserved 0, build ID `2a53135566bff1dfee4d2c7b89aef727`, 418 files, 1,393,594 chunks and four packs. Record sizes are 48/12/32; file/chunk/pack/string offsets are 128/20,192/16,743,320/16,743,448, string size 21,900. Computed total 16,765,348 matches prior LIST. Declared/calculated header CRC32 equals **1016957345**. Declared payload CRC32 **339335635** is unverified because payload metadata was unread. File/chunk/pack records and data-volume headers remain unverified.

The AMPRCFG1 runtime metadata has the same build ID, size 64 and reserved 0. Declared/calculated CRC32 equals **1328516114** and public field constraints pass. It declares decoded cache 268,435,456 bytes, physical cache 134,217,728 bytes, four workers and one reserve. These are observed metadata, not product settings or evidence of runtime behavior.

The complete AMPRIDX3 index declares exactly five paired logical UCAS records, all under `/app0/sb/content/paks/`:

| File ID | Logical basename | Declared bytes |
| ---: | --- | ---: |
| 351 | global.ucas | 1,517,056 |
| 354 | pakchunk0-ps5.ucas | 41,604,992,704 |
| 357 | pakchunk0optional-ps5.ucas | 33,554,480 |
| 360 | pakchunk1-ps5.ucas | 49,153,416,848 |
| 363 | pakchunk1optional-ps5.ucas | 536,870,960 |

These prove index declarations, not that matching AMPRPAK4 records identify packed bytes, every volume matches, or the game uses/accepts them. The [pinned IoStore reader](https://github.com/FabianFG/CUE4Parse/blob/28f385e622468c4ab84997d596b46bb1c3adfcfa/CUE4Parse/UE4/IO/IoStoreReader.cs#L65) requires same-stem UCAS streams or partitions. A checked logical provider is now a concrete candidate rather than an inference from shard names.

AMPR primary references at `drakmor/ampr_emu@cfa85df379f6eeeb165d7badf9b648e266fe77b7`: [format structs](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/include/ampr_emu_pack_format.h), [AMPRIDX3 parser](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/tools/ampr_pack_format.py#L253), [manifest validation](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/tools/ampr_pack_format.py#L642), [runtime validation](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/tools/ampr_pack_format.py#L1157), and [portable logical-file core](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/include/ampr_emu_pack_core.h). Source reuse would require retaining its GPL-3.0-or-later provenance; no upstream source was executed.

The next reusable seam needs immutable source identity/64-bit size, checked bounded `read_at`, unavailable/short-read/invalid distinctions, safe logical paths and paged metadata with aggregate work caps. AMPR manifest CRC/layout and matching file records, build-consistent AMPRDAT3 volume headers, chunk bounds and RAW/LZ4 decoding precede a UCAS provider. PAK secondary hashes/encoded entries and IoStore TOC/block/directory/partition integrity remain separate reader work. The current native DDS span ABI's 8 MiB cap cannot eagerly consume 16,765,348-byte AMPR or 22,876,847-byte UTOC metadata. Owned fixture validation and lossless no-op checks precede writers. Exact PS5 writer/game-build acceptance remain fail-closed requirements; this audit enables no install.
