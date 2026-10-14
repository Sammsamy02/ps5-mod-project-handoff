from pathlib import Path
import json,hashlib,shlex,difflib
w=Path(__file__).resolve().parent;root=w.parent.parent
old=root/'outputs/2026-10-08-native-thread-name-adapter'
def digest(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def pin(p):
 p=Path(p).resolve();return dict(path=str(p),sha256=digest(p),size=p.stat().st_size)
def save(p,d):(w/p).write_text(json.dumps(d,indent=2)+'\n')
provider=w/'candidate/ThreadNameRuntime.cpp';header=w/'candidate/NativeThreadAuthorityRetire.h'
(w/'PROVIDER.patch').write_text(''.join(difflib.unified_diff((old/'candidate/ThreadNameRuntime.cpp').read_text().splitlines(True),provider.read_text().splitlines(True),fromfile=str(old/'candidate/ThreadNameRuntime.cpp'),tofile=str(provider))))
run=(w/'tests/GREEN-controls/run.log').read_text();assert 'main-checks=8 failures=0' in run and 'PASS postconstructor' in run
red=(w/'tests/RED-controls/run.log').read_text();assert 'main-checks=8 failures=4' in red and 'FAIL postconstructor' in red
commands=json.loads((w/'target/commands.json').read_text());assert all(r['exit']==0 for r in commands)
defined=(w/'target/DEFINED.stdout').read_text().splitlines();undefined=(w/'target/C-UNDEFINED.stdout').read_text().splitlines()
assert len([l for l in defined if l.startswith('bridge_native_thread_authority_retire T ')])==1
assert len([l for l in undefined if l.startswith('bridge_native_thread_authority_retire U ')])==1
save('HANDOFF.json',dict(scope='new lifecycle implementation; no independent cleanup-review or acceptance claim',replacement_provider=dict(original=pin(old/'candidate/ThreadNameRuntime.cpp'),candidate=pin(provider)),new_c_header=pin(header),abi=dict(symbol='bridge_native_thread_authority_retire',prototype='int bridge_native_thread_authority_retire(void)',noexcept_cpp=True,statuses={'0':'retired or absent initialized owner','1':'busy/self-wait or internal retirement refusal','-1':'unknown before/after C++ managed lifetime'},resource_rule='every nonzero status retains all owners/resources; call successful retirement before fini/unload'),unchanged_public_headers=[pin(old/'candidate/ThreadNameRuntime.hpp'),pin(root/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealInitializer.hpp')],include_prefix=[str(w/'candidate'),str(old/'candidate')],target_command_receipt=pin(w/'target/commands.json'),target_provider_object=pin(w/'target/ThreadNameRuntime.o'),target_c_caller_object=pin(w/'target/target_c_entry.o'),outer_boundary_wiring_performed=False,requirements=[
 'Root wires the C call after existing code/pad residence guards and before owner/fini/resource release; this slice adds no outer bridge calls.',
 'Root freezes preparation/publication and stops new callbacks before retirement. Existing prepare_runtime can install a new owner after completed retirement; C entry is not a global restart barrier.',
 'Closing constructor phase waits only already-admitted C retire calls before owner-state destruction. It is not a substitute for retiring outstanding callback tickets before fini.',
 'Same-thread active ticket returns1 and remains owned; root must keep code/resources resident and retry after release.',
 'Pre/late phase returns-1 without touching managed mutex/shared_ptr state; unknown is not permission to unload.',
 'No native authority/import admission/profile/selected-source changes; preserve the sealed prior slice.'
]))
(w/'REPORT.md').write_text('''This new implementation adds the in-link C symbol bridge_native_thread_authority_retire(void) around the existing actual NativeNameThread::retire_runtime provider. Its ABI is int: 0 means an initialized owner is retired or absent, 1 means retirement refused or would wait on the calling callback itself, and -1 means managed C++ state is not known ready. Every nonzero result requires retained resources/code. This is implementation verification, not an independent cleanup review or acceptance.

The only replacement source is candidate/ThreadNameRuntime.cpp. The new NativeThreadAuthorityRetire.h is valid for C and C++; existing Config, runtime facade headers, binder/policy, selected/frozen sources, and profiles remain unchanged. Constant-initialized, always-lock-free, trivially destructible atomics guard entry before managed owner construction and after destruction. A marker declared after the mutex/shared_ptr publishes readiness last and closes it first on destruction. A two-check entry counter covers the race between reading ready and constructor-phase closure; admitted C calls finish before owner state is destroyed. No C call touches managed owner state in the unknown phases.

The actual retire_runtime function still retains a shared Binding while cancelling and waiting. Same-thread outstanding tickets return false and retain the current owner; release followed by a retry completes. Other-thread tickets delay completion until released. Repeated retirement of an absent initialized owner completes. The marker is not authority to run fini with active tickets, and this C entry is not a global restart barrier: the outer lifecycle must stop preparation/publication and new callbacks, obtain zero, then call fini/release resources. A concurrent new preparation after completed retirement must be excluded by that outer owner. No OS mapping-retention claim or native import admission is added.

Test-first controls preserve the unimplemented always-success stub: four main cases plus the late-destructor case fail. The implemented provider passes eight main cases and one actual late-destructor probe with fatal ASan/UBSan. The early probe TU is linked first, so its construction precedes provider readiness and its destruction follows provider state destruction; both observed calls return -1. Host tests compile actual runtime/binder/policy bodies but explicitly project the three consumed Config fields and supplier-owner interface, and model image/API inputs; these controls do not establish native thread authority. Early failed host setup attempts using incomplete or target-only headers are preserved separately from the behavioral RED run.

The actual SDK target recipe from the sealed ThreadNameRuntime compile was reused with only a new leading candidate include, replacement input source, and owned output/dependency paths. It compiles against the full genuine selected Config/supplier headers. A second target compile uses the same recipe with -x c/-std=c11 for the new C declaration caller. Both pass; two existing Atomic.hpp switch warnings in the provider are recorded. SDK symbol output defines exactly one bridge_native_thread_authority_retire and the C caller references that exact unmangled symbol. No full composition, package, native execution, console action, or outer bridge boundary edits were performed here.

HANDOFF.json contains exact provider/header/object/recipe pins and outer-owner requirements. Verify with python3 outputs/2026-10-08-native-thread-retire-entry/verify.py. Host producer commands create unique output directories; reruns must select new owned directories and preserve this freeze.
''')
external=set()
for p in w.glob('target/*.d'):
 text=p.read_text().replace('\\\n',' ')
 external.update(Path(q).resolve() for q in shlex.split(text.split(':',1)[1]) if not Path(q).resolve().is_relative_to(w))
external.update([old/'SOURCE-SELECTION.json',old/'FINAL-RECEIPT.json',old/'candidate/ThreadNameRuntime.cpp',old/'target-wiring/commands.json',old/'candidate/NativeThreadNameAdapter.cpp',old/'candidate/NameThreadPolicy.cpp'])
for r in commands:
 external.add(Path(r['argv'][0]).resolve())
 if 'LLVM_CONFIG' in r:external.add(Path(r['LLVM_CONFIG']).resolve())
external.add(Path('/usr/bin/clang++').resolve())
files=[p for p in w.rglob('*') if p.is_file() and p.name not in ['SOURCE-SELECTION.json','FINAL-RECEIPT.json','VERIFICATION.json']]
save('SOURCE-SELECTION.json',dict(scope='new lifecycle provider/header only; old slice unchanged',owned_files=[pin(p) for p in sorted(files)],external_files=[pin(p) for p in sorted(external)],selected_source_changes=False))
save('FINAL-RECEIPT.json',dict(status='SEALED_NEW_IMPLEMENTATION_REQUIRES_ROOT_OUTER_WIRING',selection=pin(w/'SOURCE-SELECTION.json'),provider=pin(provider),c_header=pin(header),handoff=pin(w/'HANDOFF.json'),report=pin(w/'REPORT.md'),host_controls=9,host_failures=0,behavioral_red_failures=5,target_compiles=2,target_compiler_failures=0,independent_acceptance_claim=False,native_execution=False,outer_wiring=False,selected_source_changes=False))
print(json.dumps(dict(final=digest(w/'FINAL-RECEIPT.json'),selection=digest(w/'SOURCE-SELECTION.json'),provider=digest(provider),header=digest(header),owned=len(files),external=len(external))))
