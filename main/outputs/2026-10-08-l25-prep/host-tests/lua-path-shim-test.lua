-- lua-path-shim-test.lua — pure-Lua contract test for the PS5 io/require
-- path-resolver rules. Agent GA, 2026-10-08 (Claude round-5 item 4).
-- Agent IB extension, 2026-10-11 (Claude round-8 item 5): writable-area
-- sandbox rules — write targets map OUTSIDE admission into
-- <writable-root>/<modname>/<mod-relative path>, reads fall back
-- mod-root -> sandbox -> store, os.remove/rename are sandbox-confined.
--
-- Asserts the RULE TABLE mirrored in host-lua-harness/path-shim.lua (the
-- host twin of round4-setaside/LuaPathShim.cpp):
--   virtual-cwd resolution, backslashes, PC-isms, traversal rejection,
--   case-insensitive fallback, mod-root write confinement (accept/reject
--   matrix incl. admission-pinned), the debug.getinfo round-trip law
--   (MiniMap's exact settingsPath() derivation), require path mapping,
--   and the writable-area sandbox matrix (shape, derivation, read order,
--   single-namespace round-trip, remove/rename confinement).
--
-- Usage: lua5.5 lua-path-shim-test.lua --root <fixture root>
-- (run-path-shim.sh builds the fixture tree and calls this).
-- Lua 5.2-5.5 common subset only. NO console contact, NO builds.

local function arg_of(name)
    for i = 1, #arg do if arg[i] == name then return arg[i + 1] end end
    return nil
end

local ROOT = assert(arg_of("--root"), "--root <fixture root> required")

local HERE = (arg[0]:match("^(.*)/[^/]+$") or ".")
local PathShim = dofile(HERE .. "/../host-lua-harness/path-shim.lua")

local passed, failed = 0, 0
local failures = {}
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(("PASS %-42s"):format(name))
    else
        failed = failed + 1
        failures[#failures + 1] = name .. (detail and (" :: " .. tostring(detail)) or "")
        print(("FAIL %-42s %s"):format(name, tostring(detail or "")))
    end
end

-- Deterministic calling mod for direct resolve() calls (no stack games yet:
-- the round-trip test below exercises the REAL stack walk).
local R = PathShim.new{ root = ROOT, mod = "TownfallMiniMap", virtual_writes = true }
local WRITABLE = PathShim.derive_writable_root(ROOT)

------------------------------------------------------------------
-- 1. virtual-cwd / anchoring resolution
------------------------------------------------------------------
local k1 = PathShim.normalize("Mods/TownfallMiniMap/settings.txt", "TownfallMiniMap")
local k2 = PathShim.normalize("ue4ss/Mods/TownfallMiniMap/settings.txt", "TownfallMiniMap")
check("mods-relative anchors at ue4ss root", k1 == "ue4ss/Mods/TownfallMiniMap/settings.txt", k1)
check("ue4ss-logical key round-trips", k2 == "ue4ss/Mods/TownfallMiniMap/settings.txt", k2)
check("both spellings hit one physical", R:key_physical(k1) == R:key_physical(k2))
local k3 = PathShim.normalize("settings.txt", "TownfallMiniMap")
check("bare relative anchors at calling mod", k3 == "ue4ss/Mods/TownfallMiniMap/settings.txt", k3)

------------------------------------------------------------------
-- 2. backslashes + '@' + PC-isms
------------------------------------------------------------------
check("backslashes normalize (MiniMap shape)",
      PathShim.normalize("ue4ss\\Mods\\TownfallMiniMap\\settings.txt", nil)
          == "ue4ss/Mods/TownfallMiniMap/settings.txt")
check("chunk-name '@' leak stripped",
      PathShim.normalize("@ue4ss/Mods/TownfallMiniMap/settings.txt", nil)
          == "ue4ss/Mods/TownfallMiniMap/settings.txt")
check("drive-letter rooted at ue4ss root",
      PathShim.normalize("C:/Game/ue4ss/Mods/TownfallMiniMap/settings.txt", nil)
          == "ue4ss/Mods/TownfallMiniMap/settings.txt")
check("leading-slash rooted at ue4ss root",
      PathShim.normalize("/Mods/TownfallMiniMap/settings.txt", nil)
          == "ue4ss/Mods/TownfallMiniMap/settings.txt")
check("virtual PC cwd leak (Binaries/Win64)",
      PathShim.normalize("Binaries/Win64/../Mods/TownfallMiniMap/settings.txt", nil)
          == "ue4ss/Mods/TownfallMiniMap/settings.txt")

------------------------------------------------------------------
-- 3. rejections
------------------------------------------------------------------
local function rejected(p)
    local key, err = PathShim.normalize(p, "TownfallMiniMap")
    return key == nil and err
end
check("traversal rejected (../)", rejected("../etc/passwd") == "path traversal is rejected")
check("traversal rejected (embedded)", rejected("ue4ss/../../x") == "path traversal is rejected")
check("traversal rejected (mid-path)", rejected("a/b/../../c") == "path traversal is rejected")
check("control chars rejected", rejected("ue4ss/Mods/X/\1y") == "path contains control characters")
check("empty rejected", rejected("") == "path is empty or too long")

------------------------------------------------------------------
-- 4. case-insensitive fallback (reads)
------------------------------------------------------------------
local p1, e1 = R:resolve("ue4ss/mods/townfallminimap/existing.cfg", false)
check("case fallback finds EXISTING.cfg", p1 == ROOT .. "/ue4ss/Mods/TownfallMiniMap/EXISTING.cfg", tostring(p1) .. " " .. tostring(e1))
local p2 = R:resolve("ue4ss/Mods/TownfallMiniMap/scripts/main.lua", false)
check("case fallback finds Scripts/main.lua", p2 == ROOT .. "/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua", tostring(p2))

------------------------------------------------------------------
-- 5. write-confinement accept/reject matrix (writes now land in the
--    per-mod writable sandbox, NOT the admitted mod root — agent IB)
------------------------------------------------------------------
local function try_write(p)
    local phys, err = R:resolve(p, true)
    return phys, err
end
local a1 = try_write("ue4ss/Mods/TownfallMiniMap/settings.txt")
check("ACCEPT own-folder write -> sandbox target",
      a1 == WRITABLE .. "/TownfallMiniMap/settings.txt", tostring(a1))
local a2 = try_write("settings.txt")
check("ACCEPT bare-relative write -> sandbox target",
      a2 == WRITABLE .. "/TownfallMiniMap/settings.txt", tostring(a2))
local a3 = try_write("ue4ss/Mods/TownfallMiniMap/Scripts/diag.log")
check("ACCEPT nested write, path-shape preserved",
      a3 == WRITABLE .. "/TownfallMiniMap/Scripts/diag.log", tostring(a3))
local r1, e1w = try_write("ue4ss/Mods/OtherMod/x.txt")
check("REJECT other mod folder", r1 == nil and e1w:find("path outside mod root", 1, true) ~= nil, tostring(e1w))
local r2, e2w = try_write("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua")
check("REJECT Mods/shared write", r2 == nil and e2w:find("path outside mod root", 1, true) ~= nil, tostring(e2w))
local r3, e3w = try_write("ue4ss/UE4SS-settings.ini")
check("REJECT ue4ss-root config write", r3 == nil and e3w:find("path outside mod root", 1, true) ~= nil, tostring(e3w))
local r4, e4w = try_write("ue4ss/Mods/TownfallMiniMap/enabled.txt")
check("REJECT pinned enabled.txt", r4 == nil and e4w:find("admission-pinned", 1, true) ~= nil, tostring(e4w))
local r5, e5w = try_write("ue4ss/Mods/TownfallMiniMap/Scripts/main.lua")
check("REJECT pinned own main.lua", r5 == nil and e5w:find("admission-pinned", 1, true) ~= nil, tostring(e5w))
local r6, e6w = try_write("ue4ss/Mods/TownfallMiniMap/pinned.cfg")
check("REJECT loadlist-row pinned.cfg", r6 == nil and e6w:find("admission-pinned", 1, true) ~= nil, tostring(e6w))
local r7, e7w = try_write("ue4ss/Mods/TownfallMiniMap/../../OtherMod/x.txt")
check("REJECT traversal toward sandbox sibling", r7 == nil and e7w:find("path traversal is rejected", 1, true) ~= nil, tostring(e7w))
local r8, e8w = try_write("ue4ss/Mods/TownfallMiniMap/../../../etc/passwd")
check("REJECT traversal above ue4ss root", r8 == nil and e8w:find("path traversal is rejected", 1, true) ~= nil, tostring(e8w))

------------------------------------------------------------------
-- 6. admission-safety pinned set facts (agent-I MFA audit)
------------------------------------------------------------------
check("settings.txt NOT pinned (safe to rewrite)", R.pinned["ue4ss/mods/townfallminimap/settings.txt"] == nil)
check("enabled.txt pinned", R.pinned["ue4ss/mods/townfallminimap/enabled.txt"] == true)
check("loadlist row parsed into pinned set", R.pinned["ue4ss/mods/townfallminimap/pinned.cfg"] == true)
check("root ini pinned (loadlist)", R.pinned["ue4ss/ue4ss-settings.ini"] == true)
check("UEHelpers.lua pinned (loadlist)", R.pinned["ue4ss/mods/shared/uehelpers/uehelpers.lua"] == true)

------------------------------------------------------------------
-- 7. debug.getinfo round-trip law (MiniMap's exact derivation)
------------------------------------------------------------------
-- The chunk below is loaded with the PORT-SHAPED chunk name
-- "@ue4ss/Mods/TownfallMiniMap/Scripts/main.lua" and runs MiniMap's own
-- settingsPath() code verbatim; resolver io.open must hit the fixture file.
local round_trip_src = [[
local function settingsPath()
    local info = debug and debug.getinfo and debug.getinfo(1, "S")
    local src = info and info.source or ""
    local dir = src:sub(1, 1) == "@" and src:sub(2):match("^(.*)[/\\][Ss]cripts[/\\][^/\\]+$") or nil
    return dir and (dir .. "\\settings.txt")
end
return settingsPath()
]]
local rt_chunk = assert(load(round_trip_src, "@ue4ss/Mods/TownfallMiniMap/Scripts/main.lua", "t", _G))
local derived = assert(rt_chunk())
check("MiniMap derivation yields logical path", derived == "ue4ss/Mods/TownfallMiniMap\\settings.txt", tostring(derived))
local R2 = PathShim.new{ root = ROOT, mod = "NOFALLBACK", virtual_writes = true }
local fh, ferr = R2:io_open(derived, "r")
check("round-trip READ hits physical file", fh ~= nil, tostring(ferr))
if fh then
    local content = fh:read("a")
    fh:close()
    check("round-trip READ returns seeded content", content == "round-trip-seed\n", tostring(content))
end
local detected = PathShim.detect_calling_mod(nil, "NOFALLBACK")
check("stack walk with no mod frame returns fallback", detected == "NOFALLBACK", tostring(detected))
-- The REAL stack-walk proof: inside the logical-named chunk, detection must
-- see the mod frame (we call the detector FROM that chunk's closure).
local walk_src = [[
-- NB: NOT a tail call — `return detect_calling_mod(...)` would erase this
-- frame (Lua 5.4+ tail-call reuse) and the walk could not see the mod.
local PathShim2 = ...
local result = PathShim2.detect_calling_mod(nil, "NOFALLBACK")
return result
]]
local walk_chunk = assert(load(walk_src, "@ue4ss/Mods/TownfallMiniMap/Scripts/walk.lua", "t", _G))
local walked = walk_chunk(PathShim)
check("stack walk resolves logical chunk to its mod", walked == "TownfallMiniMap", tostring(walked))

------------------------------------------------------------------
-- 8. write round-trip through the writable-area store (no disk writes;
--    the store models the C++ spill that backs the sandbox when the OS
--    refuses fresh /data creates — the round-4-proven console reality)
------------------------------------------------------------------
-- Section 7 seeded settings.txt on disk for the READ proof; remove it so
-- the "no disk write" assertion below is meaningful for the same path.
os.remove(ROOT .. "/ue4ss/Mods/TownfallMiniMap/settings.txt")
local R3 = PathShim.new{ root = ROOT, mod = "TownfallMiniMap", virtual_writes = true }
local wh = R3:io_open(derived, "w")
check("round-trip WRITE accepted (own folder, unpinned)", wh ~= nil)
if wh then
    wh:write("enabled=true\n")
    wh:write("zoom=2\n")
    wh:close()
    local sandbox_path = WRITABLE .. "/TownfallMiniMap/settings.txt"
    check("virtual write stored at SANDBOX path", R3.virtual_files[sandbox_path] == "enabled=true\nzoom=2\n", tostring(R3.virtual_files[sandbox_path]))
    local on_disk = io.open(ROOT .. "/ue4ss/Mods/TownfallMiniMap/settings.txt", "r")
    check("fixture tree untouched (no disk write)", on_disk == nil)
    if on_disk then on_disk:close() end
    -- Single-namespace law: the MOD keeps using its own logical path; the
    -- read falls back mod-root -> sandbox -> store and sees the bytes.
    local rh, rerr = R3:io_open(derived, "r")
    check("round-trip READ back through own logical path", rh ~= nil, tostring(rerr))
    if rh then
        local content = rh:read("a")
        rh:close()
        check("round-trip READ returns written bytes", content == "enabled=true\nzoom=2\n", tostring(content))
    end
end

------------------------------------------------------------------
-- 9. require path mapping (logical package.path entries)
------------------------------------------------------------------
-- The shim publishes logical PC-style package.path entries; expanding them
-- for require("UEHelpers") must resolve onto the physical shared tree.
local entries = {
    "ue4ss/Mods/TownfallMiniMap/scripts/?.lua",
    "ue4ss/Mods/TownfallMiniMap/?.lua",
    "ue4ss/Mods/shared/?.lua",
    "ue4ss/Mods/shared/?/init.lua",
    "ue4ss/Mods/shared/?/?.lua",
}
local function require_candidates(name)
    local dotted = name:gsub("%.", "/")
    local out = {}
    for _, entry in ipairs(entries) do
        out[#out + 1] = entry:gsub("%?", dotted)
    end
    return out
end
local hit
for _, cand in ipairs(require_candidates("UEHelpers")) do
    local phys = R:resolve(cand, false)
    if phys and io.open(phys, "r") then hit = phys break end
end
check("require mapping hits shared/UEHelpers.lua", hit == ROOT .. "/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", tostring(hit))

------------------------------------------------------------------
-- 10. writable-root derivation + write-area fact (agent IB)
------------------------------------------------------------------
check("derive /roots/<title> -> sibling /writable",
      PathShim.derive_writable_root("/data/modbridge/roots/PPSA33286") == "/data/modbridge/writable")
check("derive console-shape default matches", WRITABLE == PathShim.derive_writable_root(ROOT), WRITABLE)
check("derive non-roots root -> <root>-writable",
      PathShim.derive_writable_root("/tmp/fixture") == "/tmp/fixture-writable")
check("derive rejects empty root", PathShim.derive_writable_root("") == nil)
check("write-area fact recorded (marker twin)",
      R.write_area ~= nil and R.write_area.mod == "TownfallMiniMap"
      and R.write_area.path == WRITABLE .. "/TownfallMiniMap",
      tostring(R.write_area and R.write_area.path))
local RW = PathShim.new{ root = ROOT, mod = "TownfallMiniMap", virtual_writes = true, writable_root = "/explicit/writable" }
check("explicit writable_root override honored", RW:resolve("settings.txt", true) == "/explicit/writable/TownfallMiniMap/settings.txt")

------------------------------------------------------------------
-- 11. read order: mod-root first, then sandbox physical, then store
------------------------------------------------------------------
local RO = PathShim.new{ root = ROOT, mod = "TownfallMiniMap", virtual_writes = true }
-- (a) mod-root wins when both exist: seed the store AND the mod root.
RO.virtual_files[WRITABLE .. "/TownfallMiniMap/EXISTING.cfg"] = "from-store\n"
local rh1 = RO:io_open("ue4ss/Mods/TownfallMiniMap/EXISTING.cfg", "r")
check("read order: mod-root physical wins", rh1 ~= nil)
if rh1 then
    local content = rh1:read("a")
    rh1:close()
    check("read order: mod-root content served, not store", content == "case-data\n", tostring(content))
end
-- (b) store-only file (the previously-written case).
local rh2 = RO:io_open("ue4ss/Mods/TownfallMiniMap/logs/run.log", "r")
check("read order: missing everywhere -> nil", rh2 == nil)
local lw = RO:io_open("ue4ss/Mods/TownfallMiniMap/logs/run.log", "w")
check("store write accepted for new nested file", lw ~= nil)
if lw then
    lw:write("line-1\n")
    lw:write("line-2\n")
    lw:close()
end
local rh3, re3 = RO:io_open("ue4ss/Mods/TownfallMiniMap/logs/run.log", "r")
check("read order: store backs previously-written file", rh3 ~= nil, tostring(re3))
if rh3 then
    check("store read: first line", rh3:read("l") == "line-1")
    check("store read: second line", rh3:read("l") == "line-2")
    check("store read: EOF nil", rh3:read("l") == nil)
    rh3:close()
end
-- (c) io.lines over the same order.
local it = RO:io_lines("ue4ss/Mods/TownfallMiniMap/logs/run.log")
check("io.lines: store-backed iterator", it ~= nil and it() == "line-1" and it() == "line-2" and it() == nil)

------------------------------------------------------------------
-- 12. os.remove / os.rename: sandbox-confined (mod root untouchable)
------------------------------------------------------------------
local RM = PathShim.new{ root = ROOT, mod = "TownfallMiniMap", virtual_writes = true }
local rmw = RM:io_open("ue4ss/Mods/TownfallMiniMap/Scripts/diag.log", "w")
if rmw then rmw:write("x\n") rmw:close() end
local ok_rm, err_rm = RM:os_remove("ue4ss/Mods/TownfallMiniMap/Scripts/diag.log")
check("os.remove removes the SANDBOX store entry", ok_rm == true, tostring(err_rm))
check("os.remove left the mod-root tree alone",
      io.open(ROOT .. "/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua", "r") ~= nil)
local ok_rm2, err_rm2 = RM:os_remove("ue4ss/Mods/TownfallMiniMap/Scripts/main.lua")
check("os.remove pinned main.lua refused", ok_rm2 == nil and tostring(err_rm2):find("admission-pinned", 1, true) ~= nil, tostring(err_rm2))
local ok_rm3, err_rm3 = RM:os_remove("ue4ss/Mods/OtherMod/x.txt")
check("os.remove other mod refused", ok_rm3 == nil and tostring(err_rm3):find("path outside mod root", 1, true) ~= nil, tostring(err_rm3))
local rnw = RM:io_open("old-name.cfg", "w")
if rnw then rnw:write("kept\n") rnw:close() end
local ok_rn, err_rn = RM:os_rename("old-name.cfg", "new-name.cfg")
check("os.rename moves within the sandbox", ok_rn == true, tostring(err_rn))
local rn_read = RM:io_open("new-name.cfg", "r")
check("os.rename: old gone from store", RM:io_open("old-name.cfg", "r") == nil)
check("os.rename: new readable with same bytes", rn_read ~= nil and rn_read:read("a") == "kept\n")
if rn_read then rn_read:close() end
local ok_rn2, err_rn2 = RM:os_rename("new-name.cfg", "ue4ss/Mods/OtherMod/y.cfg")
check("os.rename crossing mods refused", ok_rn2 == nil and tostring(err_rn2):find("path outside mod root", 1, true) ~= nil, tostring(err_rn2))

------------------------------------------------------------------
-- 13. sandbox escape matrix (traversal-shaped writes never leave it)
------------------------------------------------------------------
local RX = PathShim.new{ root = ROOT, mod = "TownfallMiniMap", virtual_writes = true }
check("escape: bare-relative with .. refused",
      RX:resolve("../TownfallMiniMap/settings.txt", true) == nil)
check("escape: embedded .. toward other mod refused",
      RX:resolve("ue4ss/Mods/TownfallMiniMap/../../OtherMod/x.txt", true) == nil)
local c1 = RX:resolve("ue4ss/Mods/TownfallMiniMap/settings.txt", true)
check("confined write never lands inside the admitted root",
      c1 ~= nil and c1:sub(1, #ROOT + 1) ~= ROOT .. "/", tostring(c1))
check("confined write lands under the writable root",
      c1 ~= nil and c1:sub(1, #WRITABLE + 1) == WRITABLE .. "/", tostring(c1))
-- A PC-absolute spelling of the writable root itself is NOT a mod-root
-- path: it anchors at the ue4ss component or falls to the mod folder —
-- either way it stays inside THIS mod's sandbox or is refused.
local wx = RX:resolve("/data/modbridge/writable/OtherMod/x.txt", true)
check("escape: absolute writable-root path stays confined or refused",
      wx == nil or wx == WRITABLE .. "/TownfallMiniMap/data/modbridge/writable/OtherMod/x.txt", tostring(wx))

------------------------------------------------------------------
print(("="):rep(64))
print(("lua-path-shim-test: %d passed, %d failed (%s)"):format(passed, failed, ROOT))
for _, f in ipairs(failures) do print("  FAIL: " .. f) end
os.exit(failed == 0 and 0 or 1)
