# Native PAK preparation

The shared C API `mb_pak_write_v11_none` now prepares deterministic classic-v11, unencrypted, uncompressed PAK bytes from virtual paths and payloads. Mount and path-hash seed are profile inputs; no game or mod identifiers are compiled into the writer. It uses caller-owned workspace and output, with no filesystem, network, engine, heap or retained-pointer interface.

The first implementation accepts ASCII paths, up to 256 files, 4 MiB per payload and 16 MiB per archive. It rejects unsafe paths, duplicate or conflicting names, input/output aliases and exhausted size/work limits. Rejected preparation publishes no archive. These are current limits, not a statement of broad mod support.

Focused host verification passed 270 behavior checks. An independent parser checked three generated archives, pinned repak recovered eight exact payloads, and the existing canonical reader passed 50 metadata/payload/corruption checks. A separate profile-driven run packed three saved original game assets; repak and the canonical reader recovered all 68,829 payload bytes exactly. The same writer and SHA1 dependency compile and link for PS5; that target binary has not executed.

This removes the external PAK packing tool from the preparation primitive. It does not yet connect the primitive to the app's download/install pipeline, translate cooked PC assets, write IoStore bundles, port UE4SS, or prove engine acceptance. The larger Leon bundle remains outside this writer's limits. The [Townfall trial](2026-10-01-townfall-mod-trial.md) records the current in-game evidence.

The API contract and focused reproduction commands are in [pak_writer_tests/README.md](ps5-mod-bridge/native-host/format/pak_writer_tests/README.md). Raw game assets and development captures remain private and are not committed test fixtures.
