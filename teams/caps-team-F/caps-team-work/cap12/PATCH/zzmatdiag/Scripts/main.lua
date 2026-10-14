-- zzmatdiag v2 (cap #12, read-only). For each skeletal mesh component of the
-- local player's pawn (or of an actor attached to it), print every material
-- slot: the material and its base material. Samples every 30 s for up to 60
-- min; scans only when the pawn changes or every 2 min, and prints a slot set
-- only when it differs from the last one printed (max 12 sets).
-- Generic: no game, mod or asset names. Mod files untouched.
local function d(s) print("[mat12] "..tostring(s).."\n") end
local function fn(o) local ok,v=pcall(function() return o:GetFullName() end) return ok and tostring(v) or "?" end
local function valid(o) if o==nil then return false end local ok,v=pcall(function() return o:IsValid() end) return ok and v end
local runs,ticks,sets,lastPawn,lastSig,done=0,0,0,nil,nil,false
local function mine(c,pn)
  local ok,own=pcall(function() return c:GetOwner() end)
  if not ok or not valid(own) then return false end
  if fn(own)==pn then return true end
  local okA,par=pcall(function() return own:GetAttachParentActor() end)
  return okA and valid(par) and fn(par)==pn
end
local function probe()
  runs=runs+1
  local ok,pc=pcall(FindFirstOf,"PlayerController")
  if not ok or not valid(pc) then return end
  local ok2,pawn=pcall(function() return pc.Pawn end)
  if not ok2 or not valid(pawn) then return end
  local pn=fn(pawn)
  if pn==lastPawn and ticks%4~=0 then return end
  lastPawn=pn
  local seen,lines,nc,ns={},{},0,0
  for _,cls in ipairs({"SkeletalMeshComponent","PoseableMeshComponent"}) do
    local ok3,all=pcall(FindAllOf,cls)
    if ok3 and all then
      for _,c in ipairs(all) do
        local cn=fn(c)
        if not seen[cn] and mine(c,pn) then
          seen[cn]=true nc=nc+1
          local okN,n=pcall(function() return c:GetNumMaterials() end)
          if not okN or type(n)~="number" then n=0 end
          lines[#lines+1]="[C] "..cn.." slots="..n
          for i=0,n-1 do
            local okM,m=pcall(function() return c:GetMaterial(i) end)
            local mv=okM and valid(m)
            local b="?"
            if mv then
              local okB,bm=pcall(function() return m:GetBaseMaterial() end)
              if okB and valid(bm) then b=fn(bm) end
            end
            lines[#lines+1]="[S] c="..nc.." i="..i.." mat="..(mv and fn(m) or "nil").." base="..b
            ns=ns+1
          end
        end
      end
    end
  end
  if ns==0 then return end
  local sig=pn.."|"..table.concat(lines,"|")
  if sig==lastSig then return end
  lastSig=sig sets=sets+1
  d("[P] set="..sets.." run="..runs.." tick="..ticks.." pawn="..pn)
  for _,l in ipairs(lines) do d(l) end
  d("[DONE] set="..sets.." comps="..nc.." slots="..ns)
end
LoopAsync(30000,function()
  if done then return true end
  ticks=ticks+1
  pcall(ExecuteInGameThread,probe)
  if sets>=12 or ticks>=120 then done=true d("[END] sets="..sets.." ticks="..ticks) return true end
  return false
end)
d("[LOAD] zzmatdiag v2")
