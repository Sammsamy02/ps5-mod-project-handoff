#!/usr/bin/env python3
"""Build the UE4SS Lua global-function catalog (Mac-only, host-tested).

Source of truth: PS5/Prospero port of UE4SS at
  /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/
cross-checked against the vendored upstream copy at
  .../ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/ (version.cache 3.0.1.0.0).
"""
import json
from collections import OrderedDict

LM = "src/Mod/LuaMod.cpp"
V_LEGACY = "vendored UE4SS 3.0.1 (version.cache 3.0.1.0.0); part of the long-documented UE4SS Lua API (1.x/2.x era)"
V_30 = "vendored UE4SS 3.0.1 (version.cache 3.0.1.0.0); 3.x-era API (not verified against older local sources)"
V_PORT = "PS5 port addition in native-coherent-runtime-1 (absent from vendored UE4SS 3.0.1)"

entries = []

def add(name, signature, category, registration, version, pc_only, needs_live_game, notes=""):
    entries.append(OrderedDict([
        ("name", name),
        ("signature", signature),
        ("category", category),
        ("registration_sites", registration if isinstance(registration, list) else [registration]),
        ("ue4ss_version", version),
        ("pc_only_behavior", pc_only),
        ("ps5_runtime_status", ""),
        ("needs_live_game", needs_live_game),
        ("notes", notes),
    ]))

# ---------------- Site A: setup_lua_global_functions_internal (all mod Lua states + console executor) ----------------
A = f"{LM}:1521 (setup_lua_global_functions_internal; every true-mod Lua state via LuaMod::setup_lua_global_functions at {LM}:4072/6301, and the GUI console-executor state at {LM}:6725)"

add("print", "print(...any) -> ()", "debug", A, V_LEGACY,
    "Replaces the Lua stdlib print. Output goes through UE4SS DynamicOutput (console/GUI on PC; port log sink on PS5) and, when the global 'OutputDeviceRef' is set (LuaLibrary::set_outputdevice_ref), mirrors into the game's FOutputDevice->Log. No keyboard/mouse, no direct file I/O.",
    False, "Alias of the Lua builtin; calls LuaLibrary::global_print (src/LuaLibrary.cpp:35).")

add("LoadExport", "LoadExport(string SymbolName) -> integer Address", "other", A, V_LEGACY,
    "PC: resolves the export via Win32 GetProcAddress across all loaded modules (UnrealInitializer::LoadExport, port/unreal/src/UnrealInitializer.cpp:200). Non-Windows/Prospero: routes through the native module source FindExport instead. No keyboard/mouse; no file I/O.",
    True, "Returns the address of an exported symbol from the live process's modules; binding at 1522 -> LuaLibrary::load_export (src/LuaLibrary.cpp:112).")

add("CreateInvalidObject", "CreateInvalidObject() -> UObject", "object-management", A, V_LEGACY,
    "Pure wrapper construction around nullptr (LuaType::auto_construct_object(lua, nullptr)); no engine calls, no I/O.",
    False, "Useful for initializing variables with an invalid UObject handle.")

add("StaticFindObject", "StaticFindObject(string Name) -> UObject|nil | StaticFindObject(UClass Class, UObject InOuter, string Name, bool ExactClass=false) -> UObject|nil", "object-management", A, V_LEGACY,
    "Reflection lookup (Unreal::UObjectGlobals::StaticFindObject against live UObject hash tables). No input/paths/file I/O.",
    True, "Overloads documented in the binding's own error text (LuaMod.cpp:1538-1542).")

add("FindFirstOf", "FindFirstOf(string ShortClassName) -> UObject|nil", "object-management", A, V_LEGACY,
    "Iterates the live GUObjectArray. No input/paths/file I/O.",
    True, "")

add("FindAllOf", "FindAllOf(string ShortClassName) -> table<UObject>|nil", "object-management", A, V_LEGACY,
    "Iterates the live GUObjectArray (reserves 40 elements). No input/paths/file I/O.",
    True, "")

add("StaticConstructObject", "StaticConstructObject(UClass Class, UObject Outer, [FName|integer Name], [integer EObjectFlags], [integer EInternalObjectFlags], [bool CopyTransientsFromClassDefaults], [bool AssumeTemplateIsArchetype], [UObject Template], [integer InstanceGraphPtr], [integer ExternalPackagePtr]) -> UObject", "object-management", A, V_LEGACY,
    "Calls Unreal::UObjectGlobals::StaticConstructObject (engine StaticConstructObject_Internal). No input/file I/O.",
    True, "InstanceGraph/ExternalPackage are raw pointer integers; SubobjectOverrides param is commented out in this port.")

add("RegisterCustomProperty", "RegisterCustomProperty(table PropertyInfo) -> ()", "reflection", A, V_LEGACY,
    "Registers a synthetic FProperty (CustomArrayProperty) for a live class; resolves BelongsToClass via StaticFindObject. Prints a PropertyInfo dump via printf/console (UE4SS_LUAMOD_PRINTF) on every registration. No input/file I/O.",
    True, "Table fields: Name, Type{Name,Size,FFieldClassPointer,StaticPointer}, BelongsToClass, OffsetInternal (int or {Property,RelativeOffset}), ArrayProperty{Type{...}} for arrays.")

add("ForEachUObject", "ForEachUObject(function Callback(UObject Object, integer ChunkIndex, integer ObjectIndex) -> bool) -> ()", "object-management", A, V_LEGACY,
    "Iterates every object in the live GUObjectArray. No input/file I/O.",
    True, "Callback returning true stops iteration (LoopAction); binding at 2567.")

# Keybind / input family: true mods only (IsTrueMod::Yes)
A_T = f"{LM}:1735-2197 (setup_lua_global_functions_internal, gated by is_true_mod == IsTrueMod::Yes -> true Lua mods only, NOT the console executor)"

add("IsKeyBindRegistered", "IsKeyBindRegistered(integer Key) -> bool | IsKeyBindRegistered(integer Key, table<integer> ModifierKeys) -> bool", "input", A_T, V_LEGACY,
    "Keyboard event registry. Key codes are Windows VK values (global 'Key' table, LuaMod.cpp:614). On this Prospero port UE4SSProgram::is_keydown_event_registered throws 'Native input service is unavailable' unless an injected platform input source is active (UE4SSProgram.cpp:2903-2925; PS5 pad injection is separate port work).",
    False, "PC input model; console executor state never registers it.")

add("RegisterKeyBindAsync", "RegisterKeyBindAsync(integer Key, function Callback) -> () | RegisterKeyBindAsync(integer Key, table<integer> ModifierKeys, function Callback) -> ()", "input", A_T, V_LEGACY,
    "Registers a keydown callback fired from the input thread without the game-thread action lock (contrast RegisterKeyBind). PC: Win32/GLFW input sources. Prospero: register_keydown_event throws 'Native input service is unavailable' unless an injected input source is active (UE4SSProgram.cpp:2882-2896).",
    False, "Registration only; delivery requires a live input pump.")

add("RegisterKeyBind", "RegisterKeyBind(integer Key, function Callback) -> () | RegisterKeyBind(integer Key, table<integer> ModifierKeys, function Callback) -> ()", "input", A_T, V_LEGACY,
    "Same as RegisterKeyBindAsync but callbacks dispatch under LuaMod::m_thread_actions_mutex (game-thread-safe). PC: Win32/GLFW input sources; Prospero: throws unless injected input source is active. Key/ModifierKey globals use Windows VK codes / VK_SHIFT-family values.",
    False, "")

add("UnregisterHook", "UnregisterHook(string UFunctionName, integer PreCallbackId, integer PostCallbackId) -> ()", "hooking", A_T, V_LEGACY,
    "Resolves the UFunction via StaticFindObject then removes native pre/post hooks (UFunction::UnregisterHook) or script-hook registry entries. No input/file I/O.",
    True, "Ids come from RegisterHook's return values; binding at 2032.")

add("DumpAllObjects", "DumpAllObjects() -> ()", "debug", A_T, V_LEGACY,
    "Writes an object/property dump via UE4SSProgram::dump_all_objects_and_properties into the ObjectDumper output directory. PC-only path building: joins path components with a literal backslash (STR(\"\\\\\"), LuaMod.cpp:2131). File I/O outside the mod directory.",
    True, "")

add("GenerateSDK", "GenerateSDK() -> ()", "debug", A_T, V_LEGACY,
    "Generates C++ headers (CXXHeaderDump) under the UE4SS working directory. PC-only path building with literal '\\\\CXXHeaderDump' (LuaMod.cpp:2143). Heavy reflection + file I/O outside the mod directory; long-running.",
    True, "")

add("GenerateLuaTypes", "GenerateLuaTypes() -> ()", "debug", A_T, V_LEGACY,
    "Generates Lua type definitions under working_dir + '\\\\Mods\\\\shared\\\\types' (literal Windows separators, LuaMod.cpp:2154). File I/O outside the mod directory.",
    True, "")

add("GenerateUHTCompatibleHeaders", "GenerateUHTCompatibleHeaders() -> ()", "debug", A_T, V_LEGACY,
    "Generates UHT-compatible headers via UE4SSProgram::generate_uht_compatible_headers (SDKGenerator pipeline). File I/O outside the mod directory.",
    True, "")

add("DumpStaticMeshes", "DumpStaticMeshes() -> ()", "debug", A_T, V_LEGACY,
    "GUI::Dumpers::call_generate_static_mesh_file() (GUI subsystem dump; file output). File I/O outside the mod directory.",
    True, "")

add("DumpAllActors", "DumpAllActors() -> ()", "debug", A_T, V_LEGACY,
    "GUI::Dumpers::call_generate_all_actor_file() (GUI subsystem dump; file output). File I/O outside the mod directory.",
    True, "")

add("DumpUSMAP", "DumpUSMAP([bool IncludeBlueprintTypes=true]) -> ()", "debug", A_T, V_LEGACY,
    "OutTheShade::generate_usmap — writes a .usmap mapping file. File I/O outside the mod directory.",
    True, "")

add("DumpJMAP", "DumpJMAP([bool IncludeBlueprintTypes=true], [bool SkipPropertyValues=false]) -> ()", "debug", A_T, V_LEGACY,
    "JMapGenerator::generate_jmap — writes a .jmap file. File I/O outside the mod directory.",
    True, "")

# Hooks / events (all mod states + console executor for these)
add("NotifyOnNewObject", "NotifyOnNewObject(string UClassName, function Callback(UObject NewObject)) -> ()", "hooking", A, V_LEGACY,
    "Hooks StaticConstructObject; fires from the game thread when a UObject of the class is constructed. Class name must contain a dot and no spaces. No input/file I/O.",
    True, "Moves the callback into the mod's dedicated hook Lua thread (make_hook_state).")

add("RegisterCustomEvent", "RegisterCustomEvent(string EventName, function Callback(RemoteUnrealParam)) -> ()", "hooking", A, V_LEGACY,
    "Registers a script-side custom event dispatched via ProcessEvent/script hooking on a synthetic UFunction name. No input/file I/O.",
    True, "")

add("UnregisterCustomEvent", "UnregisterCustomEvent(string EventName) -> ()", "hooking", A, V_LEGACY,
    "Removes a RegisterCustomEvent registration from LuaMod::m_custom_event_callbacks. No input/file I/O.",
    True, "")

add("RegisterLoadMapPreHook", "RegisterLoadMapPreHook(function Callback(ULevel Level, UWorld World, FString Error)) -> ()", "hooking", A, V_LEGACY,
    "Engine LoadMap pre-hook. No input/file I/O.",
    True, "")

add("RegisterLoadMapPostHook", "RegisterLoadMapPostHook(function Callback(UWorld World)) -> ()", "hooking", A, V_LEGACY,
    "Engine LoadMap post-hook. No input/file I/O.",
    True, "")

add("RegisterInitGameStatePreHook", "RegisterInitGameStatePreHook(function Callback(AGameModeBase GameMode)) -> ()", "hooking", A, V_LEGACY,
    "Engine InitGameState pre-hook. No input/file I/O.",
    True, "")

add("RegisterInitGameStatePostHook", "RegisterInitGameStatePostHook(function Callback(AGameModeBase GameMode)) -> ()", "hooking", A, V_LEGACY,
    "Engine InitGameState post-hook. No input/file I/O.",
    True, "")

add("RegisterBeginPlayPreHook", "RegisterBeginPlayPreHook(function Callback(AActor Actor)) -> ()", "hooking", A, V_LEGACY,
    "Engine BeginPlay pre-hook. No input/file I/O.",
    True, "")

add("RegisterBeginPlayPostHook", "RegisterBeginPlayPostHook(function Callback(AActor Actor)) -> ()", "hooking", A, V_LEGACY,
    "Engine BeginPlay post-hook. No input/file I/O.",
    True, "")

add("RegisterEndPlayPreHook", "RegisterEndPlayPreHook(function Callback(AActor Actor, integer EEndPlayReason)) -> ()", "hooking", A, V_LEGACY,
    "Engine EndPlay pre-hook. No input/file I/O.",
    True, "")

add("RegisterEndPlayPostHook", "RegisterEndPlayPostHook(function Callback(AActor Actor, integer EEndPlayReason)) -> ()", "hooking", A, V_LEGACY,
    "Engine EndPlay post-hook. No input/file I/O.",
    True, "")

add("IterateGameDirectories", "IterateGameDirectories() -> table|nil", "file-io", A, V_LEGACY,
    "std::filesystem traversal of the game install root (game executable directory's grandparent/Content). Hardcoded PC layout assumption: warns 'not <RootGamePath>/Game/Binaries/Win64' (LuaMod.cpp:2943). Returns a nested directory tree with __name/__absolute_path/__files metatable accessors; depth capped at 64; the __files accessor can CREATE <Content>/Paks/LogicMods as a fallback (file writes outside the mod dir). Read-only at the top level, but the metatable path performs filesystem existence checks and directory creation.",
    False, "Needs a game install tree (app0 mount on PS5), not a running engine; binding at 2933.")

add("CreateLogicModsDirectory", "CreateLogicModsDirectory() -> bool", "file-io", A, V_LEGACY,
    "Creates <GameContent>/Paks and <GameContent>/Paks/LogicMods via std::filesystem::create_directory (file I/O outside the mod dir). On PS5 the game content mount is read-only, so creation would fail; the function still returns/throws based on filesystem results. No input.",
    False, "")

add("ExecuteAsync", "ExecuteAsync(function Callback) -> ()", "threading", A, V_LEGACY,
    "Queues the callback onto UE4SS's per-mod async thread (m_pending_actions, ActionType::Immediate). No engine state required by the mechanism itself. No input/file I/O.",
    False, "Callbacks usually touch live game objects from the async thread.")

add("ExecuteWithDelay", "ExecuteWithDelay(integer DelayInMilliseconds, function Callback) -> ()", "threading", A, V_LEGACY,
    "Queues a one-shot delayed callback on the UE4SS async thread (ActionType::Delayed, steady_clock). No input/file I/O.",
    False, "")

add("LoopAsync", "LoopAsync(integer DelayInMilliseconds, function Callback) -> ()", "threading", A, V_LEGACY,
    "Queues a repeating callback on the UE4SS async thread (ActionType::Loop). No input/file I/O.",
    False, "")

add("RegisterProcessConsoleExecPreHook", "RegisterProcessConsoleExecPreHook(function Callback(UObject Context, string Command, FOutputDevice Ar) -> bool|string) -> ()", "hooking", A, V_LEGACY,
    "UObject::ProcessConsoleExec pre-hook; return false to block, string to rewrite the command. No input/file I/O.",
    True, "")

add("RegisterProcessConsoleExecPostHook", "RegisterProcessConsoleExecPostHook(function Callback(UObject Context, string Command, FOutputDevice Ar)) -> ()", "hooking", A, V_LEGACY,
    "UObject::ProcessConsoleExec post-hook. No input/file I/O.",
    True, "")

add("RegisterCallFunctionByNameWithArgumentsPreHook", "RegisterCallFunctionByNameWithArgumentsPreHook(function Callback(UObject Context, string Command, FOutputDevice Ar, bool bForceCallWithLocalExec) -> bool) -> ()", "hooking", A, V_LEGACY,
    "UObject::CallFunctionByNameWithArguments pre-hook (requires that AOB scan to have succeeded). No input/file I/O.",
    True, "")

add("RegisterCallFunctionByNameWithArgumentsPostHook", "RegisterCallFunctionByNameWithArgumentsPostHook(function Callback(UObject Context, string Command, FOutputDevice Ar, bool bForceCallWithLocalExec)) -> ()", "hooking", A, V_LEGACY,
    "UObject::CallFunctionByNameWithArguments post-hook. No input/file I/O.",
    True, "")

add("RegisterULocalPlayerExecPreHook", "RegisterULocalPlayerExecPreHook(function Callback(ULocalPlayer Context, string Command, FOutputDevice Ar) -> bool|string) -> ()", "hooking", A, V_LEGACY,
    "ULocalPlayer::Exec pre-hook. No input/file I/O.",
    True, "")

add("RegisterULocalPlayerExecPostHook", "RegisterULocalPlayerExecPostHook(function Callback(ULocalPlayer Context, string Command, FOutputDevice Ar)) -> ()", "hooking", A, V_LEGACY,
    "ULocalPlayer::Exec post-hook. No input/file I/O.",
    True, "")

add("RegisterConsoleCommandGlobalHandler", "RegisterConsoleCommandGlobalHandler(string CommandName, function Callback) -> ()", "hooking", A, V_LEGACY,
    "Registers a global console command handler (all console traffic for CommandName, before the game sees it). No input/file I/O.",
    True, "")

add("RegisterConsoleCommandHandler", "RegisterConsoleCommandHandler(string CommandName, function Callback) -> ()", "hooking", A, V_LEGACY,
    "Registers a mod-scoped console command handler. No input/file I/O.",
    True, "")

add("LoadAsset", "LoadAsset(string AssetPathAndName) -> UObject Asset, bool WasFound, bool DidLoad", "object-management", A, V_LEGACY,
    "Requires the calling thread to be the engine game thread (throws otherwise) and a live UAssetRegistry. No input/file I/O.",
    True, "Binding at 3618; three return values.")

add("FindObject", "FindObject(UClass InClass, UObject|UClass InOuter, string Name, bool ExactClass) -> UObject|nil | FindObject(string|FName|nil ClassName, string|FName|nil ObjectShortName, integer EObjectFlags RequiredFlags, integer EObjectFlags BannedFlags) -> UObject|nil | FindObject(UClass|nil Class, string|FName|nil ObjectShortName, integer RequiredFlags, integer BannedFlags) -> UObject|nil", "object-management", A, V_LEGACY,
    "Reflection lookup over the live UObject hash tables; integer -1 for InOuter means ANY_PACKAGE. No input/file I/O.",
    True, "")

add("FindObjects", "FindObjects(integer NumObjectsToFind, string|FName|nil ClassName, string|FName|nil ObjectShortName, [integer RequiredFlags], [integer BannedFlags], [bool bExactClass=true]) -> table<UObject>", "object-management", A, V_LEGACY,
    "Reflection batch lookup over the live UObject hash tables. No input/file I/O.",
    True, "ClassName and ObjectShortName cannot both be nil.")

add("GetCurrentThreadId", "GetCurrentThreadId() -> ThreadId userdata", "threading", A, V_LEGACY,
    "std::this_thread::get_id wrapped in a ThreadId userdata (LuaType/LuaThreadId). Pure computation.",
    False, "")

add("GetMainModThreadId", "GetMainModThreadId() -> ThreadId userdata", "threading", A, V_LEGACY,
    "Returns the mod's main thread id (UE4SS-internal state). Pure lookup.",
    False, "")

add("GetAsyncThreadId", "GetAsyncThreadId() -> ThreadId userdata", "threading", A, V_LEGACY,
    "Returns the mod's async thread id (UE4SS-internal state). Pure lookup.",
    False, "")

add("GetGameThreadId", "GetGameThreadId() -> ThreadId userdata", "threading", A, V_LEGACY,
    "Returns Unreal::GetGameThreadId() — the live engine game thread identity (populated by engine hooks).",
    True, "")

add("IsInMainModThread", "IsInMainModThread() -> bool", "threading", A, V_LEGACY,
    "Compares std::this_thread::get_id against the mod main thread id. Pure comparison.",
    False, "")

add("IsInAsyncThread", "IsInAsyncThread() -> bool", "threading", A, V_LEGACY,
    "Compares against the mod async thread id. Pure comparison.",
    False, "")

add("IsInGameThread", "IsInGameThread() -> bool", "threading", A, V_LEGACY,
    "Compares against Unreal::GetGameThreadId() — meaningful only with a live game thread.",
    True, "")

# ---------------- Site B: setup_lua_global_functions_main_state_only (mod main Lua state only) ----------------
B = f"{LM}:4565-6057 (setup_lua_global_functions_main_state_only, called from LuaMod::start_mod at {LM}:6448 — main Lua state of true mods only)"

add("RegisterHook", "RegisterHook(string UFunctionName, function Callback(RemoteUnrealParam), [function PostCallback(RemoteUnrealParam, RemoteUnrealParam)]) -> integer PreCallbackId, integer PostCallbackId", "hooking", B, V_LEGACY,
    "Resolves the UFunction via StaticFindObject and installs native pre/post hooks (RegisterPreHook/RegisterPostHook for FUNC_Native) or a script-hook registry entry (ProcessInternal path). No input/file I/O.",
    True, "Callback may return a table to overwrite params/return value.")

add("ExecuteInGameThread", "ExecuteInGameThread(function Callback) -> () | ExecuteInGameThread(function Callback, EGameThreadMethod Method) -> ()", "threading", B, V_30,
    "Enqueues a callback for the engine game thread via EngineTick or ProcessEvent hooks; throws if the required hook's AOB scan failed. Method constants exposed via the global EGameThreadMethod table. On Prospero the port adds availability checks and pending queues (LuaMod.cpp:4690-4769).",
    True, "")

add("ExecuteInGameThreadWithDelay", "ExecuteInGameThreadWithDelay(integer DelayMs, function Callback) -> integer Handle | ExecuteInGameThreadWithDelay(integer Handle, integer DelayMs, function Callback) -> ()", "threading", B, V_30,
    "Delayed one-shot on the game thread; overload 2 only creates the action if Handle doesn't exist (UE Delay-node semantics). Falls back between EngineTick/ProcessEvent when the default is unavailable (Prospero-aware).",
    True, "")

add("RetriggerableExecuteInGameThreadWithDelay", "RetriggerableExecuteInGameThreadWithDelay(integer Handle, integer DelayMs, function Callback) -> ()", "threading", B, V_30,
    "Delayed one-shot whose timer resets when re-called with the same handle (UE Retriggerable Delay semantics); EngineTick/ProcessEvent fallback logic as above.",
    True, "")

add("ExecuteInGameThreadAfterFrames", "ExecuteInGameThreadAfterFrames(integer Frames, function Callback) -> integer Handle", "threading", B, V_30,
    "Frame-counted delay; hard-requires the EngineTick hook (throws if unavailable — ProcessEvent cannot count frames).",
    True, "")

add("LoopInGameThreadWithDelay", "LoopInGameThreadWithDelay(integer DelayMs, function Callback) -> integer Handle", "threading", B, V_30,
    "Repeating game-thread callback on a time interval; EngineTick/ProcessEvent fallback logic.",
    True, "")

add("LoopInGameThreadAfterFrames", "LoopInGameThreadAfterFrames(integer Frames, function Callback) -> integer Handle", "threading", B, V_30,
    "Repeating game-thread callback on a frame interval; hard-requires EngineTick.",
    True, "")

add("ResetDelayedActionTimer", "ResetDelayedActionTimer(integer Handle) -> bool Success", "threading", B, V_30,
    "UE4SS-internal delayed-action container bookkeeping (restarts a timer); no direct engine access.",
    False, "Delay/loop action family query+management API.")

add("SetDelayedActionTimer", "SetDelayedActionTimer(integer Handle, integer NewDelayMs) -> bool Success", "threading", B, V_30,
    "UE4SS-internal bookkeeping (changes an action's interval).",
    False, "")

add("PauseDelayedAction", "PauseDelayedAction(integer Handle) -> bool Success", "threading", B, V_30,
    "UE4SS-internal bookkeeping (pauses an action).",
    False, "")

add("UnpauseDelayedAction", "UnpauseDelayedAction(integer Handle) -> bool Success", "threading", B, V_30,
    "UE4SS-internal bookkeeping (resumes an action).",
    False, "")

add("CancelDelayedAction", "CancelDelayedAction(integer Handle) -> bool Success", "threading", B, V_30,
    "UE4SS-internal bookkeeping (cancels and releases the callback ref).",
    False, "")

add("IsValidDelayedActionHandle", "IsValidDelayedActionHandle(integer Handle) -> bool Valid", "threading", B, V_30,
    "UE4SS-internal container query.",
    False, "")

add("IsDelayedActionActive", "IsDelayedActionActive(integer Handle) -> bool Active", "threading", B, V_30,
    "UE4SS-internal container query.",
    False, "")

add("IsDelayedActionPaused", "IsDelayedActionPaused(integer Handle) -> bool Paused", "threading", B, V_30,
    "UE4SS-internal container query.",
    False, "")

add("GetDelayedActionTimeRemaining", "GetDelayedActionTimeRemaining(integer Handle) -> integer RemainingMs | -1", "threading", B, V_30,
    "UE4SS-internal container query.",
    False, "")

add("GetDelayedActionTimeElapsed", "GetDelayedActionTimeElapsed(integer Handle) -> integer ElapsedMs | -1", "threading", B, V_30,
    "UE4SS-internal container query.",
    False, "")

add("GetDelayedActionRate", "GetDelayedActionRate(integer Handle) -> integer RateMs | -1", "threading", B, V_30,
    "UE4SS-internal container query (configured interval).",
    False, "")

add("ClearAllDelayedActions", "ClearAllDelayedActions() -> integer Count", "threading", B, V_30,
    "Cancels every delayed/looping game-thread action for the calling mod (UE4SS-internal).",
    False, "")

add("MakeActionHandle", "MakeActionHandle() -> integer Handle", "threading", B, V_30,
    "Pure counter increment (LuaMod::m_next_delayed_action_handle++) for caller-managed handles.",
    False, "")

add("RestartCurrentMod", "RestartCurrentMod() -> ()", "other", B, V_30,
    "Queues a reinstall of the calling mod via UE4SSProgram::queue_reinstall_mod (cross-thread-safe by ModId). No engine state, no file I/O by the caller.",
    False, "Mod-lifecycle family.")

add("UninstallCurrentMod", "UninstallCurrentMod() -> ()", "other", B, V_30,
    "Queues an uninstall of the calling mod (queue_uninstall_mod).",
    False, "")

add("RestartMod", "RestartMod(string ModName) -> ()", "other", B, V_30,
    "Queues a reinstall of a named mod (queue_reinstall_mod_by_name).",
    False, "")

add("UninstallMod", "UninstallMod(string ModName) -> ()", "other", B, V_30,
    "Queues an uninstall of a named mod (queue_uninstall_mod_by_name).",
    False, "")

# ---------------- Site C: setup_lua_classes_internal (global constructors + global class tables) ----------------
C = f"{LM}:6101-6288 (setup_lua_classes_internal; every true-mod state via {LM}:6302 and the console executor at {LM}:6726)"

add("FString", "FString(string Text) -> FString userdata", "string", C, V_30,
    "Constructs an Unreal FString from a Lua string (TString/TArray-backed storage via FMemory). No input/file I/O.",
    False, "")

add("FUtf8String", "FUtf8String(string Text) -> FUtf8String userdata", "string", C, V_30,
    "Constructs an FUtf8String from UTF-8 Lua string bytes. No input/file I/O.",
    False, "")

add("FAnsiString", "FAnsiString(string Text) -> FAnsiString userdata", "string", C, V_30,
    "Constructs an FAnsiString from Lua string bytes. No input/file I/O.",
    False, "")

add("UE4SS.GetVersion", "UE4SS.GetVersion() -> integer Major, integer Minor, integer Hotfix", "debug", C, V_LEGACY,
    "Returns compiled UE4SS_LIB_VERSION constants. Pure computation.",
    False, "Method of the global 'UE4SS' table (make_global at 6113).")

add("UnrealVersion.GetMajor", "UnrealVersion.GetMajor() -> integer", "debug", C, V_LEGACY,
    "Compiled-in detected engine major version. Pure computation.",
    False, "Method of the global 'UnrealVersion' table (make_global at 6184).")

add("UnrealVersion.GetMinor", "UnrealVersion.GetMinor() -> integer", "debug", C, V_LEGACY,
    "Compiled-in detected engine minor version. Pure computation.",
    False, "")

add("UnrealVersion.IsEqual", "UnrealVersion.IsEqual(number Major, number Minor) -> bool", "debug", C, V_LEGACY,
    "Compiled-in version comparison. Pure computation.",
    False, "")

add("UnrealVersion.IsAtLeast", "UnrealVersion.IsAtLeast(number Major, number Minor) -> bool", "debug", C, V_LEGACY,
    "Compiled-in version comparison. Pure computation.",
    False, "")

add("UnrealVersion.IsAtMost", "UnrealVersion.IsAtMost(number Major, number Minor) -> bool", "debug", C, V_LEGACY,
    "Compiled-in version comparison. Pure computation.",
    False, "")

add("UnrealVersion.IsBelow", "UnrealVersion.IsBelow(number Major, number Minor) -> bool", "debug", C, V_LEGACY,
    "Compiled-in version comparison. Pure computation.",
    False, "")

add("UnrealVersion.IsAbove", "UnrealVersion.IsAbove(number Major, number Minor) -> bool", "debug", C, V_LEGACY,
    "Compiled-in version comparison. Pure computation.",
    False, "")

add("FPackageName.IsShortPackageName", "FPackageName.IsShortPackageName(string PossiblyLongName) -> bool", "other", C, V_LEGACY,
    "Static Unreal::FPackageName::IsShortPackageName — pure string/formula check, no engine state.",
    False, "Method of the global 'FPackageName' table (make_global at 6286).")

add("FPackageName.IsValidLongPackageName", "FPackageName.IsValidLongPackageName(string InLongPackageName) -> bool", "other", C, V_LEGACY,
    "Static package-name validation — pure string check.",
    False, "")

# ---------------- Site D: native module loader overrides (PS5 port addition) ----------------
D = f"{LM}:1181 (LuaMod::setup_custom_module_loader, {LM}:1124 — only when a native mod source is configured; PS5/port path)"

add("loadfile", "loadfile(string Filename) -> function|nil, string|nil Error", "file-io", D, V_PORT,
    "REPLACES the Lua stdlib loadfile with native_lua_loadfile: file reads are served from the Platform::NativeModSource buffer set (PS5 native mod payload), never from the target's disk. On PC builds (no native source) the stdlib loadfile remains untouched and a package.searchers entry is inserted instead.",
    False, "Registered via lua_setglobal in the file_functions loop; active only for native-source mods.")

add("dofile", "dofile(string Filename) -> ...results", "file-io", D, V_PORT,
    "REPLACES the Lua stdlib dofile with native_lua_dofile (native mod source buffers). PC builds keep the stdlib version.",
    False, "")

# ---------------- Site E: signature-scan Lua state (Signatures.cpp) ----------------
E = "src/Signatures.cpp:62-67 (scan_from_lua_script — separate temporary Lua state used ONLY by UE4SS_Signatures/*.lua AOB scan scripts, not by mod main.lua)"

add("Print", "Print(...any) -> ()", "debug", E, V_LEGACY,
    "Same binding as print (LuaLibrary::global_print): UE4SS console/GUI output; no input/file I/O. Only exists inside the signature-scan sandbox state.",
    False, "Signature-scan scripts must also define global Register() and OnMatchFound() themselves (Signatures.cpp:112-115).")

add("DerefToInt32", "DerefToInt32(integer Address) -> integer Value | nil", "debug", E, V_LEGACY,
    "Reads an int32 from process memory. Implementation is Windows-only: LuaLibraryWindows.cpp is guarded by #ifdef _WIN32 and validates the pointer against GetCurrentProcess (Helper::Casting::offset_deref_safe). No non-Windows definition exists in this port snapshot — on the PS5 build this registration has no backing implementation in the shown sources.",
    True, "Used by signature scripts to verify scan results.")

add("dereftoint32", "dereftoint32(integer Address) -> integer Value | nil", "debug", E, V_LEGACY,
    "Lowercase alias of DerefToInt32; same Windows-only implementation caveats.",
    True, "")

add("loadexport", "loadexport(string SymbolName) -> integer Address", "other", E, V_LEGACY,
    "Lowercase alias of LoadExport inside the signature-scan sandbox; Win32 GetProcAddress path on PC, native FindExport on the port.",
    True, "")

# ---- print and LoadExport also appear in site E (aliases/second registration) — record multi-site facts ----
for e in entries:
    if e["name"] == "print":
        e["registration_sites"].append("src/Signatures.cpp:63 (signature-scan sandbox state)")
        e["notes"] = (e["notes"] + " Also re-registered (plus capitalized alias Print) in the signature-scan sandbox.").strip()
    if e["name"] == "LoadExport":
        e["registration_sites"].append("src/Signatures.cpp:66 (signature-scan sandbox state; lowercase alias loadexport registered alongside)")

# ---------------- Sort: by category then name ----------------
cat_order = ["object-management", "reflection", "math", "string", "file-io", "input", "hooking", "threading", "debug", "settings", "other"]
entries.sort(key=lambda e: (cat_order.index(e["category"]), e["name"].lower()))

with open("/tmp/sideproject/catalog-globals/globals.json", "w") as f:
    json.dump(entries, f, indent=2, ensure_ascii=False)
    f.write("\n")

print(f"wrote {len(entries)} entries")
from collections import Counter
c = Counter(e["category"] for e in entries)
for k in cat_order:
    if c.get(k):
        print(f"  {k}: {c[k]}")
