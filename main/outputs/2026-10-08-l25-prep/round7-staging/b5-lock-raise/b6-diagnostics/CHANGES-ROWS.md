# CHANGES rows for B6 (templates; nothing applied; `CHANGES.json` and `markers-declared.json` untouched)

## 1. Build label and ledger row

- Label: `B6-diagnostics` (one labelled, gated build; variant A only; record the sha in the ledger). It is built ON TOP OF
  `B5-lock-raise` = `sites/final-b5.diff` (NOT the 15-site proposed diff); do not build B6 without B5, and do not bundle them into one
  build. The base files are the final-b5-applied copies (`SHA256SUMS-base.txt`; LuaMod.cpp base sha `afaf5caa...3550`).
- Gates before compile (each must print its checked count and fail on zero): `fs_throw_gate.py`, `no_detour_gate.py`,
  **the ONE lock gate** `gate/lock_raise_gate.py --helpers off --unprotected-calls --metamethod-calls --forbid-raw-guard m_thread_actions_mutex`
  on the build's real compiled-files list (it knows `std::` guards, `b6diag::ActionsLock`, aliases and the banned `get_mod_ref`; see `gate/GATE.md`),
  `tests/b6_source_gate.py`. Its helper tier must report 0 NEW against `gate/baseline-b5-patched-UNREVIEWED.txt`.
  (`lock_raise_gate_b6.py` here is only a wrapper over that gate.)
- After packaging: `verify_build.sh --manifest` for the exact PRX with the rows in `MARKERS.md`.

## 2. `CHANGES.json` `files[]` rows (existing schema: original / copied_original / candidate / shas)

Four files change, no file is added, no header is added (`added_headers` unchanged), no new #include in any file (gate-checked).

```json
{"original": ".../candidate/src/LuaMod.cpp", "candidate_sha256": "<fill after apply: staged copy is 0c15fd2d...>",
 "note": "B6 diagnostics on top of final-b5: b6diag::ActionsLock replaces std::lock_guard/unique_lock on m_thread_actions_mutex at 46 sites (same locking, adds depth counter + wait/holder record); [LOCKLEAK]/[LOCKWAIT]/[PERF] lines; pump-side [MEM]; widget log rate limit; FindFirstOf/FindAllOf timers"},
{"original": ".../candidate/src/LuaMadeSimple/LuaMadeSimple.cpp", "candidate_sha256": "<fill; staged copy is dfe06c28...>",
 "note": "B6: per-thread lock-scope depth, leak checks at raise / registered-function return / call_function pcall, game-thread call timing hooks (diagnostics only)"},
{"original": ".../candidate/include/LuaMadeSimple/LuaMadeSimple.hpp", "candidate_sha256": "<fill; staged copy is 936a827e...>",
 "note": "B6: DiagHooks + diag_scope_enter/exit/depth/install_hooks declarations. NOTE: widely included header, so the recompile set is the same as B2's"},
{"original": ".../candidate/src/LuaPathShim.cpp", "candidate_sha256": "<fill; staged copy is 7d1ed2e3...>",
 "note": "B6: [SHIM] write-area dir ... why=; [SHIM] write-fail with errno/strerror once per mod"}
```

`new_os_imports`: expected none, with one item to confirm. `std::strerror` is new in `LuaPathShim.cpp`, but the port's own patched
`lauxlib.c` already calls `strerror` (`ue4ss-stdio-consumer-adaptation-1/patched/lauxlib.c:256,742`), so the PRX should already import it
(Mac-only inference from source). If the build's import-contract check lists it as new, drop the `strerror=` field (the `backend=` field
already carries the same text from liolib) rather than adding an import. `snprintf`, `std::chrono::steady_clock` and `std::mutex` were
already used by these units.

## 3. Marker rows

See `MARKERS.md` (8 new rows). Python form for `add_build_markers.py` (do not apply from here):

```python
'B6': [
    ('b6-lockleak',        '[LOCKLEAK] mod=',                            'utf-8',    'candidate/src/LuaMod.cpp on_leak (snprintf narrow literal)'),
    ('b6-lockwait',        '[LOCKWAIT] site_line=',                      'utf-8',    'candidate/src/LuaMod.cpp log_lockwait'),
    ('b6-lockwait-stuck',  '[LOCKWAIT] still waiting site_line=',        'utf-8',    'candidate/src/LuaMod.cpp scan_stuck_waiters'),
    ('b6-lockwait-summary','[LOCKWAIT] summary: ',                       'utf-8',    'candidate/src/LuaMod.cpp log_lockwait'),
    ('b6-perf',            '[PERF] dt=',                                 'utf-8',    'candidate/src/LuaMod.cpp format_perf'),
    ('b6-widget-summary',  '[l22] widget/object construct summary: ',    'utf-8',    'candidate/src/LuaMod.cpp emit_widget_summary'),
    ('b6-mem-pump',        '[MEM] state=pump gc_kb=',                    'utf-8',    'candidate/src/LuaMod.cpp b6_pump_1hz'),
    ('b6-shim-write-fail', '[SHIM] write-fail mod=',                     'utf-16le', 'candidate/src/LuaPathShim.cpp io_open_wrapper (wide literal)'),
],
```

## 4. What the build report should add for B6 (one table row each)

`[LOCKLEAK]` lines (expected 0 after B5; any line names the mod and function), `[LOCKWAIT]` lines (expected 0 to a few),
`[PERF]` rows (one per second while the game thread pumps; flag any `lua_ms` mod above ~8 ms per interval-second, any `q=busy`,
`widgets=` spikes, `find_*` ms), `[MEM] ... src=pump` rows, `[SHIM] write-fail` (once per mod, shows errno).
