#!/usr/bin/env python3
"""Builds work/src/LuaPathShim.cpp from base/src/LuaPathShim.cpp (B6 shim diagnostics)."""
import os, sys
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.dirname(here)
s = open(os.path.join(root, 'base/src/LuaPathShim.cpp'), encoding='utf-8').read()
def sub_once(old, new, label):
    global s
    n = s.count(old)
    if n != 1:
        print('FAIL anchor %s count=%d' % (label, n)); sys.exit(1)
    s = s.replace(old, new); print('%-28s ok' % label)

# context fields
sub_once('''            int write_area_markers = 0;
            int dir_markers = 0;
        };''', '''            int write_area_markers = 0;
            int dir_markers = 0;
            // [B6] why the sandbox write path failed (diagnostics only).
            int write_fail_markers = 0;     // first failure per mod only
            std::string last_dir_why;       // outcome of the latest create_directories attempt
        };''', 'context fields')

# dir line says why
sub_once('''            const bool usable = !ec; // created==false + !ec == already existed (deploy pre-create)
            if (context.dir_markers < kMaxMarkers)
            {
                ++context.dir_markers;
                Output::send(STR("[SHIM] write-area dir {} created={} usable={}\\n"),
                             ensure_str(key), created, usable);
            }
            return usable;''', '''            const bool usable = !ec; // created==false + !ec == already existed (deploy pre-create)
            // [B6] say WHY: the OS error text/code when refused, else created / already there.
            const std::string why = ec ? ("refused: " + ec.message() + " (code " + std::to_string(ec.value()) + ")")
                                       : (created ? std::string{"created"} : std::string{"already existed"});
            {
                std::lock_guard<std::mutex> guard{context.mutex};
                context.last_dir_why = why;
            }
            if (context.dir_markers < kMaxMarkers)
            {
                ++context.dir_markers;
                Output::send(STR("[SHIM] write-area dir {} created={} usable={} why={}\\n"),
                             ensure_str(key), created, usable, ensure_str(why));
            }
            return usable;''', 'dir why')

# try_original records the backend failure
sub_once('''        auto try_original(lua_State* L, int argc, const std::string& physical) -> int
        {
            lua_settop(L, argc);''', '''        // [B6-SHIMFAIL-BEGIN]
        // [B6] The backend's last failure on this thread: errno (io.open returns
        // nil, message, errno) and its message. Diagnostics only.
        thread_local int tl_backend_errno = 0;
        thread_local char tl_backend_text[256] = {0};
        auto note_backend_failure(lua_State* L, int base, bool raised) -> void
        {
            tl_backend_errno = 0;
            tl_backend_text[0] = '\\0';
            const char* text = nullptr;
            if (raised)
            {
                text = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : nullptr;
            }
            else
            {
                const int top = lua_gettop(L);
                if (top >= base + 2 && lua_type(L, base + 2) == LUA_TSTRING) { text = lua_tostring(L, base + 2); }
                if (top >= base + 3 && lua_isinteger(L, base + 3)) { tl_backend_errno = static_cast<int>(lua_tointeger(L, base + 3)); }
            }
            if (text != nullptr)
            {
                // Long messages ("<path>: <strerror>") keep their TAIL, where the OS text is.
                const size_t len = std::strlen(text);
                if (len < sizeof(tl_backend_text)) { std::memcpy(tl_backend_text, text, len + 1); }
                else
                {
                    const size_t keep = sizeof(tl_backend_text) - 4; // "..." + tail + NUL
                    std::memcpy(tl_backend_text, "...", 3);
                    std::memcpy(tl_backend_text + 3, text + (len - keep), keep);
                    tl_backend_text[sizeof(tl_backend_text) - 1] = '\\0';
                }
            }
        }

        auto try_original(lua_State* L, int argc, const std::string& physical) -> int
        {
            lua_settop(L, argc);''', 'note helper')
sub_once('''            if (lua_pcall(L, argc, LUA_MULTRET, 0) != LUA_OK)
            {
                lua_settop(L, base); // drop the error; the caller tries the next location
                return 0;
            }
            const int results = lua_gettop(L) - base;
            if (results >= 1 && !lua_isnil(L, base + 1))
            {
                return results;
            }
            lua_settop(L, base);
            return 0;
        }

        auto io_open_wrapper''', '''            if (lua_pcall(L, argc, LUA_MULTRET, 0) != LUA_OK)
            {
                note_backend_failure(L, base, true); // [B6]
                lua_settop(L, base); // drop the error; the caller tries the next location
                return 0;
            }
            const int results = lua_gettop(L) - base;
            if (results >= 1 && !lua_isnil(L, base + 1))
            {
                return results;
            }
            note_backend_failure(L, base, false); // [B6]
            lua_settop(L, base);
            return 0;
        }
        // [B6-SHIMFAIL-END]

        auto io_open_wrapper''', 'try_original notes')

# write branch: remember attempt errnos, log once at the spill fallback
sub_once('''                if (const int results = try_original(L, argc, target); results > 0)
                {
                    return results;
                }
                if (ensure_parent_dirs_once(*context, target))
                {
                    if (const int results = try_original(L, argc, target); results > 0)
                    {
                        return results;
                    }
                }
                return push_memfile(L, context, target, mode);''', '''                if (const int results = try_original(L, argc, target); results > 0)
                {
                    return results;
                }
                const int first_errno = tl_backend_errno; // [B6]
                std::string first_text{tl_backend_text};
                int retry_errno = -1; // -1 = no retry (directory step refused or already attempted)
                if (ensure_parent_dirs_once(*context, target))
                {
                    if (const int results = try_original(L, argc, target); results > 0)
                    {
                        return results;
                    }
                    retry_errno = tl_backend_errno;
                }
                // [B6] First failed sandbox write per mod: say what the OS said. The write
                // still falls back to the in-memory spill below, exactly as before.
                bool log_write_fail = false;
                std::string dir_why;
                {
                    std::lock_guard<std::mutex> guard{context->mutex};
                    if (context->write_fail_markers < 1)
                    {
                        context->write_fail_markers = 1;
                        log_write_fail = true;
                        dir_why = context->last_dir_why.empty() ? std::string{"not attempted"} : context->last_dir_why;
                    }
                }
                if (log_write_fail)
                {
                    Output::send(STR("[SHIM] write-fail mod={} path={} errno={} strerror={} backend={} dir={} retry_errno={} -> spill\\n"),
                                 ensure_str(context->mod_name), ensure_str(target), first_errno,
                                 ensure_str(first_errno != 0 ? std::strerror(first_errno) : "no errno from backend"),
                                 ensure_str(first_text), ensure_str(dir_why), retry_errno);
                }
                return push_memfile(L, context, target, mode);''', 'write-fail')
open(os.path.join(root, 'work/src/LuaPathShim.cpp'), 'w', encoding='utf-8').write(s)
print('apply_shim: OK')
