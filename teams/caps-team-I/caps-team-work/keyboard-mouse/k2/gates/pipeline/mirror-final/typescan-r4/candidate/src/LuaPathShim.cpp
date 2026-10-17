// LuaPathShim.cpp — io/require logical-path resolver (PS5 UE4SS port).
// Agent GA, Claude round-5 item 4, 2026-10-08.
// Agent IB revision, Claude round-8 item 5, 2026-10-11: per-mod WRITABLE
// AREA outside admission. Diagnosis that drove it (round-4 session,
// 2026-10-11 08:47:52): raw-liolib io.open of the unlisted
// /data/modbridge/roots/.../ueselftest/progress.txt for WRITE failed
// ~3 s after module_start — and modforge intercepts NO file I/O (its
// "virtual layer" only serves require/dofile from the in-memory
// activation, LuaMod.cpp native module searcher), so the ENOENT is the
// OS-level /data reachability of the game process after the module_start
// window (modforge_activation.h:6-8 contract), not an admission check.
// Writes therefore no longer target the mod root at all: they land in
// <writable-root>/<modname>/<mod-relative path> (writable root derived
// from the modbridge root: .../roots/<title> -> .../writable), physical
// attempt first, backed by a per-context in-memory spill when the OS
// refuses (8 MiB/file, 16 MiB total). Reads resolve mod-root physical ->
// sandbox physical -> spill. os.remove/os.rename are sandbox-confined.
// Nothing in the writable area needs a loadlist row: MFA only reads
// <root>/loadlist.txt and the files it lists (agent-I audit §1).
//
// New TU (root registers the unit next to LuaMod.cpp). No header cascades:
// lua C API + DynamicOutput + Helpers/String only. The ONLY edit to
// LuaMod.cpp this round is the single __PROSPERO__-guarded registration
// block after `lua.open_all_libs()` (prepare_mod, was :7840).
//
// Everything below is inert unless compiled for the target: the whole TU is
// __PROSPERO__-guarded, mirroring how the port keeps non-PS5 builds clean.
//
// -----------------------------------------------------------------------------
// Agent IQ revision, Claude round-12 items 1-2, 2026-10-12: write-area
// hardening after the two install-time crashes (builds d971cd2f/1c48239d,
// both logs END at the "[SHIM] write-area" marker; see
// IQ-SHIM-AUDIT.md for the full return-value audit). Changes:
//   1. install() performs ZERO filesystem work and NEVER creates
//      directories (module_start/mod-prepare does nothing to /data; the
//      only pre-first-write fs call remains load_pinned_set's fopen of
//      loadlist.txt, which was always NULL-checked and runs BEFORE the
//      write-area marker, so it is proven-safe by the crash logs).
//   2. Directory creation is LAZY, ONE-SHOT per parent dir, on FIRST WRITE
//      only — checked std::filesystem error_code overload, result recorded
//      (budgeted marker) and NEVER fatal. Deploy-time pre-creation
//      (prep_modded_launch step 5.6, deploy-precreate-writable.py) makes
//      the dirs exist beforehand so the common path is a plain fopen.
//   3. Write order: physical fopen (pre-created dir) -> one-shot lazy
//      create + one retry -> in-memory spill. Every failure is liolib
//      semantics (handle works in-process, or nil + message); no code path
//      can fault the process on an fs refusal.
//   4. FIXED install-time bug: the os.remove/os.rename wrappers captured
//      a STALE absolute stack index as their context upvalue (the old io
//      table was popped between capture and use — LuaPathShim.cpp:1592
//      pre-fix), binding the OS TABLE where the ShimContext userdata was
//      expected. lua_touserdata on a table returns NULL (checked — the
//      old code degraded to a permanent refusal, not a crash). ALL
//      wrappers now source the context from the REGISTRY anchor, so no
//      stack-shift can ever misbind again, and install() leaves the Lua
//      stack exactly as it found it.
// -----------------------------------------------------------------------------

#if defined(__PROSPERO__)

#include "LuaPathShim.hpp"

#include <lua.hpp>

#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp> // ensure_str (narrow -> StringType for markers)

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RC::LuaPathShim
{
    namespace
    {
        constexpr std::size_t kMaxPath = 4096;          // matches native_lua_file's bound (LuaMod.cpp:989)
        constexpr int kMaxWalkDepth = 6;                // case-fallback depth bound (spec rule d)
        constexpr std::size_t kMaxDirScan = 2048;       // entries scanned per directory
        constexpr std::size_t kCacheCap = 128;          // case-fallback cache entries
        constexpr int kMaxStackWalk = 16;               // debug.getinfo levels walked
        constexpr int kMaxMarkers = 10;                 // first-N marker budget (FW law)
        constexpr std::size_t kMaxModName = 128;
        constexpr std::size_t kMaxLoadlist = 1024 * 1024; // 1 MiB, MFA's own cap
        // Writable-area spill caps (agent IB, round-8 item 5): per-file cap
        // mirrors MFA's max_file_bytes precedent; total cap bounds the
        // process-memory cost of the fallback store.
        constexpr std::size_t kMaxSpillFile = 8 * 1024 * 1024;  // 8 MiB per file
        constexpr std::size_t kMaxSpillTotal = 16 * 1024 * 1024; // 16 MiB all mods

        auto lower_ascii(std::string s) -> std::string
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        auto is_ascii_alpha(char c) -> bool
        {
            const auto byte = static_cast<unsigned char>(c);
            return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
        }

        auto split_components(const std::string& path) -> std::vector<std::string>
        {
            std::vector<std::string> components;
            std::string current;
            for (const char c : path)
            {
                if (c == '/')
                {
                    components.push_back(current);
                    current.clear();
                }
                else
                {
                    current.push_back(c);
                }
            }
            components.push_back(current);
            return components;
        }

        // ---------------------------------------------------------------------------
        // Shim context — one per installed mod state. Owned by a full userdata
        // whose __gc runs the destructor; referenced by every wrapper closure as
        // an upvalue (plus a registry anchor), so it outlives the io table swap.
        // ---------------------------------------------------------------------------
        struct ShimContext
        {
            std::string modbridge_root;                   // .../roots/PPSA33286
            std::string ue4ss_root;                       // <modbridge_root>/ue4ss
            std::string writable_root;                    // .../writable (OUTSIDE admission)
            std::string mod_name;                         // owning mod (fallback boundary)
            std::unordered_set<std::string> pinned_lower; // modroot-relative keys, lowercased
            std::unordered_map<std::string, std::string> ci_cache; // lowercase physical -> physical
            // In-memory spill backing the writable area (agent IB, round-8
            // item 5): sandbox-physical path -> bytes. Fills in when the
            // backend fopen of a sandbox path fails (the documented console
            // reality: fresh /data opens fail ENOENT after module_start —
            // modforge_activation.h:6-8; round-4 session 2026-10-11 08:47:52
            // proved it for the mod root). Reads fall back to it after the
            // mod-root and sandbox physical misses.
            std::unordered_map<std::string, std::string> spill;
            // One-shot lazy directory creation (IQ, round-12): parent dirs
            // whose create_directories was ATTEMPTED (success or refusal —
            // the console refuses post-module_start /data creates by
            // contract, so a refusal is remembered exactly like a success
            // and the spill serves every later write to that dir).
            std::unordered_set<std::string> ensured_parents;
            std::mutex mutex;                             // threads share the io table (globals)
            int resolved_markers = 0;
            int rejected_markers = 0;
            int write_area_markers = 0;
            int dir_markers = 0;
        };

        auto shim_context_gc(lua_State* L) -> int
        {
            auto* context = static_cast<ShimContext*>(lua_touserdata(L, 1));
            if (context)
            {
                context->~ShimContext();
            }
            return 0;
        }

        // ---------------------------------------------------------------------------
        // Normalization (rules a-c + e): logical/PC path -> modroot-relative key
        // that ALWAYS starts with "ue4ss/". `calling_mod` anchors bare relatives
        // ("settings.txt") at the calling mod's folder. False + reason on invalid
        // input; traversal ("..") is rejected outright, so the post-join key can
        // never escape the ue4ss root (the volume is the MFA-owned /data tree;
        // no symlink traversal is accepted, matching MFA's O_NOFOLLOW stance).
        // ---------------------------------------------------------------------------
        auto normalize_logical(std::string_view input, const std::string& calling_mod, std::string& out_key, std::string& out_error,
                               bool& out_self_anchored) -> bool
        {
            out_self_anchored = false;
            if (input.empty() || input.size() > kMaxPath)
            {
                out_error = "path is empty or too long";
                return false;
            }
            std::string work{input};
            for (const char c : work)
            {
                const auto byte = static_cast<unsigned char>(c);
                if (byte < 0x20 || byte == 0x7f)
                {
                    out_error = "path contains control characters";
                    return false;
                }
            }
            // (e) chunk-name leak: "@ue4ss/Mods/..." from debug.getinfo sources.
            if (!work.empty() && work.front() == '@')
            {
                work.erase(0, 1);
            }
            // (b) backslashes become slashes (MiniMap appends "\\settings.txt").
            std::replace(work.begin(), work.end(), '\\', '/');
            // (c) drive-letter PC-ism: "C:/..." / "D:\\...".
            if (work.size() >= 2 && work[1] == ':' && is_ascii_alpha(work[0]))
            {
                work.erase(0, 2);
            }
            // (c) leading-slash PC-ism: "/Mods/..." roots at the ue4ss root.
            while (!work.empty() && work.front() == '/')
            {
                work.erase(0, 1);
            }
            while (work.rfind("./", 0) == 0)
            {
                work.erase(0, 2);
            }
            // (a) virtual PC working-dir leak: "Binaries/Win64/" (+ one "..").
            if (work.size() >= 15)
            {
                const std::string head = lower_ascii(work.substr(0, 15));
                if (head == "binaries/win64/")
                {
                    work.erase(0, 15);
                    if (work.rfind("../", 0) == 0)
                    {
                        work.erase(0, 3); // Binaries/Win64/.. -> game root
                    }
                }
            }
            if (work.empty())
            {
                out_error = "path is empty after normalization";
                return false;
            }
            auto components = split_components(work);
            components.erase(std::remove(components.begin(), components.end(), std::string{}), components.end());
            if (components.empty())
            {
                out_error = "path has no components";
                return false;
            }
            for (const auto& component : components)
            {
                if (component == ".." || component == ".")
                {
                    out_error = "path traversal is rejected";
                    return false;
                }
            }
            // Anchor the key: after an embedded "ue4ss" component (full PC
            // absolute paths embed it), at a leading "Mods" component, or at
            // the calling mod's folder for bare relatives. Only the first two
            // anchor the path in itself; the bare-relative branch merely
            // echoes the calling mod (not a trustworthy mod boundary).
            std::size_t start = 0;
            bool anchored = false;
            for (std::size_t i = 0; i < components.size(); ++i)
            {
                if (lower_ascii(components[i]) == "ue4ss")
                {
                    start = i + 1;
                    anchored = true;
                    break;
                }
            }
            if (!anchored && lower_ascii(components.front()) == "mods")
            {
                start = 0;
                anchored = true;
            }
            out_key = "ue4ss";
            if (anchored)
            {
                out_self_anchored = true;
                for (std::size_t i = start; i < components.size(); ++i)
                {
                    out_key += '/';
                    out_key += components[i];
                }
            }
            else
            {
                if (calling_mod.empty() || calling_mod.find('/') != std::string::npos || calling_mod.size() > kMaxModName)
                {
                    out_error = "relative path has no calling mod to anchor against";
                    return false;
                }
                out_key += "/Mods/";
                out_key += calling_mod;
                for (const auto& component : components)
                {
                    out_key += '/';
                    out_key += component;
                }
            }
            // Post-normalization confinement check on the key itself.
            if (out_key.size() <= 6 || out_key.compare(0, 6, "ue4ss/") != 0)
            {
                out_error = "normalized path escaped the ue4ss root";
                return false;
            }
            return true;
        }

        // ---------------------------------------------------------------------------
        // Case-insensitive fallback (rule d): walk `physical`'s components
        // (relative to `base_root`, which MUST be a prefix of `physical` — the
        // mod root for admitted-tree reads, the writable root for sandbox
        // targets) against the real directory entries. depth <= 6 counted in
        // walked components, first match wins, 2048 entries/directory scan
        // cap. keep_last_literal (write modes) walks only the parent and
        // keeps the final component as given, so newly created files keep the
        // mod's casing.
        // ---------------------------------------------------------------------------
        auto case_resolve(ShimContext& context, const std::string& base_root, const std::string& physical, bool keep_last_literal) -> std::string
        {
            std::error_code ec;
            if (!keep_last_literal && std::filesystem::exists(std::filesystem::path(physical), ec))
            {
                return physical;
            }
            const std::string lower_want = lower_ascii(physical);
            {
                std::lock_guard<std::mutex> guard{context.mutex};
                if (const auto found = context.ci_cache.find(lower_want); found != context.ci_cache.end())
                {
                    return found->second;
                }
            }
            std::string current = base_root;
            if (physical.compare(0, base_root.size(), base_root) != 0)
            {
                return physical; // base mismatch: nothing to walk
            }
            std::string rest = physical.substr(base_root.size());
            while (!rest.empty() && rest.front() == '/')
            {
                rest.erase(0, 1);
            }
            auto components = split_components(rest);
            if (keep_last_literal && !components.empty())
            {
                components.pop_back();
            }
            if (components.size() > static_cast<std::size_t>(kMaxWalkDepth))
            {
                return physical; // too deep to walk; exact path stands
            }
            if (!std::filesystem::is_directory(std::filesystem::path(current), ec))
            {
                return physical;
            }
            for (const auto& want : components)
            {
                std::optional<std::string> hit;
                std::error_code iter_ec;
                std::size_t scanned = 0;
                for (std::filesystem::directory_iterator it{current, iter_ec}, end{};
                     it != end && !iter_ec && scanned < kMaxDirScan; it.increment(iter_ec))
                {
                    ++scanned;
                    const std::string name = it->path().filename().string();
                    if (lower_ascii(name) == lower_ascii(want))
                    {
                        hit = name;
                        break;
                    }
                }
                if (!hit)
                {
                    return physical; // miss stays as-is; the backend fopen decides
                }
                current += '/';
                current += *hit;
            }
            if (keep_last_literal)
            {
                const std::size_t slash = physical.find_last_of('/');
                if (slash != std::string::npos && slash + 1 < physical.size())
                {
                    current += '/';
                    current += physical.substr(slash + 1);
                }
            }
            {
                std::lock_guard<std::mutex> guard{context.mutex};
                if (context.ci_cache.size() >= kCacheCap)
                {
                    context.ci_cache.clear();
                }
                context.ci_cache.emplace(lower_want, current);
            }
            return current;
        }

        // ---------------------------------------------------------------------------
        // Calling-mod detection (write confinement): innermost debug.getinfo
        // frame whose source SELF-ANCHORS under ue4ss/Mods/<name>/ with <name>
        // neither empty nor "shared" (Mods/shared is a library folder, not a
        // mod boundary). Non-anchored frames (C functions, the shim itself)
        // are skipped — they would only echo the fallback. Falls back to the
        // install-time mod.
        // ---------------------------------------------------------------------------
        auto detect_calling_mod(lua_State* L, const ShimContext& context) -> std::string
        {
            lua_Debug ar{};
            for (int level = 1; level <= kMaxStackWalk; ++level)
            {
                if (lua_getstack(L, level, &ar) != 1)
                {
                    break;
                }
                if (lua_getinfo(L, "S", &ar) == 0)
                {
                    continue;
                }
                const char* source = ar.source;
                if (!source || !source[0] || (source[0] != '@' && std::strchr(source, '/') == nullptr))
                {
                    continue; // "=[C]" and bare chunk names are not file paths
                }
                std::string key;
                std::string error;
                bool self_anchored = false;
                if (!normalize_logical(source, context.mod_name, key, error, self_anchored) || !self_anchored)
                {
                    continue;
                }
                const std::string key_lower = lower_ascii(key);
                constexpr const char* kModsPrefix = "ue4ss/mods/";
                if (key_lower.compare(0, 11, kModsPrefix) != 0)
                {
                    continue;
                }
                const std::size_t name_start = 11;
                const std::size_t slash = key_lower.find('/', name_start);
                if (slash == std::string::npos || slash == name_start)
                {
                    continue;
                }
                const std::string name = key.substr(name_start, slash - name_start);
                if (name.empty() || lower_ascii(name) == "shared")
                {
                    continue;
                }
                return name;
            }
            return context.mod_name;
        }

        // ---------------------------------------------------------------------------
        // Writable area (agent IB, Claude round-8 item 5). Admission blocks
        // creating unlisted files under the admitted root — and the round-4
        // session proved the stricter fact that fresh /data opens fail at the
        // OS level after module_start (raw-liolib progress.txt write refused,
        // 2026-10-11 08:47:52), while modforge itself intercepts NOTHING
        // (require/dofile are served from the in-memory activation, not a
        // file layer). Writes therefore land in a per-mod sandbox OUTSIDE
        // admission: <writable-root>/<modname>/<path relative to the mod
        // folder>, path-shape preserving. The writable root is derived from
        // the modbridge root by replacing the trailing "/roots/<title>" with
        // "/writable" (/data/modbridge/roots/PPSA33286 ->
        // /data/modbridge/writable); roots without that shape get
        // "<root>-writable" (host fixtures stay outside audited trees).
        // ---------------------------------------------------------------------------
        auto derive_writable_root(const std::string& modbridge_root) -> std::string
        {
            const std::size_t marker = modbridge_root.rfind("/roots/");
            if (marker != std::string::npos && marker + 7 < modbridge_root.size())
            {
                return modbridge_root.substr(0, marker) + "/writable";
            }
            return modbridge_root + "-writable";
        }

        // "ue4ss/Mods/<mod>/<rest>" -> <writable_root>/<mod>/<rest>; empty
        // when the key is not under Mods/<mod>/ (ue4ss-root configs have no
        // sandbox analog — they are pinned/admitted territory anyway).
        auto sandbox_of_key(const ShimContext& context, const std::string& key) -> std::string
        {
            constexpr const char* kModsPrefix = "ue4ss/Mods/";
            constexpr std::size_t kPrefixLen = 11; // strlen("ue4ss/Mods/")
            if (key.compare(0, kPrefixLen, kModsPrefix) != 0)
            {
                return {};
            }
            const std::size_t slash = key.find('/', kPrefixLen);
            if (slash == std::string::npos || slash == kPrefixLen)
            {
                return {};
            }
            const std::string mod = key.substr(kPrefixLen, slash - kPrefixLen);
            const std::string rest = key.substr(slash + 1);
            if (mod.empty() || rest.empty() || mod.find('/') != std::string::npos)
            {
                return {};
            }
            return context.writable_root + "/" + mod + "/" + rest;
        }

        // --- in-memory spill (console reality: fresh /data opens fail
        // ENOENT after module_start, so the sandbox physical open may fail;
        // the spill keeps every mod functional within the process). Lookups
        // fall back to a case-insensitive scan: a write and a read may spell
        // the mod folder differently and the sandbox tree may not exist to
        // case-resolve against. ---
        auto spill_total_bytes_locked(const ShimContext& context) -> std::size_t
        {
            std::size_t total = 0;
            for (const auto& [path, bytes] : context.spill)
            {
                (void)path;
                total += bytes.size();
            }
            return total;
        }

        auto spill_store(ShimContext& context, const std::string& path, std::string bytes) -> bool
        {
            std::lock_guard<std::mutex> guard{context.mutex};
            if (bytes.size() > kMaxSpillFile)
            {
                return false;
            }
            const auto found = context.spill.find(path);
            const std::size_t previous = found != context.spill.end() ? found->second.size() : 0;
            if (spill_total_bytes_locked(context) - previous + bytes.size() > kMaxSpillTotal)
            {
                return false;
            }
            context.spill[path] = std::move(bytes);
            return true;
        }

        auto spill_fetch(ShimContext& context, const std::string& path, std::string& out_bytes) -> bool
        {
            std::lock_guard<std::mutex> guard{context.mutex};
            if (const auto found = context.spill.find(path); found != context.spill.end())
            {
                out_bytes = found->second;
                return true;
            }
            const std::string lower = lower_ascii(path);
            for (const auto& [candidate, bytes] : context.spill)
            {
                if (lower_ascii(candidate) == lower)
                {
                    out_bytes = bytes;
                    return true;
                }
            }
            return false;
        }

        auto spill_erase(ShimContext& context, const std::string& path) -> bool
        {
            std::lock_guard<std::mutex> guard{context.mutex};
            if (context.spill.erase(path) != 0)
            {
                return true;
            }
            const std::string lower = lower_ascii(path);
            for (auto it = context.spill.begin(); it != context.spill.end(); ++it)
            {
                if (lower_ascii(it->first) == lower)
                {
                    context.spill.erase(it);
                    return true;
                }
            }
            return false;
        }

        auto spill_move(ShimContext& context, const std::string& from, const std::string& to) -> bool
        {
            std::lock_guard<std::mutex> guard{context.mutex};
            auto found = context.spill.find(from);
            if (found == context.spill.end())
            {
                const std::string lower = lower_ascii(from);
                for (auto it = context.spill.begin(); it != context.spill.end(); ++it)
                {
                    if (lower_ascii(it->first) == lower)
                    {
                        found = it;
                        break;
                    }
                }
            }
            if (found == context.spill.end())
            {
                return false;
            }
            std::string bytes = std::move(found->second);
            context.spill.erase(found);
            context.spill[to] = std::move(bytes);
            return true;
        }

        // LAZY, ONE-SHOT parent-directory creation for a sandbox write
        // target (IQ, Claude round-12): attempted on FIRST WRITE to a given
        // parent only — never at install()/module_start time. CHECKED
        // std::filesystem error_code overload: ec clear means the parent is
        // usable (deploy-time pre-created, or created now — host tests and
        // any console whose reachability window is wider than the
        // documented contract); ec set means the OS refused and the spill
        // serves every write to that dir. Either way the attempt is
        // remembered (refusal == success for scheduling purposes) and the
        // return value is consumed by the caller — a refusal can NEVER
        // reach an unchecked-use path, because the only use is a retry that
        // itself falls back to the spill.
        auto ensure_parent_dirs_once(ShimContext& context, const std::string& physical) -> bool
        {
            const std::filesystem::path parent = std::filesystem::path(physical).parent_path();
            if (parent.empty())
            {
                return false;
            }
            const std::string key = parent.string();
            {
                std::lock_guard<std::mutex> guard{context.mutex};
                if (context.ensured_parents.count(key) != 0)
                {
                    return false; // already attempted (pre-created, created, or refused)
                }
                context.ensured_parents.insert(key);
            }
            std::error_code ec;
            const bool created = std::filesystem::create_directories(parent, ec); // checked below
            const bool usable = !ec; // created==false + !ec == already existed (deploy pre-create)
            if (context.dir_markers < kMaxMarkers)
            {
                ++context.dir_markers;
                Output::send(STR("[SHIM] write-area dir {} created={} usable={}\n"),
                             ensure_str(key), created, usable);
            }
            return usable;
        }

        // ---------------------------------------------------------------------------
        // Wrappers. Upvalues: 1 = original function, 2 = ShimContext userdata.
        // Wrappers never throw across the Lua boundary and never call lua_error:
        // failures follow the liolib failure contract (nil, message).
        // Read order (agent IB writable area): mod-root physical ->
        // writable-sandbox physical -> in-memory spill. Write target: the
        // per-mod sandbox only (physical attempt first, spill backing when
        // the OS refuses the create — the round-4-proven console reality).
        // ---------------------------------------------------------------------------

        auto push_memfile(lua_State* L, ShimContext* context, const std::string& path, const char* mode) -> int;

        auto push_refusal(lua_State* L, ShimContext* context, const char* reason, const std::string& path, const std::string& mod) -> int
        {
            if (context->rejected_markers < kMaxMarkers)
            {
                ++context->rejected_markers;
                Output::send(STR("[SHIM] write-rejected path={} mod={}\n"), ensure_str(path), ensure_str(mod));
            }
            lua_pushnil(L);
            lua_pushfstring(L, "%s: %s", reason, path.c_str());
            return 2;
        }

        auto is_write_mode(const char* mode) -> bool
        {
            return std::strpbrk(mode, "wa+") != nullptr;
        }

        // Resolves argv slot `slot` (a string) into its logical input and
        // modroot-relative key. Write modes additionally enforce confinement
        // to the calling mod's folder and the pinned-set refusal (the
        // admitted set stays read-only). Returns true with (out_logical,
        // out_key) on success, or false with (nil, message) already pushed.
        auto resolve_into(lua_State* L, ShimContext* context, int slot, bool write_mode, std::string& out_logical, std::string& out_key) -> bool
        {
            size_t length = 0;
            const char* raw = lua_tolstring(L, slot, &length);
            if (!raw)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "path must be a string");
                return false;
            }
            out_logical.assign(raw, length);
            const std::string mod = detect_calling_mod(L, *context);
            std::string key;
            std::string error;
            bool self_anchored = false;
            if (!normalize_logical(out_logical, mod, key, error, self_anchored))
            {
                lua_pushnil(L);
                lua_pushfstring(L, "invalid path: %s (%s)", error.c_str(), out_logical.c_str());
                return false;
            }
            const std::string key_lower = lower_ascii(key);
            if (write_mode)
            {
                // The key is modroot-relative, so the boundary is compared in
                // key space ("ue4ss/mods/<mod>/"), never against the physical
                // root prefix — that mismatch would refuse every write.
                const std::string boundary = lower_ascii("ue4ss/Mods/" + mod + "/");
                if (key_lower.compare(0, boundary.size(), boundary) != 0)
                {
                    push_refusal(L, context, "path outside mod root", out_logical, mod);
                    return false;
                }
                if (context->pinned_lower.count(key_lower) != 0)
                {
                    push_refusal(L, context, "path is admission-pinned", out_logical, mod);
                    return false;
                }
            }
            out_key = std::move(key);
            return true;
        }

        // Physical read path for a key: mod-root first (case-resolved).
        auto modroot_physical(ShimContext& context, const std::string& key) -> std::string
        {
            return case_resolve(context, context.modbridge_root, context.modbridge_root + "/" + key, false);
        }

        // Physical write target for a CONFINED key: the per-mod sandbox,
        // path-shape preserving, final component's literal casing kept for
        // new files. PURE RESOLUTION (IQ, round-12): creates NOTHING —
        // directory creation is deploy-time or lazy-on-first-write
        // (ensure_parent_dirs_once at the single call site in
        // io_open_wrapper), never a side effect of path resolution.
        auto sandbox_target(ShimContext& context, const std::string& key) -> std::string
        {
            const std::string target = sandbox_of_key(context, key);
            return case_resolve(context, context.writable_root, target, true);
        }

        // Replace argv slot `slot` with `physical` and shrink the stack back
        // to argc (a previous backend attempt may have left results behind).
        auto set_path_argument(lua_State* L, int slot, int argc, const std::string& physical) -> void
        {
            lua_pushlstring(L, physical.data(), physical.size());
            lua_replace(L, slot);
            lua_settop(L, argc);
        }

        // argv slot already replaced with the resolved physical path; tail-call
        // the original with all remaining arguments. The original's own
        // (nil, message) contract is preserved.
        auto call_original(lua_State* L) -> int
        {
            const int argc = lua_gettop(L);
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_insert(L, 1);
            if (lua_pcall(L, argc, LUA_MULTRET, 0) != LUA_OK)
            {
                const char* message = lua_tostring(L, -1);
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, message ? message : "io shim backend failed");
                return 2;
            }
            return lua_gettop(L);
        }

        // Attempts the original backend with `physical` in argv slot 1
        // (stack shrunk to argc). Returns the original's result count on
        // success (>=1 with a non-nil first value), or 0 with the original's
        // (nil, message) popped and discarded.
        //
        // [B4 fix, 2026-10-12] The backend is called on a COPY of the
        // arguments. It used to replace slot 1 and call the backend on the
        // originals, which lua_pcall consumes: after a failed first attempt
        // the stack was empty, so every later attempt in the same io.open /
        // io.lines (lazy-create retry, sandbox read after the mod-root miss)
        // ran io.open(nil, nil) and could never succeed. The originals (and
        // the mode string the caller holds a pointer into) now stay on the
        // stack below the results; a C function returns only the top values.
        auto try_original(lua_State* L, int argc, const std::string& physical) -> int
        {
            lua_settop(L, argc);
            if (argc < 1 || !lua_checkstack(L, argc + 2))
            {
                return 0;
            }
            const int base = lua_gettop(L);
            lua_pushvalue(L, lua_upvalueindex(1)); // backend (original io.open / io.lines)
            lua_pushlstring(L, physical.data(), physical.size());
            for (int i = 2; i <= argc; ++i)
            {
                lua_pushvalue(L, i);
            }
            if (lua_pcall(L, argc, LUA_MULTRET, 0) != LUA_OK)
            {
                lua_settop(L, base); // drop the error; the caller tries the next location
                return 0;
            }
            const int results = lua_gettop(L) - base;
            if (results >= 1 && !lua_isnil(L, base + 1))
            {
                return results;
            }
            lua_settop(L, base);
            return 0;
        }

        auto io_open_wrapper(lua_State* L) -> int
        {
            auto* context = static_cast<ShimContext*>(lua_touserdata(L, lua_upvalueindex(2)));
            if (!context)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "io shim context is unavailable");
                return 2;
            }
            const int argc = lua_gettop(L);
            const char* mode = argc >= 2 ? lua_tolstring(L, 2, nullptr) : "r";
            const bool write_mode = mode ? is_write_mode(mode) : false;
            std::string logical;
            std::string key;
            if (!resolve_into(L, context, 1, write_mode, logical, key))
            {
                return 2;
            }
            if (write_mode)
            {
                // Writes land ONLY in the per-mod writable sandbox (outside
                // admission; the admitted mod root is never a write target).
                // Lazy order (IQ, Claude round-12 fix design):
                //   1. physical fopen of the target — deploy-time
                //      pre-creation (prep step 5.6) makes the common path a
                //      plain, checked open that persists and is
                //      ftpsrv-visible;
                //   2. on refusal: ONE lazy create_directories attempt for
                //      that parent dir (checked error_code overload,
                //      budgeted marker, never fatal) + ONE retry;
                //   3. on refusal again: the in-memory spill (the
                //      documented console reality), so the handle always
                //      works in-process.
                // Every step's failure is consumed here; no fs refusal can
                // reach an unchecked use, and the worst case for Lua is
                // liolib semantics (working spill handle, or nil+message
                // when even the spill cap is exceeded) — never a crash.
                const std::string target = sandbox_target(*context, key);
                if (context->resolved_markers < kMaxMarkers)
                {
                    ++context->resolved_markers;
                    Output::send(STR("[SHIM] io-open resolved={}->{}\n"), ensure_str(logical), ensure_str(target));
                }
                if (const int results = try_original(L, argc, target); results > 0)
                {
                    return results;
                }
                if (ensure_parent_dirs_once(*context, target))
                {
                    if (const int results = try_original(L, argc, target); results > 0)
                    {
                        return results;
                    }
                }
                return push_memfile(L, context, target, mode);
            }
            // Read order: mod-root physical -> sandbox physical -> spill.
            const std::string modroot = modroot_physical(*context, key);
            if (context->resolved_markers < kMaxMarkers)
            {
                ++context->resolved_markers;
                Output::send(STR("[SHIM] io-open resolved={}->{}\n"), ensure_str(logical), ensure_str(modroot));
            }
            if (const int results = try_original(L, argc, modroot); results > 0)
            {
                return results;
            }
            const std::string sandbox = sandbox_of_key(*context, key);
            if (!sandbox.empty())
            {
                if (const int results = try_original(L, argc, case_resolve(*context, context->writable_root, sandbox, false)); results > 0)
                {
                    return results;
                }
                std::string bytes;
                if (spill_fetch(*context, sandbox, bytes))
                {
                    return push_memfile(L, context, sandbox, mode);
                }
            }
            lua_pushnil(L);
            lua_pushfstring(L, "%s: no such file or directory (mod root, writable area, shim store)", logical.c_str());
            return 2;
        }

        auto io_lines_wrapper(lua_State* L) -> int
        {
            auto* context = static_cast<ShimContext*>(lua_touserdata(L, lua_upvalueindex(2)));
            if (!context)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "io shim context is unavailable");
                return 2;
            }
            if (lua_type(L, 1) != LUA_TSTRING)
            {
                // liolib's io.lines() without a string iterates the current
                // input; pass that shape through untouched.
                return call_original(L);
            }
            const int argc = lua_gettop(L);
            std::string logical;
            std::string key;
            if (!resolve_into(L, context, 1, false, logical, key))
            {
                return 2;
            }
            if (int results = try_original(L, argc, modroot_physical(*context, key)); results > 0)
            {
                return results;
            }
            const std::string sandbox = sandbox_of_key(*context, key);
            if (!sandbox.empty())
            {
                if (int results = try_original(L, argc, case_resolve(*context, context->writable_root, sandbox, false)); results > 0)
                {
                    return results;
                }
                std::string bytes;
                if (spill_fetch(*context, sandbox, bytes))
                {
                    const int handle = push_memfile(L, context, sandbox, "r");
                    if (handle == 1)
                    {
                        lua_getfield(L, -1, "lines");
                        lua_pushvalue(L, -2);
                        lua_call(L, 1, 1);
                        lua_insert(L, -2); // drop the handle, keep the iterator
                        lua_pop(L, 1);
                        return 1;
                    }
                    return handle; // nil + message
                }
            }
            lua_pushnil(L);
            lua_pushfstring(L, "%s: no such file or directory (mod root, writable area, shim store)", logical.c_str());
            return 2;
        }

        auto os_remove_wrapper(lua_State* L) -> int
        {
            auto* context = static_cast<ShimContext*>(lua_touserdata(L, lua_upvalueindex(2)));
            if (!context)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "io shim context is unavailable");
                return 2;
            }
            const int argc = lua_gettop(L);
            std::string logical;
            std::string key;
            if (!resolve_into(L, context, 1, true, logical, key))
            {
                return 2;
            }
            // os.remove operates on the SANDBOX mirror only: the admitted
            // mod root can never be mutated by a mod. The spill entry goes
            // first so memory-backed files remove cleanly too.
            const std::string target = sandbox_target(*context, key);
            const bool had_spill = spill_erase(*context, target);
            set_path_argument(L, 1, argc, target);
            const int results = call_original(L);
            if (results >= 1 && !lua_isnil(L, 1))
            {
                return results;
            }
            const char* message = results >= 2 ? lua_tostring(L, 2) : nullptr;
            std::string saved = message ? message : "remove failed";
            lua_pop(L, results);
            if (had_spill)
            {
                lua_pushboolean(L, 1);
                return 1;
            }
            lua_pushnil(L);
            lua_pushstring(L, saved.c_str());
            return 2;
        }

        auto os_rename_wrapper(lua_State* L) -> int
        {
            auto* context = static_cast<ShimContext*>(lua_touserdata(L, lua_upvalueindex(2)));
            if (!context)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "io shim context is unavailable");
                return 2;
            }
            const int argc = lua_gettop(L);
            std::string logical_old;
            std::string key_old;
            if (!resolve_into(L, context, 1, true, logical_old, key_old))
            {
                return 2;
            }
            std::string logical_new;
            std::string key_new;
            if (!resolve_into(L, context, 2, true, logical_new, key_new))
            {
                return 2;
            }
            // Both endpoints map into the same mod's sandbox (confinement
            // already enforced); the spill entry travels with the rename.
            const std::string target_old = sandbox_target(*context, key_old);
            const std::string target_new = sandbox_target(*context, key_new);
            const bool had_spill = spill_move(*context, target_old, target_new);
            set_path_argument(L, 1, argc, target_old);
            lua_pushlstring(L, target_new.data(), target_new.size());
            lua_replace(L, 2);
            lua_settop(L, argc);
            const int results = call_original(L);
            if (results >= 1 && !lua_isnil(L, 1))
            {
                return results;
            }
            const char* message = results >= 2 ? lua_tostring(L, 2) : nullptr;
            std::string saved = message ? message : "rename failed";
            lua_pop(L, results);
            if (had_spill)
            {
                lua_pushboolean(L, 1);
                return 1;
            }
            lua_pushnil(L);
            lua_pushstring(L, saved.c_str());
            return 2;
        }

        auto os_execute_stub(lua_State* L) -> int
        {
            (void)L;
            lua_pushnil(L);
            lua_pushliteral(L, "os.execute is not available on this platform");
            return 2;
        }

        auto io_popen_stub(lua_State* L) -> int
        {
            (void)L;
            lua_pushnil(L);
            lua_pushliteral(L, "io.popen is not available on this platform");
            return 2;
        }

        auto io_tmpfile_stub(lua_State* L) -> int
        {
            (void)L;
            lua_pushnil(L);
            lua_pushliteral(L, "io.tmpfile is not available on this platform");
            return 2;
        }

        // ---------------------------------------------------------------------------
        // MemFile — liolib-shaped handle over the writable-area spill store
        // (agent IB, round-8 item 5). Returned by io.open when the backend
        // fopen of a sandbox path fails (documented console reality: fresh
        // /data opens fail ENOENT after module_start) and by reads that hit
        // the spill. The handle owns its bytes locally and flushes into the
        // shared per-context spill map on every mutation, so a later
        // io.open(path,"r") — or another thread's — sees them. __gc only
        // destroys the local buffer (never touches the context), so state
        // teardown order can never dangle.
        // ---------------------------------------------------------------------------
        struct MemFile
        {
            ShimContext* context; // for spill flush/fetch (state-lifetime; not touched by __gc)
            std::string path;     // sandbox-physical key
            std::string data;     // local working copy
            std::size_t offset = 0;
            bool write_mode = false;
            bool readable = false;
            bool closed = false;
        };

        auto memfile_gc(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(lua_touserdata(L, 1));
            if (file)
            {
                file->~MemFile();
            }
            return 0;
        }

        auto memfile_tostring(lua_State* L) -> int
        {
            lua_pushliteral(L, "file (writable-area store)");
            return 1;
        }

        auto memfile_flush_to_spill(MemFile& file) -> bool
        {
            return file.context && spill_store(*file.context, file.path, file.data);
        }

        auto memfile_write(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(luaL_checkudata(L, 1, "UE4SS.PathShim.MemFile"));
            if (!file || file->closed || !file->write_mode)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "file is not writable");
                return 2;
            }
            const int argc = lua_gettop(L);
            for (int i = 2; i <= argc; ++i)
            {
                std::string chunk;
                if (lua_type(L, i) == LUA_TNUMBER)
                {
                    if (lua_isinteger(L, i))
                    {
                        char buffer[32];
                        const int written = std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(lua_tointeger(L, i)));
                        if (written > 0)
                        {
                            chunk.assign(buffer, static_cast<std::size_t>(written));
                        }
                    }
                    else
                    {
                        char buffer[64];
                        const int written = std::snprintf(buffer, sizeof(buffer), "%.14g", lua_tonumber(L, i));
                        if (written > 0)
                        {
                            chunk.assign(buffer, static_cast<std::size_t>(written));
                        }
                    }
                }
                else
                {
                    size_t length = 0;
                    const char* bytes = lua_tolstring(L, i, &length);
                    if (!bytes)
                    {
                        lua_pushnil(L);
                        lua_pushliteral(L, "string or number expected");
                        return 2;
                    }
                    chunk.assign(bytes, length);
                }
                if (chunk.empty())
                {
                    continue;
                }
                const std::size_t written = chunk.size();
                if (file->offset + written > file->data.size())
                {
                    file->data.resize(file->offset);
                }
                file->data.replace(file->offset, written, chunk.data(), written);
                file->offset += written;
            }
            if (!memfile_flush_to_spill(*file))
            {
                lua_pushnil(L);
                lua_pushliteral(L, "writable-area store limit reached");
                return 2;
            }
            lua_pushvalue(L, 1); // liolib: returns the file
            return 1;
        }

        // liolib read formats: "l"/"L" line, "a" all, "n" number, <n> bytes.
        auto memfile_read_one(lua_State* L, MemFile& file, int arg_slot) -> int
        {
            if (file.offset >= file.data.size())
            {
                lua_pushnil(L); // EOF
                return 1;
            }
            if (lua_type(L, arg_slot) == LUA_TNUMBER)
            {
                const lua_Integer want = lua_tointeger(L, arg_slot);
                if (want <= 0)
                {
                    lua_pushlstring(L, "", 0);
                    return 1;
                }
                const std::size_t take = static_cast<std::size_t>(want) > file.data.size() - file.offset
                    ? file.data.size() - file.offset
                    : static_cast<std::size_t>(want);
                lua_pushlstring(L, file.data.data() + file.offset, take);
                file.offset += take;
                return 1;
            }
            size_t fmt_len = 0;
            const char* fmt = lua_tolstring(L, arg_slot, &fmt_len);
            const char format = (fmt && fmt_len >= 1) ? fmt[0] : 'l';
            if (format == 'a')
            {
                lua_pushlstring(L, file.data.data() + file.offset, file.data.size() - file.offset);
                file.offset = file.data.size();
                return 1;
            }
            if (format == 'n')
            {
                // bounded numeric prefix parse (liolib contract: nil on no number)
                std::size_t cursor = file.offset;
                while (cursor < file.data.size() &&
                       (std::isspace(static_cast<unsigned char>(file.data[cursor])) != 0))
                {
                    ++cursor;
                }
                const std::size_t start = cursor;
                while (cursor < file.data.size() &&
                       (std::isdigit(static_cast<unsigned char>(file.data[cursor])) != 0 ||
                        file.data[cursor] == '-' || file.data[cursor] == '+' ||
                        file.data[cursor] == '.' || file.data[cursor] == 'e' || file.data[cursor] == 'E' ||
                        ((file.data[cursor] == 'x' || file.data[cursor] == 'X') &&
                         cursor > start)))
                {
                    ++cursor;
                }
                if (cursor == start)
                {
                    lua_pushnil(L);
                    return 1;
                }
                const std::string number_text = file.data.substr(start, cursor - start);
                char* parse_end = nullptr;
                const double value = std::strtod(number_text.c_str(), &parse_end);
                if (parse_end == number_text.c_str())
                {
                    lua_pushnil(L);
                    return 1;
                }
                file.offset = cursor;
                lua_pushnumber(L, value);
                return 1;
            }
            // 'l' / 'L' (and anything else degrades to line mode)
            const std::size_t newline = file.data.find('\n', file.offset);
            const std::size_t line_end = newline == std::string::npos ? file.data.size() : newline;
            const std::size_t take = line_end - file.offset;
            lua_pushlstring(L, file.data.data() + file.offset, take);
            file.offset = line_end;
            if (newline != std::string::npos)
            {
                file.offset += 1;
                if (format == 'L')
                {
                    const char with_newline[] = "\n";
                    lua_pushlstring(L, with_newline, 1);
                    lua_concat(L, 2);
                }
            }
            return 1;
        }

        auto memfile_read(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(luaL_checkudata(L, 1, "UE4SS.PathShim.MemFile"));
            if (!file || file->closed || !file->readable)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "file is not readable");
                return 2;
            }
            const int argc = lua_gettop(L);
            int results = 0;
            if (argc < 2)
            {
                results = memfile_read_one(L, *file, 0); // no format arg: default 'l'
            }
            else
            {
                for (int i = 2; i <= argc; ++i)
                {
                    results += memfile_read_one(L, *file, i);
                    if (lua_isnil(L, -1) && argc > 2)
                    {
                        break;
                    }
                }
            }
            return results;
        }

        auto memfile_lines_iter(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(lua_touserdata(L, lua_upvalueindex(1)));
            if (!file || file->closed)
            {
                return 0;
            }
            if (file->offset >= file->data.size())
            {
                return 0; // iterator end
            }
            return memfile_read_one(L, *file, 0);
        }

        auto memfile_lines(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(luaL_checkudata(L, 1, "UE4SS.PathShim.MemFile"));
            if (!file || file->closed || !file->readable)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "file is not readable");
                return 2;
            }
            lua_pushvalue(L, 1);
            lua_pushcclosure(L, memfile_lines_iter, 1);
            return 1;
        }

        auto memfile_seek(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(luaL_checkudata(L, 1, "UE4SS.PathShim.MemFile"));
            if (!file || file->closed)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "file is closed");
                return 2;
            }
            size_t whence_len = 0;
            const char* whence_text = lua_tolstring(L, 2, &whence_len);
            const char whence = (whence_text && whence_len >= 1) ? whence_text[0] : 'c';
            lua_Integer delta = 0;
            int is_number = 0;
            if (lua_gettop(L) >= 3)
            {
                delta = lua_tointegerx(L, 3, &is_number);
                if (!is_number)
                {
                    delta = 0;
                }
            }
            std::size_t base = 0;
            if (whence == 's')
            {
                base = 0;
            }
            else if (whence == 'e')
            {
                base = file->data.size();
            }
            else
            {
                base = file->offset;
            }
            const long long target = static_cast<long long>(base) + static_cast<long long>(delta);
            if (target < 0 || static_cast<unsigned long long>(target) > file->data.size())
            {
                lua_pushnil(L);
                lua_pushliteral(L, "invalid offset");
                return 2;
            }
            file->offset = static_cast<std::size_t>(target);
            lua_pushinteger(L, static_cast<lua_Integer>(file->offset));
            return 1;
        }

        auto memfile_flush(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(luaL_checkudata(L, 1, "UE4SS.PathShim.MemFile"));
            if (file && !file->closed && !memfile_flush_to_spill(*file))
            {
                lua_pushnil(L);
                lua_pushliteral(L, "writable-area store limit reached");
                return 2;
            }
            lua_pushvalue(L, 1);
            return 1;
        }

        auto memfile_close(lua_State* L) -> int
        {
            auto* file = static_cast<MemFile*>(luaL_checkudata(L, 1, "UE4SS.PathShim.MemFile"));
            if (file && !file->closed)
            {
                if (file->write_mode && !memfile_flush_to_spill(*file))
                {
                    file->closed = true;
                    lua_pushnil(L);
                    lua_pushliteral(L, "writable-area store limit reached");
                    return 2;
                }
                file->closed = true;
            }
            lua_pushboolean(L, 1);
            return 1;
        }

        // mode parsing helpers for the handle factory
        auto mode_has(const char* mode, char want) -> bool
        {
            return mode && std::strchr(mode, want) != nullptr;
        }

        // Pushes a MemFile handle onto the stack (1 result). Returns 1, or
        // pushes nil+message (2 results) for r+-style opens of missing
        // files or a store-cap failure on open.
        auto push_memfile(lua_State* L, ShimContext* context, const std::string& path, const char* mode) -> int
        {
            const bool write_mode = mode ? is_write_mode(mode) : false;
            const bool wants_read = mode ? (std::strchr(mode, 'r') != nullptr || std::strchr(mode, '+') != nullptr || !write_mode) : true;
            const bool must_exist = mode ? (!write_mode && std::strchr(mode, 'r') != nullptr && std::strchr(mode, '+') != nullptr) : false;
            std::string existing;
            const bool had_existing = spill_fetch(*context, path, existing);
            if (must_exist && !had_existing)
            {
                lua_pushnil(L);
                lua_pushfstring(L, "No such file or directory: %s", path.c_str());
                return 2;
            }
            auto* file = static_cast<MemFile*>(lua_newuserdatauv(L, sizeof(MemFile), 0));
            if (!file)
            {
                lua_pushnil(L);
                lua_pushliteral(L, "writable-area store allocation failed");
                return 2;
            }
            new (file) MemFile{};
            file->context = context;
            file->path = path;
            file->write_mode = write_mode;
            file->readable = wants_read || mode_has(mode, '+');
            if (write_mode)
            {
                if (mode_has(mode, 'w'))
                {
                    file->data.clear(); // w/w+ truncate (a/a+/r+ never do)
                }
                else if (had_existing)
                {
                    file->data = std::move(existing); // a/a+ start from the spill copy
                }
                file->offset = mode_has(mode, 'a') ? file->data.size() : 0;
                if (!spill_store(*context, file->path, file->data))
                {
                    file->~MemFile();
                    lua_pop(L, 1);
                    lua_pushnil(L);
                    lua_pushliteral(L, "writable-area store limit reached");
                    return 2;
                }
            }
            else
            {
                file->data = std::move(existing);
                file->offset = 0;
            }
            if (luaL_newmetatable(L, "UE4SS.PathShim.MemFile"))
            {
                // methods table under __index
                lua_newtable(L);
                const int methods = lua_gettop(L);
                lua_pushcfunction(L, memfile_write);
                lua_setfield(L, methods, "write");
                lua_pushcfunction(L, memfile_read);
                lua_setfield(L, methods, "read");
                lua_pushcfunction(L, memfile_lines);
                lua_setfield(L, methods, "lines");
                lua_pushcfunction(L, memfile_seek);
                lua_setfield(L, methods, "seek");
                lua_pushcfunction(L, memfile_flush);
                lua_setfield(L, methods, "flush");
                lua_pushcfunction(L, memfile_close);
                lua_setfield(L, methods, "close");
                lua_setfield(L, -2, "__index");
                lua_pushcfunction(L, memfile_gc);
                lua_setfield(L, -2, "__gc");
                lua_pushcfunction(L, memfile_tostring);
                lua_setfield(L, -2, "__tostring");
            }
            lua_setmetatable(L, -2);
            return 1;
        }

        // Registry idempotence sentinel (namespace scope: install() and
        // push_context() must agree on ONE address; it was a function-local
        // static before, which worked, but the shared helper needs it here).
        const char g_sentinel = 0;

        // Idempotence probe: true when a context is ALREADY anchored for
        // this state (install already ran). Pushes nothing (balanced).
        auto context_is_anchored(lua_State* L) -> bool
        {
            lua_pushlightuserdata(L, const_cast<char*>(&g_sentinel));
            lua_gettable(L, LUA_REGISTRYINDEX);
            const bool anchored = !lua_isnil(L, -1);
            lua_pop(L, 1);
            return anchored;
        }

        // Pushes the registry-anchored ShimContext userdata (IQ, round-12).
        // Wrappers MUST source their context upvalue ONLY through this
        // lookup: absolute stack indices go STALE the moment any pop shifts
        // the stack (the pre-fix os.remove/os.rename wrappers captured the
        // OS TABLE this way — LuaPathShim.cpp:1592 pre-fix). The registry
        // anchor is immutable for the state's lifetime, so a misbind is
        // structurally impossible. Returns true when a userdata was pushed.
        auto push_context(lua_State* L) -> bool
        {
            lua_pushlightuserdata(L, const_cast<char*>(&g_sentinel));
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_isuserdata(L, -1))
            {
                return true;
            }
            lua_pop(L, 1);
            return false;
        }

        // Wrap `name` in the table at ABSOLUTE index `table_index`, with
        // upvalues [original][context]: lua_pushcclosure binds the last n
        // pushed values as upvalue(1..n) in push order, so the original
        // goes first (wrappers read upvalue(1) as the backend, upvalue(2)
        // as the context userdata). The context comes from the REGISTRY
        // (push_context), never from a caller-captured stack index.
        auto wrap_into(lua_State* L, int table_index, const char* name, lua_CFunction wrapper) -> void
        {
            lua_getfield(L, table_index, name); // upvalue 1: original
            if (!lua_isfunction(L, -1))
            {
                lua_pop(L, 1);
                return; // nothing to wrap; the field stays as-is (or absent)
            }
            if (!push_context(L)) // upvalue 2: context (registry-sourced)
            {
                lua_pop(L, 1);
                return; // no anchored context: leave the stock function in place
            }
            lua_pushcclosure(L, wrapper, 2);
            lua_setfield(L, table_index, name);
        }

        // ---------------------------------------------------------------------------
        // Pinned set (admission safety). Union of:
        //   - every `file <role> <path> <size>` row of <modbridge_root>/loadlist.txt
        //     when parseable (the authoritative MFA pin set; agent-I audit),
        //   - built-in defaults (the 4 root INIs + mods.txt, shared/UEHelpers.lua,
        //     the calling mod's enabled.txt + Scripts/main.lua) so refusal holds
        //     even when loadlist.txt cannot be read.
        // ---------------------------------------------------------------------------
        auto load_pinned_set(const std::string& modbridge_root, const std::string& mod_name) -> std::unordered_set<std::string>
        {
            std::unordered_set<std::string> pinned;
            auto add = [&pinned](std::string key) { pinned.insert(lower_ascii(std::move(key))); };
            add("ue4ss/UE4SS-settings.ini");
            add("ue4ss/MemberVariableLayout.ini");
            add("ue4ss/VTableLayout.ini");
            add("ue4ss/PS5Pad.ini");
            add("ue4ss/mods.txt"); // console-owned control file (not MFA-pinned; conservative)
            add("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua");
            if (!mod_name.empty() && mod_name.find('/') == std::string::npos && mod_name.size() <= kMaxModName)
            {
                add("ue4ss/Mods/" + mod_name + "/enabled.txt");
                add("ue4ss/Mods/" + mod_name + "/Scripts/main.lua");
            }
            if (std::FILE* file = std::fopen((modbridge_root + "/loadlist.txt").c_str(), "rb"))
            {
                std::string buffer;
                char chunk[4096];
                size_t got = 0;
                while (buffer.size() < kMaxLoadlist && (got = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
                {
                    buffer.append(chunk, got);
                }
                std::fclose(file);
                std::size_t line_start = 0;
                while (line_start < buffer.size())
                {
                    const std::size_t line_end = buffer.find('\n', line_start);
                    const std::size_t stop = line_end == std::string::npos ? buffer.size() : line_end;
                    std::string_view line{buffer.data() + line_start, stop - line_start};
                    line_start = stop + 1;
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    // Rows: "file <role> <path> <size>" (TAB- or space-split).
                    if (line.compare(0, 4, "file") == 0 && (line.size() == 4 || line[4] == ' ' || line[4] == '\t'))
                    {
                        const std::size_t role_at = line.find_first_not_of(" \t", 4);
                        if (role_at == std::string_view::npos)
                        {
                            continue;
                        }
                        const std::size_t role_end = line.find_first_of(" \t", role_at);
                        if (role_end == std::string_view::npos)
                        {
                            continue;
                        }
                        const std::size_t path_at = line.find_first_not_of(" \t", role_end);
                        if (path_at == std::string_view::npos)
                        {
                            continue;
                        }
                        std::size_t path_end = line.find_first_of(" \t", path_at);
                        if (path_end == std::string_view::npos)
                        {
                            path_end = line.size();
                        }
                        const std::string_view path = line.substr(path_at, path_end - path_at);
                        if (!path.empty())
                        {
                            add(std::string{path});
                        }
                    }
                }
            }
            return pinned;
        }

        std::string g_modbridge_root_override;
        std::string g_writable_root_override;
    } // namespace

    auto set_modbridge_root_override(std::string root) -> void
    {
        g_modbridge_root_override = std::move(root);
    }

    auto set_writable_root_override(std::string root) -> void
    {
        g_writable_root_override = std::move(root);
    }

    auto install(lua_State* L, const char* mod_name, const char* modbridge_root) -> void
    {
        if (!L)
        {
            return;
        }
        try
        {
            lua_getglobal(L, "io");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return; // io library not opened yet (or replaced): nothing to do
            }
            const int io_old = lua_gettop(L);

            // Idempotence: one install per state (registry sentinel — the
            // SAME namespace-scope g_sentinel push_context() resolves).
            if (context_is_anchored(L))
            {
                lua_pop(L, 1); // the io table; the stack is balanced again
                return;
            }

            const std::string mod = mod_name ? std::string{mod_name} : std::string{};
            auto* context = static_cast<ShimContext*>(lua_newuserdatauv(L, sizeof(ShimContext), 0));
            if (!context)
            {
                lua_pop(L, 2);
                return;
            }
            new (context) ShimContext{};
            // Rule 10 (games): NO title-id default. Resolution order: test
            // override > the runtime-resolved root passed by the installer
            // (derived from the program's mods directory). Empty => the
            // shim disables itself loudly rather than guess a game.
            context->modbridge_root = !g_modbridge_root_override.empty()
                ? g_modbridge_root_override
                : (modbridge_root && modbridge_root[0] != '\0' ? std::string{modbridge_root} : std::string{});
            if (context->modbridge_root.empty())
            {
                lua_pop(L, 2);
                Output::send(STR("[SHIM] path-resolver disabled reason=no-root (no title-id default exists; derive from the mods directory)\n"));
                return;
            }
            while (context->modbridge_root.size() > 1 && context->modbridge_root.back() == '/')
            {
                context->modbridge_root.pop_back();
            }
            context->ue4ss_root = context->modbridge_root + "/ue4ss";
            context->writable_root = !g_writable_root_override.empty()
                ? g_writable_root_override
                : derive_writable_root(context->modbridge_root);
            context->mod_name = mod;
            // The ONLY filesystem call before the first write: a checked
            // fopen of <root>/loadlist.txt (returns NULL post-module_start
            // per contract; built-in defaults then back the pinned set).
            // Runs BEFORE the write-area marker — the crash logs proving
            // both processes died AT/AFTER the marker therefore also prove
            // this call survived on console (it cannot be the crash site).
            context->pinned_lower = load_pinned_set(context->modbridge_root, mod);
            if (context->write_area_markers < kMaxMarkers && !mod.empty() && mod.find('/') == std::string::npos)
            {
                ++context->write_area_markers;
                Output::send(STR("[SHIM] write-area {} -> {}/{}\n"),
                             ensure_str(mod), ensure_str(context->writable_root), ensure_str(mod));
            }
            const int context_index = lua_gettop(L);
            if (luaL_newmetatable(L, "UE4SS.PathShim.Context"))
            {
                lua_pushcfunction(L, shim_context_gc);
                lua_setfield(L, -2, "__gc");
            }
            lua_setmetatable(L, context_index);

            // Registry anchor FIRST (IQ, round-12): from this point the
            // userdata's lifetime is the registry + closure upvalues, so
            // every later section can pop freely without endangering it,
            // and wrap_into re-fetches it by lookup (no stale indices).
            lua_pushlightuserdata(L, const_cast<char*>(&g_sentinel));
            lua_pushvalue(L, context_index);
            lua_settable(L, LUA_REGISTRYINDEX);
            lua_pop(L, 1); // the stack copy; the registry owns the context now
            // Stack here: [io_old] exactly — install-time invariant for the
            // whole table-swap section below (every index absolute).

            // Replace the io table (the patched liolib has NO path hook:
            // io_open calls fopen directly — ue4ss-stdio-consumer-adaptation-1/
            // patched/liolib.c:270-277). Copy every field (stdout/stderr/stdin
            // passthrough included), then wrap the path-taking entries.
            // NOTHING in this section touches the filesystem; a failure can
            // only throw bad_alloc (caught below, stock io still in place —
            // setglobal is the section's LAST global mutation).
            lua_newtable(L);
            const int io_new = lua_gettop(L);
            lua_pushnil(L);
            while (lua_next(L, io_old) != 0)
            {
                lua_pushvalue(L, -2); // key copy
                lua_pushvalue(L, -2); // value copy
                lua_settable(L, io_new);
                lua_pop(L, 1); // original value; keep the key for lua_next
            }
            wrap_into(L, io_new, "open", io_open_wrapper);
            wrap_into(L, io_new, "lines", io_lines_wrapper);
            lua_pushcfunction(L, io_popen_stub);
            lua_setfield(L, io_new, "popen");
            lua_pushcfunction(L, io_tmpfile_stub);
            lua_setfield(L, io_new, "tmpfile");
            lua_setglobal(L, "io"); // the ONLY global mutation of the section
                                    // (pops the new table)
            lua_pop(L, 1);          // old io table -> stack EMPTY (balanced)

            // os library: same resolver treatment for the destructive pair;
            // os.execute is stubbed (no system() passthrough, section 5 plan).
            // Absolute index for the table (the pre-fix relative -1 +
            // stale context_index pair is what misbound remove/rename).
            lua_getglobal(L, "os");
            if (lua_istable(L, -1))
            {
                const int os_index = lua_gettop(L);
                wrap_into(L, os_index, "remove", os_remove_wrapper);
                wrap_into(L, os_index, "rename", os_rename_wrapper);
                lua_pushcfunction(L, os_execute_stub);
                lua_setfield(L, os_index, "execute");
            }
            lua_pop(L, 1);

            // require seam (minimal + documented): native mode already replaced
            // package.searchers with [preload, native buffer searcher]
            // (LuaMod.cpp setup_lua_require_paths) whose module names resolve
            // as NativeModSource logical keys — require never consults
            // package.path there. We only publish logical PC-style entries so
            // Lua code that reads or derives from package.path sees consistent
            // values. package.cpath stays blank (native mode blanked it).
            if (!mod.empty() && mod.find('/') == std::string::npos && mod.size() <= kMaxModName)
            {
                lua_getglobal(L, "package");
                if (lua_istable(L, -1))
                {
                    lua_pushfstring(L,
                                    "ue4ss/Mods/%s/Scripts/?.lua;"
                                    "ue4ss/Mods/%s/?.lua;"
                                    "ue4ss/Mods/shared/?.lua;"
                                    "ue4ss/Mods/shared/?/init.lua;"
                                    "ue4ss/Mods/shared/?/?.lua",
                                    mod.c_str(),
                                    mod.c_str());
                    lua_setfield(L, -2, "path");
                }
                lua_pop(L, 1);
            }

            Output::send(STR("[SHIM] path-resolver installed mod={} root={}\n"), ensure_str(mod), ensure_str(context->modbridge_root));
        }
        catch (void*)
        {
            // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
            // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
            throw;
        }
        catch (...)
        {
            // Registration is best-effort: any failure leaves the stock io/os
            // tables in place (unshimmed behavior), never a broken Lua state.
            // Lua truncates the C-frame stack window on return, so leftover
            // stack slots from a partial install are reclaimed.
        }
    }
} // namespace RC::LuaPathShim

namespace RC
{
    auto LuaPathShim_install(lua_State* L, const char* mod_name, const char* modbridge_root) -> void
    {
        LuaPathShim::install(L, mod_name, modbridge_root);
    }
} // namespace RC

#else // !__PROSPERO__

#include "LuaPathShim.hpp"

namespace RC
{
    auto LuaPathShim_install(lua_State*, const char*, const char*) -> void
    {
        // Host builds register nothing; the PS5 shim is target-only.
    }
} // namespace RC

#endif // __PROSPERO__
