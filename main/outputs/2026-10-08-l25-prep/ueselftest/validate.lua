-- validate.lua -- host validation driver for the ueselftest mod.
-- Agent GB, 2026-10-10. Lua 5.5.1 host, no console contact.
--
-- Runs the generated main.lua + parts against the extended
-- host-lua-harness/stub-ue4ss.lua in three scenarios:
--   S1 PRIMARY (stub io=on, GA path shim): load, fire the arm keybind,
--      assert SUMMARY <M> of <M> PASS, zero FAIL lines, 'done', the
--      io-roundtrip host shape, restored property writes, and a clean
--      SECOND arm press (no part re-append, still M of M).
--   S2 RESUME DRILL (in-memory io mock, progress.txt pre-seeded "5"):
--      assert '[ST2] 5/<M> <name> CRASH (resumed)', entries 1..4 not run,
--      run resumes at 6, progress file ends at the -1 completed marker.
--   S3 IO-OFF (stub io=off): exactly the os.clock/os.date FAILs and the
--      io-roundtrip SKIP; everything else passes.
--
-- Exit code 0 = all scenarios pass; 1 = any assertion failed (printed).

local HERE = (arg and arg[0] or debug.getinfo(1, "S").source:gsub("^@", "")):match("^(.*)/[^/]+$") or "."
local HARNESS = HERE .. "/../host-lua-harness"
local MODSRC = HERE

local function log(s) print("[GB-VAL] " .. tostring(s)) end

-- ---------------------------------------------------------------- staging
local STAGE = os.getenv("ST2_STAGE") or ("/tmp/st2-host-" .. tostring(os.time()))
local SCRIPTS = STAGE .. "/ue4ss/Mods/ueselftest/Scripts"
os.execute("mkdir -p '" .. SCRIPTS .. "'")
for _, f in ipairs({ "main.lua" }) do
  local src = assert(io.open(MODSRC .. "/" .. f, "r")):read("*a")
  assert(io.open(SCRIPTS .. "/" .. f, "w")):write(src):close()
end
-- parts: discover partN.lua
local parts = {}
for n = 2, 40 do
  local p = MODSRC .. "/part" .. n .. ".lua"
  local fh = io.open(p, "r")
  if not fh then break end
  local src = fh:read("*a") fh:close()
  assert(io.open(SCRIPTS .. "/part" .. n .. ".lua", "w")):write(src):close()
  parts[#parts + 1] = n
end
assert(#parts >= 1, "no part files found next to main.lua")
-- [IK] host-only UEHelpers stand-in for the UEHelpers.GetWorld probe
-- (part11 entry 5): on console require("UEHelpers") resolves via
-- ue4ss/Mods/shared (the spawner's proven require shape); the host has
-- no shared dir, so stage a minimal stub beside the parts so the probe
-- exercises the same pcall/require path. Staged into STAGE only —
-- never part of the console deploy tree.
assert(io.open(SCRIPTS .. "/UEHelpers.lua", "w")):write(
  "-- host stub (validate.lua staging only; not deployed)\n" ..
  "local M = {}\n" ..
  "function M.GetWorld() return FindFirstOf(\"World\") end\n" ..
  "return M\n"):close()
-- enabled.txt (mod marker; harness does not need it, deploy does)
assert(io.open(STAGE .. "/ue4ss/Mods/ueselftest/enabled.txt", "w")):write(""):close()
log("staged " .. #parts .. " parts under " .. SCRIPTS)

local make_stub = dofile(HARNESS .. "/stub-ue4ss.lua")

-- ------------------------------------------------------------- env shims
-- require/dofile/loadfile bound to the STAGED scripts dir (runner.lua
-- pattern; host io, independent of the sandbox env's io).
local function env_shims(env)
  local cache = {}
  env.require = function(name)
    if cache[name] ~= nil then return cache[name] end
    local path = SCRIPTS .. "/" .. (name:gsub("%.", "/")) .. ".lua"
    local fh = io.open(path, "r")
    if not fh then error("validator require: not found " .. tostring(name)) end
    local src = fh:read("*a") fh:close()
    local chunk, err = load(src, "@" .. path, "t", env)
    if not chunk then error("validator require: " .. tostring(err)) end
    local ok, res = pcall(chunk, name)
    if not ok then error("validator require: " .. tostring(res)) end
    cache[name] = res
    return res
  end
  env.dofile = function(path)
    local fh = io.open(SCRIPTS .. "/" .. tostring(path), "r")
    if not fh then error("validator dofile: not found " .. tostring(path)) end
    local src = fh:read("*a") fh:close()
    local chunk, err = load(src, "@" .. SCRIPTS .. "/" .. tostring(path), "t", env)
    if not chunk then error("validator dofile: " .. tostring(err)) end
    local ok, res = pcall(chunk)
    if not ok then error("validator dofile: " .. tostring(res)) end
    return res
  end
  env.loadfile = function(path)
    local fh = io.open(SCRIPTS .. "/" .. tostring(path), "r")
    if not fh then return nil, "validator loadfile: not found" end
    local src = fh:read("*a") fh:close()
    return load(src, "@" .. SCRIPTS .. "/" .. tostring(path), "t", env)
  end
end

local function load_main(env)
  local fh = assert(io.open(SCRIPTS .. "/main.lua", "r"))
  local src = fh:read("*a") fh:close()
  local chunk, err = load(src, "@" .. SCRIPTS .. "/main.lua", "t", env)
  assert(chunk, "main.lua compile: " .. tostring(err))
  local ok, err2 = pcall(chunk)
  assert(ok, "main.lua load: " .. tostring(err2))
end

local function fire_keybinds(state)
  for _, kb in ipairs(state.keybinds) do
    if type(kb.callback) == "function" then
      local ok, err = pcall(kb.callback)
      assert(ok, "keybind " .. kb.key .. " fired with error: " .. tostring(err))
    end
  end
end

-- in-memory io mock for the resume drill (plain open/read/write/close/lines)
local function mock_io()
  local files = {}
  local mt = {}
  mt.__index = function(h, k)
    if k == "write" then
      return function(s, ...)
        local n = select("#", ...)
        for i = 1, n do files[s.__p] = (files[s.__p] or "") .. tostring(select(i, ...)) end
        return s
      end
    end
    if k == "read" then
      return function(s, fmt)
        local c = files[s.__p] or ""
        if c == "" then return nil end
        if fmt == "l" then return c:match("^[^\n]*") end
        return c
      end
    end
    if k == "close" then return function(s) return true end end
    if k == "lines" then return function(s) return function() return nil end end end
    return nil
  end
  return {
    open = function(path, mode)
      local m = tostring(mode or "r")
      if m:sub(1, 1) == "r" then
        if files[path] == nil then return nil, "mock: not found" end
        return setmetatable({ __p = path }, mt)
      end
      if m:sub(1, 1) == "w" then files[path] = "" end
      return setmetatable({ __p = path }, mt)
    end,
    files = files,
  }
end

-- --------------------------------------------------------------- asserts
local failures = 0
local function check(cond, msg)
  if cond then
    log("PASS " .. msg)
  else
    failures = failures + 1
    log("FAIL " .. msg)
  end
end

-- PLAIN substring find (call-site strings contain '-', '(' etc which are
-- Lua pattern magic; never use pattern mode here).
local function find_line(prints, pat)
  for _, l in ipairs(prints) do if l:find(pat, 1, true) then return l end end
  return nil
end

local function count_lines(prints, pat)
  local n = 0
  for _, l in ipairs(prints) do if l:find(pat, 1, true) then n = n + 1 end end
  return n
end

-- count NUMBERED outcome lines only ("[ST2] <idx>/<M> <name> FAIL|SKIP ..."),
-- excluding the unnumbered static-skip accounting lines ("[ST2] SKIP <API>").
local function count_numbered(prints, word)
  local n = 0
  for _, l in ipairs(prints) do
    if l:match("^%[ST2%] %d+/%d+ ") and l:find(" " .. word .. " ", 1, true) then
      n = n + 1
    end
  end
  return n
end

-- =============================================================== S1 PRIMARY
log("--- S1: primary run (stub io=on, GA path shim, staged ue4ss tree)")
do
  local env, st = make_stub({ with_io = true })
  env_shims(env)
  load_main(env)
  check(find_line(st.prints, "boot v1 M=") ~= nil, "S1 boot line present")
  check(find_line(st.prints, "armed=true") ~= nil, "S1 arm registered at load")
  fire_keybinds(st)
  check(find_line(st.prints, "K1 f12") ~= nil and find_line(st.prints, "K2 q") ~= nil, "S1 arm fired K1/K2")
  local summary = find_line(st.prints, "SUMMARY ")
  log("S1 summary: " .. tostring(summary))
  check(summary ~= nil, "S1 summary line present")
  local n, m
  if summary then n, m = summary:match("SUMMARY (%d+) of (%d+) PASS") end
  check(n ~= nil and m ~= nil, "S1 summary parseable")
  local nfails = count_numbered(st.prints, "FAIL")
  check(nfails == 0, "S1 zero numbered FAIL lines (got " .. nfails .. ")")
  local nskips = count_numbered(st.prints, "SKIP")
  check(nskips == 0, "S1 zero numbered SKIP lines (got " .. nskips .. ")")
  check(n == m, "S1 M of M PASS (got " .. tostring(n) .. " of " .. tostring(m) .. ")")
  check(find_line(st.prints, "done") ~= nil, "S1 done marker")
  local iort = find_line(st.prints, "io-roundtrip")
  log("S1 io-roundtrip: " .. tostring(iort))
  check(iort ~= nil and iort:find("w-only open-r-miss", 1, true) ~= nil,
        "S1 io-roundtrip host shape (w-only open-r-miss)")
  check(find_line(st.prints, "newindex-write-restore PASS restored") ~= nil, "S1 newindex write restored")
  check(find_line(st.prints, "SetPropertyValue-restore PASS restored") ~= nil, "S1 SetPropertyValue restored")
  check(find_line(st.prints, "RegisterHook PASS") ~= nil, "S1 RegisterHook ok")
  check(find_line(st.prints, "UnregisterHook PASS") ~= nil, "S1 UnregisterHook ok")
  check(find_line(st.prints, "hygiene-audit PASS all-") ~= nil, "S1 hygiene audit clean")
  check(find_line(st.prints, "skip-class-list PASS 27-skips") ~= nil, "S1 static-skip accounting (27)")
  check(find_line(st.prints, "WARN count") == nil, "S1 no part-count warning")
  -- restored property writes (stub-level evidence)
  local bykey, writecount = {}, 0
  local restored_ok = true
  for _, w in ipairs(st.prop_writes) do
    writecount = writecount + 1
    local e = bykey[w.key]
    if e == nil then
      bykey[w.key] = w
    elseif not (rawequal(e.val, w.val) or e.val == w.val) then
      restored_ok = false
      log("S1 divergent writes to " .. tostring(w.key))
    end
    local cur = rawget(w.obj, w.prop)
    if not (cur == w.val or rawequal(cur, w.val)) then
      restored_ok = false
      log("S1 unrestored final state on " .. tostring(w.key))
    end
  end
  check(writecount >= 1, "S1 property writes recorded (" .. writecount .. ")")
  check(restored_ok, "S1 all recorded writes restored (same value, stable final state)")
  -- second arm press: fresh full run, no part re-append
  fire_keybinds(st)
  local summary2 = find_line(st.prints, "SUMMARY ")
  log("S1 second-run summary: " .. tostring(summary2))
  check(summary2 ~= nil and m ~= nil and summary2:match("of (%d+) PASS") == m, "S1 second run same M")
  check(count_lines(st.prints, "WARN count") == 0, "S1 no re-append after second press")
  -- missing globals: with io=on nothing should be missing
  local missing = {}
  for _, mg in ipairs(st.missing) do missing[#missing + 1] = mg.name end
  check(#missing == 0, "S1 zero missing globals (got: " .. table.concat(missing, ",") .. ")")
end

-- ============================================================ S2 RESUME DRILL
log("--- S2: crash-resume drill (mock io, progress.txt seeded with index 5)")
do
  local env, st = make_stub({ with_io = false }) -- stub io off; we inject a mock
  env_shims(env)
  local mio = mock_io()
  env.io = mio
  env.os = env.os or { clock = function() return 0 end, date = function() return "x" end,
                       remove = function(p) mio.files[p] = nil return true end }
  load_main(env)
  -- PFIND already ran at load (created an empty progress file); seed a crash
  -- marker as if entry 5 had been about to run when the process died.
  local seeded = false
  for p in pairs(mio.files) do
    if p:find("progress.txt", 1, true) then mio.files[p] = "5\n"; seeded = true end
  end
  check(seeded, "S2 progress file found and seeded (5)")
  fire_keybinds(st)
  local boot = find_line(st.prints, "boot v1 M=")
  local m = boot and tonumber(boot:match("M=(%d+)"))
  check(m ~= nil, "S2 boot M parsed")
  local crash = find_line(st.prints, "CRASH (resumed)")
  log("S2 crash line: " .. tostring(crash))
  check(crash ~= nil, "S2 CRASH (resumed) line present")
  check(crash ~= nil and crash:find("5/" .. tostring(m) .. " ", 1, true) ~= nil,
        "S2 crash line names index 5")
  check(crash ~= nil and not crash:find("?", 1, true), "S2 crash line resolved the entry NAME (not '?')")
  if m then
    for i = 1, 4 do
      check(find_line(st.prints, " " .. i .. "/" .. m .. " ") == nil,
            "S2 entry " .. i .. " not run before resume point")
    end
    check(find_line(st.prints, "6/" .. m .. " ") ~= nil, "S2 run resumed at entry 6")
  end
  local summary = find_line(st.prints, "SUMMARY ")
  log("S2 summary: " .. tostring(summary))
  check(summary ~= nil, "S2 summary present")
  -- diagnostics: dump every numbered non-pass line (resume expectation:
  -- exactly the CRASH line; anything else is a scenario-env gap to fix)
  for _, l in ipairs(st.prints) do
    if l:match("^%[ST2%] %d+/%d+ ") and (l:find(" FAIL ", 1, true) or l:find(" SKIP ", 1, true)) then
      log("S2 non-pass: " .. l:gsub("\n", ""))
    end
  end
  local n2, m2
  if summary then n2, m2 = summary:match("SUMMARY (%d+) of (%d+) PASS") end
  check(n2 ~= nil and m2 ~= nil, "S2 summary parseable")
  -- resume arithmetic: the progress marker named entry 5 as about-to-run;
  -- entries 1..4 completed in the PREVIOUS session (never re-run) and the
  -- crashed entry never passes, so a clean resumed session = M - 5 passes
  -- with zero numbered FAIL/SKIP lines.
  if n2 and m2 then
    check(tonumber(n2) == tonumber(m2) - 5, "S2 resumed pass count = M - 5 (4 prior-session + 1 crashed)")
    check(count_numbered(st.prints, "FAIL") == 0 and count_numbered(st.prints, "SKIP") == 0,
          "S2 resumed session has zero numbered FAIL/SKIP lines")
  end
  -- completed marker written
  local done_marker = false
  for p, c in pairs(mio.files) do
    if p:find("progress.txt", 1, true) and c == "-1\n" then done_marker = true end
  end
  check(done_marker, "S2 progress file ended at the -1 completed marker")
  check(find_line(st.prints, "done") ~= nil, "S2 done marker")
end

-- ================================================================ S3 IO-OFF
log("--- S3: io=off run (io/os absent: documented degradation)")
do
  local env, st = make_stub({ with_io = false })
  env_shims(env)
  load_main(env)
  check(find_line(st.prints, "progress-io unavailable") ~= nil, "S3 progress-io unavailable logged")
  fire_keybinds(st)
  local summary = find_line(st.prints, "SUMMARY ")
  log("S3 summary: " .. tostring(summary))
  check(summary ~= nil, "S3 summary present")
  local fails = count_numbered(st.prints, "FAIL")
  local skips = count_numbered(st.prints, "SKIP")
  check(fails == 2, "S3 exactly 2 numbered FAILs (os.clock, os.date); got " .. fails)
  check(skips == 1, "S3 exactly 1 numbered SKIP (io-roundtrip no-io-path); got " .. skips)
  check(find_line(st.prints, "os.clock FAIL") ~= nil and find_line(st.prints, "os.date FAIL") ~= nil,
        "S3 the 2 FAILs are os.clock/os.date")
  check(find_line(st.prints, "io-roundtrip SKIP no-io-path") ~= nil, "S3 io-roundtrip SKIP no-io-path")
  local n3, m3
  if summary then n3, m3 = summary:match("SUMMARY (%d+) of (%d+) PASS") end
  check(n3 ~= nil and tonumber(n3) == tonumber(m3) - 2 - 1, "S3 pass count = M - 2 - 1")
end

log("---")
if failures == 0 then
  log("ALL SCENARIOS PASS")
  os.exit(0)
else
  log(failures .. " CHECK(S) FAILED")
  os.exit(1)
end
