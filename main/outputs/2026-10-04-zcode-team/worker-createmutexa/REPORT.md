# worker-createmutexa — CreateMutexA (ANSI) shim proposal

Status: COMPLETE, proposal-only. All files under
`outputs/2026-10-04-zcode-team/worker-createmutexa/`; canonical sources untouched
(sha256 provenance in `selftest/sha256.txt`).

## Why

The census shows UE4SS.dll imports `CreateMutexA` (ANSI, CP_ACP), which the
mutex family does not bind today (binder handles only the six wide names).
This is a thin adapter over the existing `mb_win_mutex_create` path, not a new
family.

## Design

`CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL bInitialOwner, LPCSTR lpName)` —
same Microsoft x64 signature shape as CreateMutexW with a single-byte name.
Adapter policy for the PS5 target (no codepage tables / no locale): each ANSI
byte is **zero-extended** into one UTF-16 code unit
(`(uint16_t)(unsigned char)b`), NULL name stays NULL, and the converted name is
handed to the unchanged `mb_win_mutex_create(installed, wide, flags)`. All
namespace rules therefore stay enforced in exactly one place — the wide path's
`parse_name` (`mb_win_mutex.c:92`): `Local\` prefix strip, other-backslash
refusal (87), A–Z case folding, 260-unit MAX_PATH bound.

Over-long names: the conversion buffer is `MB_WIN_MUTEX_MAX_NAME+1` units. If
the ANSI source has no terminator by then, the buffer is passed **unterminated
at 261 units**, so `parse_name` itself measures 261 > 260 and fails
`ERROR_FILENAME_EXCED_RANGE` (206) — byte-identical behavior to the W entry
point, without the shim touching any last-error API. A 260-unit ANSI name
NUL-terminates inside the buffer and is accepted, matching the W path exactly.
Stack cost: 522 bytes per call.

Binder: `else if(MATCH(i->name,"CreateMutexA",0))` added directly after the
CreateMutexW line (exact-name match, kernel32.dll ASCII-folded, ordinals still
unresolved). Header comment updated six -> seven names plus the CreateMutexA
signature note.

## Deliverables

- `mb_win_mutex_a_integration.diff` — unified diff for
  `outputs/ps5-mod-bridge/windows-services/mb_win_mutex_abi.c` (shim function
  `create_mutex_a` + binder line) and `mb_win_mutex_abi.h` (comment-only).
  `patch -C -p1` dry-run against the canonical tree: **DIFF_APPLIES_CLEAN**.
- `selftest/patched/mb_win_mutex_abi.c| .h` — canonical + diff applied
  (byte-identical to original outside the hunks; provenance hashes in
  `selftest/sha256.txt`).
- `selftest/test_createmutexa.c` — host integration test.
- `selftest/run_selftest.sh` + `build.log` / `run_output.txt` — reproducible
  build+run receipt.

## Host-test evidence (x86_64 ms_abi build, run via Rosetta; 0 warnings)

`arch -x86_64 ./run_createmutexa` →
`PASS: CreateMutexA adapter (7 passes)`:

1. **Identity**: `CreateMutexA("single.obj")` then `CreateMutexW(L"single.obj")`
   → second handle succeeds with ERROR_ALREADY_EXISTS(183); wait/grant on the A
   handle, owner re-wait via the W handle returns WAIT_TIMEOUT, release/grant
   cycles across both handles → same underlying object.
2. **Case folding**: `"fold.Case"` vs `"FOLD.case"` via A → 183; OpenMutexW with
   folded wide name resolves the A-created object; unknown name → 2; after last
   handle retires the name is gone (open → ERROR_FILE_NOT_FOUND).
3. **Zero-extension (key conversion test)**: `CreateMutexA("\x80tail")` resolves
   via `OpenMutexW({0x0080,'t','a','i','l'})` and does NOT resolve via
   `{0xFF80,...}` — proves unsigned zero-extension (a signed-char cast would
   produce 0xFF80 and fail).
4. **`Local\` prefix parity**: `CreateMutexA("Local\\pfx")` vs
   `CreateMutexW(L"local\\PFX")` → 183.
5. **Unnamed rules**: NULL and `""` both create unnamed objects (nonzero
   handle, LastError preserved).
6. **MAX_PATH/backslash parity**: 260-unit name OK; 261-unit name fails 206
   (from parse_name itself, as designed); `"bad\\name"` fails 87.
7. **bInitialOwner**: created owned → immediate wait returns WAIT_TIMEOUT,
   ReleaseMutex succeeds.

LastError discipline verified throughout (success preserves; failures write
only the documented code).

## Integration instructions for the lead

Apply `mb_win_mutex_a_integration.diff` with `patch -p1` from the repo root
(labels are `a|b/outputs/ps5-mod-bridge/windows-services/...`). The shim has no
new dependencies, no allocation, and no new error paths of its own; PS5-side
verification (prospero-clang object + IR census) is out of this worker's scope
and remains with the engine verifier, same as the existing six bindings.

## Honest limits

- Host-tested only (x86_64/Rosetta); no PS5 SDK run in this packet.
- CP_ACP on real Windows would do locale-aware widening; the verbatim
  zero-extend is the documented adapter policy for a locale-less target and is
  the only sane mapping for the ASCII names UE4SS actually uses.
- Not applied to the canonical tree by this worker (proposal-only scope).
