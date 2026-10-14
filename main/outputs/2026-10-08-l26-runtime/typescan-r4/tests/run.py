from pathlib import Path
import subprocess,json
p=Path(__file__).resolve().parent.parent;root=Path.cwd();results=[]
x=subprocess.run(['python3',str(p/'tests/test_scan.py')],capture_output=True,text=True);assert x.returncode==0
x=subprocess.run(['python3',str(p/'tests/test_custom.py')],capture_output=True,text=True);assert x.returncode==0,x.stderr
for name,src in [('custom','actual_custom.cpp'),('policy','test_policy.cpp'),('getters','actual_getters.cpp'),('backend','actual_backend.cpp'),('scan','actual_scan.cpp')]:
 for mode in ['plain','asan-ubsan']:
  binary=p/'tests'/f'{name}-{mode}';a=['clang++','-std=c++20','-include','utility','-I',str(p/'include'),'-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units'),str(p/'tests'/src),'-o',str(binary)]
  if mode!='plain':a+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
  x=subprocess.run(a,capture_output=True,text=True);assert x.returncode==0,x.stderr
  x=subprocess.run([str(binary)],capture_output=True,text=True);r=dict(name=name,mode=mode,exit=x.returncode,stdout=x.stdout,stderr=x.stderr,command=a);results.append(r);print(name,mode,x.returncode,x.stdout.strip());assert x.returncode==0,x.stderr
(p/'results/HOST-TESTS.json').write_text(json.dumps(results,indent=2))

# [Seeds][L26/L27 pin admission, agent-EQ 2026-10-09] dedicated suite: the
# REAL codecache Cache::prepare over the REAL pinned eboot (sha 3b6cfad4...)
# with the six-pin CodeSignatureProfile (l26 profile candidate), RED 3-pin
# control (the 2026-10-09 11:36 L27/Phase-6 refusals), and the Phase-armor
# log-before-rethrow order. Include order mirrors production: profile/units
# candidates first (six-pin CodeSignatureProfile + kMaxPins=8 CodeImageCache),
# candidate-v2 next (ThreadNameProfile for the six-pin header's include).
eq=[];eq_results=[]
for mode in ['plain','asan-ubsan']:
 binary=p/'tests'/f'eqpins-{mode}';a=['clang++','-std=c++20','-I',str(p.parent/'profile'/'units'),'-I',str(root/'outputs/2026-10-08-combined-outer-lifecycle/candidate-v2'),'-I',str(p/'include'),'-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units'),str(p/'tests/actual_eq_pinadmission.cpp'),'-o',str(binary)]
 if mode!='plain':a+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
 x=subprocess.run(a,capture_output=True,text=True);assert x.returncode==0,x.stderr
 x=subprocess.run([str(binary)],capture_output=True,text=True);r=dict(name='eq-pinadmission',mode=mode,exit=x.returncode,stdout=x.stdout,stderr=x.stderr,command=a);eq_results.append(r);print('eq-pinadmission',mode,x.returncode,x.stdout.strip());assert x.returncode==0,x.stderr
(p/'results/HOST-TESTS-L26-EQ-PINADMISSION.json').write_text(json.dumps(eq_results,indent=2))

# [Seeds][L27-supersede, agent-ER 2026-10-09] dedicated suite: the PURE
# IN-PORT FName pool decoder (candidate/src/NameTypes.cpp FNamePoolDecoder
# region) extracted VERBATIM (marker + sha pinned) and exercised against a
# real byte-layout mmap mock pool: known ids (0=None, 3=ByteProperty),
# Number suffix, wide+ANSI entries, refusal shapes, first-use dual-check
# (verify-once log / loud refusal), live-word re-read, and an 8-thread
# first-use race. Suites (happy/refuse/race/truncation) run as separate
# processes because the dual-check state is process-global. The truncation
# suite (agent-ES 2026-10-09) pins the /tmp/dec1.log regression contract:
# long names (20/50/100/500/1023 units, ANSI+wide) decode COMPLETELY,
# block-capacity and live-cursor boundaries refuse only genuinely-OOB
# payloads (units-vs-bytes canaries), and OOB markers fire exactly once
# per genuinely-OOB id and never for successful decodes.
import hashlib
er_dir=p/'results'/'er-pooldecoder-verify';er_regions=er_dir/'regions';er_regions.mkdir(parents=True,exist_ok=True)
nt=(p/'candidate/src/NameTypes.cpp').read_text().splitlines(keepends=True)
start=next(i for i,l in enumerate(nt) if l.strip()=='namespace FNamePoolDecoder')
end=next(i for i,l in enumerate(nt[start:],start) if l.strip()=='} // namespace FNamePoolDecoder')
er_inc=''.join(nt[start:end+1])
assert 'kCurrentBlockOffset = 0x8' in er_inc and 'rc::engtostring::kProbeId' in er_inc and 'try_tostring' in er_inc
(er_regions/'er_pooldecoder.inc').write_text(er_inc)
er_manifest={'source':str(p/'candidate/src/NameTypes.cpp'),'source_sha256':hashlib.sha256((p/'candidate/src/NameTypes.cpp').read_bytes()).hexdigest(),
 'region_lines':[start+1,end+1],'region_sha256':hashlib.sha256(er_inc.encode()).hexdigest()}
(er_dir/'er-region-manifest.json').write_text(json.dumps(er_manifest,indent=1))
er_results=[]
for mode in ['plain','asan-ubsan']:
 binary=p/'tests'/f'er-pooldecoder-{mode}';a=['clang++','-std=c++20','-I',str(er_regions),'-I',str(p/'include'),str(p/'tests/actual_er_pooldecoder.cpp'),'-o',str(binary)]
 if mode!='plain':a+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
 x=subprocess.run(a,capture_output=True,text=True);assert x.returncode==0,x.stderr
 for suite in ['happy','refuse','race','truncation']:
  x=subprocess.run([str(binary),suite],capture_output=True,text=True)
  r=dict(name=f'er-pooldecoder-{suite}',mode=mode,exit=x.returncode,stdout=x.stdout,stderr=x.stderr,command=a)
  er_results.append(r);print(f'er-pooldecoder-{suite}',mode,x.returncode,'FAIL' if x.returncode else 'pass')
  assert x.returncode==0,x.stderr+x.stdout
(er_dir/'HOST-TESTS.json').write_text(json.dumps(er_results,indent=2))
