-- zzladder v1 (agent-FO) mod-ladder probe, zzdiag v10 chassis. pcall
-- all; NO FindFunction/bpfn/class-children; never AddToViewport; no
-- spawn/destroy; P6 hooks+unhooks, never waits for fire. Separate mod
-- beside the target (targets NEVER edited); only CONFIG changes. Steps
-- P0-P7 per MANIFEST.md rungs. 6s T auto-report, +18s game-thread run,
-- +45s WD, F7 (=L3+R3) rerun. Markers: VERDICT-SCHEMA.md.
local C={MOD="tpl",S={P0=true}}
local S=C.S
local function d(s) print("[zzl] "..tostring(s).."\n") end
local function val(o) local ok,v=pcall(function() return o:IsValid() end) return ok and v end
local function el(e) e=tostring(e):gsub("^.-:%d+:%s*","") d("   E "..e:sub(1,24)) end
local function rep(i,s,ok,t) d("["..i.."] "..s.." "..(ok and "PASS" or "FAIL").." "..tostring(t)) end
local ran=false
local function cyc() local ok,o=pcall(FindFirstOf,"PlayerController") d("T pc="..(not ok and "e" or (o==nil and "n" or (val(o) and "v" or "i")))) end
local function P0() local o={} for g in ("io EObjectFlags EFindName FindObject FindObjects RegisterConsoleCommandHandler RegisterHook NotifyOnNewObject ExecuteInGameThreadWithDelay ExecuteAsync LoopInGameThreadWithDelay ForEachUObject"):gmatch("%S+") do o[#o+1]=g.."="..type(_G[g]) end d("[P0] env "..table.concat(o," ")) end
local function P34(pc)
  local ok,r=pcall(function()
    local tb=StaticConstructObject(StaticFindObject("/Script/UMG.TextBlock"),pc,FName("ztb1"))
    if not val(tb) then return {"sco",false,"obj"} end
    tb:SetText(FText("probe"))
    if not tostring(tb:GetText()):find("probe",1,true) then return {"stext",false,"rb"} end
    if FName("Tilde",EFindName and EFindName.FNAME_Find):ToString()~="Tilde" then return {"fname",false,"ts"} end
    tb.Text=FText("probe2")
    if not tostring(tb:GetText()):find("probe2",1,true) then return {"tset",false,"miss"} end
    return {"stext",true,"fn rb fname tset"} end)
  if ok then rep("P3",r[1],r[2],r[3]) else el(r) rep("P3","call",false,"e") end
end
local function P5(pc)
  local ok,r=pcall(function()
    local cm=pc.PlayerCameraManager
    local ml=StaticFindObject("/Script/Engine.KismetMathLibrary")
    local sv=cm:GetCameraLocation()
    local ev=ml:Add_VectorVector(sv,ml:Multiply_VectorInt(ml:GetForwardVector(cm:GetCameraRotation()),100.0))
    local HR,Z={},{R=0,G=0,B=0,A=0}
    local hit=StaticFindObject("/Script/Engine.KismetSystemLibrary"):LineTraceSingle(pc.Pawn,sv,ev,0,false,{},0,HR,true,Z,Z,0.0)
    local ha="none"
    pcall(function() local h=HR.HitObjectHandle and HR.HitObjectHandle.ReferenceObject or HR.Actor ha=(h and h:Get()) and "got" or "weak" end)
    return {"trace",true,"hit="..(hit and "y" or "n").." out="..ha} end)
  if ok then rep("P5",r[1],r[2],r[3]) else el(r) rep("P5","call",false,"e") end
end
local function P6() local p="/Script/Engine.PlayerController:ClientRestart" local o,pre,post=pcall(RegisterHook,p,function() end) if not o then el(pre) return rep("P6","hook",false,"reg") end local o2=pcall(UnregisterHook,p,pre,post) rep("P6","hook",o2,"unreg="..(o2 and "y" or "e")) end
local c7=0
local function P7()
  pcall(ExecuteInGameThreadWithDelay,50,function() d("[P7] eitwd") end)
  pcall(LoopInGameThreadWithDelay,100,function() c7=c7+1 d("[P7] ligwd "..c7) return c7<2 end)

end
local function run()
  d("R mod="..C.MOD)
  if S.P0 then pcall(P0) end
  d("[P2] q")
  ExecuteInGameThread(function()
    ran=true
    d("[P2] D")
    local ok,a=pcall(FindAllOf,"PlayerController")
    local pc=ok and a and a[1]
    local hp=pc and val(pc)
    if S.P3 then if hp then pcall(P34,pc) else rep("P3","arg",false,"no-pc") end end
    if S.P5 then if hp then pcall(P5,pc) else rep("P5","arg",false,"no-pc") end end
    if S.P6 then pcall(P6) end
    if S.P7 then pcall(P7) end
  end)
end
RegisterKeyBind(Key.F7,function() d("K1 f7") ran=false pcall(ExecuteInGameThread,run) end)
d("Z0 zzl1 "..C.MOD)
LoopAsync(18000,function() pcall(ExecuteInGameThread,run) return false end)
LoopAsync(45000,function() if not ran then d("[WD] no-run") end return false end)
LoopAsync(6000,function() cyc() return true end)
