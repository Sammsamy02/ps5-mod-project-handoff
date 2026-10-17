-- path-shim.lua — host mirror of the PS5 io/require path-resolver rules.
-- Agent GA, 2026-10-08 (Claude round-5 item 4).
-- Agent IB revision, 2026-10-11 (Claude round-8 item 5): writable-area
-- rules mirrored 1:1 — writes land in a per-mod sandbox
-- <writable-root>/<modname>/<mod-relative path> OUTSIDE admission (root
-- derived by replacing the trailing "/roots/<title>" of the modbridge root
-- with "/writable"; other roots get "<root>-writable"), reads resolve
-- mod-root physical -> sandbox physical -> in-memory store, and
-- os.remove/os.rename are sandbox-confined. Mirrors
-- round4-setaside/LuaPathShim.cpp (see include/LuaPathShim.hpp prose).
--
-- Pure-Lua re-implementation of the rule table in
-- outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaPathShim.cpp
-- (see include/LuaPathShim.hpp for the rule prose). One source of truth on
-- the host: host-tests/lua-path-shim-test.lua asserts the RULES here, and
-- host-lua-harness/stub-ue4ss.lua --with-io routes env.io through them so
-- ladder subjects exercise REAL path shapes.
--
-- Divergences from the C++ TU, all host-only and documented:
--   * the physical root is DETECTED from the first stack source containing
--     an "ue4ss" component (the C++ TU pins /data/modbridge/roots/<TITLE>);
--   * the case-insensitive fallback lists directories with `ls` (the C++ TU
--     walks std::filesystem::directory_iterator);
--   * write modes can be routed to an in-memory virtual store so harness
--     runs never touch the audited mod trees (the C++ TU attempts the
--     sandbox physical open first and falls back to its per-context spill
--     map — the virtual store models "physical refused", which is the
--     documented console reality: fresh /data opens fail ENOENT after
--     module_start, round-4 session 2026-10-11 08:47:52).
--
-- Lua 5.2-5.5 common subset only (harness law; no setfenv, no 5.5 globals).

local PathShim = {}

PathShim.DEFAULT_MODBRIDGE_ROOT = "/data/modbridge/roots/PPSA33286"

local MAX_PATH = 4096
local MAX_WALK_DEPTH = 6
local MAX_STACK_WALK = 16
local MAX_MOD_NAME = 128

local function lower_ascii(s)
    return (s:gsub("[A-Z]", function(c) return c:lower() end))
end

local function components_of(p)
    local out = {}
    for comp in p:gmatch("[^/]+") do out[#out + 1] = comp end
    return out
end

local function real_getinfo(level)
    if not (debug and debug.getinfo) then return nil end
    local ok, info = pcall(debug.getinfo, level, "S")
    if ok then return info end
    return nil
end

-- normalize_logical: logical/PC path -> modroot-relative key that always
-- starts "ue4ss/". Mirrors LuaPathShim.cpp normalize_logical rule-for-rule.
-- Third return: self_anchored — true when the input carried its own "ue4ss"
-- or leading "Mods" component, false when the calling-mod fallback anchored
-- it. The stack walk only trusts self-anchored sources (a bare-relative
-- echo of the fallback would be meaningless as a mod boundary).
function PathShim.normalize(path, calling_mod)
    if type(path) ~= "string" or #path == 0 or #path > MAX_PATH then
        return nil, "path is empty or too long"
    end
    for i = 1, #path do
        local b = path:byte(i)
        if b < 32 or b == 127 then return nil, "path contains control characters" end
    end
    local work = path
    if work:sub(1, 1) == "@" then work = work:sub(2) end               -- (e) chunk-name leak
    work = work:gsub("\\", "/")                                        -- (b) backslashes
    if #work >= 2 and work:sub(2, 2) == ":" and work:sub(1, 1):match("[A-Za-z]") then
        work = work:sub(3)                                             -- (c) drive letter
    end
    while work:sub(1, 1) == "/" do work = work:sub(2) end              -- (c) leading slash
    while work:sub(1, 2) == "./" do work = work:sub(3) end
    if #work >= 15 and lower_ascii(work:sub(1, 15)) == "binaries/win64/" then
        work = work:sub(16)                                            -- (a) virtual PC cwd leak
        if work:sub(1, 3) == "../" then work = work:sub(4) end
    end
    if #work == 0 then return nil, "path is empty after normalization" end
    local comps = components_of(work)
    if #comps == 0 then return nil, "path has no components" end
    for _, comp in ipairs(comps) do
        if comp == ".." or comp == "." then return nil, "path traversal is rejected" end
    end
    -- Anchor: after an embedded "ue4ss" component, at a leading "Mods",
    -- or at the calling mod's folder for bare relatives.
    local anchor_from
    for i, comp in ipairs(comps) do
        if lower_ascii(comp) == "ue4ss" then anchor_from = i + 1 break end
    end
    local self_anchored = true
    local key_parts = { "ue4ss" }
    if anchor_from then
        for i = anchor_from, #comps do key_parts[#key_parts + 1] = comps[i] end
    elseif lower_ascii(comps[1]) == "mods" then
        for _, comp in ipairs(comps) do key_parts[#key_parts + 1] = comp end
    else
        if type(calling_mod) ~= "string" or #calling_mod == 0
            or calling_mod:find("/", 1, true) or #calling_mod > MAX_MOD_NAME then
            return nil, "relative path has no calling mod to anchor against"
        end
        self_anchored = false
        key_parts[#key_parts + 1] = "Mods"
        key_parts[#key_parts + 1] = calling_mod
        for _, comp in ipairs(comps) do key_parts[#key_parts + 1] = comp end
    end
    local key = table.concat(key_parts, "/")
    if key:sub(1, 6) ~= "ue4ss/" or #key <= 6 then
        return nil, "normalized path escaped the ue4ss root"
    end
    return key, nil, self_anchored
end

-- Host-root detection: "…/anything/ue4ss/Mods/…" -> "…/anything" (the
-- C++ TU pins the modbridge constant instead).
function PathShim.detect_root_from_source(source)
    if type(source) ~= "string" then return nil end
    local prefix = source:match("^(.*)/ue4ss/")
    if prefix and #prefix > 0 then return prefix end
    return nil
end

-- derive_writable_root (agent IB, round-8 item 5; mirrors LuaPathShim.cpp
-- derive_writable_root): "/data/modbridge/roots/PPSA33286" ->
-- "/data/modbridge/writable" (the writable area sits OUTSIDE the admitted
-- roots tree, so MFA/loadlist never see it); any other root shape gets
-- "<root>-writable" so host fixtures stay out of audited trees.
function PathShim.derive_writable_root(modbridge_root)
    if type(modbridge_root) ~= "string" or #modbridge_root == 0 then return nil end
    local prefix = modbridge_root:match("^(.*)/roots/[^/]+$")
    if prefix and #prefix > 0 then return prefix .. "/writable" end
    return modbridge_root .. "-writable"
end

-- sandbox_of_key (mirrors LuaPathShim.cpp sandbox_of_key):
-- "ue4ss/Mods/<mod>/<rest>" -> <writable_root>/<mod>/<rest>; nil when the
-- key is not under Mods/<mod>/ (ue4ss-root configs have no sandbox analog).
function PathShim.sandbox_of_key(writable_root, key)
    local mod, rest = key:match("^ue4ss/Mods/([^/]+)/(.+)$")
    if not mod or not rest or #mod == 0 or #rest == 0 then return nil end
    return writable_root .. "/" .. mod .. "/" .. rest
end

-- detect_calling_mod(getinfo): innermost frame (levels 1..16) whose source
-- SELF-ANCHORS under ue4ss/Mods/<name>/ with <name> not "" and not "shared".
-- Non-anchored frames (resolver/stub/harness files, "=C") are skipped: they
-- would only echo the fallback through the bare-relative branch.
function PathShim.detect_calling_mod(getinfo, fallback_mod)
    getinfo = getinfo or real_getinfo
    for level = 1, MAX_STACK_WALK do
        local info = getinfo(level)
        if not info then break end
        local source = info and info.source
        if type(source) == "string" and #source > 0
            and (source:sub(1, 1) == "@" or source:find("/", 1, true)) then
            local key, _, self_anchored = PathShim.normalize(source, fallback_mod)
            if key and self_anchored then
                local rest = key:sub(12) -- after "ue4ss/Mods/"
                local name = rest:match("^([^/]+)/")
                if name and name ~= "" and lower_ascii(name) ~= "shared" then
                    return name
                end
            end
        end
    end
    return fallback_mod
end

-- default pinned keys: built-ins + loadlist.txt `file` rows when readable.
function PathShim.default_pinned(mod_name, root)
    local pinned = {}
    local function add(key) pinned[lower_ascii(key)] = true end
    add("ue4ss/UE4SS-settings.ini")
    add("ue4ss/MemberVariableLayout.ini")
    add("ue4ss/VTableLayout.ini")
    add("ue4ss/PS5Pad.ini")
    add("ue4ss/mods.txt")
    add("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua")
    if type(mod_name) == "string" and #mod_name > 0 and not mod_name:find("/", 1, true)
        and #mod_name <= MAX_MOD_NAME then
        add("ue4ss/Mods/" .. mod_name .. "/enabled.txt")
        add("ue4ss/Mods/" .. mod_name .. "/Scripts/main.lua")
    end
    if root then
        local fh = io.open(root .. "/loadlist.txt", "r")
        if fh then
            for line in fh:lines() do
                local path = line:match("^file[%s\t]+[^%s\t]+[%s\t]+([^%s\t]+)")
                if path then add(path) end
            end
            fh:close()
        end
    end
    return pinned
end

local function list_dir(dir)
    -- BSD/macOS ls has no `--` terminator; quote-only like runner.lua.
    local pipe = io.popen('ls -1A "' .. dir:gsub('"', '\\"') .. '" 2>/dev/null')
    if not pipe then return nil end
    local names = {}
    for name in pipe:lines() do names[#names + 1] = name end
    local ok = pipe:close()
    if not ok then return nil end
    return names
end

-- PathShim.new{ root=, mod=, pinned=, getinfo=, virtual_writes=, writable_root= }
--   resolver:normalize(path)          -> key | nil, err        (calling mod detected)
--   resolver:resolve(path, write)     -> physical | nil, err   (writes -> sandbox target)
--   resolver:io_open(path, mode)      -> handle | nil, err     (read: modroot -> sandbox -> store)
--   resolver:io_lines(path)           -> iterator | nil, err   (same read order)
--   resolver:os_remove(path)          -> true | nil, err       (sandbox-confined)
--   resolver:os_rename(old, new)      -> true | nil, err       (sandbox-confined)
--   resolver.virtual_files            -> { [sandbox physical] = string } (store)
--   resolver.write_area               -> { mod = ..., path = ... } (the [SHIM] marker fact)
function PathShim.new(opts)
    opts = opts or {}
    local r = {}
    r.root = opts.root or PathShim.DEFAULT_MODBRIDGE_ROOT
    r.mod = opts.mod or ""
    r.writable_root = opts.writable_root or PathShim.derive_writable_root(r.root)
    r.pinned = opts.pinned or PathShim.default_pinned(r.mod, r.root)
    r.getinfo = opts.getinfo or real_getinfo
    r.virtual_writes = not not opts.virtual_writes
    r.virtual_files = {}
    r.ci_cache = {}
    r.resolved_log = {}
    r.write_area = (type(r.mod) == "string" and #r.mod > 0 and not r.mod:find("/", 1, true))
        and { mod = r.mod, path = r.writable_root .. "/" .. r.mod } or nil

    function r:calling_mod()
        return PathShim.detect_calling_mod(self.getinfo, self.mod)
    end

    function r:key_physical(key)
        return self.root .. "/" .. key
    end

    -- Mirrors LuaPathShim.cpp case_resolve: depth<=6, first match, cached.
    -- base is the walk root (mod bridge root for admitted-tree paths, the
    -- writable root for sandbox paths — mirrors the C++ base_root param).
    function r:case_resolve(physical, keep_last_literal, base)
        -- No cheap short-circuit: on a case-insensitive host, every cheap
        -- probe "succeeds" for wrong-case spellings, so the bounded walk
        -- below is the only exact-case check. A full miss returns the path
        -- unchanged (the caller's fopen/miss decides, as in the C++ twin).
        base = base or r.root
        local lower_want = lower_ascii(physical)
        if r.ci_cache[lower_want] then return r.ci_cache[lower_want] end
        if physical:sub(1, #base) ~= base then return physical end
        local prefix_len = #base + 1 -- the "/" after the base
        if physical:byte(prefix_len) ~= 0x2F then return physical end
        local comps = components_of(physical:sub(prefix_len + 1))
        if keep_last_literal then comps[#comps] = nil end
        if #comps > MAX_WALK_DEPTH then return physical end
        local current = base
        for _, want in ipairs(comps) do
            local names = list_dir(current)
            if not names then return physical end
            local hit
            for _, name in ipairs(names) do
                if lower_ascii(name) == lower_ascii(want) then hit = name break end
            end
            if not hit then return physical end
            current = current .. "/" .. hit
        end
        if keep_last_literal then
            local tail = physical:match("([^/]+)$")
            if tail then current = current .. "/" .. tail end
        end
        if r.ci_cache[lower_want] == nil then r.ci_cache[lower_want] = current end
        return current
    end

    -- resolve(path, write): full pipeline (calling-mod detect, normalize,
    -- confinement + pinned refusal for writes). WRITES map to the per-mod
    -- sandbox target (writable area OUTSIDE admission); reads stay on the
    -- mod-root physical (the caller applies the sandbox/store fallbacks).
    function r:resolve(path, write)
        local mod = self:calling_mod()
        local key, err = PathShim.normalize(path, mod)
        if not key then return nil, "invalid path: " .. tostring(err) .. " (" .. tostring(path) .. ")" end
        if write then
            -- The key is modroot-relative, so the boundary is too: compare
            -- against "ue4ss/mods/<mod>/" IN KEY SPACE, never the physical
            -- root prefix (that mismatch refused every write).
            local boundary = lower_ascii("ue4ss/Mods/" .. mod .. "/")
            local key_lower = lower_ascii(key)
            if key_lower:sub(1, #boundary) ~= boundary then
                return nil, "path outside mod root: " .. tostring(path), mod
            end
            if r.pinned[key_lower] then
                return nil, "path is admission-pinned: " .. tostring(path), mod
            end
            local target = PathShim.sandbox_of_key(self.writable_root, key)
            if not target then
                return nil, "path has no writable-area mapping: " .. tostring(path), mod
            end
            target = self:case_resolve(target, true, self.writable_root)
            r.resolved_log[#r.resolved_log + 1] = { logical = path, physical = target, write = true }
            return target, nil, mod
        end
        local physical = self:case_resolve(self:key_physical(key), false)
        r.resolved_log[#r.resolved_log + 1] = { logical = path, physical = physical, write = false }
        return physical
    end

    -- Sandbox read candidate for a read-mode path (nil when the key is not
    -- under Mods/<mod>/ — ue4ss-root configs have no sandbox analog).
    function r:read_sandbox(path)
        local key = PathShim.normalize(path, self:calling_mod())
        if not key then return nil end
        local target = PathShim.sandbox_of_key(self.writable_root, key)
        if not target then return nil end
        return self:case_resolve(target, false, self.writable_root)
    end

    -- liolib-shaped open. Read modes try the REAL host file through the
    -- mod-root physical, then the sandbox physical, then the in-memory
    -- store; write modes target the SANDBOX path only (never the admitted
    -- mod root) — through the virtual store when virtual_writes is set
    -- (harness law: never touch audited trees; also the model of the C++
    -- spill, which backs writes when the OS refuses fresh /data opens).
    function r:io_open(path, mode)
        mode = mode or "r"
        local write = not not mode:find("[wa+]")
        local physical, err = self:resolve(path, write)
        if not physical then return nil, err end
        if not write then
            local fh = io.open(physical, mode) -- host liolib: mod-root physical first
            if fh then return fh end
            local sandbox = self:read_sandbox(path)
            if sandbox then
                fh = io.open(sandbox, mode) -- sandbox physical second
                if fh then return fh end
                local stored = r.virtual_files[sandbox]
                if stored == nil then -- case-insensitive store lookup (C++ twin scans)
                    local lower = lower_ascii(sandbox)
                    for key, bytes in pairs(r.virtual_files) do
                        if lower_ascii(key) == lower then stored = bytes break end
                    end
                end
                if stored ~= nil then
                    return r:store_reader(sandbox, stored)
                end
            end
            return nil, tostring(physical) .. ": no such file (resolved through mod root, writable area, shim store from " .. tostring(path) .. ")"
        end
        if not r.virtual_writes then
            return io.open(physical, mode)
        end
        local truncate = not not mode:find("w")
        local initial = r.virtual_files[physical]
        if initial == nil then
            local lower = lower_ascii(physical)
            for key, bytes in pairs(r.virtual_files) do
                if lower_ascii(key) == lower then initial = bytes break end
            end
        end
        if truncate or initial == nil then initial = "" end
        local store = { data = initial, physical = physical }
        local handle = {}
        function handle:write(...)
            for i = 1, select("#", ...) do store.data = store.data .. tostring((select(i, ...))) end
            r.virtual_files[physical] = store.data
            return self
        end
        function handle:read() return nil end
        function handle:lines() return function() return nil end end
        function handle:seek() return #store.data end
        function handle:flush() return self end
        function handle:close() return true end
        return handle
    end

    -- Reader over stored bytes (the spill-read analog of the C++ twin's
    -- MemFile read handle): read("l"/"a"/"n"/<n>) with an offset.
    function r:store_reader(path, bytes)
        local state = { data = bytes, offset = 1, path = path }
        local function read_one(fmt)
            if state.offset > #state.data then return nil end
            if type(fmt) == "number" then
                local take = math.min(fmt, #state.data - state.offset + 1)
                local chunk = state.data:sub(state.offset, state.offset + take - 1)
                state.offset = state.offset + take
                return chunk
            end
            local f = (type(fmt) == "string" and fmt ~= "") and fmt:sub(1, 1) or "l"
            if f == "a" then
                local chunk = state.data:sub(state.offset)
                state.offset = #state.data + 1
                return chunk
            end
            if f == "n" then
                local chunk = state.data:match("^%s*([%-%+%.%deExX]+)", state.offset)
                if not chunk then return nil end
                state.offset = state.offset + #state.data:match("^%s*", state.offset) + #chunk
                return tonumber(chunk)
            end
            local pos = state.data:find("\n", state.offset, true)
            local line_end = pos and pos - 1 or #state.data
            local line = state.data:sub(state.offset, line_end)
            state.offset = pos and pos + 1 or #state.data + 1
            if f == "L" then line = line .. "\n" end
            return line
        end
        local handle = {}
        function handle:read(...) return read_one((select(1, ...) or "l")) end
        function handle:lines() return function() return read_one("l") end end
        function handle:seek() return state.offset - 1 end
        function handle:write() return nil, "file is not writable" end
        function handle:flush() return self end
        function handle:close() return true end
        return handle
    end

    -- io.lines(path) through the same read order (mod root -> sandbox ->
    -- store). Format args pass through to the host liolib on the physical
    -- hits; store hits iterate default lines (documented twin boundary).
    function r:io_lines(path)
        local physical, err = self:resolve(path, false)
        if not physical then return nil, err end
        local fh = io.open(physical, "r")
        if fh then
            fh:close()
            return io.lines(physical)
        end
        local sandbox = self:read_sandbox(path)
        if sandbox then
            fh = io.open(sandbox, "r")
            if fh then
                fh:close()
                return io.lines(sandbox)
            end
            local stored = r.virtual_files[sandbox]
            if stored == nil then
                local lower = lower_ascii(sandbox)
                for key, bytes in pairs(r.virtual_files) do
                    if lower_ascii(key) == lower then stored = bytes break end
                end
            end
            if stored ~= nil then
                return r:store_reader(sandbox, stored):lines()
            end
        end
        return nil, tostring(physical) .. ": no such file (resolved through mod root, writable area, shim store)"
    end

    -- os.remove/os.rename: sandbox-confined (agent IB round-8 item 5).
    -- The mod root is NEVER mutated — removes/renames act on the sandbox
    -- mirror (physical attempt when not virtual_writes; the store entry
    -- always goes first so memory-backed files manage cleanly).
    function r:os_remove(path)
        local target, err = self:resolve(path, true)
        if not target then return nil, err end
        local had = r.virtual_files[target] ~= nil
        if not had then
            local lower = lower_ascii(target)
            for key in pairs(r.virtual_files) do
                if lower_ascii(key) == lower then
                    r.virtual_files[key] = nil
                    had = true
                    break
                end
            end
        else
            r.virtual_files[target] = nil
        end
        if r.virtual_writes then
            if had then return true end
            return nil, tostring(target) .. ": file not found in writable area"
        end
        local ok, remove_err = os.remove(target)
        if ok then return true end
        if had then return true end
        return nil, tostring(remove_err)
    end

    function r:os_rename(old_path, new_path)
        local target_old, err = self:resolve(old_path, true)
        if not target_old then return nil, err end
        local target_new, err2 = self:resolve(new_path, true)
        if not target_new then return nil, err2 end
        local bytes = r.virtual_files[target_old]
        if bytes == nil then
            local lower = lower_ascii(target_old)
            for key, value in pairs(r.virtual_files) do
                if lower_ascii(key) == lower then
                    bytes = value
                    r.virtual_files[key] = nil
                    break
                end
            end
        else
            r.virtual_files[target_old] = nil
        end
        if bytes ~= nil then r.virtual_files[target_new] = bytes end
        if r.virtual_writes then
            if bytes ~= nil then return true end
            return nil, tostring(target_old) .. ": file not found in writable area"
        end
        local ok, rename_err = os.rename(target_old, target_new)
        if ok then return true end
        if bytes ~= nil then return true end
        return nil, tostring(rename_err)
    end

    return r
end

return PathShim
