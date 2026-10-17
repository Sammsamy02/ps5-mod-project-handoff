"""Compile the actual selected header using its retained SDK compile recipe."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[2]
COMPOSITION = ROOT/'outputs/2026-10-08-menu-complete-build/pass1/COMPOSITION-RECEIPT.json'
parser = argparse.ArgumentParser()
parser.add_argument('--name', required=True)
parser.add_argument('--candidate', action='store_true')
parser.add_argument('--overlay', action='store_true')
parser.add_argument('--expect-failure', action='store_true')
parser.add_argument('--emit-ir', action='store_true')
args = parser.parse_args()
unit = next(u for u in json.loads(COMPOSITION.read_text())['selected_units'] if u['name']=='unreal-uobjectglobals')
command = []
skip = False
for arg in unit['argv']:
    if skip:
        skip = False
        continue
    if arg in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
        continue
    if arg in ('-c', '-MD', '-MMD') or arg==unit['source']:
        continue
    command.append(arg)
if args.candidate:
    command[1:1] = ['-I', str(BASE/'candidate')]
if args.overlay:
    if args.candidate:
        parser.error('--candidate and --overlay are mutually exclusive')
    selected_header = ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectGlobals.hpp'
    overlay = BASE/'probe-overlay.json'
    overlay.write_text(json.dumps({'version': 0, 'case-sensitive': 'true',
                                  'use-external-names': False,
                                  'roots': [{'type': 'file', 'name': str(selected_header),
                                             'external-contents': str(BASE/'candidate/Unreal/UObjectGlobals.hpp')}]}, indent=2)+'\n')
    command[1:1] = ['-ivfsoverlay', str(overlay)]
source = BASE/'tests/sco_layout.cpp'
run = BASE/'tests'/args.name
run.mkdir(parents=True, exist_ok=True)
if args.emit_ir:
    command += ['-S', '-emit-llvm', '-o', str(run/'sco_layout.ll')]
else:
    command += ['-fsyntax-only']
command += ['-MD', '-MF', str(run/'sco_layout.d'), str(source)]
environment = dict(os.environ, LLVM_CONFIG=unit['LLVM_CONFIG'])
result = subprocess.run(command, env=environment, cwd=ROOT, capture_output=True, text=True)
(run/'stdout.txt').write_text(result.stdout)
(run/'stderr.txt').write_text(result.stderr)
expected_failure = result.returncode != 0 and 'SCO parameter alignment must be 16' in result.stderr and '8 == 16' in result.stderr
passed = expected_failure if args.expect_failure else result.returncode==0
record = dict(name=args.name, command=command, LLVM_CONFIG=unit['LLVM_CONFIG'], returncode=result.returncode,
              expectation='alignment assertion rejects original' if args.expect_failure else 'compile succeeds',
              passed=passed, stderr_path=str(run/'stderr.txt'), actual_header_dependency=None,
              source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), target='x86_64-sie-ps5',
              execution=False, no_link=True,
              overlay_path=str(overlay) if args.overlay else None,
              overlay_sha256=hashlib.sha256(overlay.read_bytes()).hexdigest() if args.overlay else None)
dependency = run/'sco_layout.d'
if dependency.exists():
    record['actual_header_dependency'] = [line.strip().rstrip('\\').strip() for line in dependency.read_text().splitlines() if line.strip().endswith('UObjectGlobals.hpp \\') or line.strip().endswith('UObjectGlobals.hpp')]
(run/'receipt.json').write_text(json.dumps(record, indent=2)+'\n')
print(json.dumps({k:record[k] for k in ('name','returncode','expectation','passed','stderr_path','actual_header_dependency')}))
raise SystemExit(0 if passed else 1)
