#!/usr/bin/env python3
"""Generate the STAGED B7 "H4 boundary containment" patch (Mac-only; nothing is applied to any tree).

WHY (reviewer finding P1, 2026-10-12): native-UFunction hooks ARE live on the PS5 (B4 launch-1 log D:807, D:818,
D:866-D:894, D:918). The game calls them through the data-only Func swap in ScriptHookThunks.cpp, so GAME frames sit
below every hook callback. With Lua built as C++ a Lua error that escapes a hook callback is a C++ throw of a
pointer (lua_longjmp*); the thunk handlers catch std::exception only, so it would travel into game frames, which
the PS5 unwinder cannot walk: std::terminate. This patch adds one `catch (...)` that contains ANY exception at
the three places where our code hands control back to the game (pre callbacks, post callbacks, the thunk itself).
It is generic (no Lua knowledge, no mod/game value) and it only ever runs where the process would have died.

Edits (all in src/ScriptHookThunks.cpp, anchor based, each must match exactly once, else exit 1):
  H1 Dispatch  pre-callback try block   + catch (...)
  H2 Dispatch  post-callback try block  + catch (...)
  H3 Thunk<Index> outer try block       + catch (...)

The file is identical in the B4 candidate, final-b5 and the B6 work copies (none of them touches it), so one patch
serves all bases. Output (under staged-patch/):
  b7-h4/ScriptHookThunks.h4.patch   unified diff, apply from the worktree root with patch -p1 / git apply
  b7-h4/EDITS.json                  what was applied, shas
  b7-h4-on-<base>/tree/             b7-on-<base>/tree plus the patched file (overlay for b7_source_gate.py)
Usage: python3 -I make_b7_h4_patch.py
"""
import difflib, hashlib, json, shutil, sys
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
OUT = HERE.parent / 'staged-patch'
WHAT = HERE.parents[4]
REL = 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate'
SRC = WHAT / REL / 'src/ScriptHookThunks.cpp'
PIN = 'ecec915976c2b16ccecea11ec44d39c465252c7451e62a4a6671c00704d3c755'  # sha256 of the B4 candidate / final-b5 / B6 ScriptHookThunks.cpp

NOTE = ('    // [B7-H4] Game boundary. Game frames are below this function and the PS5 unwinder cannot walk them, so\n'
        '    // NOTHING may leave here. A Lua error that escaped the callback (Lua as C++: a thrown pointer, not a\n'
        '    // std::exception) ends up here instead of in std::terminate. It is logged, not delivered: if the main Lua\n'
        '    // state was inside a protected call when the game fired this hook, that call is not unwound by it.\n')


def catch_all(what): return f'    catch (...) {{ Output::send(STR("[B7-H4] contained a non-std exception (a Lua error?) at the game boundary: {what}\\n")); }}\n'


EDITS = [
    ('H1', '    catch (std::exception& e) { Output::send(STR("Error executing thunk pre callback: {}\\n"), ensure_str(e.what())); }\n', 'thunk pre callback'),
    ('H2', '    catch (std::exception& e) { Output::send(STR("Error executing thunk post callback: {}\\n"), ensure_str(e.what())); }\n', 'thunk post callback'),
    ('H3', '    catch (std::exception& e) { Output::send(STR("Error executing script thunk: {}\\n"), ensure_str(e.what())); }\n', 'script thunk'),
]


def main():
    data = SRC.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    if PIN and sha != PIN: raise SystemExit(f'FAIL: ScriptHookThunks.cpp drifted ({sha[:8]} != {PIN[:8]})')
    old = data.decode('utf-8'); new = old; applied = []
    for eid, anchor, what in EDITS:
        if new.count(anchor) != 1: raise SystemExit(f'FAIL {eid}: anchor found {new.count(anchor)} times, expected 1')
        new = new.replace(anchor, anchor + (NOTE if eid == 'H1' else '') + catch_all(what), 1)
        applied.append({'id': eid, 'what': what})
    if not applied or new == old: print('FAIL: nothing applied'); return 1
    diff = list(difflib.unified_diff(old.splitlines(True), new.splitlines(True), f'a/{REL}/src/ScriptHookThunks.cpp', f'b/{REL}/src/ScriptHookThunks.cpp', n=3))
    d = OUT / 'b7-h4'; d.mkdir(parents=True, exist_ok=True)
    (d / 'ScriptHookThunks.h4.patch').write_text(''.join(diff), encoding='utf-8')
    (d / 'EDITS.json').write_text(json.dumps({'file': 'src/ScriptHookThunks.cpp', 'sha256_before': sha, 'sha256_after': hashlib.sha256(new.encode()).hexdigest(), 'edits': applied}, indent=1) + '\n', encoding='utf-8')
    for base in ('candidate', 'b5final', 'b6'):
        src_tree = OUT / f'b7-on-{base}' / 'tree'
        if not src_tree.is_dir(): raise SystemExit(f'FAIL: {src_tree} missing (run make_b7_runtime_patch.py --base {base} first)')
        dst = OUT / f'b7-h4-on-{base}' / 'tree'
        if dst.parent.exists(): shutil.rmtree(dst.parent)
        shutil.copytree(src_tree, dst)
        (dst / REL / 'src').mkdir(parents=True, exist_ok=True)
        (dst / REL / 'src/ScriptHookThunks.cpp').write_text(new, encoding='utf-8')
    print(f'b7-h4: {len(applied)} edits applied to src/ScriptHookThunks.cpp (sha {sha[:8]} -> {hashlib.sha256(new.encode()).hexdigest()[:8]}); patch lines={len(diff)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
