--[[
 hosttest_mock_env.lua -- Mac-only host test harness (NOT part of the
 conformance deliverable's runtime; used to execute conformance_static.lua
 under a plain Lua interpreter with a mock UE4SS environment so the suite's
 syntax, runner, formatting, and pcall isolation are verified on the host).

 The mock mirrors semantics read from the UE4SS sources (LuaMod.cpp,
 LuaUnrealString.hpp, LuaFName.cpp, ...). It is NOT the real engine;
 sample logs generated through it are fixtures for testing diff_results.py.

 Variant control: MOCK_VARIANT=b (env var) introduces a handful of
 deliberate behavioral changes (version bump, a PropertyTypes size change,
 FText no longer empty, FName find-miss errors) to produce realistic
 MISMATCH rows for diff-tool testing.
--]]

local VARIANT = os and os.getenv and os.getenv("MOCK_VARIANT") or "a"

-- ----------------------------- small helpers ------------------------------
local function callable_ctor(make)
    return setmetatable({}, { __call = function(_, ...) return make(...) end })
end

-- ------------------------------- UE4SS ------------------------------------
UE4SS = {}
if VARIANT == "b" then
    function UE4SS.GetVersion() return 3, 1, 0 end
else
    function UE4SS.GetVersion() return 3, 0, 1 end
end

-- ---------------------------- UnrealVersion -------------------------------
do
    local MAJOR, MINOR = 5, 3
    UnrealVersion = {
        GetMajor = function() return MAJOR end,
        GetMinor = function() return MINOR end,
        IsEqual    = function(self, a, b) if type(a) ~= "number" or type(b) ~= "number" then error("No overload found for function 'UnrealVersion:IsEqual'.") end return a == MAJOR and b == MINOR end,
        IsAtLeast  = function(self, a, b) if type(a) ~= "number" or type(b) ~= "number" then error("No overload found for function 'UnrealVersion:IsAtLeast'.") end return MAJOR > a or (MAJOR == a and MINOR >= b) end,
        IsAtMost   = function(self, a, b) if type(a) ~= "number" or type(b) ~= "number" then error("No overload found for function 'UnrealVersion:IsAtMost'.") end return MAJOR < a or (MAJOR == a and MINOR <= b) end,
        IsBelow    = function(self, a, b) if type(a) ~= "number" or type(b) ~= "number" then error("No overload found for function 'UnrealVersion:IsBelow'.") end return MAJOR < a or (MAJOR == a and MINOR < b) end,
        IsAbove    = function(self, a, b) if type(a) ~= "number" or type(b) ~= "number" then error("No overload found for function 'UnrealVersion:IsAbove'.") end return MAJOR > a or (MAJOR == a and MINOR > b) end,
    }
end

-- -------------------------------- FName -----------------------------------
do
    local fname_mt
    local function make_fname(a, b)
        local value, index
        if type(a) == "string" then
            local find = (b == nil) and 1 or b -- EFindName: 1=FNAME_Add, 0=FNAME_Find
            if find == 0 and a:sub(1, 17) == "ConformanceStatic" and a ~= "None" then
                -- Mock: FNAME_Find miss yields NAME_None (matches UE semantics)
                value, index = "None", 0
            else
                value, index = a, 7331
            end
        elseif type(a) == "number" then
            if a < 0 then error("FName constructor cannot take an integer smaller than uint32.") end
            if a > 4294967295 then error("FName constructor cannot take an integer larger than uint32.") end
            value, index = (a == 0) and "None" or ("MockName" .. a), a
        else
            error("No overload found for function 'FName'.")
        end
        return setmetatable({ v = value, i = index }, fname_mt)
    end
    fname_mt = {
        __eq = function(x, y) return x.v == y.v and x.i == y.i end,
        __call = function(_, a, b) return make_fname(a, b) end,
        __index = {
            ToString = function(self) return self.v end,
            GetComparisonIndex = function(self) return self.i end,
            Equals = function(self, other) return self.v == other.v and self.i == other.i end,
            type = function() return "FNameUserdata" end,
        },
    }
    FName = setmetatable({}, { __call = function(_, a, b) return make_fname(a, b) end })
    NAME_None = make_fname(0)
    EFindName = { FNAME_Find = 0, FNAME_Add = 1, FNAME_Replace_Not_Safe_For_Threading = 2 }
end

-- ---------------------- FString / FUtf8String / FAnsiString ----------------
local function make_string_type(typestr)
    local mt
    local function construct(s)
        if type(s) ~= "string" then
            error(typestr .. " constructor requires a string argument")
        end
        return setmetatable({ v = s }, mt)
    end
    mt = {
        __eq = function(x, y) return x.v == y.v end,
        __index = {
            ToString  = function(self) return self.v end,
            Len       = function(self) return #self.v end,
            IsEmpty   = function(self) return #self.v == 0 end,
            Empty     = function(self) self.v = "" end,
            Clear     = function(self) self.v = "" end,
            Append    = function(self, other)
                if type(other) == "string" then self.v = self.v .. other
                elseif type(other) == "table" and getmetatable(other) == mt then self.v = self.v .. other.v
                else error("Append requires a string argument") end
            end,
            Find      = function(self, sub)
                if type(sub) ~= "string" then error("Find requires a string argument") end
                local s, e = string.find(self.v, sub, 1, true)
                if not s then return nil end
                return s
            end,
            StartsWith = function(self, p)
                if type(p) ~= "string" then error("StartsWith requires a string argument") end
                return self.v:sub(1, #p) == p
            end,
            EndsWith   = function(self, p)
                if type(p) ~= "string" then error("EndsWith requires a string argument") end
                return #p == 0 or self.v:sub(-#p) == p
            end,
            ToUpper   = function(self) return setmetatable({ v = string.upper(self.v) }, mt) end,
            ToLower   = function(self) return setmetatable({ v = string.lower(self.v) }, mt) end,
            type      = function() return typestr end,
        },
    }
    return callable_ctor(construct), mt
end
FString,      FString_mt      = make_string_type("FString")
FUtf8String,  FUtf8String_mt  = make_string_type("FUtf8String")
FAnsiString,  FAnsiString_mt  = make_string_type("FAnsiString")

-- -------------------------------- FText ------------------------------------
do
    local text_mt = {
        __index = {
            ToString = function()
                if VARIANT == "b" then return "mock-text" end
                return ""
            end,
            type = function() return "FTextUserdata" end,
        },
    }
    FText = setmetatable({}, text_mt)
end

-- ----------------------------- FPackageName --------------------------------
FPackageName = {
    IsShortPackageName = function(s)
        if type(s) ~= "string" then error("No overload found for function 'FPackageName:IsShortPackageName'.") end
        return not s:find("/", 1, true)
    end,
    IsValidLongPackageName = function(s)
        if type(s) ~= "string" then error("No overload found for function 'FPackageName:IsValidLongPackageName'.") end
        return s:sub(1, 1) == "/"
    end,
}

-- ------------------------------- ModRef ------------------------------------
do
    local modref_mt = {
        __index = {
            GetSharedVariable = function(self, name)
                if type(name) ~= "string" then
                    error("No overload found for function 'GetSharedVariable'.")
                end
                return nil
            end,
            type = function() return "ModRefUserdata" end,
        },
    }
    ModRef = setmetatable({}, modref_mt)
end

-- ------------------------------- Threads -----------------------------------
do
    local thread_id_mt = {
        __eq = function(x, y) return x.id == y.id end,
        __index = {
            ToString = function(self) return tostring(self.id) end,
            type = function() return "ThreadIdUserdata" end,
        },
    }
    local function make_tid(id) return setmetatable({ id = id }, thread_id_mt) end
    local MAIN_TID = make_tid(140233)
    local ASYNC_TID = make_tid(140234)
    local GAME_TID = make_tid(140232)
    GetCurrentThreadId  = function() return MAIN_TID end -- main.lua runs in main mod thread
    GetMainModThreadId  = function() return MAIN_TID end
    GetAsyncThreadId    = function() return ASYNC_TID end
    GetGameThreadId     = function() return GAME_TID end
    IsInMainModThread   = function() return true end
    IsInAsyncThread     = function() return false end
    IsInGameThread      = function() return false end
end

-- --------------------------- Delayed actions --------------------------------
do
    local next_handle = 1
    MakeActionHandle = function() next_handle = next_handle + 1 return next_handle end
    local function check_int(h)
        if type(h) ~= "number" then
            error("No overload found for function 'IsValidDelayedActionHandle'.")
        end
    end
    IsValidDelayedActionHandle      = function(h) check_int(h) return false end
    IsDelayedActionActive           = function(h) check_int(h) return false end
    IsDelayedActionPaused           = function(h) check_int(h) return false end
    GetDelayedActionTimeRemaining   = function(h) check_int(h) return -1 end
    GetDelayedActionTimeElapsed     = function(h) check_int(h) return -1 end
    GetDelayedActionRate            = function(h) check_int(h) return 0 end
end

-- ------------------------------ Keybinds ------------------------------------
IsKeyBindRegistered = function(key)
    if type(key) ~= "number" then
        error("No overload found for function 'IsKeyBindRegistered'.")
    end
    if key < 0 or key > 255 then
        error("Parameter #1 for function 'IsKeyBindRegistered' must be an integer between 0 and 255")
    end
    return false
end

-- ----------------------------- Constants ------------------------------------
do
    Key = {
        LEFT_MOUSE_BUTTON = 1, MIDDLE_MOUSE_BUTTON = 4, BACKSPACE = 8,
        TAB = 9, RETURN = 13, ESCAPE = 27, SPACE = 32,
    }
    Key.END = 35; Key.HOME = 36; Key.A = 65; Key.Z = 90; Key.NUM_ZERO = 96
    Key.F1 = 112; Key.F6 = 117; Key.F12 = 123; Key.LEFT_ARROW = 37
    Key.LEFT_SHIFT = 160; Key.LEFT_CONTROL = 162; Key.LEFT_ALT = 164
    Key.RIGHT_SHIFT = 161
    -- padding so the mock key count looks realistic (real table is much larger)
    for i = 1, 151 do Key["MOCK_PADDING_" .. i] = i end

    ModifierKey = { SHIFT = 0x10, CONTROL = 0x11, ALT = 0x12 }

    EObjectFlags = { RF_NoFlags = 0 }
    local rf_bits = {
        "RF_Public", "RF_Standalone", "RF_MarkAsNative", "RF_Transactional",
        "RF_ClassDefaultObject", "RF_ArchetypeObject", "RF_Transient",
        "RF_MarkAsRootSet", "RF_TagGarbageTemp", "RF_NeedInitialization",
        "RF_NeedLoad", "RF_KeepForCooker", "RF_NeedPostLoad",
        "RF_NeedPostLoadSubobjects", "RF_NewerVersionExists", "RF_BeginDestroyed",
        "RF_FinishDestroyed", "RF_BeingRegenerated", "RF_DefaultSubObject",
        "RF_WasLoaded", "RF_TextExportTransient", "RF_LoadCompleted",
        "RF_InheritableComponentTemplate", "RF_DuplicateTransient",
        "RF_StrongRefOnFrame", "RF_NonPIEDuplicateTransient", "RF_Dynamic",
        "RF_WillBeLoaded", "RF_HasExternalPackage",
    }
    for i, name in ipairs(rf_bits) do EObjectFlags[name] = 2 ^ i end
    EObjectFlags.RF_AllFlags = 4294967295

    EInternalObjectFlags = {
        ReachableInCluster = 8388608, ClusterRoot = 16777216, Native = 33554432,
        Async = 67108864, AsyncLoading = 134217728, Unreachable = 268435456,
        PendingKill = 536870912, RootSet = 1073741824,
        GarbageCollectionKeepFlags = 2147483648, AllFlags = 4294967295,
    }

    local PROP_SIZES = {
        ArrayProperty = 16, BoolProperty = 1, ByteProperty = 1,
        ClassProperty = 8, EnumProperty = 1, FloatProperty = 4,
        Int16Property = 2, Int64Property = 8, Int8Property = 1,
        IntProperty = 4, MapProperty = 80, NameProperty = 8,
        ObjectProperty = 8, ObjectPtrProperty = 8, SetProperty = 80,
        StrProperty = 16, StructProperty = 48, TextProperty = 32,
        UInt16Property = 2, UInt32Property = 4, UInt64Property = 8,
        Utf8StrProperty = 16, WeakObjectProperty = 8,
    }
    PropertyTypes = {}
    for name, size in pairs(PROP_SIZES) do
        PropertyTypes[name] = {
            Name = name,
            Size = (name == "BoolProperty" and VARIANT == "b") and 4 or size,
            FFieldClassPointer = 4702111234474983,
            StaticPointer = 0,
        }
    end

    EGameThreadMethod = { EngineTick = 0, ProcessEvent = 1 }
    EngineTickAvailable = true
    ProcessEventAvailable = true
    __OriginalReturnValue = nil
    ue4ss_loaded_modules = {}
end

-- --------------------------- Invalid object ---------------------------------
do
    local invalid_mt = {
        __index = {
            IsValid = function() return false end,
            GetAddress = function() return 0 end,
            type = function() return "RemoteUnrealBase" end,
        },
    }
    CreateInvalidObject = function() return setmetatable({}, invalid_mt) end
end

-- ------------------------------ Run suite -----------------------------------
-- Usage: lua hosttest_mock_env.lua [path/to/conformance_static.lua]
local target = arg and arg[2] or "conformance_static.lua"
local chunk, err = loadfile(target)
if not chunk then
    print("hosttest_mock_env: failed to load conformance_static.lua: " .. tostring(err))
    os.exit(2)
end
local result = chunk()
if type(result) == "table" and type(result.counts) == "table" then
    print(string.format("# HOSTTEST: counts total=%d ok=%d fail=%d",
            result.counts.total, result.counts.ok, result.counts.fail))
    os.exit(0)
end
os.exit(1)
