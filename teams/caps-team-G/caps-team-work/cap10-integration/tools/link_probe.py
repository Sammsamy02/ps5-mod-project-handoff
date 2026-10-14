"""Mac-only partial link (ld.lld -r) of R15e's exact link command with the #10
units/artifacts wired exactly as cap10_integration.wire_link does (owner/
boundary/offline-net objects + static corlib + Mono archive injected before
the first inherited fixed archive) and the program object replaced.
Output stays under --out; it is an analysis object, never packaged.
Then: llvm-nm -u of the result vs R15e's pass1/undefined.stdout.
usage: link_probe.py --program NEW_PROGRAM.o --cap10-obj O ... --artifact A ... --out DIR
"""
import argparse, json, subprocess, sys, time, re, hashlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from target_compile import find_r15e, wait_memory, LLVM, sha, BV
ap = argparse.ArgumentParser()
ap.add_argument('--program', required=True)
ap.add_argument('--cap10-obj', action='append', default=[])
ap.add_argument('--artifact', action='append', default=[])
ap.add_argument('--out', required=True)
a = ap.parse_args()
out = Path(a.out).resolve(); out.mkdir(parents=True, exist_ok=True)
r15e = find_r15e()
lc = json.loads((r15e / 'pass1/LINK-COMMAND.json').read_text())
argv = [x.replace(str(BV / 'l26-A'), str(r15e)) if x.startswith(str(BV / 'l26-A')) else x for x in lc['argv']]
old_prog = str(r15e / 'pass1/objects/program.o')
if argv.count(old_prog) != 1: raise SystemExit('FAIL: program.o not exactly once in the R15e link')
argv[argv.index(old_prog)] = str(Path(a.program).resolve())
fixed = [r['path'] for r in lc['fixed_inputs']]
anchor = next(p for p in fixed if p.endswith('.a'))
at = argv.index(anchor)
inject = [str(Path(p).resolve()) for p in a.cap10_obj + a.artifact]
argv[at:at] = inject
o = argv.index('-o'); argv[o + 1] = str(out / 'runtime-cap10.o')
argv = ['-Map=' + str(out / 'runtime.map') if x.startswith('-Map=') else x for x in argv]; argv[1:1] = ['--threads=2']
inputs = [x for i, x in enumerate(argv) if x.endswith(('.o', '.a')) and i != argv.index('-o') + 1]
(out / 'LINK-COMMAND.json').write_text(json.dumps({'argv': argv, 'inputs': len(inputs), 'r15e_inputs': len([x for x in lc['argv'] if x.endswith(('.o','.a'))]) - 1, 'injected': inject, 'anchor': anchor}, indent=1))
print(f'  R15e link: {r15e}/pass1/LINK-COMMAND.json; inputs {len(inputs)} (R15e had {len([x for x in lc["argv"] if x.endswith((".o",".a"))]) - 1}); injected {len(inject)} before {Path(anchor).name}')
fb = wait_memory('link')
t0 = time.time()
r = subprocess.run(argv, capture_output=True, text=True)
(out / 'link.stderr').write_text(r.stderr); (out / 'link.stdout').write_text(r.stdout)
print(f'  ld.lld -r exit={r.returncode} {time.time()-t0:.0f}s free-at-start={fb/1e9:.1f}GB')
if r.returncode != 0: print(r.stderr[-3000:]); sys.exit(1)
u = subprocess.run([str(LLVM / 'llvm-nm'), '-u', str(out / 'runtime-cap10.o')], capture_output=True, text=True)
(out / 'undefined.stdout').write_text(u.stdout)
rows = lambda s: {tuple(l.split()[-2:]) for l in s.splitlines() if l.split()}
before = rows((r15e / 'pass1/undefined.stdout').read_text()); after = rows(u.stdout)
added = sorted(after - before); removed = sorted(before - after)
res = {'r15e_undefined': len(before), 'cap10_undefined': len(after), 'added': [list(x) for x in added], 'removed': [list(x) for x in removed],
       'runtime_object': {'path': str(out / 'runtime-cap10.o'), 'sha256': sha(out / 'runtime-cap10.o')}}
(out / 'RAW-DELTA.json').write_text(json.dumps(res, indent=1))
print(f'  raw undefined: R15e {len(before)} -> cap10 {len(after)}; added {len(added)} (U {sum(1 for t,_ in added if t=="U")}, weak {sum(1 for t,_ in added if t!="U")}); removed {len(removed)}')
if not after or not before: print('FAIL: zero symbols'); sys.exit(2)
