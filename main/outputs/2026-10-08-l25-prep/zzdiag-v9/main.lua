-- zzdiag v9 MENU DRY RUN (agent-FA): auto full menu-path probe at +18s on
-- the game thread (ExecuteInGameThread/pump drain); NEVER AddToViewport.
-- Steps D1-D9: FindAllOf PC count, IsPlayerController, WidgetBlueprint-
-- Library CDO, UserWidget cls, Create(pc,c,pc), TextBlock cls,
-- StaticConstructObject, SetText(FText), no-viewport. v8 kept: 6s T
-- probe, F7 rerun, WD drain watchdog.
local function d(s) print("[zz] " .. tostring(s) .. "\n") end
local n, ran = 0, false
local function val(o)
  local ok, v = pcall(function() return o:IsValid() end)
  return ok and v
end
-- v8 token contract: e=err n=nil v=valid i=invalid wrapper
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
local function dr(s, p, t)
  d("[DRY] " .. s .. (p and " PASS " or " FAIL ") .. tostring(t))
end
local function el(e)
  e = tostring(e):gsub("^.-:%d+:%s*", "")
  d("   " .. e:sub(1, 24))
end
-- object step: log resolution; return object only if valid
local function ostep(s, o, obj)
  local p = o and val(obj)
  dr(s, p, o and (p and "y" or "n") or "e")
  if not o then el(obj) end
  return p and obj or nil
end
local function dry()
  ran = true
  local ok, all = pcall(FindAllOf, "PlayerController")
  local c = ok and (all and #all or 0) or -1
  dr("D1", ok and c > 0, "n=" .. c)
  if not ok then el(all) end
  local pc = (ok and c > 0) and all[1] or nil
  if pc and val(pc) then
    local o2, r2 = pcall(function() return pc:IsPlayerController() end)
    dr("D2", o2 and r2 == true, "ipc=" .. (r2 and 1 or 0))
    if not o2 then el(r2) end
  else dr("D2", false, pc and "inv" or "skip") end
  local wl = ostep("D3", pcall(StaticFindObject, "/Script/UMG.Default__WidgetBlueprintLibrary"))
  local uc = ostep("D4", pcall(StaticFindObject, "/Script/UMG.UserWidget"))
  local w
  if wl and uc and pc then
    local o5, r5 = pcall(function() return wl:Create(pc, uc, pc) end)
    w = ostep("D5", o5, r5)
  else dr("D5", false, "skip") end
  local tb = ostep("D6", pcall(StaticFindObject, "/Script/UMG.TextBlock"))
  local tbo
  if tb and (w or pc) then
    local o7, r7 = pcall(function() return StaticConstructObject(tb, w or pc, FName("zzdry9")) end)
    tbo = ostep("D7", o7, r7)
  else dr("D7", false, "skip") end
  if tbo then
    local o8, r8 = pcall(function() return tbo:SetText(FText("test")) end)
    dr("D8", o8, "ftext")
    if not o8 then el(r8) end
  else dr("D8", false, "skip") end
  dr("D9", true, "no-viewport")
end
RegisterKeyBind(Key.F7, function()
  d("K1 f7")
  pcall(ExecuteInGameThread, dry)
  d("K2 q")
end)
-- AUTO: once at +18s; WD +45s splits no-drain vs FAIL
d("[DRY] Q 18s")
LoopAsync(18000, function() pcall(ExecuteInGameThread, dry) return false end)
LoopAsync(45000, function() if not ran then d("[DRY] WD no-drain") end return false end)
LoopAsync(6000, function() cycle() return true end)
d("Z0 v9")
