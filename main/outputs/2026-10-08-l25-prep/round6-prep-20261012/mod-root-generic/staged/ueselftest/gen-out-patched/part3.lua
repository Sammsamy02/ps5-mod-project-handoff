-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"ue4ss_loaded_modules", function(S) if type(ue4ss_loaded_modules) ~= "table" then return false, "type=" .. type(ue4ss_loaded_modules) end local n = 0 for k in pairs(ue4ss_loaded_modules) do n = n + 1 if n > 15 then break end end return "n=" .. n end},
{"NAME_None", function(S) local s local ok = pcall(function() s = NAME_None:ToString() end) if ok and type(s) == "string" then return s end return false, "tostring-failed" end},
{"FName-ctor", function(S) local f = FName("st2name") S.h.f1 = f return "made" end},
{"FName.ToString", function(S) local f = S.h.f1 or FName("st2name") local s = f:ToString() if type(s) == "string" then return s end return false, "type=" .. type(s) end},
{"FName-tostring", function(S) local f = S.h.f1 or FName("st2name") local s = tostring(f) if type(s) == "string" then return s:sub(1, 24) end return false, "type=" .. type(s) end},
{"FName.Equals", function(S) local f = S.h.f1 or FName("st2name") local r = f:Equals(FName("st2name")) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"FName-eq", function(S) local f = S.h.f1 or FName("st2name") local r = (f == FName("st2name")) return "eq=" .. tostring(r) end},
{"FName.GetComparisonIndex", function(S) local f = S.h.f1 or FName("st2name") local v = f:GetComparisonIndex() if type(v) == "number" then return tostring(v) end return false, "type=" .. type(v) end},
{"FName.type", function(S) local f = S.h.f1 or FName("st2name") local t = f:type() if type(t) == "string" then return t end return false, "type=" .. type(t) end},
{"FName-ctor-FNAME_Find", function(S) local f = FName("Engine", EFindName.FNAME_Find) S.h.f2 = f return "find-ctor" end},
{"FText-ctor", function(S) local t = FText("st2text") S.h.t1 = t return "made" end},
{"FText.ToString", function(S) local t = S.h.t1 or FText("st2text") local s = t:ToString() if type(s) == "string" then return s end return false, "type=" .. type(s) end},
{"FText-tostring", function(S) local t = S.h.t1 or FText("st2text") local s = tostring(t) if type(s) == "string" then return s:sub(1, 24) end return false, "type=" .. type(s) end},
{"FText.type", function(S) local t = S.h.t1 or FText("st2text") local v = t:type() if type(v) == "string" then return v end return false, "type=" .. type(v) end},
{"FText-eq", function(S) local a = FText("st2text") local r = (a == FText("st2text")) return "eq=" .. tostring(r) end},
{"FString-ctor", function(S) local s = FString("st2str") S.h.s1 = s return "made" end},
{"FString.Len", function(S) local s = S.h.s1 or FString("st2str") local n = s:Len() if type(n) == "number" then return "len=" .. n end return false, "type=" .. type(n) end},
{"FString.IsEmpty", function(S) local s = S.h.s1 or FString("st2str") local r = s:IsEmpty() if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"FString.ToString", function(S) local s = S.h.s1 or FString("st2str") local r = s:ToString() if type(r) == "string" then return r end return false, "type=" .. type(r) end},
{"FString-tostring", function(S) local s = S.h.s1 or FString("st2str") local r = tostring(s) if type(r) == "string" then return r:sub(1, 24) end return false, "type=" .. type(r) end},
{"FString.type", function(S) local s = S.h.s1 or FString("st2str") local t = s:type() if type(t) == "string" then return t end return false, "type=" .. type(t) end},
{"FString.Append", function(S) local s = S.h.s1 or FString("st2str") s:Append("X") return "len=" .. tostring(s:Len()) end},
}
