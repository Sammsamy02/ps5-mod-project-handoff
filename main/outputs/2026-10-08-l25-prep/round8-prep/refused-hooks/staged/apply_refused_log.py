import re, sys
p = sys.argv[1]; helper = open(sys.argv[2]).read()
s = open(p).read()
def once(old, new, label):
    global s
    n = s.count(old)
    assert n == 1, (label, n)
    s = s.replace(old, new)
# E1 helper: right before get_function_name_without_prefix (after get_mod_ref)
once("    static auto get_function_name_without_prefix(const StringType& function_full_name) -> StringType\n",
     helper + "    static auto get_function_name_without_prefix(const StringType& function_full_name) -> StringType\n", "E1")

# E2 RegisterHook: function not found
m = re.search(r"(            if \(!unreal_function\)\n            \{\n)(                (?:lua\.throw_error|throw std::runtime_error)\(std::format\(\n                        \"Tried to register a hook with Lua function 'RegisterHook' but no UFunction)", s)
assert m and len(re.findall(r"Tried to register a hook with Lua function 'RegisterHook' but no UFunction", s)) == 1, "E2"
s = s[:m.end(1)] + '                refused_log::throws(lua, "RegisterHook", function_name_no_prefix, "ufunction-not-found");\n' + s[m.end(1):]

# E3 RegisterHook: unable to register (BP function with HookProcessInternal=0 etc.)
a = s.index("std::string error_message{\"Was unable to register a hook with Lua function 'RegisterHook', information:\\n\"};")
b = re.compile(r"                (?:lua\.throw_error|throw std::runtime_error)\(error_message\);").search(s, a).start()
ins = """                refused_log::throws(lua, "RegisterHook", function_name_no_prefix,
                                    !func_ptr ? "ufunction-func-null"
                                              : (unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native) ? "native-func-is-processinternal"
                                                                                                                           : "bp-script-function-needs-processinternal-hook"));
"""
s = s[:b] + ins + s[b:]

# E4 UnregisterHook: unable to unregister + not found
m = re.search(r"(                if \(!unreal_function\)\n                \{\n)(                    (?:lua\.throw_error|throw std::runtime_error)\(std::format\(\"Tried to unregister a hook with Lua function 'UnregisterHook' but no UFunction)", s)
assert m, "E4a"
s = s[:m.end(1)] + '                    refused_log::throws(lua, "UnregisterHook", function_name_no_prefix, "ufunction-not-found");\n' + s[m.end(1):]
a = s.index("std::string error_message{\"Was unable to unregister a hook with Lua function 'UnregisterHook', information:\\n\"};")
b = re.compile(r"                    (?:lua\.throw_error|throw std::runtime_error)\(error_message\);").search(s, a).start()
ins = """                    refused_log::throws(lua, "UnregisterHook", function_name_no_prefix,
                                        !func_ptr ? "ufunction-func-null"
                                                  : (unreal_function->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native) ? "native-func-is-processinternal"
                                                                                                                               : "bp-script-function-needs-processinternal-hook"));
"""
s = s[:b] + ins + s[b:]

# E5 accepted-but-undelivered registrations
fams = {
 "NotifyOnNewObject": ("class_name", "EagerDetour"),
 "RegisterCustomEvent": ("event_name", "CustomEvent"),
 "RegisterLoadMapPreHook": (None, "LoadMap"), "RegisterLoadMapPostHook": (None, "LoadMap"),
 "RegisterInitGameStatePreHook": (None, "EagerDetour"), "RegisterInitGameStatePostHook": (None, "EagerDetour"),
 "RegisterBeginPlayPreHook": (None, "EagerDetour"), "RegisterBeginPlayPostHook": (None, "EagerDetour"),
 "RegisterEndPlayPreHook": (None, "EagerDetour"), "RegisterEndPlayPostHook": (None, "EagerDetour"),
 "RegisterProcessConsoleExecPreHook": (None, "EagerDetour"), "RegisterProcessConsoleExecPostHook": (None, "EagerDetour"),
 "RegisterCallFunctionByNameWithArgumentsPreHook": (None, "EagerDetour"), "RegisterCallFunctionByNameWithArgumentsPostHook": (None, "EagerDetour"),
 "RegisterULocalPlayerExecPreHook": (None, "EagerDetour"), "RegisterULocalPlayerExecPostHook": (None, "EagerDetour"),
 "RegisterConsoleCommandHandler": ("command_name", "EagerDetour"), "RegisterConsoleCommandGlobalHandler": ("command_name", "EagerDetour"),
}
for name, (tvar, fam) in fams.items():
    key = 'lua.register_function("%s", [](const LuaMadeSimple::Lua& lua) -> int {' % name
    assert s.count(key) == 1, (name, s.count(key))
    a = s.index(key)
    b = s.index("\n            return 0;\n", a)
    nxt = s.index("lua.register_function(", a + 10)
    assert b < nxt, (name, "return 0 not inside this lambda")
    tgt = tvar if tvar else "StringType{STR(\"-\")}"
    line = '\n            refused_log::accepted(lua, "%s", %s, refused_log::Family::%s);' % (name, tgt, fam)
    s = s[:b] + line + s[b:]

# E6 frame-based refusals
for nm in ("ExecuteInGameThreadAfterFrames", "LoopInGameThreadAfterFrames"):
    for tail, why in (("frame-based delays need an EngineTick pump;", "frame-delays-need-enginetick-pump"), ("EngineTick hook is not available (AOB scan failed).", "enginetick-hook-unavailable")):
        rx = re.compile(r'                (?:lua\.throw_error|throw std::runtime_error)\("%s: %s' % (nm, re.escape(tail)))
        ms = list(rx.finditer(s)); assert len(ms) == 1, (nm, why, len(ms))
        s = s[:ms[0].start()] + '                refused_log::throws(lua, "%s", StringType{STR("-")}, "%s");\n' % (nm, why) + s[ms[0].start():]

# E7 state setters
once("            m_loadmap_deferred_arm_pending.store(true, std::memory_order_release);\n",
"""            m_loadmap_deferred_arm_pending.store(true, std::memory_order_release);
            refused_log::eager_detour_block_skipped.store(true, std::memory_order_release);
            refused_log::loadmap_state.store(1, std::memory_order_release);
            // One startup line for every family below that will accept registrations and never deliver them.
            refused_log::emit("detour-block", StringType{STR("InitGameState,BeginPlay,EndPlay,StaticConstructObject(NotifyOnNewObject),ULocalPlayerExec,CallFunctionByNameWithArguments,ProcessConsoleExec(+console handlers)")},
                              StringType{STR("*")}, "detour-block-skipped-r3-marshal", "never-delivered");
""", "E7a")
once("""            Output::send(STR("[R3] LoadMap detour armed (deferred arm; marshal world proven): queued Lua RegisterLoadMapPreHook/PostHook callbacks deliver on real LoadMap events\\n"));
            return;""",
"""            Output::send(STR("[R3] LoadMap detour armed (deferred arm; marshal world proven): queued Lua RegisterLoadMapPreHook/PostHook callbacks deliver on real LoadMap events\\n"));
            refused_log::loadmap_state.store(2, std::memory_order_release);
            return;""", "E7b")
# refusal path of the deferred arm: after its Error line (the statement ends the function)
a = s.index('Output::send<LogLevel::Error>(STR("[R3] LoadMap deferred arm refused (pre id {:X}, post id {:X})')
b = s.index(");\n", a) + 3
s = s[:b] + """        refused_log::loadmap_state.store(3, std::memory_order_release);
        refused_log::emit("RegisterLoadMapPre/PostHook", StringType{STR("UEngine::LoadMap")}, StringType{STR("*")}, "loadmap-detour-arm-refused", "never-delivered");
""" + s[b:]

# E8 Lua global: GetRefusedHookCount
once("""        lua.register_function("GetLuaMemory", [](const LuaMadeSimple::Lua& lua) -> int {""",
"""        // [REFUSED-LOG] number of [REFUSED] events so far (refused calls + registrations that can never fire).
        // Lets the self-test's GAPS line count real gaps. Read-only; never errors.
        lua.register_function("GetRefusedHookCount", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(static_cast<int64_t>(refused_log::total.load(std::memory_order_relaxed)));
            return 1;
        });

        lua.register_function("GetLuaMemory", [](const LuaMadeSimple::Lua& lua) -> int {""", "E8")
open(p, "w").write(s)
print("patched ok")
