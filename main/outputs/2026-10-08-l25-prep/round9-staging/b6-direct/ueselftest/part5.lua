-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"FindFirstOf", function(S) local ok, o = pcall(FindFirstOf, "PlayerController") if not ok then return false, "call-err" end if o == nil then return false, "returned-nil(contract=wrapper)" end local v = S.v(o) if v then S.h.gpc = o end return "valid=" .. tostring(v) end},
{"FindAllOf", function(S) local ok, a = pcall(FindAllOf, "PlayerController") if not ok then return false, "call-err" end if a == nil then return "nil(real-miss-ok)" end if type(a) ~= "table" then return false, "type=" .. type(a) end return "n=" .. #a end},
{"StaticFindObject-long", function(S) local ok, o = pcall(StaticFindObject, "/Script/Engine.GameEngine") if not ok then return false, "call-err" end if o == nil then return false, "nil" end return "valid=" .. tostring(S.v(o)) end},
{"StaticFindObject-short", function(S) local ok, o = pcall(StaticFindObject, "GameEngine") if not ok then return false, "call-err" end if o == nil then return false, "nil" end return "valid=" .. tostring(S.v(o)) end},
{"CreateInvalidObject", function(S) local o = CreateInvalidObject() if o == nil then return false, "nil" end local v local ok = pcall(function() v = o:IsValid() end) if ok and v == false then return "invalid-ok" end return false, "IsValid=" .. tostring(v) end},
{"uobj.IsValid", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local v = pc:IsValid() if v == true then return "valid" end return false, "IsValid=" .. tostring(v) end},
{"uobj.GetAddress", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local a = pc:GetAddress() if type(a) == "number" then return string.format("0x%x", a) end return false, "type=" .. type(a) end},
{"uobj.GetFullName", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local s = pc:GetFullName() if type(s) == "string" then return s:sub(1, 24) end return false, "type=" .. type(s) end},
{"uobj.GetFName", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local f = pc:GetFName() if f == nil then return false, "nil" end local s local ok = pcall(function() s = f:ToString() end) if ok and type(s) == "string" then return s:sub(1, 24) end return false, "tostring-failed" end},
{"uobj.GetClass", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local c = pc:GetClass() if c ~= nil and S.v(c) then return "valid" end return false, "class-invalid" end},
{"uobj.GetOuter", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local o = pc:GetOuter() if o == nil then return "nil-outer" end return "valid=" .. tostring(S.v(o)) end},
{"uobj.GetWorld", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local w = pc:GetWorld() if w == nil then return "nil-world" end return "valid=" .. tostring(S.v(w)) end},
{"uobj.IsA-class", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local c = pc:GetClass() local r = pc:IsA(c) if r == true then return "true" end return false, "IsA=" .. tostring(r) end},
{"uobj.IsA-string", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local r = pc:IsA("/Script/Engine.PlayerController") if r == true then return "true" end return false, "IsA=" .. tostring(r) end},
{"uobj.IsAnyClass", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local c = pc:GetClass() return "pc=" .. tostring(pc:IsAnyClass()) .. " cls=" .. tostring(c:IsAnyClass()) end},
}
