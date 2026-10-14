#!/usr/bin/env python3
"""Generate the STAGED B7 runtime patch (Mac-only; nothing is applied to any tree).

B7 = "Lua is compiled as C++" (errors/yields become C++ throws). This script produces the
RUNTIME source edits that must ride with it, as exact-match, anchor-based edits (no line
numbers), against either base tree:

  --base candidate   the B4 snapshot candidate tree (typescan-r4/candidate), sha-pinned
  --base b5final     candidate with LuaMod.cpp from the round-7 FINAL B5 proposal (15 sites + the
                     get_mod_ref_cxx helper fix; sites/patched/final-b5/src/LuaMod.cpp)
  --base b6          the round-7 B6 work files (b6-diagnostics/work/): they sit on the EARLIER
                     15-site B5 proposal (no get_mod_ref_cxx helper) plus B6 diagnostics. Round-7
                     staging is still moving, so the shas of the files used are printed and recorded.

Edits (class in brackets, see CATCH-AUDIT.md):
  [REQUIRED]    E1 LuaUObject.cpp protected_ufunction_call   catch(void*){throw;} before catch(...)
                E2 LuaMadeSimple.cpp throw_error_impl         same
  [DEFENSIVE]   E3 LuaMadeSimple.cpp call_function mem-note   same  (no raising Lua call in try today)
                E4 LuaMod.cpp native_lua_file                 same
                E5 LuaMod.cpp custom_module_searcher          same
                E6 LuaPathShim.cpp install                    same
  [LOG-PARITY]  E7 LuaFText.cpp x2, E8 RegisterKeyBind, E9 StaticConstructObject lambda,
                E10 ExecuteInGameThread lambda: today the Lua error skips these handlers (longjmp);
                with Lua as C++ they would log "non-standard C++ exception" for every Lua error.
  [PROBE]       P1 LuaMadeSimple.cpp lua_cpp_probe()          startup proof of the error mechanism
                P2 LuaMod.cpp on_program_start               prints the "[LUA-CPP] mode=..." line

Each edit must match EXACTLY the expected number of times (else exit 1); zero edits applied
is a failure. Output (under staged-patch/<base>/): the patched copies of the touched files,
b7-runtime.patch (unified diff, apply from the worktree root with `git apply` / `patch -p1`),
and EDITS.json (what was applied, shas before/after).
Usage: python3 -I make_b7_runtime_patch.py --base candidate|b5final|b6
"""
import difflib, hashlib, json, re, sys
from pathlib import Path
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
OUTROOT = HERE.parent / 'staged-patch'
WHAT = HERE.parents[4]
REL = 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
CAND = WHAT / REL
B6 = WHAT / 'outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work'
B5F = WHAT / 'outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/sites/patched/final-b5'
def base_path(base, f):
    if base == 'b6' and f in B6_FILES: return B6 / f
    if base == 'b5final' and f == 'src/LuaMod.cpp': return B5F / f
    return CAND / f

CAND_PINS = {  # sha256 of the B4 candidate files this patch was built against
    'src/LuaUObject.cpp': 'aa10e4eadabe2f324fce2f95487bff9c76c225dfab22e0809dfc440235e16a32',
    'src/LuaMadeSimple/LuaMadeSimple.cpp': '418c19505c888abe6fce58ab49385c5d5a6bd56e7641385f8a03b28310d0d2a5',
    'src/LuaMod.cpp': '4d6bc63b7a90a8026a9c918ee6bee36c2d4eaa450220be82e8071f5427bfde9a',
    'src/LuaPathShim.cpp': '9aef55a109f9ae2e9c6e8344d8b258c14be6c4e20f4e2e0dc3b8df3edac37110',
}
B6_FILES = {'src/LuaMod.cpp', 'src/LuaPathShim.cpp', 'src/LuaMadeSimple/LuaMadeSimple.cpp'}

PASS_BLOCK = '''{i}catch (void*)
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
            if (nsize == 0)
            {
                std::free(ptr);
                return nullptr;
            }
            return std::realloc(ptr, nsize);
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

PROBE_DECL = '''    namespace LuaMadeSimple
    {
        // [B7] defined in LuaMadeSimple.cpp; declared here, not in the header, so no consumer is recompiled for it.
        auto lua_cpp_probe(int* out) noexcept -> void;
    }

'''


def sha(b): return hashlib.sha256(b).hexdigest()


def insert_passthrough(text, needle, expected, label):
    """Insert a catch(void*){throw;} block before each `catch (...)` whose handler contains needle."""
    out, pos, hits = [], 0, 0
    for m in re.finditer(r'^(?P<i>[ \t]*)catch \(\.\.\.\)[ \t]*\r?\n[ \t]*\{', text, re.M):
        b0 = text.index('{', m.end('i') + len('catch (...)'))
        depth, j = 0, b0
        while j < len(text):
            if text[j] == '{': depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0: break
            j += 1
        handler = text[m.start():j + 1]
        if needle not in handler:
            continue
        before = text[max(0, m.start() - 400):m.start()].rstrip()
        if re.search(r'catch \(void\*\)\s*\{[^{}]*throw;\s*\}$', before):
            raise SystemExit(f'FAIL {label}: already has a catch (void*) pass-through (not idempotent)')
        out.append(text[pos:m.start()])
        out.append(PASS_BLOCK.format(i=m.group('i')))
        pos = m.start()
        hits += 1
    out.append(text[pos:])
    if hits != expected:
        raise SystemExit(f'FAIL {label}: expected {expected} match(es) for needle, found {hits}')
    return ''.join(out), hits


def replace_once(text, old, new, label):
    if text.count(old) != 1:
        raise SystemExit(f'FAIL {label}: anchor found {text.count(old)} times, expected 1')
    return text.replace(old, new, 1)


def main(argv):
    if len(argv) != 3 or argv[1] != '--base' or argv[2] not in ('candidate', 'b5final', 'b6'):
        print(__doc__); return 2
    base = argv[2]
    files = ['src/LuaUObject.cpp', 'src/LuaMadeSimple/LuaMadeSimple.cpp', 'src/LuaMod.cpp', 'src/LuaPathShim.cpp', 'src/LuaFText.cpp']
    src, shas_before = {}, {}
    for f in files:
        p = base_path(base, f)
        data = p.read_bytes()
        src[f] = data.decode('utf-8')
        shas_before[f] = sha(data)
        if base == 'candidate' and f in CAND_PINS and CAND_PINS[f] != shas_before[f]:
            raise SystemExit(f'FAIL: {f} drifted from the pinned candidate sha ({shas_before[f][:8]} != {CAND_PINS[f][:8]})')
    applied = []

    def note(eid, cls, f, n): applied.append({'id': eid, 'class': cls, 'file': f, 'count': n})

    t, n = insert_passthrough(src['src/LuaUObject.cpp'], 'Keep C++ exceptions from crossing the C Lua protected frame', 1, 'E1'); src['src/LuaUObject.cpp'] = t; note('E1', 'REQUIRED', 'src/LuaUObject.cpp', n)
    f = 'src/LuaMadeSimple/LuaMadeSimple.cpp'
    t, n = insert_passthrough(src[f], 'Formatting the error failed (typically std::bad_alloc under', 1, 'E2'); src[f] = t; note('E2', 'REQUIRED', f, n)
    t, n = insert_passthrough(src[f], 'mem_note = " [lua-mem: note-format-failed]"', 1, 'E3'); src[f] = t; note('E3', 'DEFENSIVE', f, n)
    src[f] = replace_once(src[f], '    auto new_state() -> Lua&\n    {', PROBE_CODE + '    auto new_state() -> Lua&\n    {', 'P1'); note('P1', 'PROBE', f, 1)
    f = 'src/LuaMod.cpp'
    t, n = insert_passthrough(src[f], 'cannot open native Lua file: source lookup failed', 1, 'E4'); src[f] = t; note('E4', 'DEFENSIVE', f, n)
    t, n = insert_passthrough(src[f], 'Native Lua module loading failed', 1, 'E5'); src[f] = t; note('E5', 'DEFENSIVE', f, n)
    t, n = insert_passthrough(src[f], 'L22Diagnostics::error("RegisterKeyBind", "non-standard C++ exception")', 1, 'E8'); src[f] = t; note('E8', 'LOG-PARITY', f, n)
    t, n = insert_passthrough(src[f], 'L22Diagnostics::error("StaticConstructObject", "non-standard C++ exception")', 1, 'E9'); src[f] = t; note('E9', 'LOG-PARITY', f, n)
    t, n = insert_passthrough(src[f], 'L22Diagnostics::error("ExecuteInGameThread", "non-standard C++ exception")', 1, 'E10'); src[f] = t; note('E10', 'LOG-PARITY', f, n)
    src[f] = replace_once(src[f], '    auto LuaMod::on_program_start() -> void\n    {\n',
                          PROBE_DECL + '    auto LuaMod::on_program_start() -> void\n    {\n' + PROBE_CALL, 'P2'); note('P2', 'PROBE', f, 1)
    f = 'src/LuaPathShim.cpp'
    t, n = insert_passthrough(src[f], 'Registration is best-effort: any failure leaves the stock io/os', 1, 'E6'); src[f] = t; note('E6', 'DEFENSIVE', f, n)
    f = 'src/LuaFText.cpp'
    t, n = insert_passthrough(src[f], '"FText Lua construction", "non-standard C++ exception"', 2, 'E7'); src[f] = t; note('E7', 'LOG-PARITY', f, n)

    total = sum(a['count'] for a in applied)
    out = OUTROOT / f'b7-on-{base}'
    tree = out / 'tree' / REL
    tree.mkdir(parents=True, exist_ok=True)
    diff_lines, shas_after = [], {}
    for f in files:
        old = None
        p = base_path(base, f)
        old = p.read_text(encoding='utf-8')
        if old == src[f]: continue
        (tree / f).parent.mkdir(parents=True, exist_ok=True)
        (tree / f).write_text(src[f], encoding='utf-8')
        shas_after[f] = sha(src[f].encode())
        diff_lines += list(difflib.unified_diff(old.splitlines(True), src[f].splitlines(True), f'a/{REL}/{f}', f'b/{REL}/{f}', n=3))
    (out / 'b7-runtime.patch').write_text(''.join(diff_lines), encoding='utf-8')
    (out / 'EDITS.json').write_text(json.dumps({'base': base, 'edits': applied, 'edit_count': total, 'files_touched': sorted(shas_after),
                                                'sha256_before': shas_before, 'sha256_after': shas_after,
                                                'b6_work_dir': str(B6.relative_to(WHAT)) if base == 'b6' else None, 'b5_final_dir': str(B5F.relative_to(WHAT)) if base == 'b5final' else None}, indent=1) + '\n', encoding='utf-8')
    print(f'base={base}: {len(applied)} edit groups ({total} sites) applied to {len(shas_after)} files; patch lines={len(diff_lines)}')
    for a in applied: print(f"  {a['id']:4s} {a['class']:11s} {a['file']}  x{a['count']}")
    for f, s in sorted(shas_before.items()): print(f'  base sha {s[:8]} {f}')
    if total == 0 or not shas_after:
        print('FAIL: nothing applied'); return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
