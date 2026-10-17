-- zzkbprobe v1 (Team I2): does the engine see a USB keyboard/mouse? Read-only.
-- Asks the game's own PlayerController IsInputKeyDown(key) ~30x/s on the game
-- thread and logs up/down changes. Binds nothing, writes nothing.
local function d(s) print("[kb] "..tostring(s).."\n") end
local N={"F8","F7","A","SpaceBar","Enter","LeftShift","LeftControl",
 "LeftMouseButton","RightMouseButton","Gamepad_FaceButton_Bottom"}
local K={}
for i=1,#N do K[i]={KeyName=FName(N[i])} end
local st,cnt={},{}
local ed,cap,pc,miss,polls,fail,fn,pend,t0=0,240,nil,0,0,0,nil,false,os.clock()
local function val(o) local ok,v=pcall(function() return o:IsValid() end) return ok and v==true end
local function getpc()
 if pc~=nil and val(pc) then return pc end
 if pc~=nil then d("PC lost") pc=nil end
 miss=miss+1
 if miss%60~=1 then return nil end
 local ok,o=pcall(function() return require("UEHelpers").GetPlayerController() end)
 if ok and o~=nil and val(o) then pc=o st={} d("PC found") end
 return pc
end
local function poll()
 polls=polls+1
 if fail>=30 then return end
 local p=getpc()
 if p==nil then return end
 for i=1,#N do
  local ok,v=pcall(function() return p:IsInputKeyDown(K[i]) end)
  if not ok then
   fail=fail+1
   if fn==nil then fn=false d("CAP fn=0 "..tostring(v):gsub("%c"," "):sub(1,80)) end
   if fail==30 then d("STOP fail=30") end
   return
  end
  if fn~=true then fn=true d("CAP fn=1 rt="..type(v)) end
  local b=(v==true)
  if st[i]~=nil and st[i]~=b then
   cnt[i]=(cnt[i] or 0)+1
   if ed<cap then
    ed=ed+1
    d((b and "DN " or "UP ")..N[i].." n="..ed.." t="..string.format("%.1f",os.clock()-t0))
    if ed==cap then d("EDGE cap="..cap) end
   end
  end
  st[i]=b
 end
end
local function sum()
 local c={}
 for i=1,#N do c[i]=N[i].."="..(cnt[i] or 0) end
 d("SUM v1 polls="..polls.." pc="..(pc~=nil and 1 or 0).." fn="..(fn==nil and -1 or (fn and 1 or 0))
  .." edges="..ed.." fail="..fail.." "..table.concat(c," "))
end
LoopAsync(33,function()
 if not pend then
  pend=true
  local ok=pcall(ExecuteInGameThread,function() pend=false poll() end)
  if not ok then pend=false end
 end
 return false
end)
LoopAsync(10000,function() sum() return false end)
d("Z0 v1 keys="..#N)
