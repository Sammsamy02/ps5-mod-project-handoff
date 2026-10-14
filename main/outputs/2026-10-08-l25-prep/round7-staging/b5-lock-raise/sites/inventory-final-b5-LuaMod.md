| # | file:lines | lock (mutex) | context | entry | Lua raise inside | verdict |
|---|---|---|---|---|---|---|
| 1 | src/LuaMod.cpp:265-278 | lock_guard LuaMod::m_thread_actions_mutex | Queue | native/C++ | own pcall/call_function | safe: errors stay inside own pcall / C++ catch |
| 2 | src/LuaMod.cpp:2251-2261 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterKeyBind" | TRY-wrapped (process_lua_function) | ambient helper at 2254; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 3 | src/LuaMod.cpp:2366-2459 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnregisterHook" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 4 | src/LuaMod.cpp:3083-3123 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterCustomEvent" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 5 | src/LuaMod.cpp:3126-3142 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnregisterCustomEvent" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 6 | src/LuaMod.cpp:4610-4632 | unique_lock LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 7 | src/LuaMod.cpp:4720-4727 | lock_guard LuaMod::m_thread_actions_mutex | process_simple_actions | native/C++ | - | none: no Lua call/raise in scope |
| 8 | src/LuaMod.cpp:4757-4762 | lock_guard LuaMod::m_thread_actions_mutex | process_simple_actions | native/C++ | - | none: no Lua call/raise in scope |
| 9 | src/LuaMod.cpp:4781-4880 | lock_guard LuaMod::m_thread_actions_mutex | process_pending_notify_on_new_object_callbacks | native/C++ | ambient helper at 4844,4845; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 10 | src/LuaMod.cpp:4895-4960 | lock_guard LuaMod::m_thread_actions_mutex | process_delayed_actions | native/C++ | - | none: no Lua call/raise in scope |
| 11 | src/LuaMod.cpp:4978-5035 | lock_guard LuaMod::m_thread_actions_mutex | process_delayed_actions | native/C++ | - | none: no Lua call/raise in scope |
| 12 | src/LuaMod.cpp:5057-5059 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 13 | src/LuaMod.cpp:5067-5085 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 14 | src/LuaMod.cpp:5510-5512 | lock_guard LuaMod::m_thread_actions_mutex | funcptr_marshal_hook | native/C++ | - | none: no Lua call/raise in scope |
| 15 | src/LuaMod.cpp:5520-5538 | lock_guard LuaMod::m_thread_actions_mutex | funcptr_marshal_hook | native/C++ | - | none: no Lua call/raise in scope |
| 16 | src/LuaMod.cpp:5584-5600 | lock_guard LuaMod::m_thread_actions_mutex | VmtEngineTickDrain | native/C++ | - | none: no Lua call/raise in scope |
| 17 | src/LuaMod.cpp:5616-5634 | lock_guard LuaMod::m_thread_actions_mutex | VmtEngineTickDrain | native/C++ | - | none: no Lua call/raise in scope |
| 18 | src/LuaMod.cpp:5662-5664 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 19 | src/LuaMod.cpp:5672-5690 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 20 | src/LuaMod.cpp:5788-5841 | lock_guard m_funcptr_marshal_carrier_mutex | r3_marshal_arm_event_carriers | native/C++ | - | none: no Lua call/raise in scope |
| 21 | src/LuaMod.cpp:5921-5934 | lock_guard m_thread_actions_mutex | LuaMod::ensure_engine_tick_hooked | native/C++ | - | none: no Lua call/raise in scope |
| 22 | src/LuaMod.cpp:5940-5962 | lock_guard m_thread_actions_mutex | LuaMod::ensure_process_event_hooked | native/C++ | - | none: no Lua call/raise in scope |
| 23 | src/LuaMod.cpp:5967-6070 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterHook" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 24 | src/LuaMod.cpp:6236-6369 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThread" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 25 | src/LuaMod.cpp:6513-6560 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 26 | src/LuaMod.cpp:6567-6596 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 27 | src/LuaMod.cpp:6719-6775 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RetriggerableExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 28 | src/LuaMod.cpp:6820-6851 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadAfterFrames" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 29 | src/LuaMod.cpp:6972-7002 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "LoopInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 30 | src/LuaMod.cpp:7047-7080 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "LoopInGameThreadAfterFrames" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 31 | src/LuaMod.cpp:7100-7140 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ResetDelayedActionTimer" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 32 | src/LuaMod.cpp:7166-7210 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "SetDelayedActionTimer" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 33 | src/LuaMod.cpp:7234-7289 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "PauseDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 34 | src/LuaMod.cpp:7313-7339 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnpauseDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 35 | src/LuaMod.cpp:7363-7386 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "CancelDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 36 | src/LuaMod.cpp:7408-7428 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsValidDelayedActionHandle" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 37 | src/LuaMod.cpp:7450-7470 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsDelayedActionActive" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 38 | src/LuaMod.cpp:7492-7512 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsDelayedActionPaused" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 39 | src/LuaMod.cpp:7534-7595 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionTimeRemaining" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 40 | src/LuaMod.cpp:7617-7680 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionTimeElapsed" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 41 | src/LuaMod.cpp:7702-7738 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionRate" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 42 | src/LuaMod.cpp:7756-7775 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ClearAllDelayedActions" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 43 | src/LuaMod.cpp:8429-8432 | lock_guard m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 44 | src/LuaMod.cpp:8445-8588 | lock_guard LuaMod::m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 45 | src/LuaMod.cpp:8536-8559 | lock_guard m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 46 | src/LuaMod.cpp:8632-8781 | lock_guard LuaMod::m_thread_actions_mutex | script_hook | native/C++ | ambient helper at 8663,8666; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 47 | src/LuaMod.cpp:9027-9121 | lock_guard LuaMod::m_thread_actions_mutex | LuaMod::on_program_start | native/C++ | ambient helper at 9060,9061; own pcall/call_function | residual: ambient-pcall only (WARN) |
