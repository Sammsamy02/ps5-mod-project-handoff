# agent-IR — no-detour static gate (Claude round-12 item 3)

Deliverable: `outputs/2026-10-08-l25-prep/harvest/no_detour_gate.py`
(build-pipeline gate, same slot as `fs_throw_gate.py`; runs BEFORE compile;
Mac-local, battery-safe; no builds, no console, no tree edits).

## Gate behavior

Scans the candidate tree (`outputs/2026-10-08-l26-runtime/typescan-r4/candidate`
by default, `--root` for more) plus staged patches (`--patch` or a `.patch`
positional arg). Only **PS5-live** code is enforced — a small preprocessor
tracker skips `#ifndef __PROSPERO__` / `#else` of `#ifdef __PROSPERO__` /
`#ifdef _WIN32` regions (conservative: unevaluable conditions count as live).
Comments and string literals are stripped before matching, so the "[R3]
refused" / "detour installation likely failed" informational lines never fire.

Flagged (exit 1, one line per violation with file:line):
- **A** direct PLH/PolyHook/MinHook/Detours APIs (`PLH::`, `x64Detour`,
  `IatHook` installs via `->hook()`, `DetourAttach`, `MH_CreateHook`,
  `InstallDetour`, `hook_func`, `InstallHook/ActivateHook/EnsureHooked`).
- **B/D** writes of function pointers or bytes INTO code pages — write-through
  or memcpy/memset destinations derived from code-segment pointers
  (`text_base`, `code_ptr`, `.text`, …; compound markers only, so `FText* text`,
  `whence_text`, `code_image` etc. are not flagged).
- **C** `mprotect`/`mmap` with `PROT_EXEC` (and `VirtualProtect`/`PAGE_EXECUTE*`).
- **E1** the LoadMap detour-arm path (`Unreal::Hook::RegisterLoadMapPre/PostCallback`)
  outside a **recognized soft-refused shape** (R3 markers within ±60 lines:
  `UE4SS_R3_MARSHAL_MODE`, `R3MarshalGate::refuse_registration_site`,
  `R3MarshalPumpPending`, `m_loadmap_deferred_arm_pending`,
  "[R3] LoadMap deferred arm refused").
- **E2 (patch mode)** pure-VMT: while the composed tree still contains the
  LoadMap detour-arm family, any patch that ADDS LoadMap vtable/slot/poke/entry
  machinery must ALSO remove at least one detour-arm line. This encodes
  Claude's exact sentence — the LoadMap hook still needs the *pure VMT
  redesign* AND the no-detour gate.

Allowed by design (never flagged): UFunction `->Func =` swaps (data writes to
UFunction objects), vtable slot swaps on PRIVATE copies (the VmtTickPump law,
`*copy_slot = …`), the R3 FuncPtr-marshal carriers, comments/log lines
mentioning detours. Exemptions: `// [DETOUR-APPROVED] <reason>` per line; a
`# [DETOUR-APPROVED] <reason>` diff comment exempts a whole patch from E2.
Empty-scan is an env error (exit 2), never a silent pass.

## Test results (built-in `--selftest`, ALL PASS)

- 14 fixture cases: every rule fires on its class (PLH APIs, install
  primitives, PROT_EXEC, byte-patch, write-through, bare LoadMap arm) and
  every allowed class stays clean (mmap-with-variable-protection, buffer
  memcpy, UFunction Func swap, copy-slot poke, R3-guarded LoadMap arm,
  `#ifndef __PROSPERO__` PLH code, exemptions, comments/log lines).
- **R1 current tree: PASS, exit 0** — 45 C++ sources, 0 violations; the four
  existing LoadMap detour-arm sites (LuaMod.cpp:5240/5241/8723/8725) are
  reported as NOTEs in recognized soft-refused (R3) shapes — visible and
  auditable, matching "the detour paths are soft-refused".
- **R2 the LoadMap patch (round5-staging/world-fix/04-event-map-logger.patch):
  FAIL, exit 1** — one E2 violation: the patch adds 63 lines of LoadMap
  vtable/slot machinery while removing none of the tree's
  RegisterLoadMap*Callback / try_deferred_loadmap_arm family, i.e. it keeps a
  detour attempt reachable instead of finishing the pure VMT redesign. Note
  honestly: the patch's own poke (`*copy_slot = …` on the pump's private
  copy) is an ALLOWED data write and is NOT the trigger; the trigger is the
  retained detour path, exactly Claude's item.
- R3/R4/R5 lifecycle: a machinery patch passes once the family is gone from
  the tree; a redesign patch (machinery + detour-arm removal) passes E2; a
  patch adding a raw `PLH::x64Detour` is caught in patch mode too.
- Extra end-to-end (not in selftest): `# [DETOUR-APPROVED]` header on a copy
  of the real patch → exit 0; a bare `RegisterLoadMapPreCallback` appended to
  a copy of the real tree → E1 violation, exit 1.

## Side-finding (pre-existing bug, not edited)

`harvest/fs_throw_gate.py` computes `WHAT` one dirname too short
(script sits at `<worktree>/outputs/2026-10-08-l25-prep/harvest/`, three
dirnames = `<worktree>/outputs`), so its SRC glob
`<worktree>/outputs/outputs/…/candidate` matches NOTHING — it currently
prints PASS after scanning zero files. no_detour_gate.py uses four dirnames
and hard-fails (exit 2) on an empty scan; fs_throw_gate.py should get the
same two fixes when its owner next touches it.

## Pipeline integration

    python3 outputs/2026-10-08-l25-prep/harvest/no_detour_gate.py \
        [--root DIR]... [--patch FILE.patch]...

Exit 0 pass / 1 violations / 2 env-usage. Compose drivers should pass every
staged `.patch` before compiling (E2 needs the tree roots to know whether the
detour-arm family is still present; the default root already covers it).
