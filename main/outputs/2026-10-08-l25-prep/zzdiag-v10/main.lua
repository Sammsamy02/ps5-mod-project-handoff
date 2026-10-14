-- zzdiag v10 MENU DRY RUN M1-M6 (agent-FI) on the v9 chassis: 6s T
-- auto-probe, ONE dry run at +18s on the game thread, +45s WD, F7 rerun.
-- LAW: pcall all; no FindFunction/bpfn/class-children (v2 crash);
-- wrappers never nil (test pcall IsValid); NEVER AddToViewport.
-- Registrations (l26 candidate src/LuaMod.cpp unless noted): print :1687;
-- Key.F7 :714; StaticFindObject :1695; FindFirstOf :1806; FindAllOf :1857;
-- RegisterKeyBind :2118; StaticConstructObject :2431 (Class,Outer,FName#);
-- LoopAsync :3663; ExecuteInGameThread :5745; FName :7609; FText :7612;
-- IsValid include/LuaType/LuaUObject.hpp:769; reflected obj:Fn dispatch
-- src/LuaUObject.cpp:2413-2438. No UMG Lua API exists: M2/M3 via
-- StaticFindObject; M4 Create = MOD:509-511 (v9 D5); M6 FText = v9 D8.
local function d(s) print("[zz] " .. tostring(s) .. "\n") end
local n, ran = 0, false
local function val(o)
  local ok, v = pcall(function() return o:IsValid() end)
  return ok and v
end
-- tokens: e=err n=nil v=valid i=invalid wrapper
local function st(nm)
  local ok, o = pcall(FindFirstOf, nm)
  if not ok then return "e" end
  if o == nil then return "n" end
  return val(o) and "v" or "i"
end
local function cycle()
  n = n + 1
  d("T" .. n .. " db=" .. st("ItemDatabaseSubsystem") .. " pc=" .. st("PlayerController"))
end
local function dm(i, s, v, t)
  d("[DRY-" .. i .. "] " .. s .. " " .. v .. " " .. t)
end
local function el(e)
  e = tostring(e):gsub("^.-:%d+:%s*", "")
  d("   " .. e:sub(1, 24))
end
-- object step: report validity; return object only when valid
local function ostep(i, s, o, obj)
  local p = o and val(obj)
  dm(i, s, p and "PASS" or "FAIL", p and "y" or (o and "n" or "e"))
  if not o then el(obj) end
  return p and obj or nil
end
local function dry()
  ran = true
  -- M1: FindAllOf count + pcall IsValid + pcall IsPlayerController
  local ok, all = pcall(FindAllOf, "PlayerController")
  if not ok then el(all) end
  local c = ok and (all and #all or 0) or -1
  local pc = (ok and c > 0) and all[1] or nil
  local iv, ipc = "s", "s"
  if pc then
    local o1, r1 = pcall(function() return pc:IsValid() end)
    iv = o1 and (r1 and "1" or "0") or "e"
    local o2, r2 = pcall(function() return pc:IsPlayerController() end)
    ipc = o2 and (r2 == true and "1" or "0") or "e"
    if not o2 then el(r2) end
  end
  dm(1, "pcs", (ok and c > 0 and iv == "1" and ipc == "1") and "PASS" or "FAIL",
     "n=" .. c .. " iv=" .. iv .. " ipc=" .. ipc)
  local vpc = (iv == "1") and pc or nil
  local wl = ostep(2, "cdo", pcall(StaticFindObject, "/Script/UMG.Default__WidgetBlueprintLibrary"))
  local uc = ostep(3, "uwcls", pcall(StaticFindObject, "/Script/UMG.UserWidget"))
  local w -- M4 reflected Create(pc,cls,pc): MOD:509-511/v9 D5 form
  if wl and uc and vpc then
    local o4, r4 = pcall(function() return wl:Create(vpc, uc, vpc) end)
    w = ostep(4, "create", o4, r4)
  else dm(4, "create", "SKIP", "no-arg") end
  -- M5 StaticConstructObject TextBlock (registered LuaMod.cpp:2431)
  local o5, tb = pcall(StaticFindObject, "/Script/UMG.TextBlock")
  local tbo
  if not o5 then
    el(tb)
    dm(5, "sco", "FAIL", "e")
  elseif not val(tb) then
    dm(5, "sco", "FAIL", "cls=n")
  elseif w or vpc then
    local o5b, r5b = pcall(function() return StaticConstructObject(tb, w or vpc, FName("zzdry10")) end)
    tbo = ostep(5, "sco", o5b, r5b)
  else dm(5, "sco", "SKIP", "no-arg") end
  if tbo then -- M6 SetText(FText) = v9 D8 form
    local o6, r6 = pcall(function() return tbo:SetText(FText("test")) end)
    dm(6, "stext", o6 and "PASS" or "FAIL", o6 and "ftext" or "e")
    if not o6 then el(r6) end
  else dm(6, "stext", "SKIP", "no-M5") end
  d("[DRY] no-viewport")
end
RegisterKeyBind(Key.F7, function()
  d("K1 f7")
  pcall(ExecuteInGameThread, dry)
  d("K2 q")
end)
-- AUTO once at +18s (T1/T2 out); WD splits no-drain
d("[DRY] Q 18s")
LoopAsync(18000, function() pcall(ExecuteInGameThread, dry) return false end)
LoopAsync(45000, function() if not ran then d("[DRY] WD no-drain") end return false end)
LoopAsync(6000, function() cycle() return true end)
d("Z0 v10")
