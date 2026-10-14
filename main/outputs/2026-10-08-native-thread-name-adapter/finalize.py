from pathlib import Path
import hashlib, json, shlex, struct, difflib

W = Path(__file__).resolve().parent
ROOT = W.parent.parent
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()
def pin(p):
    p=Path(p).resolve(); return dict(path=str(p),sha256=sha(p),size=p.stat().st_size)
def save(p,d): (W/p).write_text(json.dumps(d,indent=2)+'\n')
def elf_symbol(p,name):
    b=Path(p).read_bytes(); assert b[:6]==b'\x7fELF\x02\x01'
    off=struct.unpack_from('<Q',b,40)[0]; ents,n=struct.unpack_from('<HH',b,58)
    ss=[struct.unpack_from('<IIQQQQIIQQ',b,off+i*ents) for i in range(n)]
    for s in ss:
        if s[1]!=2: continue
        st=ss[s[6]]; strings=b[st[4]:st[4]+st[5]]
        for k in range(0,s[5],s[9]):
            no,info,other,sec,v,size=struct.unpack_from('<IBBHQQ',b,s[4]+k)
            nm=strings[no:strings.find(b'\0',no)].decode()
            if nm==name:
                q=ss[sec]; pos=q[4]+v-q[3]
                return b[pos:pos+size],dict(section_index=sec,file_offset=pos,symbol_size=size)
    raise AssertionError(name)

native=json.loads((W/'evidence/NATIVE-WINDOWS.json').read_text())
assert len(native['instructions'])==672
obj=W/'target-wiring-v2/TargetAbiCensus.o'
buf,where=elf_symbol(obj,'NAME_ABI_CENSUS')
values=list(struct.unpack('<10Q',buf)); assert values==[4,4,8,8,16,8,0,1,33,3]
old=json.loads((W/'target/ABI-CENSUS.json').read_text())
save('target-wiring-v2/ABI-CENSUS.json',dict(symbol='NAME_ABI_CENSUS',object=pin(obj),**where,values=values,semantics=old['semantics'],classification='actual SDK target object; source-contract ABI control, no native execution'))

packet=ROOT/'outputs/2026-10-08-native-thread-import-evidence/IMPORT-EVIDENCE.json'
assert sha(packet)=='5a02334cf74209c7487b2fc8c3440882ab0f0e590d9dc8baf73b77b5c2ade4f5'
prefix=bytes.fromhex('554889e54156530fb605eac79d0884c074530fb605f7c79d0884c00f849d000000')
assert len(prefix)==33
save('NATIVE-NAME-FACTS.json',dict(
 status='REVIEW_PENDING_IMAGE_BOUND_CANDIDATE_NO_NATIVE_EXECUTION',
 image=dict(path=str(ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'),sha256=native['image_sha256'],size=194241424,extent=197526216,authentication='collect_native.py hashed the complete saved image; real host cache tests independently authenticated it once per process'),
 native_evidence=pin(W/'evidence/NATIVE-WINDOWS.json'),
 accessor=dict(image_vaddr='0xa262e0',complete_bytes=283,ordinary_return_sites=['0xa26317','0xa2632e','0xa26344'],input='native ID32 in EDI; no receiver or hidden result buffer',output='address in RAX; plain one-argument callable ABI. const FString& source spelling is ABI-correlated inference, not recovered source',prefix_bytes=prefix.hex(),prefix_size=33,code_pin_id=3,code_pin_name='engine_thread_name_by_id32',target_control=pin(W/'target-wiring-v2/ABI-CENSUS.json')),
 branches=[
  dict(predicate='EDI == dword [image+0x9358a98]',window=['0xa26301','0xa26308','0xa2630a','0xa2630c','0xa26317'],return_address='0x9402ac8',name='GameThread',literal='0x76d6dec',initialization_window=['0xa2635e','0xa26365','0xa2636f']),
  dict(predicate='otherwise EDI == dword [image+0x9358a9c]',window=['0xa26318','0xa2631f','0xa26321','0xa26323','0xa2632e'],return_address='0x9402ae0',name='RenderThread',literal='0x765beba'),
  dict(predicate='otherwise',window=['0xa2632f','0xa26331','0xa26336','0xa26339','0xa2633b','0xa26344'],return_address='BD500(manager, ID32) result',fallback='default static empty FString 0x932a5a0, or registered object pointer +8',manager_window=['0xbd53d','0xbd59b','0xbd5b0','0xbd5b4','0xbd5ca','0xbd5e5'])],
 caller=dict(window=['0xa0a78b','0xa0a79a','0xa0a7a2','0xa0a7af','0xa0a7b2'],input='ID dword loaded into EDI',output='native code subsequently reads qword [RAX] and dword [RAX+8]'),
 segments=native['segments'],
 getter_import=dict(evidence=pin(packet),declaration='extern "C" int32_t scePthreadGetthreadid(void)',prototype_confidence='ABI-correlated from native no-argument calls/EAX32 flow plus saved third-party header; not an official PS5 header claim',nid='EI-5-jlq2dE#5#J',module='libkernel',module_version=257,library='libkernel',library_version=1,sdk_symbol='scePthreadGetthreadid T 1218c 0',package_admission=False,native_resolution=False,sibling_pthread_getthreadid_np_excluded=True),
 accepted_candidate_semantics='positive stable current native ID32; numerical return pointer equals authenticated image base + GameThread storage; opaque64 affinity retained separately only after this check',
 qualifications=[
  'Complete ordinary accessor return paths are decoded. Dynamic fallback object+8 cannot be proved never to alias GameThread storage from this bounded writer window alone; intended semantics assume valid engine object/map integrity. No legitimate counterexample was found, no exhaustive map-writer claim.',
  'Accessor does not read engine readiness byte. Positive getter excludes zero-ID/preinit equality; this is a primary named-thread check, not proof of overall engine initialization/readiness.',
  'Accessor may initialize strings/thread manager, lock its map, allocate/register atexit. These are ordinary engine API effects; there was no native execution here.',
  'Main-executable calls require normal process/code residence and real callback ownership. ModuleSnapshot/shared_ptr only retains a C++ source holder and is not OS mapping retention; Retention::None was not upgraded.',
  'Source import evidence is not final PRX admission or native resolution; candidate admission stays false.']
))

cache=ROOT/'outputs/2026-10-08-menu-path-audit/source-cache'
lua=cache/'LuaMod.cpp'; script=cache/'main.lua'; program=cache/'UE4SSProgram.cpp'
config=ROOT/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealInitializer.hpp'
module=config.parent/'PlatformModuleSnapshot.hpp'
owned=ROOT/'.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/OwnedMappedScanSource.hpp'
windows=[]
def window(p,a,b,meaning):
    lines=Path(p).read_text().splitlines(); text='\n'.join(lines[a-1:b])+'\n'
    windows.append(dict(source=pin(p),start_line=a,end_line=b,meaning=meaning,text=text,window_sha256=hashlib.sha256(text.encode()).hexdigest()))
for a,b,m in [
 (4729,4812,'funcptr marshal callback and complete simple/delayed queue drain'),
 (4894,4930,'native candidate posthook registration'),
 (4940,4995,'derived nonnative override posthook registration'),
 (4996,5087,'base carrier families and startup calibration entry'),
 (5118,5138,'query-triggered lazy arm scan, one-second throttle'),
 (4195,4208,'usable can return true while pump pending but no carrier armed'),
 (5319,5347,'ExecuteInGameThread carrier selection'),
 (5470,5510,'queue append and lazy alternate carrier arms'),
 (7840,7858,'one startup calibration before Lua mods')]: window(lua,a,b,m)
window(script,682,704,'unchanged F7 and key handlers queue ExecuteInGameThread')
window(script,723,740,'LoadMap prehook resets script state, no ClientRestart producer')
window(program,1002,1040,'shared shutdown_requested cancellation separated from startup Deadline')
lines=config.read_text().splitlines()
for needle,meaning in [('struct StartupWaitControl','actual StartupWaitControl cancellation/deadline contract'),('std::shared_ptr<const Platform::ModuleSnapshot> ModuleSource','actual Config supplier ownership contract')]:
 hits=[i+1 for i,l in enumerate(lines) if needle in l]
 if hits: window(config,max(1,hits[0]-3),min(len(lines),hits[0]+55),meaning)
save('evidence/SOURCE-CONSUMERS.json',dict(status='LOCAL_SELECTED_SOURCE_CENSUS_NOT_LIVE_PRODUCER_PROOF',windows=windows,producer_families=['/Script/Engine.Actor:ReceiveTick','/Script/Engine.AnimInstance:BlueprintUpdateAnimation','/Script/UMG.UserWidget:Tick'],recurring_intent=True,client_restart_producer=False,script_unchanged=pin(script),constraints=[
 'Startup calibration precedes start_lua_mods and may precede level Blueprint class creation.',
 'First ExecuteInGameThread availability query after load can trigger a bounded override rescan; scans are query-triggered and throttled, not autonomous.',
 'funcptr_marshal_usable can return true with a pending pump while no actual carrier is armed. Queued action is not delivery proof.',
 'A future accepted game-thread carrier invocation drains actions. Loaded override existence, recurring rate, pause/culling/level lifecycle, and current native callback delivery remain unmeasured.',
 'Saved Tick profile exists but selected marshal producer uses posthooks on the three override families, not a newly installed engine Tick path.',
 'Thread guard rejects a wrong-thread invocation but does not create a future producer.'
]))

r4=ROOT/'outputs/2026-10-07-l19-local-profile-r4'
selection=json.loads((r4/'reports/SOURCE-SELECTION.json').read_text())
original_init=r4/'units/UnrealInitializer.cpp'
original_bridge=r4/'units/runtime_start_bridge_r14_diag_spanpub.cpp'
original_profile=r4/'units/CodeSignatureProfile.hpp'
for p in [original_init,original_bridge,original_profile]:
 assert sha(p)==selection['source_files'][str(p.relative_to(r4))],p
for old,new in [(original_init,W/'candidate/UnrealInitializer.cpp'),(lua,W/'candidate/LuaMod.cpp'),(original_profile,W/'candidate/CodeSignatureProfile.hpp')]:
 (W/'evidence'/(new.name+'.patch')).write_text(''.join(difflib.unified_diff(old.read_text().splitlines(True),new.read_text().splitlines(True),fromfile=str(old),tofile=str(new))))
assert sha(original_bridge)==sha(W/'candidate/runtime_start_bridge_r14_diag_spanpub.cpp')

latest={}
for folder in ['target','target-wiring','target-wiring-v2']:
 for r in json.loads((W/folder/'commands.json').read_text()):
  if r['exit']==0: latest[r['name']]=dict(name=r['name'],folder=folder,command_receipt=str(W/folder/'commands.json'),object=pin(W/folder/(r['name']+'.o')),dependency_file=pin(W/folder/(r['name']+'.d')))
assert len(latest)==8
deps=set()
for r in latest.values():
 s=Path(r['dependency_file']['path']).read_text().replace('\\\n',' ')
 deps.update(Path(p).resolve() for p in shlex.split(s.split(':',1)[1]))
external=set(p for p in deps if not p.is_relative_to(W))
external.update([packet,lua,script,program,config,module,owned,original_init,original_bridge,original_profile,
 ROOT/'outputs/2026-10-07-l19-local-profile-r4/reports/SOURCE-SELECTION.json',
 ROOT/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json',
 ROOT/'outputs/2026-10-07-l19-r3-full-build/prx-candidate-l19-2/build-receipt.json',
 ROOT/'outputs/2026-10-08-native-thread-audit/THREAD-PROFILE-CANDIDATE.json'])
for folder in ['target','target-wiring','target-wiring-v2']:
 for r in json.loads((W/folder/'commands.json').read_text()):
  external.add(Path(r['argv'][0]).resolve());external.add(Path(r['LLVM_CONFIG']).resolve())
provider=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib/libkernel.so'
external.add(provider)
assert sha(provider)=='c6579f2614d8b8f905a673f810a1b562f1432b07397d61c11e876b5648428753'
save('HANDOFF.json',dict(status='SEALED_CANDIDATE_REQUIRES_REVIEW_AND_REAL_IMPORT_ADMISSION',public_config_header_changed=False,new_providers=[str(W/'candidate'/(s+'.cpp')) for s in ['NameThreadPolicy','NativeThreadNameAdapter','TargetEngineCalls','ThreadNameRuntime']],source_replacements=[dict(original=pin(original_init),candidate=pin(W/'candidate/UnrealInitializer.cpp')),dict(original=pin(lua),candidate=pin(W/'candidate/LuaMod.cpp'))],byte_identical_bridge_copy=dict(original=pin(original_bridge),candidate=pin(W/'candidate/runtime_start_bridge_r14_diag_spanpub.cpp'),reason='quote include resolves relative to source directory; owned copy admits third authenticated code pin without original bridge logic edits'),header_roles=dict(code_profile=str(W/'candidate/CodeSignatureProfile.hpp'),thread_profile=str(W/'candidate/ThreadNameProfile.hpp'),runtime_binding_data=str(W/'candidate/CandidateBindingData.hpp'),import_admission_control=str(W/'candidate/ThreadNamePackageAdmission.hpp'),runtime_interface=str(W/'candidate/ThreadNameRuntime.hpp')),include_prefix=[str(W/'candidate'),str(ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include'),str(ROOT/'outputs/2026-10-07-l19-local-profile-r4/units')],target_tus=list(latest.values()),next_integration_constraints=[
 'Derive actual affected consumer closure and recipes from current R5+ACK/pad composition, not these older compile recipes alone.',
 'Resolve import admission via exact generated contract/startup identity route and verify final PRX NID/module/library versions; false candidate control intentionally refuses preseed.',
 'Call retire_runtime at actual callback retirement/failed-start rollback before code/supplier unload; a false self-wait requires continued code residence.',
 'ModuleSnapshot is not OS VM retention. No BSS reader, new transport, or Retention::None upgrade.',
 'Ticket must cover whole queue drain; supplied LuaMod overlay does so.',
 'Preserve owned IsCancelled lifecycle; do not use elapsed startup Deadline as postinit cancellation.',
 'Prove/observe recurring native producer separately before a menu-delivery success claim.'
]))

report='''The saved native accessor at image+0xA262E0 supports a plain ID32 argument and address return. The GameThread branch returns image+0x9402AC8; this candidate authenticates a positive, stable scePthreadGetthreadid result by comparing only that returned pointer. It reads no returned FString, BSS global, or string contents. This is a separate image-bound profile candidate; selected sources, R4/R5 profiles, the original mod, and console state were not changed.

The complete 283-byte ordinary accessor body has three return sites. Its primary branch compares EDI with dword [image+0x9358A98], then returns the storage initialized from the native UTF16 GameThread literal. The render branch returns another storage address. The remaining branch invokes the locked thread manager, returning its empty string or a registered object's embedded string. All these paths and the lazy initializer are preserved in NATIVE-NAME-FACTS.json and 672 exact instruction windows. The bounded registration window does not prove every possible dynamic fallback pointer can never numerically alias the GameThread storage. The semantic conclusion assumes normal engine object/map integrity; it is not unconditional exhaustive writer proof. No legitimate counterexample was found. This API may initialize strings/manager state, lock, allocate, and register atexit; no native call was executed here.

The native caller passes an ID dword in EDI and consumes the RAX address. Actual SDK compiler controls produce an ordinary 8-byte function pointer, ID32, and 16-byte FString; emitted call code has no receiver/hidden result argument and does not dereference the returned address. const FString& is an ABI-compatible source spelling, not recovered original source. Getter declaration is qualified ABI correlation from saved PS5 callsites plus a saved third-party header. The exact root import packet identifies EI-5-jlq2dE#5#J, libkernel module257/library1, and pinned SDK export scePthreadGetthreadid T1218c/size0. pthread_getthreadid_np is excluded. Native resolution and final PRX admission are untested.

The generic binder uses the actual Config ModuleSource and matching NativeScanSource owner, exact image SHA/size/extent/base, selected layout digest, authenticated third 33-byte executable/nonwritable code pin, and static storage PT_LOAD geometry. Existing FName/SCO pins remain unchanged. The initializer prepares this binding before seeds/maps publish and invokes no name accessor during preparation. Callback acquire uses only the owned StartupWait.IsCancelled; the authentication freshness/startup deadline is not reused after initialization. A ticket retains the C++ source holder and spans the entire real marshal queue drain; nested publisher tickets also gate opaque affinity publication. Opaque thread identity is never truncated into ID32. ModuleSnapshot/shared_ptr is not OS mapping retention, and Retention::None was not upgraded. Ordinary main-executable calls require continued normal process/code residence and callback ownership, as existing engine API calls do.

Current integration deliberately holds: ThreadNamePackageAdmission.hpp is false, so preseed refuses before publication until a reviewed generated import contract/startup identity route and final package verification supply real admission. The bool is not tied to a self-changing artifact hash and source correlation is not native resolution. retire_runtime cancels, waits, and refuses same-thread self retirement, but the actual selected runtime teardown/failed-start rollback does not yet invoke it. The next combined integration must wire that boundary and preserve code residence on a false retirement; this slice does not claim wired-and-ready runtime authority.

The unchanged F7 handler enqueues ExecuteInGameThread(toggleMenu). Selected startup calibration and query-triggered, one-second-throttled scans posthook ReceiveTick, BlueprintUpdateAnimation, and UserWidget.Tick Blueprint overrides. Future accepted invocations can drain the queue. No ClientRestart producer is present in the unchanged script. Startup scanning can precede level classes, and a later query can discover loaded overrides, but funcptr_marshal_usable can admit queueing even with no armed carrier. Pending work does not itself retry scanning or create a callback. Loaded carrier existence, recurring rate, pause/culling/save-load behavior, and delivery are unmeasured. The guard rejects a foreign callback; it does not establish a future producer or menu success.

Verification: 20 policy and21 actual-binder host controls pass with fatal ASan/UBSan; the binder uses the genuine saved-image cache authentication once per process with three native pins and an explicit modeled native-API seam. The positive host owner is a fixture, not live mapping authority. Eight distinct actual SDK target TUs compile: four helper/ABI TUs, runtime facade, initializer, LuaMod, and the byte-identical bridge copy that admits the new code-pin header. The latest target census is [4,4,8,8,16,8,0,1,33,3]. An earlier bridge compile using the wrong TU recipe failed and is preserved; the correct original package-bridge recipe passes. No full link, package, native execution, console action, or selected source mutation was performed here.

Reproduce the bounded verifier with python3 outputs/2026-10-08-native-thread-name-adapter/verify.py. Exact historical target argv, environment, outputs, dependency files, RED controls, and GREEN logs are pinned in SOURCE-SELECTION.json and HANDOFF.json. Producers create unique output directories; reruns must use a new owned directory rather than overwrite this freeze. The combined worker must derive the actual dependency closure from its current composition. All remaining admission, retirement, ordinary-image-life, dynamic-fallback, and producer-liveness qualifications are preserved.
'''
(W/'REPORT.md').write_text(report)
owned_files=[p for p in W.rglob('*') if p.is_file() and p.name not in ['SOURCE-SELECTION.json','FINAL-RECEIPT.json','VERIFICATION.json']]
save('SOURCE-SELECTION.json',dict(scope='NEW owned adapter only; selected/shared/frozen sources unchanged',owned_files=[pin(p) for p in sorted(owned_files)],external_files=[pin(p) for p in sorted(external)],latest_target_dependency_union=len(deps),latest_target_external_dependencies=len(set(p for p in deps if not p.is_relative_to(W))),image_pin_reference='NATIVE-NAME-FACTS.json; complete image was authenticated by collect_native and genuine host cache processes; verifier reads bounded windows',original_script=pin(script),original_init=pin(original_init),original_lua=pin(lua),original_bridge=pin(original_bridge),public_config=pin(config)))
save('FINAL-RECEIPT.json',dict(status='SEALED_REVIEW_PENDING_CANDIDATE_NOT_NATIVE_READY',source_selection=pin(W/'SOURCE-SELECTION.json'),facts=pin(W/'NATIVE-NAME-FACTS.json'),report=pin(W/'REPORT.md'),handoff=pin(W/'HANDOFF.json'),native_instruction_windows=672,policy_checks=20,binding_checks=21,host_failures=0,target_distinct_tus=8,target_successful_executions=9,preserved_target_failures=1,native_execution=False,final_package_import_admitted=False,selected_source_changes=False,queue_delivery_proved=False,menu_success_claim=False))
print(json.dumps(dict(source_selection_sha256=sha(W/'SOURCE-SELECTION.json'),facts_sha256=sha(W/'NATIVE-NAME-FACTS.json'),final_sha256=sha(W/'FINAL-RECEIPT.json'),owned_files=len(owned_files),external_files=len(external),dependencies=len(deps))))
