# Automatic selected-source IoStore generations

The resident download coordinator now accepts an explicit selection of retained
original archives and composes one private IoStore result. It does not require
an individual-mod conversion recipe. A selection is separate from single-file
preparation: the existing batch core handles its container metadata, source
asset hashes and strict incoming overlaps together.

## Behavior

POST `/api/generations` accepts exactly `prepare_iostore_generation`, an ordered
array of1..8 unique `source_job_ids`, and an idempotency key. Existing local
cookie, CSRF and Origin admission apply. Each selected source must be a successful
analyzed download owned by that cookie and match the configured source domain.
The generation binds original IDs, sizes and SHA256 values plus the captured
base token. Changed selection/order with a reused key returns409; repeated
unchanged requests return the same record. GET collection/detail remain local,
owner-scoped and read-only. There is no new account network request during
generation preparation.

The serial jobs loop reopens and rehashes the original source leaves read-only,
then gives them to one resident batch owner. Each archive uses the shared
selection/materialization path and its own private inode journal. All verified
TOC/UCAS member descriptors feed exactly one batch-core call into one output
directory. Completed download records are not implicitly selected. Exact
source-inode, chunk-ID, package-ID or directory-path overlap returns a stored
conflict; order grants no overwrite priority.

One deadline and cumulative owner budgets cover intake, member reads, core I/O
and complete physical output readback. Every successful generation retains five
inode-bound artifact sizes/SHA256 values. Conflicts/refusals close descriptors
and remove only journaled scratch artifacts; original downloads remain private
until jobs cleanup. Unknown closure/ownership latches storage refusal.

## Evidence

The direct real-reader/core owner controls passed7 cases and120 assertions:
the unchanged Leon ZIP and movement archive produce all five prior batch-oracle
artifacts exactly, verifying162 incoming asset hashes and emitting163 chunks.
Legacy single-source Leon also matches all five expected artifacts. Repeated
original inode, distinct byte-identical source, invalid count, known input alias
and actual1ms deadline cases withhold completion and clean all private contents.

The HTTP framing controls pass17 checks, and dispatch controls pass171 checks
against explicit test-owned coordinator/auth boundaries. Those component tests
do not substitute for execution of the real coordinator.

The real owned-TLS coordinator integration passes29 checks. It downloads and
analyzes unchanged archives, accepts only the explicit eligible source set and
produces the five Leon+movement batch-oracle identities. A second distinct
download of unchanged Leon reports a chunk-ID conflict with no new scratch
output. Repeat/order-conflict, duplicate ID, mixed-owner/domain, failed-source,
read-only owner scoping and shutdown cleanup checks pass. Six archive transfers
carry no account headers. Original input device/inode/size/SHA identities remain
unchanged. The historical5861be3 binary returns404 for the new generation route.

The current ordinary-host and SDK artifacts compile/link with zero diagnostics;
135 source records and53 native objects match their manifests. Native include
inspection records16518 current entries with zero host headers. The only new
import is `qsort`, declared and exported by the pinned SDK and called by the
existing batch core. The aligned336-byte dynamic table ends with DT_NULL. No
payload was executed and no game/system file was written.

The focused current-host single-route regression passes18 checks, including
unchanged camera/Leon archives, automatic routing, all five Leon artifact
identities, unsupported companions, wrong-game refusal and clean shutdown.
The paused254-file baseline matches all saved hashes.

Host artifact:2168168 bytes,
`c5bf559ee131b1b9dd93b100536487794df2631e72a90acc47b350d91a3cb46c`.
SDK artifact:2650880 bytes,
`bfe0e2e5e58831c69f8bd60ce09ea1f4f44f61bb8f641867f889ae5e90064ced`.

Receipts beneath `.superpowers/native-dependency-spike/`:

* `native-iostore-generations-1/green-3/receipt.json`
* `native-iostore-generations-1/red-4/receipt.json`
* `native-iostore-generations-1/single-route-green-1/receipt.json`
* `native-iostore-generations-1/native-artifact-audit-1/receipt.json`
* `native-iostore-generations-1/root-coordinator-compile-2/receipt.json`
* `native-iostore-generations-1/http-controls-1/{green,dispatch-green-1}/receipt.json`
* `native-iostore-batch-owner-1/green-2/receipt.json`

## Limits and next work

This admits the existing unsigned, unencrypted None/v8 IoStore and constrained
ContainerHeader scope. It does not recook assets, remap input, translate DLLs,
provide Unreal reflection/game-thread calls, acquire prerequisites, discover
console target captures, publish an installation or survive process loss.
Prepared generations report runtime/installation support as false. Windows
UE4SS/DLL gameplay, other engines, prerequisite selection and the final PS5
account/store app remain unfinished; the full goal remains active.

All tests use owned local fixtures and saved captures. No console or production
account was contacted. The paused library/UI work and original user files remain
outside this phase. Specific previously stopped native placement, mapbase
memory-read and curl-upgrade actions were not retried or rerouted.

Implementation:
`outputs/ps5-mod-bridge/native-host/{jobs.cpp,jobs.h,http.c,http.h}` and
`iostore/private_iostore_job.{cpp,hpp}`.
Reproduction:
`outputs/ps5-mod-bridge/native-host/tests/nexus_iostore_generations_verify.py`.
Detailed contracts: `iostore/PRIVATE_PREPARATION.md`.
