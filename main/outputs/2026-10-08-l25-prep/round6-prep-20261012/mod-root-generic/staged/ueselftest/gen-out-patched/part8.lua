-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"NameProp-userdata-read", function(S) local a = S.h.ck if not a then return "SKIP", "no-array" end local e = a[1] if e == nil then return "SKIP", "empty-array" end local k = e.KeyName if k == nil then return "SKIP", "no-keyname" end local s local ok = pcall(function() s = k:ToString() end) if ok and type(s) == "string" then return s:sub(1, 24) end return false, "tostring-failed" end},
{"loadfile-native", function(S) if type(loadfile) ~= "function" then return false, "nil" end local f = loadfile("part2.lua") return "fn=" .. type(f) end},
{"RegisterHook", function(S) local pre, post = RegisterHook("/Script/Engine.PlayerController:ClientRestart", function() end) if type(pre) == "number" and type(post) == "number" then S.h.hk = {"/Script/Engine.PlayerController:ClientRestart", pre, post} return pre .. "," .. post end return false, "pre=" .. type(pre) .. " post=" .. type(post) end},
{"UnregisterHook", function(S) local h = S.h.hk if not h then return "SKIP", "no-hook-ids" end local r = UnregisterHook(h[1], h[2], h[3]) S.hyd.unhook = true return "ret=" .. tostring(r) end},
{"NotifyOnNewObject", function(S) local r = NotifyOnNewObject("Engine.DemoNetDriver", function() end) return "ret=" .. tostring(r) end},
{"RegisterCustomEvent", function(S) local r = RegisterCustomEvent("ST2Event", function() end) return "ret=" .. tostring(r) end},
{"UnregisterCustomEvent", function(S) local r = UnregisterCustomEvent("ST2Event") S.hyd.uncustev = true return "ret=" .. tostring(r) end},
{"RegisterLoadMapPreHook", function(S) local r = RegisterLoadMapPreHook(function() end) return "ret=" .. tostring(r) end},
{"RegisterLoadMapPostHook", function(S) local r = RegisterLoadMapPostHook(function() end) return "ret=" .. tostring(r) end},
{"RegisterInitGameStatePreHook", function(S) local ok, a = pcall(RegisterInitGameStatePreHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterInitGameStatePostHook", function(S) local ok, a = pcall(RegisterInitGameStatePostHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterBeginPlayPreHook", function(S) local ok, a = pcall(RegisterBeginPlayPreHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterBeginPlayPostHook", function(S) local ok, a = pcall(RegisterBeginPlayPostHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterEndPlayPreHook", function(S) local ok, a = pcall(RegisterEndPlayPreHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterEndPlayPostHook", function(S) local ok, a = pcall(RegisterEndPlayPostHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterProcessConsoleExecPreHook", function(S) local ok, a = pcall(RegisterProcessConsoleExecPreHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
}
