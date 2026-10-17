--[[
 conformance_static.lua -- UE4SS Lua static conformance suite
 =============================================================
 Mac-only authored. Host-tested on macOS (syntax/logic dry-run against a
 mock UE4SS environment); reported as Mac-only / host-tested.
 NOT yet executed inside a real UE4SS/game session by this author.

 Purpose
 -------
 Tests every UE4SS Lua API that does NOT require live game state:
 pure computation, string types, type constructors, constant tables,
 read-only queries, and deterministic error paths. Every call is
 wrapped in pcall so a single failure never stops the suite.

 API list provenance (source read, static analysis only; tree root
 .superpowers/native-dependency-spike/native-coherent-runtime-1):
   source/port/ue4ss/src/Mod/LuaMod.cpp                  (globals, UE4SS/
     UnrealVersion/FPackageName/Key/ModifierKey/EObjectFlags/EInternalObjectFlags/
     EFindName/PropertyTypes, thread queries, delayed-action queries, string ctors)
   source/port/ue4ss/include/LuaType/LuaUnrealString.hpp (FString/FUtf8String/FAnsiString)
   source/port/ue4ss/src/LuaType/LuaFName.cpp            (FName)
   source/port/ue4ss/src/LuaType/LuaFText.cpp            (FText; ToString null-guarded)
   source/port/ue4ss/src/LuaType/LuaModRef.cpp           (ModRef shared-variable read)
   source/port/ue4ss/src/LuaType/LuaThreadId.cpp         (ThreadId ToString/__eq)
   source/port/ue4ss/include/LuaType/LuaUObject.hpp      (GetAddress/IsValid wrappers)
 Calling conventions verified against the pinned LuaMadeSimple dep
 (is_string/get_string/discard_value default to stack index 1): dot call
 for FPackageName (no self-discard in impl), colon call for userdata
 methods and UnrealVersion checks (impl discards the self table).

 Output format (machine-parseable, one line per call):
   CALL<TAB>api_name<TAB>params_summary<TAB>result_type<TAB>result_value_or_error<TAB>ok_or_fail
 Section markers:
   # SECTION: category_name
 Environment-dependent values (thread ids, addresses, dynamic name
 indices, handle counters, pointer hashes) are NORMALIZED to stable
 markers so logs remain diffable across runs/builds. Deterministic
 values are logged raw. See README.md for the full contract.

 Excluded by design (side effects / game state / heavy IO):
   Register*/Unregister*, Execute*/Loop*/delayed-action scheduling,
   FindFirstOf/FindAllOf/StaticFindObject/FindObject/FindObjects,
   LoadAsset, StaticConstructObject, ForEachUObject, NotifyOnNewObject,
   DumpAllObjects, GenerateSDK, GenerateLuaTypes,
   GenerateUHTCompatibleHeaders, DumpStaticMeshes, DumpAllActors,
   DumpUSMAP, DumpJMAP, IterateGameDirectories (heavy directory walk),
   RestartMod/RestartCurrentMod/UninstallMod/UninstallCurrentMod,
   ModRef:SetSharedVariable (persists across mods), LoadExport call
   (emits [Fatal] noise; presence-checked only).

 Usage: place as Mods/ConformanceStatic/scripts/main.lua, enable the
 mod, launch, then extract lines starting with "CALL\t" from the log.
--]]

-- ---------------------------------------------------------------------------
-- Small utilities (Lua 5.1/LuaJIT/Lua 5.4 compatible)
-- ---------------------------------------------------------------------------

local OUT_LINES = {}
local COUNTS    = { total = 0, ok = 0, fail = 0 }

local function sanitize(s)
    s = tostring(s)
    s = string.gsub(s, "\\", "\\\\")
    s = string.gsub(s, "\t", "\\t")
    s = string.gsub(s, "\r", "\\r")
    s = string.gsub(s, "\n", "\\n")
    if #s > 300 then s = string.sub(s, 1, 300) .. "..." end
    return s
end

local function is_integer_like(v)
    if math.type then
        local mt = math.type(v)
        return mt == "integer"
    end
    return type(v) == "number" and v % 1 == 0
end

local function userdata_type_name(v)
    local ok, t = pcall(function() return v:type() end)
    if ok and type(t) == "string" then return t end
    return "userdata"
end

-- Deterministic display for a single Lua value.
local function value_to_string(v)
    local tv = type(v)
    if tv == "nil" then return "nil"
    elseif tv == "boolean" then return tostring(v)
    elseif tv == "number" then return tostring(v)
    elseif tv == "string" then return "\"" .. sanitize(v) .. "\""
    elseif tv == "table" then
        local n = 0
        for _ in pairs(v) do n = n + 1 end
        return "table(n=" .. tostring(n) .. ")"
    elseif tv == "userdata" then return "userdata<" .. userdata_type_name(v) .. ">"
    elseif tv == "function" then return "function"
    elseif tv == "thread" then return "thread"
    else return sanitize(tostring(v))
    end
end

local function result_type_name(v)
    local tv = type(v)
    if tv == "number" then return is_integer_like(v) and "integer" or "float"
    elseif tv == "userdata" then return "userdata:" .. userdata_type_name(v)
    else return tv
    end
end

local function emit(line)
    table.insert(OUT_LINES, line)
    print(line)
end

local function section(name)
    emit("# SECTION: " .. name)
end

-- Normalizers for environment-dependent values ------------------------------
local function norm_positive_int(v)
    if type(v) ~= "number" then return value_to_string(v) end
    if v < 0 then return "int<0" end
    if v == 0 then return "0" end
    return "int>0"
end

local function norm_nonzero_int(v)
    if type(v) ~= "number" then return value_to_string(v) end
    if v == 0 then return "0" end
    return "int!=0"
end

local function norm_handle(v)
    if type(v) ~= "number" then return value_to_string(v) end
    if v < 0 then return "int<0" end
    return "int>=0"
end

-- pcall wrapper: run fn, return a structured record (error or value).
-- If fn already returns a structured record (e.g. a custom display/rtype),
-- it is passed through unwrapped.
local function try(fn)
    local ok, v = pcall(fn)
    if not ok then
        return { __conformance = true, errored = true, errmsg = tostring(v or "unknown error") }
    end
    if type(v) == "table" and v.__conformance then
        return v
    end
    return { __conformance = true, value = v }
end

-- ---------------------------------------------------------------------------
-- Core runner.
--   api    : stable call identifier (diff key; duplicates disambiguated by
--            occurrence order in the diff tool)
--   params : short deterministic params summary
--   fn     : closure returning ONE value (plain value, or a try() record;
--            records with .display/.rtype override presentation)
--   opts   : {
--     expect       = function(value) -> boolean  -- deterministic assertion
--     expect_error = true                         -- record MUST be an error
--     normalize    = function(value) -> string   -- stable display override
--   }
-- ok/fail semantics:
--   ok   = call succeeded (no Lua error) AND (expect, if given, held)
--   fail = call raised (value column carries the sanitized error text)
--          OR expectation not met (value still logged raw)
-- ---------------------------------------------------------------------------

local function run_call(api, params, fn, opts)
    opts = opts or {}
    COUNTS.total = COUNTS.total + 1

    local ok, v = pcall(fn)
    if not ok then
        -- The suite closure itself raised before/after the inner call.
        COUNTS.fail = COUNTS.fail + 1
        emit("CALL\t" .. api .. "\t" .. sanitize(params) .. "\terror\t"
                .. sanitize(tostring(v or "unknown error")) .. "\tfail")
        return
    end

    local value, display, rtype, errored, errmsg
    if type(v) == "table" and v.__conformance then
        if v.errored then
            errored = true
            errmsg  = v.errmsg or "error"
            local okrow = opts.expect_error == true
            COUNTS[okrow and "ok" or "fail"] = COUNTS[okrow and "ok" or "fail"] + 1
            emit("CALL\t" .. api .. "\t" .. sanitize(params) .. "\terror\t"
                    .. sanitize(errmsg) .. "\t" .. (okrow and "ok" or "fail"))
            return
        end
        value   = v.value
        display = v.display
        rtype   = v.rtype
    else
        value = v
    end

    if opts.normalize then
        display = opts.normalize(value)
    else
        display = display or value_to_string(value)
    end
    rtype = rtype or result_type_name(value)

    local okrow = true
    if opts.expect and not opts.expect(value) then okrow = false end
    COUNTS[okrow and "ok" or "fail"] = COUNTS[okrow and "ok" or "fail"] + 1
    emit("CALL\t" .. api .. "\t" .. sanitize(params) .. "\t" .. rtype .. "\t"
            .. sanitize(display) .. "\t" .. (okrow and "ok" or "fail"))
end

-- ---------------------------------------------------------------------------
-- Header / environment info
-- ---------------------------------------------------------------------------

emit("# UE4SS Lua static conformance suite v1 (conformance_static.lua)")
emit("# FORMAT: CALL<TAB>api<TAB>params<TAB>result_type<TAB>result_value_or_error<TAB>ok_or_fail")
emit("# Normalized markers: int>0 int!=0 int<0 int>=0 table(n=..) userdata<..>")

section("env_info")

run_call("_VERSION", "()", function() return _VERSION end)

run_call("jit.version", "()", function()
    if type(jit) == "table" then return jit.version end
    return "not-available"
end, { normalize = value_to_string })

run_call("jit.arch", "()", function()
    if type(jit) == "table" and jit.arch then return jit.arch end
    return "not-available"
end, { normalize = value_to_string })

run_call("platform", "(queryable sources: jit.os)", function()
    if type(jit) == "table" and jit.os then return jit.os end
    return "not-queryable-via-lua"
end, { normalize = value_to_string })

run_call("UE4SS.GetVersion", "() -> major,minor,hotfix", function()
    return try(function()
        local a, b, c = UE4SS.GetVersion()
        return { __conformance = true, value = a,
                 display = tostring(a) .. " | " .. tostring(b) .. " | " .. tostring(c),
                 rtype = "integer,integer,integer" }
    end)
end)

run_call("UnrealVersion.GetMajor", "()", function()
    return try(function() return UnrealVersion.GetMajor() end)
end)

run_call("UnrealVersion.GetMinor", "()", function()
    return try(function() return UnrealVersion.GetMinor() end)
end)

do
    local maj, min = 0, 0
    pcall(function() maj = UnrealVersion.GetMajor() or 0 end)
    pcall(function() min = UnrealVersion.GetMinor() or 0 end)
    run_call("UnrealVersion.IsEqual", "(" .. tostring(maj) .. "," .. tostring(min) .. ")",
            function() return try(function() return UnrealVersion:IsEqual(maj, min) end) end,
            { expect = function(v) return v == true end })
    run_call("UnrealVersion.IsEqual", "(99999,99999)",
            function() return try(function() return UnrealVersion:IsEqual(99999, 99999) end) end,
            { expect = function(v) return v == false end })
    run_call("UnrealVersion.IsAtLeast", "(1,0)",
            function() return try(function() return UnrealVersion:IsAtLeast(1, 0) end) end,
            { expect = function(v) return v == true end })
    run_call("UnrealVersion.IsAtMost", "(99999,0)",
            function() return try(function() return UnrealVersion:IsAtMost(99999, 0) end) end,
            { expect = function(v) return v == true end })
    run_call("UnrealVersion.IsBelow", "(1,0)",
            function() return try(function() return UnrealVersion:IsBelow(1, 0) end) end,
            { expect = function(v) return v == false end })
    run_call("UnrealVersion.IsAbove", "(99999,0)",
            function() return try(function() return UnrealVersion:IsAbove(99999, 0) end) end,
            { expect = function(v) return v == false end })
    run_call("UnrealVersion.IsEqual", "(\"bad\",1) [error path]", function()
        return try(function() return UnrealVersion:IsEqual("bad", 1) end)
    end, { expect_error = true })
end

do
    local okm, m = pcall(function() return EGameThreadMethod end)
    if okm and type(m) == "table" then
        run_call("EGameThreadMethod.EngineTick", "()", function() return m.EngineTick end)
        run_call("EGameThreadMethod.ProcessEvent", "()", function() return m.ProcessEvent end)
    else
        run_call("EGameThreadMethod.EngineTick", "()", function() error("EGameThreadMethod not a table") end)
        run_call("EGameThreadMethod.ProcessEvent", "()", function() error("EGameThreadMethod not a table") end)
    end
end

run_call("EngineTickAvailable", "()", function()
    return try(function() return EngineTickAvailable end)
end)

run_call("ProcessEventAvailable", "()", function()
    return try(function() return ProcessEventAvailable end)
end)

-- ---------------------------------------------------------------------------
-- Global presence (type() of every registered global; no invocation)
-- ---------------------------------------------------------------------------

section("global_presence")

local PRESENCE = {
    "UE4SS", "UnrealVersion", "FName", "NAME_None", "FText",
    "FString", "FUtf8String", "FAnsiString", "FPackageName",
    "Key", "ModifierKey", "EObjectFlags", "EInternalObjectFlags",
    "EFindName", "PropertyTypes", "ModRef", "EGameThreadMethod",
    "GetCurrentThreadId", "GetMainModThreadId", "GetAsyncThreadId",
    "GetGameThreadId", "IsInMainModThread", "IsInAsyncThread",
    "IsInGameThread", "MakeActionHandle", "IsValidDelayedActionHandle",
    "IsDelayedActionActive", "IsDelayedActionPaused",
    "GetDelayedActionTimeRemaining", "GetDelayedActionTimeElapsed",
    "GetDelayedActionRate", "IsKeyBindRegistered",
    "CreateInvalidObject", "__OriginalReturnValue",
    "ue4ss_loaded_modules", "print", "LoadExport",
    -- Presence only; NEVER called by this suite (side effects):
    "RegisterHook", "NotifyOnNewObject", "RegisterKeyBind",
    "ExecuteAsync", "ExecuteWithDelay", "LoopAsync",
    "ExecuteInGameThread", "RestartMod", "UninstallMod",
    "FindFirstOf", "FindAllOf", "StaticFindObject", "FindObject",
    "FindObjects", "LoadAsset", "StaticConstructObject",
    "ForEachUObject", "DumpAllObjects", "GenerateSDK",
    "RegisterCustomProperty", "RegisterCustomEvent",
}
for _, g in ipairs(PRESENCE) do
    run_call("presence:" .. g, "type(_G[name])", function()
        local ok, v = pcall(function() return _G[g] end)
        if not ok then error(v) end
        return v
    end, { normalize = function(v)
        local tv = type(v)
        if tv == "table" then
            local n = 0
            for _ in pairs(v) do n = n + 1 end
            return "table(n=" .. tostring(n) .. ")"
        elseif tv == "userdata" then return "userdata<" .. userdata_type_name(v) .. ">"
        elseif tv == "function" then return "function"
        else return tv end
    end })
end

-- ---------------------------------------------------------------------------
-- FName (constructor metamethods, methods, metamethods, error paths)
-- ---------------------------------------------------------------------------

section("fname")

run_call("FName", "(\"ConformanceStaticAlpha\")", function()
    return try(function() return FName("ConformanceStaticAlpha") end)
end, { expect = function(v)
    return userdata_type_name(v):find("FName", 1, true) ~= nil
end })

run_call("FName:ToString", "(\"ConformanceStaticAlpha\")", function()
    return try(function() return FName("ConformanceStaticAlpha"):ToString() end)
end, { expect = function(v) return v == "ConformanceStaticAlpha" end })

run_call("FName:GetComparisonIndex", "(\"ConformanceStaticAlpha\") [normalized]", function()
    return try(function() return FName("ConformanceStaticAlpha"):GetComparisonIndex() end)
end, { normalize = norm_positive_int })

run_call("FName:type", "(instance)", function()
    return try(function() return FName("ConformanceStaticAlpha"):type() end)
end)

run_call("FName:Equals", "(same-name)", function()
    return try(function() return FName("ConformanceStaticAlpha"):Equals(FName("ConformanceStaticAlpha")) end)
end, { expect = function(v) return v == true end })

run_call("FName:Equals", "(different-name)", function()
    return try(function() return FName("ConformanceStaticAlpha"):Equals(FName("ConformanceStaticBeta")) end)
end, { expect = function(v) return v == false end })

run_call("FName.__eq", "(same-name)", function()
    return try(function() return FName("ConformanceStaticGamma") == FName("ConformanceStaticGamma") end)
end, { expect = function(v) return v == true end })

run_call("FName.__eq", "(different-name)", function()
    return try(function() return FName("ConformanceStaticGamma") == FName("ConformanceStaticDelta") end)
end, { expect = function(v) return v == false end })

run_call("FName", "(0) [integer overload]", function()
    return try(function() return FName(0) end)
end)

run_call("FName:ToString", "(FName(0))", function()
    return try(function() return FName(0):ToString() end)
end, { expect = function(v) return v == "None" end })

run_call("NAME_None:ToString", "()", function()
    return try(function() return NAME_None:ToString() end)
end, { expect = function(v) return v == "None" end })

run_call("NAME_None:GetComparisonIndex", "()", function()
    return try(function() return NAME_None:GetComparisonIndex() end)
end, { expect = function(v) return v == 0 end })

run_call("FName.__eq", "(FName(0) == NAME_None)", function()
    return try(function() return FName(0) == NAME_None end)
end, { expect = function(v) return v == true end })

run_call("FName", "(\"ConformanceStaticMissing\", FNAME_Find) [find-miss behavior]", function()
    return try(function() return FName("ConformanceStaticMissing", EFindName.FNAME_Find) end)
end)

run_call("FName", "() [error path]", function()
    return try(function() return FName() end)
end, { expect_error = true })

run_call("FName", "(-1) [error path]", function()
    return try(function() return FName(-1) end)
end, { expect_error = true })

run_call("FName", "(4294967296) [error path]", function()
    return try(function() return FName(4294967296) end)
end, { expect_error = true })

-- ---------------------------------------------------------------------------
-- FString / FUtf8String / FAnsiString batteries
-- ---------------------------------------------------------------------------

local STRING_CTORS = { "FString", "FUtf8String", "FAnsiString" }
local SAMPLE       = "Hello Conformance"

for _, ctor in ipairs(STRING_CTORS) do
    section(string.lower(ctor))

    run_call(ctor .. ".ctor", "(\"" .. SAMPLE .. "\")", function()
        return try(function() return _G[ctor](SAMPLE) end)
    end, { expect = function(v)
        return userdata_type_name(v) == ctor
    end })

    run_call(ctor .. ":ToString", "(roundtrip)", function()
        return try(function() return _G[ctor](SAMPLE):ToString() end)
    end, { expect = function(v) return v == SAMPLE end })

    run_call(ctor .. ":Len", "()", function()
        return try(function() return _G[ctor](SAMPLE):Len() end)
    end, { expect = function(v) return v == #SAMPLE end })

    run_call(ctor .. ":IsEmpty", "(non-empty)", function()
        return try(function() return _G[ctor](SAMPLE):IsEmpty() end)
    end, { expect = function(v) return v == false end })

    run_call(ctor .. ":Find", "(\"Conformance\") 1-based", function()
        return try(function() return _G[ctor](SAMPLE):Find("Conformance") end)
    end, { expect = function(v) return v == 7 end })

    run_call(ctor .. ":Find", "(\"ZZZNOTPRESENT\")", function()
        return try(function() return _G[ctor](SAMPLE):Find("ZZZNOTPRESENT") end)
    end, { expect = function(v) return v == nil end })

    run_call(ctor .. ":StartsWith", "(\"Hello\")", function()
        return try(function() return _G[ctor](SAMPLE):StartsWith("Hello") end)
    end, { expect = function(v) return v == true end })

    run_call(ctor .. ":StartsWith", "(\"Xello\")", function()
        return try(function() return _G[ctor](SAMPLE):StartsWith("Xello") end)
    end, { expect = function(v) return v == false end })

    run_call(ctor .. ":EndsWith", "(\"Conformance\")", function()
        return try(function() return _G[ctor](SAMPLE):EndsWith("Conformance") end)
    end, { expect = function(v) return v == true end })

    run_call(ctor .. ":ToUpper", "()", function()
        return try(function() return _G[ctor](SAMPLE):ToUpper():ToString() end)
    end, { expect = function(v) return v == "HELLO CONFORMANCE" end })

    run_call(ctor .. ":ToLower", "(after ToUpper)", function()
        return try(function() return _G[ctor](SAMPLE):ToUpper():ToLower():ToString() end)
    end, { expect = function(v) return v == string.lower(SAMPLE) end })

    run_call(ctor .. ":Append", "(\"-appended\") then :Len", function()
        local s = _G[ctor](SAMPLE)
        s:Append("-appended")
        return s:Len()
    end, { expect = function(v) return v == #SAMPLE + 9 end })

    run_call(ctor .. ":Append", "(userdata) then :Len", function()
        local s = _G[ctor](SAMPLE)
        s:Append(_G[ctor]("++"))
        return s:Len()
    end, { expect = function(v) return v == #SAMPLE + 2 end })

    run_call(ctor .. ":Append", "(\"-appended\") then :ToString", function()
        local s = _G[ctor](SAMPLE)
        s:Append("-appended")
        return s:ToString()
    end, { expect = function(v) return v == SAMPLE .. "-appended" end })

    run_call(ctor .. ":Empty", "() then IsEmpty/Len/ToString", function()
        local s = _G[ctor](SAMPLE)
        s:Empty()
        return tostring(s:IsEmpty()) .. "/" .. tostring(s:Len()) .. "/" .. s:ToString()
    end, { expect = function(v) return v == "true/0/" end })

    run_call(ctor .. ":Clear", "() then IsEmpty/Len", function()
        local s = _G[ctor](SAMPLE)
        s:Clear()
        return tostring(s:IsEmpty()) .. "/" .. tostring(s:Len())
    end, { expect = function(v) return v == "true/0" end })

    run_call(ctor .. ":type", "()", function()
        return try(function() return _G[ctor](SAMPLE):type() end)
    end, { expect = function(v) return v == ctor end })

    run_call(ctor .. ".ctor", "() [error path]", function()
        return try(function() return _G[ctor]() end)
    end, { expect_error = true })

    run_call(ctor .. ".ctor", "(123) [error path]", function()
        return try(function() return _G[ctor](123) end)
    end, { expect_error = true })

    run_call(ctor .. ":Find", "(123) [error path]", function()
        return try(function() return _G[ctor](SAMPLE):Find(123) end)
    end, { expect_error = true })

    run_call(ctor .. ":Append", "(123) [error path]", function()
        return try(function() return _G[ctor](SAMPLE):Append(123) end)
    end, { expect_error = true })
end

-- ---------------------------------------------------------------------------
-- FText (global default instance; ToString is null-guarded in FText.cpp)
-- ---------------------------------------------------------------------------

section("ftext")

run_call("FText:type", "(global default instance)", function()
    return try(function() return FText:type() end)
end)

run_call("FText:ToString", "(global default instance; expect empty)", function()
    return try(function() return FText:ToString() end)
end, { expect = function(v) return v == "" end })

-- ---------------------------------------------------------------------------
-- FPackageName (pure string logic; DOT call: impl reads params at stack
-- index 1 and registers no self-discard, unlike UnrealVersion helpers)
-- ---------------------------------------------------------------------------

section("fpackagename")

run_call("FPackageName.IsShortPackageName", "(\"MyMap\")", function()
    return try(function() return FPackageName.IsShortPackageName("MyMap") end)
end, { expect = function(v) return v == true end })

run_call("FPackageName.IsShortPackageName", "(\"/Game/Maps/MyMap\")", function()
    return try(function() return FPackageName.IsShortPackageName("/Game/Maps/MyMap") end)
end, { expect = function(v) return v == false end })

run_call("FPackageName.IsValidLongPackageName", "(\"/Game/Maps/MyMap\")", function()
    return try(function() return FPackageName.IsValidLongPackageName("/Game/Maps/MyMap") end)
end, { expect = function(v) return v == true end })

run_call("FPackageName.IsValidLongPackageName", "(\"MyMap\")", function()
    return try(function() return FPackageName.IsValidLongPackageName("MyMap") end)
end, { expect = function(v) return v == false end })

run_call("FPackageName.IsShortPackageName", "() [error path]", function()
    return try(function() return FPackageName.IsShortPackageName() end)
end, { expect_error = true })

-- ---------------------------------------------------------------------------
-- ModRef (read-only shared variable lookup; Set intentionally not called)
-- ---------------------------------------------------------------------------

section("modref")

run_call("ModRef:type", "()", function()
    return try(function() return ModRef:type() end)
end)

run_call("ModRef:GetSharedVariable", "(\"ConformanceStaticUnset\")", function()
    return try(function() return ModRef:GetSharedVariable("ConformanceStaticUnset") end)
end, { expect = function(v) return v == nil end })

-- ---------------------------------------------------------------------------
-- Thread identity queries (values normalized; booleans deterministic)
-- ---------------------------------------------------------------------------

section("threads")

run_call("GetCurrentThreadId", "()", function()
    return try(function() return GetCurrentThreadId() end)
end, { normalize = value_to_string })

run_call("GetCurrentThreadId:type", "()", function()
    return try(function() return GetCurrentThreadId():type() end)
end)

run_call("GetMainModThreadId", "()", function()
    return try(function() return GetMainModThreadId() end)
end, { normalize = value_to_string })

run_call("GetAsyncThreadId", "()", function()
    return try(function() return GetAsyncThreadId() end)
end, { normalize = value_to_string })

run_call("GetGameThreadId", "()", function()
    return try(function() return GetGameThreadId() end)
end, { normalize = value_to_string })

run_call("ThreadId:type", "(GetMainModThreadId())", function()
    return try(function() return GetMainModThreadId():type() end)
end)

run_call("ThreadId:ToString", "(GetCurrentThreadId()) [nonempty check]", function()
    local ok, s = pcall(function() return GetCurrentThreadId():ToString() end)
    if not ok then return { __conformance = true, errored = true, errmsg = tostring(s) } end
    return #s > 0
end, { expect = function(v) return v == true end })

run_call("ThreadId.__eq", "(GetCurrentThreadId() == itself)", function()
    local ok, a = pcall(GetCurrentThreadId)
    if not ok then return { __conformance = true, errored = true, errmsg = tostring(a) } end
    return try(function() return a == GetCurrentThreadId() end)
end, { expect = function(v) return v == true end })

run_call("IsInMainModThread", "()", function()
    return try(function() return IsInMainModThread() end)
end, { expect = function(v) return v == true end })

run_call("IsInAsyncThread", "()", function()
    return try(function() return IsInAsyncThread() end)
end, { expect = function(v) return v == false end })

run_call("IsInGameThread", "()", function()
    return try(function() return IsInGameThread() end)
end, { expect = function(v) return v == false end })

-- ---------------------------------------------------------------------------
-- Delayed-action read-only queries (no actions are ever scheduled here)
-- ---------------------------------------------------------------------------

section("delayed_actions")

local INVALID_HANDLE = 123456789 -- never allocated by this suite; counter starts near 0

run_call("MakeActionHandle", "() #1 [normalized]", function()
    return try(function() return MakeActionHandle() end)
end, { normalize = norm_handle })

do
    local ok, h1 = pcall(MakeActionHandle)
    local ok2, h2 = pcall(MakeActionHandle)
    run_call("MakeActionHandle", "() #2 uniqueness", function()
        if not ok or not ok2 then error("MakeActionHandle failed") end
        return h2 ~= h1
    end, { expect = function(v) return v == true end })
end

run_call("IsValidDelayedActionHandle", "(" .. INVALID_HANDLE .. ")", function()
    return try(function() return IsValidDelayedActionHandle(INVALID_HANDLE) end)
end, { expect = function(v) return v == false end })

run_call("IsDelayedActionActive", "(" .. INVALID_HANDLE .. ")", function()
    return try(function() return IsDelayedActionActive(INVALID_HANDLE) end)
end, { expect = function(v) return v == false end })

run_call("IsDelayedActionPaused", "(" .. INVALID_HANDLE .. ")", function()
    return try(function() return IsDelayedActionPaused(INVALID_HANDLE) end)
end, { expect = function(v) return v == false end })

run_call("GetDelayedActionTimeRemaining", "(" .. INVALID_HANDLE .. ")", function()
    return try(function() return GetDelayedActionTimeRemaining(INVALID_HANDLE) end)
end, { expect = function(v) return v == -1 end })

run_call("GetDelayedActionTimeElapsed", "(" .. INVALID_HANDLE .. ")", function()
    return try(function() return GetDelayedActionTimeElapsed(INVALID_HANDLE) end)
end)

run_call("GetDelayedActionRate", "(" .. INVALID_HANDLE .. ")", function()
    return try(function() return GetDelayedActionRate(INVALID_HANDLE) end)
end)

run_call("IsValidDelayedActionHandle", "(\"notanint\") [error path]", function()
    return try(function() return IsValidDelayedActionHandle("notanint") end)
end, { expect_error = true })

-- ---------------------------------------------------------------------------
-- Keybind registration query (read-only)
-- ---------------------------------------------------------------------------

section("keybind_query")

run_call("IsKeyBindRegistered", "(Key.F6) [false unless another mod bound it]", function()
    return try(function() return IsKeyBindRegistered(Key.F6) end)
end)

run_call("IsKeyBindRegistered", "(999) [error path: out of 0..255]", function()
    return try(function() return IsKeyBindRegistered(999) end)
end, { expect_error = true })

-- ---------------------------------------------------------------------------
-- Constant tables (deterministic enum values; fixed key order)
-- ---------------------------------------------------------------------------

section("constants")

for _, k in ipairs({ "FNAME_Find", "FNAME_Add", "FNAME_Replace_Not_Safe_For_Threading" }) do
    run_call("EFindName." .. k, "()", function()
        local ok, t = pcall(function() return EFindName end)
        if not ok or type(t) ~= "table" then error("EFindName missing") end
        return t[k]
    end)
end

for _, k in ipairs({ "SHIFT", "CONTROL", "ALT" }) do
    run_call("ModifierKey." .. k, "()", function()
        local ok, t = pcall(function() return ModifierKey end)
        if not ok or type(t) ~= "table" then error("ModifierKey missing") end
        return t[k]
    end)
end

local EOBJECT_FLAGS = {
    "RF_NoFlags", "RF_Public", "RF_Standalone", "RF_MarkAsNative",
    "RF_Transactional", "RF_ClassDefaultObject", "RF_ArchetypeObject",
    "RF_Transient", "RF_MarkAsRootSet", "RF_TagGarbageTemp",
    "RF_NeedInitialization", "RF_NeedLoad", "RF_KeepForCooker",
    "RF_NeedPostLoad", "RF_NeedPostLoadSubobjects", "RF_NewerVersionExists",
    "RF_BeginDestroyed", "RF_FinishDestroyed", "RF_BeingRegenerated",
    "RF_DefaultSubObject", "RF_WasLoaded", "RF_TextExportTransient",
    "RF_LoadCompleted", "RF_InheritableComponentTemplate",
    "RF_DuplicateTransient", "RF_StrongRefOnFrame",
    "RF_NonPIEDuplicateTransient", "RF_Dynamic", "RF_WillBeLoaded",
    "RF_HasExternalPackage", "RF_AllFlags",
}
for _, k in ipairs(EOBJECT_FLAGS) do
    run_call("EObjectFlags." .. k, "()", function()
        local ok, t = pcall(function() return EObjectFlags end)
        if not ok or type(t) ~= "table" then error("EObjectFlags missing") end
        return t[k]
    end, { normalize = (k == "RF_AllFlags") and norm_nonzero_int or nil })
end

local EINTERNAL_FLAGS = {
    "ReachableInCluster", "ClusterRoot", "Native", "Async", "AsyncLoading",
    "Unreachable", "PendingKill", "RootSet", "GarbageCollectionKeepFlags",
    "AllFlags",
}
for _, k in ipairs(EINTERNAL_FLAGS) do
    run_call("EInternalObjectFlags." .. k, "()", function()
        local ok, t = pcall(function() return EInternalObjectFlags end)
        if not ok or type(t) ~= "table" then error("EInternalObjectFlags missing") end
        return t[k]
    end, { normalize = (k == "AllFlags") and norm_nonzero_int or nil })
end

-- Key table: fixed spot-check list + key count (pure Lua iteration).
local KEY_SPOT = { "LEFT_MOUSE_BUTTON", "MIDDLE_MOUSE_BUTTON", "BACKSPACE",
                   "TAB", "RETURN", "ESCAPE", "SPACE", "END", "HOME",
                   "A", "Z", "NUM_ZERO", "F1", "F12", "LEFT_ARROW",
                   "LEFT_SHIFT", "LEFT_CONTROL", "LEFT_ALT", "RIGHT_SHIFT" }
for _, k in ipairs(KEY_SPOT) do
    run_call("Key." .. k, "()", function()
        local ok, t = pcall(function() return Key end)
        if not ok or type(t) ~= "table" then error("Key missing") end
        return t[k]
    end)
end

run_call("Key.count", "(#distinct keys)", function()
    local ok, t = pcall(function() return Key end)
    if not ok or type(t) ~= "table" then error("Key missing") end
    local n = 0
    for _ in pairs(t) do n = n + 1 end
    return n
end)

-- PropertyTypes: fixed sorted name list; Size deterministic per build,
-- FFieldClassPointer normalized (pointer hash), StaticPointer expected 0.
local PROPERTY_TYPE_NAMES = {
    "ArrayProperty", "BoolProperty", "ByteProperty", "ClassProperty",
    "EnumProperty", "FloatProperty", "Int16Property", "Int64Property",
    "Int8Property", "IntProperty", "MapProperty", "NameProperty",
    "ObjectProperty", "ObjectPtrProperty", "SetProperty", "StrProperty",
    "StructProperty", "TextProperty", "UInt16Property", "UInt32Property",
    "UInt64Property", "Utf8StrProperty", "WeakObjectProperty",
}
for _, name in ipairs(PROPERTY_TYPE_NAMES) do
    run_call("PropertyTypes." .. name .. ".Name", "()", function()
        local ok, t = pcall(function() return PropertyTypes end)
        if not ok or type(t) ~= "table" then error("PropertyTypes missing") end
        local e = t[name]
        if type(e) ~= "table" then return nil end
        return e.Name
    end, { expect = function(v) return v == name end })

    run_call("PropertyTypes." .. name .. ".Size", "()", function()
        local ok, t = pcall(function() return PropertyTypes end)
        if not ok or type(t) ~= "table" then error("PropertyTypes missing") end
        local e = t[name]
        if type(e) ~= "table" then return nil end
        return e.Size
    end)

    run_call("PropertyTypes." .. name .. ".FFieldClassPointer", "[normalized]", function()
        local ok, t = pcall(function() return PropertyTypes end)
        if not ok or type(t) ~= "table" then error("PropertyTypes missing") end
        local e = t[name]
        if type(e) ~= "table" then return nil end
        return e.FFieldClassPointer
    end, { normalize = norm_nonzero_int })

    run_call("PropertyTypes." .. name .. ".StaticPointer", "()", function()
        local ok, t = pcall(function() return PropertyTypes end)
        if not ok or type(t) ~= "table" then error("PropertyTypes missing") end
        local e = t[name]
        if type(e) ~= "table" then return nil end
        return e.StaticPointer
    end, { expect = function(v) return v == 0 end })
end

-- ---------------------------------------------------------------------------
-- Invalid-object wrapper (null UObject; wrapper-level methods only)
-- ---------------------------------------------------------------------------

section("invalid_object")

run_call("CreateInvalidObject", "()", function()
    return try(function() return CreateInvalidObject() end)
end)

run_call("CreateInvalidObject:type", "()", function()
    return try(function() return CreateInvalidObject():type() end)
end)

run_call("CreateInvalidObject:IsValid", "()", function()
    return try(function() return CreateInvalidObject():IsValid() end)
end, { expect = function(v) return v == false end })

run_call("CreateInvalidObject:GetAddress", "(null wrapper; expect 0)", function()
    return try(function() return CreateInvalidObject():GetAddress() end)
end, { expect = function(v) return v == 0 end })

-- ---------------------------------------------------------------------------
-- Misc globals
-- ---------------------------------------------------------------------------

section("misc")

run_call("__OriginalReturnValue", "()", function()
    return try(function() return __OriginalReturnValue end)
end, { expect = function(v) return v == nil end })

run_call("ue4ss_loaded_modules", "(type)", function()
    return try(function() return ue4ss_loaded_modules end)
end, { normalize = function(v)
        if type(v) ~= "table" then return type(v) end
        local n = 0
        for _ in pairs(v) do n = n + 1 end
        return "table(n=" .. tostring(n) .. ")"
    end })

run_call("print", "(smoke call)", function()
    return try(function() print("ConformanceStatic: print smoke OK") return true end)
end, { expect = function(v) return v == true end })

-- ---------------------------------------------------------------------------
-- Footer summary (human-readable; parsers ignore non-CALL lines)
-- ---------------------------------------------------------------------------

emit(string.format("# SUMMARY: total=%d ok=%d fail=%d", COUNTS.total, COUNTS.ok, COUNTS.fail))
emit("# END OF conformance_static.lua")

return { lines = OUT_LINES, counts = COUNTS }
