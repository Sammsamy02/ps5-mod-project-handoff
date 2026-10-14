-- Host test for PATCH/zzmatdiag/Scripts/main.lua with a minimal UE4SS mock.
-- Checks: loads; runs through LoopAsync+ExecuteInGameThread; prints a slot set
-- only when it changes; follows a pawn change (menu pawn -> player pawn);
-- includes components of actors attached to the pawn; excludes other actors;
-- survives throwing calls; bounded scans; stops at the 60-min window and at
-- the 12-set cap. Exit code 0 = PASS.
local script = arg[1]
local function obj(name, t)
  t = t or {}
  t.IsValid = function() return true end
  t.GetFullName = function() return name end
  return t
end
local function mat(n, base)
  return obj("MaterialInstanceConstant "..n, {GetBaseMaterial=function() return obj("Material "..base) end})
end

local function run(world_at)
  local out, loops, scans = {}, {}, 0
  local k = 0
  local env = setmetatable({
    print=function(s) out[#out+1]=s end,
    FindFirstOf=function(n) if n=="PlayerController" then return obj("PC", {Pawn=world_at(k).pawn}) end end,
    FindAllOf=function(n) scans = scans + 1 return world_at(k)[n] end,
    ExecuteInGameThread=function(f) f() end,
    LoopAsync=function(ms, f) assert(ms==30000) loops[#loops+1]=f end,
  }, {__index=_G})
  local chunk = assert(loadfile(script, "t", env))
  chunk()
  assert(#loops==1, "expected one LoopAsync")
  local stopped_at
  for i=1,400 do k=i if loops[1]() then stopped_at=i break end end
  return out, scans, stopped_at
end

local function count(out, pat) local n=0 for _,l in ipairs(out) do if l:find(pat) then n=n+1 end end return n end

-- scenario A: menu pawn (base-game slot) for 4 ticks, then player pawn; one
-- material change at tick 40; otherwise unchanged until the window ends.
local menu = obj("BP_Menu_C /Game/Maps/M.M:PersistentLevel.BP_Menu_C_0")
local pawn = obj("BP_Player_C /Game/Maps/L.L:PersistentLevel.BP_Player_C_0")
local npc = obj("BP_Npc_C /Game/Maps/L.L:PersistentLevel.BP_Npc_C_3")
local hat = obj("BP_Hat_C /Game/Maps/L.L:PersistentLevel.BP_Hat_C_1", {GetAttachParentActor=function() return pawn end})
local m0 = obj("SkeletalMeshComponent /x.Menu", {GetOwner=function() return menu end,
  GetNumMaterials=function() return 1 end, GetMaterial=function() return mat("/Game/UI/MI_Menu.MI_Menu","/Game/UI/M_Menu.M_Menu") end})
local changed = false
local c1 = obj("SkeletalMeshComponent /x.Body", {GetOwner=function() return pawn end,
  GetNumMaterials=function() return 2 end,
  GetMaterial=function(self, i)
    if i==0 then return changed and mat("/Game/A/MI_B.MI_B","/Game/Base/M_Base.M_Base") or mat("/Game/A/MI_A.MI_A","/Game/A/M_A.M_A") end
    error("boom") end})
local c2 = obj("SkeletalMeshComponent /x.Npc", {GetOwner=function() return npc end,
  GetNumMaterials=function() return 5 end, GetMaterial=function() return nil end})
local c3 = obj("PoseableMeshComponent /x.Face", {GetOwner=function() return pawn end,
  GetNumMaterials=function() return 1 end, GetMaterial=function() return mat("/Game/F/MI_F.MI_F","/Game/F/M_F.M_F") end})
local c4 = obj("SkeletalMeshComponent /x.Hat", {GetOwner=function() return hat end,
  GetNumMaterials=function() return 1 end, GetMaterial=function() return mat("/Game/H/MI_H.MI_H","/Game/H/M_H.M_H") end})
local A = function(k)
  changed = k >= 40
  if k <= 4 then return {pawn=menu, SkeletalMeshComponent={m0, c2}, PoseableMeshComponent={}} end
  return {pawn=pawn, SkeletalMeshComponent={c1, c2, c4}, PoseableMeshComponent={c3}}
end
local out, scans, stopped = run(A)
local s, sets, ends = count(out, "%[mat12%] %[S%]"), count(out, "%[mat12%] %[P%] set="), count(out, "%[END%]")
assert(count(out, "%[LOAD%] zzmatdiag v2")==1, "no LOAD line")
assert(sets==3, "expected 3 slot sets (menu, player, change), got "..sets)
-- menu 1 slot + player 2 sets x 4 slots (c1 x2 incl. one throwing, c4 attached, c3)
assert(s==9, "expected 9 slot lines, got "..s)
assert(count(out, "Npc")==0, "printed a non-pawn component")
assert(count(out, "/x.Hat")==2, "attached-actor component not printed")
assert(count(out, "mat=nil")==2, "throwing slot not reported as nil")
assert(stopped==120 and ends==1, "window end: stopped at "..tostring(stopped))
assert(scans <= 2*34, "too many FindAllOf scans: "..scans)

-- scenario B: material changes every tick -> stops at the 12-set cap
local B = function(k)
  local m = obj("SkeletalMeshComponent /x.B", {GetOwner=function() return pawn end, GetNumMaterials=function() return 1 end,
    GetMaterial=function() return mat("/Game/B/MI_"..k..".MI_"..k, "/Game/B/M.M") end})
  return {pawn=obj("P"..k), SkeletalMeshComponent={m}, PoseableMeshComponent={}}
end
local outB, _, stoppedB = run(function(k) local w = B(k) pawn = w.pawn return w end)
local setsB = count(outB, "%[mat12%] %[P%] set=")
assert(setsB==12 and stoppedB==12, "cap: sets "..setsB.." stopped "..tostring(stoppedB))

local f = io.open(script, "rb"); local n = #f:read("a"); f:close()
assert(n < 4096, "script over 4096-byte admission cap: "..n)
print("zzmatdiag host test: checked "..(s + count(outB, "%[mat12%] %[S%]")).." slot lines, "..(sets + setsB)..
      " sets, "..scans.." scans, size "..n.." bytes: PASS")
