#!/usr/bin/env python3
"""Fixture cases for PATCH/tools/early_timeline.py (synthetic launch logs in the shape the patched
runtime writes). Mac-only. Prints how many cases ran; fails on zero cases or any mismatch."""
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE.parent / 'PATCH/tools/early_timeline.py'
FIX = HERE / '.work' / 'timeline-fixtures'

EXEC = 51277108
MS = 51345995  # module_start (diag clock)


def ue_lines(mods=MS + 6200, pump=MS + 17700, live=MS + 17716, rep_dec='replay', gs='1', first_src='replay',
             pre=MS + 17720, post=MS + 20800, detour=False, wide_marker=False, stats='dropped=0 errors=0', no_request=False, snap_ms=3):
    L = []
    w = '[2026-10-12 20:51:47] '
    L.append(w + '[EARLY] stage=loadmap-native state=off reason=ps5-no-code-detours owner=world-ev t_ms=%d' % (mods - 300))
    if detour:
        L.append(w + '[UE4SS.LoadMap.LuaModImpl] Failed to add hook, detour installation likely failed!')
    L.append(w + '[L25] start gate: 7 started mod(s)')
    L.append(w + '[EARLY] stage=mods-started t_ms=%d mods=7' % mods)
    L.append(w + '[EARLY] stage=first-pump t_ms=%d src=marshal' % pump)
    L.append(w + '[EARLY] stage=startup-hooks t_ms=%d loadmap_pre=1 loadmap_post=0 initgamestate=0 beginplay=0' % pump)
    L.append(w + '[WORLD-EV] route live: object index ... (tries=1, waited_ms=16)')
    L.append(w + '[EARLY] stage=world-route-live t_ms=%d waited_ms=16' % live)
    if not no_request:
        L.append(w + '[EARLY] stage=replay-request t_ms=%d startup_hooks=1 replay=requested' % live)
    L.append(w + '[EARLY] stage=startup-replay t_ms=%d decision=%s candidates=812 baseline=812 excluded_new=0 gamestates=%s snapshot_ms=%d' % (live + 1, rep_dec, gs, snap_ms))
    L.append(w + "[WORLD-EV] map change seen (object index): world='Level_X' held_joined=0 -> LoadMap pre queued")
    L.append(w + "[EARLY] stage=first-map-seen t_ms=%d src=%s world='Level_X'" % (live + 2, first_src))
    L.append(w + '[EARLY] stage=startup-replay-done t_ms=%d fed=700 took_ms=4 window_open=1 held=0' % (live + 5))
    L.append(w + '[EARLY] stage=first-delivery kind=LoadMapPre t_ms=%d' % pre)
    L.append(w + '[WORLD-EV] map load window closed after 3006 ms (reason=quiet): gamemodes=0 actors=0 held_joined=0')
    L.append(w + '[EARLY] stage=first-delivery kind=LoadMapPost t_ms=%d' % post)
    L.append(w + '[WORLD-EV] stats maps=1 gamemodes=0 actors=0 lua_calls=2 skipped_dead=0 streamed_unreported=0 late_after_post=0 held=0 settling=0 window_open=0 %s queued=0 paced_skips=0' % stats)
    data = ('\n'.join(L) + '\n').encode()
    if wide_marker:  # the dual-path writer: one marker written wide (unit-doubled with NULs)
        target = b'[EARLY] stage=first-pump'
        data = data.replace(target, b''.join(bytes([c, 0]) for c in target))
    return data


def diag_rows(mods=MS + 6200, pump=MS + 17700, clock_skew=0):
    rows = [
        {'pid': 365, 'seq': 1, 'tag': 'module_start_entered', 'value': 0, 't_ms': MS},
        {'pid': 365, 'seq': 1076, 'tag': 'module_start_return', 'value': 0, 't_ms': MS + 3991},
        {'pid': 365, 'seq': 1107, 'tag': 'on_program_start_entered', 'value': 1, 't_ms': MS + 6170},
        {'pid': 365, 'seq': 1110, 'tag': 'early_mods_started', 'value': (mods - clock_skew) & 0xFFFFFFFF, 't_ms': mods + 1},
        {'pid': 365, 'seq': 1122, 'tag': 'early_first_pump', 'value': (pump - clock_skew) & 0xFFFFFFFF, 't_ms': pump + 1},
        {'pid': 300, 'seq': 1, 'tag': 'module_start_entered', 'value': 0, 't_ms': 100},  # an older session (other pid)
    ]
    rows = rows[-1:] + rows[:-1]
    return ('\n'.join(json.dumps(r) for r in rows) + '\n').encode()


KLOG = ('<365> EXEC /app0/eboot.bin [user], vm#1, dmem#1 abi=native category=native_game <%d msec>\n' % EXEC).encode()


def case(name, ue, diag, want_rc, want_verdict, want_checks=None, klog=KLOG):
    d = FIX / name
    d.mkdir(parents=True, exist_ok=True)
    (d / 'ue4ss.log').write_bytes(ue)
    args = [sys.executable, '-B', str(TOOL), '--ue4ss', str(d / 'ue4ss.log'), '--json', str(d / 'out.json')]
    if diag is not None:
        (d / 'diag.jsonl').write_bytes(diag)
        args += ['--diag', str(d / 'diag.jsonl')]
    if klog is not None:
        (d / 'klog.txt').write_bytes(klog)
        args += ['--klog', str(d / 'klog.txt')]
    p = subprocess.run(args, capture_output=True, text=True)
    ok = p.returncode == want_rc
    verdict = None
    checks = {}
    if (d / 'out.json').exists():
        j = json.loads((d / 'out.json').read_text())
        verdict = j['verdict']
        checks = {c['check'].split()[0]: c['verdict'] for c in j['checks']}
    ok = ok and (want_verdict is None or verdict == want_verdict)
    for k, v in (want_checks or {}).items():
        ok = ok and checks.get(k) == v
    print('%s %-34s rc=%d verdict=%s %s' % ('PASS' if ok else 'FAIL', name, p.returncode, verdict, json.dumps(checks)))
    if not ok:
        print(p.stdout[-2500:])
    return ok


def main():
    cases = [
        ('pass-all', ue_lines(), diag_rows(), 0, 'PASS', {'K1': 'PASS', 'K2': 'PASS', 'K3': 'PASS', 'K4': 'PASS', 'K5': 'PASS', 'K8': 'PASS'}),
        # review finding 5: a pump during mod start is legitimate (s9 shape): K2 still PASS
        ('pass-pump-before-mods-started', ue_lines(pump=MS + 6000), diag_rows(pump=MS + 6000), 0, 'PASS', {'K2': 'PASS'}),
        ('pass-wide-marker-decoded', ue_lines(wide_marker=True), diag_rows(), 0, 'PASS', {'K2': 'PASS'}),
        # review finding 5: K1/K2 are timeline questions, not runtime faults -> INVESTIGATE, never FAIL
        ('investigate-clock-mismatch', ue_lines(), diag_rows(clock_skew=5000), 1, 'INVESTIGATE', {'K1': 'INVESTIGATE'}),
        ('investigate-order-post-before-pre', ue_lines(pre=MS + 21000, post=MS + 20800), diag_rows(), 1, 'INVESTIGATE', {'K2': 'INVESTIGATE'}),
        ('investigate-route-before-mods', ue_lines(live=MS + 6000), diag_rows(), 1, 'INVESTIGATE', {'K2': 'INVESTIGATE'}),
        ('fail-detour-wins-over-investigate', ue_lines(detour=True), diag_rows(clock_skew=5000), 1, 'FAIL', {'K1': 'INVESTIGATE', 'K3': 'FAIL'}),
        # review finding 6: one-time snapshot over 20 ms
        ('investigate-snapshot-slow', ue_lines(snap_ms=45), diag_rows(), 1, 'INVESTIGATE', {'K8': 'INVESTIGATE'}),
        ('fail-detour-attempt', ue_lines(detour=True), diag_rows(), 1, 'FAIL', {'K3': 'FAIL'}),
        ('fail-first-map-src', ue_lines(first_src='live'), diag_rows(), 1, 'FAIL', {'K5': 'FAIL'}),
        ('fail-health-dropped', ue_lines(stats='dropped=3 errors=0'), diag_rows(), 1, 'FAIL', {'K7': 'FAIL'}),
        ('fail-startup-hooks-not-replayed', ue_lines(no_request=True), diag_rows(), 1, 'FAIL', {'K4': 'FAIL'}),
        ('inconclusive-ambiguous', ue_lines(rep_dec='skip-ambiguous', gs='2', first_src='live'), diag_rows(), 1, 'INCONCLUSIVE', {'K4': 'INCONCLUSIVE'}),
        ('inconclusive-no-diag', ue_lines(), None, 1, 'INCONCLUSIVE', {'K1': 'INCONCLUSIVE'}),
        ('legacy-no-early-lines', b'[2026-10-12 20:51:47] [l25] on_program_start entered\n', diag_rows(), 1, 'INCONCLUSIVE', None),
        ('empty-log-fails', b'', diag_rows(), 1, None, None),
    ]
    n = 0
    bad = 0
    for c in cases:
        n += 1
        bad += not case(*c)
    print('%d cases, %d failed' % (n, bad))
    return 0 if n > 0 and bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
