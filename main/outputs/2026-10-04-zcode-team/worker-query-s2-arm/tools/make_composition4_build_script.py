#!/usr/bin/env python3
"""Generate build_runtime_composition4.py: a copy of worker-composition-3's
build_runtime_composition3.py (itself a bounded extension of the pinned
ue4ss-native-tick-file-owner-1/build_runtime.py) plus ONLY:

  1. --replace-runtime-unit name=source  (bounded to freshly compiled affected
     units; requires --replace-receipt; consumed via the existing new_sources
     hook in the affected-unit loop)
  2. replace-receipt (rr) attestation fallback in the EXTRA-unit dependency
     loop, identical to the bootstrap loop's existing fallback: an unpinned
     .superpowers dependency is admitted ONLY when the replace-receipt
     attests its exact bytes. Without a receipt the behavior is unchanged.
  3. early rr load so the extras loop can consult it (boot block re-reads the
     same file; validation is idempotent).

All frozen gates byte-identical. Writes only inside this worker output dir.
"""
import hashlib
import subprocess
import sys
from pathlib import Path

WHAT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
SRC3 = WHAT / "outputs/2026-10-04-zcode-team/worker-composition-3/build_runtime_composition3.py"
OUT = WHAT / "outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/build"
DST = OUT / "build_runtime_composition4.py"
DIFF = OUT / "composition4-vs-composition3-build-script.diff"

text = SRC3.read_text()

def swap(old, new, label, count=1):
    global text
    n = text.count(old)
    if n != count:
        sys.exit(f"anchor {label}: expected {count} occurrence(s), found {n}")
    text = text.replace(old, new)

swap(
    '''for composition-3: adds the bounded --replace-bootstrap-unit/--replace-receipt
mechanism PROPOSAL (worker-ack-integration-deep controller-patch/02). All frozen
gates unchanged; replacement sources are hash-attested staging copies.''',
    '''for composition-4 (worker-query-s2-arm): keeps the bounded
--replace-bootstrap-unit/--replace-receipt mechanism and adds (a) the equally
bounded --replace-runtime-unit flag for freshly compiled affected runtime units
(only 'unreal-unrealinitializer' is targeted: the S2-arm/disarm/dispatch wiring)
and (b) the replace-receipt attestation fallback in the EXTRA-unit dependency
loop, identical to the bootstrap loop's existing fallback. All frozen gates
unchanged; replacement sources are hash-attested staging copies.''',
    "docstring")

swap(
    '''ap.add_argument('--replace-receipt',type=Path,help='hash-pinned attestation covering replacement sources and every spike project file the replacement touches');a=ap.parse_args()''',
    '''ap.add_argument('--replace-receipt',type=Path,help='hash-pinned attestation covering replacement sources and every spike project file the replacement touches');ap.add_argument('--replace-runtime-unit',action='append',default=[],help='name=source_path; recompile a freshly compiled affected runtime unit from an attested replacement source');a=ap.parse_args()''',
    "argparse")

swap(
    ''' extras=[];extra_pinned=[]
 if a.extra_unit and not a.extend_undefined_baseline''',
    ''' rr={}
 if a.replace_receipt:
  rrd=read(a.replace_receipt)
  for p,h in rrd['files'].items():
   rp=str(Path(p).resolve())
   if not Path(rp).is_file() or sha(rp)!=h:raise ValueError('Replace-receipt hash mismatch: '+p)
   rr[rp]=h
 extras=[];extra_pinned=[]
 if a.extra_unit and not a.extend_undefined_baseline''',
    "early-rr-load")

swap(
    '''    if rp not in pins:
     if rp.startswith(str(S)) or rp.startswith(str(ROOT/'.superpowers')):raise ValueError('Unpinned extra project dependency: '+pd)
     pins[rp]=sha(pd);extra_pinned.append(rp)
    check(pd,pins[rp])''',
    '''    if rp not in pins:
     if rp in rr:pins[rp]=rr[rp]
     elif rp.startswith(str(S)) or rp.startswith(str(ROOT/'.superpowers')):raise ValueError('Unpinned extra project dependency: '+pd)
     else:pins[rp]=sha(pd);extra_pinned.append(rp)
    check(pd,pins[rp])''',
    "extras-rr-fallback")

swap(
    ''' new_sources={'native-ownedmappedscansource':W/'src/OwnedMappedScanSource.cpp','native-loadercontext':W/'src/NativeLoaderContext.cpp','native-programfactory':W/'src/NativeProgramFactory.cpp','native-ownedserializedelfsource':W/'src/OwnedSerializedElfSource.cpp'}''',
    ''' new_sources={'native-ownedmappedscansource':W/'src/OwnedMappedScanSource.cpp','native-loadercontext':W/'src/NativeLoaderContext.cpp','native-programfactory':W/'src/NativeProgramFactory.cpp','native-ownedserializedelfsource':W/'src/OwnedSerializedElfSource.cpp'}
 rt_repl=[]
 for rspec in a.replace_runtime_unit:
  rname,rsep,rsrc=rspec.partition('=')
  if not rsep or rname not in units or rname not in affected or not Path(rsrc).is_file():raise ValueError('Bad replace-runtime-unit spec (target must be a freshly compiled affected unit): '+rspec)
  rt_repl.append((rname,Path(rsrc)))
 if rt_repl and not a.replace_receipt:raise ValueError('Runtime-unit replacement requires --replace-receipt attestation')
 for rname,rsrc in rt_repl:
  if rname in new_sources:raise ValueError('Runtime-unit replacement target already has a staged source: '+rname)
  new_sources[rname]=rsrc''',
    "runtime-replace-block")

swap(
    '''  selected[name]={**old,'name':name,'source':str(source),'source_sha256':sha(source),'object':str(target),'object_sha256':sha(target),'dependency_file':str(dep),'argv':argv,'exit':0};fresh.append(name)
  if name!='native-ownedserializedelfsource':replacements[old['object']]=str(target)''',
    '''  selected[name]={**old,'name':name,'source':str(source),'source_sha256':sha(source),'object':str(target),'object_sha256':sha(target),'dependency_file':str(dep),'argv':argv,'exit':0};fresh.append(name)
  if name!='native-ownedserializedelfsource':replacements[old['object']]=str(target)
 for rname,_ in rt_repl:selected[rname]['runtime_replaced']=True''',
    "runtime-replaced-mark")

DST.parent.mkdir(parents=True, exist_ok=True)
DST.write_text(text)
print("composition-4 build script:", DST)
print("sha256:", hashlib.sha256(text.encode()).hexdigest())
import py_compile
py_compile.compile(str(DST), doraise=True)
print("py_compile: OK")

DIFF.parent.mkdir(parents=True, exist_ok=True)
r = subprocess.run(["diff", "-u", str(SRC3), str(DST)], capture_output=True, text=True)
body = r.stdout.replace(str(SRC3), "a/build_runtime_composition3.py").replace(str(DST), "b/build_runtime_composition4.py")
DIFF.write_text(body)
print("review diff lines:", len(body.splitlines()))
