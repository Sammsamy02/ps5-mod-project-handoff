# agent-Z19: E1 one-line compile fix (UObjectArray.cpp const_cast)

Date: 2026-10-08. Scope: exactly one line in
`outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp` plus its
`typescan-r4/CHANGES.json` row. Nothing else touched; no builds run beyond the
`-fsyntax-only` / single-TU `-c` verification below; no console contact. E3
(header routing for `candidate/include/Unreal/UObjectArray.hpp`) was NOT touched —
that is the parallel agent's assignment (`target_compile.py` / `headers.vfs.json`
unchanged by me).

## 1. The bug (Z13 finding E1)

`candidate/src/UObjectArray.cpp:610`, inside the new L25
`UObjectArray::RemoveUObjectDeleteListener` post-removal header dump. The line
inlined `DeleteListeners.GetData()` — which returns
`RC::Unreal::FUObjectDeleteListener**` (non-const `T**`) — directly into
`const_cast<void*>(...)`. `const_cast` from `T**` to `void*` is ill-formed (it is
neither a constness adjustment nor a allowed target type), so the TU failed on
BOTH host clang and the real prospero-clang with
`error: const_cast from 'ElementType *' (aka 'RC::Unreal::FUObjectDeleteListener **') to 'void *' is not allowed`.

The sibling lines in the same function (509, 514, 555, 599, 603) are legal
because they `const_cast` a `const void*` local (`pre_data` / `post_data`); line
610 was the only direct-call site of the pattern in the file (verified by grep).

## 2. The fix (one line, line 610)

What the `void*` feeds: the `Data={}` argument of an `Output::send` diagnostic
log line — a pointer printout, identical purpose to the sibling `pre_data`
lines. So the correct well-formed form is the value-preserving explicit
pointer-to-`void*` conversion; `static_cast<void*>` (the exact minimal fix Z13
validated as shim S1). Semantics identical: same pointer value (address of the
TArray's data buffer) passed to the same formatter. No comment added — the
target API's expectation is obvious from the sibling lines.

Before (line 610):

```cpp
            Output::send(STR("[L25][DeleteListener] Remove header post: Data={} Num={} Max={} (was_present={})\n"),
                         const_cast<void*>(DeleteListeners.GetData()), DeleteListeners.Num(), DeleteListeners.Max(), was_present ? 1 : 0);
```

After (line 610):

```cpp
            Output::send(STR("[L25][DeleteListener] Remove header post: Data={} Num={} Max={} (was_present={})\n"),
                         static_cast<void*>(DeleteListeners.GetData()), DeleteListeners.Num(), DeleteListeners.Max(), was_present ? 1 : 0);
```

Diff = `const_cast` -> `static_cast` on that one argument expression. Byte-level
change: 1 line of the file.

## 3. Compile proof

All runs used the reusable Z13 harness at `/tmp/l25-combined-smoke/` (stub
headers, adapted receipt argv, same -I set) with the source argument pointed at
the REAL fixed tree file (the harness JSON pointed at Z13's /tmp shim, which was
NOT used as evidence here). Z13's original logs were left untouched; new logs
were written under fresh names.

1. Host `/usr/bin/clang++` (Apple clang 17.0.0, arm64), Z13's exact adapted
   argv (stubs + l25 include roots first, `-std=c++23 -fms-extensions
   -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero -D__PROSPERO__ -D__SCE__
   -fshort-wchar`, same vfs overlay), source = fixed real file:
   - `-fsyntax-only`: **exit 0** (log `logs/e1fix-host-syntax-uobjectarray.log`)
   - full `-c` codegen: **exit 0** (`logs/e1fix-host-obj-uobjectarray.log`,
     object `obj/e1fix-uobjectarray.o`)
   Before the fix this same TU FAILED host `-fsyntax-only` with the E1 error
   (Z13 matrix row UObjectArray.cpp).
2. Real target compiler fidelity: `prospero-clang++` (receipt argv verbatim with
   only the `target_compile.py`-equivalent `-I` insertions, no stubs, no
   shims; source = fixed real file), `-fsyntax-only`: **exit 0**, only the two
   pre-existing inherited `-Wswitch` warnings at port
   `Unreal/Core/Templates/Atomic.hpp:241,262` (log
   `logs/prospero-syntax-e1fix-unreal-uobjectarray.log`). Z13's identical
   prospero run on the pre-fix file FAILED with E1.
   Note: this fidelity run resolves `<Unreal/UObjectArray.hpp>` to the OLD
   port-tree header (include trace in the log) — that is the E3 routing gap,
   still open with the parallel agent; it does not affect this body-level cast
   fix, which is header-variant-independent.

## 4. CHANGES.json update

`outputs/2026-10-08-l25-runtime/typescan-r4/CHANGES.json`:

- `candidate_sha256` recomputed for `candidate/src/UObjectArray.cpp` in BOTH
  rows that carry it (the `changes[]` row and the `files[]` row; verified
  against the file after the edit, and JSON re-validated):
  - before: `668b2682751e47e5dc4fe0a1594f1df896f793e21c7c255057ff6f3db5a79575`
  - after:  `8f45e6ce03fb066dee130c57ded022d555ace23b41860403975cae8a03c45c9c`
- Appended a dated `review_fixes` entry (2026-10-08, source = Z13 combined-smoke
  finding E1, fix by Z19) describing the one-line change, the verification
  above, and the hash rotation. No other JSON fields altered.

## 5. Files touched

- `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp`
  (1 line)
- `outputs/2026-10-08-l25-runtime/typescan-r4/CHANGES.json` (2 hash fields + 1
  appended review_fixes entry)
- /tmp only (verification): 3 new logs + 1 new object under
  `/tmp/l25-combined-smoke/`; Z13's logs/objects/shims untouched.
