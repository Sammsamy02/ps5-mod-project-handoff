# B6 markers (for the verify_build manifest and the report decoder)

Staged only. `harvest/markers-declared.json` and `harvest/add_build_markers.py` are NOT edited here; the
rows below are for whoever wires the B6 build. All line numbers are in the staged
`work/src/...` files (the files `b6.diff` produces on top of final-b5).

## How the lines are written (this decides the manifest encoding)

- New B6 lines are built with `snprintf` from NARROW literals and printed through one helper
  (`b6diag::emit_line`, LuaMod.cpp:680-716), which widens the ASCII buffer one char at a time. So the
  format text sits in the PRX as UTF-8 (`utf-8`). The runtime log line is the rendered text.
- Pre-existing wide `STR(...)` lines that B6 touches keep a stable prefix (`utf-16le`).
- Every narrow literal is one contiguous `snprintf` format string, so the prefix below is a contiguous
  byte run in the PRX (Mac-only expectation; the proof is `verify_build.sh --manifest` on the built PRX).

## New markers (add as B6 rows)

| id | text to require | enc | source (staged file:line) | what it proves is compiled in |
|---|---|---|---|---|
| b6-lockleak | `[LOCKLEAK] mod=` | utf-8 | LuaMod.cpp `on_leak` ("[LOCKLEAK] mod=%s fn=%s kind=%s extra=%d depth=%d repeats=%lld err=%s") | skipped-destructor detector reports |
| b6-lockwait | `[LOCKWAIT] site_line=` | utf-8 | LuaMod.cpp `log_lockwait` | acquisition over 250 ms is logged |
| b6-lockwait-stuck | `[LOCKWAIT] still waiting site_line=` | utf-8 | LuaMod.cpp `scan_stuck_waiters` | a lock that never returns is still reported (pump side) |
| b6-lockwait-summary | `[LOCKWAIT] summary: ` | utf-8 | LuaMod.cpp `log_lockwait` | rate limit summary |
| b6-perf | `[PERF] dt=` | utf-8 | LuaMod.cpp `format_perf` ("[PERF] dt=%lldms lua_ms={") | the 1 Hz pump line |
| b6-widget-summary | `[l22] widget/object construct summary: ` | utf-8 | LuaMod.cpp `emit_widget_summary` | the widget log is rate limited |
| b6-mem-pump | `[MEM] state=pump gc_kb=` | utf-8 | LuaMod.cpp `b6_pump_1hz` | [MEM] also comes from the pump |
| b6-shim-write-fail | `[SHIM] write-fail mod=` | utf-16le | LuaPathShim.cpp:921 (wide literal) | errno/strerror of the first failed sandbox write |

Canonical short markers asked for: `[LOCKLEAK] `, `[LOCKWAIT] `, `[PERF] ` (each is the prefix of the rows above).

## Existing markers that change text but keep the manifest prefix (no manifest edit needed)

| existing id | prefix still present | change |
|---|---|---|
| b4-shim-write-area-dir (`[SHIM] write-area dir `, utf-16le) | yes | line now ends `... usable={} why={}` (LuaPathShim.cpp:654) |
| b2-mem-line (`[MEM] state=`, utf-16le) | yes | old wide line untouched; the pump line is an extra narrow line with `state=pump` and `src=pump` |
| the per-widget line `[l22] widget/object constructed via StaticConstructObject` | yes | now at most 5 per second (LuaMod.cpp:3181-3190) |

## Collision note (read before merging with the lock-fix design)

`round7-staging/lock-fix/design/CHANGES-ROWS-1.md` declares `[LOCKLEAK] repaired queue-lock levels=%d site=%s`
and `[LOCK] ...` lines for a different build (DESIGN-1). B6 lines start `[LOCKLEAK] mod=`; the repair line
starts `[LOCKLEAK] repaired`. A decoder that matches on `[LOCKLEAK]` alone sees both; match on the
longer prefix. If both ever land together, rename one set before the build.

## Decoder hints (log lines are NUL-interleaved: use decode_session_log.py)

- `[LOCKLEAK] mod=<name> fn=<LuaFunction or ?> kind=<raise|return|pcall|pump> extra=N depth=N repeats=N err=<first 160 chars>`
  - `raise`: a Lua error was about to be raised while a lock scope opened inside that function was alive.
  - `return`: a registered function returned with extra scopes (an inner raise was swallowed by a pcall).
  - `pcall`: a pcall in call_function came back with an error and extra scopes.
  - `pump`: the game thread showed the same lock depth > 0 in two consecutive 1 Hz pumps with no dispatch running (this is what B4 would have printed).
  - first line per mod is immediate; later ones at most every 30 s with `repeats`.
- `[LOCKWAIT] site_line=N thread=game|other waited_ms=N` -> look N up in `SITES.txt`.
- `[LOCKWAIT] still waiting ... holder_line=N holder_thread=... holder_held_ms=N` -> who has had the lock since when. `holder=none-recorded` means nobody tracked holds it.
- `[PERF] dt=<ms> lua_ms={mod:<ms>/<calls>,...} find_first=<n>/<ms>ms find_all=<n>/<ms>ms widgets=<n> q=g,p,d,pd,e,pe (or q=busy) lockwait=<slow acquisitions>/max<ms>ms/over<count over 250 ms, cumulative> leaks=<cumulative> pump_depth=<n>`
  - `q=` is game, game-pending, delayed, delayed-pending, engine-tick, engine-tick-pending.
  - `lua_ms` only lists mods that ran game-thread Lua in the last interval; `?` is a state no mod registered.
- `[MEM] state=pump ... src=pump`: same fields as the existing [MEM]; `gc_kb` is the last value the ExecuteInGameThread path saw (the pump makes no Lua call).
