-- ueselftest v1 (agent GB, gen_selftest.py 1.0; Claude round-5 item 6a):
-- parity self-test from FS API-PARITY-TABLE.md. LAWS: pcall every call; ONE
-- log line/call; progress.txt marker BEFORE each call (crash-resume); engine
-- base classes only; NO FindFunction/bpfn/class-children; no spawns/saves/
-- constructs; writes restore+verify; hooks unregister; loops one-shot.
local M = 167
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
local function ONE(e)
  S.n = S.n + 1 PFW(S.n)
  local ok, a, b = pcall(e[2], S)
  local l = S.n .. "/" .. M .. " " .. e[1] .. " "
  if not ok then P(l .. "FAIL " .. S.et(a))
  elseif a == "SKIP" then P(l .. "SKIP " .. TS(b))
  elseif a == false then P(l .. "FAIL " .. TS(b))
  else S.p = S.p + 1 P(l .. "PASS" .. ((a ~= nil and a ~= true) and (" " .. TS(a)) or "")) end
end
local function LP(k, cnt)
  if S.h["lp" .. k] then return end
  local nm = "part" .. k local ok, t = pcall(require, nm)
  if not ok or type(t) ~= "table" then ok, t = pcall(dofile, nm .. ".lua") end
  if not ok or type(t) ~= "table" then ok, t = pcall(dofile, "ue4ss/Mods/ueselftest/Scripts/" .. nm .. ".lua") end
  if ok and type(t) == "table" and #t == cnt then
    for i = 1, cnt do T[#T + 1] = t[i] end P(nm .. " loaded " .. cnt)
  else
    P(nm .. " LOAD-FAIL " .. TS(t))
    for i = 1, cnt do T[#T + 1] = {nm .. "-" .. i, function() return false, "part-not-loaded" end} end
  end
  S.h["lp" .. k] = true
end
local function RUN()
  if S.busy then P("busy") return end
  S.busy = true
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
  LP(11, 5)
  LP(12, 1)
  if #T ~= M then P("WARN count " .. #T .. " expected " .. M) end
  local cr = PFR()
  if cr then P(cr .. "/" .. M .. " " .. (T[cr] and T[cr][1] or "?") .. " CRASH (resumed)") end
  S.n = cr or 0
  for i = S.n + 1, #T do ONE(T[i]) end
  PFW(-1)
  P("SUMMARY " .. S.p .. " of " .. M .. " PASS")
  P("done")
  S.busy = false
end
PF = PFIND()
if not PF then P("progress-io unavailable (resume disabled)") end
S.pf = PF
RegisterKeyBind(Key.F12, function() P("K1 f12") pcall(ExecuteInGameThread, RUN) P("K2 q") end)
S.h.armed = true
P("boot v1 M=" .. M .. " armed=true pf=" .. tostring(PF ~= nil))
local E = {
{"print", function(S) print("[ST2] print-probe\n") return "logged" end},
}for i = 1, #E do T[#T + 1] = E[i] end
