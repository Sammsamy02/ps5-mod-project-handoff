# shim-fix APPLY NOTES (agent IQ, Claude round-12 items 1-2, 2026-10-12)

Contents of this staging dir (`round5-staging/shim-fix/`):

| File | What |
|---|---|
| `LuaPathShim.cpp` | The corrected TU (base: `round4-setaside/LuaPathShim.cpp` sha `821d74a2…f3aeb3` — byte-identical to the deployed crashed build's `typescan-r4/candidate/src/LuaPathShim.cpp`, verified this session) |
| `01-luapathshim-writearea-fix.patch` | Candidate-relative diff (`a/src/LuaPathShim.cpp` → `b/src/LuaPathShim.cpp`), `patch -p1` from the candidate root |
| `deploy-precreate-writable.py` | DEPLOY-TIME pre-creation of `/data/modbridge/writable/<mod>/` (drop-in step 5.6 for `harvest/prep_modded_launch.py`; standalone-runnable; `--dry-run` = zero network) |
| `syntax-receipt-LuaPathShim-shimfix.log` | prospero-clang++ `-fsyntax-only` on the staged TU: EXIT=0, 0 errors, 0 warnings (luamod-unit argv from `EY-TARGET-COMPILE.json`, `LLVM_CONFIG` per the IP recipe) |

## Apply order (next AC-power build window)

1. Shim TU: from `outputs/2026-10-08-l26-runtime/typescan-r4/candidate`:
   `patch -p1 < <this dir>/01-luapathshim-writearea-fix.patch`
   (or copy `LuaPathShim.cpp` over `candidate/src/LuaPathShim.cpp` — same bytes).
   The PUBLIC SURFACE IS UNCHANGED (`LuaPathShim_install(L, mod, root)` 3-arg;
   `LuaPathShim.hpp` untouched; `LuaMod.cpp` needs NO edit; the
   `LuaMod-insertion-block.txt` extern still matches verbatim).
2. Build driver: no edit needed — unit `lua-pathshim` already registers this
   path (`build_variants.py:124`); refresh the CHANGES-row hash per the
   standard restore law (same as every shim revision since GA).
3. Deploy: add step 5.6 to `harvest/prep_modded_launch.py` per the wiring
   block in `deploy-precreate-writable.py`'s docstring (after step 5.5
   rowcheck, before step 6 arm). Root may also run it standalone before any
   launch that includes the shim.

## What the fix changes (see IQ-SHIM-AUDIT.md for the full audit)

1. install(): NO filesystem work, NO directory creation, stack fully
   balanced, all wrapper context upvalues sourced from the REGISTRY anchor
   (fixes the stale-`context_index` misbind of os.remove/os.rename —
   pre-fix `LuaPathShim.cpp:1592-1593`), `setglobal("io")` remains the
   section's only global mutation and its LAST operation.
2. Write path: physical fopen first (deploy pre-created dir) → ONE-SHOT
   lazy `create_directories` (checked `error_code` overload, budgeted
   `[SHIM] write-area dir … created=… usable=…` marker) + one retry →
   in-memory spill. No failure path can crash; worst case is liolib
   semantics (working spill handle, or nil+message at the spill cap).
3. Deploy-time pre-creation makes the physical path the COMMON case, so the
   next launch also measures whether console physical writes work at all
   (marker + post-session ftpsrv `ls /data/modbridge/writable/`).

## Markers for the launch manifest

Existing (unchanged literals, already in II's round-5 marker set):
`[SHIM] write-area `, `[SHIM] path-resolver installed`, `[SHIM] io-open resolved=`.
NEW literal (first-10 budget, first write per parent dir only):
`[SHIM] write-area dir ` — root decides whether to pin it in the manifest
grep list (recommended: yes, it is the physical-vs-spill discriminator).

## Out of scope / NOT claimed

- No build, no console contact, no live-tree edits this session (battery
  law). The syntax receipt is the only compile evidence.
- The crash mechanism identification is bounded by elimination (see audit
  §2); the fix removes the whole risk surface rather than betting on one
  instruction. A klog fault-address capture at the next sanctioned launch
  would close it definitively.
