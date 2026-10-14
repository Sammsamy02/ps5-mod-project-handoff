-- run-bpml.lua — BPModLoaderMod harness driver (agent GN, Claude round-6
-- item 3d). Runs the staged BPModLoaderMod main.lua against stub-ue4ss.lua
-- with the opt-in "logicmods-fs" virtual game directory (stub [GN] section)
-- so the loader runs PAST its main.lua:117 guard, through its own
-- CreateLogicModsDirectory retry, its config.lua/pak scan, load_order
-- handling, and every LoadMods trigger it registers.
--
-- Usage:
--   lua5.5 run-bpml.lua [--main <BPModLoaderMod/Scripts/main.lua>]
--        [--shared <Mods/shared dir>] [--fixture <game-root dir on disk>]
--        [--game-dir <game dir name>] [--ue4ss-root <ue4ss dir>]
--        [--name <label>] [--fresh] [--populate]
--
--   --fresh     reset the fixture to "no Paks/LogicMods yet" (exercises the
--               CreateLogicModsDirectory branch) and drop load_order.txt
--   --populate  write a realistic LogicMods layout:
--                 LogicMods/SampleBPMod/config.lua  (AssetName/AssetPath)
--                 LogicMods/SampleBPMod/SampleBPMod.pak (name-scan dummy)
--                 LogicMods/LooseBPMod_P.pak (loose pak -> default config)
--                 ue4ss-root/Mods/BPModLoaderMod/load_order.txt ("SampleBPMod")
--               .pak bodies are NEVER opened by the loader (name-scan only,
--               main.lua:149-163), so dummies are faithful.
--
-- Mac-local Lua only (no console, no builds). All fixture writes are
-- confined to the --fixture/--ue4ss-root directories (guarded rm/mkdir).
--
-- Lua 5.2-5.5 common subset (parses under the console's 5.4 family).

local function arg_of(name, default)
    for i = 1, #arg do
        if arg[i] == name then return arg[i + 1] end
    end
    return default
end
local FLAG = {}
for i = 1, #arg do if arg[i]:sub(1, 2) == "--" then FLAG[arg[i]] = true end end

local HERE = (arg[0]:match("^(.*)/[^/]+$") or ".")
local WHAT = HERE .. "/../../.." -- .../translation-core/what
local TREEA = WHAT ..
    "/.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1"
local TREEB = WHAT ..
    "/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods"

local MAIN = arg_of("--main", TREEA .. "/ue4ss-v1/ue4ss/Mods/BPModLoaderMod/Scripts/main.lua")
local SHARED = arg_of("--shared", TREEB .. "/shared")
local FIXTURE = arg_of("--fixture", HERE .. "/fixture/gamelayout")
local GAME_DIR = arg_of("--game-dir", "Townfall-Win64-Shipping")
local UE4SS_ROOT = arg_of("--ue4ss-root", HERE .. "/fixture/ue4ssroot")
local NAME = arg_of("--name", "BPModLoaderMod")
local WALL_LIMIT = tonumber(arg_of("--wall", "20")) or 20

--------------------------------------------------------------------
-- Fixture management (host-side; every path is prefix-guarded)
--------------------------------------------------------------------
local function sh(cmd)
    local p = io.popen(cmd)
    if p then p:close() end
end

local function quote(s) return "'" .. tostring(s):gsub("'", "'\\''") .. "'" end

local function guarded_rmtree(path, under)
    path = tostring(path)
    if path:len() == 0 or path == "/" then return end
    if path:sub(1, #under) ~= under then error("refusing rm outside fixture: " .. path, 0) end
    sh("rm -rf " .. quote(path))
end

local function write_file(path, content)
    local d = path:match("^(.*)/[^/]+$")
    sh("mkdir -p " .. quote(d))
    local f = assert(io.open(path, "w"))
    f:write(content)
    f:close()
end

local game_root = FIXTURE .. "/" .. GAME_DIR
local content = game_root .. "/Content"

-- Base layout: the port derives everything from
-- <root>/<game>/Binaries/<plat> (LuaMod.cpp:3276-3287); Content must exist
-- or IterateGameDirectories answers nil (the :3278/:3280 "unknown
-- structure" path). Binaries/Win64 exists in the fixture to document the
-- PC derivation shape even though the walk starts at the root. Paks is
-- kept present even in the empty state: main.lua:119 indexes
-- Dirs.Game.Content.Paks directly, and a missing Paks dir is the
-- nil-chain error the harness already demonstrated (bpml.no-paks.log).
sh("mkdir -p " .. quote(content .. "/Paks") .. " " .. quote(game_root .. "/Binaries/Win64"))

if FLAG["--fresh"] then
    guarded_rmtree(content .. "/Paks/LogicMods", content)
    sh("rm -f " .. quote(UE4SS_ROOT .. "/Mods/BPModLoaderMod/load_order.txt"))
end

if FLAG["--populate"] then
    local lm = content .. "/Paks/LogicMods"
    write_file(lm .. "/SampleBPMod/config.lua", [[
-- Sample BP mod configuration (realistic PC shape; dofile'd by
-- BPModLoaderMod main.lua:132 with the mod's own global table).
Mods = Mods or {}
Mods["SampleBPMod"] = {
    AssetName = "SampleModActor_C",
    AssetPath = "/Game/Mods/SampleBPMod/SampleModActor",
}
]])
    -- The loader only NAME-scans paks (main.lua:149-163); bytes never read.
    write_file(lm .. "/SampleBPMod/SampleBPMod.pak",
        "dummy classic pak (name-scan only, never opened by BPModLoaderMod)\n")
    write_file(lm .. "/LooseBPMod_P.pak",
        "dummy classic pak (name-scan only, never opened by BPModLoaderMod)\n")
    write_file(UE4SS_ROOT .. "/Mods/BPModLoaderMod/load_order.txt",
        "; round-6 item 3d harness load order\nSampleBPMod\n")
end

--------------------------------------------------------------------
-- Stub + sandbox (mirrors runner.lua; driver-only, runner.lua untouched)
--------------------------------------------------------------------
local make_stub = dofile(HERE .. "/../host-lua-harness/stub-ue4ss.lua")
local env, S = make_stub({
    with_io = false, -- fixture mode installs its own env.io ([GN] section)
    logicmods_fs = { root = FIXTURE, game_dir = GAME_DIR, ue4ss_root = UE4SS_ROOT },
})

local checks, deadline
local watchdog_err
local function hook()
    checks = checks + 1
    if checks % 50 == 1 and os.clock() > deadline then
        watchdog_err = "HARNESS-WATCHDOG: wall-clock limit exceeded"
        error(watchdog_err)
    end
end
local function arm()
    checks = 0
    deadline = os.clock() + WALL_LIMIT
    debug.sethook(hook, "", 200000)
end
local function disarm() debug.sethook() end

local scripts_dir = MAIN:match("^(.*)/[^/]+$") or "."
local mod_root = scripts_dir:match("^(.*)/[Ss]cripts$") or scripts_dir

local function candidates(name)
    local rel = (name:gsub("%.", "/"))
    local list = {}
    local function add(p) list[#list + 1] = p end
    add(scripts_dir .. "/" .. rel .. ".lua")
    add(scripts_dir .. "/" .. rel .. "/" .. rel .. ".lua")
    add(mod_root .. "/" .. rel .. ".lua")
    add(SHARED .. "/" .. rel .. ".lua")
    add(SHARED .. "/" .. rel .. "/" .. rel .. ".lua")
    local pipe = io.popen('ls "' .. SHARED .. '" 2>/dev/null')
    if pipe then
        for dir in pipe:lines() do
            add(SHARED .. "/" .. dir .. "/" .. rel .. ".lua")
        end
        pipe:close()
    end
    return list
end

local module_cache = {}
env.require = function(name)
    local entry = { name = tostring(name) }
    if module_cache[name] ~= nil then
        entry.resolved = "(cached)"
        S.require_calls[#S.require_calls + 1] = entry
        return module_cache[name]
    end
    S.require_calls[#S.require_calls + 1] = entry
    for _, path in ipairs(candidates(name)) do
        local fh = io.open(path, "r")
        if fh then
            local src = fh:read("*a")
            fh:close()
            entry.resolved = path
            local chunk, cerr = load(src, "@" .. path, "t", env)
            if not chunk then error("harness require '" .. name .. "': " .. tostring(cerr)) end
            arm()
            local ok, result = pcall(chunk, name)
            disarm()
            if not ok then error("harness require '" .. name .. "': " .. tostring(result)) end
            module_cache[name] = result
            return result
        end
    end
    entry.error = "not found in harness search paths"
    error(string.format("module '%s' not found:harness search paths", name))
end

env.dofile = function(path)
    S.calls[#S.calls + 1] = { name = "dofile", line = "0", args = tostring(path) }
    S.call_count = S.call_count + 1
    local fh = io.open(tostring(path), "r")
    if not fh then error("harness dofile: not found " .. tostring(path)) end
    local src = fh:read("*a")
    fh:close()
    local chunk, cerr = load(src, "@" .. tostring(path), "t", env)
    if not chunk then error("harness dofile: " .. tostring(cerr)) end
    arm()
    local ok, res = pcall(chunk)
    disarm()
    if not ok then error(res) end
    return res
end
env.loadfile = function(path)
    S.calls[#S.calls + 1] = { name = "loadfile", line = "0", args = tostring(path) }
    S.call_count = S.call_count + 1
    local fh = io.open(tostring(path), "r")
    if not fh then return nil, "harness loadfile: not found" end
    local src = fh:read("*a")
    fh:close()
    return load(src, "@" .. tostring(path), "t", env)
end

--------------------------------------------------------------------
-- Phases
--------------------------------------------------------------------
local report = { load = "?", first_error = nil, keybind_fired = 0, keybind_errors = 0,
                 hook_fired = 0, hook_errors = 0, maphook_fired = 0, maphook_errors = 0 }

local fh = io.open(MAIN, "r")
if not fh then
    print("=== MOD " .. NAME .. " [bpml-driver] ===")
    print("load: MISSING-FILE")
    os.exit(0)
end
local src = fh:read("*a")
fh:close()

local chunk, cerr = load(src, "@" .. MAIN, "t", env)
if not chunk then
    report.load = "SYNTAX-ERROR"
    report.first_error = tostring(cerr)
else
    arm()
    local ok, err = pcall(chunk)
    disarm()
    if not ok then
        report.load = "RUNTIME-ERROR"
        report.first_error = tostring(err)
    else
        report.load = "OK"
    end
end

local function param(name) return env.FindFirstOf(name) end

for i, kb in ipairs(S.keybinds) do
    if type(kb.callback) == "function" then
        arm()
        local ok, err = pcall(kb.callback)
        disarm()
        report.keybind_fired = report.keybind_fired + 1
        if not ok then
            report.keybind_errors = report.keybind_errors + 1
            if not report.first_error then
                report.first_error = "keybind " .. tostring(kb.key) .. ": " .. tostring(err)
            end
        end
    end
end

for i, h in ipairs(S.hooks) do
    if type(h.callback) == "function" then
        arm()
        local ok, err = pcall(h.callback, param("HookSelf"), param("HookParam2"), param("HookParam3"))
        disarm()
        report.hook_fired = report.hook_fired + 1
        if not ok then
            report.hook_errors = report.hook_errors + 1
            if not report.first_error then
                report.first_error = "hook " .. h.path .. ": " .. tostring(err)
            end
        end
    end
end

for i, mh in ipairs(S.map_hooks) do
    if type(mh.callback) == "function" then
        arm()
        local ok, err = pcall(mh.callback, param("Engine"), param("World"))
        disarm()
        report.maphook_fired = report.maphook_fired + 1
        if not ok then
            report.maphook_errors = report.maphook_errors + 1
            if not report.first_error then
                report.first_error = mh.which .. ": " .. tostring(err)
            end
        end
    end
end

--------------------------------------------------------------------
-- Terminal classification + report
--------------------------------------------------------------------
local function prints_have(needle)
    for _, p in ipairs(S.prints) do
        if p:find(needle, 1, true) then return true end
    end
    return false
end

local terminal
if report.load ~= "OK" then
    local last
    for n in tostring(report.first_error):gmatch(":(%d+):") do last = n end
    terminal = "FAILS-AT line " .. (last or "?") .. " (" ..
        (report.load == "SYNTAX-ERROR" and "syntax" or "runtime") .. ")"
elseif report.keybind_errors > 0 or report.hook_errors > 0 or report.maphook_errors > 0 then
    terminal = "LOADS + FIRES-WITH-ERRORS"
elseif prints_have("Executing 'PreBeginPlay'") then
    terminal = "RUNS-TO-TERMINAL: full-load (PreBeginPlay fired)"
elseif prints_have("not present or no matching mods") then
    terminal = "RUNS-TO-TERMINAL: no-logic-mods (load_order absent/empty, 0 mods discovered)"
elseif #S.prints > 0 then
    terminal = "RUNS-TO-TERMINAL: " .. S.prints[#S.prints]:gsub("\n", "\\n")
else
    terminal = "RUNS-TO-TERMINAL: silent (0 mods, load_order present with entries)"
end

print("=== MOD " .. NAME .. " [bpml-driver, logicmods-fs] ===")
print("main: " .. MAIN)
print("fixture: " .. FIXTURE .. " (game dir " .. GAME_DIR .. "), ue4ss-root: " .. UE4SS_ROOT)
print("load: " .. report.load)
if report.first_error then print("first_error: " .. report.first_error) end
print("terminal: " .. terminal)
print(string.format("keybinds: %d fired, %d errors; hooks: %d fired, %d errors; map/beginplay hooks: %d fired, %d errors",
    report.keybind_fired, report.keybind_errors, report.hook_fired, report.hook_errors,
    report.maphook_fired, report.maphook_errors))

local mg = {}
for _, m in ipairs(S.missing) do
    mg[#mg + 1] = string.format("%s x%d (first %s)", m.name, m.count, m.line)
end
print("missing_globals(" .. #mg .. "): " .. (#mg > 0 and table.concat(mg, "; ") or "none"))
print("io_uses(" .. #S.io_uses .. "):")
for _, u in ipairs(S.io_uses) do print("  " .. u) end

local rq = {}
for _, r in ipairs(S.require_calls) do
    rq[#rq + 1] = r.name .. " -> " .. (r.resolved or ("UNRESOLVED (" .. tostring(r.error) .. ")"))
end
print("requires(" .. #rq .. "): " .. (#rq > 0 and table.concat(rq, "; ") or "none"))

if #S.errors > 0 then
    print("async_errors(" .. #S.errors .. "): " .. table.concat(S.errors, " | "))
end
print("ue4ss_calls_recorded: " .. S.call_count)

print("prints(" .. #S.prints .. "):")
for i, p in ipairs(S.prints) do
    if i <= 300 then
        print("  [" .. i .. "] " .. p:gsub("\n", "\\n"))
    end
end
if #S.prints > 300 then print("  ... (" .. (#S.prints - 300) .. " more)") end
print("=== END " .. NAME .. " ===")
