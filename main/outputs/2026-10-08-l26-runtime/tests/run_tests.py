#!/usr/bin/env python3
"""l25 host tests runner (Claude l25 item 6). Mac-only / host-tested: compiles
test_scan_refusals.cpp against the shipped l25 typescan-r4 guard headers
(unmodified, read-only reference) in plain and ASan+UBSan modes, runs both, and
records RESULTS.json with the header hashes used. No console contact, no live
engine calls, no engine source changes."""
from pathlib import Path
import subprocess,json,hashlib
root=Path(__file__).resolve().parent
include=Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/typescan-r4/include')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
headers={h.name:{'path':str(h),'sha256':sha(h)} for h in sorted(include.glob('Native*.hpp'))+sorted(include.glob('Native*.inl'))}
assert set(headers)=={'NativeTypeScanPolicy.hpp','NativePropertyPointerGuard.hpp','NativePropertyPointerGuard.inl','NativePropertyReadBackend.inl'},set(headers)
results=[]
for mode in ('plain','asan-ubsan'):
    binary=root/f'test_scan_refusals-{mode}'
    argv=['clang++','-std=c++20','-I'+str(include),str(root/'test_scan_refusals.cpp'),'-o',str(binary)]
    if mode!='plain':argv+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
    c=subprocess.run(argv,capture_output=True,text=True)
    r={'name':'scan_refusals','mode':mode,'compile':{'argv':argv,'exit':c.returncode,'stderr':c.stderr}}
    assert c.returncode==0,c.stderr
    x=subprocess.run([str(binary)],capture_output=True,text=True)
    r.update({'exit':x.returncode,'stdout':x.stdout,'stderr':x.stderr})
    results.append(r);print(mode,x.returncode,x.stdout.strip())
    assert x.returncode==0,x.stderr
(root/'RESULTS.json').write_text(json.dumps({'status':'PASS_HOST_ONLY_MAC','label':'Mac-only / host-tested','environment':'macOS arm64 host clang++; no console contact; no live engine calls','suite':'Claude l25 item 6: (a) unknown-yet *Property class passes bootstrap scan mode without CastFlags; (b) bad class pointer refused with reason/field-address/class-pointer details; (c) refusal does not stop the scan; (d) scan mode resets after an exception via RAII scope','tested_headers':headers,'headers_unmodified':True,'results':results,'notes':'ScanCollectionScope is a host-side RAII pattern over the shipped Begin/EndScanCollection API; the counter-control documents that the l24 candidate TypeChecker pairs Begin/End without RAII, so an escaping exception would leave scan mode stuck — adopt the scope in the next runtime revision.'},indent=2)+'\n')
print('PASS l25 item-6 host tests: plain + asan-ubsan')
