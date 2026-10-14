#!/usr/bin/env python3
"""Verify l23 twin packages and build a local review packet without Epic source."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,zipfile,difflib
from prepare_package import HERE,RUNTIME,ROOT,BASE,BUILD,OUT,require,sha,pin,read,write,verify_profile,check_pin
PKG=OUT
FORBIDDEN_PARTS={'official-source','official-headers','official-additional','2026-10-08-l23-layout-generator'}

def normalized_identities(data):
    require(data['count']==len(data['imports'])==207,'expected final207 imports')
    rows=[{'name':r['name'],'published':r['published'],'type':r['type'],
           'module':{k:v for k,v in r['module'].items() if k!='value_offset'},
           'library':{k:v for k,v in r['library'].items() if k!='value_offset'}} for r in data['imports']]
    return sorted(rows,key=lambda row:json.dumps(row,sort_keys=True,separators=(',',':')))

def verify_packages(package_dir=PKG,baseline_dir=BASE):
    baseline=read(baseline_dir/'pass-1/EXACT-IMPORT-IDENTITIES.json')
    admitted=normalized_identities(baseline)
    expected_checks=set(read(baseline_dir/'pass-1/STATIC-CHECKS.json')['checks'])
    require(len(expected_checks)==20,'baseline gate set must be exact20')
    records=[];arrays=[];artifacts=[]
    input_hash=sha(package_dir/'BUILD-INPUTS.json')
    for i in (1,2):
        p=package_dir/f'pass-{i}';r=read(p/'BUILD-RECEIPT.json')
        require(r['status']=='MAC_PACKAGE_CHECKS_PASS' and not r['gaps'],'package pass failed')
        require(r['inputs_sha256']==input_hash,'package receipt input manifest mismatch')
        exact=read(p/'EXACT-IMPORT-IDENTITIES.json');require(normalized_identities(exact)==admitted,'import identity delta requires explicit admission')
        static=read(p/'STATIC-CHECKS.json')
        require(static['status']=='STATIC_CHECKS_PASSED' and set(static['checks'])==expected_checks and all(v['pass'] is True for v in static['checks'].values()),'exact20 static gates must pass')
        require(static['checks']['sdk_stamp_0x11000000']['detail']=='0x11000000','SDK stamp drift')
        a=read(p/'CONSTRUCTOR-ARRAY-CENSUS.json');require(a['status']=='PASS_MAC_ONLY_FULL_EXTENDED_AND_LEGACY_CENSUS','array census did not pass')
        for name in ('.init_array','.fini_array'):
            require(a['combined'][name]['bytes']==a['packaged'][name]['bytes'] and a['combined'][name]['slots']==a['packaged'][name]['slots'],'constructor array loss')
        arrays.append({k:a['packaged'][k]['slots'] for k in ('.init_array','.fini_array')})
        prx=p/'modbridge_supplier_composition.prx';blob=prx.read_bytes();require(blob[:4].hex()=='4f153d1d','SELF magic mismatch')
        artifacts.append(pin(prx));records.append({'package':pin(p/'BUILD-RECEIPT.json'),'imports':pin(p/'EXACT-IMPORT-IDENTITIES.json'),'static':pin(p/'STATIC-CHECKS.json'),'arrays':pin(p/'CONSTRUCTOR-ARRAY-CENSUS.json')})
    require(artifacts[0]['sha256']==artifacts[1]['sha256'] and artifacts[0]['bytes']==artifacts[1]['bytes'],'PRX twins differ')
    require(arrays[0]==arrays[1],'twin array count drift')
    return {'prx':artifacts[0],'twin':artifacts[1],'checks':records,'static_checks_each_pass':20,'constructor_arrays':arrays[0],'exact_import_count':207,'imports_unchanged':True,'import_baseline':pin(baseline_dir/'pass-1/EXACT-IMPORT-IDENTITIES.json')}

def safe_packet_source(path):
    p=Path(path).resolve()
    require(not FORBIDDEN_PARTS.intersection(p.parts),'Epic source/compiler tree excluded from packet: '+str(p))
    return p

def packet_data(path):
    """Keep licensed declaration excerpts local; never alter the build input."""
    path=Path(path);data=path.read_bytes();omitted=[]
    if path.suffix!='.json':return data,omitted
    document=json.loads(data)
    def visit(value,trail=()):
        if isinstance(value,list):return [visit(v,trail+(str(i),)) for i,v in enumerate(value)]
        if not isinstance(value,dict):return value
        result={}
        for key,item in value.items():
            if trail and trail[-1]=='source_declaration' and key=='text':
                digest=hashlib.sha256(item.encode()).hexdigest()
                result['text_omitted']='Licensed source excerpt stays local; see original artifact pin.'
                result['text_sha256']=digest;omitted.append({'json_path':'.'.join(trail+(key,)),'sha256':digest})
            else:result[key]=visit(item,trail+(key,))
        return result
    redacted=visit(document)
    return ((json.dumps(redacted,indent=2)+'\n').encode() if omitted else data),omitted

def source_rows():
    changed=read(BUILD/'CHANGE-SELECTION.json')['files']
    selected={r['candidate']:r for r in changed}
    require(len(selected)==len(changed),'ambiguous changed candidate')
    for r in changed:
        require(sha(r['original'])==r['original_sha256'] and sha(r['candidate'])==r['candidate_sha256'],'changed source drift')
        safe_packet_source(r['original']);safe_packet_source(r['candidate'])
    for part in ('profile','typescan-r2','thread'):
        for r in read(RUNTIME/part/'CHANGES.json')['files']:
            require(r['candidate'] in selected,'missing approved change '+r['candidate'])
            for key in ('original','candidate','original_sha256','candidate_sha256'):
                require(selected[r['candidate']][key]==r[key],'approved source selection mismatch')
    return changed

def main():
    twin=read(BUILD/'TWIN-RECEIPT.json');require(twin['status']=='MAC_ONLY_IDENTICAL_L23_RUNTIME_PAIR' and twin['new_imports']==[],'runtime twins not admitted')
    require(len(twin['passes'])==2,'two runtime passes required')
    for row in twin['passes']:check_pin(row['runtime']);check_pin(row['composition'])
    require(sha(BUILD/'pass1/runtime-l23.o')==sha(BUILD/'pass2/runtime-l23.o'),'runtime twins changed')
    inputs=read(PKG/'BUILD-INPUTS.json')
    for row in inputs['pins']:check_pin(row)
    for row in inputs['runtime_objects_by_pass'].values():check_pin(row)
    require(inputs['runtime_independent_twin_receipt']['sha256']==sha(BUILD/'TWIN-RECEIPT.json'),'packaging twin receipt mismatch')
    checks=verify_packages();changes=source_rows()
    digest=verify_profile(read(RUNTIME/'profile/CHANGES.json'),read(RUNTIME/'profile/profiles/townfall-build-facts.json'),read(ROOT/'outputs/2026-10-08-l22-runtime/profile/profiles/townfall-build-facts.json'),RUNTIME/'profile/units')
    require(inputs['l23_profile_digest']==digest,'packaged profile digest mismatch')
    counts=[]
    for i in (1,2):
        c=read(BUILD/f'pass{i}/COMPOSITION-RECEIPT.json')
        require(c['status']=='MAC_ONLY_L23_COMPLETE_LINK' and not c['actual_undefined_added'] and not c['actual_undefined_removed'],'runtime composition failed')
        counts.append({'recompiled_each_pass':c['fresh_current_pass'],'retained_units':c['retained_current_units'],'all_selected_units':c['selected_count'],'link_inputs':c['full_link_inputs']})
        require(c['fresh_current_pass']+c['retained_current_units']==c['selected_count'],'composition counts inconsistent')
    require(counts[0]==counts[1],'twin composition counts differ')
    lua=ROOT/'outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/harvest-20261008T142548Z-029d07/main.lua'
    require(sha(lua)=='f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658','retained original mod drift')
    result={'status':'MAC_ONLY_L23_TWINS_AND_PACKAGE_CHECKS_PASS',**checks,**counts[0],
      'magic':'4f153d1d','SDK':'0x11000000','runtime_twins':pin(BUILD/'TWIN-RECEIPT.json'),'profile_digest':digest,'retained_layout_facts':93,'new_container_offsets_shipped':0,'mod':pin(lua),'original_and_candidate_files_verified':len(changes),'console_contact':False,'native_execution':False,'menu_or_spawn_proven':False,'upload_approval_pending':True,'official_compiler_measurement_required_for_this_retained93_build':False,'epic_source_in_packet':False}
    write(PKG/'FINAL-RECEIPT.json',result)
    report=f'''# l23 local package verification

Two independently rebuilt runtimes and two packaged PRXs match. Both packages pass the retained 20 static gates, preserve constructor/destructor arrays, and retain the exact 207 l22 provider import identities. SELF magic is 4f153d1d; SDK stamp is 0x11000000.

PRX SHA256: `{checks['prx']['sha256']}` ({checks['prx']['bytes']} bytes). Profile digest: `{digest}`. Each pass recompiles {counts[0]['recompiled_each_pass']} units and retains {counts[0]['retained_units']} of {counts[0]['all_selected_units']} selected units; {counts[0]['link_inputs']} full link inputs. Counts come from current composition receipts.

The user-approved build retains all 93 prior layout facts. Map KeyProp/ValueProp and Set ElementProp remain absent from compiled declarations and conditional in discovery. Their blocked official compiler measurement and the separate 711-key inventory do not block this approved 93-fact build. No new container offset was shipped. Array Inner remains required.

The runtime includes optional discovery refusal, pointer validation, core-type gating and the actual ExecuteInGameThread diagnostic no-op. Host/unit tests and source review are copied for inspection. Native queue delivery, menu/spawn operation and runtime success remain unproved. VMT fallback remains design-only. Original mod content is unchanged.

This packet is local review material. No console action or upload occurred. Deploying this exact PRX still awaits upload approval. Official Epic source and its diagnostic excerpts are excluded; local evidence paths/hashes remain in receipts.
'''
    (PKG/'BUILD-REPORT.md').write_text(report)
    (PKG/'PREFLIGHT-TABLE.md').write_text('# l23 preflight scope\n\n66 joined entries: 50 required, 14 conditional, two runtime-discovered. Array Inner is required. Map KeyProp, Map ValueProp and Set ElementProp are conditional/unpinned and absent from the 93-fact declarations. Exact entries and offsets are in the copied JoinedLayoutPreflight.hpp and profile CHANGES.json.\n')
    (PKG/'FINAL-DIFF-REVIEW.md').write_text('# l23 final source selection\n\nAll '+str(len(changes))+' original/candidate hash pairs match the current build selection and each component CHANGES manifest. Existing l22 inputs remain immutable. Code changes are included as exact original/candidate pairs and unified diffs. Profile JSON copies and corresponding diffs omit historical licensed declaration snippets; ORIGIN-INDEX retains original hashes and redaction metadata. Independent queue review is implementation/profile/REVIEW.md.\n\n'+'\n'.join('- `'+Path(r['candidate']).name+'`: `'+r['candidate_sha256']+'`' for r in changes)+'\n')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ');packet=PKG/('review-l23-'+stamp);packet.mkdir(exist_ok=False)
    origins=[]
    def copy(p,rel):
        p=safe_packet_source(p);rel=Path(rel);require(not rel.is_absolute() and '..' not in rel.parts,'unsafe packet destination')
        data,omitted=packet_data(p);dest=packet/rel;require(not dest.exists(),'duplicate packet path '+str(rel));dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);origins.append({'original':str(p),'original_sha256':sha(p),'packet':str(rel),'sha256':hashlib.sha256(data).hexdigest(),'omitted_licensed_excerpts':omitted})
    copy(checks['prx']['path'],Path('artifact/modbridge_supplier_composition.prx'))
    for name in ('BUILD-REPORT.md','PREFLIGHT-TABLE.md','FINAL-RECEIPT.json','BUILD-INPUTS.json','PREPARATION-RECEIPT.json','FINAL-DIFF-REVIEW.md'):copy(PKG/name,name)
    # Explicit local implementation roots only. Never traverse Track B's official source tree.
    suffixes={'.md','.json','.py','.hpp','.h','.cpp','.c','.diff','.tsv','.log','.txt','.stdout','.stderr'}
    for section in ('profile','typescan-r2','thread','packaging-tools'):
        for p in sorted((RUNTIME/section).rglob('*')):
            if p.is_file() and p.suffix in suffixes and '__pycache__' not in p.parts:copy(p,Path('implementation')/p.relative_to(RUNTIME))
    for name in ('build_runtime.py','SOURCE-CHANGES.json'):
        if (RUNTIME/name).is_file():copy(RUNTIME/name,Path('implementation')/name)
    for name in ('package_combined.py','package_prx_l19.py','prep_guard.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py','checker-tests.log','inputs/NuGet.Config','STAGED-SOURCE-PINS.json','pass1-controller.log','pass2-controller.log'):copy(PKG/name,Path('packaging')/name)
    for r in changes:
        p=Path(r['original']);key=r['original_sha256'][:12]+'-'+p.name
        copy(p,Path('original-sources')/key)
        before,_=packet_data(p);after,_=packet_data(Path(r['candidate']))
        text=''.join(difflib.unified_diff(before.decode().splitlines(keepends=True),after.decode().splitlines(keepends=True),fromfile=r['original'],tofile=r['candidate']))
        d=packet/'diffs'/(key+'.diff');d.parent.mkdir(exist_ok=True);d.write_text(text)
    for i in (1,2):
        for name in ('BUILD-RECEIPT.json','STATIC-CHECKS.json','EXACT-IMPORT-IDENTITIES.json','CONSTRUCTOR-ARRAY-CENSUS.json','supplier-contract.json'):copy(PKG/f'pass-{i}'/name,Path(f'package-pass-{i}')/name)
        for name in ('COMPOSITION-RECEIPT.json','COMPILE-RECEIPT.json','IMPORT-DELTA.json','DEPENDENCY-GRAPH.json','undefined.stdout'):copy(BUILD/f'pass{i}'/name,Path(f'runtime-pass-{i}')/name)
    for name in ('TWIN-RECEIPT.json','SOURCE-SELECTION.json','CHANGE-SELECTION.json','headers.vfs.json'):copy(BUILD/name,Path('runtime')/name)
    copy(BASE/'pass-1/EXACT-IMPORT-IDENTITIES.json','baseline/l22-EXACT-IMPORT-IDENTITIES.json')
    write(packet/'ORIGIN-INDEX.json',origins)
    (packet/'README.md').write_text('Start with BUILD-REPORT.md and FINAL-RECEIPT.json. All current results are local-only; no l23 console execution occurred. Original mod is hash-pinned and unchanged. No official Epic source or compiler excerpt is included. Profile JSON review copies redact historical declaration text; original input hashes and omitted-text hashes are retained in ORIGIN-INDEX. The accepted build retains93 facts and ships no new container offsets.\n')
    files=[{'path':str(p.relative_to(packet)),**{k:v for k,v in pin(p).items() if k!='path'}} for p in sorted(packet.rglob('*')) if p.is_file()]
    write(packet/'MANIFEST.json',{'files':files})
    zip_path=packet.with_suffix('.zip')
    with zipfile.ZipFile(zip_path,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(packet.rglob('*')):
            if p.is_file():z.write(p,str(p.relative_to(packet)))
    with zipfile.ZipFile(zip_path) as z:
        require(z.testzip() is None,'ZIP integrity failed')
        for row in files:
            data=z.read(row['path']);require(len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],'ZIP member mismatch')
    receipt={'zip':pin(zip_path),'manifest_files_verified':len(files),'console_contact':False,'upload_approval_pending':True,'epic_source_included':False}
    write(PKG/'PACKET-RECEIPT.json',receipt);print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
