#!/usr/bin/env python3
"""cap #4 host test suite (Mac-only; no console, no PRX build).

  python3 -B tests/test_cap04.py [--no-syntax]

Every step prints what it checked. The suite FAILS when any step fails or when zero items were
checked. Scratch copies live in a temporary directory under this folder's tests/.work (deleted and
recreated per run); nothing outside the cap04 folder is written (the gates and the decoder are
only READ / executed; their outputs go to tests/ files here).
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
CAP = HERE.parent
PATCH = CAP / 'PATCH'
WORK = HERE / '.work'
WHAT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = WHAT / 'outputs'
CANDIDATE = OUT / '2026-10-08-l26-runtime/typescan-r4/candidate'
R11_TAR = OUT / '2026-10-08-l26-frozen/R11-watch-world-uenum-bfd95db6/snapshot-20261013T005957Z.tar.gz'
HARVEST = OUT / '2026-10-08-l25-prep/harvest'
EV14 = OUT / '2026-10-08-l25-prep/evidence/launch-l26-R11-launch14-20261013'
CLANG = '/usr/bin/clang++'

results = []  # (name, ok, detail)


def step(name, ok, detail=''):
    results.append((name, bool(ok), detail))
    print('%s %-58s %s' % ('PASS' if ok else 'FAIL', name, detail))


def run(cmd, **kw):
    p = subprocess.run(cmd, capture_output=True, text=True, **kw)
    return p.returncode, p.stdout + p.stderr


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def fresh_r11(dst):
    if dst.exists():
        shutil.rmtree(dst)
    tmp = WORK / 'r11-extract'
    if not tmp.exists():
        with tarfile.open(R11_TAR) as t:
            members = [m for m in t.getmembers() if m.name.startswith('2026-10-08-l26-runtime/typescan-r4/candidate/')
                       and (m.isfile() or m.isdir()) and '..' not in Path(m.name).parts and not m.name.startswith('/')]
            t.extractall(tmp, members=members)
    shutil.copytree(tmp / '2026-10-08-l26-runtime/typescan-r4/candidate', dst)
    return dst


N_EDITS = 25  # exact-once replacements in PATCH/apply_cap04.py


def apply(tree, *extra):
    return run([sys.executable, '-B', str(PATCH / 'apply_cap04.py'), '--tree', str(tree), *extra])


def main():
    do_syntax = '--no-syntax' not in sys.argv
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)

    # ---------------------------------------------------------------- T1 apply script
    base = fresh_r11(WORK / 'base-r11')
    tree = fresh_r11(WORK / 'tree-r11')
    rc, out = apply(tree)
    step('T1a dry run on R11 copy', rc == 0 and 'checked %d replacements' % N_EDITS in out and 'dry run' in out, out.strip().splitlines()[-2] if out.strip() else '')
    step('T1b dry run wrote nothing', sha(tree / 'src/LuaMod.cpp') == sha(base / 'src/LuaMod.cpp'))
    rc, out = apply(tree, '--write')
    step('T1c write on R11 copy (base R11-bfd95db6)', rc == 0 and 'base: R11-bfd95db6' in out and 'checked %d replacements' % N_EDITS in out)
    rc, out = apply(tree, '--write', '--allow-drift')
    step('T1d second application refused (exact-once anchors)', rc == 1 and 'REFUSED' in out and 'already applied' in out, out.strip().splitlines()[-1])
    drift = fresh_r11(WORK / 'drift')
    (drift / 'src/LuaMod.cpp').write_text((drift / 'src/LuaMod.cpp').read_text() + '\n// drift\n')
    rc, out = apply(drift)
    step('T1e unknown base refused without --allow-drift', rc == 1 and 'matches no known base' in out)
    t = (drift / 'src/UE4SSProgram.cpp').read_text().replace('[L25] start gate: {} started mod(s){}', '[L25] start gate changed', 1)
    (drift / 'src/UE4SSProgram.cpp').write_text(t)
    rc, out = apply(drift, '--allow-drift')
    step('T1f missing anchor refused even with --allow-drift', rc == 1 and 'E4-mods-started anchor count=0' in out)
    # the live candidate tree (moving: R12 follow-ups) -- a COPY only
    cur = WORK / 'tree-current'
    shutil.copytree(CANDIDATE, cur, ignore=shutil.ignore_patterns('.syntax-logs'))
    cur_hash = {f: sha(cur / f) for f in ('src/LuaMod.cpp', 'src/UObjectGlobals.cpp', 'src/UE4SSProgram.cpp')}
    rc, out = apply(cur, '--write')  # pinned base: no --allow-drift (a live tree that moved again fails here: re-stage)
    step('T1g applies to a copy of the live candidate (pinned base R11+R12)', rc == 0 and 'base: candidate-R12-20261012T2159' in out and 'checked %d replacements' % N_EDITS in out,
         json.dumps({k: v[:12] for k, v in cur_hash.items()}) if rc == 0 else out.strip().splitlines()[-1])

    # unified diff for review (candidate-relative paths)
    # proper per-file unified diffs with candidate-relative paths
    lines = []
    for f in ('src/LuaMod.cpp', 'src/UObjectGlobals.cpp', 'src/UE4SSProgram.cpp'):
        p = subprocess.run(['diff', '-u', '--label', 'a/' + f, '--label', 'b/' + f, str(base / f), str(tree / f)], capture_output=True, text=True)
        lines.append(p.stdout)
    (PATCH / 'cap04-vs-R11.diff').write_text(''.join(lines))
    hunks = sum(x.count('\n@@ ') + (1 if x.startswith('@@ ') else 0) for x in lines)
    step('T1h unified diff written (R11 base)', hunks > 0, '%d hunks -> PATCH/cap04-vs-R11.diff' % hunks)
    # the diff against the build base (live candidate = R11 + R12 follow-ups)
    lines = []
    for f in ('src/LuaMod.cpp', 'src/UObjectGlobals.cpp', 'src/UE4SSProgram.cpp'):
        p = subprocess.run(['diff', '-u', '--label', 'a/' + f, '--label', 'b/' + f, str(CANDIDATE / f), str(cur / f)], capture_output=True, text=True)
        lines.append(p.stdout)
    (PATCH / 'cap04-vs-live-R12.diff').write_text(''.join(lines))
    hunks_live = sum(x.count('\n@@ ') + (1 if x.startswith('@@ ') else 0) for x in lines)
    step('T1i unified diff written (live candidate base)', hunks_live == hunks, '%d hunks -> PATCH/cap04-vs-live-R12.diff (R11 diff: %d)' % (hunks_live, hunks))

    # ---------------------------------------------------------------- T2 simulation of the shipping code
    sim = HERE / 'world_ev_sim'
    gen = WORK / 'gen'
    rc, out = run([sys.executable, '-B', str(sim / 'extract.py'), str(tree), str(gen)])
    step('T2a extract world_ev/feed/snapshot verbatim from patched tree', rc == 0 and 'extracted 4 blocks' in out, out.strip())
    flags = ['-std=c++20', '-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-I', str(sim), '-I', str(gen)]
    rc, out = run([CLANG, *flags, str(sim / 'sim_main.cpp'), '-o', str(WORK / 'sim')])
    step('T2b compile simulation (clang++ -std=c++20, ASan+UBSan)', rc == 0, out.strip()[-300:] if rc else '')
    scen = ['s1', 's2', 's3', 's4', 's5', 's6', 's7', 's8', 's9', 's10', 's11']
    total_checks = 0
    if rc == 0:
        for s in scen:
            rc2, o2 = run([str(WORK / 'sim'), s])
            m = re.search(r'(\d+) checks, (\d+) failed', o2)
            total_checks += int(m.group(1)) if m else 0
            step('T2c scenario %s' % s, rc2 == 0, (o2.strip().splitlines() or [''])[-1])
    rc, out = run([CLANG, *flags, str(sim / 'snapshot_test.cpp'), '-o', str(WORK / 'snap')])
    rc2, o2 = run([str(WORK / 'snap')]) if rc == 0 else (1, out)
    step('T2d r9_index_snapshot_live unit test', rc == 0 and rc2 == 0, (o2.strip().splitlines() or [''])[-1])

    # ---------------------------------------------------------------- T3 mutants must be caught
    mutants = [
        ('M1 no exclusion of the unmatched batch', 'if (std::binary_search(unmatched.begin(), unmatched.end(), s_rp_idx[r])) { ++excluded; continue; }', 'if (false) { ++excluded; continue; }', ['s5']),
        ('M2 empty new batch skips the replay', 'if (s_backlog_idx.empty() && !world_ev::replay_running()) { return; }', 'if (s_backlog_idx.empty()) { return; }', ['s7', 's8']),
        ('M3 ambiguity threshold off by one', 'gamestates > 1', 'gamestates > 2', ['s3']),
        ('M4 replay for late registrations', 's_replay_state = s_startup_wanted ? 1 : 3;', 's_replay_state = 1;', ['s4', 's10']),
        ('M5 replay step never runs', 'if (!world_ev::replay_step(budget_end)) { return; }', '', ['s1']),
        ('M6 freed object not skipped by the replay', 'if (!item || item->IsUnreachable()) { continue; }', 'if (!item) { continue; }', ['s6']),
        ('M7 startup window closes before the start gate', 'if (!s_mods_started.load(std::memory_order_acquire)) { return; }', '', ['s9']),
        ('M8 feed takes the route live before startup closes', 'if (world_ev::s_wanted && world_ev::s_startup_closed) { (void)world_ev::ensure_ready(); }',
         'if (world_ev::s_wanted) { (void)world_ev::ensure_ready(); }', ['s9', 's11']),
        ('M9 note_index takes the route live before startup closes', 'if (s_wanted && s_startup_closed && refused_log::world_route_state',
         'if (s_wanted && refused_log::world_route_state', ['s9', 's11']),
    ]
    inc = (gen / 'world_ev_extract.inc').read_text()
    caught = 0
    for name, old, new, scs in mutants:
        if inc.count(old) < 1:
            step('T3 %s' % name, False, 'mutation anchor missing')
            continue
        mdir = WORK / ('mut-' + name.split()[0])
        mdir.mkdir()
        (mdir / 'world_ev_extract.inc').write_text(inc.replace(old, new))
        rc, out = run([CLANG, '-std=c++20', '-O1', '-I', str(sim), '-I', str(mdir), str(sim / 'sim_main.cpp'), '-o', str(mdir / 'sim')])
        failed = []
        if rc == 0:
            for s in scs:
                r3, _ = run([str(mdir / 'sim'), s])
                if r3 != 0:
                    failed.append(s)
        ok = rc == 0 and failed == scs
        caught += ok
        step('T3 %s caught' % name, ok, 'failing scenarios: %s' % failed if rc == 0 else out[-200:])

    # snapshot mutant (review finding 3): without the current-class check the stale case must fail
    snap_inc = (gen / 'snapshot_extract.inc').read_text()
    s_old = 'if (Class == nullptr || Object->GetClassPrivate() != Class) { continue; }'
    n_mut = len(mutants) + 1
    if snap_inc.count(s_old) != 1:
        step('T3 S1 stale class entry handed to Want caught', False, 'mutation anchor missing')
    else:
        mdir = WORK / 'mut-S1'
        mdir.mkdir()
        (mdir / 'snapshot_extract.inc').write_text(snap_inc.replace(s_old, 'if (Class == nullptr) { continue; }'))
        rc, out = run([CLANG, '-std=c++20', '-O1', '-I', str(sim), '-I', str(mdir), str(sim / 'snapshot_test.cpp'), '-o', str(mdir / 'snap')])
        r3, o3 = run([str(mdir / 'snap')]) if rc == 0 else (0, out)
        ok = rc == 0 and r3 != 0
        caught += ok
        step('T3 S1 stale class entry handed to Want caught', ok, (o3.strip().splitlines() or [''])[-1] if rc == 0 else out[-200:])

    # ---------------------------------------------------------------- T4 no-detour gate
    rc_b, out_b = run([sys.executable, '-B', str(HARVEST / 'no_detour_gate.py'), '--root', str(base)])
    rc_p, out_p = run([sys.executable, '-B', str(HARVEST / 'no_detour_gate.py'), '--root', str(tree)])
    (HERE / 'no-detour-gate-baseline-r11.txt').write_text(out_b)
    (HERE / 'no-detour-gate-patched-r11.txt').write_text(out_p)
    m_b = re.search(r'PASS \((\d+) detour-attempt sites; (\d+) notes', out_b)
    m_p = re.search(r'PASS \((\d+) detour-attempt sites; (\d+) notes', out_p)
    m_n = re.search(r'scanned (\d+) C\+\+ sources', out_p)
    step('T4 no_detour_gate: patched PASS with 0 grandfathered LoadMap detour sites (R11: 4)',
         rc_p == 0 and m_p and m_p.group(2) == '0' and m_b and m_b.group(2) == '4' and m_n and int(m_n.group(1)) > 0,
         'scanned %s sources; notes R11=%s patched=%s' % (m_n.group(1) if m_n else '?', m_b.group(2) if m_b else '?', m_p.group(2) if m_p else '?'))

    # ---------------------------------------------------------------- T5 fs-throw gate: no new throwing std::filesystem call
    def fs_sites(root):
        rc, out = run([sys.executable, '-B', str(HARVEST / 'fs_throw_gate.py'), '--root', str(root), '--baseline', str(HARVEST / 'fs_throw_baseline-1e49188b.txt')])
        sites = sorted(re.sub(r'^.*?/(src|include)/', r'\1/', re.sub(r':\d+: ', ': ', ln.strip())) for ln in out.splitlines() if re.search(r'\.(cpp|hpp|h|inc):\d+: ', ln))
        m = re.search(r'checked (\d+) files', out)
        return sites, int(m.group(1)) if m else 0, out
    sb, nb, ob = fs_sites(base)
    sp, np_, op = fs_sites(tree)
    (HERE / 'fs-throw-gate-patched-r11.txt').write_text(op)
    step('T5 fs_throw_gate: identical std::filesystem site set (no new throwing call)', sb == sp and np_ > 0, '%d files, %d sites (both trees)' % (np_, len(sp)))

    # ---------------------------------------------------------------- T6 PS5 syntax check (prospero-clang++ -fsyntax-only, not a build)
    if do_syntax:
        units = ['--unit', 'unreal-uobjectglobals=src/UObjectGlobals.cpp', '--unit', 'program=src/UE4SSProgram.cpp', '--unit', 'luamod=src/LuaMod.cpp']
        for label, root in (('R11+cap04', tree), ('live-candidate+cap04', cur)):
            logd = HERE / ('syntax-' + label)
            if logd.exists():
                shutil.rmtree(logd)
            rc, out = run([sys.executable, '-B', str(HARVEST / 'syntax_check_tree.py'), '--root', str(root), *units, '--log-dir', str(logd)])
            m = re.search(r'checked (\d+) units, (\d+) failed', out)
            step('T6 PS5 -fsyntax-only %s (3 units)' % label, rc == 0 and m and m.group(1) == '3' and m.group(2) == '0', m.group(0) if m else out[-200:])
        neg = WORK / 'neg'
        shutil.copytree(tree, neg)
        t = (neg / 'src/LuaMod.cpp').read_text().replace('return s_replay_state == 2; }', 'return s_replay_state_typo == 2; }', 1)
        (neg / 'src/LuaMod.cpp').write_text(t)
        rc, out = run([sys.executable, '-B', str(HARVEST / 'syntax_check_tree.py'), '--root', str(neg), '--unit', 'luamod=src/LuaMod.cpp', '--log-dir', str(WORK / 'neglog')])
        step('T6 negative control: a planted error is caught', rc != 0 and 'errors=1' in out)
    else:
        print('SKIP T6 (--no-syntax)')

    # ---------------------------------------------------------------- T7 verifier on synthetic launch logs
    rc, out = run([sys.executable, '-B', str(HERE / 'test_early_timeline.py')])
    m = re.search(r'(\d+) cases, (\d+) failed', out)
    step('T7 early_timeline.py fixture cases', rc == 0 and m and m.group(2) == '0' and int(m.group(1)) > 0, m.group(0) if m else out[-300:])

    # ---------------------------------------------------------------- T8 verifier on the real launch-14 harvest (legacy build)
    rep = HERE / 'launch14-early-timeline.json'
    rc, out = run([sys.executable, '-B', str(PATCH / 'tools/early_timeline.py'), '--ue4ss', str(EV14 / 'harvest-l26-R11-launch14-20261013-ue4ss.log'),
                   '--diag', str(EV14 / 'harvest-l26-R11-launch14-20261013-supplier-diag.jsonl'), '--klog', str(EV14 / 'klog.decoded.txt'), '--json', str(rep)])
    (HERE / 'launch14-early-timeline.txt').write_text(out)
    d = json.loads(rep.read_text()) if rep.exists() else {}
    tl = {x['name']: x['rel_s'] for x in d.get('timeline', [])}
    step('T8 launch 14 replay: LEGACY, INCONCLUSIVE, exec->module_start 68.9 s',
         rc == 1 and d.get('mode') == 'legacy' and d.get('verdict') == 'INCONCLUSIVE' and abs(tl.get('runtime module_start', 0) - 68.887) < 0.01,
         'module_start +%.1f s, mods +%.1f s, first pump +%.1f s, first map +%.1f s' % (
             tl.get('runtime module_start', -1), tl.get('on_program_start (mods about to start)', -1),
             tl.get('first carrier fire (legacy)', -1), tl.get('first map seen (legacy, +-1 s)', -1)))

    # ---------------------------------------------------------------- summary
    n = len(results)
    bad = [r for r in results if not r[1]]
    print('\nchecked %d steps (%d simulation checks, %d/%d mutants caught): %d failed' % (n, total_checks, caught, n_mut, len(bad)))
    if n == 0 or bad or total_checks == 0 or caught == 0 or caught != n_mut:
        print('SUITE: FAIL')
        return 1
    print('SUITE: PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
