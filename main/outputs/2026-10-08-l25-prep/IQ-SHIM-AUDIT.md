# IQ — SHIM WRITE-AREA AUDIT (Claude round-12 items 1-2)

2026-10-12. Battery-safe: read-only analysis + staged patches + ONE prospero
`-fsyntax-only` receipt. No builds, no console contact, no live-tree edits.

Audited TU: `round4-setaside/LuaPathShim.cpp` sha
`821d74a261a177ca483a518607c1cb91fc56974a86c697ff9e4e069770f3aeb3` (1656 ln)
— **byte-identical to the deployed crashed build's source** (verified against
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaPathShim.cpp`,
same sha). Line numbers below refer to THIS file (the pre-fix code).

Deliverables: `round5-staging/shim-fix/` (patched TU + candidate-relative
patch + deploy pre-creation script + apply notes + syntax receipt).

---

## 1. THE AUDIT — every filesystem call from install() through the first write

### 1.1 The exact code path (who runs when)

`LuaMod::start_mod` (candidate `src/LuaMod.cpp:8128-8138`):
`prepare_mod(lua())` → … → `start_async_thread()` → `load_and_execute_script`.
`prepare_mod` calls the shim block at `LuaMod.cpp:7945-7987` — pure LEXICAL
root derivation (`lexically_normal`, no syscall; the throwing
`weakly_canonical` was already removed after launch-2's LOGGED
"Exception during mod preparation: filesystem error: in weakly_canonical" —
that failure class produces an ERROR LINE, not a silent death, so it is not
our crash) → `LuaPathShim_install(L, mod, root)` → `install()` runs to
completion BEFORE the mod's main.lua ever executes. The state is the mod's
PRIVATE `m_lua` (`LuaMod.cpp:1126`, `LuaMadeSimple::new_state()`); the async
worker thread starts 4 statements LATER (`:8133`) — no concurrent thread can
touch the state during install.

### 1.2 Filesystem calls in the path, one by one

| # | Call | Site (pre-fix) | Returns on failure | Checked? | Consequence if it fails | Crash candidate? |
|---|------|--------|--------------------|----------|------------------------|------------------|
| 1 | `std::fopen(<root>/loadlist.txt, "rb")` (load_pinned_set) | :1417 | `NULL` | **YES** — `if (std::FILE* file = …)` guards the whole use; `fread`/`fclose` only inside | pinned set degrades to built-in defaults | **NO** — and it runs BEFORE the write-area marker, so the crash logs (both of which PRINTED the marker) prove this call already survived on console |
| 2 | `std::filesystem::exists(path, ec)` (case_resolve) | :291 | `false` + `ec` set | bool result used; `ec` ignored — but `false` just falls through to the walk | walk proceeds / backend fopen decides | NO |
| 3 | `std::filesystem::is_directory(path, ec)` (case_resolve) | :322 | `false` + `ec` | **YES** — `if (!is_directory) return physical;` | exact path stands, backend decides | NO |
| 4 | `std::filesystem::directory_iterator{dir, iter_ec}` + `increment(iter_ec)` | :331-332 | end-iterator + `iter_ec` | **YES** — `it != end && !iter_ec` in the loop condition | miss stays as-is | NO |
| 5 | `std::filesystem::create_directories(parent, ec)` (ensure_parent_dirs) | :583 | `false` + `ec` | **NO** — both return and `ec` ignored (by-design "best-effort") | nothing used afterward — the following fopen simply fails and liolib handles it | NO (cannot throw; error_code overload; nothing consumes the result — an ignored result cannot be MISUSED) |
| 6 | backend `fopen(filename, mode)` in the PATCHED liolib `io_open` | patched `liolib.c:277-278` | `NULL` | **YES** — `(p->f == NULL) ? luaL_fileresult(L, 0, filename) : 1` → pushes nil+msg+errno, never errors | our `call_original` gets LUA_OK + [nil,…] → `try_original` pops → 0 | **NO** — this is the definitive answer to "can the shim's fopen crash": the only fopen in the write path is the liolib one and its NULL is checked at the call site (Claude's correction confirmed in source) |

Everything else between install() entry and the first write is pure string /
Lua C API work (`normalize_logical`, `detect_calling_mod`, `sandbox_of_key`,
table copies, closure registrations).

### 1.3 THE CRASH WINDOW — and why the fs theories die in it

The log pipeline is SYNCHRONOUS: `Output::send` → the single default
FileDevice (UE4SS.log) → `PosixFile::write_string_to_file` → `write_all`
(looped `::write`) on the retained `O_WRONLY|O_APPEND` fd opened at
module_start (CE report chain, `agent-CE-logwriter.md` §1; CL's F1-F4 staging
kept the one-`::write`-per-message property). There is no userspace buffering
layer that could silently swallow a later, completed line.

Both crash logs therefore bound the death precisely:

```
[SHIM] write-area CheatManagerEnablerMod -> /data/modbridge/writable/CheatManagerEnablerMod   ← :1548
<LOG ENDS>                                                                                    :1624 never printed
```

The window **:1549–:1623 contains ZERO filesystem calls** (verified line by
line: metatable setup, registry anchor, io-table copy loop, `wrap_into` x2,
stub registrations, `setglobal`, os/package sections, one `Output::send`).
Consequences:

- ACCEPTANCE-PASS1 item 3's mechanism guesses ("fopen segfaults on a
  PS5-restricted path", "create_directories could fault") are **refuted for
  this crash**: neither call exists in the window, and `ensure_parent_dirs`
  is not even reached before the first write, which never happened (the
  crash precedes main.lua).
- Claude round-12's "the open/mkdir fails and a NULL/invalid handle or
  unchecked result is used afterward" is **refuted in its fs form** — there
  is no open/mkdir in the window — but **confirmed in its general form**:
  the audit found exactly one use-of-invalid-value defect in the window, of
  the predicted shape with a non-fs origin (§2).

---

## 2. THE ROOT CAUSE

### 2.1 The provable defect IN the crash window: a stale stack index used afterward

`LuaPathShim.cpp` pre-fix:

```cpp
:1551  const int context_index = lua_gettop(L);      // == 2  (stack: [io=1, ctx=2])
:1584      lua_setglobal(L, "io");                   // pops the NEW table
:1585      lua_pop(L, 1);                            // pops OLD io  → STACK SHIFTS: ctx is now index 1
:1589      lua_getglobal(L, "os");                   // stack: [ctx=1, os=2]
:1592      wrap_into(L, -1, context_index, "remove", os_remove_wrapper);
:1593      wrap_into(L, -1, context_index, "rename", os_rename_wrapper);
```

`context_index` was captured when the context userdata sat at absolute index
2. After `:1585` pops the old io table, **absolute index 2 is the OS TABLE**.
`wrap_into` binds `lua_pushvalue(L, context_index)` as upvalue(2) — so
`os.remove`/`os.rename` ship with **the os TABLE where the ShimContext
userdata belongs**. This is exactly "an invalid result captured and used
afterward" — the value is invalid because its index went stale between
capture (:1551) and use (:1592).

Failure behavior when a mod then calls `os.remove(...)`: `lua_touserdata` on
a TABLE returns NULL; the wrapper's `if (!context)` guard catches it →
graceful `nil + "io shim context is unavailable"`. **A real bug (os.remove /
os.rename permanently refused) but NOT a segfault.** Its significance: it
proves the install section was written under an absolute-index assumption
that the section itself violates — the defect class Claude predicted, in
window, minus the fs origin.

### 2.2 The crash-line verdict (by elimination, honestly stated)

What the two crash sessions pin down:

1. The death is between :1548 and :1624 — a window of pure Lua C API with
   balanced, audited usage (I re-verified every push/pop pair: the
   `lua_next` copy loop, both `wrap_into` calls, the os/package sections —
   all balanced; the one genuine defect is §2.1, which degrades safely).
2. The window's ONLY first-ever-executed-on-console, state-mutating code is
   the io-table replacement (`:1568-:1585`: new table, field copy, wrapper
   closures, `setglobal`). In the clean build 1e49188b the shim
   self-disabled at the no-root guard (`:1529-1534`) and THIS CODE NEVER
   RAN; the shim-root builds ran it for the first time — and died there,
   twice, at the same boundary, with zero error output (the `catch (...)`
   at :1626 catches exceptions, not signals).
3. install() also leaked one stack slot (the context userdata is never
   popped in the success path) — harmless by itself, fixed in the patch.

**Most likely crash site: the io-table replacement region, `:1568-:1585`
(the field-copy loop / `wrap_into` closure construction / `setglobal`).**
No single instruction in it can be PROVEN to be the faulting one from two
log excerpts; the section's one provable defect (§2.1) degrades safely, so
the kill mechanism most plausibly lives below this source (first execution
of the userdata+cclosure+global-swap pattern on the port's Lua runtime, or
an environmental reaction to swapping the io table mid-`prepare_mod`) —
which is exactly why the fix removes the entire risk surface instead of
betting on one line. A klog fault-address capture at the next sanctioned
launch (the fix makes that launch safe) closes this definitively.

What the root cause is NOT (each eliminated by direct source evidence):
- NOT `fopen` of loadlist.txt (:1417 — checked, and pre-marker);
- NOT `fopen` of the sandbox target (liolib :277-278 checks NULL — and the
  first write never ran);
- NOT `create_directories` (first-write only, error_code overload, nothing
  consumed its result);
- NOT `weakly_canonical` (removed pre-crash; its failure class LOGS an
  exception line, the crashes show none);
- NOT MFA/admission (§3 — it intercepts nothing and never runs at mod time).

---

## 3. THE MFA EXEMPTION — confirmed structurally, no code change needed

Source: `modforge_activation.c` (claude-takeover-43a96969 lineage, byte-identical
to the deployed `outputs/2026-10-08-l26-package-A/lineage/modforge_activation.h`).

- `mfa_activate` reads EXACTLY TWO kinds of paths, both built by
  `join_path(cfg->root, rel)` (:151-160): `<root>/loadlist.txt`
  (`read_loadlist` :211-224) and each listed row (`read_exact` :162-208).
  There is **no directory walk, no path-block list, and no interception of
  any kind** — MFA is a passive one-shot reader at module_start.
- The root is `/data/modbridge/roots/PPSA33286`
  (`runtime_module_entry.c` MODBRIDGE_MOD_ROOT). The writable area
  `/data/modbridge/writable/<mod>/…` is a **SIBLING of `roots/`** — no
  MFA code path can construct, list, or open it. **There is nothing to
  exempt: the "admission layer" never had jurisdiction outside the root.**
- Admission safety of pre-created dirs: no loadlist row will ever name a
  writable-area file (rows are deploy-authored for the roots tree only), so
  the 2026-10-11 `-6` all-or-nothing incident class (row names missing
  file) is structurally impossible for the writable tree. The step-5.5
  rowcheck only fetches declared rows under the root → unaffected.
- Corroborating contract facts: `mfa_status_open` (:599-609) CREATES
  `<root>/runtime-status.txt` at module_start with a retained fd — proving
  /data creates work ONLY inside the module_start window and only via
  retained descriptors. The writable area has no retained fd, so runtime
  physical writes there are expected to fail (the spill is the real
  backend) — which is precisely why DEPLOY-time pre-creation (a root-level
  ftpsrv process, not bound by the game-process contract) is the reliable
  way to give physical persistence a chance, per IB §1/§3.

---

## 4. THE FIX (staged in `round5-staging/shim-fix/`)

Claude's fix design, implemented verbatim:

1. **Deploy-time pre-creation** — `deploy-precreate-writable.py`: drop-in
   step 5.6 for `prep_modded_launch.py` (after the 5.5 rowcheck, before
   arming). Fetches loadlist.txt, parses the mod names from `file` rows
   (`ue4ss/Mods/<name>/…`, `shared` excluded — verified against the live
   28-row receipt: TownfallItemSpawner, zzdiag, ueselftest,
   CheatManagerEnablerMod, ConsoleEnablerMod, InfiniteStamina), then per
   mod: LIST → if missing, one marker upload with curl `--ftp-create-dirs`
   (the MKD/CWD/STOR core set; small-ftpsrv-safe) → LIST + byte-exact
   readback verify. Idempotent; NEVER aborts the launch on failure (the
   spill keeps mods functional — the failure is a FAIL ledger row + note);
   never touches anything under `roots/`; `--dry-run` = zero network.
2. **Exemption confirmed in MFA** — §3 above: no code change; the audit is
   the confirmation.
3. **Nothing at module_start / mod-prepare** — install() performs ZERO
   filesystem work (its only fs call remains the checked loadlist fopen
   that the crash logs already proved safe). No directory creation anywhere
   in install().
4. **Lazy creation on FIRST WRITE** — `ensure_parent_dirs_once`: one
   attempt per parent dir (mutex-guarded set), checked `error_code`
   overload, budgeted marker
   `[SHIM] write-area dir <path> created=<b> usable=<b>`; refusal is
   remembered like success (the spill serves that dir thereafter).
   `sandbox_target` is now PURE resolution (no create side effect). Write
   order in `io_open_wrapper`: physical fopen (pre-created dir) → lazy
   create + ONE retry → spill.
5. **Every failure → PC semantics, never a crash** — the backend fopen is
   NULL-checked by liolib itself (`luaL_fileresult` → nil+msg); every
   shim-side failure pushes `nil` + message (`push_refusal`,
   `push_memfile` cap, context guards). The spill returns a WORKING handle
   (liolib semantics — the mission needs in-process writes to function);
   `nil+err` surfaces only when even the spill cap is exceeded.
6. **The install-region hardening** (crash-window risk removal): all
   wrapper context upvalues now come from the REGISTRY anchor
   (`push_context`), so no stack shift can ever misbind them (kills the
   §2.1 defect class); the os-table section uses a fresh absolute index;
   `setglobal("io")` is the io-section's LAST operation (a throw before it
   leaves the STOCK io table in place); install() leaves the Lua stack
   exactly as it found it (the one-slot leak is fixed).

Public surface unchanged: `LuaPathShim.hpp` untouched, `LuaMod.cpp` needs
no edit, unit registration (`build_variants.py:124`) unchanged.

---

## 5. Syntax verification (one check, standard recipe)

`prospero-clang++ -fsyntax-only` on the staged TU — **EXIT=0, 0 errors,
0 warnings**. argv = the real `luamod` unit argv from
`typescan-r4/results/EY-TARGET-COMPILE.json` (source swapped to the staged
TU, `-I round4-setaside` for `LuaPathShim.hpp`, compile-output tail
stripped), `LLVM_CONFIG` per the IP recipe. Receipt:
`round5-staging/shim-fix/syntax-receipt-LuaPathShim-shimfix.log`.

Host-side checks run this session: `deploy-precreate-writable.py --dry-run`
(zero network, exit 0) and parser verification against the live 28-row
loadlist receipt (6 mods, `shared` excluded).

## 6. Honest boundaries

- No build, no console contact; the syntax receipt is the only compile
  evidence. The host Lua twin (`host-lua-harness/path-shim.lua`) was NOT
  updated this round — the resolver rules are unchanged; only write-path
  timing (lazy create) and install mechanics changed, which the twin does
  not model (its store already models the console "physical open fails"
  reality — IB §4).
- The §2.2 crash-site identification is by elimination; it is stated with
  its evidence and its limits. The patch is designed to be sufficient
  under EITHER candidate mechanism (fs-side or Lua-side), per Claude's
  directive.
- Cross-launch persistence remains unproven until a physical write lands
  (deploy pre-creation + the new markers make the next sanctioned launch
  the deciding experiment; post-session `ftpsrv ls /data/modbridge/writable/`
  is the physical cross-check).

— agent IQ (ZCode), 2026-10-12
