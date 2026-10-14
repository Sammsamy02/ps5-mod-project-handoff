# IP — SHIM ROOT FIX (Claude round-10 item 3): wire the path shim's root to the real modbridge root

2026-10-11. Battery-safe: analysis + staged patch + ONE prospero `-fsyntax-only`
receipt (GA/IB/IA precedent). No builds, no console contact, ZERO live-tree
edits (candidate file sha verified unchanged at close).

Deliverables (this dir):
- `01-ue4ssprogram-shim-root.patch` — the one-branch change (candidate-relative paths)
- `syntax-receipt-UE4SSProgram-shim-root.log` — prospero-clang++ -fsyntax-only EXIT=0, 0 errors
- `syntax-argv.txt` — the exact reconstructed argv (program unit, l24-build-r2 receipt base)
- `APPLY-NOTES.md` — apply order + integration with the round-5 staging scripts

## 1. THE BUG (why the shim self-disables)

`UE4SSProgram::setup_mod_directory_path()` (candidate
`src/UE4SSProgram.cpp:929-936`) pins the native mods directory as the
RELATIVE literal `"ue4ss/Mods"`. The shim root derivation in
`LuaMod::prepare_mod` (`LuaMod.cpp:7973-7978`, the [SHIM] block) is lexical:

```
path("ue4ss/Mods").lexically_normal()   -> "ue4ss/Mods"
filename() == "Mods" -> parent_path()   -> "ue4ss"
parent_path() again                    -> ""   <-- EMPTY
```

`LuaPathShim_install(L, mod, "")` then hits the no-root guard
(`LuaPathShim.cpp` install, ~:1523): "no title-id default exists" →
self-disable. Launch-2 evidence: `evidence/launch2-final-session-20261011.decoded.log`
lines 782/812/818/823/833/836/845 — seven
`[SHIM] path-resolver disabled reason=no-root` lines. IB's writable-area
sandbox (`agent-IB-writable.md` §2) is therefore completely untested: it
derives `writable_root` FROM the modbridge root (`.../roots/<title>` →
`.../writable`), which never arrives.

## 2. WHERE THE ABSOLUTE ROOT ACTUALLY IS (candidate audit, a-e)

The absolute root `/data/modbridge/roots/PPSA33286` is available at runtime,
already carried inside UE4SS — through the existing module_start pipe:

```
runtime_module_entry.c (module_start)
  #define MODBRIDGE_MOD_ROOT "/data/modbridge/roots/PPSA33286"   (:74, composition fact)
  config.root = MODBRIDGE_MOD_ROOT                                (:280, mfa_activate)
  modbridge_runtime_start_with_activation(MODBRIDGE_MODULE_PATH, MODBRIDGE_MOD_ROOT, ...)  (:315-317)
      |
      v
runtime_start_bridge.cpp  modbridge_runtime_start_with_activation
  converted.facts.configured_working_directory = std::filesystem::path(mod_root);   (:1907, "Resolution B")
  ALSO RuntimeAckConfig::status_root = mod_root (ack sink copy, StartActivatedRuntimeWithAck)
      |
      v
Platform::NativeProgramContext.configured_working_directory          (NativeProgramContext.hpp:78)
  -- public field, lexical-validated (absolute/normalized) when non-empty --
      |
      v
UE4SSProgram::m_native_context   (private member, process lifetime; ctor :216)
  ...and NEVER CONSUMED by setup_mod_directory_path today  <-- the gap
```

Task candidates evaluated:
- **(a) modforge activation path — CHOSEN.** The entry knows the root and the
  bridge ALREADY copies it into `m_native_context.configured_working_directory`.
  Note: `mfa_activation` itself does NOT retain the root (struct has
  title/engine/sdk/frameworks/mods/files only — `modforge_activation.c:24-37`),
  so the context field, not the activation, is the accessible copy.
- (b) PRX module path → module parent = `/data/plugins` (= m_root_directory /
  log dir). Not the modbridge root. Rejected.
- (c) env var / settings — no setting carries the absolute root; native branch
  ignores `Overrides.ModsFolderPath` (early return before it). Rejected.
- (d) derive from loadlist.txt location — circular (mfa_activate RECEIVES the
  root as input), and /data fresh-opens fail ENOENT after module_start, so
  mod-start-time re-read is impossible. Rejected (only useful as IB's
  pinned-set enrichment, which already degrades gracefully on fopen failure).
- (e) UE4SS-settings.ini root-dir setting — same as (c). Rejected.

## 3. THE WIRING (task option d, realized through the existing pipe)

**One branch edit in `UE4SSProgram::setup_mod_directory_path()`**: the native
mods directory becomes
`m_native_context.configured_working_directory / "ue4ss" / "Mods"`
(empty configured root → legacy relative `"ue4ss/Mods"` fallback, so an older
composition lineage keeps today's loud no-root disable instead of guessing).

**Zero edits in LuaMod.cpp / LuaPathShim.cpp.** The existing derivation was
designed for the absolute shape ("<root>/ue4ss[/Mods]" — its own comment,
LuaMod.cpp:7954-7955): `…/roots/<title>/ue4ss/Mods` → strip `Mods` → strip
`ue4ss` → `…/roots/<title>`. The shim then computes
`ue4ss_root = <root>/ue4ss` and `writable_root = /data/modbridge/writable`
exactly per IB §2.

### Consumer audit (why the relative→absolute change is safe on target)

Every native-mode consumer of the mods directory either early-returns before
touching it, is compiled out, or is exists()-guarded:

| Consumer | File:line | Verdict |
|---|---|---|
| Shim root derivation (the fix target) | LuaMod.cpp:7973 | wants absolute — this IS the fix |
| `custom_module_searcher` physical tail | LuaMod.cpp:1469 | unreachable in native mode (buffer branch returns at ~:1445) |
| `setup_lua_require_paths` mods_dir use | LuaMod.cpp:1601 | unreachable in native mode (blank path early-return at ~:1590) |
| `install_mods` directory walk | UE4SSProgram.cpp:1962 | native early-return at :1946 (mods come from mod_source logical keys) |
| `get_mods_txt_entries` physical branch | UE4SSProgram.cpp:2190 area | native early-return at :2819-2830 |
| `start_mods` part #2 enabled.txt walk | UE4SSProgram.cpp:2278 | `exists()`-guarded; /data ENOENT post-module_start → skip (identical skip as today) |
| filesystem watcher add_dir | UE4SSProgram.cpp:1699/1703 | `#ifndef __PROSPERO__` — dead on target |
| boot log print | UE4SSProgram.cpp:472-474 | diagnostic only; now MORE informative (prints the absolute dir) |

Mod discovery never used the string's VALUE in native mode — `mod_source`
serves logical `ue4ss/…` keys; only path SHAPE consumers matter, and they are
the rows above.

### Rule 10 compliance

- The derivation contains **no title-id**: it reads whatever root the
  composition activated with, verbatim, at runtime. Multi-game ports change
  only the entry's per-title composition constants (`MODBRIDGE_TITLE_ID` /
  `MODBRIDGE_MOD_ROOT`, which "must equal package-profile.json" — the entry
  must know the title anyway for `MFA_E_TITLE` validation).
- Pure lexical `operator/` join — no `weakly_canonical`/`canonical`/
  `current_path`/`exists` (the launch-2 lesson: those THROW or touch the OS on
  this target).
- `ValidateNativeProgramContext` already guarantees the field is empty or an
  absolute normalized path (`NativeProgramContext.hpp:120-122` lexical gate),
  so the join is well-formed whenever taken.

## 4. PATCH + VERIFICATION

- `01-ue4ssprogram-shim-root.patch`: 1 file, one hunk @ :929 (+18/-1). Proven:
  `patch -p1 --dry-run` clean, apply clean, result byte-identical to the
  staged edit on a fresh /tmp copy. Candidate-relative `a/src/…` paths
  (world-fix patch convention).
- Syntax: prospero-clang++ `-fsyntax-only` on the patched TU — **EXIT=0,
  0 errors, 7 warnings** (pre-existing base-tree classes: `-Wswitch` in
  port/unreal Atomic.hpp, linker-input note — same families as the IA/IK
  receipts). argv = the REAL `program` unit argv from
  `l24-build-r2/pass1/COMPOSITION-RECEIPT.json` (source swapped to the staged
  copy, l26 candidate `-I` dirs prepended, l26-A `headers.vfs.json` overlay)
  — see `syntax-argv.txt`.
- Baseline honesty: the UNPATCHED candidate UE4SSProgram.cpp is the same TU
  the verified l26 builds compiled; the receipt isolates MY 19-line change.

## 5. WRITABLE-AREA TEST PLAN (what the next sanctioned launch proves)

Preconditions: patch applied to the l26 candidate (see APPLY-NOTES), full
build → fake-sign (`make_fself.py`) → decrypt-OFF verified upload → single
launch per the mission ladder. Optional (harmless, IB §3): ops pre-creates
`/data/modbridge/writable/` via ftpsrv.

Expected UE4SS.log transitions (decoded):

1. **Root wired:** the seven `reason=no-root` disables are replaced by
   `[SHIM] path-resolver installed mod=<name> root=/data/modbridge/roots/PPSA33286`
   per Lua state (existing marker, already in the manifest set).
2. **Write area live:** NEW `[SHIM] write-area <mod> -> /data/modbridge/writable/<mod>`
   per mod (first-10 budget; IB's optional 4th marker — II's
   markers-add-round5.json already carries it).
3. **The IB probe:** ueselftest/ST2 writes `ue4ss/Mods/ueselftest/progress.txt`
   through `io.open` → resolves into the writable sandbox. `io-open resolved=…/writable/…`
   + backend SUCCESS = the console permits physical /data writes (cross-launch
   resume becomes possible); backend FAILURE + successful readback = the
   in-memory spill is serving (within-process saves/logs work either way).
4. **Physical cross-check:** post-session ftpsrv `ls /data/modbridge/writable/`
   non-empty ⟺ physical writes landed.
5. **Regression guards (must NOT change):** all mods still load (admission is
   untouched — no loadlist row ever names a writable file, IB §3), the
   CE-108255-1 save-load behavior matches the launch-2 baseline, the
   remaining [ST2]/phase markers are otherwise identical.

Failure triage:
- `reason=no-root` persists → `configured_working_directory` was empty at
  runtime: the packaged lineage predates bridge Resolution B — verify
  `runtime_start_bridge.cpp:1907` is in the packaged .prx's lineage.
- `root=` prints relative `ue4ss…` → the empty-fallback branch ran (same cause).
- Installed but writes misbehave → that is IB's domain (spill/sandbox), not
  the root wiring; root wiring is proven by markers 1-2 alone.

## 6. Honest residuals

- No full build was run (battery law); the single-file syntax receipt is the
  compile evidence. The l26 packaging driver must re-hash the changed unit
  (CHANGES row refresh) at the next compose — APPLY-NOTES documents this.
- The entry's per-title constants remain build-time facts by design; a
  runtime roots-scan at module_start would need readdir in the frozen import
  contract (out of scope, and the composition model already regenerates the
  entry per game).
- If a future composition ever supplies BOTH a game executable path AND a
  garbage configured_working_directory, validation only checks the workdir
  when the exe path is empty; my empty-check guards shape but a non-absolute
  garbage value would flow through. Not reachable in any current lineage
  (Resolution B sets it unconditionally from the entry constant).
