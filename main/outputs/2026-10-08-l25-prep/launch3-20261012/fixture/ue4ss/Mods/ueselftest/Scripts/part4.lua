-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"FString.EndsWith", function(S) local s = S.h.s1 or FString("st2strX") local r = s:EndsWith("X") if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"FString.StartsWith", function(S) local s = S.h.s1 or FString("st2str") local r = s:StartsWith("st2") if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"FString.Find", function(S) local s = S.h.s1 or FString("st2str") local r = s:Find("2") return "r=" .. tostring(r) end},
{"FString.ToLower", function(S) local s = S.h.s1 or FString("St2StR") local r = s:ToLower() return "r=" .. type(r) .. ":" .. tostring(r):sub(1, 12) end},
{"FString.ToUpper", function(S) local s = S.h.s1 or FString("St2StR") local r = s:ToUpper() return "r=" .. type(r) .. ":" .. tostring(r):sub(1, 12) end},
{"FString.Clear", function(S) local s = S.h.s1 or FString("x") s:Clear() local e = s:IsEmpty() return "empty=" .. tostring(e) end},
{"FString.Empty", function(S) local s = S.h.s1 or FString("x") s:Empty() local e = s:IsEmpty() return "empty=" .. tostring(e) end},
{"FUtf8String-ctor", function(S) local u = FUtf8String("st2u8") S.h.u1 = u return "made" end},
{"FUtf8String.Len", function(S) local u = S.h.u1 or FUtf8String("st2u8") local n = u:Len() if type(n) == "number" then return "len=" .. n end return false, "type=" .. type(n) end},
{"FUtf8String.ToString", function(S) local u = S.h.u1 or FUtf8String("st2u8") local r = u:ToString() if type(r) == "string" then return r end return false, "type=" .. type(r) end},
{"FAnsiString-ctor", function(S) local a = FAnsiString("st2an") S.h.a1 = a return "made" end},
{"FAnsiString.Len", function(S) local a = S.h.a1 or FAnsiString("st2an") local n = a:Len() if type(n) == "number" then return "len=" .. n end return false, "type=" .. type(n) end},
{"FAnsiString.ToString", function(S) local a = S.h.a1 or FAnsiString("st2an") local r = a:ToString() if type(r) == "string" then return r end return false, "type=" .. type(r) end},
-- [IU-GC calls 58-61] upstream LuaMod.cpp GetCurrentThreadId family pushes
-- LuaType::ThreadId USERDATA (metatable ThreadIdUserdata, members ToString +
-- type, __eq) — never a number. Port is byte-identical. Old number
-- expectations were test errors; expect userdata with a working ToString.
-- NOTE: the __eq metamethod shares the upstream return-bool-as-count quirk, so
-- it is deliberately NOT asserted here.
{"GetCurrentThreadId", function(S) local v = GetCurrentThreadId() if type(v) == "userdata" then local ok, s = pcall(function() return v:ToString() end) if ok and type(s) == "string" then return "userdata tostring-ok" end return false, "tostring-failed" end return false, "type=" .. type(v) end},
{"GetMainModThreadId", function(S) local v = GetMainModThreadId() if type(v) == "userdata" then local ok, s = pcall(function() return v:ToString() end) if ok and type(s) == "string" then return "userdata tostring-ok" end return false, "tostring-failed" end return false, "type=" .. type(v) end},
{"GetAsyncThreadId", function(S) local v = GetAsyncThreadId() if type(v) == "userdata" then local ok, s = pcall(function() return v:ToString() end) if ok and type(s) == "string" then return "userdata tostring-ok" end return false, "tostring-failed" end return false, "type=" .. type(v) end},
{"GetGameThreadId", function(S) local v = GetGameThreadId() if type(v) == "userdata" then local ok, s = pcall(function() return v:ToString() end) if ok and type(s) == "string" then return "userdata tostring-ok" end return false, "tostring-failed" end return false, "type=" .. type(v) end},
{"IsInMainModThread", function(S) local v = IsInMainModThread() if type(v) == "boolean" then return tostring(v) end return false, "type=" .. type(v) end},
{"IsInAsyncThread", function(S) local v = IsInAsyncThread() if type(v) == "boolean" then return tostring(v) end return false, "type=" .. type(v) end},
{"IsInGameThread", function(S) local v = IsInGameThread() if v == true then return "true" end return false, "expected-true got " .. tostring(v) end},
{"os.clock", function(S) local t = os.clock() if type(t) == "number" then return "t=" .. t end return false, "type=" .. type(t) end},
{"os.date", function(S) local t = os.date() if type(t) == "string" then return "ok" end return false, "type=" .. type(t) end},
{"debug.getinfo", function(S) local i = debug.getinfo(1, "S") if type(i) == "table" and type(i.source) == "string" then return "src" end return false, "type=" .. type(i) end},
}
