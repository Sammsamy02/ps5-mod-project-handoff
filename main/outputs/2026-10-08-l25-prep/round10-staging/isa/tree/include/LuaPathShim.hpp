#pragma once

// LuaPathShim — io/require logical-path resolver for the PS5 UE4SS port.
// Agent GA, Claude round-5 item 4 (2026-10-08). New TU; battery-safe (no
// builds this round — syntax-check only).
//
// Problem being solved (API-PARITY-TABLE.md section 4/5): io IS linked
// (patched fopen-backed liolib, luaL_openlibs at LuaMod.cpp:7840) but mods
// derive Windows/logical paths from debug.getinfo — e.g. TownfallMiniMap
// main.lua:241-245 builds "ue4ss/Mods/<Mod>\\settings.txt" — which fopen
// cannot resolve. The patched liolib has NO path hook (io_open calls
// fopen directly), so this shim REPLACES the io library table right after
// openlibs: io.open/io.lines wrapped as resolve-then-call-original, and
// os.remove/os.rename get the same treatment; os.execute/io.popen/io.tmpfile
// become error-returning stubs (no passthrough, per the section 5 plan).
//
// Resolver rules (mirrored 1:1 in host-lua-harness/path-shim.lua and tested
// by host-tests/lua-path-shim-test.lua):
//   (a) virtual PC working dir is Binaries/Win64-shaped: a leading
//       "Binaries/Win64/" (case-insensitive, backslashes already
//       normalized) is dropped, plus ONE following "../" — so both
//       "Mods/<name>/..." and "ue4ss/Mods/<name>/..." land on the physical
//       Mods directory;
//   (b) backslashes become slashes;
//   (c) PC-isms are rooted at the ue4ss root: a drive-letter prefix
//       ("C:...") is dropped, a leading "/" is dropped, and any embedded
//       "ue4ss/" component restarts the key after it (full PC absolute
//       paths like "C:/Game/ue4ss/Mods/..." therefore work);
//   (d) case-insensitive fallback: on exact-physical miss, a bounded
//       directory walk (depth <= 6, first match, 2048 entries/dir) matching
//       case-insensitively, cached in a small map (128 entries, clear on
//       full). Write modes case-walk only the parent, keeping the final
//       component's literal casing for newly created files;
//   (e) debug.getinfo-derived "@..." chunk names normalize through the same
//       rules (round-trip law: normalize(source_folder .. rel) then
//       io.open must hit).
//
// Write confinement + WRITABLE AREA (agent IB, Claude round-8 item 5,
// 2026-10-11): io.open in w/a/+ modes and os.remove/os.rename accept only
// paths INSIDE the CALLING mod's folder (found by the debug.getinfo stack
// walk; install-time mod as fallback) — and now LAND in a per-mod writable
// sandbox OUTSIDE admission: <writable-root>/<modname>/<path relative to the
// mod folder>, path-shape preserving. The writable root is derived from the
// modbridge root by stripping the trailing "/roots/<title>" segment and
// appending "/writable" (/data/modbridge/roots/PPSA33286 ->
// /data/modbridge/writable; a root without that shape gets "<root>-writable"
// so host fixtures never sit inside an audited tree). WHY a sandbox at all:
// admission is not merely "unlisted files are not served" — after
// module_start the game process cannot freshly open ANY /data path (fresh
// opens fail ENOENT; modforge_activation.h:6-8 contract, and the round-4
// session proved it: raw-liolib io.open of
// /data/modbridge/roots/.../ueselftest/progress.txt for write failed at
// 2026-10-11 08:47:52, ~3 s after module_start). modforge installs NO I/O
// interception — the ENOENT is the OS-level reachability of /data from the
// game process, not a modforge virtual layer inside roots/. Therefore the
// sandbox alone cannot be presumed OS-writable at runtime either (no console
// experiment distinguishes ANY /data subpath); the shim attempts the sandbox
// physical open FIRST (if the console permits it, writes persist and are
// ftpsrv-visible) and falls back to a per-mod IN-MEMORY SPILL (context-owned
// map keyed by the sandbox path, 8 MiB/file + 16 MiB total caps, flushed on
// every write) so settings saves, logs and new files always work within the
// process regardless. os.remove/os.rename operate on the SANDBOX mirror only
// (the mod root — admitted set included — is never mutated); spill entries
// are erased/moved alongside the physical attempt. Reads resolve in order:
// mod-root physical (case-resolved) -> writable-sandbox physical -> in-memory
// spill; the loadlist-admitted set stays read-only (pinned refusal below
// still fires before any sandbox mapping).
//
// Size-exact admission safety (agent-I MFA audit): MFA pins ONLY the
// loadlist rows (UE4SS-settings.ini, MemberVariableLayout.ini,
// VTableLayout.ini, PS5Pad.ini, each mod's Scripts/main.lua + enabled.txt,
// Mods/shared/UEHelpers/UEHelpers.lua) and admission is size-exact-only.
// The shim refuses writes to the pinned set itself (loadlist.txt rows when
// parseable, plus built-in defaults), so pinned files stay byte-identical.
// NOTHING in the writable area needs admission: it sits OUTSIDE the
// admitted root tree (MFA only ever reads <root>/loadlist.txt and the files
// it lists), so no loadlist row, no re-pin and no prereq exists for
// <writable-root>/<modname>/... — that is the point of the sandbox.
//
// require/package.path seam (minimal, documented): native mode already
// replaced package.searchers with [preload, native buffer searcher]
// (LuaMod.cpp:1376-1389) whose module names resolve as NativeModSource
// logical keys — require itself never touches package.path there. The shim
// therefore only SETS package.path to logical PC-style entries so Lua code
// that reads/derives from it sees consistent values; no searcher is added.

#include <string>

struct lua_State;

namespace RC
{
    // Single-entry registration used by LuaMod::prepare_mod. Declared as a
    // plain namespace-scope function so prepare_mod can reach it with ONE
    // tiny block-scope extern declaration inside its single insertion —
    // LuaMod.cpp is otherwise untouchable this round (FY split plan cites
    // line numbers).
    auto LuaPathShim_install(lua_State* L, const char* mod_name) -> void;
}

namespace RC::LuaPathShim
{
    // Physical root facts. NO title-id default exists (Rule 10 / games,
    // release-audit finding): the installer derives the root at runtime from
    // the program's mods directory; tests override via set_modbridge_root_override.

    // Installs the resolver for one mod Lua state. Must run AFTER
    // luaL_openlibs (the shim replaces the io table). Idempotent per state
    // (registry sentinel). mod_name is the owning Lua mod used as the
    // write-confinement fallback when no mod frame is found on the stack.
    auto install(lua_State* L, const char* mod_name, const char* modbridge_root) -> void;

    // Test/seam hook: overrides the physical modbridge root for the next
    // install() on any state (empty string restores the default). Host
    // harnesses use this to point the rules at a fixture tree.
    auto set_modbridge_root_override(std::string root) -> void;

    // Test/seam hook: overrides the writable-area root for the next
    // install() (empty string restores derivation from the modbridge root).
    auto set_writable_root_override(std::string root) -> void;
} // namespace RC::LuaPathShim
