"""Bounded exact-source transform, hash/anchor/inverse validation; no target build.

JA review fix (2026-10-13): the recipe now also transforms
profile/CHANGES.json so the two refreshed profile candidate hashes
(CodeImageCache.hpp kMaxPins 8->16, CodeSignatureProfile.hpp 12 pins) stay
consistent with the pinned candidates that build_variants G2 and
l26_packaging_driver assert. The stager additionally audits every consumer of
the shared CodeImageCache Metadata layout and records the census in STAGE.json.
"""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import hashlib,json,os,argparse
HOME=Path(__file__).resolve().parent.parent
CAP=4*1024*1024
def digest(b):return hashlib.sha256(b).hexdigest()
def read(path):
 p=Path(path)
 with p.open('rb') as f:
  a=os.fstat(f.fileno());b=f.read(CAP+1);z=os.fstat(f.fileno())
 if len(b)>CAP or (a.st_dev,a.st_ino,a.st_size,a.st_mtime_ns)!=(z.st_dev,z.st_ino,z.st_size,z.st_mtime_ns) or p.stat().st_ino!=a.st_ino:raise ValueError('unstable/oversize source')
 return b
def transform(source,recipe):
 if not source or len(source)>CAP or len(source)!=recipe['bytes'] or digest(source)!=recipe['sha256']:raise ValueError('exact source SHA/size required')
 lines=source.decode().splitlines(True);out=[];cursor=0;inverse=[];offset=0
 if not recipe['hunks']:raise ValueError('empty operation list')
 for h in recipe['hunks']:
  old=h['old'].splitlines(True);new=h['new'].splitlines(True);start=h['start']
  if start<cursor or lines[start:start+len(old)]!=old:raise ValueError('anchor mismatch')
  out.extend(lines[cursor:start]);out.extend(new);inverse.append((start+offset,new,old));offset+=len(new)-len(old);cursor=start+len(old)
 out.extend(lines[cursor:]);result=''.join(out).encode()
 if len(result)>CAP or digest(result)!=recipe['derived_sha256']:raise ValueError('derived hash mismatch')
 back=result.decode().splitlines(True)
 for start,new,old in reversed(inverse):
  if back[start:start+len(new)]!=new:raise ValueError('inverse anchor mismatch')
  back[start:start+len(new)]=old
 if ''.join(back).encode()!=source:raise ValueError('inverse mismatch')
 return result
def audit_code_image_cache(root,derived_changes):
 """Consumer census for the shared kMaxPins Metadata layout.

 Every site that hands the compiled layout to a TU or pins its bytes:
  1. profile/units/CodeImageCache.hpp  -- the promoted candidate; this stage
     writes its kMaxPins=16 derived bytes (recipe row 4).
  2. profile/CHANGES.json             -- pins that candidate's sha for
     build_variants G2 (runtime) and l26_packaging_driver (lineage overlay);
     this stage refreshes both candidate hashes (recipe row 5).
  3. 2026-10-07-l19-local-profile-r4/units/CodeImageCache.hpp -- immutable
     ORIGINAL revision shadowed by (1) through include order + the vfs alias;
     must still match the original_sha256 pinned in CHANGES.json or the alias
     routing silently breaks. Verified here, never edited.
  4. lineage/CodeImageCache.hpp in package trees -- regenerated from (1) by
     l26_packaging_driver at the next package build; historical copies hold
     kMaxPins=8 and must not be reused for a kMaxPins=16 runtime.
 """
 changes=json.loads(derived_changes)
 row=next(r for r in changes['files'] if r['candidate'].endswith('/CodeImageCache.hpp'))
 alias=root/Path(row['original']).relative_to(root)
 b=read(alias)
 if digest(b)!=row['original_sha256']:raise ValueError('CodeImageCache alias original drifted: '+row['original'])
 return {
  'candidate':row['candidate'],'candidate_expected_sha256':row['candidate_sha256'],
  'alias_original':row['original'],'alias_original_sha256':row['original_sha256'],
  'lineage_destination':'lineage/CodeImageCache.hpp (regenerated from candidate by l26_packaging_driver)',
  'note':'all compiled copies must resolve to the staged candidate bytes; historical package lineage copies are stale',
 }
def main():
 p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 out=a.out.resolve();allowed=(HOME/'derived').resolve()
 if out.parent!=allowed or out.exists():raise ValueError('new output directory under owned derived/ required')
 recipe=json.loads(read(HOME/'evidence/stage-recipe.json'));staged=[];sources=[]
 for row in recipe['files']:
  path=a.root/row['relative_path'];source=read(path);staged.append((row['relative_path'],transform(source,row)));sources.append((path,source))
 if len(staged)!=5:raise ValueError('expected five exact current source files (incl. profile/CHANGES.json)')
 additions={'RuntimePakMount.cpp':'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/RuntimePakMount.cpp'}
 for name in ['RuntimePakMount.hpp','PakRequestProducer.hpp','NativePakMountAdapter.inl','NativePakMountEntry.hpp','pak_mount_pins.inc']:additions[name]='outputs/2026-10-08-l26-runtime/typescan-r4/include/'+name
 additions['PakMountCodeProfile.hpp']='outputs/2026-10-08-l26-runtime/profile/units/PakMountCodeProfile.hpp'
 for name,rel in additions.items():staged.append((rel,read(HOME/'PATCH'/name)))
 for path,b in sources:
  if read(path)!=b:raise ValueError('input drift before output')
 derived_changes=next(b for rel,b in staged if rel.endswith('profile/CHANGES.json'))
 census=audit_code_image_cache(a.root,derived_changes)
 allowed.mkdir(exist_ok=True);out.mkdir()
 for rel,b in staged:
  path=out/rel;path.parent.mkdir(parents=True,exist_ok=True)
  with path.open('xb') as f:f.write(b)
 (out/'STAGE.json').write_text(json.dumps({'label':'Unapplied derived source only','files':[{'path':rel,'bytes':len(b),'sha256':digest(b)} for rel,b in staged],'code_image_cache_consumers':census,'originals_unchanged':True,'target_build':False},indent=2)+'\n')
 print('PASS:',len(sources),'exact sources/anchors/inverses;',len(staged),'derived files; CodeImageCache consumer audit OK; no originals edited')
if __name__=='__main__':main()
