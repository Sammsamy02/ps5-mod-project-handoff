# Automatic archive/config preparation host example

This one-job C CLI composes the archive intake, profile-scoped config selector, scalar merger and streamed PAK writer in one process. It accepts an unchanged downloaded archive and an immutable original PAK. It discovers the selected member's size and SHA256 from completed intake, binds its owning materialization journal to an opened descriptor, reads the base config directly from the original PAK, merges in memory and writes a fresh unpublished PAK. No pre-extracted/premerged payload, per-mod member facts, AI content rewrite or preparation subprocess is required.

```text
archive_config_job ORIGINAL_PAK ARCHIVE ARCHIVE_SHA256 ARCHIVE_BYTES TARGET_TOKEN BASENAME MAX_MEMBER_BYTES TARGET_ENTRY MOUNT SEED FRESH_OUTPUT QUARANTINE SECTION [SECTION...]
```

Raw archive identity comes from the caller's completed download receipt. Basename, target logical entry, mount, seed and selected sections are profile data. The context token binds related preparation records; it does not establish target-game compatibility. `QUARANTINE` must be a fresh private directory, and `FRESH_OUTPUT` must not exist. All source and output backing must be immutable/disjoint with no concurrent writers.

Compile the three C translation units here with `archive/intake.c`, `archive/private/posix_compat.c`, `archive/candidate_selector.c`, `format/ini_scalar_merge.c`, `format/pak_stream_writer.c`, and the existing PAK reader/capture/secondary-integrity/directory/entry/physical-header modules. Use the already pinned host libarchive, LZMA, zlib and Mbed TLS crypto inputs, with archive/format/pak include directories. The retained `unreal-archive-config-pak-composition-2/trial-1/receipt.json` under `.superpowers/native-dependency-spike` records the exact successful compile and invocation. Its verification runner uses repak only after preparation as an independent oracle.

The real unchanged RAR trial completed preparation in 0.821 seconds on the Mac. Independent extraction retained all 4,699 paths and 4,698 unchanged payload hashes. Its 297,483,249-byte output matched the previously installed console trial artifact byte for byte. That timing excludes download, extraction verification and console transfer; it is not a PS5 performance measurement.

This example admits exactly one bounded scalar-config regular member plus directories. Other regular members are refused before materialization. Limits include a 1 MiB raw archive, 16 inventory rows, 4 KiB logical-name storage, a selected member at most 1 MiB, and the shared unsigned/unencrypted v11 None PAK bounds. It does not interpret DLLs/Lua or translate platform-cooked assets/shaders.

The helper owns process-lifetime allocations and is intended for one job per host process, not repeated calls in a resident app. Detected failures remove only an observed output inode created by this job. Unknown identities or process-loss artifacts remain unpublished; quarantine journals remain available. This private-scratch cleanup is not a hostile-directory concurrency protocol or a complete crash-recovery installer.

These source files preserve the tested host example; the helper has one blank-line whitespace cleanup. They are not part of the app download/job path, default Makefile or PS5 payload build, and the composed job has not executed on the PS5. Prepared archive parity does not prove that its config settings are honored in-game or establish support for another engine, mod class or game.
