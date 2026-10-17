-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"RegisterProcessConsoleExecPostHook", function(S) local ok, a = pcall(RegisterProcessConsoleExecPostHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterCallFunctionByNameWithArgumentsPreHook", function(S) local ok, a = pcall(RegisterCallFunctionByNameWithArgumentsPreHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterCallFunctionByNameWithArgumentsPostHook", function(S) local ok, a = pcall(RegisterCallFunctionByNameWithArgumentsPostHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterULocalPlayerExecPreHook", function(S) local ok, a = pcall(RegisterULocalPlayerExecPreHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterULocalPlayerExecPostHook", function(S) local ok, a = pcall(RegisterULocalPlayerExecPostHook, function() end) if ok then return "registered" end if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end return false, a end},
{"RegisterConsoleCommandHandler", function(S) local r = RegisterConsoleCommandHandler("st2h", function() end) return "ret=" .. tostring(r) end},
{"RegisterConsoleCommandGlobalHandler", function(S) local r = RegisterConsoleCommandGlobalHandler("st2g", function() end) return "ret=" .. tostring(r) end},
{"ForEachUObject", function(S) local n = 0 local ok = pcall(ForEachUObject, function(o, c, i) n = n + 1 end) if ok then return "n=" .. n end return false, "iterate-err" end},
{"IterateGameDirectories", function(S) local r = IterateGameDirectories() return "ret=" .. type(r) end},
{"IsKeyBindRegistered", function(S) local r = IsKeyBindRegistered(Key.F12) if r == true then return "true" end return false, "expected-true got " .. tostring(r) end},
{"RegisterKeyBind-arm-report", function(S) if S.h.armed then return "F12-armed" end return false, "arm-failed" end},
{"RegisterKeyBindAsync", function(S) local r = RegisterKeyBindAsync(Key.F12, function() end) return "ret=" .. tostring(r) end},
{"ExecuteInGameThread", function(S) ExecuteInGameThread(function() end) return "queued" end},
{"ExecuteAsync", function(S) ExecuteAsync(function() end) return "queued" end},
{"ExecuteWithDelay", function(S) ExecuteWithDelay(1, function() end) return "queued" end},
{"LoopAsync-one-shot", function(S) LoopAsync(1, function() return false end) return "one-shot" end},
{"ExecuteInGameThreadWithDelay", function(S) local h = ExecuteInGameThreadWithDelay(1, function() end) S.h.d1 = h return "h=" .. tostring(h) end},
{"RetriggerableEITWD", function(S) local h = RetriggerableExecuteInGameThreadWithDelay(1, function() end) S.h.d2 = h return "h=" .. tostring(h) end},
{"ExecuteInGameThreadAfterFrames", function(S) local h = ExecuteInGameThreadAfterFrames(1, function() end) S.h.d3 = h return "h=" .. tostring(h) end},
{"LoopInGameThreadWithDelay-one-shot", function(S) LoopInGameThreadWithDelay(1, function() return false end) return "one-shot" end},
{"LoopInGameThreadAfterFrames-one-shot", function(S) LoopInGameThreadAfterFrames(1, function() return false end) return "one-shot" end},
{"MakeActionHandle", function(S) local h = MakeActionHandle() if type(h) == "number" then S.h.d4 = h return "h=" .. h end return false, "type=" .. type(h) end},
}
