-- zzdiag v11
local function d(s) print("[zz] "..tostring(s).."\n") end
local str=tostring
local n,ran,n250=0,false,0
local PN="PickupInteractable"
local function val(o) local ok,v=pcall(function() return o:IsValid() end) return ok and v end
local function st(nm) local ok,o=pcall(FindFirstOf,nm) if not ok then return "e" end if o==nil then return "n" end return val(o) and "v" or "i" end
local lm="?" local mn=function()
  local ok,w=pcall(FindFirstOf,"World")
  if ok and w~=nil and val(w) then
    local o2,s2=pcall(function() return w:GetFullName() end)
    if o2 then return ts(s2,48) end
  end
  return "?"
end
local function cycle()
  n=n+1
  d("T"..n.." db="..st("ItemDatabaseSubsystem").." pc="..st("PlayerController").." cb="..n250) n250=0
  local m=mn()
  if m~=lm then d("[MAP] "..m.." @"..(n*6).."s") lm=m end
  if n==5 then local t0=os.clock() local _,al=pcall(FindAllOf,"Actor") local t1=os.clock() d("[SCAN] FAOms="..string.format("%.0f",(t1-t0)*1000).." n="..(al and #al or -1)) end
end
local function el(e) e=str(e):gsub("^.-:%d+:%s*","") d("   "..e:sub(1,60)) end
local function ts(v,c) local ok,s=pcall(str,v) if not ok then return "e" end return (s:gsub("%c"," ")):sub(1,c or 40) end
local function bo(o) return (o and 1 or 0) end
local function skip(a,b,w) for i=a,b do d("[PRB-"..i.."] SKIP "..w) end d("[PRB] done") end
local function ids(o,b,sfx,udt)
  local function L(k) return "[PRB-"..k.."]"..sfx end
  local oq,iq=pcall(function() return o.PickupInventoryItem end)
  d(L(b).." inv ok="..bo(oq).." type="..(oq and type(iq) or "e"))
  if not oq then el(iq) return end
  if udt and type(iq)=="userdata" then
    local ot,t=pcall(function() return iq:type() end)
    d(L(b).." udt ok="..bo(ot).." t="..ts(t))
  end
  if iq==nil then d(L(b+1).." SKIP no-inv") return end
  local oi,ii=pcall(function() return iq.ItemId end)
  d(L(b+1).." id ok="..bo(oi).." type="..(oi and type(ii) or "e"))
  if not oi then el(ii) return iq end
  if ii==nil then d(L(b+2).." SKIP no-id") return iq end
  local o3,s3=pcall(str,ii)
  d(L(b+2).." tos ok="..bo(o3).." val="..ts(s3))
  local o4,s4=pcall(function() return ii.ToString(ii) end)
  d(L(b+3).." TS ok="..bo(o4).." val="..ts(s4))
  if not o4 then el(s4) end
  local o7,r7,t7=pcall(function() local f=FName(o4 and s4 or s3) return f==ii, str(f)==s3 end)
  d(L(7).." fname-eq "..(o7 and "ok="..bo(r7).." w="..bo(t7) or "e"))
  if not o7 then el(r7) end
  local o8,s8=pcall(function() return require("UEHelpers").GetWorld() end)
  d(L(8).." gw ok="..bo(o8).." val="..ts(s8,40)) if not o8 then el(s8) end
  return iq
end
local function prb()
  ran=true
  local ok,all=pcall(FindAllOf,PN)
  if not ok then el(all) end
  local tg,fb={}, ""
  if ok and type(all)=="table" and #all>0 then for i=1,math.min(3,#all) do tg[i]=all[i] end else
    fb=" ffo"
    local o2,one=pcall(FindFirstOf,PN)
    if not o2 then el(one) elseif one~=nil then tg[1]=one end
  end
  if #tg==0 then d("[PRB-0] none") skip(1,8,"no-target") return end
  d("[PRB-0] targets n="..#tg..fb)
  local p
  for i=1,#tg do if val(tg[i]) then p=tg[i] d("[PRB-1] pick t"..i) break end end
  if p==nil then d("[PRB-1] no-valid") skip(2,8,"no-valid") return end
  local inv=ids(p,1,"",true)
  if type(inv)=="table" then
    local ok5=pcall(function() local c5=0
      for k,v in pairs(inv) do c5=c5+1 if c5>12 then break end d("[PRB-5] field k="..ts(k,20).." t="..type(v)) end
      if c5==0 then d("[PRB-5] empty-table") end end)
    if not ok5 then d("[PRB-5] pairs-throw") end
  else d("[PRB-5] struct-probe skipped-not-table") end
  local ok6,s6=pcall(str,p)
  d("[PRB-6] actor ok="..bo(ok6).." val="..ts(s6))
  for i=1,#tg do ids(tg[i],1," t"..i,false) end
  d("[PRB] done")
end
RegisterKeyBind(Key.F7,function() d("K1 f7") pcall(ExecuteInGameThread,prb) d("K2 q") end)
d("[PRB] Q 18s")
LoopAsync(18000,function() pcall(ExecuteInGameThread,prb) return false end)
LoopAsync(45000,function() if not ran then d("[PRB] WD no-drain") end return false end)
LoopAsync(6000,function() cycle() return false end)
LoopAsync(250,function() n250=n250+1 return false end)
d("Z0 v11")
