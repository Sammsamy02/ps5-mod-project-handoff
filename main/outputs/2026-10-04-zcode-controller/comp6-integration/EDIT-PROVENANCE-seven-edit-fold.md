# EDIT PROVENANCE — the 2026-10-05 seven-edit fold

Controller-authored deliberate edits between the l11 GO run (candidates
`prx-candidate-l11-{1,2}`, composed object `39c83a3c…`) and the next one-pass.
Every edit is recorded in `launch11-combined/EDITIONS.json`
(`seven_edit_fold_2026_10_05.edits`, pins refreshed accordingly). This file is
the semantic provenance the byte-diffs cannot fully provide for files that are
not git-tracked; the LuaMod diff IS archived byte-exact
(`worker-luamod-lineage/diffs/`), and the SeedValidation v4→v5 diff is archived
byte-exact against the former pinned copy.

## Edit 1+2 — SeedValidation.hpp v5 (`b3d81b0f` → `db07ac60`, two identical copies)

`launch10-build/final-units/SeedValidation.hpp` and
`worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp` (kept
byte-identical), mirrored also to `launch11-combined/units/SeedValidation.hpp`.

1. `publish_readable_spans` policy: records overlapping the main-image extent
   are excluded from publication candidacy (envelope computation unchanged —
   still over ALL decoded records). Rationale: the launch-9 image-BSS-tail
   placement hypothesis for Blocks[0] is disproven by the first-ever captured
   Blocks[0] (pid 700: `0x22C0000000` = 149,250,113,536 B ≈ 139 GiB, far above
   the complete-listing envelope `0x1100400000` = 73,018,638,336 B ≈ 68 GiB);
   image readability remains proven by anchor geometry; image spans only
   crowded the saturated window (live: 126 candidates, 64 published).
2. `PoolShapeCheck::failure_class` enum added (machine-checkable class; the
   initializer branches on it, never on string compare).
3. `resolve_listing_gap` pure function added — the evidence contract: a
   mapped-ness verdict is necessary, NEVER sufficient (on-target proof:
   mincore is protection-blind, prospero-5c2d3055 case c); no supported
   readable-coverage source exists for listing-absent addresses today, so a
   (CaptureTruncated|NotCovered, Mapped, None) resolution REFUSES with the
   mapped-but-unreadable note. Direct-memory omission stays a hypothesis.

## Edit 3+7 — bridge (`faec2336` → `ec53442b` → `59db9dcd`)

`launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp`:

- 3 (ec53442b): `bridge_kernel_range_verdict(addr,len)` added — one mincore
  syscall over the whole page-aligned range through the generic `syscall`
  trampoline with kernel-header `SYS_mincore`; 16 MiB cap (4 KiB static vec);
  fresh-evidence diagnostic tag `pool_range_verdict_fresh`; includes
  `<sys/syscall.h>`/`<errno.h>`; stale pre-v4 span-table comment corrected.
- 7 (59db9dcd): return-convention fix from on-target evidence — the Prospero
  trampoline returns kernel errors as POSITIVE raw codes and does not set
  errno (observed `raw_rc=12=ENOMEM`, `errno==0`). Mapping now accepts BOTH
  conventions so NotMapped is never demoted to Unavailable.

## Edit 4 — initializer (`22f06286` → `e7f349af`)

`launch11-combined/units/UnrealInitializer.cpp`: pool wait loop consults
`resolve_listing_gap` ONCE per boot on the first listing-gap miss
(`bridge_kernel_range_verdict(blocks0, kFNameBlockCapacityBytes)`,
`ReadableCoverageSource::None`); the resolution note is appended to any
`seed_refused` message. The normal shape-ok break condition is unchanged.

## Edit 5 — program (`7b5d4636` → `68b8bd18`)

`worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` `setup_mods()`
PROSPERO branch: `widen_ascii` lambda replaces both `ensure_str(mod.name)` /
`ensure_str(mod.path)` — kills the NUL-interleave double-garble (pid 700 log
18:40:10 "Mod is absent from native Lua source: u\0e\0 4\0s\0s\0…", 15 NULs at
log offset 1040, archived at
`launch10-evidence/pid700-fullfix/primary/fullfix-ue4ss.log`).

## Edit 6 — LuaMod (`3c39a86b` → `c92c92ff`, byte-diff archived)

`worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp`: all 8 unqualified
`normalize_path_for_lua(` call sites → `modbridge::normalize_path_for_lua(`.
Provenance of the fix: unqualified lookup from `RC::LuaMod` stopped at the
enclosing-namespace `RC::normalize_path_for_lua` (String.hpp, included before
the override block), so the local `modbridge::` override was dead code at
every call site. Comparator regenerated declared-divergence
(`8343578b…`, UND 540==540, defined delta = exactly the dropped dead symbol).

## Gate-facing consequences (all deliberate, all recorded)

- One NEW undefined symbol in the composed object: `syscall` (SDK-declared:
  `libc.a(syscalls.o) .text.syscall`, runtime.map lines 497820–497825) — the
  pass needs `--extend-undefined-baseline` recorded, not a gate edit.
- `PoolShapeCheck` grows 24→32 bytes (failure_class field) — noted; the
  initializer and fixtures compile against the same header.
- Regression-guard manifest re-pinned (widen edit + editions binding
  `05a064e6…`) — fail-closed gate passes exit 0.
