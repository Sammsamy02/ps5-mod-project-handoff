# 0002-lua-arena-enable — runtime activation of the Lua arena (no code change)

[IS-LUA-ALLOC] The arena code ships INERT inside 0001 (compiled in, disabled
by default: byte-identical realloc behavior when off). This "patch" is the
activation runbook for the launch AFTER the diagnostics have been read.

## Decision gate (from the 0001 [MEM] evidence)

Turn the arena ON only if the next launch shows EITHER:

- `fails=` rising while `gc_kb=` stays flat (backing store refusal, i.e. the
  game-process libc heap is squeezed — the 2026-10-11 pass-2 signature), OR
- `fails=` rising together with `gc_kb=` climbing beyond what the mods should
  pin (then ALSO hunt the leak with the once-armed self-test evidence).

If `fails=0` all session, the allocator was never the constraint and the
arena stays off (no risk taken).

## Activation

The PRX reads two env vars once, before the first Lua state is created:

```
UE4SS_LUA_ARENA=1        # enable the dedicated small-block reservation
UE4SS_LUA_ARENA_MB=64    # optional size, 8..512 MiB, default 64
```

On PS5 the payload environment is inherited from the game process; set the
vars in the ploader ini / mod service environment the same way other env
knobs are delivered, or export them from the module_start path if the env
route is unavailable in that launch configuration (follow the established
env-var delivery path used by prior diagnostics; do not invent a new one).

## What it changes

- All Lua allocations <= 2048 B (the overwhelming majority of GCObjects,
  strings, table nodes) are served from ONE 64 MiB reservation taken at
  startup, before the engine starts squeezing the shared libc heap during
  save/map load. They can no longer fail due to game-load pressure.
- Larger blocks stay malloc-backed (with a 16-byte header) and remain
  honestly observable as `arena_ext_kb=`; a large-block failure still
  surfaces as LUA_ERRMEM with the `[lua-mem: ...]` note.
- Resize-in-place is honored whenever the block's class capacity fits;
  growth copies (memcpy) like realloc; shrink never moves the block.
- Arena exhaustion degrades per-block to malloc (never a hard fail from the
  arena itself), and `[MEM] arena_cap_kb/arena_used_kb` make stranding
  visible.

## Guardrails honored

- No new link-time imports: malloc/realloc/free/getenv only (frozen
  207-import contract untouched).
- Default OFF: without the env var the allocator is byte-identical to the
  frozen build's realloc path (plus counters).
- Reservation is attempted halving 64 -> 8 MiB; if all fail, arena stays off.
