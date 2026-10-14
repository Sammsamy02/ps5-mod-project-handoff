#!/usr/bin/env python3
"""Build the exact reviewed source-only follow-on; no build or network operation."""
from pathlib import Path
import hashlib
import json
import posixpath
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
SPIKE = '.superpowers/native-dependency-spike/'
PAD = SPIKE + 'ue4ss-native-pad-config-1/'
CPP = SPIKE + 'native-cppmod-contract-1/'
LINK = SPIKE + 'native-coherent-pad-runtime-1/'
FINAL = CPP + '3a3c14679b1b4e3593636f3e1975bb65/'
PARENT = '3087931fe2598d290288fd4b6474d108f898550a'

def digest(data): return hashlib.sha256(data).hexdigest()
def read(path): return json.loads((ROOT/path).read_text())
def save(name, data): (OUT/name).write_text(json.dumps(data, indent=2)+'\n')
def validate_path(path):
    assert path == posixpath.normpath(path) and not path.startswith('/')
    assert '..' not in Path(path).parts
    assert not any(x in path.lower() for x in ('nexus-test', '/dotnet-home/', '/obj/', '/native-observations/'))
    assert Path(path).suffix in {'.cpp','.hpp','.c','.h','.cs','.csproj','.props','.config','.py','.md','.json','.ini','.patch'} or Path(path).name in {'LICENSE','SharpProspero-LICENSE'}
    assert not (ROOT/path).is_symlink() and (ROOT/path).is_file()
    return path
def checked(path, expected=None):
    data=(ROOT/validate_path(path)).read_bytes()
    data.decode('utf-8')
    assert b'\x00' not in data, path
    if expected is not None: assert digest(data)==expected, ('changed reviewed input',path)
    return {'path':path,'bytes':len(data),'sha256':digest(data)}

base_paths=['outputs/2026-10-03-claude-runtime-full.manifest.json','outputs/2026-10-03-runtime-source-delta/MANIFEST.json']
base={}
for path in base_paths:
    base.update({posixpath.normpath(row['path']):row['sha256'] for row in read(path)['files']})

checked(CPP+'export-files.json','37413b76588a461e9f9aafb9ffbd3932277c9e9de069ea04ab2dc0ec8f6a995f')
checked(LINK+'source-backup-files.json','3430ed47078df078b31979fd4eec670ebc50ac0f398fe7a89528c395632e98ae')
checked(FINAL+'source-receipt.json','84959de090998e431fd03c65de19827be0be31791c89afd515ae74e2b12478e7')
checked(FINAL+'independent-review-alignment.json','89fead92b593c2944763dca92e493de610ddf139a30f2472cef356f31e3d49b0')
paths=set(read('outputs/2026-10-03-runtime-followon/pad-export-files.json')['files'])
paths.update(read(CPP+'export-files.json')['files'])
paths.update(row['path'] for row in read(LINK+'source-backup-files.json')['files'])
paths.update([LINK+'source-backup-files.json', FINAL+'independent-review-alignment.json',
              CPP+'043cd6e76ccd4b8cade0054215883dc2/independent-review/receipt.json',
              'outputs/2026-10-03-runtime-followon/build_archive.py',
              'outputs/2026-10-03-runtime-followon/pad-export-files.json'])
for row in read(FINAL+'source-receipt.json')['files']:
    assert row['path'] in paths
    assert checked(row['path'],row['sha256'])['bytes']==row['bytes']
for row in read(LINK+'source-backup-files.json')['files']:
    assert checked(row['path'],row['sha256'])['bytes']==row['bytes']
for row in read(PAD+'integration-manifest.json')['files']:
    assert PAD+row['path'] in paths
    assert checked(PAD+row['path'],row['sha256'])['bytes']==row['bytes']
alignment=read(FINAL+'independent-review-alignment.json')
checked(alignment['previous_independent_review'],alignment['previous_review_sha256'])
checked(alignment['final_run_receipt'],alignment['final_run_receipt_sha256'])
checked(FINAL+'verification/receipt.json',alignment['final_verification_receipt_sha256'])
for path,sha in alignment['final_source_hashes'].items(): checked(path,sha)
assert alignment['material_findings']==[]

allowed=[checked(path) for path in sorted(paths)]
files=[]
for row in allowed:
    row['baseline_sha256']=base.get(row['path'])
    row['provided_by']='prior-backups' if row['sha256']==row['baseline_sha256'] else 'followon-archive'
    if row['provided_by']=='followon-archive': files.append(row)
available=dict(base)
available.update({row['path']:row['sha256'] for row in allowed})
closure=read(PAD+'backup-dependency-closure.json')
assert not closure['missing_local_text_dependencies']
for path in closure['prior_source_dependencies']:
    path=posixpath.normpath(path)
    checked(path,base[path])
for row in closure['reconstructed_header_dependencies']:
    assert available.get(posixpath.normpath(row['origin']))==row['sha256'],row
snapshot=read(SPIKE+'native-coherent-runtime-1/snapshot-plan.json')
for row in snapshot['files']:
    assert available.get(posixpath.normpath(row['origin']))==row['sha256'],row['origin']
for row in read(LINK+'overlay-plan.json')['sources']:
    assert available.get(PAD+row['path'])==row['sha256']

save('ALLOWLIST.json',{'schema':1,'files':allowed,'scope':'Exact reviewed sources, build recipes, licenses, declared contract metadata and selected text receipts.'})
manifest={'schema':1,'parent_commit':PARENT,'base_manifests':[{'path':p,'sha256':digest((ROOT/p).read_bytes())} for p in base_paths],
          'files':files,'prior_backup_reused_files':[r for r in allowed if r['provided_by']=='prior-backups'],
          'scope':'Source-only follow-on. No compiled artifacts, provider capture, platform binary, game data or credentials.',
          'canonical_or_sealed_runtime_changed':False,'target_execution':False,'console_contact':False}
save('MANIFEST.json',manifest)
save('DEPENDENCY-CLOSURE.json',{
    'base_manifests':manifest['base_manifests'],'source_snapshot_origins_verified':len(snapshot['files']),
    'pad_prior_direct_source_dependencies_verified':len(closure['prior_source_dependencies']),
    'pad_reconstructed_headers_verified':len(closure['reconstructed_header_dependencies']),
    'missing_local_text_dependencies':[], 'external_sdk_headers_not_exported':len(closure['external_sdk_toolchain_headers']),
    'cpp_typed_packager_project_source_files':37,
    'cpp_dependency_allowlist':CPP+'export-files.json',
    'external_prerequisites':['Existing PS5 payload SDK v0.43 with pinned type-evidence libraries',
                              'Existing LLVM 21.1.8 macOS ARM64 toolchain',
                              'Existing .NET 10.0.401 macOS ARM64 SDK'],
    'rebuild_from_source':'Restore original full source backup, then sealed 303-file delta, then this follow-on. Rebuild the base runtime with its REBUILD.md before consuming its new per-unit outputs.'})
save('CHECKPOINT.json',{
    'pad_host_control_checks':64,'pad_host_control_backend':'Actual PS5PadSource and Handler with explicitly owned test backend',
    'pad_successor_runtime_sha256':'5d737dafac1b2ca908c1b2632a114a443549d31def8d82b733a935a945571cf2',
    'pad_successor_units':{'runtime':359,'bootstrap':3,'fresh_runtime':2,'fresh_bootstrap':1,'retained_runtime':357,'retained_bootstrap':2,'retained_actual_activation_c':True},
    'pad_successor_undefined_symbols':199,'undefined_symbol_delta':[],
    'pad_successor_form':'Relocatable object only; excluded from archive',
    'cpp_contract_run':FINAL.rstrip('/'),'cpp_review_alignment':FINAL+'independent-review-alignment.json',
    'cpp_proof':'Local genuine host export/plugin import packaging and unchanged fake-SELF loadable segments; no module loading or lifecycle execution',
    'runtime_entry_or_game_execution_proven':False,'hardware_input_proven':False,'console_contact':False,
    'separate_limitations':['Production startup/context producer remains required','Native binary activation remains unavailable in retained source mode','Existing weak imports remain unresolved']})
zip_path=OUT/'runtime-followon-source.zip'
with zipfile.ZipFile(zip_path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for path,data in [(r['path'],(ROOT/r['path']).read_bytes()) for r in files]+[('RUNTIME-FOLLOWON-MANIFEST.json',(OUT/'MANIFEST.json').read_bytes())]:
        info=zipfile.ZipInfo(path,(2026,10,3,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
        archive.writestr(info,data)
with zipfile.ZipFile(zip_path) as archive:
    assert archive.testzip() is None
    assert set(archive.namelist())=={r['path'] for r in files}|{'RUNTIME-FOLLOWON-MANIFEST.json'}
    for row in files: assert digest(archive.read(row['path']))==row['sha256']
data=zip_path.read_bytes()
summary={'source_files':len(files),'explicit_allowlist_files':len(allowed),'reused_from_prior_backup_files':len(allowed)-len(files),
         'source_bytes':sum(r['bytes'] for r in files),'zip_bytes':len(data),'zip_sha256':digest(data),
         'zip_git_blob_sha1':hashlib.sha1(f'blob {len(data)}\0'.encode()+data).hexdigest(),
         'zip_crc_and_all_file_sha256_verified':True,'source_snapshot_origins_verified':len(snapshot['files'])}
save('ARCHIVE.json',summary)
print(json.dumps(summary,indent=2))
