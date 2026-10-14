#!/usr/bin/env python3
"""Compare pinned UE5.6.1 allocator declarations with preserved image evidence.
Compiles only tiny synthetic header-layout probes to LLVM IR, never runtime/PRX.
No network. Writes only below allocator/<out> and preserves pre-fetch artifacts.
"""
from pathlib import Path
import argparse, json, hashlib, re, subprocess, shutil
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
EXPECTED={
 'official-headers/MemoryBase.h':'244972909e1857852834f86ca99503efa4e2c322df942e1f57d3692347d0602d',
 'official-headers/Exec.h':'3a25991b06d7f132ce5bb5a11b7c667b7b68a84817a6769dec7d6ca85e4346bf',
 'official-additional/Build.h':'d5d70bd0538dbc7e648cb5647d98d1238247288e8435156d6d76654bb6a54966',
 'official-additional/CoreTypes.h':'0047e4f35148adf121465f70bd0341eb57ad58d7673010d64adfd0b16857abf3',
 'official-additional/TargetRules.cs':'b3e96c0cce33043a36a5ae744676e618f2f62a1f3736abc9e768e752598d5f0d',
 'official-additional/UEBuildTarget.cs':'bafaf4753a95ccb95a448b9e7fc487a3d6262f9c61004eea856b041535d8bbca',
}
def pin(p):
 p=Path(p);b=p.read_bytes();return {'path':str(p.resolve()),'size':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def main():
 a=argparse.ArgumentParser();a.add_argument('--out',default='postfetch');args=a.parse_args();out=(HERE/args.out).resolve()
 assert out.is_relative_to(HERE) and out!=HERE
 out.mkdir(exist_ok=True,parents=True)
 def save(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n')
 pins=[]
 for rel,h in EXPECTED.items():
  p=pin(HERE/rel);assert p['sha256']==h,(rel,'pin mismatch');pins.append(p)
 for rel in ['official-headers/FETCH-RECEIPT.json','official-additional/FETCH-RECEIPT.json','official-additional/UBT-FETCH-RECEIPT.json']:
  rp=HERE/rel;receipt=json.loads(rp.read_text());pins.append(pin(rp))
  for f in receipt['files']:
   fp=Path(f['local_path']);fp=ROOT/fp if not fp.is_absolute() else fp;b=fp.read_bytes()
   assert f['commit']=='6978b63c8951e57d97048d8424a0bebd637dde1d'
   assert hashlib.sha256(b).hexdigest()==f['sha256']
   assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==f['blob_sha1']
 image=json.loads((HERE/'measured/image-measurements.json').read_text());before=json.loads((HERE/'measured/candidate-table.json').read_text())
 for rel in ['measured/image-measurements.json','measured/candidate-table.json','measured/image-functions.json']:pins.append(pin(HERE/rel))
 # Preserve exact engine declarations; stub only unrelated include dependencies.
 inc=out/'stubs';(inc/'HAL').mkdir(parents=True,exist_ok=True);(inc/'Misc').mkdir(exist_ok=True);(inc/'Templates').mkdir(exist_ok=True)
 (inc/'CoreTypes.h').write_text('#pragma once\nusing SIZE_T=__SIZE_TYPE__;\nusing size_t=__SIZE_TYPE__;\nusing uint32=unsigned int;\nusing uint64=unsigned long long;\nusing TCHAR=char16_t;\n#define CORE_API\n#define UE_DEPRECATED(...)\n#define TEXT(x) u##x\n')
 for rel in ['HAL/PlatformAtomics.h','HAL/PlatformCrt.h','Templates/Atomic.h','Misc/AssertionMacros.h']:(inc/rel).write_text('#pragma once\n')
 (inc/'Misc/OutputDevice.h').write_text('#pragma once\nclass FOutputDevice { public: void Logf(const TCHAR*, ...); };\n')
 (inc/'Misc/Exec.h').write_text('#include "'+str(HERE/'official-headers/Exec.h')+'"\n')
 fixture='''#include "MEMORY_HEADER"
static_assert(sizeof(void*) == 8);
FExec::~FExec() {}
bool FExec::Exec(UWorld*, const TCHAR*, FOutputDevice&) { return false; }
#if !UE_ALLOW_EXEC_COMMANDS
bool FExec::Exec(UWorld*, const TCHAR*) { return false; }
#endif
void* FMalloc::TryMalloc(SIZE_T c, uint32 a) { return Malloc(c,a); }
void* FMalloc::TryRealloc(void* p, SIZE_T c, uint32 a) { return Realloc(p,c,a); }
void* FMalloc::MallocZeroed(SIZE_T c, uint32 a) { return Malloc(c,a); }
void* FMalloc::TryMallocZeroed(SIZE_T c, uint32 a) { return TryMalloc(c,a); }
void FMalloc::InitializeStatsMetadata() {}
void FMalloc::UpdateStats() {}
void FMalloc::GetAllocatorStats(FGenericMemoryStats&) {}
struct AllocatorLayoutProbe final : FMalloc {
 void* Malloc(SIZE_T, uint32) override { return nullptr; }
 void* Realloc(void*, SIZE_T, uint32) override { return nullptr; }
 void Free(void*) override {}
};
AllocatorLayoutProbe force_vtable;
'''.replace('MEMORY_HEADER',str(HERE/'official-headers/MemoryBase.h'))
 cpp=out/'header-layout-probe.cpp';cpp.write_text(fixture)
 clang=shutil.which('clang++');assert clang
 version=subprocess.run([clang,'--version'],capture_output=True,text=True,check=True).stdout
 evidence=[]
 for enabled in [1,0]:
  flags=['-std=c++20','-target','x86_64-unknown-freebsd','-fno-rtti','-DUE_BUILD_SHIPPING=1','-DWITH_EDITOR=0',f'-DUE_ALLOW_EXEC_COMMANDS_IN_SHIPPING={enabled}','-I',str(inc)]
  argv=[clang,*flags,'-S','-emit-llvm','-Xclang','-fdump-vtable-layouts',str(cpp),'-o',str(out/f'exec-{enabled}.ll')]
  r=subprocess.run(argv,capture_output=True,text=True);(out/f'exec-{enabled}.layout.txt').write_text(r.stdout);(out/f'exec-{enabled}.stderr.txt').write_text(r.stderr);assert r.returncode==0,r.stderr
  pp=subprocess.run([clang,*flags,'-E',str(cpp)],capture_output=True,text=True,check=True);(out/f'exec-{enabled}.preprocessed.cpp').write_text(pp.stdout)
  macros=subprocess.run([clang,*flags,'-dM','-E',str(cpp)],capture_output=True,text=True,check=True).stdout
  relevant=[x for x in macros.splitlines() if any(k in x for k in ['UE_ALLOW_EXEC','UE_BUILD_SHIPPING','WITH_EDITOR'])]
  evidence.append({'exec_in_shipping_input':enabled,'argv':argv,'macros':relevant,'exit_code':r.returncode,'layout_file':str(out/f'exec-{enabled}.layout.txt'),'ir_file':str(out/f'exec-{enabled}.ll')})
 save('compiler-probes.json',{'purpose':'Exact supplied MemoryBase.h/Exec.h classes with minimal dependency types; synthetic x86_64 Itanium-family Clang layout corroboration, not a Prospero compiler or game build macro receipt','compiler_path':clang,'compiler_version':version,'stubs_limit':'CoreTypes primitives and output-device declarations stubbed; unrelated dependencies empty. Both real engine class headers are unmodified. No game code executed and no runtime or PRX rebuilt.','runs':evidence})
 # Extract all fresh virtual declarations; FMalloc::Exec overrides FExec's existing slot.
 memory=(HERE/'official-headers/MemoryBase.h').read_text(); lines=memory.splitlines(); own=[]
 for n,l in enumerate(lines,1):
  m=re.search(r'\bvirtual\s+.+?\b(\w+)\s*\(',l)
  if m and m[1]!='Exec':own.append({'name':m[1],'line':n,'declaration':l.strip()})
 assert len(own)==26
 branches={}
 for enabled in [1,0]:
  base=['~FExec [complete]','~FExec [deleting]','Exec(UWorld*,const TCHAR*,FOutputDevice&)']
  if not enabled:base.append('Exec(UWorld*,const TCHAR*)')
  base+=['Exec_Runtime','Exec_Dev','Exec_Editor']
  seq=[{'name':name,'source':'FExec'} for name in base]+[dict(x,source='FMalloc') for x in own]
  for i,x in enumerate(seq):x.update({'index':i,'byte_offset':hex(i*8)})
  branches[str(enabled)]=seq
  # Parse Clang's exact FMalloc primary vtable, whose first two entries are metadata.
  dump=(out/f'exec-{enabled}.layout.txt').read_text()
  mat=re.search(r"Vtable for 'FMalloc' \((\d+) entries\)\.\n(.*?)(?=\n(?:VTable|Original map:|Vtable for)|\Z)",dump,re.S)
  assert mat, 'FMalloc vtable dump absent'
  entries=re.findall(r'^\s*(\d+) \| (.+)$',mat[2],re.M)
  expected_size=len(seq)+2; assert int(mat[1])==expected_size,(enabled,mat[1],expected_size)
  body={int(i):s for i,s in entries}
  for row in seq:
   value=body[row['index']+2]
   wanted=row['name'] if row['source']=='FMalloc' else row['name'].split('(')[0].split(' [')[0]
   if row['name'].startswith('~FExec'):
    assert 'FMalloc::~FMalloc()' in value and row['name'].split(' ',1)[1] in value,(row,value)
   else: assert wanted in value,(row,value)
   row['compiler_entry']=value
   row['compiler_verified']=True
 slots={int(x['byte_offset'],16):x for x in image['slots']}
 previous={e['method']:e for e in before['entries']}
 candidate=[]
 for method,prior in previous.items():
  yes=next(x for x in branches['1'] if x['name']==method);no=next(x for x in branches['0'] if x['name']==method)
  slot=slots[int(yes['byte_offset'],16)]
  independent=prior['image_semantic_candidate_offset']
  agreeing=independent==yes['byte_offset'] if independent else None
  if independent:assert agreeing,('source/image conflict',method,independent,yes['byte_offset'])
  candidate.append({'method':method,'source_declaration':{'path':str(HERE/'official-headers/MemoryBase.h'),'line':yes['line'],'text':yes['declaration']},'conditional_source_byte_offsets':{'UE_ALLOW_EXEC_COMMANDS=1':yes['byte_offset'],'UE_ALLOW_EXEC_COMMANDS=0':no['byte_offset']},'source_default_branch':1,'source_default_branch_reason':'TargetRules.bUseExecCommandsInShipping defaults true; UEBuildTarget emits UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING=1; Exec.h computes command flag1 for Shipping noneditor unless overridden','source_default_image_slot':slot,'preexisting_independent_image_semantic_offset':independent,'independent_semantic_anchor_agrees_with_default':agreeing,'image_semantic_confidence':'Independent byte-behavior anchor agrees' if independent else 'Source-assigned slot with executable target; body alone does not independently identify method','status':'DEFAULT_SOURCE_AND_IMAGE_SEMANTIC_ANCHOR_AGREE' if independent else 'DEFAULT_SOURCE_ASSIGNED_EXECUTABLE_IMAGE_ENTRY','actual_game_macro_captured':False,'approved_for_profile':False,'citations':['compiler-probes.json',str(HERE/'measured/image-measurements.json'),str(HERE/'measured/image-functions.json'),str(HERE/'measured/candidate-table.json')]})
 excerpts=[]
 for rel,needles in [('official-headers/Exec.h',['UE_ALLOW_EXEC','virtual','class FExec']),('official-headers/MemoryBase.h',['class FMalloc','public FUse','public FExec','virtual','UE_BUILD_SHIPPING','MaxSingleAlloc']),('official-additional/TargetRules.cs',['bUseExecCommandsInShipping']),('official-additional/UEBuildTarget.cs',['bUseExecCommandsInShipping','UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING'])]:
  for n,l in enumerate((HERE/rel).read_text().splitlines(),1):
   if any(s in l for s in needles):excerpts.append({'path':str(HERE/rel),'line':n,'text':l})
 save('source-excerpts.json',excerpts)
 save('candidate-table.json',{'supersedes':'The pre-fetch source-A-unavailable conclusion in allocator/REPORT.md and measured/candidate-table.json; originals preserved','scope':'Pinned Epic UE5.6.1-release declarations at commit6978b63c8951e57d97048d8424a0bebd637dde1d; synthetic x86_64 Itanium layout; saved Townfall image hash'+image['image']['sha256']+'; NOT unconditional other-game ABI','source_A_available':True,'actual_game_macro_captured':False,'conditional_correspondence':'Default Shipping/non-editor exec-enabled source branch agrees with all six preexisting independent semantic anchors; no actual Townfall target-rule/command-line override receipt exists. Disabled branch gives different offsets and does not agree with those anchor offsets.','runtime_wrappers':len(candidate),'complete_source_virtual_sequences':branches,'entries':candidate,'ready_for_runtime_build':False,'profile_mutated':False,'review_required':True})
 assumptions={'UE_BUILD_SHIPPING':{'probe_value':1,'game_evidence':'retained Shipping path/profile, not actual compiler response file'},'WITH_EDITOR':{'probe_value':0,'game_evidence':'Shipping PS5 non-editor scope; actual UBT compile-definition receipt absent'},'UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING':{'official_UBT_default':1,'zero_branch_also_derived':True,'actual_game_override':'unknown'},'UE_ALLOW_EXEC_COMMANDS':{'exec_h_rule':'when undefined: Shipping && !WITH_EDITOR -> UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING; otherwise1','actual_game_definition':'not independently captured','image_agreeing_branch':1},'UE_ALLOW_EXEC_DEV':{'default_formula':'!UE_BUILD_SHIPPING && UE_ALLOW_EXEC_COMMANDS','effect_on_virtual_declaration_count':'none; Exec_Dev is unconditional'},'UE_ALLOW_EXEC_EDITOR':{'default_formula':'WITH_EDITOR && UE_ALLOW_EXEC_COMMANDS','effect_on_virtual_declaration_count':'none; Exec_Editor is unconditional'},'STATS_and_UPDATE_MALLOC_STATS':{'effect_on_virtual_declaration_count':'none in exact MemoryBase.h/Exec.h; stats method declarations remain'},'FMalloc_Exec_override':{'when_enabled':'reuses FExec::Exec slot; adds no slot','when_disabled':'absent in FMalloc; two final Exec overloads exist in FExec'},'destructors':{'source':'Exec.h:31 declares virtual ~FExec; FMalloc inherits virtual destructor','abi':'complete/deleting pair at inherited primary-table slots0/1, checked by Clang probes','not_counted':'nonvirtual FUseSystemMallocForNew operators new/delete add no virtual slots; nonshipping static MaxSingleAlloc adds no virtual slot'},'portability':'Actual Prospero vendor compiler/build modifications are not reproduced. Byte correspondence is for the pinned saved image only.'}
 save('macro-assumptions.json',assumptions)
 pins += [pin(__file__)]
 save('source-pins.json',{'pins':pins,'commit':'6978b63c8951e57d97048d8424a0bebd637dde1d','all_six_official_files_hash_and_git_blob_verified':True})
 verify={'official_files_verified':len(EXPECTED),'header_layout_probes_passed':2,'exec_enabled_virtual_slots':len(branches['1']),'exec_disabled_virtual_slots':len(branches['0']),'runtime_wrappers_source_assigned':len(candidate),'runtime_wrapper_targets_executable':sum(x['source_default_image_slot']['target_pt_load_flags']&1!=0 for x in candidate),'independent_semantic_anchors_agree':sum(x['independent_semantic_anchor_agrees_with_default'] is True for x in candidate),'unconditional_other_game_ABI_proved':False,'actual_game_macro_captured':False,'runtime_builds':0,'runtime_or_profile_mutations':0,'disagreement_with_default_source_branch':False,'disabled_branch_disagrees_with_anchor_offsets':True,'ready_for_runtime_build':False}
 save('verification.json',verify);print(json.dumps(verify,indent=2))
if __name__=='__main__':main()
