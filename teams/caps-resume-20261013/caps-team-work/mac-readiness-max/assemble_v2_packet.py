"""Assemble pinned current component work plus explicitly historical provenance.
No compilation, driver execution, integration mutation or console contact.
"""
from pathlib import Path
import hashlib,json,zipfile
ROOT=Path(__file__).resolve().parents[2]
OUT=Path('/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs')
PREFIX='GPT-7-10-MAC-READY-V2'
DAY='2026-10-13'
selected={};verified=[]
def sha(raw):return hashlib.sha256(raw).hexdigest()
def add(path,name=None):
 path=Path(path);assert path.is_file() and not path.is_symlink(),path
 name=name or str(path.relative_to(ROOT))
 assert not name.startswith('/') and '..' not in Path(name).parts
 assert name not in selected or selected[name]==path,name
 selected[name]=path

def read(value):return value if isinstance(value,bytes) else value.read_bytes()
def from_manifest(component,manifest,include=lambda p:True):
 base=ROOT/'caps-team-work'/component;m=base/manifest;j=json.loads(m.read_text());count=0
 for row in j['files']:
  p=Path(row.get('source',row['path']));p=p if p.is_absolute() else base/p
  assert p.is_relative_to(base),p
  raw=p.read_bytes();assert len(raw)==row['bytes'] and sha(raw)==row['sha256'],p
  if include(p):add(p);count+=1
 add(m);verified.append({'component':component,'manifest':str(m.relative_to(ROOT)),
  'sha256':sha(m.read_bytes()),'files_checked':len(j['files']),'files_included':count})

old_manifest=OUT/f'GPT-7-10-MAC-FINAL-MANIFEST-{DAY}.json'
old_archive=OUT/f'GPT-7-10-MAC-FINAL-PACKET-{DAY}.zip'
old=json.loads(old_manifest.read_text());historical_count=0
with zipfile.ZipFile(old_archive) as packet:
 for row in old['files']:
  name=row['path'];raw=packet.read(name)
  assert len(raw)==row['bytes'] and sha(raw)==row['sha256'],name
  # Preserve small original source, recipes, licenses and evidence. Historical
  # target archives, corlib/reference DLLs and redundant stages stay local.
  if (name.startswith('caps-team-work/') and '/derived/review-final/' not in name
      and '/derived/current/' not in name and Path(name).suffix not in ('.o','.a','.dll','.dylib','.elf','.zip')
      and row['bytes']<2_000_000):
   selected[name]=raw;historical_count+=1
selected['historical/'+old_manifest.name]=old_manifest.read_bytes()
selected['historical/GPT-7-10-MAC-FINAL-HANDOFF-'+DAY+'.md']=(OUT/f'GPT-7-10-MAC-FINAL-HANDOFF-{DAY}.md').read_bytes()
verified.append({'component':'historical-original-packet','manifest':str(old_manifest),
 'sha256':sha(old_manifest.read_bytes()),'files_checked':len(old['files']),'files_included':historical_count,
 'status':'historical provenance, not authoritative current integration inputs'})

from_manifest('cap07-mac-ready','FROZEN-READY.json',lambda p:p.suffix!='.o' and '/derived/review-final/' not in str(p))
from_manifest('cap10-native-ready','HANDOFF-MANIFEST.json',lambda p:p.suffix not in ('.o','.elf','.map') and (p.suffix!='.a' or '/handoff/' in str(p)))
from_manifest('cap10-signal-ready','HANDOFF-MANIFEST.json',lambda p:p.suffix not in ('.o','.a','.dylib'))
from_manifest('cap10-integration-ready','HANDOFF-MANIFEST.json')
from_manifest('cap10-exec-ready','HANDOFF-MANIFEST.json',lambda p:'/bin/' not in str(p) and p.suffix not in ('.o','.dylib'))
from_manifest('mac-readiness-max/shared-final','HANDOFF-FILES.json')
for p in (ROOT/'caps-team-work/mac-readiness-max').glob('*'):
 if p.is_file():add(p)
for n in ('CHATGPT-HANDOFF-7-10.md','caps-team-work/PROGRESS.md','caps-team-work/cap13-dll/ZCODE-ASSIGNMENT.md'):add(ROOT/n)
handoff=OUT/f'{PREFIX}-HANDOFF-{DAY}.md';add(handoff,handoff.name)
manifest={'scope':'Current Mac-ready #7 step2/#10 component sources, authoritative combined stage and binaries, dependency preparation, tests and evidence; no combined game/PRX or target acceptance',
 'authoritative_stage':'caps-team-work/cap10-integration-ready/derived/target-ready/',
 'component_manifests_verified':verified,'files':[],
 'excluded':['Host/runtime/compiler binaries and upstream source/build trees retained locally with pinned recipes',
 'Large historical link-only ELF and duplicate target archives/corlib DLLs',
 'Individual target compilation objects: receipts/dependencies retained, Claude performs full rebuild',
 'SDK/LLVM installations, saved platform provider binaries, game files and proprietary SDK material'],
 'historical_qualification':'Earlier manifests preserve their original file hashes/paths. Files excluded from this packet remain historical local inputs; current large target binaries appear only in the authoritative stage.'}
for name,value in sorted(selected.items()):
 raw=read(value);manifest['files'].append({'path':name,'bytes':len(raw),'sha256':sha(raw)})
mp=OUT/f'{PREFIX}-MANIFEST-{DAY}.json'
with mp.open('x') as f:json.dump(manifest,f,indent=2);f.write('\n')
archive=OUT/f'{PREFIX}-PACKET-{DAY}.zip'
with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as packet:
 for name,value in sorted(selected.items()):
  if isinstance(value,bytes):packet.writestr(name,value)
  else:packet.write(value,name)
 packet.write(mp,mp.name)
with zipfile.ZipFile(archive) as packet:
 assert packet.testzip() is None
 assert len(packet.namelist())==len(set(packet.namelist()))==len(selected)+1
 for row in manifest['files']:
  raw=packet.read(row['path']);assert len(raw)==row['bytes'] and sha(raw)==row['sha256'],row['path']
 assert packet.read(mp.name)==mp.read_bytes()
receipt={'archive':archive.name,'bytes':archive.stat().st_size,'sha256':sha(archive.read_bytes()),
 'members':len(selected)+1,'crc_verified':True,'all_member_hashes_verified':True,
 'manifest_sha256':sha(mp.read_bytes()),'handoff_sha256':sha(handoff.read_bytes()),
 'uncompressed_file_bytes':sum(x['bytes'] for x in manifest['files']),
 'target_execution':False,'combined_game_prx_build_or_package':False,'main_tree_modified':False}
rp=OUT/f'{PREFIX}-ARCHIVE-RECEIPT-{DAY}.json'
with rp.open('x') as f:json.dump(receipt,f,indent=2);f.write('\n')
print(json.dumps(receipt,indent=2))
