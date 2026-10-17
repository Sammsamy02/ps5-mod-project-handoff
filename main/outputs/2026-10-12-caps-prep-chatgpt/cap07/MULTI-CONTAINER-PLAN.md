# Offline multi-container composition

The prior goal turn made progress: a real compact secondary projection, source-lookup/identity fixes, dependency evidence, new utilities and 79 fresh tests. It did not prove runtime mounting.

Next concrete implementation: compose compact per-owner projections through the existing builder, returning the entire generation only after every input/output passes. Original mod payloads remain unchanged. Input bases supply captured TOC, exact ContainerHeader bytes and a declared UCAS size; this cannot authenticate absent physical UCAS bytes or prove a complete catalogue.

1. Validate base lookup/geometry/header identity and IoHash; derive ownership from matching export/store entries. Refuse missing stores, ambiguity, unresolved imports, conflicts or unplaced unmatched packages. Explicit unmatched placements are caller choices, never proof a package is new.
2. Project each source mod per owner into confined temporary files, use the existing builder, and return all container results together. Default conflict policy rejects conflicts. Bind original and generated hashes; clean temporary projections on success/failure.
3. Check a two-container/two-mod fixture, exact export/bulk payloads, cross-owner imports, an explicitly placed unmatched package and refusal cases. Real Leon remains refused until its missing store/ownership/material evidence arrives. No runtime or disk publication code, build or console contact.
