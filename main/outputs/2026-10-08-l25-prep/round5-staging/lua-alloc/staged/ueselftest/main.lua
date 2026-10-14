-- ueselftest v1.1 (agent GB gen 1.0; Claude round-5 item 6a; IU round-13
-- GROUP-C expectations corrected to upstream, GROUP-D desync-tolerant LP,
-- item-6 one-run-per-session hygiene): parity self-test from FS
-- API-PARITY-TABLE.md. LAWS: pcall every call; ONE log line/call;
-- progress.txt marker BEFORE each call (crash-resume); engine base classes
-- only; NO FindFunction/bpfn/class-children; no spawns/saves/constructs;
-- writes restore+verify; hooks unregister; ONE completed run per session.
local M = 168
local T = {}
local S = {h = {}, hyd = {}, n = 0, p = 0, busy = false}
local function P(s) print("[ST2] " .. tostring(s) .. "\n") end
S.log = P
local function TS(v) local ok, s = pcall(tostring, v) if not ok then return "e" end s = s:gsub("^.-:%d+:%s*", "") return (s:gsub("%c", " ")):sub(1, 40) end
S.et = TS
function S.v(o) if o == nil then return false end local ok, r = pcall(function() return o:IsValid() end) return ok and r and true or false end
function S.g(nm) local c = S.h["g." .. nm] if c ~= nil then return c or nil end local ok, o = pcall(FindFirstOf, nm) if ok and o ~= nil and S.v(o) then S.h["g." .. nm] = o return o end S.h["g." .. nm] = false return nil end
-- progress file (mod folder; resume off if io unavailable). ROOT = deploy constant.
local ROOT = "/data/modbridge/roots/PPSA33286/ue4ss/Mods/ueselftest"
local PF
local function PFIND()
  if type(io) ~= "table" or type(io.open) ~= "function" then return nil end
  local ok, i = pcall(debug.getinfo, 1, "S")
  local sd = (ok and i and type(i.source) == "string") and (i.source:gsub("^@", ""):match("^(.*)/[^/]+$") or "") or ""
  local cand = {ROOT .. "/progress.txt", sd .. "/../progress.txt"}
  for _, p in ipairs(cand) do local o, f = pcall(io.open, p, "w") if o and f then f:close() return p end end
  return nil
end
local function PFW(n) if PF then local o, f = pcall(io.open, PF, "w") if o and f then f:write(tostring(n) .. "\n") f:close() end end end
local function PFR() if not PF then return nil end local o, f = pcall(io.open, PF, "r") if not o or not f then return nil end local l = f:read("l") f:close() if l == "-1" then return nil end return tonumber(l) end
local SLOW = 0
local function ONE(e)
  S.n=S.n+1 PFW(S.n)
  local t0=os.clock() local ok,a,b=pcall(e[2],S) local ms=(os.clock()-t0)*1000
  local sl=ms>16 and " SLOW-ms="..string.format("%.0f",ms) or "" if ms>16 then SLOW=SLOW+1 end
  local l=S.n.."/"..M.." "..e[1].." "
  if not ok then P(l.."FAIL "..S.et(a)..sl)
  elseif a == "SKIP" then P(l .. "SKIP " .. TS(b))
  elseif a == false then P(l.."FAIL "..TS(b)..sl)
  else S.p = S.p + 1 P(l.."PASS"..((a~=nil and a~=true) and (" "..TS(a)) or "")..sl) end
end
local function LP(k, cnt)
  if S.h["lp" .. k] then return end
  local nm = "part" .. k local ok, t = pcall(require, nm)
  if not ok or type(t) ~= "table" then ok, t = pcall(dofile, nm .. ".lua") end
  if not ok or type(t) ~= "table" then ok, t = pcall(dofile, "ue4ss/Mods/ueselftest/Scripts/" .. nm .. ".lua") end
  if ok and type(t) == "table" and #t == cnt then
    for i = 1, cnt do T[#T + 1] = t[i] end P(nm .. " loaded " .. cnt)
  elseif ok and type(t) == "table" and #t >= 1 then
    -- [IU-GD] deployed part file is an older/newer generation than this
    -- main.lua expects (2026-10-11 console: new main + stale 5-entry part11).
    -- The require path itself WORKED (a table came back) — run the entries
    -- that exist and surface the desync loudly instead of hiding 6 stub FAILs.
    for i = 1, #t do T[#T + 1] = t[i] end
    P(nm .. " DESYNC loaded " .. #t .. " expected " .. cnt .. " (redeploy all ueselftest files)")
    for i = #t + 1, cnt do T[#T + 1] = {nm .. "-missing-" .. i, function() return false, "part-entry-missing-desync" end} end
  else
    P(nm .. " LOAD-FAIL " .. TS(t))
    for i = 1, cnt do T[#T + 1] = {nm .. "-" .. i, function() return false, "part-not-loaded" end} end
  end
  S.h["lp" .. k] = true
end
local function RUN()
  if S.busy then P("busy") return end
  -- [IU hygiene item 6] ONE completed run per session. Re-triggers (extra
  -- F12 presses / re-armed AUTO) are ignored: every re-run re-registers the
  -- no-unregister callback families (LoadMap/InitGameState/BeginPlay/EndPlay/
  -- ConsoleExec/...) and was the measured cause of run-3 "not enough memory"
  -- registry exhaustion in CLAUDE-SELFTEST-RESULTS GROUP A.
  if S.done then P("already-done-once-this-session") return end
  S.busy = true
  -- [IS-LUA-ALLOC 0003] bracket every run with exact allocator numbers
  -- (Claude round-13 item 1). GetLuaMemory is the 0001 engine binding; when
  -- absent (older PRX) the line degrades to "unavailable" and changes nothing.
  local okm, m = pcall(function() return GetLuaMemory() end)
  P("MEM before " .. (okm and tostring(m) or "unavailable"))
  P("RUN start 1.." .. M)
  S.p = 0
  LP(2, 21)
  LP(3, 22)
  LP(4, 23)
  LP(5, 15)
  LP(6, 14)
  LP(7, 13)
  LP(8, 16)
  LP(9, 22)
  LP(10, 14)
  LP(11, 6)
  LP(12, 1)
  if #T ~= M then P("WARN count " .. #T .. " expected " .. M) end
  local cr = PFR()
  if cr then P(cr .. "/" .. M .. " " .. (T[cr] and T[cr][1] or "?") .. " CRASH (resumed)") end
  S.n = cr or 0
  for i = S.n + 1, #T do ONE(T[i]) end
  PFW(-1)
  P("SUMMARY "..S.p.." of "..M.." PASS SLOW "..SLOW)
  -- [IS-LUA-ALLOC 0003] post-run numbers: gc_kb delta vs "before" is what the
  -- run pinned in the Lua heap; rising fails with flat gc_kb means the shared
  -- backing store (game libc heap) refused, not Lua garbage.
  local okm2, m2 = pcall(function() return GetLuaMemory() end)
  P("MEM after " .. (okm2 and tostring(m2) or "unavailable"))
  P("done")
  S.done = true
  S.busy = false
end
PF = PFIND()
if not PF then P("progress-io unavailable (resume disabled)") end
S.pf = PF
RegisterKeyBind(Key.F12, function() P("K1 f12") pcall(ExecuteInGameThread, RUN) P("K2 q") end)
S.h.armed = true

-- AUTO self-test at +180s (Claude r11)
LoopAsync(180000,function() if S.n==0 and not S.busy then P("AUTO start") pcall(ExecuteInGameThread,RUN) end return false end)
P("boot v1.1 M=" .. M .. " armed=true pf=" .. tostring(PF ~= nil) .. " one-run=true")
local E = {
{"print", function(S) print("[ST2] print-probe\n") return "logged" end},
}for i = 1, #E do T[#T + 1] = E[i] end
