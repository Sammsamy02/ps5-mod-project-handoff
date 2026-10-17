from pathlib import Path
import json,hashlib,shlex,difflib,struct
w=Path(__file__).resolve().parent;root=w.parent.parent;prior=root/'outputs/2026-10-08-native-thread-retire-entry'
def digest(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def pin(p):
 p=Path(p).resolve();return dict(path=str(p),size=p.stat().st_size,sha256=digest(p))
def save(p,d):(w/p).write_text(json.dumps(d,indent=2)+'\n')
old=(prior/'candidate/ThreadNameRuntime.cpp').read_text();new=(w/'candidate/ThreadNameRuntime.cpp').read_text()
expected=old
for order in ['release','acquire','acq_rel']:expected=expected.replace('std::memory_order_'+order,'std::memory_order_seq_cst')
expected=expected.replace('// Trivial, constant-initialized guards outlive the managed owner state.','// Trivial, constant-initialized guards outlive the managed owner state.\n// Both phase and entry count use one SC order; see SC-ORDERING.md.')
assert new==expected
assert new.count('std::memory_order_seq_cst')==10
assert (w/'candidate/NativeThreadAuthorityRetire.h').read_bytes()==(prior/'candidate/NativeThreadAuthorityRetire.h').read_bytes()
(w/'PROVIDER-SC.patch').write_text(''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile=str(prior/'candidate/ThreadNameRuntime.cpp'),tofile=str(w/'candidate/ThreadNameRuntime.cpp'))))
run=(w/'tests/GREEN-controls/run.log').read_text();assert 'main-checks=8 failures=0' in run and 'PASS postconstructor' in run
commands=json.loads((w/'target/commands.json').read_text());assert all(r['exit']==0 for r in commands)
assert len([l for l in (w/'target/DEFINED.stdout').read_text().splitlines() if l.startswith('bridge_native_thread_authority_retire T ')])==1
obj=w/'target/ThreadNameRuntime.o';b=obj.read_bytes();assert b[:6]==b'\x7fELF\x02\x01'
off=struct.unpack_from('<Q',b,40)[0];ents,n,si=struct.unpack_from('<HHH',b,58)
ss=[struct.unpack_from('<IIQQQQIIQQ',b,off+i*ents) for i in range(n)];st=ss[si];strings=b[st[4]:st[4]+st[5]]
names={strings[s[0]:strings.find(b'\0',s[0])].decode():s for s in ss}
sets=[
 ('.text._ZN16NativeNameThread12_GLOBAL__N_115RuntimeLifetimeC2Ev',[('ready store',0x18,'edx',0x1d)]),
 ('.text._ZN16NativeNameThread12_GLOBAL__N_115RuntimeLifetimeD2Ev',[('closing store',0x18,'edx',0x1d),('entry count load',0x29,'esi',0x2e),('after destruction store',0x50,'edx',0x55)]),
 ('.text._ZN16NativeNameThread12_GLOBAL__N_112retire_entryEv',[('first phase load',0xf,'esi',0x14),('counter add',0x33,'edx',0x38),('second phase load',0x44,'esi',0x49),('refused counter decrement',0x5f,'edx',0x64),('completed counter decrement',0x95,'edx',0x9a)])]
windows=[]
for name,ops in sets:
 s=names[name];raw=b[s[4]:s[4]+s[5]];items=[]
 for meaning,at,reg,call in ops:
  expected=bytes([0xba if reg=='edx' else 0xbe,5,0,0,0]);assert raw[at:at+5]==expected and raw[call]==0xe8
  items.append(dict(meaning=meaning,order_argument_register=reg,order_value=5,instruction_offset=hex(at),bytes=expected.hex(),call_offset=hex(call)))
 windows.append(dict(section=name,file_offset=s[4],size=s[5],bytes=raw.hex(),operations=items))
save('TARGET-SC-ORDERING.json',dict(classification='actual SDK source-contract compiler control, not native engine measurement',object=pin(obj),gate_operations=9,windows=windows,full_disassembly=pin(w/'target/DISASSEMBLY.stdout')))
save('HANDOFF.json',dict(status='CORRECTED_IMPLEMENTATION_ONLY_NO_INDEPENDENT_ACCEPTANCE',replacement_provider=pin(w/'candidate/ThreadNameRuntime.cpp'),supersedes_provider=pin(prior/'candidate/ThreadNameRuntime.cpp'),c_header=pin(w/'candidate/NativeThreadAuthorityRetire.h'),identical_prior_header=pin(prior/'candidate/NativeThreadAuthorityRetire.h'),c_abi='int bridge_native_thread_authority_retire(void): 0complete/absent initialized,1busy/self refusal,-1unknown pre/late',unchanged_interfaces=[pin(root/'outputs/2026-10-08-native-thread-name-adapter/candidate/ThreadNameRuntime.hpp'),pin(root/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealInitializer.hpp')],sc_ordering_proof=pin(w/'SC-ORDERING.md'),target_ordering=pin(w/'TARGET-SC-ORDERING.json'),target_recipe=pin(w/'target/commands.json'),target_object=pin(obj),include_prefix=[str(w/'candidate')],outer_requirements='stop new preparation/publication/callback admission; obtain0 before fini/resource release; every nonzero retains allresources/code; constructor guard does not substitute for callback retirement',prior_header_and_c_caller_reuse_allowed=True,selected_source_changes=False,outer_wiring=False))
(w/'REPORT.md').write_text('''This additive revision corrects the two-atomic retirement entry gate. The earlier sealed acquire/release provider remains unchanged and held; its behavioral controls did not establish a cross-atomic close/admission ordering proof. This provider changes only the nine phase/counter memory-order arguments to seq_cst and adds a source comment identifying the proof. The helper cancellation/wait logic, public C declaration, runtime facade header, Config, and status ABI remain byte-identical.

SC-ORDERING.md gives the ordering argument. If the entrant's second Ready load precedes Closing, its counter addition precedes the destructor's counter zero-read and remains counted until owner access finishes. If Closing precedes that second load, it observes a closed phase and refuses without touching managed state. The constructor Ready store publishes completed owner construction. The marker closes and waits before managed state destruction. Pre/late states still return-1, self-wait returns1, and every nonzero retains resources/code. Root still owns freezing new preparation/callback entry, successful retirement, and only then fini/release; this correction is not a new outer lifecycle or unload authorization.

Nine focused host controls pass again with fatal ASan/UBSan, using actual corrected runtime/binder/policy bodies and the previously explicit host Config/supplier/native-identity fixtures. This is behavioral regression coverage, not exhaustive weak-memory execution proof or native authority. One fresh actual SDK provider TU compiles against the full selected target headers. Exact object sections show the seq_cst argument value5 at all nine phase/counter calls; TARGET-SC-ORDERING.json pins those instructions and the full disassembly. Two inherited Atomic.hpp switch warnings remain recorded. The unchanged C declaration/caller may be retained from the prior target compile because its header bytes and ABI are identical.

Only candidate/ThreadNameRuntime.cpp is a replacement provider. HANDOFF.json supplies its immutable source/object/recipe pins and the identical declaration header. No selected sources, profiles, imports, package, console, original mod, or prior freezes were changed. No independent review acceptance or native execution is claimed. Verify with python3 outputs/2026-10-08-native-thread-retire-entry-sc/verify.py; producer reruns must use new owned output directories.
''')
external=set()
for p in [w/'target/ThreadNameRuntime.d',w/'tests/GREEN-controls/controls.d']:
 text=p.read_text().replace('\\\n',' ')
 external.update(Path(q).resolve() for q in shlex.split(text.split(':',1)[1]) if not Path(q).resolve().is_relative_to(w))
external.update([prior/'SOURCE-SELECTION.json',prior/'FINAL-RECEIPT.json',prior/'candidate/ThreadNameRuntime.cpp',prior/'candidate/NativeThreadAuthorityRetire.h',prior/'target/commands.json',prior/'target/target_c_entry.o',prior/'tests/GREEN-controls/commands.json',prior/'tests/lifetime_probe.cpp',prior/'tests/retire_controls.cpp',root/'outputs/2026-10-08-native-thread-name-adapter/candidate/NativeThreadNameAdapter.cpp',root/'outputs/2026-10-08-native-thread-name-adapter/candidate/NameThreadPolicy.cpp'])
for r in commands:
 external.add(Path(r['argv'][0]).resolve())
 if 'LLVM_CONFIG' in r:external.add(Path(r['LLVM_CONFIG']).resolve())
external.add(Path('/usr/bin/clang++').resolve())
files=[p for p in w.rglob('*') if p.is_file() and p.name not in ['SOURCE-SELECTION.json','FINAL-RECEIPT.json','VERIFICATION.json']]
save('SOURCE-SELECTION.json',dict(scope='NEW seq_cst correction only; prior freeze unchanged',owned_files=[pin(p) for p in sorted(files)],external_files=[pin(p) for p in sorted(external)],unchanged_prior_declaration=True))
save('FINAL-RECEIPT.json',dict(status='SEALED_CORRECTED_IMPLEMENTATION_REQUIRES_ROOT_INTEGRATION',selection=pin(w/'SOURCE-SELECTION.json'),handoff=pin(w/'HANDOFF.json'),provider=pin(w/'candidate/ThreadNameRuntime.cpp'),header=pin(w/'candidate/NativeThreadAuthorityRetire.h'),sc_proof=pin(w/'SC-ORDERING.md'),target_control=pin(w/'TARGET-SC-ORDERING.json'),host_controls=9,host_failures=0,target_compiles=1,sc_gate_operations=9,header_unchanged=True,helper_wait_unchanged=True,independent_acceptance=False,selected_source_changes=False,native_execution=False,outer_wiring=False))
print(json.dumps(dict(final=digest(w/'FINAL-RECEIPT.json'),selection=digest(w/'SOURCE-SELECTION.json'),provider=digest(w/'candidate/ThreadNameRuntime.cpp'),header=digest(w/'candidate/NativeThreadAuthorityRetire.h'),owned=len(files),external=len(external))))
