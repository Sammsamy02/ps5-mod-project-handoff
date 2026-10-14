-- zzdiag v12 (PROPOSED, not deployed): lock/dispatch probe, light (no object scans)
-- Replaces v11 (pickup probe). Fixes v11 main.lua:12 forward reference to 'ts'.
local function d(s) print("[zz] "..tostring(s).."\n") end
local str=tostring
local n,n250,qn,kn=0,0,0,0
local function tail(e) return (str(e):gsub("^.-:%d+:%s*","")):sub(1,60) end
-- [Q] async thread -> ExecuteInGameThread -> game thread.
--   "try" with no "ok"/"err" = caller stuck inside ExecuteInGameThread (action lock)
--   "ok" with no "ran"        = queued but the game-thread pump never drained it
local function q(tag)
  qn=qn+1 local k=qn
  d("[Q] "..k.." "..tag.." try")
  local ok,e=pcall(ExecuteInGameThread,function() d("[Q] "..k.." ran") end)
  d("[Q] "..k.." "..(ok and "ok" or ("err "..tail(e))))
end
RegisterKeyBind(Key.F7,function() kn=kn+1 d("K1 f7 #"..kn) q("f7") d("K2 q") end)
LoopAsync(18000,function() q("loop") return false end)
LoopAsync(6000,function() n=n+1 d("T"..n.." cb="..n250) n250=0 return false end)
LoopAsync(250,function() n250=n250+1 return false end)
d("Z0 v12")
