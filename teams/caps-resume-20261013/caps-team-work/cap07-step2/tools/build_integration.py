"""Unapplied exact-source build/packaging integration; no compiler or driver main."""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import json,hashlib,ast,importlib.util,argparse,copy,difflib
H=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('cap7_stage',H/'tools/stage_current.py');stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
def sha(b):return hashlib.sha256(b).hexdigest()
def once(s,a,b):
 if s.count(a)!=1:raise ValueError('nonunique integration anchor: '+a[:80])
 return s.replace(a,b,1)
def runtime_driver(s,source_sha):
 s=once(s,'NEW_UNITS={\n',"NEW_UNITS={\n    'pakmount':{'source':HERE/'typescan-r4/candidate/src/RuntimePakMount.cpp','template':'unreal-unrealinitializer','note':'cap07 synchronized native pak mount (always fresh)','source_sha256':'"+source_sha+"'},\n")
 # Inserting after the docstring is required for the actual driver; AST locates its end.
 tree=ast.parse(s);fn=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='compute_selection');lines=s.splitlines(True)
 pos=fn.body[0].end_lineno if isinstance(fn.body[0],ast.Expr) and isinstance(fn.body[0].value,ast.Constant) and isinstance(fn.body[0].value.value,str) else fn.lineno
 lines[pos:pos]=["    if incremental:raise RuntimeError('cap07 CodeImageCache ABI change requires full header-consumer rebuild; incremental prohibited')\n"]
 s=''.join(lines)
 anchor="        tpl=by_name[spec['template']]"
 if anchor in s:s=once(s,anchor,"        if spec.get('source_sha256') and sha(spec['source'])!=spec['source_sha256']:raise RuntimeError('registered NEW_UNIT source pin drift: '+name)\n"+anchor)
 return s
def overlay_helper():
 return '''def pakmount_profile_overlay(changes):
    path=L26RUN/'profile/units/PakMountCodeProfile.hpp'
    expected=(changes.get('added_header_hashes') or {}).get(str(path))
    if not expected or str(path) not in changes.get('added_headers',[]):raise RuntimeError('PakMountCodeProfile missing required profile header admission')
    if not path.is_file() or sha(path)!=expected:raise RuntimeError('PakMountCodeProfile header pin drift')
    return {'path':str(path),'sha256':expected,'bytes':path.stat().st_size,'destination':'lifecycle/PakMountCodeProfile.hpp'}

'''
def packaging_driver(s):
 s=once(s,'def derive_manifest(variant,record,steps,execute):',overlay_helper()+'def derive_manifest(variant,record,steps,execute):')
 return once(s,"    m['lineage_overlays']=overlays", "    required_pak_profile=pakmount_profile_overlay(prof_changes)\n    if any(o['destination']==required_pak_profile['destination'] for o in overlays):raise RuntimeError('duplicate PakMountCodeProfile overlay')\n    overlays.append(required_pak_profile)\n    m['lineage_overlays']=overlays")
def refresh_rows(d,candidates):
 d=copy.deepcopy(d);count=0
 for row in d['files']:
  if row['candidate'] in candidates:row['candidate_sha256']=sha(candidates[row['candidate']]);count+=1
 if not candidates or count!=len(candidates):raise ValueError('source row census mismatch')
 return d
def add_headers(d,headers):
 d=copy.deepcopy(d);added=d.setdefault('added_headers',[]);pins=d.setdefault('added_header_hashes',{})
 for path,b in headers.items():
  if path not in added:added.append(path)
  pins[path]=sha(b)
 return d
def integration_patch(derived,originals):
 # difflib leaves unterminated input lines unterminated. Git needs its explicit
 # marker, otherwise old/new EOF lines join and corrupt the patch. Include new
 # files as well so this is the complete staged change, not only its edits.
 chunks=[]
 for rel,new in derived.items():
  old=originals.get(rel,b'')
  lines=difflib.unified_diff(old.decode().splitlines(True),new.decode().splitlines(True),
                           'a/'+rel if rel in originals else '/dev/null','b/'+rel)
  chunks.extend(line if line.endswith('\n') else line+'\n\\ No newline at end of file\n' for line in lines)
 return ''.join(chunks)
def generate(root):
 root=Path(root).resolve();recipe=json.loads(stage.read(H/'evidence/stage-recipe.json'));derived={};originals={}
 driver_pins=json.loads(stage.read(H/'evidence/build-integration-inputs.json'))['files']
 if len(driver_pins)!=3:raise ValueError('driver pin census')
 for row in driver_pins:
  b=stage.read(root/row['relative_path'])
  if len(b)!=row['bytes'] or sha(b)!=row['sha256']:raise ValueError('exact build-driver/metadata input drift: '+row['relative_path'])
 for row in recipe['files']:
  p=root/row['relative_path'];b=stage.read(p);originals[row['relative_path']]=b;derived[row['relative_path']]=stage.transform(b,row)
 prefix='outputs/2026-10-08-l26-runtime/';additions={'RuntimePakMount.cpp':prefix+'typescan-r4/candidate/src/RuntimePakMount.cpp','PakMountCodeProfile.hpp':prefix+'profile/units/PakMountCodeProfile.hpp'}
 for name in ['RuntimePakMount.hpp','PakRequestProducer.hpp','NativePakMountAdapter.inl','NativePakMountEntry.hpp','pak_mount_pins.inc']:additions[name]=prefix+'typescan-r4/include/'+name
 for name,rel in additions.items():derived[rel]=stage.read(H/'PATCH'/name)
 for name,fn in [('build_variants.py',lambda s:runtime_driver(s,sha(derived[additions['RuntimePakMount.cpp']]))),('l26_packaging_driver.py',packaging_driver)]:
  rel=prefix+name;b=stage.read(root/rel);originals[rel]=b;derived[rel]=fn(b.decode()).encode();ast.parse(derived[rel])
 rel=prefix+'typescan-r4/CHANGES.json';b=stage.read(root/rel);originals[rel]=b
 src={str(root/p):v for p,v in derived.items() if p.endswith(('candidate/src/UnrealInitializer.cpp','candidate/src/LuaMod.cpp'))}
 types=refresh_rows(json.loads(b),src);types=add_headers(types,{str(root/p):derived[p] for name,p in additions.items() if name not in ('RuntimePakMount.cpp','PakMountCodeProfile.hpp')});derived[rel]=(json.dumps(types,indent=2)+'\n').encode()
 rel=prefix+'profile/CHANGES.json';prof=add_headers(json.loads(derived[rel]),{str(root/additions['PakMountCodeProfile.hpp']):derived[additions['PakMountCodeProfile.hpp']]});derived[rel]=(json.dumps(prof,indent=2)+'\n').encode()
 census=stage.audit_code_image_cache(root,derived[rel])
 # Keep all original metadata/source bytes unchanged through preparation.
 for rel,b in originals.items():
  if stage.read(root/rel)!=b:raise ValueError('source drift during integration')
 return derived,originals,census
def main():
 p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
 if out.parent!=(H/'derived').resolve() or out.exists():raise ValueError('new owned derived output required')
 derived,originals,census=generate(a.root);out.mkdir()
 for rel,b in derived.items():
  p=out/rel;p.parent.mkdir(parents=True,exist_ok=True)
  with p.open('xb') as f:f.write(b)
 patch=integration_patch(derived,originals)
 (out/'integration.patch').write_text(patch)
 receipt={'label':'Complete source/build/packaging stage only','originals':[{'path':r,'bytes':len(b),'sha256':sha(b)} for r,b in originals.items()],'files':[{'path':r,'bytes':len(b),'sha256':sha(b)} for r,b in derived.items()],'cache_consumers':census,'new_units':5,'expected_link_inputs':396,'target_build':False,'execute_driver_main':False,'policy':'One combined next-launch full build; incremental prohibited'}
 (out/'INTEGRATION.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS:',len(originals),'exact originals;',len(derived),'staged files; NEW_UNITS5/link census396; no target build')
if __name__=='__main__':main()
