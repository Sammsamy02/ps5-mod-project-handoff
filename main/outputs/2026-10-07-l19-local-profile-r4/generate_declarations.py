#!/usr/bin/env python3
"""Generate local declarations from a frozen authenticated profile; never publish maps."""
import argparse, hashlib, json, re
from pathlib import Path


def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def integer(value, positive=False): return type(value) is int and (1 if positive else 0) <= value <= 0x7fffffff

def token(value):
 if not isinstance(value,str) or not re.fullmatch(r'[!-~]{1,512}',value) or any(c in value for c in ';#=[]'):
  raise ValueError('ASCII metadata token required')
 return value

def build(profile, schema, widths, require_complete=False):
 if type(profile.get('schema')) is not int or profile['schema']!=1: raise ValueError('Unsupported profile schema')
 axes=profile['axes']; layout=profile['layout']
 if type(axes.get('build_configuration')) is not int or axes['build_configuration'] not in (1,2,3) or type(axes.get('stats')) is not int or axes['stats'] not in (1,2):
  raise ValueError('Independent resolved axes required')
 for key in ('build_source','stats_source'):
  if not isinstance(axes.get(key),str) or not axes[key]: raise ValueError('Independent axis provenance required')
 if type(layout.get('format')) is not int or layout['format']!=1 or type(layout.get('pointer_size')) is not int or layout['pointer_size']!=8 or layout.get('target')!='prospero-x86_64-sysv':
  raise ValueError('Unsupported native format/target/pointer ABI')
 if not integer(layout.get('major')) or layout['major']<4 or not integer(layout.get('minor')): raise ValueError('Integer engine version required')
 for key in ('target','engine_revision','build_id'): token(layout[key])
 members_schema={}
 for section,key,target,canonical,bitfield in re.findall(r'\{"([\w:]+)", "(\w+)", "([\w:]+)", "(\w+)", (true|false)\}',schema):
  members_schema[section+'::'+key]=(target+'::'+canonical,section,key,bitfield=='true')
 virtual_classes=set(re.findall(r'\{"([\w:]+)", &Unreal::[\w:]+::VTableLayoutMap\}',schema))
 if not members_schema or not virtual_classes: raise ValueError('Actual binding schema unavailable')
 known_members={};known_vtables={};unresolved=[]; seen=set(); all_non_total=set()
 for kind,rows in (('member',profile['required_members']),('virtual',profile['required_vtables'])):
  for row in rows:
   name=row.get('name'); offset=row.get('offset'); native_width=row.get('width'); source=row.get('source')
   if not isinstance(source,str): raise ValueError('String fact source required: '+str(name))
   if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z_]\w*(?:::[A-Za-z_]\w*)+',name): raise ValueError('Invalid fact name')
   if kind=='member':
    if name not in members_schema: raise ValueError('Unsupported member: '+name)
    canonical,section,key,bitfield=members_schema[name]
    if bitfield: raise ValueError('Bitfield tuple not supplied by profile: '+name)
    if not canonical.endswith('::UEP_TotalSize'): all_non_total.add(canonical)
   else:
    section,key=name.rsplit('::',1);canonical=name
    if section not in virtual_classes: raise ValueError('Unsupported virtual class: '+section)
   identity=(kind,canonical)
   if identity in seen: raise ValueError('Duplicate canonical '+kind+': '+canonical)
   seen.add(identity)
   if native_width is not None and not integer(native_width,positive=True): raise ValueError('Positive native width required: '+name)
   if offset is None:
    if source or native_width is not None: raise ValueError('Unresolved fact cannot claim source/width: '+name)
    unresolved.append(canonical); continue
   if not integer(offset): raise ValueError('Integer offset required: '+name)
   if not isinstance(source,str) or not source: raise ValueError('Known fact requires source: '+name)
   if kind=='virtual':
    if offset%layout['pointer_size']: raise ValueError('Aligned virtual byte offset required: '+name)
    if native_width not in (None,layout['pointer_size']): raise ValueError('Virtual width conflicts with pointer ABI: '+name)
    known_vtables[canonical]={'section':section,'key':key,'offset':offset}; continue
   width=0
   if not canonical.endswith('::UEP_TotalSize'):
    width=widths.get(canonical)
    if not integer(width,positive=True): raise ValueError('Missing compiled accessor width: '+canonical)
    if native_width is not None and width<native_width: raise ValueError('Compiled footprint is narrower: '+canonical)
   elif offset==0: raise ValueError('Positive measured total size required: '+name)
   known_members[canonical]={'section':section,'key':key,'offset':offset,'compiled_width':width,'native_width':native_width}
 if set(widths)!=all_non_total: raise ValueError('Compiled width census key set conflicts with required profile')
 static=profile.get('unknown_required_static_facts',[])
 if not isinstance(static,list) or any(not isinstance(x,str) or not x for x in static): raise ValueError('Invalid unresolved static facts')
 unresolved+=static
 classes={name.rsplit('::',1)[0] for name in known_members}
 blocked=[]; geometry=[]
 for cls in sorted(classes):
  total=known_members.get(cls+'::UEP_TotalSize')
  if total is None: blocked.append(cls);continue
  for name,row in known_members.items():
   if name.rsplit('::',1)[0]!=cls or name.endswith('::UEP_TotalSize'): continue
   if row['offset']+row['compiled_width']>total['offset']: raise ValueError('Outside measured class size: '+name)
   geometry.append({'name':name,'offset':row['offset'],'native_width':row['native_width'],'compiled_footprint':row['compiled_width'],'measured_total':total['offset']})
 unresolved=sorted(unresolved)
 ready=not unresolved and not blocked
 if require_complete and not ready: raise ValueError('Incomplete authenticated profile: '+', '.join(unresolved))
 identity='[NativeLayout]\n'+'\n'.join(k+'='+str(v) for k,v in (
  ('Format',layout['format']),('Target',layout['target']),('PointerSize',layout['pointer_size']),('EngineMajor',layout['major']),('EngineMinor',layout['minor']),
  ('NonShipping',int(axes['build_configuration']!=1)),('Stats',int(axes['stats']==2)),('EngineRevision',layout['engine_revision']),('BuildId',layout['build_id'])))+'\n'
 def render(rows,excluded=()):
  groups={}
  for row in rows.values():
   if row['section'] not in excluded: groups.setdefault(row['section'],[]).append(row)
  text=identity
  for section,entries in sorted(groups.items()):
   text+='\n['+section+']\n'
   text+=''.join(row['key']+'='+hex(row['offset'])+'\n' for row in sorted(entries,key=lambda x:x['key']))
  return text
 return {'members':render(known_members),'vtables':render(known_vtables),'stageable_members':render(known_members,blocked),
  'manifest':{'ready':ready,'status':'COMPLETE-LOCAL-DECLARATIONS' if ready else 'HOLD-UNRESOLVED-NATIVE-FACTS','known_members':len(known_members),'known_vtables':len(known_vtables),
   'unresolved':unresolved,'blocked_member_classes':blocked,'geometry':geometry,
   'stageable_subset_excluded_known_members':[n for n,r in sorted(known_members.items()) if r['section'] in blocked],
   'stageable_subset_is_complete':ready,'maps_published':False,'console_support_claimed':False}}


def generate(root, profile_path, selection_path, receipt_path, census_path, schema_path, out, require_complete=False):
 root=Path(root).resolve();paths=list(map(lambda p:Path(p).resolve(),(profile_path,selection_path,receipt_path,census_path,schema_path)))
 profile_path,selection_path,receipt_path,census_path,schema_path=paths
 selection=json.loads(selection_path.read_text());final=json.loads(receipt_path.read_text())
 if final.get('pass') is not True or final['source_selection_sha256']!=digest(selection_path) or final['profile_sha256']!=digest(profile_path): raise ValueError('Frozen profile/selection receipt mismatch')
 checked={}
 for relative,pin in selection['source_files'].items(): checked[str((selection_path.parents[1]/relative).resolve())]=pin
 for relative,pin in selection['evidence_files'].items(): checked[str((root/relative).resolve())]=pin
 for path,pin in checked.items():
  if digest(path)!=pin: raise ValueError('Frozen input drift: '+path)
 for path in (profile_path,schema_path):
  if str(path) not in checked or checked[str(path)]!=digest(path): raise ValueError('Input not covered by source freeze: '+str(path))
 if final['receipt_hashes'].get(census_path.name)!=digest(census_path): raise ValueError('Target width census receipt drift')
 census=json.loads(census_path.read_text())
 if census['exit']!=0 or census['missing_provider'] or census['narrower_than_measured']: raise ValueError('Target width census has unresolved providers')
 for path,pin in census['source_hashes'].items():
  if digest(path)!=pin: raise ValueError('Target width source drift: '+path)
 argv=census['argv'];object_path=Path(argv[argv.index('-o')+1]);
 if digest(object_path)!=census['object_sha256']: raise ValueError('Target width object drift')
 profile=json.loads(profile_path.read_text());result=build(profile,schema_path.read_text(),census['observed_widths'],require_complete)
 out=Path(out).resolve();owned=Path(__file__).resolve().parent
 if owned not in out.parents: raise ValueError('Output must stay inside isolated declaration slice')
 if out.exists(): raise ValueError('Preserving existing declaration output')
 out.mkdir(parents=True)
 files={}
 for directory,members in (('known-declarations',result['members']),('stageable-known-subset',result['stageable_members'])):
  target=out/directory;target.mkdir()
  for name,text in (('MemberVariableLayout.ini',members),('VTableLayout.ini',result['vtables'])):
   path=target/name;path.write_text(text);files[str(path.relative_to(out))]=digest(path)
 manifest=result['manifest'];manifest.update(input_pins={str(p):digest(p) for p in paths},profile_digest=final['profile_digest'],generated_files=files,
  source_freeze_files_checked=len(checked),image_identity=profile['image'],notes=['Known-declarations retain every measured fact; unknown totals make actual binder refuse.',
  'Stageable-known-subset omits whole classes without measured totals; required profile preflight still refuses.',
  'Local drafts only; no activation/configuration changes.'])
 (out/'UNRESOLVED.json').write_text(json.dumps(manifest,indent=2)+'\n')
 return manifest

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ('root','profile','selection','receipt','census','schema','out'): parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--require-complete',action='store_true');args=parser.parse_args()
 try:
  m=generate(args.root,args.profile,args.selection,args.receipt,args.census,args.schema,args.out,args.require_complete)
  print(json.dumps({'status':m['status'],'known_members':m['known_members'],'known_vtables':m['known_vtables'],'unresolved':m['unresolved']}))
 except (ValueError,KeyError,OSError) as error: parser.exit(2,'REFUSE: '+str(error)+'\n')
