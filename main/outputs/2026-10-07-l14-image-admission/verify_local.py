from pathlib import Path
import concurrent.futures,datetime,hashlib,json,struct,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');out=HERE/'receipts'/('verification-'+stamp);out.mkdir()
def run(name,commands):
 records=[]
 for argv in commands:
  argv=['/usr/bin/sandbox-exec','-p','(version 1) (allow default) (deny network*)']+[str(a) for a in argv]
  r=subprocess.run(argv,cwd=ROOT,capture_output=True,text=True);records.append(dict(argv=argv,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:break
 report=dict(name=name,passed=all(r['exit_code']==0 for r in records),commands=records)
 (out/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report
compile=lambda name:['clang++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(HERE/'units'),HERE/('tests/'+name+'.cpp'),'-o',HERE/('tests/'+name)]
before={str(p.relative_to(HERE)):sha(p) for p in (HERE/'units').iterdir() if p.is_file()}
tasks=[('range-engine',[compile('admission_engine_test'),[HERE/'tests/admission_engine_test']]),
 ('image-pool',[compile('image_pool_test'),[HERE/'tests/image_pool_test']]),
 ('actual-initializer',[[sys.executable,HERE/'tests/initializer_integration.py']]),
 ('actual-bridge',[[sys.executable,HERE/'tests/bridge_segments.py']]),
 ('sink-lifetime',[[sys.executable,HERE/'sink-tests/run_sink_tests.py']]),
 ('ps5-compile',[[sys.executable,HERE/'compile_units.py']])]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:reports=list(pool.map(lambda task:run(*task),tasks))
baseline=json.loads((HERE/'baseline.json').read_text());pres=baseline['preserve'];preserved=[]
for r in pres['baseline']+pres['untouched_pad']+[pres['frozen_prx']]:
 preserved.append(dict(path=r['path'],expected=r['sha256'],actual=sha(ROOT/r['path'])))
# Reparse the original captured ELF and require every PT_LOAD field to match the generated profile.
image=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
data=image.read_bytes();image_sha=hashlib.sha256(data).hexdigest();assert image_sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
phoff=struct.unpack_from('<Q',data,32)[0];size,count=struct.unpack_from('<HH',data,54);loads=[]
for i in range(count):
 typ,flags,off,vaddr,paddr,filesz,memsz,align=struct.unpack_from('<IIQQQQQQ',data,phoff+i*size)
 if typ==1:loads.append(dict(index=i,flags=flags,vaddr=vaddr,filesz=filesz,memsz=memsz))
prof=json.loads((ROOT/'outputs/2026-10-04-zcode-team/worker-geometry-reference/phdrs-derived.json').read_text())
assert loads==[dict(index=p['index'],flags=p['p_flags'],vaddr=p['p_vaddr'],filesz=p['p_filesz'],memsz=p['p_memsz']) for p in prof['phdrs'] if p['p_type']==1]
(out/'profile-provenance.json').write_text(json.dumps(dict(image_sha256=image_sha,loads=loads,generated_header_sha256=sha(HERE/'units/ImageLoadProfile.hpp'),scope='Recorded file headers; runtime additionally requires same-base live segment/mapping evidence'),indent=2)+'\n')
checks=dict(commands_pass=all(r['passed'] for r in reports),sources_stable=before=={str(p.relative_to(HERE)):sha(p) for p in (HERE/'units').iterdir() if p.is_file()},pad_and_frozen_preserved=all(r['expected']==r['actual'] for r in preserved),l13_sources_preserved=all(sha(ROOT/p)==h for p,h in baseline['l13_sources'].items()),profile_matches_captured_elf=True)
g=dict(status='PASS_MAC_ONLY' if all(checks.values()) else 'FAIL',checks=checks,source_hashes=before,preserved=preserved,receipts=[str(out/(r['name']+'.json')) for r in reports],console_contact=False,console_proven=False,upload_gate='HOLD: awaiting separate approval after full pipeline')
(out/'GATES.json').write_text(json.dumps(g,indent=2)+'\n');(HERE/'receipts/LATEST-GATES.json').write_text(json.dumps(g,indent=2)+'\n')
for r in reports:
 print(r['name'],'PASS' if r['passed'] else 'FAIL',flush=True)
 if not r['passed']:
  print(r['commands'][-1]['stdout'][-2000:],r['commands'][-1]['stderr'][-4000:])
print(g['status'],out/'GATES.json');sys.exit(0 if all(checks.values()) else 1)
