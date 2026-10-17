-- Team I2 host test for probe/zzkbprobe/main.lua (Mac-only, plain Lua).
-- Stubs the few UE4SS globals the probe uses, drives the loops by hand, and
-- checks the log lines. usage: lua test_probe.lua <path to main.lua>
local path = assert(arg[1], "usage: lua test_probe.lua <main.lua>")
local checks, failures = 0, 0
local function check(cond, what)
  checks = checks + 1
  if not cond then failures = failures + 1 print("  FAIL " .. what) end
end

-- size gate: the mod loader admits small diagnostic scripts only (< 4096 B)
local fh = assert(io.open(path, "rb")) local src = fh:read("a") fh:close()
check(#src < 4096, "size " .. #src .. " < 4096")
check(not src:find("[\128-\255]"), "ASCII only")

local function run(scenario)
  local log, loops, queue = {}, {}, {}
  local env = setmetatable({}, {__index = _G})
  env.print = function(s) log[#log + 1] = (s:gsub("\n$", "")) end
  env.FName = function(n) return {name = n} end
  env.LoopAsync = function(ms, fn) loops[#loops + 1] = {ms = ms, fn = fn} end
  env.ExecuteInGameThread = function(fn) queue[#queue + 1] = fn end
  local pc = {valid = true, down = {}, throws = scenario.throws}
  function pc:IsValid() return self.valid end
  function pc:IsInputKeyDown(key)
    if self.throws then error("no such function IsInputKeyDown") end
    return self.down[key.KeyName.name] == true
  end
  package.loaded["UEHelpers"] = {GetPlayerController = function()
    if scenario.no_pc then return {IsValid = function() return false end} end
    return pc end}
  local chunk = assert(load(src, "=main.lua", "t", env))
  chunk()
  local poll_loop, sum_loop
  for _, l in ipairs(loops) do if l.ms == 33 then poll_loop = l elseif l.ms == 10000 then sum_loop = l end end
  check(poll_loop and sum_loop, "two loops registered (33 ms, 10 s)")
  local function frame(n)
    for _ = 1, (n or 1) do
      poll_loop.fn() poll_loop.fn() -- second call must not queue a duplicate
      check(#queue <= 1, "at most one queued poll")
      local q = queue queue = {}
      for _, f in ipairs(q) do f() end
    end
  end
  scenario.drive(pc, frame)
  sum_loop.fn()
  return log
end
local function has(log, pat) for _, l in ipairs(log) do if l:find(pat) then return true end end return false end
local function count(log, pat) local n = 0 for _, l in ipairs(log) do if l:find(pat) then n = n + 1 end end return n end

-- 1. keyboard present: F8 tap, mouse click, controller Cross
local log = run({drive = function(pc, frame)
  frame(3)
  pc.down.F8 = true frame(2) pc.down.F8 = false frame()
  pc.down.LeftMouseButton = true frame() pc.down.LeftMouseButton = nil frame()
  pc.down.Gamepad_FaceButton_Bottom = true frame() pc.down.Gamepad_FaceButton_Bottom = nil frame()
end})
check(has(log, "^%[kb%] Z0 v1 keys=10$"), "startup line")
check(has(log, "^%[kb%] PC found$"), "PC found")
check(has(log, "^%[kb%] CAP fn=1 rt=boolean$"), "CAP fn=1")
check(count(log, "^%[kb%] DN F8 ") == 1 and count(log, "^%[kb%] UP F8 ") == 1, "F8 down/up once")
check(has(log, "^%[kb%] DN LeftMouseButton "), "mouse edge")
check(has(log, "^%[kb%] DN Gamepad_FaceButton_Bottom "), "controller positive control")
check(has(log, "^%[kb%] SUM v1 .* edges=6 fail=0 F8=2 "), "SUM counts")

-- 2. held at start: no phantom edge
log = run({drive = function(pc, frame) pc.down.F8 = true frame(5) end})
check(count(log, " F8 n=") == 0, "no phantom edge when held at start")

-- 3. no keyboard at all: zero edges, no errors
log = run({drive = function(pc, frame) frame(50) end})
check(has(log, "SUM v1 .* fn=1 edges=0 fail=0"), "no keyboard -> zero edges, zero failures")

-- 4. engine function missing: one CAP fn=0 line, stops after 30 failures
log = run({throws = true, drive = function(pc, frame) frame(100) end})
check(count(log, "CAP fn=0") == 1 and has(log, "STOP fail=30") and has(log, "fn=0 edges=0 fail=30"), "fail closed")

-- 5. no player controller: polls continue, no reads
log = run({no_pc = true, drive = function(pc, frame) frame(10) end})
check(has(log, "SUM v1 polls=10 pc=0 fn=%-1 edges=0"), "no controller -> idle")

print(string.format("test_probe: size=%d checks=%d failures=%d", #src, checks, failures))
if checks == 0 or failures > 0 then os.exit(1) end
