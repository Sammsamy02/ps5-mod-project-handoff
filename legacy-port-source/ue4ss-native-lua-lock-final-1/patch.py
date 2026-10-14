from pathlib import Path
import difflib
w=Path(__file__).resolve().parent;o=(w/'original/platform_locks.c').read_text();needle='''void LuaLockFinal(lua_State *L) {
    /* Process-lifetime mutex: Lua states/coroutines can overlap. */
    (void)L;
}''';assert o.count(needle)==1;s=o.replace(needle,'''void LuaLockFinal(lua_State *L) {
    /* luai_userstateclose runs for a complete main state inside lua_close.
       lua_close acquired the lock but does not call lua_unlock afterwards.
       Balance exactly that entry; retain the process-lifetime mutex for other
       live states/coroutines and preserve any outer recursive acquisition.
       Failed partial opens and coroutine frees do not call this hook.
       As with Lua itself, callers must quiesce uses before closing a state. */
    LuaUnlock(L);
}''');(w/'src/platform_locks.c').write_text(s);(w/'source.patch').write_text(''.join(difflib.unified_diff(o.splitlines(True),s.splitlines(True),fromfile='original/platform_locks.c',tofile='src/platform_locks.c')))
