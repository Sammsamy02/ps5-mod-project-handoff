#!/usr/bin/env python3
"""early_timeline.py -- capability #4 verifier: same-run startup timeline + first-map ordering.

Mac-only, read-only. Reads ONE launch's harvested files (never the console):
  --ue4ss  UE4SS.log (raw NUL-interleaved or already decoded; always decoded first with
           outputs/2026-10-08-l25-prep/harvest/decode_session_log.py)
  --diag   modbridge-supplier-diag.jsonl (optional; same decoder; rows of the launch's pid)
  --klog   decoded klog (optional; gives the game process EXEC time on the kernel ms clock)
  --pid N  (optional; default = pid of the LAST module_start_entered row in --diag)
  --json OUT (optional report; must not be one of the inputs)

Checks (each prints PASS / FAIL / INCONCLUSIVE with the log lines it used):
  K1 clock    : the [EARLY] steady-clock ms and the sink's CLOCK_MONOTONIC t_ms are the same clock
                (diag rows early_mods_started / early_first_pump carry the steady ms low 32 bits).
                A mismatch only means the two clocks differ: INVESTIGATE, never FAIL.
  K2 order    : exec < module_start < mods-started <= world-route-live <= first-map-seen <=
                first LoadMapPre delivery <= first LoadMapPost delivery, and module_start < first-pump.
                (A pump during mod start is legitimate on PS5, so first-pump is not ordered against
                mods-started.) An out-of-order timeline is INVESTIGATE (read the timeline), never FAIL.
  K3 no-detour: "[EARLY] stage=loadmap-native state=off" and NO LoadMap detour attempt lines.
  K4 startup  : startup-hooks seen; when hooks were registered at startup, the replay was
                requested (replay-request) and decided (replay / none); skip-ambiguous = INCONCLUSIVE.
  K5 first-map: the first map seen is the replayed one when the baseline held one GameState.
  K6 pairs    : every map change seen got its LoadMap post (the last one may still be open).
  K7 health   : last [WORLD-EV] stats line has dropped=0 errors=0.
  K8 hitch    : startup-replay snapshot_ms <= 20 (one-time index scan); above = INVESTIGATE. took_ms is
                printed only (the replay itself is spread over frames under the matcher budget).
Verdict: PASS = every check PASS; FAIL = any FAIL; else INVESTIGATE if any check says so; else
INCONCLUSIVE. Only FAIL is a runtime fault; INVESTIGATE means read the timeline, not roll back. A log without any [EARLY]
line (a build before this patch) is reported in LEGACY mode (timeline from older markers) and is
always INCONCLUSIVE. Exit 0 PASS, 1 FAIL/INVESTIGATE/INCONCLUSIVE, 2 input error. Zero lines read = exit 1.
"""
import argparse
import json
import re
import sys
from pathlib import Path

HARVEST = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest')


def load_decoder():
    sys.dont_write_bytecode = True  # never write __pycache__ into the harvest folder
    sys.path.insert(0, str(HARVEST))
    try:
        import decode_session_log  # noqa: E402  (read-only import of the project decoder)
    finally:
        sys.path.pop(0)
    return decode_session_log


def decoded_lines(path, dec):
    data, _stats, _notes = dec.decode_file(str(path))
    return data.decode('utf-8', errors='replace').splitlines()


WALL = re.compile(r'^\[(\d{4}-\d\d-\d\d) (\d\d):(\d\d):(\d\d)\]')
KV = re.compile(r'(\w+)=(\'[^\']*\'|\S+)')


def wall_s(line):
    m = WALL.match(line)
    if not m:
        return None
    return int(m.group(2)) * 3600 + int(m.group(3)) * 60 + int(m.group(4))


def kv(line):
    return {k: v.strip("'") for k, v in KV.findall(line)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ue4ss', required=True)
    ap.add_argument('--diag')
    ap.add_argument('--klog')
    ap.add_argument('--pid', type=int)
    ap.add_argument('--json')
    a = ap.parse_args()
    ins = [Path(p).resolve() for p in (a.ue4ss, a.diag, a.klog) if p]
    for p in ins:
        if not p.is_file():
            print('INPUT ERROR: missing %s' % p)
            return 2
    if a.json and Path(a.json).resolve() in ins:
        print('INPUT ERROR: --json would overwrite an input')
        return 2
    dec = load_decoder()

    ue = decoded_lines(a.ue4ss, dec)
    diag_rows = []
    if a.diag:
        for ln in decoded_lines(a.diag, dec):
            ln = ln.strip()
            if ln.startswith('{'):
                try:
                    diag_rows.append(json.loads(ln))
                except ValueError:
                    pass
    klog = decoded_lines(a.klog, dec) if a.klog else []
    print('read: ue4ss %d lines, diag %d rows, klog %d lines' % (len(ue), len(diag_rows), len(klog)))
    if len(ue) == 0:
        print('FAIL: zero UE4SS lines read')
        return 1

    # ---- diag rows of this launch's pid
    pid = a.pid
    if pid is None:
        starts = [r for r in diag_rows if r.get('tag') == 'module_start_entered']
        pid = starts[-1]['pid'] if starts else None
    rows = [r for r in diag_rows if r.get('pid') == pid] if pid is not None else []

    def first_row(tag):
        return next((r for r in rows if r.get('tag') == tag), None)

    # ---- [EARLY] lines
    early = []
    for i, ln in enumerate(ue, 1):
        p = ln.find('[EARLY] ')
        if p >= 0:
            early.append((i, ln, kv(ln[p:])))
    legacy = len(early) == 0

    def early_stage(stage, **want):
        for i, ln, d in early:
            if d.get('stage') == stage and all(d.get(k) == v for k, v in want.items()):
                return i, ln, d
        return None

    def ue_first(needle):
        for i, ln in enumerate(ue, 1):
            if needle in ln:
                return i, ln
        return None

    checks = []

    def add(name, verdict, detail, cites=()):
        checks.append({'check': name, 'verdict': verdict, 'detail': detail, 'cite': [c for c in cites if c]})

    def cite_ue(hit):
        return ('ue4ss:%d: %s' % (hit[0], hit[1][:200])) if hit else None

    def cite_row(r):
        return ('diag pid=%s seq=%s tag=%s t_ms=%s value=%s' % (r.get('pid'), r.get('seq'), r.get('tag'), r.get('t_ms'), r.get('value'))) if r else None

    # ---- kernel EXEC of the game (klog "<N msec>" kernel clock), the latest one before module_start
    ms_start = first_row('module_start_entered')
    exec_ms, exec_cite = None, None
    for j, ln in enumerate(klog, 1):
        m = re.search(r'EXEC /app0/eboot\.bin \[user\].*<(\d+) msec>', ln)
        if m:
            v = int(m.group(1))
            if ms_start is None or v <= ms_start['t_ms']:
                exec_ms, exec_cite = v, 'klog:%d: %s' % (j, ln.strip()[:160])

    # ---- the timeline on the sink clock (ms)
    tl = []  # (name, ms, source, cite)
    if exec_ms is not None:
        tl.append(('game process exec', exec_ms, 'klog', exec_cite))
    if ms_start:
        tl.append(('runtime module_start', ms_start['t_ms'], 'diag', cite_row(ms_start)))
    r = first_row('module_start_return')
    if r:
        tl.append(('module_start returned', r['t_ms'], 'diag', cite_row(r)))
    r = first_row('on_program_start_entered')
    if r:
        tl.append(('on_program_start (mods about to start)', r['t_ms'], 'diag', cite_row(r)))
    stage_names = [('mods-started', 'mods started'), ('loadmap-native', 'LoadMap native delivery off'),
                   ('first-pump', 'first game-thread pump'), ('startup-hooks', 'startup world hooks counted'),
                   ('world-route-live', 'WORLD-EV route live'), ('replay-request', 'startup replay requested'),
                   ('startup-replay', 'startup replay decided'),
                   ('startup-replay-done', 'startup replay done'), ('first-map-seen', 'first map seen')]
    for st, label in stage_names:
        hit = early_stage(st)
        if hit and 't_ms' in hit[2]:
            tl.append((label, int(hit[2]['t_ms']), 'ue4ss', cite_ue(hit)))
    for kind in ('LoadMapPre', 'InitGameState', 'BeginPlay', 'LoadMapPost'):
        hit = early_stage('first-delivery', kind=kind)
        if hit and 't_ms' in hit[2]:
            tl.append(('first %s delivery' % kind, int(hit[2]['t_ms']), 'ue4ss', cite_ue(hit)))
    if legacy:
        r = first_row('r3_marshal_first_invoke')
        if r:
            tl.append(('first carrier fire (legacy)', r['t_ms'], 'diag', cite_row(r)))
        # legacy wall-clock markers, mapped onto the sink clock via on_program_start (+-1 s)
        anchor_ue = ue_first('[l25] on_program_start entered')
        anchor_diag = first_row('on_program_start_entered')
        if anchor_ue and anchor_diag and wall_s(anchor_ue[1]) is not None:
            base_wall = wall_s(anchor_ue[1])
            for needle, label in (('[WORLD-EV] route live', 'WORLD-EV route live (legacy, +-1 s)'),
                                  ('[WORLD-EV] map change seen', 'first map seen (legacy, +-1 s)'),
                                  ('[WORLD-EV] first delivery kind=LoadMapPre', 'first LoadMapPre delivery (legacy, +-1 s)'),
                                  ('[WORLD-EV] LoadMap post delivered', 'first LoadMapPost delivery (legacy, +-1 s)'),
                                  ('[R3] LoadMap deferred arm refused', 'LoadMap detour arm refused (legacy)')):
                hit = ue_first(needle)
                if hit and wall_s(hit[1]) is not None:
                    tl.append((label, anchor_diag['t_ms'] + (wall_s(hit[1]) - base_wall) * 1000, 'ue4ss-wall', cite_ue(hit)))
    tl.sort(key=lambda x: x[1])
    t0 = exec_ms if exec_ms is not None else (tl[0][1] if tl else 0)

    # ---- K1 clock identity
    pairs = [first_row('early_mods_started'), first_row('early_first_pump')]
    pairs = [p for p in pairs if p]
    if legacy:
        add('K1 clock', 'INCONCLUSIVE', 'legacy build: no [EARLY] clock rows')
    elif not pairs:
        add('K1 clock', 'INCONCLUSIVE', 'no early_* rows in the diag sink (no --diag, or rows missing)')
    else:
        bad = [p for p in pairs if not (0 <= ((p['t_ms'] - p['value']) % (1 << 32)) <= 100)]
        add('K1 clock', 'INVESTIGATE' if bad else 'PASS',
            'steady ms (low 32 bits) vs sink t_ms differ by %s ms' % [((p['t_ms'] - p['value']) % (1 << 32)) for p in pairs],
            [cite_row(p) for p in pairs])

    # ---- K2 order
    def tval(label):
        return next((x[1] for x in tl if x[0] == label), None)
    seq = ['game process exec', 'runtime module_start', 'mods started', 'WORLD-EV route live',
           'first map seen', 'first LoadMapPre delivery', 'first LoadMapPost delivery']
    if legacy:
        add('K2 order', 'INCONCLUSIVE', 'legacy build: wall-clock markers only (+-1 s), see timeline')
    else:
        present = [(s, tval(s)) for s in seq if tval(s) is not None]
        viol = []
        for (s1, v1), (s2, v2) in zip(present, present[1:]):
            strict = s1 in ('game process exec', 'runtime module_start')
            if (v2 <= v1) if strict else (v2 < v1):
                viol.append('%s (%d) !< %s (%d)' % (s1, v1, s2, v2))
        ms_v, pump_v = tval('runtime module_start'), tval('first game-thread pump')
        if ms_v is not None and pump_v is not None and pump_v <= ms_v:
            viol.append('runtime module_start (%d) !< first game-thread pump (%d)' % (ms_v, pump_v))
        missing = [s for s in seq + ['first game-thread pump'] if tval(s) is None]
        if viol:
            add('K2 order', 'INVESTIGATE', '; '.join(viol))
        elif missing:
            add('K2 order', 'INCONCLUSIVE', 'in order; missing: %s' % ', '.join(missing))
        else:
            add('K2 order', 'PASS', 'all %d milestones in order on one clock' % len(present))

    # ---- K3 no LoadMap detour attempt
    attempt = ue_first('[UE4SS.LoadMap.LuaModImpl] Failed to add hook') or ue_first('[R3] LoadMap deferred arm') or ue_first('LoadMap detour armed')
    off = early_stage('loadmap-native', state='off')
    if attempt:
        add('K3 no-detour', 'FAIL' if not legacy else 'INCONCLUSIVE',
            'a LoadMap code-detour arm was attempted on PS5 (soft-refused in R11)', [cite_ue(attempt)])
    elif off:
        add('K3 no-detour', 'PASS', 'LoadMap native delivery off from startup; no detour attempt line', [cite_ue(off)])
    else:
        add('K3 no-detour', 'INCONCLUSIVE', 'no loadmap-native line')

    # ---- K4 startup registrations and the replay decision
    hooks = early_stage('startup-hooks')
    live = early_stage('world-route-live')
    req = early_stage('replay-request')
    rep = early_stage('startup-replay')
    if legacy:
        add('K4 startup', 'INCONCLUSIVE', 'legacy build: no startup replay (objects alive before the index was used are never reported)')
    elif not hooks:
        add('K4 startup', 'INCONCLUSIVE', 'no startup-hooks line (the feed never ran?)')
    else:
        d = hooks[2]
        n_hooks = sum(int(d.get(k, '0')) for k in ('loadmap_pre', 'loadmap_post', 'initgamestate', 'beginplay'))
        if n_hooks == 0:
            add('K4 startup', 'PASS', 'no world hooks at startup: nothing to replay', [cite_ue(hooks)])
        elif not live:
            add('K4 startup', 'INCONCLUSIVE', 'world hooks at startup but the route never went live', [cite_ue(hooks)])
        elif not req or req[2].get('replay') != 'requested':
            add('K4 startup', 'FAIL', 'startup hooks present but the replay was not requested', [cite_ue(hooks), cite_ue(live), cite_ue(req)])
        elif not rep:
            add('K4 startup', 'FAIL', 'replay requested but never decided', [cite_ue(req)])
        elif rep[2].get('decision') in ('replay', 'none'):
            add('K4 startup', 'PASS', 'replay decision=%s gamestates=%s candidates=%s' % (rep[2].get('decision'), rep[2].get('gamestates'), rep[2].get('candidates')),
                [cite_ue(hooks), cite_ue(req), cite_ue(rep)])
        else:
            add('K4 startup', 'INCONCLUSIVE', 'replay decision=%s (fail-closed path)' % rep[2].get('decision'), [cite_ue(rep)])

    # ---- K5 first map
    fm = early_stage('first-map-seen')
    if legacy or not rep or rep[2].get('decision') != 'replay':
        add('K5 first-map', 'INCONCLUSIVE' if legacy or not fm else 'PASS',
            'first map: %s' % (fm[2] if fm else 'not seen'), [cite_ue(fm)])
    else:
        gs = rep[2].get('gamestates')
        want = 'replay' if gs == '1' else 'live'
        if not fm:
            add('K5 first-map', 'INCONCLUSIVE', 'no first-map-seen line')
        else:
            add('K5 first-map', 'PASS' if fm[2].get('src') == want else 'FAIL',
                'baseline gamestates=%s -> expected src=%s, got src=%s world=%s' % (gs, want, fm[2].get('src'), fm[2].get('world')), [cite_ue(rep), cite_ue(fm)])

    # ---- K6 map pre/post pairs
    seen = [(i, ln) for i, ln in enumerate(ue, 1) if '[WORLD-EV] map change seen' in ln]
    closed = [(i, ln) for i, ln in enumerate(ue, 1) if '[WORLD-EV] map load window closed' in ln]
    if not seen:
        add('K6 pairs', 'INCONCLUSIVE', 'no map change seen')
    else:
        ok = len(closed) >= len(seen) - 1
        add('K6 pairs', 'PASS' if ok else 'FAIL', 'maps seen=%d, load windows closed=%d' % (len(seen), len(closed)),
            [cite_ue(seen[0]), cite_ue(closed[0]) if closed else None])

    # ---- K7 health
    stats = [(i, ln) for i, ln in enumerate(ue, 1) if '[WORLD-EV] stats ' in ln]
    if not stats:
        add('K7 health', 'INCONCLUSIVE', 'no [WORLD-EV] stats line')
    else:
        d = kv(stats[-1][1][stats[-1][1].find('[WORLD-EV]'):])
        ok = d.get('dropped') == '0' and d.get('errors') == '0'
        add('K7 health', 'PASS' if ok else 'FAIL', 'last stats dropped=%s errors=%s skipped_dead=%s streamed_unreported=%s' % (
            d.get('dropped'), d.get('errors'), d.get('skipped_dead'), d.get('streamed_unreported')), [cite_ue(stats[-1])])

    # ---- K8 one-time snapshot cost
    done = early_stage('startup-replay-done')
    if legacy or not rep or 'snapshot_ms' not in rep[2]:
        add('K8 hitch', 'INCONCLUSIVE', 'no startup-replay line with snapshot_ms')
    else:
        sms = int(rep[2]['snapshot_ms'])
        add('K8 hitch', 'PASS' if sms <= 20 else 'INVESTIGATE', 'snapshot_ms=%d (limit 20) took_ms=%s' % (sms, done[2].get('took_ms') if done else '-'),
            [cite_ue(rep), cite_ue(done)])

    # ---- report
    print('mode: %s   pid: %s' % ('LEGACY (no [EARLY] markers)' if legacy else 'EARLY markers', pid))
    print('timeline (ms after game process exec%s):' % ('' if exec_ms is not None else ' unknown: after first event'))
    for name, ms, src, cite in tl:
        print('  %+10.1f s  %-44s [%s] %s' % ((ms - t0) / 1000.0, name, src, cite or ''))
    for c in checks:
        print('%-13s %-12s %s' % (c['check'], c['verdict'], c['detail']))
        for ci in c['cite']:
            print('              <- %s' % ci)
    verdicts = [c['verdict'] for c in checks]
    if 'FAIL' in verdicts:
        overall = 'FAIL'
    elif verdicts and all(v == 'PASS' for v in verdicts) and not legacy:
        overall = 'PASS'
    elif 'INVESTIGATE' in verdicts:
        overall = 'INVESTIGATE'
    else:
        overall = 'INCONCLUSIVE'
    print('checked %d checks over %d [EARLY] lines, %d timeline points' % (len(checks), len(early), len(tl)))
    print('VERDICT: %s' % overall)
    if a.json:
        Path(a.json).write_text(json.dumps({'schema': 1, 'mode': 'legacy' if legacy else 'early', 'pid': pid, 'verdict': overall,
                                            'timeline': [{'name': n, 'ms': m, 'rel_s': (m - t0) / 1000.0, 'source': s, 'cite': c} for n, m, s, c in tl],
                                            'checks': checks,
                                            'inputs': [str(p) for p in ins]}, indent=1))
    if len(checks) == 0:
        return 1
    return 0 if overall == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
