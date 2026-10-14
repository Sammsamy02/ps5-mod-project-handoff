# Agent GA — io/path shim (Claude round-5 item 4)

2026-10-08. Battery-safe throughout: NO builds, NO console contact, NO
`--execute`. Two single `-fsyntax-only` checks (one per touched C++ TU),
pure-Lua host runs only. `CHANGES.json` untouched (root registers the new
unit + syncs).

## Deliverables

| File | What |
|---|---|
| `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaPathShim.cpp` | NEW TU — resolver + io/os table replacement (839 lines, whole TU `__PROSPERO__`-guarded; host build registers a no-op) |
| `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include/LuaPathShim.hpp` | NEW header — rule prose + `RC::LuaPathShim_install(lua_State*, const char*)` entry |
| `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` | ONE insertion: `#if defined(__PROSPERO__)` block at new lines **7842-7855** in `prepare_mod`, after `setup_lua_require_paths(lua);` (old :7841). Block-scope `extern` declaration keeps it a single contiguous edit — no `#include` edit elsewhere in the file. **Line-shift note for FY's split plan: everything from old :7841 shifts +14.** Nothing else in LuaMod.cpp touched. |
| `outputs/2026-10-08-l25-prep/host-lua-harness/path-shim.lua` | NEW host twin of the rule table (single source of truth on host; 5.2-5.5 subset) |
| `outputs/2026-10-08-l25-prep/host-tests/lua-path-shim-test.lua` + `run-path-shim.sh` | NEW pure-Lua rule-contract test (mktemp fixture tree incl. real loadlist.txt shape) — **38/38 PASS** |
| `outputs/2026-10-08-l25-prep/host-tests/run-all.sh` | NEW orchestrator (no host-tests/run-all existed; runs prop-parity + path-shim) |
| `outputs/2026-10-08-l25-prep/host-lua-harness/stub-ue4ss.lua` | `--with-io` mode now routes `env.io` through the resolver rules (real reads on the audited tree via physical paths, in-memory virtual writes so mod trees stay pristine, refusal messages like the port). Header comment updated. Legacy always-succeed stub kept only as load-failure fallback. |

## Design facts checked before coding

- **Patched liolib has NO hook**: `ue4ss-stdio-consumer-adaptation-1/patched/liolib.c:270-277` — `io_open` calls `fopen(filename, mode)` directly (diff vs vanilla is only the 4 Sony-macro swaps). Therefore the shim REPLACES the io table post-openlibs (copies every field — stdout/stderr/stdin passthrough — then wraps `open`/`lines`, stubs `popen`/`tmpfile`), and wraps `os.remove`/`os.rename`, stubs `os.execute`. Exactly FS's registration shape (§5 of API-PARITY-TABLE): registration AFTER `luaL_openlibs` (:7840) and after `setup_lua_require_paths` — the latter because native mode BLANKS `package.path` there (LuaMod.cpp:1589-1592), which would wipe the shim's logical entries if installed before.
- **Native chunknames are the path source**: mod scripts load as `@ue4ss/Mods/<Mod>/Scripts/main.lua` (logical keys; LuaMod.cpp:1009, native_lua_loadfile :989-1026). MiniMap's `settingsPath()` (main.lua:241-245) therefore produces `ue4ss/Mods/TownfallMiniMap\settings.txt` — backslash + logical key. That is the canonical input shape.

## Resolver rules (identical in C++ TU and path-shim.lua)

Input -> modroot-relative key (always `ue4ss/...`) -> physical `<root>/key`
where root = `/data/modbridge/roots/PPSA33286` (the `MODBRIDGE_MOD_ROOT`
fact hardwired in runtime_module_entry.c:74; overridable via
`LuaPathShim::set_modbridge_root_override` for tests/future configured
roots):

1. control-char/NUL/empty/>4096 rejected;
2. strip leading `@` (chunk-name leak), `\` -> `/` (MiniMap's `\\settings.txt`);
3. drive-letter `C:` dropped; leading `/` dropped (PC-isms root at ue4ss root);
4. leading `Binaries/Win64/` dropped plus ONE following `../` (virtual PC cwd leak) — so both `Mods/<name>/...` and `ue4ss/Mods/<name>/...` land on the physical Mods dir;
5. `..`/`.` components rejected outright — post-normalization the key always starts `ue4ss/` (confinement is enforced in KEY space; MFA's O_NOFOLLOW stance is the precedent, no symlink traversal accepted);
6. anchor: after any embedded `ue4ss` component (full PC absolutes like `C:/Game/ue4ss/Mods/...` work), else at a leading `Mods`, else bare-relative anchored at the CALLING mod's folder;
7. case-insensitive fallback on read miss: bounded walk depth<=6, first match, 2048 entries/dir, cache 128 (clear on full). Write modes case-walk only the parent and keep the final component's literal casing (new files keep the mod's spelling);
8. round-trip law: `normalize(debug_source_folder .. rel)` then `io.open` hits — proven by the host test loading a chunk named `@ue4ss/Mods/TownfallMiniMap/Scripts/main.lua` running MiniMap's verbatim `settingsPath()` code and reading the seeded physical file.

**Calling-mod detection** (write confinement + bare-relative anchor):
debug.getinfo stack walk (levels 1..16), innermost frame whose source
SELF-ANCHORS under `ue4ss/Mods/<name>/` with `<name>` not ""/`shared`;
fallback = the install-time owning mod (passed by LuaMod). Only
self-anchored sources count — a bare-relative echo of the fallback would be
a meaningless boundary (this also makes the walk skip the shim/harness
frames on the host twin). Known boundary: a Lua **tail call** into
`io.open` erases the caller's frame (Lua 5.4+), so detection falls back to
the owning mod — still confined to a mod folder, just possibly the owner
instead of a nested script's mod.

## Write confinement + the REAL admission rule (documented, agent-I audit)

- `io.open` in w/a/+ modes, `os.remove`, `os.rename` (both args): resolved
  key must be inside `ue4ss/Mods/<calling mod>/` else
  `nil, "path outside mod root: <path>"` (liolib failure contract; no
  lua_error longjmp across C++ frames — wrappers never throw).
- Console save-data and the ue4ss root are read-only to mods; `Mods/shared`
  is NOT a mod boundary (UEHelpers writes refused).
- **The actual pinned set (verified, not assumed)**: MFA admission is
  size-exact-only over the loadlist rows — `ue4ss/{UE4SS-settings.ini,
  MemberVariableLayout.ini, VTableLayout.ini, PS5Pad.ini}`,
  `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua`,
  `.../enabled.txt` (0 bytes), `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua`
  (modforge_activation.c `read_exact` :163-208; agent-I-mfa-audit.md §1/§5).
  **Mod config files under Mods/<mod>/ are NOT loadlist-admitted** — a mod
  rewriting its own `settings.txt` cannot trip next-launch admission. The
  shim nevertheless refuses writes to the pinned set itself (union of
  parseable `<root>/loadlist.txt` `file` rows + built-in defaults incl. the
  calling mod's `enabled.txt`/`Scripts/main.lua` and root INIs), so even
  admitted files stay byte-identical. No temp+rename discipline is needed
  because non-pinned files are never admission-checked — that is the real
  rule; write modes also do NOT create parent directories (settings files
  land at the mod root, which always exists).

## require/package.path seam (minimal, documented)

Native mode already replaced `package.searchers` with
[preload, native buffer searcher] (LuaMod.cpp:1376-1389) whose module names
resolve as NativeModSource logical keys (`native_lua_module` :1073-1119 —
dots->slashes, backslash/colon names rejected; `require("UEHelpers")`
proven on console). require never consults `package.path` there. The shim
therefore only SETS `package.path` to logical PC-style entries
(`ue4ss/Mods/<mod>/Scripts/?.lua;ue4ss/Mods/<mod>/?.lua;ue4ss/Mods/shared/?.lua;ue4ss/Mods/shared/?/init.lua;ue4ss/Mods/shared/?/?.lua`)
so Lua code that reads or derives from it sees consistent values;
`package.cpath` stays blank. No searcher hook added — smallest safe seam.

Dual-read via the NativeModSource snapshot was NOT implemented and is NOT
needed: MFA physically materializes the admitted files under the root
(`read_exact` opens them from `<root>/...`), so fopen on the resolved
physical path is the real route. Documented in the header.

## Markers (FW law: contiguous literals, budgets)

- one per install: `"[SHIM] path-resolver installed mod={} root={}\n"`
- first 10: `"[SHIM] io-open resolved={}->{}\n"`
- first 10 refusals: `"[SHIM] write-rejected path={} mod={}\n"`

(LuaPathShim.cpp:807/:451/:395.)

## Host verification

1. **Rule contract tests — 38/38 PASS**
   (`host-tests/run-path-shim.sh`, throwaway mktemp fixture incl. real
   loadlist.txt + extra pinned row): virtual-cwd anchoring ("Mods/X" ==
   "ue4ss/Mods/X" == bare-relative), backslashes, '@', drive-letter,
   leading-slash, Binaries/Win64 leak, traversal/control/empty rejections,
   case fallback (EXISTING.cfg, Scripts/main.lua), the full write
   accept/reject matrix (own-folder accept; other-mod/shared/root/pinned
   rejects with the right reason), pinned-set facts, MiniMap-verbatim
   getinfo round-trip (read seeded file + virtual write + tree-untouched),
   logical stack-walk mod detection, require entry expansion hitting
   shared/UEHelpers.lua.
2. **13-subject matrix — verdicts IDENTICAL to FU's baseline**
   (run-all.sh rerun with the resolver-backed stub): all previous
   LOADS-CLEAN subjects stay LOADS-CLEAN in io-on; MiniMap io-off still
   FAILS-AT :251; BPModLoader :117 both modes; jsbLuaProfiler io-off :115,
   io-on LOADS-CLEAN (jsb.write tolerates nil handles — `if not File then
   return end` jsbProfi.lua:71-74).
3. **MiniMap io-on LOADS-CLEAN through REAL path shapes** — telemetry:
   `io.open(…/minimap-v3.1/ue4ss/Mods/TownfallMiniMap\settings.txt,r)` ->
   resolver normalized the backslash, mapped onto the physical tree
   (`…/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/settings.txt`), read missed
   non-fatally exactly like a fresh install on the port; startup clean.

## Syntax checks (single -fsyntax-only per TU, Prospero toolchain)

Flags cloned from receipts (llvm@18 wrapper needs
`PATH=/opt/homebrew/Cellar/llvm@18/18.1.8/bin` — `prospero-llvm-config`
finds no system llvm-config otherwise; target x86_64-sie-ps5, so
`__PROSPERO__` is defined).

- **LuaPathShim.cpp: exit 0, zero diagnostics** — flags from
  EY-TARGET-COMPILE.json argv minus `-c/-o pair/-MD/-MF pair` (watch: an
  orphaned `-o` silently swallows `-fsyntax-only` as a filename and turns
  the run into a LINK — that is not a build attempt, it was a flag-stitching
  bug corrected in the same check series). One real fix found by the check:
  `case_resolve` takes `ShimContext&` (mutates cache/mutex), not const.
- **LuaMod.cpp (with insertion): exit 0, zero errors** — EY flags + thread
  include (`2026-10-08-r5-thread-source-integration/thread`) + lineage
  include (`2026-10-08-l26-package-B/lineage`) + `-Wno-invalid-constexpr`
  (the vendored clang-x86-headers xmmintrin noise is pre-existing and
  unrelated; the l25 luamod unit receipt lacks the r4 candidate-include
  routing, so EY's r4 receipt is the right flag base).

## Host-twin divergences (documented in path-shim.lua header)

- physical root is DETECTED from the first stack source containing an
  `ue4ss` component (the C++ TU pins the modbridge constant);
- directory listing via `ls` popen (C++ walks directory_iterator);
- writes go to an in-memory store (harness never touches audited trees);
- case-existence: the C++ twin short-circuits on
  `std::filesystem::exists` (its semantics always match that platform's
  fopen); the host twin always runs the bounded walk because macOS
  fopen/rename are case-insensitive and would short-circuit the
  case-fallback before it runs (rule identical, probe differs).

## Known boundaries / future notes

- Lua tail-call frame erasure in calling-mod detection (above) — fallback
  is the owning mod.
- `io.input`/`io.output` redirection and `r+`/`w+` update modes pass
  through to the original (routed through the resolver like reads; no
  ladder mod uses them — BPModLoader is SKIP, jsbProfiler tolerates nil).
- Parent directories are not created for writes (by design).
- Registration is per mod state (registry-sentinel idempotent); main/async/
  hook "states" are lua_newthreads sharing globals — one install covers
  them (parity-table fact). The console-executor state (:8277) is NOT
  registered this round — one insertion only; if console io ever needs the
  resolver, add the same call after its `open_all_libs`.
- Root registers `LuaPathShim.cpp` in the unit list + CHANGES; nothing here
  edits CHANGES.json.
