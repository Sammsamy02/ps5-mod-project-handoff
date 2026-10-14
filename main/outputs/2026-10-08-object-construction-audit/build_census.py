from pathlib import Path
import hashlib,json,struct,subprocess,os,re
root=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
out=root/'outputs/2026-10-08-object-construction-audit'
pins=lambda p:{'path':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
recipe=root/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json'
r=json.loads(recipe.read_text())['recipes']['unreal-uobjectglobals']
cls='RC::Unreal::UObjectGlobals::Above55::FStaticConstructObjectParameters'
fields=['Class','Outer','Name','SetFlags','InternalSetFlags','bCopyTransientsFromClassDefaults','bAssumeTemplateIsArchetype','Template','InstanceGraph','ExternalPackage','PropertyInitCallback','SerialNumber','SubobjectOverrides']
items=[('size','sizeof(P)'),('align','alignof(P)')]+[(f,'__builtin_offsetof(P,'+f+')')for f in fields]+[(f+'.width','sizeof(((P*)nullptr)->'+f+')')for f in fields]
src=out/'target-census.cpp'
src.write_text('#include <Unreal/UObjectGlobals.hpp>\n#include <Unreal/NameTypes.hpp>\nusing P='+cls+';\nextern "C" { extern const unsigned long long ConstructionFacts[] = {\n'+',\n'.join(e for _,e in items)+'\n}; }\nextern "C" RC::Unreal::UObject* CallConstruct(RC::Unreal::UObject*(*fn)(const P&),const P& p) {return fn(p);}\n')
a=list(r['pass1_argv'])
for flag,val in [('-c',str(src)),('-o',str(out/'target-census.o')),('-MF',str(out/'target-census.d'))]:a[a.index(flag)+1]=val
# Same flags/actual selected include-prefix. Only a new census TU and outputs.
env=os.environ.copy();env['LLVM_CONFIG']=r['LLVM_CONFIG'];p=subprocess.run(a,cwd=root,env=env,capture_output=True,text=True)
(out/'COMMAND.json').write_text(json.dumps({'argv':a,'LLVM_CONFIG':r['LLVM_CONFIG'],'exit':p.returncode,'recipe':pins(recipe)},indent=2)+'\n');(out/'STDERR.log').write_text(p.stderr)
assert p.returncode==0,p.stderr[-1500:]
b=(out/'target-census.o').read_bytes();shoff=struct.unpack_from('<Q',b,40)[0];ent,num,_=struct.unpack_from('<HHH',b,58);sections=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*ent)for i in range(num)]
found=None
for sec in sections:
 if sec[1]!=2:continue
 names=sections[sec[6]];ns=b[names[4]:names[4]+names[5]]
 for pos in range(sec[4],sec[4]+sec[5],sec[9]):
  no,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',b,pos)
  name=ns[no:ns.index(0,no)].decode()
  if name=='ConstructionFacts':found=(sections[idx][4]+val,size)
assert found and found[1]==len(items)*8
values=struct.unpack_from('<'+'Q'*len(items),b,found[0]);facts={k:v for(k,_),v in zip(items,values)}
image=root/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';im=image.read_bytes();assert hashlib.sha256(im).hexdigest()=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
asm=root/'outputs/2026-10-07-l19-static-layout-check/StaticConstructObject.asm';verified=[]
phoff=struct.unpack_from('<Q',im,32)[0];pe,pn=struct.unpack_from('<HH',im,54)
segments=[struct.unpack_from('<IIQQQQQQ',im,phoff+i*pe) for i in range(pn)]
def file_offset(vaddr,size):
 seg=next(x for x in segments if x[0]==1 and x[3]<=vaddr and vaddr+size<=x[3]+x[5]);return seg[2]+vaddr-seg[3]
for line in asm.read_text().splitlines():
 match=re.match(r'^0x[0-9a-f]+\s+\(f 0x([0-9a-f]+)\)\s+([0-9a-f]+)\s+(.*)',line)
 if not match:continue
 vaddr=int(match[1],16);raw=bytes.fromhex(match[2]);offset=file_offset(vaddr,len(raw));assert im[offset:offset+len(raw)]==raw
 verified.append({'image_vaddr':hex(vaddr),'file_offset':hex(offset),'bytes':raw.hex(),'text':match[3]})
expected={'Class':0,'Outer':8,'Name':0x10,'SetFlags':0x18,'InternalSetFlags':0x1c,'bAssumeTemplateIsArchetype':0x21,'Template':0x28,'ExternalPackage':0x38,'SerialNumber':0x70}
for name,v in expected.items():assert facts[name]==v,(name,facts[name],v)
result={'scope':'Actual target-compiled selected parameter declaration + saved native instruction bytes, NOT native construction execution','target_facts':facts,'native_correlated_fields':expected,'native_instruction_windows_verified':len(verified),'native_instructions':verified,'inputs':[pins(recipe),pins(src),pins(asm),pins(image)],'output':pins(out/'target-census.o'),'limits':['Exact native aggregate total not established','Some parameter fields/constructor callback internals remain uncorrelated','Native allocation/outer/lifetime and widget Create payload unproved','zero property callback only; arbitrary native TFunction construction unsupported','historical asm f captions are image vaddrs, not file offsets; independent PT_LOAD translation required']}
(out/'CENSUS.json').write_text(json.dumps(result,indent=2)+'\n');print('construction census',len(facts),'values;',len(expected),'native-correlated offsets;',len(verified),'instruction bytes checks; size',facts['size'])
