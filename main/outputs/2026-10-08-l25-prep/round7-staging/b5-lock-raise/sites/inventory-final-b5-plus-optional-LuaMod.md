| # | file:lines | lock (mutex) | context | entry | Lua raise inside | verdict |
|---|---|---|---|---|---|---|
| 1 | src/LuaMod.cpp:265-278 | lock_guard LuaMod::m_thread_actions_mutex | Queue | native/C++ | own pcall/call_function | safe: errors stay inside own pcall / C++ catch |
| 2 | src/LuaMod.cpp:2267-2277 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterKeyBind" | TRY-wrapped (process_lua_function) | own pcall/call_function | safe: errors stay inside own pcall / C++ catch |
| 3 | src/LuaMod.cpp:2382-2475 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnregisterHook" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 4 | src/LuaMod.cpp:3099-3139 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterCustomEvent" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 5 | src/LuaMod.cpp:3142-3158 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnregisterCustomEvent" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 6 | src/LuaMod.cpp:4626-4648 | unique_lock LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 7 | src/LuaMod.cpp:4736-4743 | lock_guard LuaMod::m_thread_actions_mutex | process_simple_actions | native/C++ | - | none: no Lua call/raise in scope |
| 8 | src/LuaMod.cpp:4773-4778 | lock_guard LuaMod::m_thread_actions_mutex | process_simple_actions | native/C++ | - | none: no Lua call/raise in scope |
| 9 | src/LuaMod.cpp:4797-4896 | lock_guard LuaMod::m_thread_actions_mutex | process_pending_notify_on_new_object_callbacks | native/C++ | ambient helper at 4861; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 10 | src/LuaMod.cpp:4911-4976 | lock_guard LuaMod::m_thread_actions_mutex | process_delayed_actions | native/C++ | - | none: no Lua call/raise in scope |
| 11 | src/LuaMod.cpp:4994-5051 | lock_guard LuaMod::m_thread_actions_mutex | process_delayed_actions | native/C++ | - | none: no Lua call/raise in scope |
| 12 | src/LuaMod.cpp:5073-5075 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 13 | src/LuaMod.cpp:5083-5101 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 14 | src/LuaMod.cpp:5526-5528 | lock_guard LuaMod::m_thread_actions_mutex | funcptr_marshal_hook | native/C++ | - | none: no Lua call/raise in scope |
| 15 | src/LuaMod.cpp:5536-5554 | lock_guard LuaMod::m_thread_actions_mutex | funcptr_marshal_hook | native/C++ | - | none: no Lua call/raise in scope |
| 16 | src/LuaMod.cpp:5600-5616 | lock_guard LuaMod::m_thread_actions_mutex | VmtEngineTickDrain | native/C++ | - | none: no Lua call/raise in scope |
| 17 | src/LuaMod.cpp:5632-5650 | lock_guard LuaMod::m_thread_actions_mutex | VmtEngineTickDrain | native/C++ | - | none: no Lua call/raise in scope |
| 18 | src/LuaMod.cpp:5678-5680 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 19 | src/LuaMod.cpp:5688-5706 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 20 | src/LuaMod.cpp:5804-5857 | lock_guard m_funcptr_marshal_carrier_mutex | r3_marshal_arm_event_carriers | native/C++ | - | none: no Lua call/raise in scope |
| 21 | src/LuaMod.cpp:5937-5950 | lock_guard m_thread_actions_mutex | LuaMod::ensure_engine_tick_hooked | native/C++ | - | none: no Lua call/raise in scope |
| 22 | src/LuaMod.cpp:5956-5978 | lock_guard m_thread_actions_mutex | LuaMod::ensure_process_event_hooked | native/C++ | - | none: no Lua call/raise in scope |
| 23 | src/LuaMod.cpp:5983-6086 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterHook" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 24 | src/LuaMod.cpp:6252-6385 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThread" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 25 | src/LuaMod.cpp:6529-6576 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 26 | src/LuaMod.cpp:6583-6612 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 27 | src/LuaMod.cpp:6735-6791 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RetriggerableExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 28 | src/LuaMod.cpp:6836-6867 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadAfterFrames" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 29 | src/LuaMod.cpp:6988-7018 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "LoopInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 30 | src/LuaMod.cpp:7063-7096 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "LoopInGameThreadAfterFrames" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 31 | src/LuaMod.cpp:7116-7156 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ResetDelayedActionTimer" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 32 | src/LuaMod.cpp:7182-7226 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "SetDelayedActionTimer" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 33 | src/LuaMod.cpp:7250-7305 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "PauseDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 34 | src/LuaMod.cpp:7329-7355 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnpauseDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 35 | src/LuaMod.cpp:7379-7402 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "CancelDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 36 | src/LuaMod.cpp:7424-7444 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsValidDelayedActionHandle" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 37 | src/LuaMod.cpp:7466-7486 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsDelayedActionActive" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 38 | src/LuaMod.cpp:7508-7528 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsDelayedActionPaused" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 39 | src/LuaMod.cpp:7550-7611 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionTimeRemaining" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 40 | src/LuaMod.cpp:7633-7696 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionTimeElapsed" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 41 | src/LuaMod.cpp:7718-7754 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionRate" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 42 | src/LuaMod.cpp:7772-7791 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ClearAllDelayedActions" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 43 | src/LuaMod.cpp:8445-8448 | lock_guard m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 44 | src/LuaMod.cpp:8461-8604 | lock_guard LuaMod::m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 45 | src/LuaMod.cpp:8552-8575 | lock_guard m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 46 | src/LuaMod.cpp:8648-8797 | lock_guard LuaMod::m_thread_actions_mutex | script_hook | native/C++ | ambient helper at 8682; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 47 | src/LuaMod.cpp:9043-9137 | lock_guard LuaMod::m_thread_actions_mutex | LuaMod::on_program_start | native/C++ | ambient helper at 9077; own pcall/call_function | residual: ambient-pcall only (WARN) |
