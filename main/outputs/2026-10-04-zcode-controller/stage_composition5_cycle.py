#!/usr/bin/env python3
"""Gated composition-5 build+stage cycle (controller tool, 2026-10-04).

Directives implemented:
- successful builds and gate GO are PREREQUISITES for upload (each step's
  exit code is captured; any nonzero aborts BEFORE upload)
- readback checker rejects empty data / unexpected length, validates the
  expected decrypted representation (content law), exits nonzero on mismatch
  (CHECK FAILED fails the job)
Readback law (validated on sdg2 + comp5 r2..r5 samples): console RETR of the
staged fself returns the decrypted inner ELF truncated just before the
section-header table region, with e_shnum zeroed (offsets 60/61) and the
final 24 bytes zeroed; every other byte is identical to the build's
bare-sdk inner ELF.
"""
import hashlib, json, subprocess, sys, time
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
REBUILD = ROOT / 'outputs/2026-10-04-zcode-team/worker-composition-5-rebuild'
GATE = ROOT / 'outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py'
FLAGS = Path('/tmp/comp5_flags_verified.txt').read_text().split()
BRIDGE_H = 'runtime_start_bridge.h=outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.h'
SLOT = 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/modbridge_supplier_composition.prx'

def step(name, argv, **kw):
    print(f'== {name}: {" ".join(str(a) for a in argv[:6])}...', flush=True)
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True, cwd=ROOT, **kw)
    tail = (r.stdout + r.stderr).strip().splitlines()[-3:]
    for l in tail: print('   |', l[:180])
    if r.returncode != 0:
        print(f'JOB FAILED at {name}: exit {r.returncode}'); sys.exit(1)
    return r

def sh(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    import sys as _sys
    if len(_sys.argv) != 1:
        print('no arguments accepted (guard added after a --help invocation ran the pipeline); '
              'edit constants in-file to retarget'); raise SystemExit(2)

    c1, c2 = REBUILD / 'prx-candidate-composition5-1', REBUILD / 'prx-candidate-composition5-2'
    for d in (c1, c2, REBUILD / 'staging/lineage-overlay-1'):
        if d.exists(): subprocess.run(['rm', '-rf', str(d)])
    drv = REBUILD / 'build_runtime_composition5.py'
    common = ['python3', str(drv), '--reuse-composition-run', '--stage-header', BRIDGE_H]
    for n, out in ((1, c1), (2, c2)):
        subprocess.run(['rm', '-rf', str(REBUILD / 'staging/lineage-overlay-1')])
        step(f'build-{n}', common + FLAGS + ['--out', str(out)])
    s1, s2 = sh(c1 / 'modbridge_supplier_composition.prx'), sh(c2 / 'modbridge_supplier_composition.prx')
    print(f'== determinism: {s1[:16]} vs {s2[:16]}')
    if s1 != s2 or not s1:
        print('JOB FAILED: determinism mismatch'); sys.exit(1)
    g = step('gate', ['python3', str(GATE), '--build-dir', c1, '--compare-dir', c2,
                      '--json', str(GATE.parent / 'gate-receipt-composition5-cycle.json')])
    if 'verdict: GO' not in g.stdout:
        print('JOB FAILED: gate verdict not GO'); sys.exit(1)
    step('upload', ['curl', '-s', '-S', '--connect-timeout', '15', '-T',
                    str(c1 / 'modbridge_supplier_composition.prx'), SLOT])
    time.sleep(15)
    rb = Path('/tmp/cycle-readback.prx')
    step('readback-fetch', ['curl', '-s', '-S', '--connect-timeout', '12', SLOT, '-o', str(rb)])
    bare_p = c1 / 'bare-sdk/modbridge_supplier_composition.prx'
    bare = bare_p.read_bytes(); data = rb.read_bytes()
    import struct
    e_shoff = struct.unpack_from('<Q', bare, 0x28)[0]
    if not data:
        print('CHECK FAILED: readback empty'); sys.exit(1)
    if not (e_shoff - 512 <= len(data) <= e_shoff):
        print(f'CHECK FAILED: readback length {len(data)} outside expected band [{e_shoff-512},{e_shoff}]'); sys.exit(1)
    n = len(data)
    bad = [i for i in range(n) if bare[i] != data[i] and not (60 <= i <= 61) and i < n - 24]
    if bad:
        print(f'CHECK FAILED: {len(bad)} non-artifact byte diffs (first at {bad[0]}: '
              f'{bare[bad[0]]:02x} vs {data[bad[0]]:02x})'); sys.exit(1)
    print(f'== STAGE VERIFIED: len {n} (band ok), content law ok, staged sha {s1}')
    receipt = {'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'staged_sha256': s1,
               'readback_len': n, 'e_shoff': e_shoff, 'bare_sha256': sh(bare_p)}
    (ROOT / 'outputs/2026-10-04-zcode-controller/console/cycle-receipt.json').write_text(json.dumps(receipt, indent=1))
    print('CYCLE COMPLETE')

if __name__ == '__main__':
    main()
