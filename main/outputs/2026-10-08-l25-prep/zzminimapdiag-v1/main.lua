-- zzminimapdiag v1 (agent IN): times MiniMap v3.1 50ms-tick calls (main.lua:4062,
-- LIGWD :4268) phase-aligned to its rescans to attribute the 1Hz stutter second.
-- Read-only. Deploy: harvest/lua_fast_deploy.sh. A/B: run minimap ON, then OFF.
-- Report key: n=count a=avg m=max(ms) c=objects found.
local C=os and os.clock
if not C or not LoopInGameThreadWithDelay then print("[INMD] prereq missing\n") return end
local N,A,W,PC,K=0,{},0,nil,nil
local function m(k,f)
  local t0=C() local ok,r=pcall(f) local d=(C()-t0)*1000
  local a=A[k] or{0,0,0,0} a[1],a[2]=a[1]+1,a[2]+d if d>a[3] then a[3]=d end
  if type(r)=="table" then a[4]=#r end A[k]=a return ok,r end
local function val(o) local ok,v=pcall(function() return o:IsValid() end) return ok and v end
local function rep(t)
  local p={"t"..N.." w"..("%.2f"):format(W).." n"..("%.2f"):format(t)} W=0
  for k,a in pairs(A) do p[#p+1]=k..(" n%d a%.2f m%.2f c%d"):format(a[1],a[2]/a[1],a[3],a[4]) end
  table.sort(p) print("[INMD] "..table.concat(p,"|").."\n") A={} end
local function tick()
  local t0=C() N=N+1 local n,pc=N,PC
  m("noop",function() end)
  if not(pc and val(pc)) then local _,l=m("FAOPC",function() return FindAllOf("PlayerController") end) pc=l and l[1] end
  if not(pc and val(pc)) then rep((C()-t0)*1000) return end
  PC=pc
  if not K then K={} for i=1,8 do K[i]=FName("F"..i+3) end end
  m("Keys8",function() for i=1,8 do pcall(function() pc:IsInputKeyDown({KeyName=K[i]}) end) end end)
  local _,pawn=m("PawnLocRot",function()
    local p=pc.Pawn
    pcall(function() p:K2_GetActorLocation() end)
    pcall(function() pc.PlayerCameraManager:GetCameraRotation() end)
    return p end)
  if pawn and val(pawn) then
    m("MapChain",function()
      local c=pawn.MapRenderComponent if not val(c) then return end
      pcall(function() c:IsMapOpen() end)
      local ok,w=pcall(function() return c.WidgetTextures end)
      if ok and w and #w>0 then pcall(function()
        local x=w[1] x.Material:K2_GetTextureParameterValue(x.MaterialTextureName) end) end end)
    if n%20==0 then local _,i=m("InvC",function() return pawn.InventoryComponent end)
      if i and val(i) then m("R23x",function()
        for t=1,23 do pcall(function() return i:GetNumItemsOfTypeInInventory(t,true) end) end end) end end end
  m("GSHUD",function()
    local l=StaticFindObject("/Script/Townfall.Default__TownfallBlueprintLibrary")
    if l then pcall(function() l:GetGameState(pc) end) end
    pcall(function() pc:GetHUD():GetHUDDisplayState() end) end)
  if n%20==0 then m("FAOEn",function() return FindAllOf("TownfallEnemyCharacter") end) end
  if n%10==3 then m("FAOPk",function() return FindAllOf("PickupInteractable") end) end
  if n%40==11 then m("FAOIn",function() return FindAllOf("Interactable") end) end
  if n%40==31 then m("FAOHd",function() return FindAllOf("HidingPlaceInteractable") end) end
  local t=(C()-t0)*1000 if t>W then W=t end
  if n%20==0 then rep(t) end end
LoopInGameThreadWithDelay(50,function() pcall(tick) end)
print("[INMD] v1 up: 50ms tick, 1s reports; stutter second = FAOEn row\n")
