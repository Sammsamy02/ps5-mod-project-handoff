#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,runpy,subprocess,configparser,copy
root=Path(__file__).resolve().parents[1];ws=root.parents[2];old=ws/'outputs/2026-10-08-l22-runtime/profile';prior=ws/'outputs/2026-10-07-l19-local-profile-r4';out=root/'tests'
g=runpy.run_path(str(root/'generate_overlay.py'));join=g['join'];sha=g['sha']
im=json.loads((root/'evidence/image-measurements.json').read_text());changes=json.loads((root/'CHANGES.json').read_text())
p=json.loads((root/'profiles/townfall-build-facts.json').read_text());base=json.loads((old/'profiles/townfall-build-facts.json').read_text())
assert p['required_members'][:69]==base['required_members'] and p['required_vtables']==base['required_vtables']
assert len(p['required_members'])+len(p['required_vtables'])==93+changes['new_promoted_facts']
for f in changes['files']:assert sha(Path(f['original']))==f['original_sha256'] and sha(Path(f['candidate']))==f['candidate_sha256']
texts=[json.loads(x.rstrip(',')) for x in (root/'units/CompiledNativeLayoutDeclarations.hpp').read_text().splitlines() if x.startswith('"')]
digest=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest();assert digest==texts[3]==changes['profile_digest']
assert digest in (root/'units/RuntimeBuildProfile.hpp').read_text()
fixtures=[]
for typ,ini in zip(('members','vtables'),texts):
 c=configparser.ConfigParser();c.optionxform=str;c.read_string(ini)
 for section in c:
  if section in ('DEFAULT','NativeLayout'):continue
  for key,v in c[section].items():fixtures.append(typ+'.push_back({'+json.dumps(section+'::'+key)+','+str(int(v,0))+'});')
code=r'''
#include "JoinedLayoutPreflight.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
struct Fact {std::string name;uint32_t offset;bool operator==(const Fact&)const=default;};
int main(int argc,char**argv){assert(argc==2);std::string mode=argv[1];std::vector<Fact> members,vtables;
FIXTURE
 auto remove=[&](const char*n){std::erase_if(members,[&](const auto&x){return x.name==n;});};
 bool expected=true;
 if(mode=="inner_missing"){remove("FArrayProperty::Inner");expected=false;}
 else if(mode=="inner_mismatch"){for(auto&x:members)if(x.name=="FArrayProperty::Inner")++x.offset;expected=false;}
 else if(mode=="optional_absent"){remove("FMapProperty::KeyProp");remove("FMapProperty::ValueProp");remove("FSetProperty::ElementProp");}
 else if(mode=="multi_missing"){remove("FArrayProperty::Inner");remove("FField::ClassPrivate");expected=false;}
 else if(mode=="conditional_mismatch"){for(auto&x:vtables)if(x.name=="FProperty::GetMinAlignment")++x.offset;expected=false;}
 else if(mode!="baseline")return 3;
 const auto before=members;const auto before_v=vtables;
 const auto r=rc::joinedlayout::evaluate(&members,&vtables);
 assert(r.checks.size()==66&&r.required==50&&r.conditional==14&&r.deferred==2);assert(r.ok==expected);
 assert(before==members&&before_v==vtables);
 assert(!rc::joinedlayout::conditional_absent(rc::joinedlayout::Kind::Member,"FArrayProperty::Inner",&members));
 if(mode=="multi_missing")assert(r.missing_required==2);
 if(mode=="conditional_mismatch")assert(r.missing_required==0);
 if(mode=="optional_absent")for(const char*n:{"FMapProperty::KeyProp","FMapProperty::ValueProp","FSetProperty::ElementProp"})assert(rc::joinedlayout::conditional_absent(rc::joinedlayout::Kind::Member,n,&members));
 std::cout<<"PASS "<<mode<<" 66 keys, 50 required, 14 conditional, 2 deferred; no mutation\n";
}
'''.replace('FIXTURE','\n'.join(fixtures))
(out/'preflight.cpp').write_text(code);commands=[]
def run(cmd,ok=True):
 r=subprocess.run(cmd,capture_output=True,text=True);commands.append({'argv':[str(x) for x in cmd],'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});assert (r.returncode==0)==ok,commands[-1];return r
run(['clang++','-std=c++20','-fsanitize=address,undefined','-I'+str(root/'units'),str(out/'preflight.cpp'),'-o',str(out/'preflight')])
for scenario in ('baseline','inner_missing','inner_mismatch','optional_absent','multi_missing','conditional_mismatch'):run([str(out/'preflight'),scenario])
(out/'digest_gate.cpp').write_text('#include "NativeLayoutDeclarations.hpp"\nint main(){}\n')
cmd=['clang++','-std=c++20','-I'+str(root/'units'),'-I'+str(prior/'units'),'-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(root/'units/CompiledNativeLayoutDeclarations.hpp')+'"',str(out/'digest_gate.cpp'),'-o',str(out/'digest_gate')];run(cmd)
bad=cmd.copy();bad[2]='-I'+str(old/'units');r=run(bad,False);assert 'Compiled native declarations must match' in r.stderr
controls=[]
def reject(name,a,b):
 try:join(a,b)
 except ValueError as e:controls.append({'name':name,'refused':True,'reason':str(e)})
 else:raise AssertionError(name+' accepted')
assert join(im,None)==[];controls.append({'name':'missing official receipt','promoted':0})
# Synthetic receipts only test the decision function; they are never used for artifact generation.
fake={'status':'TARGET_COMPILER_MEASURED','target':'x86_64-sie-ps5','official_commit':'6978b63c8951e57d97048d8424a0bebd637dde1d','source_pins':{'fixture':'fixture'},'compiler_receipt':'fixture','facts':[{k:x[k] for k in ('name','offset','width')} for x in im['facts']]}
assert len(join(im,fake))==3;controls.append({'name':'synthetic exact two-source decision control','promoted':3,'artifact_input':False})
a=copy.deepcopy(im);a['image']['sha256']='0'*64;reject('wrong image',a,fake)
a=copy.deepcopy(fake);a['facts'][0]['offset']+=8;reject('conflicting offset',im,a)
a=copy.deepcopy(fake);a['facts'][0]['width']=4;reject('conflicting width',im,a)
a=copy.deepcopy(fake);a['facts'].pop();reject('missing array calibration',im,a)
a=copy.deepcopy(fake);a['facts'].pop(0);reject('missing new fact',im,a)
a=copy.deepcopy(fake);a['status']='SOURCE_ONLY';reject('source only',im,a)
a=copy.deepcopy(fake);a['target']='host';reject('wrong target',im,a)
a=copy.deepcopy(fake);a['official_commit']='0'*40;reject('wrong source commit',im,a)
a=copy.deepcopy(fake);a['source_pins']={};reject('missing source provenance',im,a)
a=copy.deepcopy(fake);a['facts'].append(a['facts'][0]);reject('duplicate source facts',im,a)
a=copy.deepcopy(im);a['facts'].append(a['facts'][0]);reject('duplicate image facts',a,fake)
(out/'RESULTS.json').write_text(json.dumps({'status':'PASS_HOST_ONLY','prior93_retained':True,'new_promoted':changes['new_promoted_facts'],'profile_digest':digest,'commands':commands,'join_controls':controls,'limits':'No live engine calls; target full packaging belongs to parent. Official readiness is separately recorded in CHANGES.json.'},indent=2)+'\n')
print('PASS host preflight6, digest pair+negative compile, strict join13 controls, prior93 immutable')
