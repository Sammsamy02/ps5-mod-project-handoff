-- runner.lua — load one UE4SS mod against stub-ue4ss.lua and report.
-- Agent FU, 2026-10-10. Lua 5.5.1 host, 5.2-5.5 common subset only.
--
-- Usage:
--   lua5.5 runner.lua --main <path/to/main.lua> --name <Label> \
--        [--with-io] [--shared <Mods/shared dir>] [--wall 20] [--instr 100000000]
--
-- Phases per mod (each pcall-wrapped, watchdog-protected):
--   1. compile (syntax)      2. run main chunk (the mod's startup path)
--   3. fire every registered keybind callback once (incl. RegisterKeyBindAsync)
--   4. fire every RegisterHook callback once (self-param stub)
--   5. fire every console-command handler once with sample args
--   6. fire LoadMap pre/post + BeginPlayPost hooks once (param stubs)
--   7. fire NotifyOnNewObject callbacks once
--   Custom events (RegisterCustomEvent) are NEVER fired: on console no BP
--   caller exists, so firing them would test a path that cannot happen.
--
-- Output: one markdown block on stdout, parsed/collected by run-all.sh.

local function arg_of(name, default)
    for i = 1, #arg do
        if arg[i] == name then return arg[i + 1] end
    end
    return default
end
local FLAG = {}
for i = 1, #arg do if arg[i]:sub(1, 2) == "--" then FLAG[arg[i]] = true end end

local MAIN = assert(arg_of("--main"), "--main required")
local NAME = arg_of("--name") or MAIN
local WITH_IO = not not FLAG["--with-io"]
local SHARED = arg_of("--shared")
local WALL_LIMIT = tonumber(arg_of("--wall", "20")) or 20
local INSTR_LIMIT = tonumber(arg_of("--instr", "100000000")) or 100000000

local HERE = (arg[0]:match("^(.*)/[^/]+$") or ".")
local make_stub = dofile(HERE .. "/stub-ue4ss.lua")

local env, S = make_stub({ with_io = WITH_IO })

--------------------------------------------------------------------
-- Watchdog (instruction-count + wall clock, host-side)
--------------------------------------------------------------------
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

--------------------------------------------------------------------
-- Sandbox require with UE4SS-ish search paths
--------------------------------------------------------------------
local scripts_dir = MAIN:match("^(.*)/[^/]+$") or "."
local mod_root = scripts_dir:match("^(.*)/[Ss]cripts$") or scripts_dir

local function candidates(name)
    local rel = (name:gsub("%.", "/"))
    local list = {}
    local function add(p) list[#list + 1] = p end
    add(scripts_dir .. "/" .. rel .. ".lua")
    add(scripts_dir .. "/" .. rel .. "/" .. rel .. ".lua")
    add(mod_root .. "/" .. rel .. ".lua")
    if SHARED then
        add(SHARED .. "/" .. rel .. ".lua")
        add(SHARED .. "/" .. rel .. "/" .. rel .. ".lua")
        -- shared/<any>/<name>.lua (jsbProfiler/jsbProfi.lua pattern)
        local pipe = io.popen('ls "' .. SHARED .. '" 2>/dev/null')
        if pipe then
            for dir in pipe:lines() do
                add(SHARED .. "/" .. dir .. "/" .. rel .. ".lua")
            end
            pipe:close()
        end
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
            if not chunk then
                entry.error = "syntax: " .. tostring(cerr)
                error("harness require '" .. name .. "': " .. tostring(cerr))
            end
            arm()
            local ok, result = pcall(chunk, name)
            disarm()
            if not ok then
                entry.error = tostring(result)
                error("harness require '" .. name .. "': " .. tostring(result))
            end
            module_cache[name] = result
            return result
        end
    end
    entry.error = "not found in harness search paths"
    error(string.format("module '%s' not found:harness search paths (scripts dir, mod root, Mods/shared)", name))
end

-- dofile/loadfile restricted to the mod tree (port ships native restricted
-- loaders bound to the mod's scripts dir — LuaMod.cpp setup_custom_module_loader)
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
local function line_of(msg)
    -- innermost frame = LAST "file:line:" in the error chain; skip the
    -- harness's own wrapper lines by preferring the last match.
    local last
    for n in tostring(msg):gmatch(":(%d+):") do last = n end
    return last or "?"
end

local report = { load = "?", first_error = nil, keybind_fired = 0, keybind_errors = 0,
                 hook_fired = 0, hook_errors = 0, console_fired = 0, console_errors = 0,
                 maphook_fired = 0, maphook_errors = 0, notify_fired = 0 }

-- read main source (host io, not the sandbox's)
local fh = io.open(MAIN, "r")
if not fh then
    print("=== MOD " .. NAME .. " [io=" .. (WITH_IO and "on" or "off") .. "] ===")
    print("load: MISSING-FILE")
    os.exit(0)
end
local src = fh:read("*a")
fh:close()


-- 1. compile
local chunk, cerr = load(src, "@" .. MAIN, "t", env)
if not chunk then
    report.load = "SYNTAX-ERROR"
    report.first_error = tostring(cerr)
else
    -- 2. run startup chunk
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

-- 3. keybinds
for i, kb in ipairs(S.keybinds) do
    if type(kb.callback) == "function" then
        arm()
        local ok, err = pcall(kb.callback)
        disarm()
        report.keybind_fired = report.keybind_fired + 1
        if not ok then
            report.keybind_errors = report.keybind_errors + 1
            if not report.first_error then report.first_error = "keybind " .. kb.key .. ": " .. tostring(err) end
        end
    end
end

-- 4. hooks (self + extra params as RemoteUnrealParam-style cells)
local function param(name) return env.FindFirstOf(name) end
for i, h in ipairs(S.hooks) do
    if type(h.callback) == "function" then
        arm()
        local ok, err = pcall(h.callback, param("HookSelf"), param("HookParam2"), param("HookParam3"))
        disarm()
        report.hook_fired = report.hook_fired + 1
        if not ok then
            report.hook_errors = report.hook_errors + 1
            if not report.first_error then report.first_error = "hook " .. h.path .. ": " .. tostring(err) end
        end
    end
end

-- 5. console handlers
for i, c in ipairs(S.console_handlers) do
    if type(c.callback) == "function" then
        local params = { "Engine", "bUseSplitscreen", "true" }
        arm()
        local ok, err = pcall(c.callback, c.cmd .. " Engine bUseSplitscreen true", params, param("FOutputDevice_Ar"))
        disarm()
        report.console_fired = report.console_fired + 1
        if not ok then
            report.console_errors = report.console_errors + 1
            if not report.first_error then report.first_error = "console " .. c.cmd .. ": " .. tostring(err) end
        end
    end
end

-- 6. map hooks
for i, mh in ipairs(S.map_hooks) do
    if type(mh.callback) == "function" then
        arm()
        local ok, err = pcall(mh.callback, param("Engine"), param("World"))
        disarm()
        report.maphook_fired = report.maphook_fired + 1
        if not ok then
            report.maphook_errors = report.maphook_errors + 1
            if not report.first_error then report.first_error = mh.which .. ": " .. tostring(err) end
        end
    end
end

-- 7. NotifyOnNewObject
for i, cb in ipairs(S.new_object_cbs) do
    if type(cb) == "function" then
        arm()
        local ok, err = pcall(cb, param("NewObject"))
        disarm()
        report.notify_fired = report.notify_fired + 1
        if not ok and not report.first_error then
            report.first_error = "NotifyOnNewObject: " .. tostring(err)
        end
    end
end

--------------------------------------------------------------------
-- Verdict
--------------------------------------------------------------------
local io_gap, os_gap = false, false
for _, m in ipairs(S.missing) do
    if m.name == "io" then io_gap = true end
    if m.name == "os" then os_gap = true end
end

local verdict
if report.load == "SYNTAX-ERROR" then
    verdict = "FAILS-AT line " .. line_of(report.first_error) .. " (syntax)"
elseif report.load == "RUNTIME-ERROR" then
    verdict = "FAILS-AT line " .. line_of(report.first_error) .. " (runtime)"
else
    local gaps = {}
    if io_gap then gaps[#gaps + 1] = "IO" end
    if os_gap then gaps[#gaps + 1] = "OS" end
    for _, m in ipairs(S.missing) do
        if m.name ~= "io" and m.name ~= "os" then gaps[#gaps + 1] = m.name end
    end
    if #gaps == 0 then verdict = "LOADS-CLEAN"
    else verdict = "LOADS (gap-reads: " .. table.concat(gaps, ",") .. ")" end
    if report.keybind_errors > 0 or report.hook_errors > 0 or report.console_errors > 0 then
        verdict = verdict .. " + FIRES-WITH-ERRORS"
    end
end

--------------------------------------------------------------------
-- Emit block
--------------------------------------------------------------------
print("=== MOD " .. NAME .. " [io=" .. (WITH_IO and "shim-on" or "off") .. "] ===")
print("load: " .. report.load)
if report.first_error then print("first_error: " .. report.first_error) end
print("verdict: " .. verdict)
print("predicted_console: " ..
    (report.load == "OK" and "see verdict above" or verdict))

local mg = {}
for _, m in ipairs(S.missing) do
    mg[#mg + 1] = string.format("%s x%d (first %s)", m.name, m.count, m.line)
end
print("missing_globals(" .. #mg .. "): " .. (#mg > 0 and table.concat(mg, "; ") or "none"))

print("io_uses(" .. #S.io_uses .. "): " .. (#S.io_uses > 0 and table.concat(S.io_uses, "; ") or "none"))
print("os_uses(" .. #S.os_uses .. "): " .. (#S.os_uses > 0 and table.concat(S.os_uses, "; ") or "none"))

local rq = {}
for _, r in ipairs(S.require_calls) do
    rq[#rq + 1] = r.name .. " -> " .. (r.resolved or ("UNRESOLVED (" .. tostring(r.error) .. ")"))
end
print("requires(" .. #rq .. "): " .. (#rq > 0 and table.concat(rq, "; ") or "none"))

print(string.format("keybinds: %d registered, %d fired, %d errors",
    #S.keybinds, report.keybind_fired, report.keybind_errors))
print(string.format("hooks: %d registered, %d fired, %d errors",
    #S.hooks, report.hook_fired, report.hook_errors))
print(string.format("console_handlers: %d registered, %d fired, %d errors",
    #S.console_handlers, report.console_fired, report.console_errors))
print(string.format("map/beginplay hooks: %d, fired %d, errors %d; NotifyOnNewObject: %d fired, custom_events: %d (never fired)",
    #S.map_hooks, report.maphook_fired, report.maphook_errors, report.notify_fired, #S.custom_events))
print("ue4ss_calls_recorded: " .. S.call_count)
if #S.errors > 0 then
    print("async_errors(" .. #S.errors .. "): " .. table.concat(S.errors, " | "))
end
local np = math.min(6, #S.prints)
if np > 0 then
    local pp = {}
    for i = 1, np do pp[i] = S.prints[i]:gsub("\n", "\\n") end
    print("prints(first " .. np .. "): " .. table.concat(pp, " || "))
end
print("=== END " .. NAME .. " ===")
