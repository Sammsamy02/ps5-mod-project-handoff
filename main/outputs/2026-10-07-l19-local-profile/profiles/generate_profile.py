#!/usr/bin/env python3
"""LOCAL ONLY: validate retained image facts and generate a typed per-game profile."""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,re
root=Path(__file__).resolve().parents[1];workspace=root.parents[1]
image=workspace/'outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin'
classification=workspace/'outputs/2026-10-07-l18-axes-fact-review/20261007T211427Z-9e46e87d/CLASSIFICATION.json'
worker=workspace/'outputs/2026-10-04-zcode-team/worker-axes-detector'
identity=workspace/'outputs/2026-10-04-zcode-team/worker-layout-identity/IDENTITY-RESOLUTION.md'
def validate_profile(p):
 expected={'schema','name','image','layout','axes','fname','evidence','required_members','required_vtables','unknown_required_static_facts'}
 if set(p)!=expected:raise ValueError('unexpected/missing profile schema field')
 for key,allowed in [('image',{'sha256','size','extent','base'}),('layout',{'format','target','pointer_size','major','minor','engine_revision','build_id'}),('axes',{'build_configuration','stats','build_source','stats_source'}),('fname',{'case_preserving','output_bytes','output_alignment','source'})]:
  if set(p[key])!=allowed:raise ValueError('unexpected/missing '+key+' schema field')
 if type(p['schema']) is not int or p['schema']!=1:raise ValueError('unsupported profile schema')
 if not isinstance(p['name'],str) or not p['name']:raise ValueError('profile name required')
 if not isinstance(p['image']['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',p['image']['sha256']):raise ValueError('full image SHA256 required')
 for obj,keys in [(p['image'],['size','extent','base']),(p['layout'],['format','pointer_size','major','minor']),(p['axes'],['build_configuration','stats']),(p['fname'],['output_bytes','output_alignment'])]:
  for key in keys:
   if type(obj[key]) is not int or obj[key]<0:raise ValueError('unsigned integer '+key+' required')
 for key in ['target','engine_revision','build_id']:
  if not isinstance(p['layout'][key],str) or not p['layout'][key]:raise ValueError('explicit '+key+' required')
 for group in ['required_members','required_vtables']:
  seen=set()
  for pin in p[group]:
   if set(pin)!={'name','offset','width','source'} or not isinstance(pin['name'],str) or '::' not in pin['name'] or pin['name'] in seen:raise ValueError('complete unique layout pin required')
   seen.add(pin['name'])
   if not isinstance(pin['source'],str):raise ValueError('layout pin source must be string')
   if pin['width'] is not None and (type(pin['width']) is not int or not 1<=pin['width']<=0x7fffffff):raise ValueError('positive bounded measured width required')
   if pin['offset'] is not None and (type(pin['offset']) is not int or not 0<=pin['offset']<=0x7fffffff or not pin['source']):raise ValueError('bounded measured offset and source required')
 if type(p['fname']['case_preserving']) is not bool:raise ValueError('case-preserving must be explicit bool')
 if p['axes']['build_configuration'] not in [1,2,3] or p['axes']['stats'] not in [1,2]:raise ValueError('independent known axes required')
 if not isinstance(p['axes']['build_source'],str) or not isinstance(p['axes']['stats_source'],str) or not p['axes']['build_source'] or not p['axes']['stats_source'] or p['axes']['build_source']==p['axes']['stats_source']:raise ValueError('independent axis provenance required')
 if not p['required_members'] or not p['required_vtables']:raise ValueError('required runtime layout facts must be declared')
 return p
def generate(p):
 validate_profile(p);digest=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest();i=p['image'];l=p['layout'];a=p['axes'];f=p['fname'];q=lambda s:json.dumps(s)
 h=['#pragma once','#include "BuildFactsChecks.hpp"','// Generated from evidence-backed JSON; all game-specific facts live here.','namespace rc::runtimeprofile {',f'inline constexpr const char* kDigest={q(digest)};','inline constexpr buildfacts::Profile kProfile{',f' 1,{q(p["name"])},kDigest,{q(i["sha256"])},{i["size"]}ull,{i["extent"]}ull,{i["base"]}ull,',f' {l["format"]},{l["pointer_size"]},{l["major"]},{l["minor"]},{q(l["target"])},{q(l["engine_revision"])},{q(l["build_id"])},',f' {a["build_configuration"]},{a["stats"]},{q(a["build_source"])},{q(a["stats_source"])},',f' true,{str(f["case_preserving"]).lower()},{f["output_bytes"]},{f["output_alignment"]}','};']
 for key in ['members','vtables']:
  h.append('inline constexpr buildfacts::LayoutPin kRequired'+key.title()+'[]={'+','.join('{'+q(v['name'])+','+str(v['offset'] is not None).lower()+','+str(v['offset'] or 0)+','+str(v['width'] or 0)+','+q(v['source'])+'}' for v in p['required_'+key])+'};')
 h.append('inline constexpr const char* kUnknownRequiredStaticFacts[]={'+','.join(q(v) for v in p['unknown_required_static_facts'])+'};')
 h+=['}',''];(root/'units/RuntimeBuildProfile.hpp').write_text('\n'.join(h));return digest
if __name__=='__main__':
 b=image.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc' and len(b)==194241424
 assert b[:7]==b'\x7fELF\x02\x01\x01';off=struct.unpack_from('<Q',b,32)[0];width,n=struct.unpack_from('<HH',b,54);assert width==56
 ph=[struct.unpack_from('<IIQQQQQQ',b,off+i*width) for i in range(n)];assert off+n*width<=len(b)
 paths=[]
 for t,flags,o,v,pa,size,mem,align in ph:
  assert o<=len(b) and size<=len(b)-o
  if t==0x6fffff00:
   payload=b[o:o+size];assert payload[:4]==b'PATH';record_length,string_length=struct.unpack_from('<II',payload,4);assert record_length==size-8 and string_length>0 and 12+string_length<=size
   s=payload[12:12+string_length];assert s[-1:]==b'\0';paths.append({'phdr_type':hex(t),'offset':o,'size':size,'record_length':record_length,'path':s[:-1].decode('ascii')})
 assert len(paths)==1 and paths[0]['path'].endswith('Townfall-PS5-Shipping.self')
 extent=max(v+mem for t,flags,o,v,pa,size,mem,align in ph if t==1);assert extent==0xbc602c8
 assert b[0x1f37db:0x1f37e7]==bytes.fromhex('c7030500060066c743040100')
 assert b[0x40e0:0x40e8]==bytes.fromhex('554889e531c05dc3') and b[0x782a782:0x782a78a]=='UE5\0'.encode('utf-16le')
 assert b[0xb93b250:0xb93b274]==bytes.fromhex('040000001400000003000000474e550004bb4330a7d71dcefe1cc2c4d19769d500000000')
 cmds=[[str(classification.parent/'test_axes_detector'),str(image)],[sys.executable,str(worker/'tools/ref_axes.py'),str(workspace/'.superpowers/native-dependency-spike/ue4ss-native-patternsleuth-1/upstream/patternsleuth/src/resolvers/unreal/engine_version.rs'),str(image)]];rows=[]
 for cmd in cmds:
  r=subprocess.run(cmd,capture_output=True,text=True);rows.append({'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});assert r.returncode==0
 c=json.loads(next(s[len('T4 RESULT '):] for s in rows[0]['stdout'].splitlines() if s.startswith('T4 RESULT ')));py=json.loads(rows[1]['stdout']);assert c['build_configuration']==py['build_configuration']==1 and c['stats']==py['stats']==1 and c['present_count']==py['present_count']==54 and c['stats_axis']['margin']==12
 old=json.loads(classification.read_text());assert old['agreement'] and old['image']['sha256']==sha
 sources=[classification,identity,worker/'src/axes_detector.c',worker/'src/axes_detector.h',worker/'src/axes_table.h',worker/'tools/ref_axes.py',root/'units/FNameAbiProfile.hpp']
 evidence={'sources':{str(p.relative_to(workspace)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'shipping_path':paths[0],'fresh_c_result':c,'fresh_python_result':py,'engine_revision_kind':'semantic reconstruction from independently verified image components','build_id_kind':'ELF GNU build-id significant 16 bytes (note descriptor has trailing zeroes)'}
 inventory_path=workspace/'outputs/2026-10-07-l19-fact-inventory/FACTS.json';inventory=json.loads(inventory_path.read_text())
 sources+=[inventory_path,workspace/'outputs/2026-10-04-zcode-team/worker-emit-final-pair/emission-final/MemberVariableLayout.ini',workspace/'outputs/2026-10-04-zcode-team/worker-gap-l2a/staged/VTableLayout.ini']
 evidence['sources']={str(p.relative_to(workspace)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
 pins={}
 for kind in ['member','vtable']:
  pins[kind]=[]
  for fact in inventory['facts']:
   if not fact['id'].startswith(kind+':') or fact['requirement']!='required':continue
   name=fact['id'].split(':',1)[1].replace('.', '::',1)
   value=fact['current_value'];offset=int(value,0) if isinstance(value,str) and value.startswith('0x') else None
   pins[kind].append({'name':name,'offset':offset,'width':None,'source':'retained authenticated image/static evidence' if offset is not None else ''})
 static_resolved={}
 for filename in ['STATIC-FACTS.json','REFLECTION-FACTS.json']:
  static_path=workspace/'outputs/2026-10-07-l19-static-layout-check'/filename
  static=json.loads(static_path.read_text());assert static['image']['sha256']==sha
  for key,window in static['evidence'].items():
   offset=int(window['file_byte_offset'],0);vaddr=int(window['elf_image_vaddr'],0);raw=bytes.fromhex(window['instruction_bytes']);assert b[offset:offset+len(raw)]==raw
   loads=[h for h in ph if h[0]==1 and h[3]<=vaddr and vaddr-h[3]+len(raw)<=h[5]];assert len(loads)==1 and offset==loads[0][2]+vaddr-loads[0][3]
  for fact in static['facts']:
   if fact['status']!='validated-static' or fact['confidence']!='high':continue
   static_resolved[fact['key']]=fact
   matches=[pin for pin in pins['member'] if pin['name']==fact['key'].replace('.', '::',1)]
   for pin in matches:pin.update(offset=int(fact['value'],0),width=fact['width_bytes'],source=str(static_path.relative_to(workspace))+':'+fact['key'])
  sources.append(static_path);evidence['sources'][str(static_path.relative_to(workspace))]=hashlib.sha256(static_path.read_bytes()).hexdigest()
  evidence['static_layout_windows_verified_'+filename]=len(static['evidence'])
 assert static_resolved['object_storage.native_chunked_65536']['value'] is True
 assert static_resolved['FField.owner_variant_encoding']['value']=={'storage_bytes':8,'tag_bit':0,'tag_set_kind':'UObject','tag_clear_kind':'FField','untag_mask':'0xfffffffffffffffe'}
 evidence['resolved_runtime_static_facts']={k:static_resolved[k] for k in ['object_storage.native_chunked_65536','FField.owner_variant_encoding']}
 required_names={v['name'].replace('::','.',1) for v in pins['member']}
 unknown=['accessor_width.'+v for v in ['UStruct.SuperStruct','UClass.ClassFlags','UClass.ClassCastFlags','FField.Owner','FField.Next','FField.NamePrivate','FFieldClass.CastFlags','FProperty.PropertyFlags','UFunction.FunctionFlags'] if v in required_names and v not in static_resolved]
 p={'schema':1,'name':'townfall-ps5-ue56-shipping-stats-off','image':{'sha256':sha,'size':len(b),'extent':extent,'base':0x400000},'layout':{'format':1,'target':'prospero-x86_64-sysv','pointer_size':8,'major':5,'minor':6,'engine_revision':'5.6.1-0+++UE5','build_id':'04bb4330a7d71dcefe1cc2c4d19769d5'},'axes':{'build_configuration':1,'stats':1,'build_source':'PT_SCE PATH:Townfall-PS5-Shipping.self','stats_source':'independent 244-marker classification:54 present;off margin12;C+Python agreement'},'fname':{'case_preserving':False,'output_bytes':8,'output_alignment':8,'source':'retained independently authenticated FNameAbiProfile + l18 native roundtrip'},'evidence':evidence,'required_members':pins['member'],'required_vtables':pins['vtable'],'unknown_required_static_facts':unknown}
 (root/'profiles/townfall-build-facts.json').write_text(json.dumps(p,indent=2)+'\n');digest=generate(p)
 (root/'reports/EVIDENCE.json').write_text(json.dumps({'profile_digest':digest,'profile_path':'profiles/townfall-build-facts.json','image':p['image'],'facts':evidence,'commands':rows},indent=2)+'\n');print('PASS verified parser shipping PATH + independent C/Python STATS, digest='+digest)
