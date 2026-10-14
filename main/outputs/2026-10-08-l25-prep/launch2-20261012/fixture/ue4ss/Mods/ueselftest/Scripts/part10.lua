-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"IsValidDelayedActionHandle", function(S) local h = S.h.d4 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = IsValidDelayedActionHandle(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"IsDelayedActionActive", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = IsDelayedActionActive(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"IsDelayedActionPaused", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = IsDelayedActionPaused(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"GetDelayedActionTimeRemaining", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = GetDelayedActionTimeRemaining(h) return "r=" .. tostring(r) end},
{"GetDelayedActionTimeElapsed", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = GetDelayedActionTimeElapsed(h) return "r=" .. tostring(r) end},
{"GetDelayedActionRate", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = GetDelayedActionRate(h) return "r=" .. tostring(r) end},
{"PauseDelayedAction", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = PauseDelayedAction(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"UnpauseDelayedAction", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = UnpauseDelayedAction(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"SetDelayedActionTimer", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = SetDelayedActionTimer(h, 2) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"ResetDelayedActionTimer", function(S) local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = ResetDelayedActionTimer(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"CancelDelayedAction-d2", function(S) local h = S.h.d2 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = CancelDelayedAction(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"CancelDelayedAction-d3", function(S) local h = S.h.d3 if type(h) ~= "number" then return "SKIP", "no-handle" end local r = CancelDelayedAction(h) if type(r) == "boolean" then return tostring(r) end return false, "type=" .. type(r) end},
{"ClearAllDelayedActions", function(S) local r = ClearAllDelayedActions() S.hyd.clearactions = true if type(r) == "number" then return "cleared=" .. r end return false, "type=" .. type(r) end},
{"SetPropertyValue-restore", function(S) local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end local o = S.h.mcur if type(o) ~= "boolean" then return "SKIP", "no-orig" end local ok, e = pcall(function() pc:SetPropertyValue("bShowMouseCursor", o) end) if not ok then return false, e end local b = pc.bShowMouseCursor if b ~= o then pcall(function() pc:SetPropertyValue("bShowMouseCursor", o) end) return false, "readback=" .. tostring(b) end S.hyd.setprop = true return "restored" end},
}
