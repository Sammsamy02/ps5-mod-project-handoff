#!/usr/bin/env python3
"""Independent additional actual-method controls; candidate is never modified."""
from pathlib import Path
import hashlib,importlib.util,json,subprocess,sys
sys.dont_write_bytecode=True
OWN=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('parent',OWN/'tests/parent-run.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
h=OWN/'candidate/FField.hpp';c=OWN/'candidate/FField.cpp';source=m.generate(h,c)
source=source.replace('#include <cstdint>','#include <cstdint>\n#include <bit>')
source=source.replace('auto AsUFieldUnsafe() -> UObject* {return nullptr;}','auto AsUFieldUnsafe() -> UObject* {return reinterpret_cast<UObject*>(this);}')
anchors=['auto FFieldVariant::HashObject() -> size_t']
source=source.replace('\n}\nusing namespace RC::Unreal;', '\n'+ '\n'.join(m.block(c.read_text(),a) for a in anchors)+'\n}\nusing namespace RC::Unreal;',1)
source=source.replace('int main(int argc,char**argv)', 'int parent_main(int argc,char**argv)')
source+=r'''
int main(int argc,char**argv){
 if(argc!=2)return 2;std::string mode=argv[1];UObject object{},outer{};FField field{},parent{};bool ok=false;
 if(mode=="pre24_local_set_object"||mode=="pre24_local_set_field"){
  Version::major=4;Version::minor=24;
  if(mode=="pre24_local_set_object"){FFieldVariant v(&field);v.Set(&object);ok=v.IsUObject()&&v.ToUObject()==&object;}
  else{FFieldVariant v(&object);v.Set(&field);ok=!v.IsUObject()&&v.ToField()==&field;}
 }else if(mode=="pre24_outer_fallback"){
  Version::major=4;Version::minor=24;expected_self=reinterpret_cast<UObject*>(&field);expected_outer=&outer;
  auto v=field.GetOwnerVariant();ok=masked_self&&v.IsUObject()&&v.ToUObject()==&outer&&FFieldVariant::StaticSize()==16;
 }else if(mode=="legacy52_wrapper_object"){
  Version::major=5;Version::minor=2;struct Legacy{FFieldVariant owner;uintptr_t next;};Legacy native{FFieldVariant(&parent),0x1122334455667788ULL};
  field.owner=reinterpret_cast<unsigned char*>(&native.owner);field.SetOwnerVariant(&object);auto v=field.GetOwnerVariant();
  ok=v.IsUObject()&&v.ToUObject()==&object&&native.next==0x1122334455667788ULL&&FFieldVariant::StaticSize()==16;
 }else if(mode=="modern_null_variant"){
  FFieldVariant v(static_cast<FField*>(nullptr));auto next=v.GetOwnerVariant();ok=!next.IsValid();
 }else if(mode=="modern_hash"){
  FFieldVariant v(&object);ok=v.HashObject()==reinterpret_cast<uintptr_t>(&object);
 }else{
  if(mode=="transition53_wrapper")Version::minor=3;
  struct alignas(8) Native{uintptr_t owner,next;};Native native{0,0x1122334455667788ULL},parentNative{0,0x8877665544332210ULL};
  field.owner=reinterpret_cast<unsigned char*>(&native.owner);parent.owner=reinterpret_cast<unsigned char*>(&parentNative.owner);
  if(mode=="modern_wrapper_object"||mode=="transition53_wrapper"){field.SetOwnerVariant(&object);auto v=field.GetOwnerVariant();ok=native.owner==(reinterpret_cast<uintptr_t>(&object)|1)&&native.next==0x1122334455667788ULL&&v.ToUObject()==&object;}
  else if(mode=="modern_wrapper_field"){field.SetOwnerVariant(&parent);auto v=field.GetOwnerVariant();ok=native.owner==reinterpret_cast<uintptr_t>(&parent)&&native.next==0x1122334455667788ULL&&v.ToField()==&parent;}
  else if(mode=="modern_wrapper_null"){native.owner=reinterpret_cast<uintptr_t>(&object)|1;field.SetOwnerVariant(static_cast<UObject*>(nullptr));auto v=field.GetOwnerVariant();ok=native.owner==0&&native.next==0x1122334455667788ULL&&!v.IsValid();}
  else if(mode=="modern_nested_outer"){native.owner=reinterpret_cast<uintptr_t>(&parent);parentNative.owner=reinterpret_cast<uintptr_t>(&object)|1;ok=field.GetOutermostOwner()==&object;}
  else if(mode=="modern_null_outer"){ok=field.GetOutermostOwner()==nullptr;}
  else if(mode=="modern_chain_null_outer"){native.owner=reinterpret_cast<uintptr_t>(&parent);ok=field.GetOutermostOwner()==nullptr;}
  else return 2;
 }
 if(!ok){std::cerr<<"FAIL "<<mode<<"\n";return 1;}std::cout<<"PASS "<<mode<<"\n";return 0;
}
'''
p=OWN/'tests/independent_methods.cpp';p.write_text(source)
cases=['pre24_local_set_object','pre24_local_set_field','pre24_outer_fallback','legacy52_wrapper_object','modern_null_variant','modern_hash','transition53_wrapper','modern_wrapper_object','modern_wrapper_field','modern_wrapper_null','modern_nested_outer','modern_null_outer','modern_chain_null_outer']
rows=[]
for sanitized in (False,True):
 exe=OWN/'tests'/('independent-san' if sanitized else 'independent-plain');argv=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1',str(p),'-o',str(exe)]
 if sanitized:argv+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 r=subprocess.run(argv,capture_output=True,text=True);rows.append(dict(kind='compile',sanitized=sanitized,argv=argv,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
 if r.returncode:print(r.stderr);break
 for case in cases:
  argv=[str(exe),case];r=subprocess.run(argv,capture_output=True,text=True);rows.append(dict(kind='case',sanitized=sanitized,name=case,argv=argv,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
summary={case:[r['exit'] for r in rows if r.get('name')==case] for case in cases}
receipt={'scope':'original requested candidate, independent additional extracted methods; host engine/version storage doubles only','cases':len(cases),'records':rows,'summary':summary,'extracted_methods':anchors,'sources':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in(h,c,p,Path(__file__))}}
(OWN/'INDEPENDENT-CASES.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(summary));sys.exit(int(any(r['exit'] for r in rows)))
