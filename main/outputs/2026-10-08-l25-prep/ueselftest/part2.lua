-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
{"LoadExport", function(S) local ok, r = pcall(LoadExport, "st2_missing_export") if ok then return "ret=" .. type(r) end return "missing-name-err(expected)" end},
{"Key.F12", function(S) local v = Key.F12 if type(v) == "number" then return "F12=" .. v end return false, "type=" .. type(v) end},
{"ModifierKey.SHIFT", function(S) local v = ModifierKey.SHIFT if type(v) == "number" then return "v=" .. v end return false, "type=" .. type(v) end},
{"EObjectFlags.RF_Public", function(S) local v = EObjectFlags.RF_Public if type(v) == "number" then return "v=" .. v end return false, "type=" .. type(v) end},
{"EInternalObjectFlags.PendingKill", function(S) local v = EInternalObjectFlags.PendingKill if type(v) == "number" then return "v=" .. v end return false, "type=" .. type(v) end},
{"EFindName.FNAME_Find", function(S) local v = EFindName.FNAME_Find if type(v) == "number" then return "v=" .. v end return false, "type=" .. type(v) end},
-- [IU-GC call 8] upstream LuaMod.cpp register_all_property_types: each
-- PropertyTypes.<X> is a TABLE {Name, Size, FFieldClassPointer, StaticPointer}
-- (add_property_type_table). Port matches upstream. Old number expectation was
-- a test error; expect table with numeric Size.
{"PropertyTypes.ObjectProperty", function(S) local v = PropertyTypes.ObjectProperty if type(v) == "table" and type(v.Size) == "number" then return "table Size=" .. v.Size end return false, "type=" .. type(v) end},
{"UE4SS.GetVersion", function(S) local a, b, c = UE4SS:GetVersion() if type(a) == "number" and type(b) == "number" then return a .. "." .. b .. "." .. tostring(c) end return false, "types=" .. type(a) .. "," .. type(b) end},
{"UnrealVersion.GetMajor", function(S) local a = UnrealVersion:GetMajor() if type(a) == "number" then return tostring(a) end return false, "type=" .. type(a) end},
{"UnrealVersion.GetMinor", function(S) local a = UnrealVersion:GetMinor() if type(a) == "number" then return tostring(a) end return false, "type=" .. type(a) end},
{"UnrealVersion.IsEqual", function(S) local a = UnrealVersion:IsEqual(5, 6) if type(a) == "boolean" then return tostring(a) end return false, "type=" .. type(a) end},
{"UnrealVersion.IsAtLeast", function(S) local a = UnrealVersion:IsAtLeast(4, 0) if type(a) == "boolean" then return tostring(a) end return false, "type=" .. type(a) end},
{"UnrealVersion.IsAtMost", function(S) local a = UnrealVersion:IsAtMost(6, 0) if type(a) == "boolean" then return tostring(a) end return false, "type=" .. type(a) end},
{"UnrealVersion.IsBelow", function(S) local a = UnrealVersion:IsBelow(4, 0) if type(a) == "boolean" then return tostring(a) end return false, "type=" .. type(a) end},
{"UnrealVersion.IsAbove", function(S) local a = UnrealVersion:IsAbove(4, 0) if type(a) == "boolean" then return tostring(a) end return false, "type=" .. type(a) end},
{"EGameThreadMethod", function(S) if type(EGameThreadMethod) == "table" then return "fm=" .. tostring(EGameThreadMethod.FuncPtrMarshal) end return false, "type=" .. type(EGameThreadMethod) end},
{"EngineTickAvailable", function(S) if type(EngineTickAvailable) == "boolean" then return tostring(EngineTickAvailable) end return false, "type=" .. type(EngineTickAvailable) end},
{"ProcessEventAvailable", function(S) if type(ProcessEventAvailable) == "boolean" then return tostring(ProcessEventAvailable) end return false, "type=" .. type(ProcessEventAvailable) end},
-- [IU-GC call 20] upstream LuaMod::prepare_mod explicitly sets
-- __OriginalReturnValue = nil at mod start; it is only populated while a hook
-- post-callback is in flight (and current revisions pass the original return
-- value as the trailing callback arg instead). nil at rest is CORRECT parity.
{"__OriginalReturnValue", function(S) if type(__OriginalReturnValue) == "nil" then return "nil-at-rest(upstream-parity)" end return false, "type=" .. type(__OriginalReturnValue) end},
{"MainThread", function(S) if type(MainThread) == "thread" then return "thread" end return false, "type=" .. type(MainThread) end},
{"AsyncThread", function(S) if type(AsyncThread) == "thread" then return "thread" end return false, "type=" .. type(AsyncThread) end},
}
