--=====================================================================
-- conformance_live.lua
-- UE4SS LIVE-GAME API CONFORMANCE SCRIPT (read-only / safe)
--
-- PROVENANCE: Authored and staged on the Mac host only.
--   * No FTP, no payloads, no game launches, no console contact.
--   * Nothing outside this module's staging directory was touched.
--   * This file has NOT been executed against any game from the Mac.
--
-- PURPOSE: Tests UE4SS Lua APIs that REQUIRE a running game
-- (object finding, reflection, names, strings, containers, input,
-- threading, hooks, scheduling/delayed actions).
--
-- WHEN IT RUNS: only after the game is loaded AND the player is
-- in-game (a Pawn and a PlayerController exist). Gating is done with
-- a startup delay + polling (the global ExecuteWithDelay, falling
-- back to ExecuteInGameThread tick counting at ~60fps if it is
-- missing). NOTE: the port registers scheduling as PLAIN GLOBALS
-- (ExecuteWithDelay, ExecuteInGameThreadWithDelay, LoopInGameThread*)
-- -- there is no 'Loop' table (verified: full-text search of
-- source/port/ue4ss/src/Mod/LuaMod.cpp register_function calls).
--
-- SAFETY CONTRACT (non-negotiable):
--   * READ-ONLY: no spawning, no destroying, no property writes,
--     no ProcessEvent calls, no asset loads, no game-state changes.
--   * Every registration uses a no-op callback that is never
--     deliberately triggered (keybind combo, console command,
--     custom event, load-map hook). RegisterHook is unregistered
--     immediately after registration is recorded.
--   * Every call is wrapped in pcall; a FAIL records an error,
--     it can never crash the game.
--   * Tests needing game state (Pawn / PlayerController) check
--     prerequisites and emit SKIP (not FAIL) when unmet.
--
-- LOG FORMAT (fixed, tab-separated, one row per call):
--   CALL<TAB>api_name<TAB>params<TAB>result_type<TAB>result<TAB>status
-- Metadata rows: PREQ<TAB>name<TAB>expected<TAB>state<TAB>actual<TAB>MET|NOT_MET
-- Comment lines start with '#'. Only dynamic fields are sanitized
-- (control chars -> space) so columns never break.
--
-- LOG OUTPUT: every row is print()'ed to the UE4SS console/log, and
-- (when io is available) written to conformance_live.log, resolved as:
--   1. $CONFORMANCE_LIVE_LOG_DIR/conformance_live.log
--   2. ./conformance_live.log (game process working directory)
--
-- INSTALL (on target, performed manually by the user, NOT from the Mac):
--   copy to <Game>/Mods/conformance-live/Scripts/main.lua
--   add line "conformance-live : 1" to <Game>/Mods/mods.txt
--=====================================================================

local CONFIG = {
    STARTUP_DELAY_MS   = 10000,  -- grace after mod load before polling
    POST_JOIN_GRACE_MS = 5000,   -- extra settle time once in-game state is seen
    POLL_INTERVAL_MS   = 2000,   -- in-game poll interval
    WAIT_TIMEOUT_S     = 240,    -- max time to wait for in-game state
    DEFERRED_CHECK_MS  = 1500,   -- delay before deferred-execution verification
    KEY_COMBO          = { "LEFT_CONTROL", "LEFT_SHIFT", "F8" },
    CONSOLE_COMMAND    = "conformance.noop",
    CONSOLE_COMMAND_GLOBAL = "conformance.noop.global",
    CUSTOM_EVENT       = "ConformanceNoopEvent",
    HOOK_PATH          = "/Script/Engine.GameplayStatics:GetAllActorsOfClass",
    NEVER_SPAWNED_CLASS = "/Script/Engine.ConformanceNeverSpawned",
    -- Self-owned delayed-action cycle delays: long enough that the
    -- action never fires before CancelDelayedAction removes it, and
    -- the callback is a no-op regardless.
    CYCLE_DELAY_MS     = 30000,
    CYCLE_FRAMES       = 100000,
    LOG_FILE_NAME      = "conformance_live.log",
}

local state = {
    ran   = false,
    file  = nil,
    loop_api_ok = nil,  -- set once first queue_delayed runs
    counts = { PASS = 0, FAIL = 0, SKIP = 0 },
    flags  = { game_thread_noop = false, loop_noop = false },
    prereq = { engine = nil, pawn = nil, player_controller = nil,
               has_execute_in_game_thread = false, in_game = false },
}

--=====================================================================
-- Logging helpers
--=====================================================================

local function sanitize(s)
    local t = tostring(s)
    t = t:gsub("%c", " ")  -- control chars (tab/newline/etc) -> space
    return t
end

local function truncate(s, maxlen)
    local t = tostring(s)
    if #t > maxlen then
        return t:sub(1, maxlen - 3) .. "..."
    end
    return t
end

local function emit(line)
    print("[ConformanceLive] " .. line)
    if state.file then
        pcall(function() state.file:write(line .. "\n") end)
    end
end

local function bump(status)
    if state.counts[status] then
        state.counts[status] = state.counts[status] + 1
    end
end

--=====================================================================
-- Core test recorders (all UE4SS access goes through pcall here)
--=====================================================================

-- Runs fn under pcall and records one fixed-format CALL row.
-- validator (optional): function(first_return, ...) -> true to accept.
--   * no error + validator accepts (or no validator) -> PASS
--   * no error + validator rejects                 -> FAIL
--   * error raised (including "API not present")   -> FAIL
-- Returns the first return value of fn (nil on error).
local function call(api_name, params, fn, validator)
    local results = table.pack(pcall(fn))
    local ok = results[1]
    if not ok then
        local err = sanitize(tostring(results[2] or "unknown error"))
        emit("CALL\t" .. api_name .. "\t" .. truncate(sanitize(params), 120)
            .. "\terror\t" .. truncate(err, 200) .. "\tFAIL")
        bump("FAIL")
        return nil
    end

    local n = results.n - 1
    local r1 = results[2]
    local rtype, rtext
    if n == 0 then
        rtype, rtext = "none", "no-return-values"
    else
        rtype = type(r1)
        if r1 == nil then
            rtext = "nil"
        elseif rtype == "table" then
            rtext = "table(count=" .. tostring(#r1) .. ")"
        else
            rtext = sanitize(tostring(r1))
        end
        if n > 1 then
            rtext = rtext .. " (+ret_count=" .. n .. ")"
        end
    end

    local status = "PASS"
    if validator then
        local vok, vres = pcall(validator, table.unpack(results, 2, results.n))
        if not vok then
            status = "FAIL"
            rtext = rtext .. " [validator error: " .. truncate(sanitize(tostring(vres)), 120) .. "]"
        elseif vres ~= true then
            status = "FAIL"
            rtext = rtext .. " [validator rejected result]"
        end
    end

    emit("CALL\t" .. api_name .. "\t" .. truncate(sanitize(params), 120)
        .. "\t" .. rtype .. "\t" .. truncate(rtext, 200) .. "\t" .. status)
    bump(status)
    return r1
end

-- Records a SKIP row (prerequisite or dependency not met; never a failure).
local function skip(api_name, params, reason)
    emit("CALL\t" .. api_name .. "\t" .. truncate(sanitize(params), 120)
        .. "\tskip\t" .. truncate(sanitize(reason), 200) .. "\tSKIP")
    bump("SKIP")
end

--=====================================================================
-- Safe environment helpers
--=====================================================================

local function resolve_log_path()
    local ok, dir = pcall(function() return os.getenv("CONFORMANCE_LIVE_LOG_DIR") end)
    if ok and type(dir) == "string" and #dir > 0 then
        return dir .. "/" .. CONFIG.LOG_FILE_NAME
    end
    return CONFIG.LOG_FILE_NAME
end

local function safe_timestamp()
    local ok, ts = pcall(function() return os.date("%Y-%m-%d %H:%M:%S") end)
    if ok and type(ts) == "string" then return ts end
    return "unavailable(os.date)"
end

local function run_in_game_thread(fn)
    local ok = pcall(function() ExecuteInGameThread(fn) end)
    if not ok then
        pcall(fn)  -- last-resort fallback: run on the Lua thread
    end
end

local function wait_game_ticks(n, fn)
    local remaining = n
    local function step()
        if remaining <= 0 then
            pcall(fn)
            return
        end
        remaining = remaining - 1
        run_in_game_thread(step)
    end
    run_in_game_thread(step)
end

-- Delay mechanism per task spec: the global ExecuteWithDelay timer,
-- or ExecuteInGameThread tick-counting fallback (~60fps estimate).
-- Returns "loop" | "ticks" | "none" ("loop" = ExecuteWithDelay worked).
local function queue_delayed(delay_ms, fn)
    local used_delay = pcall(function() ExecuteWithDelay(delay_ms, fn) end)
    if state.loop_api_ok == nil then state.loop_api_ok = used_delay end
    if used_delay then return "loop" end
    local has_gamethread = pcall(function()
        assert(type(ExecuteInGameThread) == "function")
    end)
    if has_gamethread then
        local ticks = math.floor(delay_ms / 17) + 1  -- ~60 fps
        if ticks > 6000 then ticks = 6000 end
        wait_game_ticks(ticks, fn)
        return "ticks"
    end
    -- No deferral mechanism at all: run now with an explanatory note.
    emit("# conformance_live: no delay mechanism (ExecuteWithDelay/ExecuteInGameThread missing); running immediately.")
    pcall(fn)
    return "none"
end

local function try_find_first_of(class_name)
    local ok, res = pcall(function() return FindFirstOf(class_name) end)
    if ok and res ~= nil then return res end
    return nil
end

--=====================================================================
-- Prerequisites section (logged at top of every report)
--=====================================================================

local function evaluate_prerequisites()
    pcall(function()
        state.prereq.has_execute_in_game_thread = (type(ExecuteInGameThread) == "function")
    end)
    state.prereq.engine            = try_find_first_of("Engine")
    state.prereq.pawn              = try_find_first_of("Pawn")
    state.prereq.player_controller = try_find_first_of("PlayerController")
    state.prereq.in_game           = (state.prereq.pawn ~= nil
                                      and state.prereq.player_controller ~= nil)
end

local function preq_row(name, expected, actual, met)
    emit("PREQ\t" .. name .. "\t" .. sanitize(expected)
        .. "\tstate\t" .. sanitize(actual) .. "\t" .. (met and "MET" or "NOT_MET"))
end

local function log_prerequisites(degraded)
    emit("# === PREREQUISITES ===")
    preq_row("lua_env", "pcall+print available", "available", true)
    preq_row("game_thread_access", "ExecuteInGameThread callable",
        tostring(state.prereq.has_execute_in_game_thread),
        state.prereq.has_execute_in_game_thread)
    preq_row("engine_instance", "FindFirstOf('Engine') ~= nil",
        state.prereq.engine and "found" or "not found",
        state.prereq.engine ~= nil)
    preq_row("pawn_instance", "FindFirstOf('Pawn') ~= nil (player in-game)",
        state.prereq.pawn and "found" or "not found",
        state.prereq.pawn ~= nil)
    preq_row("player_controller_instance", "FindFirstOf('PlayerController') ~= nil",
        state.prereq.player_controller and "found" or "not found",
        state.prereq.player_controller ~= nil)
    preq_row("in_game", "pawn AND player_controller present",
        state.prereq.in_game and "yes" or "no",
        state.prereq.in_game)
    if degraded then
        emit("# NOTE: in-game wait timed out; DEGRADED mode: battery still runs")
        emit("# (it is read-only/safe), all game-state-dependent tests will SKIP.")
    end
end

--=====================================================================
-- CATEGORY: Object System
--=====================================================================

local function category_object_system()
    emit("# === CATEGORY: Object System ===")
    local engine = state.prereq.engine

    if engine then
        call("FindFirstOf", "class=Engine",
            function() return FindFirstOf("Engine") end,
            function(v) return v ~= nil end)
        call("FindAllOf", "class=Engine",
            function() return FindAllOf("Engine") end,
            function(v) return type(v) == "table" and #v >= 1 end)
        call("UObject.GetFullName", "receiver=Engine instance",
            function() return engine:GetFullName() end,
            function(v) return type(v) == "string" and #v > 0 end)
        call("UObject.GetAddress", "receiver=Engine instance",
            function() return engine:GetAddress() end,
            function(v) return type(v) == "number" and v > 0 end)
        call("UObject.IsValid", "receiver=Engine instance",
            function() return engine:IsValid() end,
            function(v) return v == true end)
        local cls = call("UObject.GetClass", "receiver=Engine instance",
            function() return engine:GetClass() end,
            function(v) return v ~= nil end)
        if cls then
            call("UObject.IsA(userdata)", "receiver=Engine instance class=<GetClass()>",
                function() return engine:IsA(cls) end,
                function(v) return v == true end)
        else
            skip("UObject.IsA(userdata)", "receiver=Engine instance class=<GetClass()>",
                "dependency unavailable: GetClass returned nil")
        end
        call("UObject.GetOuter", "receiver=Engine instance",
            function() return engine:GetOuter() end)
    else
        local reason = "prerequisite not met: FindFirstOf('Engine') returned nil"
        local rows = {
            { "FindFirstOf",              "class=Engine" },
            { "FindAllOf",                "class=Engine" },
            { "UObject.GetFullName",      "receiver=Engine instance" },
            { "UObject.GetAddress",       "receiver=Engine instance" },
            { "UObject.IsValid",          "receiver=Engine instance" },
            { "UObject.GetClass",         "receiver=Engine instance" },
            { "UObject.IsA(userdata)",    "receiver=Engine instance class=<GetClass()>" },
            { "UObject.GetOuter",         "receiver=Engine instance" },
        }
        for _, spec in ipairs(rows) do skip(spec[1], spec[2], reason) end
    end

    -- Static lookups (engine loaded is implied by this script running;
    -- no live instance dependency).
    call("StaticFindObject", "path=/Script/Engine.Actor (UClass)",
        function() return StaticFindObject("/Script/Engine.Actor") end,
        function(v) return v ~= nil end)
    call("StaticFindObject", "path=/Script/Engine.Actor:GetVelocity (UFunction)",
        function() return StaticFindObject("/Script/Engine.Actor:GetVelocity") end,
        function(v) return v ~= nil end)
    call("StaticFindObject", "path=/Script/Conformance.DoesNotExist (negative probe)",
        function() return StaticFindObject("/Script/Conformance.DoesNotExist") end,
        function(v) return v == nil end)

    -- Pawn-gated rows.
    local pawn = state.prereq.pawn
    if pawn then
        call("FindFirstOf", "class=Pawn",
            function() return FindFirstOf("Pawn") end,
            function(v) return v ~= nil end)
        call("FindAllOf", "class=Pawn",
            function() return FindAllOf("Pawn") end,
            function(v) return type(v) == "table" and #v >= 1 end)
        call("UObject.IsA(string)", "receiver=Pawn instance class=Pawn",
            function() return pawn:IsA("Pawn") end,
            function(v) return v == true end)
        call("UObject.IsA(string)", "receiver=Pawn instance class=Actor",
            function() return pawn:IsA("Actor") end,
            function(v) return v == true end)
    else
        local reason = "prerequisite not met: FindFirstOf('Pawn') returned nil (main menu or not in-game)"
        local rows = {
            { "FindFirstOf",         "class=Pawn" },
            { "FindAllOf",           "class=Pawn" },
            { "UObject.IsA(string)", "receiver=Pawn instance class=Pawn" },
            { "UObject.IsA(string)", "receiver=Pawn instance class=Actor" },
        }
        for _, spec in ipairs(rows) do skip(spec[1], spec[2], reason) end
    end

    -- PlayerController-gated rows.
    if state.prereq.player_controller then
        call("FindFirstOf", "class=PlayerController",
            function() return FindFirstOf("PlayerController") end,
            function(v) return v ~= nil end)
    else
        skip("FindFirstOf", "class=PlayerController",
            "prerequisite not met: FindFirstOf('PlayerController') returned nil (main menu or not in-game)")
    end
end

--=====================================================================
-- CATEGORY: Reflection (property reads + UFunction lookup)
--=====================================================================

local function category_reflection()
    emit("# === CATEGORY: Reflection ===")
    local engine = state.prereq.engine

    if engine then
        -- Read-only property access on a known UObject property.
        call("UObject.__index(property-read)", "receiver=Engine instance property=bUseFixedFrameRate",
            function() return engine.bUseFixedFrameRate end)
        call("UObject.GetPropertyValue", "receiver=Engine instance property=bUseFixedFrameRate",
            function() return engine:GetPropertyValue("bUseFixedFrameRate") end)
    else
        local reason = "prerequisite not met: FindFirstOf('Engine') returned nil"
        skip("UObject.__index(property-read)", "receiver=Engine instance property=bUseFixedFrameRate", reason)
        skip("UObject.GetPropertyValue", "receiver=Engine instance property=bUseFixedFrameRate", reason)
    end

    local pawn = state.prereq.pawn
    if pawn then
        call("UObject.__index(property-read)", "receiver=Pawn instance property=Owner",
            function() return pawn.Owner end)
    else
        skip("UObject.__index(property-read)", "receiver=Pawn instance property=Owner",
            "prerequisite not met: FindFirstOf('Pawn') returned nil (main menu or not in-game)")
    end

    local pc = state.prereq.player_controller
    if pc then
        call("UObject.__index(property-read)", "receiver=PlayerController instance property=AcknowledgedPawn",
            function() return pc.AcknowledgedPawn end)
    else
        skip("UObject.__index(property-read)", "receiver=PlayerController instance property=AcknowledgedPawn",
            "prerequisite not met: FindFirstOf('PlayerController') returned nil")
    end

    -- UFunction lookup on the AActor UClass. NOTE: the port's UClass
    -- metatable has NO GetFunction method (verified:
    -- src/LuaType/LuaUClass.cpp adds only GetCDO/IsChildOf/type;
    -- src/LuaType/LuaUStruct.cpp adds GetSuperStruct/ForEachFunction/
    -- ForEachProperty). UFunction lookup itself is exercised via
    -- StaticFindObject(':GetVelocity') in the Object System category.
    local ok_cls, actor_class = pcall(function() return StaticFindObject("/Script/Engine.Actor") end)
    if ok_cls and actor_class ~= nil then
        -- ForEachFunction: callback receives each UFunction; returning
        -- true STOPS iteration (immediate stop = bounded, read-only).
        call("UStruct.ForEachFunction", "receiver=UClass(Actor) callback=immediate-stop(no-op returns true)",
            function()
                return actor_class:ForEachFunction(function() return true end)
            end)
        call("UStruct.GetSuperStruct", "receiver=UClass(Actor)",
            function() return actor_class:GetSuperStruct() end,
            function(v) return v ~= nil end)
        call("UClass.IsChildOf", "receiver=UClass(Actor) other=UClass(Actor) (self is a child of itself)",
            function() return actor_class:IsChildOf(actor_class) end,
            function(v) return v == true end)
        call("UClass.GetCDO", "receiver=UClass(Actor)",
            function() return actor_class:GetCDO() end,
            function(v) return v ~= nil end)
    else
        local reason = "dependency unavailable: StaticFindObject('/Script/Engine.Actor') failed"
        skip("UStruct.ForEachFunction", "receiver=UClass(Actor) callback=immediate-stop(no-op returns true)", reason)
        skip("UStruct.GetSuperStruct", "receiver=UClass(Actor)", reason)
        skip("UClass.IsChildOf", "receiver=UClass(Actor) other=UClass(Actor)", reason)
        skip("UClass.GetCDO", "receiver=UClass(Actor)", reason)
    end
end

--=====================================================================
-- CATEGORY: Names (FName)
--=====================================================================

local function category_names()
    emit("# === CATEGORY: Names (FName) ===")
    local fname = call("FName.new", "value=ConformanceLive",
        function() return FName("ConformanceLive") end,
        function(v) return v ~= nil end)
    -- Legacy-API absence probe: full-text search of the port source
    -- finds no toFName registration anywhere (it is not part of this
    -- port's API). The row intentionally records its absence/behavior
    -- as conformance data; FAIL here is expected and informative.
    call("string.toFName(legacy-probe)", "value=ConformanceLive (absence probe; not in port source)",
        function() return ("ConformanceLive"):toFName() end,
        function(v) return v ~= nil end)

    if not fname then
        local reason = "dependency unavailable: FName construction errored or returned nil"
        skip("FName.ToString", "self=FName('ConformanceLive')", reason)
        skip("FName.Equals", "self=FName('ConformanceLive') other=FName('ConformanceLive')", reason)
        skip("FName.__eq(==)", "two FName instances with same value", reason)
        return
    end

    call("FName.ToString", "self=FName('ConformanceLive')",
        function() return fname:ToString() end,
        function(v) return type(v) == "string" and #v > 0 end)

    local other_ok, fname_other = pcall(function() return FName("ConformanceLive") end)
    local cmp = (other_ok and fname_other ~= nil) and fname_other or fname
    call("FName.Equals", "self=FName('ConformanceLive') other=FName('ConformanceLive')",
        function() return fname:Equals(cmp) end,
        function(v) return v == true end)
    call("FName.__eq(==)", "two FName instances with same value",
        function() return fname == cmp end,
        function(v) return v == true end)
end

--=====================================================================
-- CATEGORY: Strings (FString / FText)
--=====================================================================

local function category_strings()
    emit("# === CATEGORY: Strings (FString / FText) ===")

    -- NOTE: the port's string types (FString/FUtf8String/FAnsiString)
    -- register NO metamethods (include/LuaType/LuaUnrealString.hpp:
    -- setup_metamethods body is empty), so tostring() and '..' do not
    -- convert/concatenate them. The real API is :ToString()/:Append().
    local fstr = call("FString.new", "value=conformance_fstring",
        function() return FString("conformance_fstring") end,
        function(v) return v ~= nil end)
    if fstr then
        call("FString.ToString", "self=FString('conformance_fstring')",
            function() return fstr:ToString() end,
            function(v) return v == "conformance_fstring" end)
        call("FString.Len", "self=FString('conformance_fstring')",
            function() return fstr:Len() end,
            function(v) return v == #"conformance_fstring" end)
        -- Append mutates only this Lua-owned in-memory copy (never game state).
        call("FString.Append", "self=FString('conformance_fstring') suffix='_suffix' (in-memory copy only)",
            function() return fstr:Append("_suffix") end)
        call("FString.ToString", "self=FString after Append('_suffix')",
            function() return fstr:ToString() end,
            function(v) return v == "conformance_fstring_suffix" end)
    else
        local reason = "dependency unavailable: FString construction errored or returned nil"
        skip("FString.ToString", "self=FString('conformance_fstring')", reason)
        skip("FString.Len", "self=FString('conformance_fstring')", reason)
        skip("FString.Append", "self=FString('conformance_fstring') suffix='_suffix' (in-memory copy only)", reason)
        skip("FString.ToString", "self=FString after Append('_suffix')", reason)
    end

    -- Lua-native concat baseline (UE4SS bridges Lua string <-> FString).
    call("FString.concat(lua)", "a='conformance_' b='fstring'",
        function() return "conformance_" .. "fstring" end,
        function(v) return v == "conformance_fstring" end)

    local ftext = call("FText.new", "value=ConformanceLiveText (created, never displayed)",
        function() return FText("ConformanceLiveText") end,
        function(v) return v ~= nil end)
    if ftext then
        call("FText.ToString", "self=FText('ConformanceLiveText')",
            function() return ftext:ToString() end,
            function(v) return type(v) == "string" and #v > 0 end)
    else
        skip("FText.ToString", "self=FText('ConformanceLiveText')",
            "dependency unavailable: FText construction errored or returned nil")
    end
end

--=====================================================================
-- CATEGORY: Containers (TArray, read-only view of a live array)
--=====================================================================
-- The port has NO Lua-callable TArray constructor: TArray userdata
-- only exists when an array-typed UObject property is read
-- (src/LuaType/LuaUObject.cpp Operation::Get -> TArray::construct).
-- So the battery discovers a real array property dynamically via
-- ForEachProperty on UClass(Actor) and reads it from the Pawn.
-- Real catalog members (src/LuaType/LuaTArray.cpp): GetArrayNum,
-- GetArrayMax, GetArrayAddress, GetArrayDataAddress, Empty, ForEach,
-- IsValid (+inherited GetAddress/type) and metamethods __index /
-- __newindex (1-based) / __len. There is NO Length()/Add()/Get().
-- SAFETY: __index grows the array with AddZeroed on out-of-bounds
-- reads and __newindex writes/autogrows -- both MUTATE live game
-- memory -- so element reads are length-guarded and element writes
-- are excluded by the read-only contract.

local function category_containers()
    emit("# === CATEGORY: Containers (TArray) ===")

    local pawn = state.prereq.pawn
    local ok_cls, actor_class = pcall(function() return StaticFindObject("/Script/Engine.Actor") end)
    if not (ok_cls and actor_class ~= nil) then
        local reason = "dependency unavailable: StaticFindObject('/Script/Engine.Actor') failed"
        local rows = {
            { "UStruct.ForEachProperty",  "receiver=UClass(Actor) callback=find-first-array-property" },
            { "XProperty.GetFName",       "receiver=first ArrayProperty on UClass(Actor)" },
            { "UObject.__index(array-property-read)", "receiver=Pawn instance property=<discovered array property>" },
            { "TArray.GetArrayNum",       "self=<live array view>" },
            { "TArray.__len(#)",          "self=<live array view>" },
            { "TArray.GetArrayMax",       "self=<live array view>" },
            { "TArray.IsValid",           "self=<live array view>" },
            { "TArray.GetArrayAddress",   "self=<live array view>" },
            { "TArray.__index(element-read)", "self=<live array view> index=1 (only when #arr>=1)" },
            { "TArray.ForEach",           "self=<live array view> callback=immediate-stop" },
        }
        for _, spec in ipairs(rows) do skip(spec[1], spec[2], reason) end
        skip("TArray.__newindex(element-write)", "self=<live array view>",
            "excluded by the read-only contract: no in-memory TArray constructor exists in this port, so a write mutates live game memory")
        return
    end

    -- Step 1: find the name of the first reflected array property on Actor.
    local array_prop_name = nil
    local ok_fp = pcall(function()
        actor_class:ForEachProperty(function(prop)
            local ok_t, ptype = pcall(function() return prop:type() end)
            if ok_t and ptype == "ArrayProperty" then
                local ok_n, name = pcall(function()
                    local f = prop:GetFName()
                    return f and f:ToString()
                end)
                if ok_n and type(name) == "string" and #name > 0 then
                    array_prop_name = name
                    return true -- stop iteration
                end
            end
            return nil -- continue
        end)
    end)
    call("UStruct.ForEachProperty", "receiver=UClass(Actor) callback=find-first-array-property",
        function() return ok_fp and array_prop_name ~= nil end,
        function(v) return v == true end)
    if array_prop_name == nil then
        local reason = "dependency unavailable: no reflected ArrayProperty found on UClass(Actor) via ForEachProperty"
        skip("XProperty.GetFName", "receiver=first ArrayProperty on UClass(Actor)", reason)
        skip("UObject.__index(array-property-read)", "receiver=Pawn instance property=<discovered array property>", reason)
        skip("TArray.GetArrayNum", "self=<live array view>", reason)
        skip("TArray.__len(#)", "self=<live array view>", reason)
        skip("TArray.GetArrayMax", "self=<live array view>", reason)
        skip("TArray.IsValid", "self=<live array view>", reason)
        skip("TArray.GetArrayAddress", "self=<live array view>", reason)
        skip("TArray.__index(element-read)", "self=<live array view> index=1 (only when #arr>=1)", reason)
        skip("TArray.ForEach", "self=<live array view> callback=immediate-stop", reason)
        skip("TArray.__newindex(element-write)", "self=<live array view>",
            "excluded by the read-only contract: no in-memory TArray constructor exists in this port, so a write mutates live game memory")
        return
    end
    call("XProperty.GetFName", "receiver=first ArrayProperty on UClass(Actor) -> name=" .. array_prop_name,
        function() return array_prop_name end,
        function(v) return type(v) == "string" and #v > 0 end)

    -- Step 2: read the array property from a live Pawn instance.
    if not pawn then
        local reason = "prerequisite not met: FindFirstOf('Pawn') returned nil (main menu or not in-game)"
        skip("UObject.__index(array-property-read)", "receiver=Pawn instance property=" .. array_prop_name, reason)
        skip("TArray.GetArrayNum", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.__len(#)", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.GetArrayMax", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.IsValid", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.GetArrayAddress", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.__index(element-read)", "self=<live array view of Pawn." .. array_prop_name .. "> index=1 (only when #arr>=1)", reason)
        skip("TArray.ForEach", "self=<live array view of Pawn." .. array_prop_name .. "> callback=immediate-stop", reason)
        skip("TArray.__newindex(element-write)", "self=<live array view>",
            "excluded by the read-only contract: no in-memory TArray constructor exists in this port, so a write mutates live game memory")
        return
    end

    local arr = call("UObject.__index(array-property-read)",
        "receiver=Pawn instance property=" .. array_prop_name,
        function() return pawn[array_prop_name] end,
        function(v) return v ~= nil end)
    if not arr then
        local reason = "dependency unavailable: reading Pawn." .. array_prop_name .. " errored or returned nil"
        skip("TArray.GetArrayNum", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.__len(#)", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.GetArrayMax", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.IsValid", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.GetArrayAddress", "self=<live array view of Pawn." .. array_prop_name .. ">", reason)
        skip("TArray.__index(element-read)", "self=<live array view of Pawn." .. array_prop_name .. "> index=1 (only when #arr>=1)", reason)
        skip("TArray.ForEach", "self=<live array view of Pawn." .. array_prop_name .. "> callback=immediate-stop", reason)
        skip("TArray.__newindex(element-write)", "self=<live array view>",
            "excluded by the read-only contract: no in-memory TArray constructor exists in this port, so a write mutates live game memory")
        return
    end

    -- Step 3: exercise the REAL catalog members against the live view.
    local num = call("TArray.GetArrayNum", "self=<live array view of Pawn." .. array_prop_name .. ">",
        function() return arr:GetArrayNum() end,
        function(v) return type(v) == "number" and v >= 0 end)
    call("TArray.__len(#)", "self=<live array view> (must equal GetArrayNum)",
        function() return #arr end,
        function(v) return type(v) == "number" and v == num end)
    call("TArray.GetArrayMax", "self=<live array view> (Max >= Num expected)",
        function() return arr:GetArrayMax() end,
        function(v) return type(v) == "number" and (type(num) ~= "number" or v >= num) end)
    call("TArray.IsValid", "self=<live array view>",
        function() return arr:IsValid() end,
        function(v) return v == true end)
    call("TArray.GetArrayAddress", "self=<live array view>",
        function() return arr:GetArrayAddress() end,
        function(v) return type(v) == "number" and v > 0 end)

    -- Element read ONLY when in bounds: an out-of-bounds __index read
    -- calls AddZeroed (grows the live array) -- a mutation we must not do.
    if type(num) == "number" and num >= 1 then
        call("TArray.__index(element-read)", "self=<live array view> index=1 (in-bounds: #arr=" .. tostring(num) .. ")",
            function() return arr[1] end)
        -- ForEach with immediate-stop callback: bounded to one element,
        -- dodging the known large-array stack-corruption TODO in source.
        call("TArray.ForEach", "self=<live array view> callback=immediate-stop(returns true)",
            function() return arr:ForEach(function() return true end) end)
    else
        skip("TArray.__index(element-read)", "self=<live array view> index=1",
            "array empty (#arr=" .. tostring(num) .. "); an OOB read would AddZeroed-grow live game memory")
        skip("TArray.ForEach", "self=<live array view> callback=immediate-stop",
            "array empty (#arr=" .. tostring(num) .. "); nothing to iterate")
    end

    skip("TArray.__newindex(element-write)", "self=<live array view> index=1 value=42",
        "excluded by the read-only contract: no in-memory TArray constructor exists in this port, so an element write (__newindex, auto-grows) mutates live game memory")
    skip("TArray.Empty", "self=<live array view>",
        "excluded by the read-only contract: Empty() clears live game memory")
end

--=====================================================================
-- CATEGORY: Input (register only, never triggered)
--=====================================================================

local function category_input()
    emit("# === CATEGORY: Input ===")
    call("RegisterKeyBind", "combo=LEFT_CONTROL+LEFT_SHIFT+F8 callback=no-op(never triggered)",
        function()
            return RegisterKeyBind(CONFIG.KEY_COMBO, function() end)
        end)
    call("IsKeyBindRegistered", "combo=LEFT_CONTROL+LEFT_SHIFT+F8",
        function() return IsKeyBindRegistered(CONFIG.KEY_COMBO) end,
        function(v) return type(v) == "boolean" end)
end

--=====================================================================
-- CATEGORY: Threading (queue a no-op closure)
--=====================================================================

local function category_threading()
    emit("# === CATEGORY: Threading ===")
    call("ExecuteInGameThread", "callback=no-op(sets deferred flag)",
        function()
            ExecuteInGameThread(function() state.flags.game_thread_noop = true end)
            return "queued"
        end,
        function(v) return v == "queued" end)
end

--=====================================================================
-- CATEGORY: Hooks (no-op pass-through callbacks only)
--=====================================================================
-- All Register*Hook globals in the port take a no-op-able callback and
-- return NO values (verified: each register_function body in
-- src/Mod/LuaMod.cpp ends in `return 0`), so "returned without error"
-- is the conformance criterion -- there is no return value to validate.
-- Only RegisterHook/UnregisterCustomEvent have unregister APIs; the
-- remaining no-op callbacks stay registered (same accepted pattern as
-- the pre-existing RegisterLoadMapPostHook row: they are never
-- deliberately triggered and are no-ops if the game fires them).

local function category_hooks()
    emit("# === CATEGORY: Hooks ===")
    call("RegisterLoadMapPostHook", "callback=no-op",
        function() return RegisterLoadMapPostHook(function() end) end)

    -- Lifecycle hooks (single LuaFunction, no unregister API in port).
    call("RegisterLoadMapPreHook", "callback=no-op",
        function() return RegisterLoadMapPreHook(function() end) end)
    call("RegisterInitGameStatePreHook", "callback=no-op",
        function() return RegisterInitGameStatePreHook(function() end) end)
    call("RegisterInitGameStatePostHook", "callback=no-op",
        function() return RegisterInitGameStatePostHook(function() end) end)
    call("RegisterBeginPlayPreHook", "callback=no-op",
        function() return RegisterBeginPlayPreHook(function() end) end)
    call("RegisterBeginPlayPostHook", "callback=no-op",
        function() return RegisterBeginPlayPostHook(function() end) end)
    call("RegisterEndPlayPreHook", "callback=no-op",
        function() return RegisterEndPlayPreHook(function() end) end)
    call("RegisterEndPlayPostHook", "callback=no-op",
        function() return RegisterEndPlayPostHook(function() end) end)

    -- Exec-path hooks (single LuaFunction, no unregister API in port).
    call("RegisterProcessConsoleExecPreHook", "callback=no-op",
        function() return RegisterProcessConsoleExecPreHook(function() end) end)
    call("RegisterProcessConsoleExecPostHook", "callback=no-op",
        function() return RegisterProcessConsoleExecPostHook(function() end) end)
    call("RegisterCallFunctionByNameWithArgumentsPreHook", "callback=no-op",
        function() return RegisterCallFunctionByNameWithArgumentsPreHook(function() end) end)
    call("RegisterCallFunctionByNameWithArgumentsPostHook", "callback=no-op",
        function() return RegisterCallFunctionByNameWithArgumentsPostHook(function() end) end)
    call("RegisterULocalPlayerExecPreHook", "callback=no-op",
        function() return RegisterULocalPlayerExecPreHook(function() end) end)
    call("RegisterULocalPlayerExecPostHook", "callback=no-op",
        function() return RegisterULocalPlayerExecPostHook(function() end) end)

    -- Console command handlers: per-command and global variants.
    call("RegisterConsoleCommandHandler", "command=" .. CONFIG.CONSOLE_COMMAND .. " handler=no-op",
        function()
            return RegisterConsoleCommandHandler(CONFIG.CONSOLE_COMMAND, function() end)
        end)
    call("RegisterConsoleCommandGlobalHandler", "command=" .. CONFIG.CONSOLE_COMMAND_GLOBAL .. " handler=no-op",
        function()
            return RegisterConsoleCommandGlobalHandler(CONFIG.CONSOLE_COMMAND_GLOBAL, function() end)
        end)

    -- Custom events: register then unregister our own no-op event.
    call("RegisterCustomEvent", "event=" .. CONFIG.CUSTOM_EVENT .. " handler=no-op",
        function()
            return RegisterCustomEvent(CONFIG.CUSTOM_EVENT, function() end)
        end)
    call("UnregisterCustomEvent", "event=" .. CONFIG.CUSTOM_EVENT,
        function() return UnregisterCustomEvent(CONFIG.CUSTOM_EVENT) end)

    -- New-object notification. The class name is one that never
    -- instantiates at runtime, so the no-op callback never fires.
    -- There is no unregister API for it in the port (the registration
    -- lives for the life of the mod -- same accepted trade-off as the
    -- lifecycle hooks above, but with a callback that cannot fire).
    call("NotifyOnNewObject", "class=" .. CONFIG.NEVER_SPAWNED_CLASS .. " callback=no-op(class never spawns)",
        function()
            return NotifyOnNewObject(CONFIG.NEVER_SPAWNED_CLASS, function() end)
        end)

    -- No-op pre-callback returning nothing = pass-through to the original
    -- function; game behavior unchanged even if the game calls it. The hook
    -- is unregistered immediately after registration is recorded.
    -- RegisterHook returns TWO ids (pre, post) and UnregisterHook needs
    -- (path, pre_id, post_id) -- verified against src/Mod/LuaMod.cpp.
    -- NOTE: the call() helper returns only the FIRST return value, so the
    -- second id (post) is captured through an upvalue instead.
    local pre_id, post_id
    local hook_ok = false
    pre_id = call("RegisterHook", "path=" .. CONFIG.HOOK_PATH .. " pre=no-op pass-through",
        function()
            local a, b = RegisterHook(CONFIG.HOOK_PATH, function() end)
            hook_ok = (a ~= nil)
            post_id = b
            return a
        end,
        function(v) return v ~= nil end)
    -- Non-native script hooks use post_id == pre_id in the port; if a build
    -- returns only one id, reuse it rather than passing nil to UnregisterHook.
    if post_id == nil and pre_id ~= nil then post_id = pre_id end
    if hook_ok and pre_id ~= nil then
        call("UnregisterHook", "path=" .. CONFIG.HOOK_PATH .. " pre_id/post_id=<from RegisterHook>",
            function()
                return UnregisterHook(CONFIG.HOOK_PATH, pre_id, post_id)
            end)
    else
        skip("UnregisterHook", "path=" .. CONFIG.HOOK_PATH .. " pre_id/post_id=<from RegisterHook>",
            "dependency unavailable: RegisterHook returned no id")
    end
end

--=====================================================================
-- CATEGORY: Scheduling (real globals; self-owned + cancelled cycles)
--=====================================================================
-- The port registers scheduling as PLAIN GLOBALS (no 'Loop' table):
-- ExecuteWithDelay / LoopAsync / ExecuteInGameThreadWithDelay /
-- RetriggerableExecuteInGameThreadWithDelay / LoopInGameThreadWithDelay
-- / LoopInGameThreadAfterFrames / ExecuteInGameThreadAfterFrames
-- (src/Mod/LuaMod.cpp register_function list). Test pattern: register
-- a self-owned no-op action with a delay long enough that it never
-- fires, record the returned handle, then CancelDelayedAction(handle)
-- (which only cancels the calling mod's own actions). LoopAsync is
-- the one exception -- see the skip row below for its specific reason.

local function category_scheduling()
    emit("# === CATEGORY: Scheduling ===")
    call("ExecuteWithDelay", "delay_ms=50 callback=no-op(sets deferred flag)",
        function()
            ExecuteWithDelay(50, function() state.flags.loop_noop = true end)
            return "queued"
        end,
        function(v) return v == "queued" end)

    -- One-shot game-thread delayed action + cancel (never fires: 30s
    -- delay, cancelled immediately; callback is a no-op regardless).
    local h1 = call("ExecuteInGameThreadWithDelay", "delay_ms=" .. CONFIG.CYCLE_DELAY_MS .. " callback=no-op(never fires)",
        function() return ExecuteInGameThreadWithDelay(CONFIG.CYCLE_DELAY_MS, function() end) end,
        function(v) return type(v) == "number" end)
    if h1 ~= nil then
        call("CancelDelayedAction", "handle=<from ExecuteInGameThreadWithDelay>",
            function() return CancelDelayedAction(h1) end,
            function(v) return v == true end)
    else
        skip("CancelDelayedAction", "handle=<from ExecuteInGameThreadWithDelay>",
            "dependency unavailable: ExecuteInGameThreadWithDelay returned no handle")
    end

    -- Retriggerable variant on a fresh self-owned handle.
    local h2 = call("MakeActionHandle", "no-args (fresh self-owned handle)",
        function() return MakeActionHandle() end,
        function(v) return type(v) == "number" end)
    if h2 ~= nil then
        call("RetriggerableExecuteInGameThreadWithDelay",
            "handle=<from MakeActionHandle> delay_ms=" .. CONFIG.CYCLE_DELAY_MS .. " callback=no-op(never fires)",
            function()
                return RetriggerableExecuteInGameThreadWithDelay(h2, CONFIG.CYCLE_DELAY_MS, function() end)
            end)
        call("CancelDelayedAction", "handle=<retriggerable action>",
            function() return CancelDelayedAction(h2) end,
            function(v) return v == true end)
    else
        skip("RetriggerableExecuteInGameThreadWithDelay",
            "handle=<from MakeActionHandle> delay_ms=" .. CONFIG.CYCLE_DELAY_MS,
            "dependency unavailable: MakeActionHandle returned no handle")
        skip("CancelDelayedAction", "handle=<retriggerable action>",
            "dependency unavailable: MakeActionHandle returned no handle")
    end

    -- Game-thread LOOP variants: repeating actions (is_looping) with
    -- handles; registered with delays that never elapse before the
    -- immediate CancelDelayedAction, so the no-op callback never runs
    -- and no loop survives the battery.
    local h3 = call("LoopInGameThreadWithDelay", "delay_ms=" .. CONFIG.CYCLE_DELAY_MS .. " callback=no-op(never fires)",
        function() return LoopInGameThreadWithDelay(CONFIG.CYCLE_DELAY_MS, function() end) end,
        function(v) return type(v) == "number" end)
    if h3 ~= nil then
        call("CancelDelayedAction", "handle=<from LoopInGameThreadWithDelay>",
            function() return CancelDelayedAction(h3) end,
            function(v) return v == true end)
    else
        skip("CancelDelayedAction", "handle=<from LoopInGameThreadWithDelay>",
            "dependency unavailable: LoopInGameThreadWithDelay returned no handle")
    end

    local h4 = call("LoopInGameThreadAfterFrames", "frames=" .. CONFIG.CYCLE_FRAMES .. " callback=no-op(never fires)",
        function() return LoopInGameThreadAfterFrames(CONFIG.CYCLE_FRAMES, function() end) end,
        function(v) return type(v) == "number" end)
    if h4 ~= nil then
        call("CancelDelayedAction", "handle=<from LoopInGameThreadAfterFrames>",
            function() return CancelDelayedAction(h4) end,
            function(v) return v == true end)
    else
        skip("CancelDelayedAction", "handle=<from LoopInGameThreadAfterFrames>",
            "dependency unavailable: LoopInGameThreadAfterFrames returned no handle")
    end

    -- LoopAsync: NOT called, and this is the specific reason (not
    -- "not written yet"): LoopAsync schedules a callback that repeats
    -- FOREVER on the shared UE4SS async thread (ActionType::Loop,
    -- src/Mod/LuaMod.cpp:3390); it returns no handle and there is NO
    -- cancellation API for it (CancelDelayedAction only searches the
    -- delayed-action containers, not m_pending_actions). Calling it
    -- would leave permanent background work running for the life of
    -- the mod -- excluded by this suite's no-persistent-background-work
    -- contract.
    skip("LoopAsync", "delay_ms=<any> callback=no-op",
        "unsafe by design: perpetual repeating async-thread callback with no handle and no cancellation API in this port (ActionType::Loop); would leave permanent background work for the mod's lifetime")
end

--=====================================================================
-- Deferred verification + summary
--=====================================================================

local function finish()
    emit("# === CATEGORY: Deferred Execution Verification ===")
    call("ExecuteInGameThread.executed", "flag set by queued no-op callback",
        function() return state.flags.game_thread_noop end,
        function(v) return v == true end)
    call("ExecuteWithDelay.executed", "flag set by delayed no-op callback",
        function() return state.flags.loop_noop end,
        function(v) return v == true end)

    emit("# === SUMMARY ===")
    emit("# delay_api_available=" .. tostring(state.loop_api_ok)
        .. " (false = ExecuteInGameThread tick-count timing was used)")
    emit("# totals: PASS=" .. state.counts.PASS
        .. " FAIL=" .. state.counts.FAIL
        .. " SKIP=" .. state.counts.SKIP)
    emit("# NOTE: FAIL rows on constructors (FName/FString/FText) or optional")
    emit("# APIs (IsKeyBindRegistered, GetPropertyValue, string.toFName ")
    emit("# legacy-probe, ...) record absence of the API in this UE4SS build;")
    emit("# they are conformance data, not game damage.")
    emit("# Read-only script: no game state was modified.")
    emit("# END OF REPORT")

    if state.file then
        pcall(function() state.file:close() end)
        state.file = nil
    end
end

--=====================================================================
-- Battery runner
--=====================================================================

local function run_all(degraded)
    if state.ran then
        emit("# conformance_live: battery already ran; ignoring duplicate run request.")
        return
    end
    state.ran = true

    local log_path = resolve_log_path()
    local ok, fh = pcall(function() return io.open(log_path, "w") end)
    if ok and fh then state.file = fh end

    emit("# ====================================================================")
    emit("# UE4SS LIVE-GAME API CONFORMANCE REPORT")
    emit("# script: conformance_live.lua (read-only / safe; pcall-wrapped)")
    emit("# generated: " .. safe_timestamp())
    emit("# mode: " .. (degraded and "DEGRADED (in-game wait timed out)" or "IN-GAME"))
    emit("# log_file: " .. sanitize(log_path) .. " (io_available=" .. tostring(state.file ~= nil) .. ")")
    emit("# format: CALL<TAB>api_name<TAB>params<TAB>result_type<TAB>result<TAB>status")
    emit("# ====================================================================")

    evaluate_prerequisites()
    log_prerequisites(degraded)

    category_object_system()
    category_reflection()
    category_names()
    category_strings()
    category_containers()
    category_input()
    category_threading()
    category_hooks()
    category_scheduling()

    -- Give queued/delayed no-ops time to actually run, then verify flags.
    queue_delayed(CONFIG.DEFERRED_CHECK_MS, finish)
end

--=====================================================================
-- Bootstrap: startup delay, then wait until the player is in-game
-- (Pawn + PlayerController present), then run on the game thread.
--=====================================================================

local function start_polling()
    local attempts_left = math.ceil(CONFIG.WAIT_TIMEOUT_S * 1000 / CONFIG.POLL_INTERVAL_MS)

    local function launch(degraded)
        run_in_game_thread(function() run_all(degraded) end)
    end

    local function poll()
        local pawn = try_find_first_of("Pawn")
        local pc = try_find_first_of("PlayerController")
        if pawn and pc then
            -- Post-join grace so the game is fully initialized.
            queue_delayed(CONFIG.POST_JOIN_GRACE_MS, function() launch(false) end)
        elseif attempts_left <= 0 then
            emit("# conformance_live: in-game wait timed out; starting DEGRADED run.")
            launch(true)
        else
            attempts_left = attempts_left - 1
            queue_delayed(CONFIG.POLL_INTERVAL_MS, poll)
        end
    end

    queue_delayed(CONFIG.STARTUP_DELAY_MS, poll)
end

-- Manual trigger for developers (no-op if the battery already ran).
pcall(function() _G.ConformanceLiveRun = function() run_all(false) end end)

local bootstrap_ok, bootstrap_err = pcall(start_polling)
if not bootstrap_ok then
    print("[ConformanceLive] bootstrap failed: " .. tostring(bootstrap_err))
    pcall(function() run_in_game_thread(function() run_all(true) end) end)
end
