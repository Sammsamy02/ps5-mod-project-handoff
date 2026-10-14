from pathlib import Path
import json,hashlib
H=Path(__file__).resolve().parent;R=H.parents[2]
mods=R/'outputs/2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp'
prog=R/'outputs/2026-10-07-l19-local-profile-r4/units/UE4SSProgram.cpp'
s=mods.read_text();s='#if defined(__PROSPERO__)\n#include "L23GameThreadProbe.hpp"\n#endif\n'+s
needle='namespace RC\n{';assert s.count(needle)==1
insert=r'''
#if defined(__PROSPERO__)
    namespace L23GameThreadProbe {
        extern "C" int32_t scePthreadGetthreadid(void);
        static State state;
        static uint64_t now_ms() noexcept {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }
        static int callback(lua_State*) noexcept {
            try {
                const int32_t tid=scePthreadGetthreadid();
                const bool genuine=Unreal::IsInGameThreadRaw();
                const auto result=state.ran(now_ms(),tid,genuine);
                L22Diagnostics::observe([&] {
                    if(result.accepted) Output::send(STR("[l23] game-thread self-test ran thread={} drain={}ms late={}\n"),result.thread,result.elapsed,result.late);
                    else Output::send(STR("[l23] game-thread self-test callback refused thread={} genuine={} state={}\n"),tid,genuine,static_cast<int>(state.status()));
                });
            } catch(...) { /* A diagnostic must not throw across the Lua C ABI. */ }
            return 0;
        }
        static int request(lua_State* L) {
            // Run all potentially raising Lua operations inside the caller's
            // pcall. No C++ owner lives on this Lua-longjmp frame.
            lua_getglobal(L,"ExecuteInGameThread");
            if(!lua_isfunction(L,-1)) return luaL_error(L,"ExecuteInGameThread is unavailable");
            lua_pushcfunction(L,callback);
            lua_call(L,1,0);
            return 0;
        }
        static void Queue(const void* owner,lua_State* L) noexcept {
            try {
                // Same lock as the production dispatcher: no concurrent drain
                // can race the queued marker or the protected main-state call.
                std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
                if(!state.begin(owner,now_ms(),scePthreadGetthreadid())) return;
                const int top=lua_gettop(L);
                lua_pushcfunction(L,request);
                const int result=lua_pcall(L,0,0,0);
                if(result==LUA_OK) {
                    state.queued();
                    L22Diagnostics::observe([&]{Output::send(STR("[l23] game-thread self-test queued via ExecuteInGameThread deadline=30000ms\n"));});
                } else {
                    state.failed();
                    L22Diagnostics::observe([&]{const char* msg=lua_tostring(L,-1);Output::send(STR("[l23] game-thread self-test enqueue failed: {}\n"),ensure_str(msg?msg:"non-string Lua error"));});
                }
                lua_settop(L,top);
            } catch(...) { state.failed(); }
        }
        static void Cancel(const void* owner) noexcept {
            try { if(state.cancel(owner)) L22Diagnostics::observe([]{Output::send(STR("[l23] game-thread self-test cancelled before Lua owner teardown\n"));}); }
            catch(...) {}
        }
        void Poll() noexcept {
            try { if(state.timeout(now_ms())) L22Diagnostics::observe([]{Output::send(STR("[l23] game-thread self-test NOT DRAINED within 30000ms; startup continues; callback remains owned by the normal queue\n"));}); }
            catch(...) {}
        }
    }
#endif
'''
s=s.replace(needle,needle+insert,1)
needle='Output::send(STR("[l22] Lua mod started: {} (main script returned)\\n"), m_mod_name);\n                    });';assert s.count(needle)==1
s=s.replace(needle,needle+'\n#if defined(__PROSPERO__)\n                    L23GameThreadProbe::Queue(this,main_lua()->get_lua_state());\n#endif',1)
needle='        fire_on_lua_stop_for_cpp_mods();';assert s.count(needle)==1
s=s.replace(needle,'#if defined(__PROSPERO__)\n        L23GameThreadProbe::Cancel(this);\n#endif\n'+needle,1)
(H/'src/LuaMod.cpp').write_text(s)
s=prog.read_text();s='#if defined(__PROSPERO__)\n#include "L23GameThreadProbe.hpp"\n#include "NativePropertyPointerGuard.hpp"\n#endif\n'+s
needle='            if (m_pause_events_processing)';assert s.count(needle)==1
s=s.replace(needle,'#if defined(__PROSPERO__)\n            L23GameThreadProbe::Poll();\n#endif\n'+needle,1)
needle='        if (s_program == this)\n        {\n            try { uninstall_mods(); }';assert s.count(needle)==1
# Revoke after joined program update, before any engine-facing cleanup.
s=s.replace('        if (m_native_owns_output_devices)\n', '        Unreal::NativePropertyGuard::Revoke();\n        if (m_native_owns_output_devices)\n', 1)
(H/'src/UE4SSProgram.cpp').write_text(s)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
rows=[{'original':str(p),'candidate':str(H/'src'/p.name),'original_sha256':sha(p),'candidate_sha256':sha(H/'src'/p.name)} for p in (mods,prog)]
(H/'CHANGES.json').write_text(json.dumps({'files':rows,'added_headers':[str(H/'include/L23GameThreadProbe.hpp')],'include_directories':[str(H/'include'),str(H.parent/'typescan-r2/include')]},indent=2)+'\n')
print('2 unit overlays, 1 state header; no mod edits')
