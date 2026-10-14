# Resident downloaded IoStore preparation

The existing serialized native jobs loop can prepare one unchanged downloaded
archive through shared scan, grouping, content selection, materialization and
`mb_iostore_original_file_job_gated_v1`. It uses the read-only downloaded FD and
its observed SHA/size. There is no CLI, subprocess, per-mod rewrite or expected
output input in this route. Logical archive paths are metadata.

The optional `--iostore-profile` JSON contains exactly:

* `base_toc_path`, `base_toc_bytes`, `base_toc_sha256`
* `base_header_path`, `base_header_bytes`, `base_header_sha256`
* `observed_base_ucas_bytes`, `nexus_game`, `timeout_ms`

The startup owner keeps both read-only capture FDs alive until successful jobs
cleanup. Admission independently verifies SHA/size and unchanged snapshots.
The target token derives from both capture hashes and observed partition length;
it binds these local observations, granting no authenticity/platform authority.
The composer separately verifies the captured ContainerHeader against its TOC
chunk length/BLAKE3 digest. The large base UCAS is never opened. Canonical Mac
capture paths in development profiles are not console deployment/discovery.

`download_prepare_iostore` uses the same five flat protected job fields as other
handlers. `download_prepare` gives an IoStore filename hint priority over a
companion PAK, then the core independently rescans the complete original archive.
Exactly one complete IoStore group is admitted. An empty v11 PAK companion and
bounded valid UTF8 plaintext readme documents are explicit verified roles;
material PAK companions, unknown regular files, missing partitions and ambiguous
groups refuse. Download/analysis remain bounded separately from the 512MiB
preparation input/member limit and 1GiB expanded limit.

The shared composer admits unsigned, unencrypted v8 Indexed TOCs with 64KiB
None blocks, one finite sidecar and the constrained v5 ContainerHeader subset
documented in `portable_preparation.md`. Incoming assets retain their source
bytes and hashes. Header/directory metadata and offsets are composed against
the captured base. This performs no asset recooking, shader conversion, input
remapping, Lua/UE4SS bridge or native DLL execution.

Each owner has job-ID-derived private0700 quarantine/output directories. Member
identities derive from the materializer's retained inode journal. The new core
control reports exclusive artifact creation before writes, confirmed close, and
uncertain close. The resident owner retains those five inode identities. All
artifact writes must be contiguous physical order; a SHA256 stream records every
successfully written byte. After core cleanup and member-FD closure, the owner
reopens each artifact read-only, checks its inode and snapshot, hashes every byte
and compares that SHA to the write stream. This includes the complete sidecar,
then a final monotonic gate precedes completion. Public reports contain five
index/size/SHA objects, observed incoming chunk counts and verified companion
counts, with no filesystem path, signed URI or credential. Runtime and install
authority remain false; these output identities do not prove console loading.

Gate checks bracket I/O, allocation and bounded CPU phases. Blocking calls and
child parsers remain cooperative. Cleanup always runs and removes only journaled
identities, without retrying uncertain descriptor closure or deleting substituted
leaves. Unknown closure/ownership retains the context and refuses further jobs.
There is no persistent publication or process-loss recovery integration yet.

An explicit local generation composes1..8 selected retained downloads into one
result. POST `/api/generations` takes exactly `kind` with value
`prepare_iostore_generation`, an ordered `source_job_ids` array of unique
lowercase32-hex IDs, and `idempotency_key`. The existing local cookie, CSRF and
Origin checks apply; this operation performs no new account network request.
Only successful analyzed source jobs owned by that cookie and matching the
configured source domain are eligible. The generation binds their IDs, sizes,
SHA256 values and immutable base token. Reusing a key with changed selection or
order conflicts. Source job keys and generation keys are separate namespaces.
GET `/api/generations` returns owner-scoped `items`; GET
`/api/generations/<id>` returns its queued or terminal record. There are at most
eight resident generations, with no eviction or persistence.

The serialized loop runs a queued generation after source download jobs. Each
original leaf is reopened no-follow read-only and independently rehashed before
admission. One deadline covers those verification passes and subsequent owner
work. Each source retains its own private quarantine inode journal and verified
member descriptors; the existing strict batch composer runs exactly once into
one output directory. No prepared single-mod overlay is used as a batch input.
The batch owner's shared16GiB I/O,1million operation and128MiB path-work budgets
cover scan, materialization, member reads, core I/O and output readback. Its
per-source512MiB raw/member and1GiB expansion bounds, and the composer's512MiB
aggregate payload/allocation bounds, remain separate. Deadline and parser
limits are cooperative; blocking calls are not hard interrupts.

Repeated source inode identities and overlapping incoming chunk IDs, package
IDs or exact virtual paths produce a stored `conflict`, including the two
selected source indices and conflict kind. Source order never implies priority.
Conflicting/refused owners close descriptors and remove their journaled scratch
outputs while preserving retained original downloads. Successful generations
retain exactly five private artifacts with complete physical readback identities
until successful jobs cleanup. They do not acquire prerequisites, publish to a
game, or establish runtime compatibility. Completed queue records are never
implicitly selected; separately prepared overlays are never treated as one
combined installation.

The build uses pinned portable BLAKE3 with all SIMD backends disabled, existing
archive/LZMA/zlib intake, selected transport SHA256 and the isolated same-pin PAK
SHA1 probe provider. SDK ELF composition is a compile/link check; executing the
payload, finding installed captures, placing artifacts and demonstrating a mod
in a running game are separate remaining work.
