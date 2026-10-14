-- Host test of PATCH/zzcrashtest/Scripts/main.lua under stubs.
-- usage: lua lua_mod_host_test.lua <path-to-main.lua>
local path = assert(arg[1], "usage: lua lua_mod_host_test.lua <main.lua>")
local checked, failed = 0, 0
local function check(name, cond, detail)
  checked = checked + 1
  if not cond then failed = failed + 1 end
  io.stderr:write(string.format("%-40s %s %s\n", name, cond and "PASS" or "FAIL", detail or ""))
end

local function run(env_setup)
  local log, calls, loops = {}, {}, {}
  local env = setmetatable({}, {__index = _G})
  env.print = function(s) log[#log + 1] = s end
  env.LoopAsync = function(ms, fn) loops[#loops + 1] = {ms = ms, fn = fn} end
  env_setup(env, calls)
  local chunk = assert(loadfile(path, "t", env))
  chunk()
  -- drive the loop synchronously until it returns true (cap 1000 ticks)
  local ticks = 0
  for _, l in ipairs(loops) do
    while ticks < 1000 do
      ticks = ticks + 1
      local ok, done = pcall(l.fn)
      if not ok then log[#log + 1] = "LOOP-ERROR " .. tostring(done) break end
      if done then break end
    end
  end
  return log, calls, loops, ticks
end

local function joined(log) return table.concat(log, "") end

-- 1. runtime WITHOUT the patch: no function -> says so, no loop, no crash call
do
  local log, calls, loops = run(function(env) env.ModBridgeCrashTest = nil end)
  check("missing-function: no loop scheduled", #loops == 0)
  check("missing-function: message printed", joined(log):find("ModBridgeCrashTest is missing", 1, true) ~= nil)
end

-- 2. patched runtime, simulated: refusals return false,msg; valid request records a "crash"
do
  local log, calls, loops, ticks = run(function(env, calls)
    env.ModBridgeCrashTest = function(kind, token)
      calls[#calls + 1] = {kind = kind, token = token}
      if token ~= "CRASH-THE-GAME-NOW" then return false, "refused: token (nothing happened)" end
      if kind ~= "segv" and kind ~= "trap" then return false, "refused: kind (nothing happened)" end
      error("SIMULATED-CRASH " .. kind)   -- stands in for the process dying
    end
  end)
  local s = joined(log)
  check("gate-check PASS printed", s:find("GATE-CHECK PASS", 1, true) ~= nil)
  check("one loop at 5000 ms", #loops == 1 and loops[1].ms == 5000)
  check("exactly 3 calls (2 refusals + 1 crash)", #calls == 3, "calls=" .. #calls)
  check("call 1 = wrong token", calls[1] and calls[1].token == "wrong-token")
  check("call 2 = bad kind", calls[2] and calls[2].kind == "bogus-kind")
  check("call 3 = segv + token", calls[3] and calls[3].kind == "segv" and calls[3].token == "CRASH-THE-GAME-NOW")
  check("crash after 30 ticks (~150 s)", ticks == 30, "ticks=" .. ticks)
  check("CRASHING NOW marker printed", s:find("CRASHING NOW", 1, true) ~= nil)
end

-- 3. a runtime whose refusal misbehaves (returns true) -> must NOT schedule a crash
do
  local log, calls, loops = run(function(env, calls)
    env.ModBridgeCrashTest = function(kind, token) calls[#calls + 1] = kind return true, "?" end
  end)
  check("bad-gate: no loop scheduled", #loops == 0)
  check("bad-gate: GATE-CHECK FAIL printed", joined(log):find("GATE-CHECK FAIL", 1, true) ~= nil)
end

io.stderr:write(string.format("lua_mod_host_test: checked %d, failed %d\n", checked, failed))
os.exit((checked > 0 and failed == 0) and 0 or 1)
