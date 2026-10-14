#!/usr/bin/env python3
"""apply_b7.py -- B7 runtime edits (Lua compiled as C++: Lua errors become C++ throws).

Applies the B7 runtime-source edits to a candidate tree ROOT (the directory holding src/ and include/).
  usage: python3 -I apply_b7.py [ROOT]
  default ROOT: outputs/2026-10-08-l26-runtime/typescan-r4/candidate (the real candidate tree)

Every edit is an exact string replacement whose anchor must occur EXACTLY ONCE in its file, else the
script exits 1 naming the anchor and writes NOTHING (all files are edited in memory first). Applying
twice fails (the [B7-PROBE-BEGIN] marker is checked first). Zero edits = failure. No line numbers, no
sha pins, so it still applies after the other round-10 tracks (B7 is merge-friendly by design).

Edits (rebased 2026-10-12 onto round10 base-tree; classes as in round8-prep CATCH-AUDIT.md):
  REQUIRED    E1  LuaUObject.cpp protected_ufunction_call   catch (void*) { throw; } before catch (...)
              E2  LuaMadeSimple.cpp throw_error_impl         same
  DEFENSIVE   E3  LuaMadeSimple.cpp call_function mem-note   same
              E4  LuaMod.cpp native_lua_file                 same
              E5  LuaMod.cpp custom_module_searcher          same
              E6  LuaPathShim.cpp install                    same
              E11 LuaMod.cpp mod_name_of (NEW since round 8: the R9 hook-report helper runs inside Lua C
                  functions such as RegisterHook, i.e. under the mod's pcall; swallowing a Lua throw there
                  would leave the outer pcall's error status set with no error delivered)
  LOG-PARITY  E7a/E7b LuaFText.cpp, E8 RegisterKeyBind, E9 StaticConstructObject, E10 ExecuteInGameThread
              (handlers already rethrow; without the pass-through each Lua error would also log
              "non-standard C++ exception")
  PROBE       P1  LuaMadeSimple.cpp lua_cpp_probe() definition
              P2  LuaMod.cpp on_program_start: one "[LUA-CPP] mode=..." startup line
  BOUNDARY    H1-H3 ScriptHookThunks.cpp: catch (...) where our code hands control back to the game
              (native-UFunction hook thunks are live; game frames below cannot be unwound on the PS5)
No new #include. New library calls: none (the probe allocator uses realloc/free, which
LuaMadeSimple.cpp already calls in lms_counting_alloc).
"""
import hashlib, sys
from pathlib import Path
sys.dont_write_bytecode = True

DEFAULT_ROOT = Path(__file__).resolve().parents[4] / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate'

PASS = '''{i}catch (void*)
{i}{{
{i}    // [B7] With Lua built as C++, a Lua error is a C++ throw of a pointer. It must reach
{i}    // lua_pcall's own handler (rethrow); swallowing it here leaves Lua's lock held.
{i}    throw;
{i}}}
'''

PROBE_CODE = r'''    // [B7-PROBE-BEGIN]
    // Startup proof of how Lua errors really travel in THIS binary (generic; no mod/game values).
    // It runs a tiny pcall, a tiny coroutine yield and a catch(void*) pass-through check on a
    // throw-away Lua state and counts whether C++ destructors / handlers in the raising C functions'
    // frames ran. Lua as C (longjmp) never runs them; Lua as C++ (throw) always does. The raise and
    // the yield happen inside lua_pcall / lua_resume (protected); lua_newstate returns NULL on
    // failure; the pushes outside a protected call do not allocate, except lua_newthread (a failure
    // there at startup would abort, like any top-level Lua out-of-memory today).
    // out[0]: 1 = throw, 0 = longjmp, -1 = mixed or probe failed; out[1] pcall status (2 expected);
    // out[2] error-frame destructors; out[3] resume status (1 = LUA_YIELD expected); out[4]
    // yield-frame destructors; out[5] 1 = ran; out[6] a catch (void*) handler matched the Lua throw
    // (this is what the E1/E2 pass-through edits rely on); out[7] the catch (...) after it swallowed
    // instead (must be 0); out[8] status of the pcall that carried the void* check (2 expected).
    namespace
    {
        struct LuaCppProbeSentinel
        {
            int* counter;
            explicit LuaCppProbeSentinel(int* c) noexcept : counter(c) {}
            LuaCppProbeSentinel(const LuaCppProbeSentinel&) = delete;
            ~LuaCppProbeSentinel() { ++*counter; }
        };

        static void* lua_cpp_probe_alloc(void*, void* ptr, size_t, size_t nsize)
        {
            // Same allocator ABI as l_alloc; realloc/free are already used by lms_counting_alloc.
            if (nsize == 0)
            {
                free(ptr);
                return nullptr;
            }
            return realloc(ptr, nsize);
        }

        static int lua_cpp_probe_raise(lua_State* L)
        {
            LuaCppProbeSentinel sentinel{static_cast<int*>(lua_touserdata(L, 1))};
            lua_pushstring(L, "b7-probe");
            return lua_error(L);
        }

        static int lua_cpp_probe_yield(lua_State* L)
        {
            LuaCppProbeSentinel sentinel{static_cast<int*>(lua_touserdata(L, 1))};
            return lua_yield(L, 0);
        }

        static int lua_cpp_probe_voidcatch(lua_State* L)
        {
            int* flags = static_cast<int*>(lua_touserdata(L, 1));
            try
            {
                lua_pushstring(L, "b7-probe-void");
                lua_error(L);
            }
            catch (void*)
            {
                ++flags[0];
                throw;
            }
            catch (...)
            {
                ++flags[1];
            }
            return 0;
        }
    } // namespace

    auto lua_cpp_probe(int* out) noexcept -> void
    {
        for (int i = 0; i < 9; ++i) { out[i] = 0; }
        out[0] = -1;
        lua_State* L = lua_newstate(lua_cpp_probe_alloc, nullptr);
        if (L == nullptr) { return; }
        int err_dtor = 0;
        int yield_dtor = 0;
        int void_flags[2]{0, 0};
        lua_pushcfunction(L, lua_cpp_probe_raise);
        lua_pushlightuserdata(L, &err_dtor);
        out[1] = lua_pcall(L, 1, 0, 0);
        lua_settop(L, 0);
        lua_pushcfunction(L, lua_cpp_probe_voidcatch);
        lua_pushlightuserdata(L, void_flags);
        out[8] = lua_pcall(L, 1, 0, 0);
        lua_settop(L, 0);
        lua_State* co = lua_newthread(L);
        lua_pushcfunction(co, lua_cpp_probe_yield);
        lua_pushlightuserdata(co, &yield_dtor);
        int results = 0;
        out[3] = lua_resume(co, L, 1, &results);
        lua_close(L);
        out[2] = err_dtor;
        out[4] = yield_dtor;
        out[5] = 1;
        out[6] = void_flags[0];
        out[7] = void_flags[1];
        if (out[1] == LUA_ERRRUN && out[3] == LUA_YIELD && out[8] == LUA_ERRRUN)
        {
            if (err_dtor == 1 && yield_dtor == 1 && void_flags[0] == 1 && void_flags[1] == 0) { out[0] = 1; }
            else if (err_dtor == 0 && yield_dtor == 0 && void_flags[0] == 0 && void_flags[1] == 0) { out[0] = 0; }
        }
    }
    // [B7-PROBE-END]

'''

PROBE_DECL = '''    namespace LuaMadeSimple
    {
        // [B7] defined in LuaMadeSimple.cpp; declared here, not in the header, so no consumer is recompiled for it.
        auto lua_cpp_probe(int* out) noexcept -> void;
    }

'''

PROBE_CALL = '''        // [B7] one startup line that proves how Lua errors travel in this binary.
        {
            int b7_probe[9]{};
            LuaMadeSimple::lua_cpp_probe(b7_probe);
            if (b7_probe[0] == 1)
            {
                Output::send(STR("[LUA-CPP] mode=cxx-throw err_status={} err_dtor={} yield_status={} yield_dtor={} void_catch={} swallowed={}\\n"), b7_probe[1], b7_probe[2], b7_probe[3], b7_probe[4], b7_probe[6], b7_probe[7]);
            }
            else if (b7_probe[0] == 0)
            {
                Output::send(STR("[LUA-CPP] mode=longjmp err_status={} err_dtor={} yield_status={} yield_dtor={} void_catch={} swallowed={}\\n"), b7_probe[1], b7_probe[2], b7_probe[3], b7_probe[4], b7_probe[6], b7_probe[7]);
            }
            else
            {
                Output::send(STR("[LUA-CPP] mode=UNKNOWN ran={} err_status={} err_dtor={} yield_status={} yield_dtor={} void_catch={} swallowed={} void_status={}\\n"), b7_probe[5], b7_probe[1], b7_probe[2], b7_probe[3], b7_probe[4], b7_probe[6], b7_probe[7], b7_probe[8]);
            }
        }
'''

H4_NOTE = ('    // [B7-H4] Game boundary. Game frames are below this function and the PS5 unwinder cannot walk them, so\n'
           '    // NOTHING may leave here. A Lua error that escaped the callback (Lua as C++: a thrown pointer, not a\n'
           '    // std::exception) ends up here instead of in std::terminate. It is logged, not delivered: if the main Lua\n'
           '    // state was inside a protected call when the game fired this hook, that call is not unwound by it.\n')


def h4_catch(what):
    # the log call is itself wrapped (same pattern as the R9-GUARD handlers): a failing log must not escape either.
    return f'    catch (...) {{ try {{ Output::send(STR("[B7-H4] contained a non-std exception (a Lua error?) at the game boundary: {what}\\n")); }} catch (...) {{}} }}\n'


def before(anchor, indent):
    """insert a catch (void*) pass-through block directly before the anchor (which starts with the catch (...) line)."""
    return (anchor, PASS.format(i=indent) + anchor)


def after(anchor, text):
    return (anchor, anchor + text)


LMS = 'src/LuaMadeSimple/LuaMadeSimple.cpp'
MOD = 'src/LuaMod.cpp'
EDITS = [
    ('E1', 'src/LuaUObject.cpp', *before('        catch (...)\n        {\n            // Keep C++ exceptions from crossing the C Lua protected frame. The\n', ' ' * 8)),
    ('E2', LMS, *before('        catch (...)\n        {\n            // [B2] Formatting the error failed (typically std::bad_alloc under\n', ' ' * 8)),
    ('E3', LMS, *before('                catch (...)\n                {\n                    mem_note = " [lua-mem: note-format-failed]";\n', ' ' * 16)),
    ('P1', LMS, '    auto new_state() -> Lua&\n    {\n', PROBE_CODE + '    auto new_state() -> Lua&\n    {\n'),
    ('E4', MOD, *before('                catch (...)\n                {\n                    std::snprintf(error, sizeof(error), "cannot open native Lua file: source lookup failed");\n', ' ' * 16)),
    ('E5', MOD, *before('            catch (...)\n            {\n                native_lua_push_text(L, "Native Lua module loading failed");\n', ' ' * 12)),
    ('E8', MOD, *before('        catch (...)\n        {\n            L22Diagnostics::error("RegisterKeyBind", "non-standard C++ exception");\n', ' ' * 8)),
    ('E9', MOD, *before('        catch (...)\n        {\n            L22Diagnostics::error("StaticConstructObject", "non-standard C++ exception");\n', ' ' * 8)),
    ('E10', MOD, *before('        catch (...)\n        {\n            L22Diagnostics::error("ExecuteInGameThread", "non-standard C++ exception");\n', ' ' * 8)),
    ('E11', MOD, *before('            catch (...)\n            {\n                out = StringType{STR("?")};\n            }\n', ' ' * 12)),
    ('P2', MOD, '    auto LuaMod::on_program_start() -> void\n    {\n', PROBE_DECL + '    auto LuaMod::on_program_start() -> void\n    {\n' + PROBE_CALL),
    ('E6', 'src/LuaPathShim.cpp', *before('        catch (...)\n        {\n            // Registration is best-effort: any failure leaves the stock io/os\n', ' ' * 8)),
    ('E7a', 'src/LuaFText.cpp', *after('                LuaType::FText::construct(lua, text_bridge.to_ftext());\n            }\n'
                                       '            catch (const std::exception& error)\n            {\n'
                                       '                L22Diagnostics::error("FText Lua construction", error.what());\n                throw;\n            }\n', PASS.format(i=' ' * 12))),
    ('E7b', 'src/LuaFText.cpp', *after('                LuaType::FText::construct(lua, Unreal::FText(text_string));\n            }\n'
                                       '            catch (const std::exception& error)\n            {\n'
                                       '                L22Diagnostics::error("FText Lua construction", error.what());\n                throw;\n            }\n', PASS.format(i=' ' * 12))),
    ('H1', 'src/ScriptHookThunks.cpp', *after('    catch (std::exception& e) { Output::send(STR("Error executing thunk pre callback: {}\\n"), ensure_str(e.what())); }\n', H4_NOTE + h4_catch('thunk pre callback'))),
    ('H2', 'src/ScriptHookThunks.cpp', *after('    catch (std::exception& e) { Output::send(STR("Error executing thunk post callback: {}\\n"), ensure_str(e.what())); }\n', h4_catch('thunk post callback'))),
    ('H3', 'src/ScriptHookThunks.cpp', *after('    catch (std::exception& e) { Output::send(STR("Error executing script thunk: {}\\n"), ensure_str(e.what())); }\n', h4_catch('script thunk'))),
]


def sha8(b): return hashlib.sha256(b).hexdigest()[:8]


def main(argv):
    root = Path(argv[1]) if len(argv) > 1 else DEFAULT_ROOT
    if not (root / 'src').is_dir():
        print(f'FAIL: {root}/src not found'); return 2
    files = sorted({f for _, f, _, _ in EDITS})
    text, before_sha = {}, {}
    for f in files:
        p = root / f
        if not p.is_file():
            print(f'FAIL: missing file {p}'); return 1
        data = p.read_bytes(); before_sha[f] = sha8(data); text[f] = data.decode('utf-8')
    if '[B7-PROBE-BEGIN]' in text[LMS] or '[B7-H4]' in text['src/ScriptHookThunks.cpp']:
        print('FAIL: B7 already applied to this root (marker present); refusing to apply twice'); return 1
    for eid, f, old, new in EDITS:
        n = text[f].count(old)
        if n != 1:
            first = old.strip().splitlines()[0][:90] if old.strip() else old
            print(f'FAIL {eid}: anchor in {f} found {n} times, expected 1: {first!r} ...'); return 1
        text[f] = text[f].replace(old, new, 1)
    changed = 0
    for f in files:
        data = text[f].encode('utf-8')
        (root / f).write_bytes(data)
        print(f'  {f}: {before_sha[f]} -> {sha8(data)}')
        changed += 1
    print(f'apply_b7: {len(EDITS)} edits applied to {changed} files under {root}')
    if len(EDITS) == 0 or changed == 0:
        print('FAIL: zero edits'); return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
