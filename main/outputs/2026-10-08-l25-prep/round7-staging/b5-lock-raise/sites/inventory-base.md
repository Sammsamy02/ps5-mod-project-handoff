| # | file:lines | lock (mutex) | context | entry | Lua raise inside | verdict |
|---|---|---|---|---|---|---|
| 1 | include/DynamicOutput/FileDevice.hpp:93-103 | lock_guard m_write_mutex | receive | native/C++ | - | none: no Lua call/raise in scope |
| 2 | include/Helpers/String.hpp:667-669 | shared_lock uestringpool_lock | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 3 | include/Helpers/String.hpp:676-679 | lock_guard uestringpool_lock | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 4 | src/ClassListener.cpp:191-193 | lock_guard ObjectSearcherPool<UClass, AnySuperStruct>::PoolMutex | FClassDeleteListener::NotifyUObjectDeleted | native/C++ | - | none: no Lua call/raise in scope |
| 5 | src/ClassListener.cpp:197-199 | lock_guard ObjectSearcherPool<UClass, AActor>::PoolMutex | FClassDeleteListener::NotifyUObjectDeleted | native/C++ | - | none: no Lua call/raise in scope |
| 6 | src/ClassListener.cpp:203-205 | lock_guard ObjectSearcherPool<AActor, AnySuperStruct>::PoolMutex | FClassDeleteListener::NotifyUObjectDeleted | native/C++ | - | none: no Lua call/raise in scope |
| 7 | src/ClassListener.cpp:209-211 | lock_guard ObjectSearcherPool<AnyInstance, AnyInstance>::PoolMutex | FClassDeleteListener::NotifyUObjectDeleted | native/C++ | - | none: no Lua call/raise in scope |
| 8 | src/LuaMadeSimple/LuaMadeSimple.cpp:281-291 | lock_guard g_arena_state.mutex | arena_alloc_small | native/C++ | - | none: no Lua call/raise in scope |
| 9 | src/LuaMadeSimple/LuaMadeSimple.cpp:315-317 | lock_guard g_arena_state.mutex | arena_free_user | native/C++ | - | none: no Lua call/raise in scope |
| 10 | src/LuaMadeSimple/LuaMadeSimple.cpp:476-478 | lock_guard lua_error_callbacks_mutex | register_error_callback | native/C++ | - | none: no Lua call/raise in scope |
| 11 | src/LuaMadeSimple/LuaMadeSimple.cpp:482-486 | lock_guard lua_error_callbacks_mutex | unregister_error_callback | native/C++ | - | none: no Lua call/raise in scope |
| 12 | src/LuaMadeSimple/LuaMadeSimple.cpp:490-498 | lock_guard lua_error_callbacks_mutex | notify_error_callbacks | native/C++ | - | none: no Lua call/raise in scope |
| 13 | src/LuaMod.cpp:265-278 | lock_guard LuaMod::m_thread_actions_mutex | Queue | native/C++ | own pcall/call_function | safe: errors stay inside own pcall / C++ catch |
| 14 | src/LuaMod.cpp:2231-2241 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterKeyBind" | TRY-wrapped (process_lua_function) | ambient helper at 2234; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 15 | src/LuaMod.cpp:2346-2439 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnregisterHook" | TRY-wrapped (process_lua_function) | direct raise at 2355,2366,2373,2379,2385,2392,2435 | SITE: needs C++ throw |
| 16 | src/LuaMod.cpp:3063-3103 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterCustomEvent" | TRY-wrapped (process_lua_function) | direct raise at 3071,3081; get_mod_ref at 3084; Lua alloc API (ERRMEM only) | SITE: needs C++ throw |
| 17 | src/LuaMod.cpp:3106-3122 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnregisterCustomEvent" | TRY-wrapped (process_lua_function) | direct raise at 3114 | SITE: needs C++ throw |
| 18 | src/LuaMod.cpp:4590-4612 | unique_lock LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 19 | src/LuaMod.cpp:4700-4707 | lock_guard LuaMod::m_thread_actions_mutex | process_simple_actions | native/C++ | - | none: no Lua call/raise in scope |
| 20 | src/LuaMod.cpp:4737-4742 | lock_guard LuaMod::m_thread_actions_mutex | process_simple_actions | native/C++ | - | none: no Lua call/raise in scope |
| 21 | src/LuaMod.cpp:4761-4860 | lock_guard LuaMod::m_thread_actions_mutex | process_pending_notify_on_new_object_callbacks | native/C++ | ambient helper at 4824,4825; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 22 | src/LuaMod.cpp:4875-4940 | lock_guard LuaMod::m_thread_actions_mutex | process_delayed_actions | native/C++ | - | none: no Lua call/raise in scope |
| 23 | src/LuaMod.cpp:4958-5015 | lock_guard LuaMod::m_thread_actions_mutex | process_delayed_actions | native/C++ | - | none: no Lua call/raise in scope |
| 24 | src/LuaMod.cpp:5037-5039 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 25 | src/LuaMod.cpp:5047-5065 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 26 | src/LuaMod.cpp:5490-5492 | lock_guard LuaMod::m_thread_actions_mutex | funcptr_marshal_hook | native/C++ | - | none: no Lua call/raise in scope |
| 27 | src/LuaMod.cpp:5500-5518 | lock_guard LuaMod::m_thread_actions_mutex | funcptr_marshal_hook | native/C++ | - | none: no Lua call/raise in scope |
| 28 | src/LuaMod.cpp:5564-5580 | lock_guard LuaMod::m_thread_actions_mutex | VmtEngineTickDrain | native/C++ | - | none: no Lua call/raise in scope |
| 29 | src/LuaMod.cpp:5596-5614 | lock_guard LuaMod::m_thread_actions_mutex | VmtEngineTickDrain | native/C++ | - | none: no Lua call/raise in scope |
| 30 | src/LuaMod.cpp:5642-5644 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 31 | src/LuaMod.cpp:5652-5670 | lock_guard LuaMod::m_thread_actions_mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 32 | src/LuaMod.cpp:5768-5821 | lock_guard m_funcptr_marshal_carrier_mutex | r3_marshal_arm_event_carriers | native/C++ | - | none: no Lua call/raise in scope |
| 33 | src/LuaMod.cpp:5901-5914 | lock_guard m_thread_actions_mutex | LuaMod::ensure_engine_tick_hooked | native/C++ | - | none: no Lua call/raise in scope |
| 34 | src/LuaMod.cpp:5920-5942 | lock_guard m_thread_actions_mutex | LuaMod::ensure_process_event_hooked | native/C++ | - | none: no Lua call/raise in scope |
| 35 | src/LuaMod.cpp:5947-6050 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RegisterHook" | TRY-wrapped (process_lua_function) | direct raise at 5956,5966,5994,6043; get_mod_ref at 5969; Lua alloc API (ERRMEM only) | SITE: needs C++ throw |
| 36 | src/LuaMod.cpp:6216-6349 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThread" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 37 | src/LuaMod.cpp:6493-6540 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 38 | src/LuaMod.cpp:6547-6576 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 39 | src/LuaMod.cpp:6699-6755 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "RetriggerableExecuteInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 40 | src/LuaMod.cpp:6800-6831 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ExecuteInGameThreadAfterFrames" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 41 | src/LuaMod.cpp:6952-6982 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "LoopInGameThreadWithDelay" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 42 | src/LuaMod.cpp:7027-7060 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "LoopInGameThreadAfterFrames" | TRY-wrapped (process_lua_function) | Lua alloc API (ERRMEM only) | residual: allocation failure only |
| 43 | src/LuaMod.cpp:7080-7120 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ResetDelayedActionTimer" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 44 | src/LuaMod.cpp:7146-7190 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "SetDelayedActionTimer" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 45 | src/LuaMod.cpp:7214-7269 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "PauseDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 46 | src/LuaMod.cpp:7293-7319 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "UnpauseDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 47 | src/LuaMod.cpp:7343-7366 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "CancelDelayedAction" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 48 | src/LuaMod.cpp:7388-7408 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsValidDelayedActionHandle" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 49 | src/LuaMod.cpp:7430-7450 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsDelayedActionActive" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 50 | src/LuaMod.cpp:7472-7492 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "IsDelayedActionPaused" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 51 | src/LuaMod.cpp:7514-7575 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionTimeRemaining" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 52 | src/LuaMod.cpp:7597-7660 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionTimeElapsed" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 53 | src/LuaMod.cpp:7682-7718 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "GetDelayedActionRate" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 54 | src/LuaMod.cpp:7736-7755 | lock_guard LuaMod::m_thread_actions_mutex | registered Lua fn "ClearAllDelayedActions" | TRY-wrapped (process_lua_function) | - | none: no Lua call/raise in scope |
| 55 | src/LuaMod.cpp:8409-8412 | lock_guard m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 56 | src/LuaMod.cpp:8425-8568 | lock_guard LuaMod::m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 57 | src/LuaMod.cpp:8516-8539 | lock_guard m_thread_actions_mutex | LuaMod::uninstall | native/C++ | - | none: no Lua call/raise in scope |
| 58 | src/LuaMod.cpp:8612-8761 | lock_guard LuaMod::m_thread_actions_mutex | script_hook | native/C++ | direct raise at 8695; ambient helper at 8643,8646; own pcall/call_function | SITE (native): needs C++ throw, local catch |
| 59 | src/LuaMod.cpp:9007-9101 | lock_guard LuaMod::m_thread_actions_mutex | LuaMod::on_program_start | native/C++ | ambient helper at 9040,9041; own pcall/call_function | residual: ambient-pcall only (WARN) |
| 60 | src/LuaPathShim.cpp:334-339 | lock_guard context.mutex | case_resolve | native/C++ | - | none: no Lua call/raise in scope |
| 61 | src/LuaPathShim.cpp:396-402 | lock_guard context.mutex | case_resolve | native/C++ | - | none: no Lua call/raise in scope |
| 62 | src/LuaPathShim.cpp:530-543 | lock_guard context.mutex | spill_store | native/C++ | - | none: no Lua call/raise in scope |
| 63 | src/LuaPathShim.cpp:547-563 | lock_guard context.mutex | spill_fetch | native/C++ | - | none: no Lua call/raise in scope |
| 64 | src/LuaPathShim.cpp:567-582 | lock_guard context.mutex | spill_erase | native/C++ | - | none: no Lua call/raise in scope |
| 65 | src/LuaPathShim.cpp:586-608 | lock_guard context.mutex | spill_move | native/C++ | - | none: no Lua call/raise in scope |
| 66 | src/LuaPathShim.cpp:631-637 | lock_guard context.mutex | ensure_parent_dirs_once | native/C++ | - | none: no Lua call/raise in scope |
| 67 | src/LuaUObject.cpp:55-57 | lock_guard s_lua_unreal_objects_map_mutex | add_to_global_unreal_objects_map | native/C++ | - | none: no Lua call/raise in scope |
| 68 | src/LuaUObject.cpp:64-69 | lock_guard s_lua_unreal_objects_map_mutex | remove_from_global_unreal_objects_map | native/C++ | - | none: no Lua call/raise in scope |
| 69 | src/LuaUObject.cpp:74-76 | lock_guard s_lua_unreal_objects_map_mutex | is_object_in_global_unreal_object_map | native/C++ | - | none: no Lua call/raise in scope |
| 70 | src/ScriptHookThunks.cpp:40-45 | lock_guard mutex | (file scope / class member) | native/C++ | - | none: no Lua call/raise in scope |
| 71 | src/ScriptHookThunks.cpp:54-66 | lock_guard mutex | Dispatch | native/C++ | - | none: no Lua call/raise in scope |
| 72 | src/ScriptHookThunks.cpp:103-107 | lock_guard mutex | ConfigureCapacity | native/C++ | - | none: no Lua call/raise in scope |
| 73 | src/ScriptHookThunks.cpp:110-115 | lock_guard mutex | Lookup | native/C++ | - | none: no Lua call/raise in scope |
| 74 | src/ScriptHookThunks.cpp:118-122 | lock_guard mutex | FindCallback | native/C++ | - | none: no Lua call/raise in scope |
| 75 | src/ScriptHookThunks.cpp:125-163 | lock_guard mutex | Register | native/C++ | - | none: no Lua call/raise in scope |
| 76 | src/ScriptHookThunks.cpp:166-173 | lock_guard mutex | Remove | native/C++ | - | none: no Lua call/raise in scope |
| 77 | src/ScriptHookThunks.cpp:177-187 | lock_guard mutex | BeginTeardown | native/C++ | - | none: no Lua call/raise in scope |
| 78 | src/ScriptHookThunks.cpp:190-205 | lock_guard mutex | Retire | native/C++ | - | none: no Lua call/raise in scope |
| 79 | src/UE4SSProgram.cpp:546-599 | lock_guard m_native_startup_mutex | UE4SSProgram::shutdown_native_program | native/C++ | - | none: no Lua call/raise in scope |
| 80 | src/UE4SSProgram.cpp:563-565 | lock_guard m_event_queue_mutex | UE4SSProgram::shutdown_native_program | native/C++ | - | none: no Lua call/raise in scope |
| 81 | src/UE4SSProgram.cpp:610-651 | lock_guard m_native_startup_mutex | UE4SSProgram::start_native_mods | native/C++ | - | none: no Lua call/raise in scope |
| 82 | src/UE4SSProgram.cpp:668-672 | lock_guard m_native_startup_mutex | UE4SSProgram::init | native/C++ | - | none: no Lua call/raise in scope |
| 83 | src/UE4SSProgram.cpp:716-720 | lock_guard m_engine_ready_callback_mutex | UE4SSProgram::init | native/C++ | - | none: no Lua call/raise in scope |
| 84 | src/UE4SSProgram.cpp:1460-1481 | lock_guard UE4SSProgram::get_program().m_render_thread_mutex | gui_render_thread_tick | native/C++ | - | none: no Lua call/raise in scope |
| 85 | src/UE4SSProgram.cpp:1518-1543 | lock_guard m_render_thread_mutex | UE4SSProgram::on_program_start | native/C++ | - | none: no Lua call/raise in scope |
| 86 | src/UE4SSProgram.cpp:1814-1819 | lock_guard m_event_queue_mutex | UE4SSProgram::update | native/C++ | - | none: no Lua call/raise in scope |
| 87 | src/UE4SSProgram.cpp:1827-1841 | lock_guard m_event_queue_mutex | UE4SSProgram::update | native/C++ | - | none: no Lua call/raise in scope |
| 88 | src/UE4SSProgram.cpp:3092-3098 | lock_guard m_event_queue_mutex | UE4SSProgram::queue_event | native/C++ | - | none: no Lua call/raise in scope |
| 89 | src/UE4SSProgram.cpp:3108-3113 | lock_guard m_event_queue_mutex | UE4SSProgram::is_queue_empty | native/C++ | - | none: no Lua call/raise in scope |
| 90 | src/UObjectArray.cpp:781-956 | lock_guard GUObjectDeleteListenerWriteMutex | UObjectArray::AddUObjectDeleteListener | native/C++ | - | none: no Lua call/raise in scope |
| 91 | src/UObjectArray.cpp:963-1029 | lock_guard GUObjectDeleteListenerWriteMutex | UObjectArray::RemoveUObjectDeleteListener | native/C++ | - | none: no Lua call/raise in scope |
| 92 | src/UnrealInitializer.cpp:578-586 | lock_guard ObjectSearcherPool<UClass, AnySuperStruct>::PoolMutex | UKismetStringLibrary_Conv_NameToString | native/C++ | - | none: no Lua call/raise in scope |
| 93 | src/UnrealInitializer.cpp:583-585 | lock_guard ObjectSearcherPool<UClass, AActor>::PoolMutex | UKismetStringLibrary_Conv_NameToString | native/C++ | - | none: no Lua call/raise in scope |
| 94 | src/UnrealInitializer.cpp:590-592 | lock_guard ObjectSearcherPool<AActor, AnySuperStruct>::PoolMutex | UKismetStringLibrary_Conv_NameToString | native/C++ | - | none: no Lua call/raise in scope |
| 95 | src/UnrealInitializer.cpp:596-598 | lock_guard ObjectSearcherPool<AnyInstance, AnyInstance>::PoolMutex | UKismetStringLibrary_Conv_NameToString | native/C++ | - | none: no Lua call/raise in scope |
| 96 | src/UnrealInitializer.cpp:665-667 | lock_guard ObjectSearcherPool<AActor, AnySuperStruct>::PoolMutex | UKismetStringLibrary_Conv_NameToString | native/C++ | - | none: no Lua call/raise in scope |
| 97 | src/UnrealInitializer.cpp:671-673 | lock_guard ObjectSearcherPool<AnyInstance, AnyInstance>::PoolMutex | UKismetStringLibrary_Conv_NameToString | native/C++ | - | none: no Lua call/raise in scope |
