#!/usr/bin/env python3
"""Compile ONLY the extracted wait_stable readiness logic with host mocks; never
full Ploader. House pattern: ploader-cleanup-candidate-1/run_controls.py.

Usage: python3 run_wait_controls.py <ploader_main.c> <output_dir>

The runner extracts the real count_modules / dep_present / deps_ready /
wait_stable / get_mode_params / now_ms text plus the mode_params_t typedef and
MAX_DEP* defines from the given source, writes extracted.inc, compiles
host_wait_controls.c against it (cc -std=c11 -Wall -Wextra -Werror -O1) and runs
the controls. -DHAS_CACHE is defined exactly when the source carries the 05r
readiness memo (proven_stable_pid), so the same harness serves as the red
control (unamended source: deadline/cache controls fail by construction).
"""
import argparse, json, re, subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)

s = a.source.read_text(encoding='utf-8-sig')

def function(header_prefix: str) -> str:
    start = s.index(header_prefix)
    opening = s.index('{', start)
    depth = 1
    i = opening + 1
    while depth:
        depth += (s[i] == '{') - (s[i] == '}')
        i += 1
    return s[start:i]

defines = '\n'.join(re.findall(r'^#define MAX_DEP.*$', s, re.M))
end = s.index('} mode_params_t;')
start = s.rindex('typedef struct {', 0, end)
typedef = s[start:end + len('} mode_params_t;')]

body = defines + '\n' + typedef + '\n'
# 05r memo slot (present only in amended sources; -DHAS_CACHE matches)
memo = re.search(r'^static pid_t proven_stable_pid = 0;$', s, re.M)
if memo is not None:
    body += '\n' + memo.group(0) + '\n'
# now_ms DEFINITION before wait_stable so no forward declaration is needed
body += '\nstatic struct timespec g_t0;\n\n' + function('static long now_ms(void) {') + '\n'
for fn in ('static mode_params_t get_mode_params(',
           'static int count_modules(',
           'static int dep_present(',
           'static int deps_ready(',
           'static int wait_stable('):
    body += '\n' + function(fn) + '\n'

(a.output / 'extracted.inc').write_text(body)

has_cache = 'proven_stable_pid' in s
base = Path(__file__).resolve().parent
cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O1',
       '-I', str(a.output)]
if has_cache:
    cmd.append('-DHAS_CACHE')
cmd += [str(base / 'host_wait_controls.c'), '-o', str(a.output / 'host-wait-controls')]
b = subprocess.run(cmd, text=True, capture_output=True)
(a.output / 'compile.stdout').write_text(b.stdout)
(a.output / 'compile.stderr').write_text(b.stderr)
r = None
if b.returncode == 0:
    r = subprocess.run([str((a.output / 'host-wait-controls').resolve())], text=True, capture_output=True)
    (a.output / 'controls.stdout').write_text(r.stdout)
    (a.output / 'controls.stderr').write_text(r.stderr)
result = {
    'scope': 'host-only extracted ploader wait_stable readiness logic; kill/usleep/'
             'kernel_dynlib_*/pt_resolve/clock_gettime mocked at the external call '
             'boundary; simulated-tick clock; no loader assembly/discovery/API/native '
             'execution/console contact',
    'source': str(a.source),
    'has_cache': has_cache,
    'compile_command': cmd,
    'compile_returncode': b.returncode,
    'controls_returncode': None if r is None else r.returncode,
    'stdout': None if r is None else r.stdout,
    'stderr': None if r is None else r.stderr,
}
(a.output / 'receipt.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
raise SystemExit(b.returncode or (r.returncode if r else 1))
