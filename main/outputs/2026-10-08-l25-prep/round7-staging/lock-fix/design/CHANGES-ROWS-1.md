# CHANGES-ROWS-1 — rows to add for DESIGN-1 (templates; shas are filled in by the implementer)

Nothing here has been applied. `CHANGES.json`, `markers-declared.json` and the candidate tree are untouched.

## 1. Build label and ledger row

- Label: `B5-lockfix-1` (one labelled, gated build per fix; variant A only; keep every intermediate, record its sha in the ledger).
- Gates before compile: `fs_throw_gate.py`, `no_detour_gate.py` (this change adds no detour, no executable change, no new OS import).
  Each must print its checked count and fail on zero.
- Optional new gate before compile: `lock_raise_gate.py` (spec in DESIGN-1 section 9). Must print "lambdas checked N, raise sites checked M" and exit non-zero when N or M is zero.
- After packaging: `verify_build.sh --manifest` for the exact PRX, with the marker rows below added to the manifest.

## 2. `CHANGES.json` rows

`files[]` (existing schema: original / copied_original / candidate / shas):

```json
{
  "original": ".../candidate/src/LuaMod.cpp",
  "candidate": ".../round7-staging/lock-fix/<impl>/src/LuaMod.cpp",
  "candidate_sha256": "<fill>",
  "note": "DESIGN-1: QueueLock swap, dispatch scope rewrite, key queue, fast path, unlock_early x14, ref-on-own-stack x10, Tier-2 script/notify hooks"
}
```

`added_headers[]` and `added_header_hashes{}` (same mechanism as `VmtTickPumpCore.inc`):

```json
"added_headers": [".../candidate/src/LuaModLockLayer.inc"],
"added_header_hashes": {".../candidate/src/LuaModLockLayer.inc": "<sha256 of the file as built>"}
```
Prototype sha (proto-1, `proto-1/SHA256SUMS.txt`, verified): `fed0eb96ec9d00aae2fc521239f7fd4f663eec702cc843686fc661e5c88a4a36`. It will change during implementation; record the final file's sha, not this one.

Include-graph gate: no new system include in `LuaMod.cpp` (builtins only). The only new include edge is the project file `LuaModLockLayer.inc`, which depends on `<chrono> <cstdint> <cstdio> <cstring> <mutex> <thread>`; all are already in `LuaMod.cpp`'s graph.
No file other than `LuaMod.cpp` and the new `.inc` changes, so the recompile set is one translation unit.

## 3. `markers-declared.json` rows (verify_build manifest mode)

All five are narrow `snprintf` format literals (the stats and leak lines are built with `std::snprintf` and handed to `Output::send` as text), so the primary
encoding is UTF-8. Runtime-rendered lines never appear contiguously in the PRX, so the declared texts are the composable format literals.

```json
{
  "id": "lock-stats-line",
  "fix": "B5 lock fix (DESIGN-1): [LOCK] statistics line, at most one every 10 s",
  "canonical_marker": "[LOCK] ",
  "required": [
    {"text": "[LOCK] %s t=%llds key q=%lld d=%lld deferred=%lld ovf=%lld wait_ms mean=", "encodings": ["utf-8"],
     "source": "LuaModLockLayer.inc emit_now_ (first part of the stats format; one contiguous literal after concatenation)"},
    {"text": " | qlock takes=%lld contended=%lld wait_max_ms=", "encodings": ["utf-8"], "source": "LuaModLockLayer.inc emit_now_"},
    {"text": " | modlock game_defers=%lld key_yields=%lld nested_skips=%lld hook_skips=%lld order_viol=%lld leaks=%lld raise_unlocks=%lld",
     "encodings": ["utf-8"], "source": "LuaModLockLayer.inc emit_now_"},
    {"text": "init layer=B5-1", "encodings": ["utf-8"], "source": "LuaModLockLayer.inc emit_now_ (beacon on first use)"}
  ],
  "note": "A PRX without these has no lock statistics and probably not the layer (phantom-build class)."
},
{
  "id": "lock-leak-line",
  "fix": "B5 lock fix (DESIGN-1): leak repair line",
  "canonical_marker": "[LOCKLEAK]",
  "required": [
    {"text": "[LOCKLEAK] repaired queue-lock levels=%d site=%s", "encodings": ["utf-8"], "source": "LuaModLockLayer.inc emit_leak_line_"}
  ]
}
```
(Mac-only check: a small script joined the adjacent string literals of the prototype's stats format and found all five texts; the real proof is `verify_build.sh --manifest` on the built PRX.)
If the implementer switches `LOCKLAYER_EMIT` to a wide `STR()` literal for any of these, the entry must list `utf-16le` instead; do not guess, grep the PRX.

## 4. Log markers the B5 decoder/report should look for

`[LOCK] init layer=B5-1` (once, first Poll), `[LOCK] stats ...` (every 10 s), `[LOCK] final ...` (uninstall), `[LOCKLEAK] ...` (only if something leaked).
Decode with `harvest/decode_session_log.py` (NUL-interleaved; no plain grep). Report fields: DESIGN-1 section 8.
