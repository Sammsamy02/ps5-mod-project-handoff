# agent IQ — shim write-area audit + fix staging (Claude round-12 items 1-2)

2026-10-12. Battery-safe: analysis + staged patches + ONE prospero
`-fsyntax-only` receipt (EXIT=0, 0 errors, 0 warnings). No builds, no
console, no live-tree edits. Full audit: `IQ-SHIM-AUDIT.md`. Patches:
`round5-staging/shim-fix/`.

## The audit verdict

Audited the EXACT deployed crash source (round4-setaside LuaPathShim.cpp,
sha 821d74a2 — byte-identical to the candidate copy the crashed builds
compiled). Every fs call from install() through first write, checked/unchecked:

| call | pre-fix site | checked? |
|---|---|---|
| fopen loadlist.txt | :1417 | YES (NULL-guarded; runs BEFORE the crash marker → proven safe by the logs) |
| exists/is_directory/dir-iterator (case walk) | :291/:322/:331 | YES (bool/ec-consumed) |
| create_directories (ensure_parent_dirs) | :583 | ignored-but-unusable-as-NULL (error_code overload, first-write only — NEVER REACHED in the crashed sessions) |
| backend fopen (patched liolib io_open) | liolib.c:277-278 | YES — `(p->f==NULL) ? luaL_fileresult : 1` (nil+msg, never crashes) |

**The crash window contains NO filesystem calls.** Log writes are
synchronous (looped ::write on the retained fd — CE/CL receipts), so both
crashes are bounded to LuaPathShim.cpp :1549-:1623 — between the write-area
marker (:1548) and the never-printed path-resolver-installed marker (:1624).
That refutes the acceptance-doc "fopen/create_directories faulted" family
AND Claude's fs-form "unchecked NULL" guess for THIS crash (the first write
never ran — the crash precedes the mod's main.lua).

## The crash line

- **Provable defect in the window** (Claude's predicted shape, non-fs
  origin): stale absolute index. `context_index`==2 captured at :1551 is
  INVALIDATED by `lua_pop(L,1)` at :1585 (stack shifts); `wrap_into` at
  **:1592-1593** then binds **the OS TABLE** as the os.remove/os.rename
  context upvalue. Degrades safely (lua_touserdata→NULL, checked) — a real
  bug (os.remove/rename permanently refused), not the segfault itself.
- **Crash site by elimination: the io-table replacement region :1568-:1585**
  — the only first-ever-executed state-mutating code in the window (the
  clean build 1e49188b self-disabled at no-root and never ran it; both
  shim-root builds died exactly there, zero error output). Honest limit: a
  single faulting instruction cannot be proven from two log excerpts; the
  fix removes the whole surface instead of betting on one line. Klog fault
  address at the next sanctioned launch settles it.

## MFA exemption: confirmed, no code needed

mfa_activate reads ONLY `<root>/loadlist.txt` + its listed rows
(join_path/read_loadlist/read_exact — no walk, no blocklist, no
interception). `/data/modbridge/writable` is a SIBLING of roots/ →
structurally outside admission; no row ever names it → the -6 incident
class is impossible; step-5.5 rowcheck unaffected.

## The fix (Claude's design, verbatim)

1. Deploy pre-creation: `deploy-precreate-writable.py` — prep step 5.6,
   per-mod `/data/modbridge/writable/<mod>/` via curl `--ftp-create-dirs`
   + LIST/readback verify; idempotent; never aborts on spill-covered
   failure; parsed the live 28-row loadlist correctly (6 mods, shared
   excluded); --dry-run zero-network.
2. Nothing at module_start: install() = zero fs work (kept: only the
   checked, crash-log-proven loadlist fopen).
3. Lazy first-write creation: `ensure_parent_dirs_once` — one shot per
   parent, checked error_code, budgeted `[SHIM] write-area dir …` marker;
   write order = physical fopen → lazy create + 1 retry → spill.
4. PC semantics everywhere: liolib-checked backend, nil+msg on every
   shim-side failure, spill returns a working handle (nil+err only at the
   spill cap). No crash path.
5. Crash-window hardening: context upvalues sourced from the REGISTRY
   anchor (stale-index class eliminated), absolute indices only, balanced
   stack (one-slot leak fixed), setglobal("io") last-op-of-section.

Public surface unchanged (hpp untouched, LuaMod.cpp needs no edit).
Syntax receipt: round5-staging/shim-fix/syntax-receipt-LuaPathShim-shimfix.log
(luamod unit argv from EY-TARGET-COMPILE.json, LLVM_CONFIG per IP recipe).
Apply order + marker notes: round5-staging/shim-fix/APPLY-NOTES.md.
