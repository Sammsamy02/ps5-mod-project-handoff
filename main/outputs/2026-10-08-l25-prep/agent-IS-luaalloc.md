# agent-IS-luaalloc — Claude round-13 item 1 (Lua allocator) — compact report

Agent IS, 2026-10-12. Analysis + staged patches only; battery-safe (no
builds, no console, no live-tree edits). Full analysis:
`outputs/2026-10-08-l25-prep/IS-LUA-ALLOCATOR.md`; staging + receipts:
`outputs/2026-10-08-l25-prep/round5-staging/lua-alloc/` (README-STAGING.md).

## The allocator

LuaMod.cpp:902 → `LuaMadeSimple::new_state()` → `luaL_newstate()` →
`lua_newstate(l_alloc, NULL)`; `l_alloc` is stock `realloc/free` from
`.superpowers/.../ue4ss-stdio-consumer-adaptation-1/patched/lauxlib.c`.
malloc/realloc/free resolve to the GAME PROCESS's `libSceLibcInternal`
(lineage import-contract evidence). No custom allocator, no lua_setallocf,
no FMalloc for Lua (FMalloc exists only as a vtable-layout name map).

## The cap

None in the port (no LUAI_MAXALLOC, no PRX pool, no arena, no FMalloc
limit). The failing ceiling is the shared game-process libc heap: the
pass-2 log shows the LUA_ERRMEM storm exactly during the engine's
save/map-load window (20:47:54–20:48:21) and full Lua recovery once the map
is in (20:49:39). Lua 5.4's emergency-GC retry already ran before each
reported failure (lmem.c), so failures are genuine backing-store refusals.
IMPORTANT second mechanism: our `TRY` wrapper converts any C++ exception to
`luaL_error(...)`, which ALLOCATES — under exhaustion that fails too, so a
`std::bad_alloc` (visible as truncated `[l22] ExecuteInG: std::ba` lines)
surfaces as `LUA_ERRMEM => not enough memory` with the real message
destroyed. "Not enough memory" on this port = allocation failed OR a
masked exception; the diagnostics disambiguate via `fails=` vs flat gc_kb.

## The leak

Small and real: self-test state's registry refs grow monotonically
(7→28→33→51→56) — part9's 8 no-unregister registration families re-armed
per run (deployed mod predates IU's run-once guard). Cannot explain the
storm by magnitude; 0003 fences it (once-only arming) and current main.lua
already enforces one run/session.

## The deliverables (round5-staging/lua-alloc/)

1. `patches/0001-lua-alloc-diagnostic.patch` (+ full staged files): counting
   allocator (realloc passthrough), `[lua-mem: …]` appended to every
   LUA_ERRMEM line, 1/s `[MEM]` line with lua_gc(LUA_GCCOUNT) + one
   LUA_GCSTEP + allocator counters, new `GetLuaMemory()` Lua binding; arena
   code compiled-in but env-OFF (byte-identical when off).
2. `patches/0002-lua-arena-enable.md`: decision gate + activation runbook
   (UE4SS_LUA_ARENA=1, UE4SS_LUA_ARENA_MB=64 default) — 64 MiB startup
   reservation, 32 size classes for blocks ≤2048 B, malloc-with-header for
   larger, per-block malloc fallback, zero new link-time imports.
3. `patches/0003-ueselftest-mem-hygiene.patch`: `MEM before/after` per run +
   once-only registration arming.
4. Verification: prospero-clang++ -fsyntax-only rc=0 for both staged TUs
   (exact frozen l26-A flag sets), luac -p OK for both Lua files, host
   ASan+UBSan fuzz of the arena logic PASS (200k ops).

## Recommended sequence

Diagnostic launch with 0001+0003 (arena OFF) → read [MEM]/MEM-before-after
→ if `fails` rises with flat `gc_kb` (expected, matching pass-2), enable
arena per 0002 on the following launch; if `gc_kb` climbs instead, hunt the
ref leak with the once-armed evidence.
