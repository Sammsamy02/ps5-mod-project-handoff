# DIFF.md — worker-l11-loadmap-guard r2

One replace-unit source revision: the `luamod` staging copy gains the LoadMap
deferral gate (2 hunks, one file). Base: the marshal-consumer staging
(`worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp`,
`04c9d916…`). Deliverable diff: `diffs/l11-loadmap-defer-r2.diff`.

| link | artifact | sha256 (first 16) |
|---|---|---|
| base | `worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp` | `04c9d91664dd82c7` |
| r1 (preserved, superseded rationale) | `diffs/l11-loadmap-defer.diff` / `staging/luamod-l11-loadmap-defer/LuaMod.cpp` | `da8923ba345ba791` / `d661e68fd32c1acd` |
| r2 diff (DELIVERABLE) | `diffs/l11-loadmap-defer-r2.diff` | `d2ad2818db4b7338` |
| r2 staged | `staging/luamod-l11-loadmap-defer-r2/LuaMod.cpp` | `9a7a37d6c8377c02` |
| control obj | `work/luamod-control-r2.o` (base under frozen argv) == marshal-integration consumer object | `854a476bdd4f9985` |
| r2 obj | `work/luamod-defer-r2.o` (r2 staged under frozen argv, NEW) | `3eeba02fd92efd99` |

## What changed vs the marshal-consumer staging

Exactly TWO hunks in `LuaMod.cpp`:

1. **Hunk 1 (on_program_start, after `common_opts` at :7323):** a
   `#ifdef __PROSPERO__` gate — `if (Unreal::R3MarshalPumpPending())` emits ONE
   `LogLevel::Warning` line (`[R3] LoadMap detour registrations deferred: …`)
   and skips the eager-arm block; `else` falls through to it unchanged. The
   comment block states the MEASURED routing (owned-memory-mediated soft
   refusal; not the WalkRxGuard site; not an unguarded copy), the honest reasons
   to defer (R3D §5.3 item 3; log hygiene; process-lifetime loss declaration),
   the Lua-side loss (RegisterLoadMapPreHook/PostHook accepted but never
   invoked; TownfallItemSpawner main.lua:731-747 per-map reset lost), and the
   reachability bound (fold-0 walk refusal aborts before on_program_start).
2. **Hunk 2 (after the last eager registration, "Enabling custom events" /
   ProcessInternal script hook):** the closing brace of the gated block, tagged
   `[R3][worker-l11-loadmap-guard r2]`.

Zero new imports (the undefined-symbol sets are IDENTICAL —
`R3MarshalPumpPending` was already UND in the control and resolves against the
armed-fold initializer at link). Same 172-token argv, same driver machinery,
same program source.

## Machine checks

- **Round-trip**: `patch -p1` on a fresh byte-verified base copy reproduces the
  staged r2 source byte-identically (`work/roundtrip`, `cmp` clean).
- **Compile gate**: both objects exit 0, 0 errors, 23 baseline warnings each
  (kind parity with the r1 gate and the marshal-integration gate receipts).
- **Controller steps** (unchanged from RUNBOOK §2, one substitution): point the
  replace receipt's `add-luamod-replacement-source` at the r2 staging
  (`3eeba02f…` is the expected fresh object under the frozen argv; the compose's
  luamod.o comparator replaces `854a476b…` with `3eeba02f…`; all other
  comparators, gates, and the manifest derivation carry over verbatim).
- **Rollback**: point the replace receipt back at the marshal-consumer staging
  (`04c9d916…`); the compose comparators revert to the marshal-integration
  world. The r1/r2 stagings are worker-owned files and are never referenced by
  any delivered driver.
