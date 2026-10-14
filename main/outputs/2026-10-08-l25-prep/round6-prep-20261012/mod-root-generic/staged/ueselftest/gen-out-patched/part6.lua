-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"uobj.IsClass", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local c = pc:GetClass() return "pc=" .. tostring(pc:IsClass()) .. " cls=" .. tostring(c:IsClass()) end},
{"uobj.HasAllFlags", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local r = pc:HasAllFlags(EObjectFlags.RF_Public) return "r=" .. tostring(r) end},
{"uobj.HasAnyFlags", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local r = pc:HasAnyFlags(EObjectFlags.RF_Public) return "r=" .. tostring(r) end},
{"uobj.HasAnyInternalFlags", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local r = pc:HasAnyInternalFlags(EInternalObjectFlags.PendingKill) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"uobj.GetName-reflected", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local s = pc:GetName() if type(s) == "string" then return s:sub(1, 24) end return false, "type=" .. type(s) end},
{"uobj.K2_GetActorLocation", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local v = pc:K2_GetActorLocation() if v == nil then return false, "nil" end local x local ok = pcall(function() x = v.X end) return "type=" .. type(v) .. " X=" .. tostring(x) end},
{"uobj.CallFunction", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local v = pc:CallFunction("K2_GetActorLocation") if v == nil then return "nil-ret" end return "type=" .. type(v) end},
{"uobj.ProcessConsoleExec", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local r = pc:ProcessConsoleExec("st2_noop_cmd", nil, pc) return "ret=" .. tostring(r) end},
{"prop-read.Pawn", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local p = pc.Pawn if p == nil then return "nil" end return "valid=" .. tostring(S.v(p)) end},
{"prop-read.PlayerCameraManager", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local p = pc.PlayerCameraManager if p == nil then return "nil" end return "valid=" .. tostring(S.v(p)) end},
{"prop-read.Player", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local p = pc.Player if p == nil then return "nil" end return "valid=" .. tostring(S.v(p)) end},
{"prop-read.bool-bShowMouseCursor", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local v = pc.bShowMouseCursor if type(v) == "boolean" then S.h.mcur = v return "v=" .. tostring(v) end return false, "type=" .. type(v) end},
{"uobj.GetPropertyValue", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local v = pc:GetPropertyValue("bShowMouseCursor") return "type=" .. type(v) .. " same-as-direct=" .. tostring(tostring(v) == tostring(S.h.mcur)) end},
{"uobj.Reflection.GetProperty", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local pr = pc:Reflection() if pr == nil then return false, "reflection-nil" end local ph = pr:GetProperty("bShowMouseCursor") if ph == nil then return false, "prop-nil" end local iv = ph:IsValid() if iv == true then return "valid" end return false, "IsValid=" .. tostring(iv) end},
}
