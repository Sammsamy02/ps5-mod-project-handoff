# Native JMap source port

The actual upstream generator now compiles with the PS5 SDK. Native boundaries use the real owned-region provider for fresh readable/executable metadata, `gmtime_r` for UTC, the existing program executable path, and the retained module snapshot for its image label. The original Windows bodies remain. Public headers and class layouts are unchanged; only the private cpp dumper retains the snapshot.

Twenty helper assertions pass with ASan/UBSan on real registered buffers and anonymous mappings, covering adjacent ranges, gaps, permissions and changes, overflow, timestamp errors and Unicode filenames. These tests do not execute engine traversal or produce a game JMap. The full SDK composition link is recorded separately.

Permission queries do not provide a lease. Registrations and exact storage must remain owned and stable across direct traversal; no arbitrary game mappings are registered. Unreal initialization and a retained snapshot are required before entry.

Focused review identified inherited direct object/vtable/class dereferences at source lines 2190 and 2226 before the owned-readable helper. This port is compile-only: full JMap traversal requires guards and retained stable registrations for every followed reflection edge. The initialization/module gate alone does not establish those guarantees.
