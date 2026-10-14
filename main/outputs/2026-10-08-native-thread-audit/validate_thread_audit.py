from pathlib import Path
import struct,json,hashlib,copy,subprocess,os
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def checkpin(p):assert Path(p['path']).stat().st_size==p['bytes'] and sha(p['path'])==p['sha256'],p['path']
s=json.loads((OUT/'THREAD-SUPPORT.json').read_text());checkpin(s['image']);b=Path(s['image']['path']).read_bytes()
def checkwindows(w):
 for x in w['instruction_evidence'].values():assert b[int(x['file_offset'],16):int(x['file_offset'],16)+len(bytes.fromhex(x['bytes']))]==bytes.fromhex(x['bytes']),x['image_vaddr']
 for x in w['data_evidence']:assert b[int(x['file_offset'],16):int(x['file_offset'],16)+len(bytes.fromhex(x['bytes']))]==bytes.fromhex(x['bytes']),x['value']
w=json.loads((OUT/'THREAD-CONTRACT-WINDOWS.json').read_text());checkwindows(w)
r=json.loads((OUT/'THREAD-CONTRACT-REFS.json').read_text())
for x in r['scalar_rip_refs']:
 raw=bytes.fromhex(x['bytes']);assert b[int(x['file_offset'],16):int(x['file_offset'],16)+len(raw)]==raw
for x in r['import_evidence']:
 for key in ['relocation','symbol']:
  raw=bytes.fromhex(x[key+'_bytes']);assert b[int(x[key+'_file_offset'],16):int(x[key+'_file_offset'],16)+len(raw)]==raw
 assert x['name']=='EI-5-jlq2dE#5#J'
lib=json.loads((OUT/'LIBKERNEL-IMPORT-IDENTITY.json').read_text());checkpin(lib['local_format_reference'])
for x in lib['native_dynamic_records']:
 assert b[int(x['file_offset'],16):int(x['file_offset'],16)+16]==bytes.fromhex(x['bytes'])
 assert b[int(x['name_file_offset'],16):int(x['name_file_offset'],16)+len(bytes.fromhex(x['name_bytes']))]==bytes.fromhex(x['name_bytes'])
for p in s['source_inputs']+s['compiler_dependency_pins']:checkpin(p)
checkpin(s['target_adapter_command'])
adapter_command=json.loads(Path(s['target_adapter_command']['path']).read_text());assert adapter_command['exit_code']==0
adapter_object=Path(adapter_command['argv'][adapter_command['argv'].index('-o')+1]);ab=adapter_object.read_bytes();ao=struct.unpack_from('<Q',ab,40)[0];ae,an,ast=struct.unpack_from('<HHH',ab,58);asecs=[struct.unpack_from('<IIQQQQIIQQ',ab,ao+i*ae) for i in range(an)];undefined=[]
for sec in asecs:
 if sec[1]!=2:continue
 names=ab[asecs[sec[6]][4]:asecs[sec[6]][4]+asecs[sec[6]][5]]
 for at in range(sec[4],sec[4]+sec[5],sec[9]):
  no,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',ab,at)
  if idx==0:undefined.append(names[no:names.index(0,no)].decode())
assert 'scePthreadGetthreadid' in undefined and 'pthread_getthreadid_np' not in undefined
t=json.loads((OUT/'TARGET-THREAD-CENSUS.json').read_text())
for p in t['inputs']+[t['command'],t['object']]:checkpin(p)
assert {x['key']:x['value'] for x in t['facts']}=={'pointer.size':'0x8','pthread_t.size':'0x8','pthread_t.align':'0x8','pthread_t.is_pointer':'0x1','std.thread_id.size':'0x8','std.thread_id.align':'0x8','pthread_self.same_pthread_t':'0x1','engine.consumed_id32.size':'0x4','int.size':'0x4','id32.getter_pointer.size':'0x8'}
assert any(x['name']=='pthread_self' and x['section_index']==0 for x in t['symbols'])
# Decode the actual compiler object, not only generated JSON metadata.
o=Path(t['object']['path']).read_bytes();shoff=struct.unpack_from('<Q',o,40)[0];se,sn,st=struct.unpack_from('<HHH',o,58);ss=[struct.unpack_from('<IIQQQQIIQQ',o,shoff+i*se) for i in range(sn)];found=None
for sec in ss:
 if sec[1]!=2:continue
 names=o[ss[sec[6]][4]:ss[sec[6]][4]+ss[sec[6]][5]]
 for at in range(sec[4],sec[4]+sec[5],sec[9]):
  no,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',o,at)
  if names[no:names.index(0,no)]==b'ThreadFacts':found=struct.unpack_from('<10Q',o,ss[idx][4]+val)
assert found==(8,8,8,1,8,8,1,4,4,8)
p=json.loads((OUT/'THREAD-PROFILE-CANDIDATE.json').read_text())
def checkcandidate(p):
 assert p['image_sha256']==s['image']['sha256'] and not p['live_observation'] and not p['selected_profile_promoted']
 assert [(x['key'],x['image_vaddr'],x['width_bytes']) for x in p['facts']]==[('GameThread.Id32','0x9358a98',4),('GameThread.IdInitialized','0x9358aa4',1),('CurrentNativeThreadId.GetterPLT','0x74723a0',None)]
 assert p['facts'][1]['accepted_ready_value']==1
 assert p['facts'][2]['declaration']=='int32_t scePthreadGetthreadid(void);'
 assert p['readiness_predicate']['preinit_returns_true'] and not p['readiness_predicate']['use_as_strict_authentication']
 assert not p['writer_census']['all_writers_proven'] and p['writer_census']['computed_aliases_unresolved']
 for x in p['evidence_inputs']:checkpin(x)
checkcandidate(p)
controls=[]
for name,change in [('wrong-ID-address',lambda x:x['facts'][0].update(image_vaddr='0x9358a9c')),('wrong-ID-width',lambda x:x['facts'][0].update(width_bytes=8)),('ready-zero',lambda x:x['facts'][1].update(accepted_ready_value=0)),('wrong-getter-prototype',lambda x:x['facts'][2].update(declaration='void* pthread_self(void);')),('preinit-predicate-bypass',lambda x:x['readiness_predicate'].update(use_as_strict_authentication=True))]:
 q=copy.deepcopy(p);change(q)
 try:checkcandidate(q)
 except AssertionError:controls.append(dict(name=name,refused=True))
 else:raise AssertionError(name+' unexpectedly accepted')
q=copy.deepcopy(w);q['instruction_evidence']['0x3a14e9c']['bytes']='9001'
try:checkwindows(q)
except AssertionError:controls.append(dict(name='wrong-native-store-byte',refused=True))
else:raise AssertionError('wrong native byte accepted')
env=os.environ.copy();env['ASAN_OPTIONS']='halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1';z=subprocess.run([str(OUT/'policy/strict-policy-green-final')],text=True,capture_output=True,env=env);assert z.returncode==0 and 'tests=27 failures=0' in z.stdout and not z.stderr,(z.returncode,z.stdout,z.stderr)
if (OUT/'SOURCE-SELECTION.json').exists():
 for x in json.loads((OUT/'SOURCE-SELECTION.json').read_text())['files']:checkpin(x)
print(json.dumps(dict(pass_result=True,native_instruction_windows=len(w['instruction_evidence']),native_strings=len(w['data_evidence']),scalar_reference_windows=len(r['scalar_rip_refs']),compiler_dependency_pins=len(s['compiler_dependency_pins']),target_census_facts=10,target_adapter_exit=0,host_tests=27,host_failures=0,mutation_controls=controls,live_proof=False,profile_promoted=False)))
