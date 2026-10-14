#!/usr/bin/env python3
"""Local-only evidence census. No console access and no target code generation.
A binary error literal proves linkage, not call-graph reachability.
Unresolved entries are never relabelled unreachable to make a build pass.
"""
import ast,collections,configparser,hashlib,json,pathlib,re,struct,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[3]
OUT=pathlib.Path(__file__).resolve().parent
BARE=ROOT/'outputs/2026-10-08-menu-complete-package/pass-1/bare-sdk/modbridge_supplier_composition.prx'
VIRTUAL=re.compile(rb'Virtual ([^\x00\r\n]+?) is unavailable, possibly unsupported in engine version')
MEMBER=re.compile(rb"Tried getting member variable '([^'\x00]+)' that doesn't exist in this engine version(?: or its parent class)?\.")

def sha(b):return hashlib.sha256(b).hexdigest()
def uncomment(t):
 t=re.sub(r'/\*.*?\*/',lambda m:'\n'*m.group().count('\n'),t,flags=re.S)
 return re.sub(r'//[^\n]*','',t)
def extract(b):
 return {(k,m.group(1).decode()):m.start() for k,rx in [('virtual',VIRTUAL),('member',MEMBER)] for m in rx.finditer(b)}
def evaluate(entries,facts,unreachable=None):
 """Collect all omissions; an exemption needs a reason AND evidence reference."""
 unreachable=unreachable or {};rows=[]
 for kind,name in sorted(entries):
  fact=facts.get((kind,name));proof=unreachable.get((kind,name),{})
  if fact is not None and fact.get('source') and fact.get('offset') is not None:
   state='measured_in_existing_profile';reason='Retained profile fact; provenance below. Not a new native measurement.'
  elif proof.get('reason') and proof.get('evidence'):
   state='unreachable_for_frozen_scope';reason=proof['reason']
  else:state='UNRESOLVED';reason='No measured profile entry and no reviewed proof of unreachability; cannot approve build.'
  rows.append({'kind':kind,'name':name,'status':state,'reason':reason,'profile':fact,'unreachable_proof':proof or None})
 return rows

def main():
 b=BARE.read_bytes();assert b[:4]==b'\x7fELF';assert sha(b)=='3d83d03a681ce8ce2f498f4879b6e99a4c3d8bf62ea45cfb9e3ec7068b23db5e'
 # User-requested strings output independently compared to the binary-byte regex.
 r=subprocess.run(['strings','-a','-t','x',str(BARE)],check=True,text=True,capture_output=True)
 (OUT/'bare-strings.txt').write_text(r.stdout);found=extract(b);other=extract(r.stdout.encode());assert set(found)==set(other)
 phoff=struct.unpack_from('<Q',b,32)[0];phentsize,phnum=struct.unpack_from('<HH',b,54)
 loads=[]
 for i in range(phnum):
  typ,flags,off,va,pa,fs,ms,align=struct.unpack_from('<IIQQQQQQ',b,phoff+i*phentsize)
  if typ==1:loads.append({'flags':flags,'file_start':off,'file_end':off+fs,'vaddr':va,'memsize':ms})
 profile_path=ROOT/'outputs/2026-10-07-l19-local-profile-r4/profiles/townfall-build-facts.json';profile=json.loads(profile_path.read_text())
 facts={(kind,x['name']):x for kind,key in [('member','required_members'),('virtual','required_vtables')] for x in profile[key]}
 # Independently parse actual compiled INI string literals and require agreement.
 decl_path=ROOT/'outputs/2026-10-07-l19-local-profile-r4/units/CompiledNativeLayoutDeclarations.hpp';dt=decl_path.read_text()
 lits=[ast.literal_eval(m.group()) for m in re.finditer(r'"(?:\\.|[^"\\])*"',dt)];texts=[x for x in lits if x.startswith('[NativeLayout]')];assert len(texts)==2
 declared={}
 for kind,t in zip(['member','virtual'],texts):
  cfg=configparser.ConfigParser(interpolation=None);cfg.optionxform=str;cfg.read_string(t)
  for cls in cfg.sections():
   if cls!='NativeLayout':
    for field,value in cfg[cls].items():declared[(kind,cls+'::'+field)]=int(value,0)
 assert len(declared)==len(facts)==76
 assert all(declared[k]==x['offset'] for k,x in facts.items())
 sel_path=ROOT/'outputs/2026-10-08-menu-complete-build/SOURCE-SELECTION.json';sel=json.loads(sel_path.read_text());comp=json.loads((ROOT/'outputs/2026-10-08-menu-complete-build/pass1/COMPOSITION-RECEIPT.json').read_text())
 selected_units={str(pathlib.Path(u['source']).resolve()):u for u in comp['selected_units']};pin_by_path={str(pathlib.Path(x['path']).resolve()):x for x in sel['source_pins']}
 direct=[];macro_reads=[];setter_lines=0;wrapper_lines=0
 for path,pin in pin_by_path.items():
  p=pathlib.Path(path)
  if p.suffix not in ['.hpp','.cpp','.h','.c','.inc','.inl']:continue
  raw=p.read_bytes();t=raw.decode(errors='replace')
  if not any(x in t for x in ['MemberOffsets','VTableLayoutMap','GET_ADDRESS_OF_UNREAL_VIRTUAL']):continue
  assert sha(raw)==pin['sha256'],path
  active=path in selected_units or p.suffix not in ['.cpp','.c']
  for n,line in enumerate(uncomment(t).splitlines(),1):
   if re.search(r'(?:VTableLayoutMap|MemberOffsets)\s*(?:\.\s*(?:find|at|contains)\s*\(|\[)',line):
    if 'DefaultSetter' in p.name or 'FunctionBodies/' in path:setter_lines+=1;continue
    if 'SrcWrapper' in p.name:wrapper_lines+=1;continue
    if 'UnrealVirtualBaseVC.hpp' in p.name:category='macro_definition'
    elif not active:category='retained_source_not_selected_TU'
    else:category='direct_selected_lookup'
    direct.append({'path':str(p.relative_to(ROOT)),'line':n,'code':line.strip(),'sha256':pin['sha256'],'category':category})
   if active and p.suffix in ['.cpp','.c']:
    m=re.search(r'((?:OPTIONAL_)?GET_ADDRESS_OF_UNREAL_VIRTUAL)\s*\(\s*(\w+)\s*,\s*(\w+)\s*,',line)
    if m:macro_reads.append({'kind':'virtual','name':m[2]+'::'+m[3],'optional':m[1].startswith('OPTIONAL'),'path':str(p.relative_to(ROOT)),'line':n,'code':line.strip()})
 origins=collections.defaultdict(list)
 for key,off in found.items():
  seg=[x for x in loads if x['file_start']<=off<x['file_end']];assert seg,('literal outside PT_LOAD',key,off)
  origins[key].append({'source':'binary_error_literal','file_offset':hex(off),'load_flags':seg[0]['flags']})
 for k in facts:origins[k].append({'source':'existing_required_profile','path':str(profile_path.relative_to(ROOT))})
 for row in macro_reads:origins[('virtual',row['name'])].append({'source':'direct_virtual_lookup',**row})
 # Explicit direct readers not guaranteed to leave failure literals. Entries already in binary are merged.
 custom=[('member','FProperty::RepNotifyFunc'),('member','UObjectBase::InternalIndex_Private'),('member','FUObjectHashTables::PackageToObjectListMap'),('member','FClassProperty::MetaClass'),('member','FSoftClassProperty::MetaClass')]
 for key in custom:origins[key].append({'source':'direct_map_reader','evidence':'OTHER-MAP-READERS.json; FOptionalProperty comment stripped, actual guards retained'})
 rows=evaluate(origins,facts)
 for row in rows:row['origins']=origins[(row['kind'],row['name'])]
 summary={'scope':'Mac-only linked lookup + selected-source reader census; no target build or console contact','binary_path':str(BARE),'bare_sha256':sha(b),'raw_string_virtuals':sum(k[0]=='virtual' for k in found),'raw_string_members':sum(k[0]=='member' for k in found),'python_bytes_equals_system_strings':True,'profile_source':str(profile_path),'profile_sha256':sha(profile_path.read_bytes()),'compiled_declarations_sha256':sha(decl_path.read_bytes()),'compiled_declarations_equal_profile':True,'existing_profile_entries':len(facts),'union_entries':len(rows),'status_counts':dict(collections.Counter(x['status'] for x in rows)),'raw_default_setter_reader_lines':setter_lines,'raw_generated_wrapper_reader_lines':wrapper_lines,'explicit_other_reader_lines':len(direct),'direct_virtual_macro_sites':len(macro_reads),'build_allowed':False,'runtime_gate_changed':False,'reason':'Reachability/measurement requirements incomplete; conservative admission audit refuses all unresolved entries together. This audit is not yet a replacement of the deployed startup gate.','coverage_limits':['Error literals prove linkage, not reachability. No use of grep absence to certify unreachable.','Generated default-setter lookups are provisioning and not evidence all version tables are selected on Prospero.','Direct map reader census strips comments and distinguishes actual selected TUs from retained source pins.','No whole-program dynamic/reflection reachability proof exists; unclassified linked entries remain unresolved.']}
 (OUT/'FULL-INVENTORY.json').write_text(json.dumps(rows,indent=2)+'\n');(OUT/'OTHER-MAP-READERS.json').write_text(json.dumps({'readers':direct,'virtual_macro_calls':macro_reads},indent=2)+'\n');(OUT/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n')
 lines=['kind\tname\tstatus\tprofile_offset\tprovenance\treason']
 for x in rows:
  f=x['profile'] or {};lines.append('\t'.join([x['kind'],x['name'],x['status'],hex(f['offset']) if 'offset'in f else '',f.get('source',''),x['reason']]))
 (OUT/'FULL-INVENTORY.tsv').write_text('\n'.join(lines)+'\n')
 unresolved=[x for x in rows if x['status']=='UNRESOLVED'];(OUT/'ALL-MISSING.txt').write_text('\n'.join(f"MISSING {x['kind']} {x['name']}" for x in unresolved)+f'\nREFUSED: {len(unresolved)} unresolved facts; no target build\n')
 print(json.dumps({k:summary[k] for k in ['raw_string_virtuals','raw_string_members','union_entries','status_counts','build_allowed','runtime_gate_changed']},indent=2))
if __name__=='__main__':main()
