#!/usr/bin/env python3
"""Verify completed local l22 outputs and assemble a cloud review packet."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,zipfile,difflib
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
BUILD=ROOT/'outputs/2026-10-08-l22-build-r3';PKG=ROOT/'outputs/2026-10-08-l22-package-r2';OLD=ROOT/'outputs/2026-10-08-menu-complete-package'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def read(p):return json.loads(Path(p).read_text())
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def main():
    twin=read(BUILD/'TWIN-RECEIPT.json');assert twin['status']=='MAC_ONLY_IDENTICAL_L22_RUNTIME_PAIR'
    prxs=[PKG/f'pass-{i}/modbridge_supplier_composition.prx' for i in [1,2]]
    assert prxs[0].read_bytes()==prxs[1].read_bytes()
    assert prxs[0].read_bytes()[:4].hex()=='4f153d1d'
    exact=[];checks=[];arrays=[]
    for i in [1,2]:
        p=PKG/f'pass-{i}';r=read(p/'BUILD-RECEIPT.json');assert r['status']=='MAC_PACKAGE_CHECKS_PASS' and not r['gaps']
        identities=read(p/'EXACT-IMPORT-IDENTITIES.json');assert identities['count']==207
        static=read(p/'STATIC-CHECKS.json');assert static['status']=='STATIC_CHECKS_PASSED' and all(row['pass'] for row in static['checks'].values())
        assert len(static['checks'])==20
        assert static['checks']['sdk_stamp_0x11000000']['detail']=='0x11000000'
        array=read(p/'CONSTRUCTOR-ARRAY-CENSUS.json')
        assert array['status']=='PASS_MAC_ONLY_FULL_EXTENDED_AND_LEGACY_CENSUS'
        for key in ('.init_array','.fini_array'):
            assert array['combined'][key]['bytes']==array['packaged'][key]['bytes']
            assert array['combined'][key]['slots']==array['packaged'][key]['slots']
        arrays.append({key:array['packaged'][key]['slots'] for key in ('.init_array','.fini_array')})
        exact.append(identities);checks.append(static)
    # Import identity comparison must be structural, independent of paths in receipts.
    old=read(OLD/'pass-1/EXACT-IMPORT-IDENTITIES.json')
    def identities_only(d):
        # File offsets move when code grows; provider identity and versions must not.
        return [{'name':r['name'],'published':r['published'],'type':r['type'],
                 'module':{k:v for k,v in r['module'].items() if k!='value_offset'},
                 'library':{k:v for k,v in r['library'].items() if k!='value_offset'}} for r in d['imports']]
    # The exact-checker report consists of count and normalized identity rows.
    assert identities_only(exact[0])==identities_only(exact[1])==identities_only(old),'import identity delta requires explicit declaration'
    changes=read(BUILD/'CHANGE-SELECTION.json')['files']
    for r in changes:
        assert sha(r['original'])==r['original_sha256'];assert sha(r['candidate'])==r['candidate_sha256']
    for part in ['profile','abi','diagnostics']:
        d=read(HERE/part/'CHANGES.json')
        for r in d['files']:
            assert sha(r['original'])==r['original_sha256'];assert sha(r['candidate'])==r['candidate_sha256']
            assert r in changes
    lua=ROOT/'outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/main.lua'
    assert sha(lua)=='f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658'
    result={'status':'MAC_ONLY_L22_TWINS_AND_PACKAGE_CHECKS_PASS','prx':pin(prxs[0]),'twin':pin(prxs[1]),'magic':'4f153d1d','SDK':'0x11000000','runtime_twins':pin(BUILD/'TWIN-RECEIPT.json'),'recompiled_each_pass':167,'retained_units':207,'all_selected_units':374,'link_inputs':391,'exact_import_count':207,'imports_unchanged':True,'mod':pin(lua),'console_contact':False,'native_execution':False,'menu_or_spawn_proven':False,'upload_approval_pending':True}
    assert arrays[0]==arrays[1]
    result.update(static_checks_each_pass=len(checks[0]['checks']),constructor_arrays=arrays[0],original_and_candidate_files_verified=len(changes))
    write(PKG/'FINAL-RECEIPT.json',result)
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ');packet=PKG/('claude-review-'+stamp);packet.mkdir(exist_ok=False)
    origins=[]
    def copy(p,rel):
        p=Path(p);data=p.read_bytes();dest=packet/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);origins.append({'original':str(p),'packet':str(rel),'sha256':hashlib.sha256(data).hexdigest()})
    copy(prxs[0],Path('artifact')/prxs[0].name)
    for name in ['BUILD-REPORT.md','PREFLIGHT-TABLE.md','FINAL-RECEIPT.json','BUILD-INPUTS.json','PREPARATION-RECEIPT.json','FINAL-DIFF-REVIEW.md']:
        copy(PKG/name,Path(name))
    for name in ['REVIEW-SCOPE.md','build_runtime.py','prepare_package.py','finalize_review.py','PROGRESS.md']:
        copy(HERE/name,Path('implementation')/name)
    for name in ['package_combined.py','package_prx_l19.py','prep_guard.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py','checker-tests.log','inputs/NuGet.Config','STAGED-SOURCE-PINS.json','pass1-controller.log','pass2-controller.log']:
        copy(PKG/name,Path('packaging')/name)
    suffixes={'.md','.json','.py','.hpp','.h','.cpp','.c','.diff','.tsv','.log','.txt','.stdout','.stderr','.ll'}
    for section in ['profile','abi','diagnostics','tests','candidate']:
        for p in sorted((HERE/section).rglob('*')):
            if p.is_file() and p.suffix in suffixes and '__pycache__' not in p.parts:
                copy(p,Path('implementation')/p.relative_to(HERE))
    for r in changes:
        p=Path(r['original']);copy(p,Path('original-sources')/(r['original_sha256'][:12]+'-'+p.name))
        difference=''.join(difflib.unified_diff(p.read_text().splitlines(keepends=True),Path(r['candidate']).read_text().splitlines(keepends=True),fromfile=r['original'],tofile=r['candidate']))
        dpath=packet/'diffs'/(r['original_sha256'][:12]+'-'+p.name+'.diff');dpath.parent.mkdir(exist_ok=True);dpath.write_text(difference)
    for i in [1,2]:
        for name in ['BUILD-RECEIPT.json','STATIC-CHECKS.json','EXACT-IMPORT-IDENTITIES.json','CONSTRUCTOR-ARRAY-CENSUS.json','supplier-contract.json']:
            copy(PKG/f'pass-{i}'/name,Path(f'package-pass-{i}')/name)
        for name in ['COMPOSITION-RECEIPT.json','COMPILE-RECEIPT.json','IMPORT-DELTA.json','DEPENDENCY-GRAPH.json','undefined.stdout']:
            copy(BUILD/f'pass{i}'/name,Path(f'runtime-pass-{i}')/name)
    for name in ['TWIN-RECEIPT.json','SOURCE-SELECTION.json','CHANGE-SELECTION.json','headers.vfs.json']:
        copy(BUILD/name,Path('runtime')/name)
    copy(lua,Path('unchanged-mod/main.lua'))
    parent=ROOT/'outputs/2026-10-08-l21-audit'
    for folder in ['allocator/postfetch','allocator/official-headers','allocator/official-additional']:
        for p in sorted((parent/folder).rglob('*')):
            if p.is_file() and p.suffix in suffixes:copy(p,Path('l21-evidence')/p.relative_to(parent))
    for name in ['allocator/REPORT-SOURCE-CORROBORATION.md','mod-api/OFFICIAL-SCO-COMPARISON.md','mod-api/OFFICIAL-SCO-COMPARISON.json','mod-api/SCO-IMAGE-VALIDATION.json','mod-api/MENU-STARTUP-MISSING-JOIN.json']:
        copy(parent/name,Path('l21-evidence')/name)
    write(packet/'ORIGIN-INDEX.json',origins)
    (packet/'README.md').write_text('Start with BUILD-REPORT.md, then implementation/REVIEW-SCOPE.md. All results here are Mac-only unless explicitly labeled prior-console evidence. No new console run occurred. The PRX is awaiting upload review. ORIGIN-INDEX.json maps original Mac references to included files. Original downloaded Lua mod is unchanged. This private packet contains licensed source context; keep it within authorized review.\n')
    files=[{'path':str(p.relative_to(packet)),**{k:v for k,v in pin(p).items() if k!='path'}} for p in sorted(packet.rglob('*')) if p.is_file()]
    write(packet/'MANIFEST.json',{'files':files})
    zpath=packet.with_suffix('.zip')
    with zipfile.ZipFile(zpath,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(packet.rglob('*')):
            if p.is_file():z.write(p,str(p.relative_to(packet)))
    with zipfile.ZipFile(zpath) as z:
        assert z.testzip() is None
        for r in files:
            b=z.read(r['path']);assert len(b)==r['bytes'] and hashlib.sha256(b).hexdigest()==r['sha256']
    receipt={'zip':pin(zpath),'manifest_files_verified':len(files),'console_contact':False,'upload_approval_pending':True}
    write(PKG/'PACKET-RECEIPT.json',receipt);print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
