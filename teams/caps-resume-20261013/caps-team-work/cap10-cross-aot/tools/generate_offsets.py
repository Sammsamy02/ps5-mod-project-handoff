"""Derive complete Mono target ABI constants with public target Clang; no execution.

Follow upstream offsets-tool's runtime/JIT type lists. Clang AST discovers actual
non-bitfield fields; Clang LLVM IR evaluates sizeof, alignment and offsetof.
Configured CPPFLAGS come from the genuine target-runtime Makefile snapshot.
"""
from pathlib import Path
import hashlib,json,os,re,subprocess
H=Path(__file__).resolve().parents[1];W=H/'cross-work';C=H/'target-config'
inv=json.loads((H.parent/'cap10-port-research/LOCAL-INVENTORY.json').read_text())
options=json.loads((H/'evidence/TARGET-COMPILE-OPTIONS.json').read_text())
cc=Path(inv['files'][0]['path']);env=os.environ.copy();env['LLVM_CONFIG']=str(Path(inv['llvm_bin'])/'llvm-config')
for key in ['CFLAGS','CPPFLAGS','CXXFLAGS','MONO_ENV_OPTIONS','MONO_PATH']:env.pop(key,None)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def walk(n):
 yield n
 for child in n.get('inner',[]):yield from walk(child)
def fields_from_ast(data,names):
 nodes=list(walk(data));records={n['id']:n for n in nodes if n.get('kind')=='RecordDecl' and n.get('completeDefinition')}
 named={n['name']:n for n in records.values() if n.get('name')};typedefs={n['name']:n for n in nodes if n.get('kind')=='TypedefDecl'}
 result={}
 for name in names:
  typedef=typedefs.get(name);record=None
  if typedef:
   for n in walk(typedef):
    if n.get('kind')=='RecordType':
     ref=n['decl'];record=records.get(ref['id']) or named.get(ref.get('name'));break
  if record:
   result[name]=[n['name'] for n in record.get('inner',[]) if n.get('kind')=='FieldDecl' and n.get('name') and not n.get('isBitfield')]
 return result
names={
 'metadata':['MonoObject','MonoClass','MonoVTable','MonoDomain','MonoDelegate','MonoInternalThread','MonoMulticastDelegate','MonoTransparentProxy','MonoRealProxy','MonoRemoteClass','MonoArray','MonoArrayBounds','MonoSafeHandle','MonoHandleRef','MonoComInteropProxy','MonoString','MonoException','MonoTypedRef','MonoThreadsSync','SgenThreadInfo','SgenClientThreadInfo','MonoProfilerCallContext'],
 'mini':['MonoLMF','MonoMethodRuntimeGenericContext','MonoJitTlsData','MonoGSharedVtMethodRuntimeInfo','MonoContinuation','MonoContext','MonoDelegateTrampInfo','GSharedVtCallInfo','SeqPointInfo','DynCallArgs','MonoLMFTramp','CallContext','MonoFtnDesc']}
basic=['gint8','gint16','gint32','gint64','float','double','gpointer'];records=[];sections={}
for part in ['metadata','mini']:
 source=H/'artifacts'/f'{part}-offsets-complete.c';ir=source.with_suffix('.ll');astfile=source.with_suffix('.ast.json')
 prefix='#define MONO_GENERATING_OFFSETS 1\n#include "'+str(W/'mono'/part/(part+'-cross-helpers.c'))+'"\n'
 source.write_text(prefix)
 cmd=[str(cc),'-std=gnu99']+options['cppflags']+(options['metadata_gc_flags'] if part=='metadata' else options['mini_gc_flags'])+['-I'+str(C),'-I'+str(C/'mono/eglib'),'-I'+str(W),'-I'+str(W/'mono'),'-I'+str(W/'mono/eglib')]
 astcmd=cmd+['-Xclang','-ast-dump=json','-fsyntax-only',str(source)]
 with astfile.open('w') as f,(H/'logs'/f'COMPLETE-AST-{part.upper()}.log').open('x') as err:r=subprocess.run(astcmd,env=env,stdout=f,stderr=err)
 if r.returncode:raise SystemExit(r.returncode)
 types=fields_from_ast(json.loads(astfile.read_text()),names[part]);missing=sorted(set(names[part])-set(types))
 if missing and missing!=['MonoFtnDesc']:raise ValueError('missing target types: '+repr(missing))
 text=prefix
 if part=='metadata':
  for typ in basic:
   text+='const unsigned long align__'+typ+' = __alignof__('+typ+');\nconst unsigned long size__'+typ+' = sizeof('+typ+');\n'
 for typ,fields in types.items():
  text+='const unsigned long size__'+typ+' = sizeof('+typ+');\n'
  for field in fields:text+='const unsigned long offset__'+typ+'__'+field+' = __builtin_offsetof('+typ+','+field+');\n'
 source.write_text(text)
 ircmd=cmd+['-S','-emit-llvm','-O0',str(source),'-o',str(ir)]
 with (H/'logs'/f'COMPLETE-OFFSETS-{part.upper()}.log').open('x') as f:r=subprocess.run(ircmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 if r.returncode:raise SystemExit(r.returncode)
 data=ir.read_text();triple=re.search(r'target triple = "([^"]+)"',data).group(1)
 if triple!='x86_64-sie-ps5':raise ValueError('wrong compiler target: '+triple)
 values=re.findall(r'^@(offset__\w+|size__\w+|align__\w+) = .*?constant i64 (\d+),',data,re.M)
 expected=sum(len(v)+1 for v in types.values())+(14 if part=='metadata' else 0)
 if len(values)!=expected:raise ValueError('compiler constant census mismatch')
 lines=[]
 for name,value in values:
  bits=name.split('__',2)
  if bits[0]=='offset':lines.append('DECL_OFFSET2('+bits[1]+','+bits[2]+','+value+')')
  elif bits[0]=='size':lines.append('DECL_SIZE2('+bits[1]+','+value+')')
  else:lines.append('DECL_ALIGN2('+bits[1]+','+value+')')
 sections[part]=lines
 records.append({'part':part,'ast_command':astcmd,'ir_command':ircmd,'exit_code':0,'target_triple':triple,'constants':len(values),'fields':types,'absent_target_types':missing,'compiler_wrapper_sha256':sha(cc),'source_sha256':sha(source),'ast_sha256':sha(astfile),'ir_sha256':sha(ir)})
header='''/* Generated from genuine x86_64-sie-ps5 target compiler constants. */
#ifndef USED_CROSS_COMPILER_OFFSETS
#if defined(TARGET_AMD64) && !defined(TARGET_MACH)
#ifndef HAVE_BOEHM_GC
#define HAS_CROSS_COMPILER_OFFSETS
#if defined(USE_CROSS_COMPILE_OFFSETS) || defined(MONO_CROSS_COMPILE)
'''
for part,guard in [('metadata','DISABLE_METADATA_OFFSETS'),('mini','DISABLE_JIT_OFFSETS')]:
 header+='#ifndef '+guard+'\n#define USED_CROSS_COMPILER_OFFSETS\n'+'\n'.join(sections[part])+'\n#endif\n'
header+='#endif\n#endif\n#endif\n#endif\n'
output=W/'x86_64-sie-ps5.h';output.write_text(header)
with (H/'evidence/OFFSETS-COMPLETE.json').open('x') as f:json.dump({'compilations':records,'offset_header_sha256':sha(output),'target_config_sha256':sha(C/'config.h'),'target_eglib_config_sha256':sha(C/'mono/eglib/eglib-config.h'),'target_execution':False,'source_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40'},f,indent=2);f.write('\n')
print('complete target offsets:',{k:len(v) for k,v in sections.items()},flush=True)
