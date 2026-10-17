# Portable IoStore asset preparation

These C operations replace host-tool steps in asset overlay preparation. The shared code contains no game or mod names. It prepares container bytes; asset recooking, shader conversion, script execution and game mounting remain separate capabilities.

`none_packer.h` streams ordered raw chunk payloads through a caller-owned 64 KiB buffer. It computes each chunk's first 20 BLAKE3 bytes, writes None-compression UCAS data and emits a v8 Indexed TOC. Row order is preserved. The supplied directory index must already refer to that row order; the packer does not parse it. Payloads concatenate without padding between chunks. Final zeros cover every block's reader-aligned extent, which can differ from rounding the total file length to 16 bytes.

`directory_merge.h` combines base and mod directory indexes and remaps each file's UserData through chunk IDs into the requested final TOC order. Exact path collisions select the mod. Aliases and empty directories survive. It validates UTF-8/UTF-16 FStrings and the directory graph, then emits canonical UTF-8 and deterministic ordering. Mount/root semantics must match. Paths are virtual metadata and are never interpreted as host filesystem destinations.

`toc_overlay.h` builds the final TOC from immutable base/small TOCs, observed UCAS lengths and merged directory bytes. It retains base row order and block descriptors, redirects changed IDs into partition 1 and appends new IDs sorted by their raw 12-byte values. One negative seed and an explicit overflow row table provide lookup. Block coverage and ranges are validated without opening UCAS payloads; actual padding bytes and payload hashes are caller responsibilities.

`none_range_reader.h` plans the selected chunk's complete None-block coverage once, then reads requested ranges through a borrowed partition callback. Each range is at most 65,536 bytes and touches at most two blocks. The immutable plan owns decoded facts and observed lengths, while borrowing the TOC and provider. Each range rebinds its captured geometry and checks the participating blocks before reading. It does not rescan the entire chunk for every request. Provider error or short read clears the requested output range; bytes beyond that range remain unchanged. Plans are trusted caller objects, not authentication tokens.

`container_header_merge.h` merges the v5 package-store header by exact 64-bit package IDs, retaining the base container ID and selecting mod entries on collisions. It sorts IDs and reconstructs descriptor-relative imported-package and shader-hash arrays. Inputs and all caller storage must be valid, aligned and disjoint. Other header versions, unknown nonzero bytes and nonempty optional/localization/redirect/soft-reference tables are unsupported. This does not convert shader code. The format anchor is retained retoc commit `d034ade1ae8117d4786eaf6b0418d4cf48474d7f` and its `container_header.rs` definitions.

## Limits and ownership

The admitted preparation profile is unsigned, unencrypted v8 Indexed containers with 65,536-byte None blocks. Source TOCs for the overlay each use a single UINT64_MAX partition. Unsupported metadata, changed memory-mapped chunks, duplicate IDs and malformed graphs refuse. The packer permits at most 8,192 chunks, 65,536 blocks, 512 MiB content plus up to 15 tail bytes and an 8 MiB TOC. The overlay caps the entire padded sidecar at 512 MiB, so the largest packer output can exceed the overlay limit.

Directory inputs/output are at most 8 MiB each, with 200,000 directory/file/string rows, 2,000,000 ID rows, 4,096-byte strings, 16 KiB individual virtual paths and bounded 32 MiB path storage. Caller storage is capped at 128 MiB plus a separate 32 MiB path arena. The retained real inputs used roughly 10.7 MiB of that storage and 0.45 MiB of the path arena. Insufficient workspace reports the failed allocation's minimum requirement, not a complete sizing estimate.

The range reader admits the same strict v8 None profile with one unbounded or two finite partitions, at most 512 MiB per selected chunk and 65,536 selected blocks. It checks stored extents needed for exact reads, not runtime padding reads. The v5 header merger caps each header/output at 64 MiB and each source/final package table at 262,144 entries.

The format modules above borrow immutable sources and caller buffers. They perform no heap allocation, filesystem/network access or engine calls. The packer invokes input/output callbacks, and the range reader invokes its input callback. A packer callback failure may leave partial unpublished output; its completion flag remains clear. Directory/overlay refusals leave output untouched after admission. A null-output sizing query returns OUTPUT_TOO_SMALL and is never completion. See each header for ABI, alias and workspace contracts. The caller owns source consistency, output disposal, synchronization, publication and rollback.

## Evidence and integration boundary

Focused local controls cover stream failure, duplicate IDs, geometry, aliases, output sizing, Unicode remapping and malformed directory graphs. The retained camera and Leon compositions passed complete byte comparison for their padded sidecars and final TOCs:

| Corpus | Packed chunks | Padded sidecar bytes | Final TOC bytes | Merged directory bytes |
| --- | ---: | ---: | ---: | ---: |
| Camera | 27 | 48,621,708 | 29,837,613 | 2,871,033 |
| Leon | 162 | 187,713,177 | 29,870,724 | 2,873,878 |

Composition 2 derived final IDs in C and computed the directory index from base/small input indexes. The retained final TOC was read only after serialization, as an oracle. No retoc executable or Python packing/merging routine participated in C preparation. The 128 GB base UCAS was not opened. Its length was an explicit input from an earlier capture.

That composition still supplied raw chunk files, a premerged ContainerHeader and the retained small TOC's row order/directory. A separate [original-container example](examples/original_input/README.md) removes those fixture inputs: its C range reader and header merger derive them from the original mod TOC/UCAS and captured base metadata. The retained Leon preparation passed with 161 source asset digests verified. Its source-derived packing order changes physical layout; a separate independent comparison passed all 162 logical chunks (including the merged header), 56,993 file mappings and the changed/unaffected final mappings. The [original-archive example](examples/archive_input/README.md) now joins archive extraction/discovery to a reusable preparation job. App download/install integration remains unfinished. No target build, console write or new in-game result was produced by this work.

The user previously reported camera switching and the Leon replacement working in separate Townfall trials. Those observations do not establish UE4SS support, shader equivalence or compatibility with every game. The INI trial established normal gameplay, while its intended visual effect remains unconfirmed.

Private receipts are retained under `.superpowers/native-dependency-spike/iostore-portable-pack-composition-2/`, `iostore-directory-merge-controls-1/`, `iostore-none-packer-1/` and `iostore-toc-overlay-c-1/`. No captured game or mod assets are included here.

## Reusable original-file preparation job

`mb_iostore_original_file_job_v1` accepts borrowed readonly FDs for the original mod TOC/UCAS, original base TOC and captured base ContainerHeader, an observed base UCAS length, and a borrowed private destination directory FD. It derives chunk order, header identity and merged metadata from those inputs; no prepared raw chunks, expected output, target paths or publication authority are supplied.

The caller guarantees immutable sources and a private/disjoint destination. The job validates regular-file modes and distinct input inodes before creating files, uses positional reads/writes without changing borrowed offsets, and exclusively creates five fixed artifact basenames. Existing files are never overwritten or removed. Every admitted return releases job-owned allocations and closes job-owned artifact FDs; borrowed FDs remain open. Failed partial artifacts remain caller-owned and unpublished, identified by `created_artifact_mask`. `prepared` reports completed file preparation, while `installation_authority` always remains zero.

Optional callbacks provide exact positional I/O and paired allocation/release; defaults use bounded pread/pwrite and malloc/free. Callback backing must remain disjoint from API objects and job buffers. Requested job-managed allocation is capped at 512 MiB; the actual Leon proof measured 279,458,434 bytes peak and released 23/23 allocations. This measurement excludes stack, process and libc allocations.

Completion includes source snapshot checks, artifact sync, sidecar size validation, and full metadata readback. The sidecar digest is computed from source bytes during packing; the job does **not** reread and rehash the complete written sidecar. Independent sidecar/output validation, directory sync, publication and installation remain caller responsibilities.

The focused host control passed 55 assertions across five calls in one process, covering exact actual-Leon artifacts, borrowed FD lifetime/positions, existing-file refusal, a retained 17-byte failed write, input-inode alias refusal and API-object alias refusal. All five successful artifacts matched the previously verified source-order outputs byte-for-byte. The later composed original-ZIP → intake/group selection → materialization → FD-job host proof also produced those exact artifacts. These results prove the admitted ordinary-host file preparation route; they do not establish PS5 execution, transport, installation or additional in-game compatibility.

The retained `preparation_tests/file_job_control.c` accepts original mod TOC/UCAS, original base TOC/header, observed base UCAS length, a fresh trial directory and a post-job reference directory. Link it with `original_file_job.c` and the same six format/BLAKE3 units as the original-container example. Its reference directory is opened only after preparation and never passed into the job.

## Reusable batch preparation API

`mb_iostore_batch_file_job_v1` prepares one private sidecar/index from one to eight original mod TOC/UCAS pairs plus original base TOC/header descriptors and the observed base UCAS length. It derives all chunk/package/path metadata from those inputs. Combined selected rows are limited to 8,192, generated sidecar bytes to 512 MiB, and requested job-managed allocation to 512 MiB. The existing single-mod API and exact single output remain supported.

The default policy requires disjoint incoming mod scopes. Repeated source identities, duplicate nonheader chunk IDs, intersecting package IDs or exact directory-file path collisions return an explicit conflict with contributing source indices and an ID where available. No priority is inferred. Original base overlap remains ordinary replacement behavior. Incoming rows retain source order, foreign headers are omitted, and one merged original-base-ID header is appended.

Sources and the private destination directory FD are borrowed. Optional positional I/O and paired allocator callbacks retain the single-job contract. The caller guarantees immutable inputs and a private/disjoint unpublished directory; the job preserves borrowed positions/lifetimes, creates fixed artifact names exclusively, and releases all owned allocations/FDs on every admitted return. Existing files are never overwritten or removed, and failed partial files remain unpublished. Completion includes metadata readback, sidecar size/sync and source snapshots; whole-sidecar reread/hash, publication and installation remain caller responsibilities. Prepared output has zero installation authority.

Focused host evidence passed 60 batch assertions, including actual two-mod preparation and separate chunk-ID, package-ID and exact-path conflict fixtures that create no artifacts. The existing 55 single-job lifecycle/equality assertions also passed. The actual batch verified 162 incoming asset digests, emitted 163 rows, requested 290,124,492 bytes peak and released 29/29 allocations plus 5/5 owned FDs. These are ordinary-host preparation results, not PS5 runtime or broader compatibility claims.

The batch control in `preparation_tests/batch_file_job_control.c` takes two original mod TOC/UCAS pairs, original base TOC/header, observed base UCAS length and a fresh private trial directory. Its small second pair supplies test-owned copies for the three conflict branches; originals remain unchanged. The [batch archive caller](examples/archive_input/README.md#multiple-selected-downloads) adds automatic intake of each unchanged download.

## Build dependencies

The reader, overlay and directory merger require C11 and libc only. The packer additionally uses the unchanged official BLAKE3 C implementation in `vendor/blake3`, pinned in its `source-lock.json` with upstream licenses. Compile `blake3.c`, `blake3_dispatch.c` and `blake3_portable.c` with:

```text
-DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2
-DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0
```

Add this directory and `vendor/blake3` to the include path. With all SIMD disabled, Clang warns that the vendor CPU probe is unused; use `-Wno-unused-function` for the vendor dispatch translation unit. Keep vendor source unchanged.

The standalone `preparation_tests/packer_control.c`, `overlay_control.c`, `directory_control.c` and `range_control.c` retain the already-run focused host controls. Link the first with the packer/BLAKE3, the second with the overlay/reader, the third with the directory merger and the fourth with the range/TOC readers. Run the directory control with `--synthetic` for its asset-free Unicode/graph checks. Range controls cover cross-block/two-partition reads, failure clearing and plan binding; an actual incoming Leon chunk matched all 172,032 retained bytes. These are local format tests, not console deployment commands.
