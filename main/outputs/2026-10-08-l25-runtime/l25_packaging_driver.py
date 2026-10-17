#!/usr/bin/env python3
"""l25 per-variant packaging driver (Claude-approved l25 item 5, agent R prep).

Stages and runs the proven l24 packaging pipeline (package_combined.py +
package_prx_l19.py + prep_guard.py + check_import_identity.py +
extended_sections.py) once per l25 runtime variant:

  outputs/2026-10-08-l25-package-A/  <- variant A (normal registration)
  outputs/2026-10-08-l25-package-B/  <- variant B (skip-define diagnostic)

Per variant the driver:
  1. copies the five frozen l24 packaging scripts byte-identical plus
     inputs/NuGet.Config (the packager restores dotnet with it);
  2. writes an adapted controller whose ONLY difference from the frozen
     l24 controller is the b.note(...) pass label (variant + safety text);
  3. derives BUILD-INPUTS.json from the l24 manifest, substituting the l25
     variant runtime objects/receipts, the l25 profile lineage overlay
     (FMap/FSet container facts promoted by l25 item 5), the l25 part
     CHANGES.json files, and the copied script pins;
  4. --execute runs the controller twice (pass-1, pass-2) per variant and
     then enforces the l24 acceptance set: 20/20 static checks, preserved
     constructor arrays (.init_array 238 / .fini_array 2), exactly 207
     exact ELF import identities, identical pass-1/pass-2 PRX twins;
  5. writes FINAL-RECEIPT-VARIANT-{A,B}.json plus a cross-variant
     VARIANT-PAIR-PACKAGING-RECEIPT.json (A and B PRXes must differ; both
     must carry the identical 207-name import set).

--plan lists every step, input hash and gate without executing anything.
No console/network operations. Requires build_variants.py --execute to have
completed (runtime twin variants) before --prepare/--execute.
"""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.dont_write_bytecode=True
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pinrow(p):p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')

L24PKG=ROOT/'outputs/2026-10-08-l24-package'
L25RUN=HERE
PAIR_ROOT=ROOT/'outputs/2026-10-08-l25-build-variants'
TOKEN='MODBRIDGE_L25_SKIP_DELETE_LISTENER'
UNSAFE_NOTE=('UNSAFE FOR LONG-TERM USE: PRX built with '+TOKEN+
 ' so the delete-listener registration is skipped and stale object pointers '
 'are NOT cleared. Diagnostic A/B isolation only; never deploy long-term.')
VARIANTS=(
 {'key':'A','build':PAIR_ROOT/'l25-A','pkg':ROOT/'outputs/2026-10-08-l25-package-A','artifact':'runtime-l25-A.o',
  'note':'l25 variant A (normal delete-listener registration); l25 profile promotes 5 new container facts; l23-admitted 207 import identities; local packaging only; no console contact.',
  'unsafe':False},
 {'key':'B','build':PAIR_ROOT/'l25-B','pkg':ROOT/'outputs/2026-10-08-l25-package-B','artifact':'runtime-l25-B.o',
  'note':'l25 variant B UNSAFE DIAGNOSTIC (delete-listener registration compiled out via -D'+TOKEN+'; stale object pointers not cleared); l25 profile promotes 5 new container facts; l23-admitted 207 import identities; local packaging only; no console contact.',
  'unsafe':True},
)
FROZEN_NOTE_LINE="    b.note('l24 retained93 profile (unchanged from l23); typescan-r3 refusal-rework change; l23-admitted 207 import identities; local packaging only.')"
COPY_FILES=('package_prx_l19.py','prep_guard.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py')
# manifest['pins'] rows under these l24 prefixes are remapped; anything else
# under them is a hard error so nothing silently keeps a stale l24 pin.
REMAP_SCRIPTS={'package_prx_l19.py','prep_guard.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py','package_combined.py','inputs/NuGet.Config'}

def l25_profile_candidates():
    d=json.loads((L25RUN/'profile/CHANGES.json').read_text())
    return {Path(r['candidate']).name:r for r in d['files']},d

def derive_manifest(variant,record,steps,execute):
    m=json.loads((L24PKG/'BUILD-INPUTS.json').read_text())
    bdir=variant['build'];pkg=variant['pkg']
    def pending(p):return {'path':str(p),'sha256':'PENDING-BUILD','bytes':'PENDING-BUILD'}
    def pin_or_pending(p,rel):return pinrow(p) if p.exists() else (record('P-await-runtime-artifacts',None,{'missing':str(p)}),pending(p))[1]
    m['runtime_source_selection']=pin_or_pending(bdir/'SOURCE-SELECTION.json','sel')
    m['runtime_object']=pin_or_pending(bdir/'pass1'/variant['artifact'],'rt')
    m['runtime_independent_twin_receipt']=pin_or_pending(bdir/'TWIN-RECEIPT.json','twin')
    m['runtime_composition_receipts']=[pin_or_pending(bdir/f'pass{n}'/'COMPOSITION-RECEIPT.json','comp') for n in (1,2)]
    m['runtime_objects_by_pass']={str(n):pin_or_pending(bdir/f'pass{n}'/variant['artifact'],'rt') for n in (1,2)}
    # l25 profile promotion: the staged lineage must carry the NEW l25 profile
    # headers (the l24 manifest overlaid the unchanged l23 versions).
    cand,prof_changes=l25_profile_candidates()
    overlays=[]
    for row in m['lineage_overlays']:
        name=row['destination']
        if name not in cand:raise RuntimeError('l25 profile candidate missing for overlay '+name)
        c=cand[name]
        assert sha(c['candidate'])==c['candidate_sha256']
        overlays.append({'path':c['candidate'],'sha256':c['candidate_sha256'],'bytes':Path(c['candidate']).stat().st_size,'destination':name})
    m['lineage_overlays']=overlays
    m.pop('l24_profile_digest',None);m.pop('l24_profile_changes',None)
    m['l25_profile_digest']=prof_changes.get('profile_digest')
    m['l25_profile_promoted_facts']=prof_changes.get('new_promoted_facts')
    changes=[];added=[]
    for part in ('profile','typescan-r4','thread'):
        d=json.loads((L25RUN/part/'CHANGES.json').read_text());changes.extend(d['files']);added.extend(d.get('added_headers',[]))
    m['changed_runtime_units']={'files':changes,'added_headers':added,'note':'l25 twin-variant runtime changes (listener instrumentation + phase tags + review fixes + profile promotion)'}
    marker=json.loads((bdir/'VARIANT-MARKER.json').read_text()) if (bdir/'VARIANT-MARKER.json').exists() else {'variant':variant['key'],'pending':True}
    m['l25_variant']=marker
    m['final_import_baseline']=pinrow(L24PKG/'pass-1/EXACT-IMPORT-IDENTITIES.json')
    m['parent_build_inputs']=pinrow(L24PKG/'BUILD-INPUTS.json')
    m['package_script']=pin_or_pending(pkg/'package_prx_l19.py','script')
    m['controller']=pin_or_pending(pkg/'package_combined.py','controller')
    # copyin-probe bridge wiring (agent Q, WIRING.md File 2): the staged
    # packaging lineage must compile Q's patched runtime_start_bridge.cpp.
    # The controller writes stage/'lifecycle/runtime_start_bridge.cpp' from
    # inputs['outer_bridge'], so substituting this one field performs the
    # wiring; observation-only, no import changes.
    qman=json.loads((L25RUN/'copyin-probe/MANIFEST.json').read_text())
    qb=qman['base_files']['runtime_start_bridge']
    if m['outer_bridge']['sha256']!=qb['base_sha256']:
        raise RuntimeError('l24 outer_bridge no longer matches the base agent Q patched: expected '+qb['base_sha256'])
    if not (Path(qb['candidate_path']).exists() and sha(qb['candidate_path'])==qb['candidate_sha256']):
        raise RuntimeError('agent-Q bridge candidate pin drift')
    m['outer_bridge']=pinrow(qb['candidate_path'])
    m['copyin_probe_wiring']={'runtime_start_bridge':{'base':{'path':qb['base_path'],'sha256':qb['base_sha256']},
        'patch':{'path':str(L25RUN/'copyin-probe'/qb['patch']),'sha256':qb['patch_sha256']},
        'wired_outer_bridge':{'path':qb['candidate_path'],'sha256':qb['candidate_sha256']},
        'ue4ss_side':'composed by build_variants.py (see copyin_probe_wiring in the variant l25 build receipts / VARIANT-MARKER.json)',
        'import_delta':qman.get('import_delta'),'console_contact':False}}
    # pin remapping
    l24build=str(ROOT/'outputs/2026-10-08-l24-build-r2')+'/'
    l24pkg=str(L24PKG)+'/'
    l24run=str(ROOT/'outputs/2026-10-08-l24-runtime')+'/'
    l25map={'TWIN-RECEIPT.json':pin_or_pending(bdir/'TWIN-RECEIPT.json','twin'),
            'CHANGE-SELECTION.json':pin_or_pending(bdir/'CHANGE-SELECTION.json','cs'),
            'pass1/runtime-l24.o':pin_or_pending(bdir/'pass1'/variant['artifact'],'rt'),
            'pass2/runtime-l24.o':pin_or_pending(bdir/'pass2'/variant['artifact'],'rt'),
            'pass1/COMPOSITION-RECEIPT.json':pin_or_pending(bdir/'pass1'/'COMPOSITION-RECEIPT.json','comp'),
            'pass2/COMPOSITION-RECEIPT.json':pin_or_pending(bdir/'pass2'/'COMPOSITION-RECEIPT.json','comp'),
            'pass1/IMPORT-DELTA.json':pin_or_pending(bdir/'pass1'/'IMPORT-DELTA.json','id')}
    runmap={'profile/CHANGES.json':pinrow(L25RUN/'profile/CHANGES.json'),
            'typescan-r3/CHANGES.json':pinrow(L25RUN/'typescan-r4/CHANGES.json'),
            'thread/CHANGES.json':pinrow(L25RUN/'thread/CHANGES.json')}
    scriptmap={name:pin_or_pending(pkg/name,'script') for name in REMAP_SCRIPTS}
    newpins=[]
    for row in m['pins']:
        p=row['path']
        if p.startswith(l24pkg):
            rel=p[len(l24pkg):]
            if rel in scriptmap:newpins.append(scriptmap[rel])
            else:raise RuntimeError('unmapped l24-package pin: '+p)
        elif p.startswith(l24build):
            rel=p[len(l24build):]
            if rel in l25map:newpins.append(l25map[rel])
            else:raise RuntimeError('unmapped l24-build-r2 pin: '+p)
        elif p.startswith(l24run):
            rel=p[len(l24run):]
            if rel in runmap:newpins.append(runmap[rel])
            else:raise RuntimeError('unmapped l24-runtime pin: '+p)
        else:newpins.append(row)
    m['pins']=newpins
    m['scope']='LOCAL_COMBINED_PACKAGE_ONLY'
    m['console_actions_authorized']=False
    m['variant_warning']=UNSAFE_NOTE if variant['unsafe'] else None
    return m

def prepare_variant(variant,record,steps,execute):
    pkg=variant['pkg']
    adapted_text=None
    if execute and pkg.exists():
        # --prepare may have staged this dir already. Reuse ONLY if every
        # staged file still matches what we would write now (no silent drift,
        # no overwrite of a divergent tree).
        adapted_text=(L24PKG/'package_combined.py').read_text()
        if adapted_text.count(FROZEN_NOTE_LINE)!=1:raise RuntimeError('frozen controller note line not found exactly once')
        adapted_text=adapted_text.replace(FROZEN_NOTE_LINE,"    b.note('"+variant['note']+"')")
        ok=(pkg/'BUILD-INPUTS.json').exists() and (pkg/'PREPARATION-RECEIPT.json').exists()
        if ok:
            for name in COPY_FILES:ok=ok and (pkg/name).exists() and sha(pkg/name)==sha(L24PKG/name)
            ok=ok and (pkg/'inputs/NuGet.Config').exists() and sha(pkg/'inputs/NuGet.Config')==sha(L24PKG/'inputs/NuGet.Config')
            ok=ok and (pkg/'package_combined.py').exists() and (pkg/'package_combined.py').read_text()==adapted_text
        record('P1-package-dir-reuse-verified',ok,{'path':str(pkg)})
        if not ok:raise RuntimeError('existing package dir does not match the reproducible staging (drift or partial failure): '+str(pkg))
        fresh=derive_manifest(variant,record,steps,execute)
        staged_text=(pkg/'BUILD-INPUTS.json').read_text()
        if staged_text!=json.dumps(fresh,indent=2)+'\n':
            raise RuntimeError('staged BUILD-INPUTS.json differs from freshly derived manifest: '+str(pkg))
        manifest=json.loads(staged_text)
        return manifest
    if execute:
        record('P1-package-dir-fresh',not pkg.exists(),{'path':str(pkg)})
        pkg.mkdir(parents=True,exist_ok=False);(pkg/'inputs').mkdir()
        for name in COPY_FILES:
            src=L24PKG/name;dst=pkg/name;shutil.copyfile(src,dst)
            assert sha(src)==sha(dst)
        shutil.copyfile(L24PKG/'inputs/NuGet.Config',pkg/'inputs/NuGet.Config')
        text=(L24PKG/'package_combined.py').read_text()
        if text.count(FROZEN_NOTE_LINE)!=1:raise RuntimeError('frozen controller note line not found exactly once')
        adapted=text.replace(FROZEN_NOTE_LINE,"    b.note('"+variant['note']+"')")
        (pkg/'package_combined.py').write_text(adapted)
        diff_note={'only_change':'b.note(...) pass label','old':FROZEN_NOTE_LINE.strip(),'new':"b.note('"+variant['note']+"')"}
    else:
        diff_note={'only_change':'b.note(...) pass label','old':FROZEN_NOTE_LINE.strip(),'new':"b.note('"+variant['note']+"')"}
        steps.append({'step':'copy-frozen-packaging-scripts','variant':variant['key'],'dest':str(pkg),
                      'files':[{ 'source':{'path':str(L24PKG/n),'sha256':sha(L24PKG/n)},'dest':str(pkg/n)} for n in COPY_FILES],
                      'inputs/NuGet.Config':{'source':{'path':str(L24PKG/'inputs/NuGet.Config'),'sha256':sha(L24PKG/'inputs/NuGet.Config')},'dest':str(pkg/'inputs/NuGet.Config')}})
        steps.append({'step':'write-adapted-controller','variant':variant['key'],'dest':str(pkg/'package_combined.py'),'source':{'path':str(L24PKG/'package_combined.py'),'sha256':sha(L24PKG/'package_combined.py')},'diff':diff_note})
    manifest=derive_manifest(variant,record,steps,execute)
    if execute:
        write(pkg/'BUILD-INPUTS.json',manifest)
        receipt={'status':'MAC_ONLY_L25_PACKAGE_PREPARED','variant':variant['key'],'unsafe':variant['unsafe'],
                 'unsafe_note':UNSAFE_NOTE if variant['unsafe'] else None,'token':TOKEN,
                 'manifest':{'path':str(pkg/'BUILD-INPUTS.json'),'sha256':sha(pkg/'BUILD-INPUTS.json')},
                 'controller_diff':diff_note,'runtime_inputs':{'twin_receipt':manifest['runtime_independent_twin_receipt'],
                 'runtime_object':manifest['runtime_object'],'composition_receipts':manifest['runtime_composition_receipts']},
                 'lineage_overlays':manifest['lineage_overlays'],'copyin_probe_wiring':manifest['copyin_probe_wiring'],
                 'console_contact':False}
        write(pkg/'PREPARATION-RECEIPT.json',receipt)
    steps.append({'step':'write-build-inputs-manifest','variant':variant['key'],'path':str(pkg/'BUILD-INPUTS.json'),
                  'substituted_keys':['runtime_source_selection','runtime_object','runtime_independent_twin_receipt','runtime_composition_receipts','runtime_objects_by_pass','lineage_overlays','changed_runtime_units','package_script','controller','pins','final_import_baseline','parent_build_inputs','l25_variant']})
    return manifest

def run_packaging_passes(variant,record,steps):
    pkg=variant['pkg']
    for passno in ('1','2'):
        out=pkg/('pass-'+passno)
        record('P3-pass-dir-fresh-'+passno,not out.exists(),{'path':str(out)})
        if out.exists():raise RuntimeError('refuse to overwrite '+str(out))
        cmd=[sys.executable,str(pkg/'package_combined.py'),'--pass-id',passno]
        steps.append({'step':'run-packaging-pass','variant':variant['key'],'pass':passno,'argv':cmd})
        log=pkg/(f'pass{passno}-controller.log')
        r=subprocess.run(cmd,capture_output=True,text=True)
        log.write_text(r.stdout+'\n===STDERR===\n'+r.stderr)
        record('P4-controller-exit-'+passno,r.returncode==0,{'variant':variant['key'],'pass':passno,'rc':r.returncode,'log':str(log)})
        if r.returncode!=0:raise RuntimeError('packaging pass failed: '+str(log))

def finalize_variant(variant,record,steps):
    pkg=variant['pkg'];prx_name='modbridge_supplier_composition.prx'
    checks=[]
    for passno in ('1','2'):
        out=pkg/('pass-'+passno)
        receipt=json.loads((out/'BUILD-RECEIPT.json').read_text())
        static=json.loads((out/'STATIC-CHECKS.json').read_text())
        ids=json.loads((out/'EXACT-IMPORT-IDENTITIES.json').read_text())
        arrays=json.loads((out/'CONSTRUCTOR-ARRAY-CENSUS.json').read_text())
        prx=out/prx_name
        record('P5-build-receipt-'+passno,receipt['status']=='MAC_PACKAGE_CHECKS_PASS',{'status':receipt['status']})
        record('P6-static-checks-'+passno,static['status']=='STATIC_CHECKS_PASSED' and len(static['checks'])==20 and all(c['pass'] for c in static['checks'].values()),{'count':len(static['checks'])})
        record('P7-import-identities-'+passno,ids['status'].startswith('PASS') and ids['count']==207,{'count':ids['count']})
        init=arrays['combined']['.init_array']['slots'];fini=arrays['combined']['.fini_array']['slots']
        record('P8-constructor-arrays-'+passno,arrays['status'].startswith('PASS') and init==238 and fini==2,{'init':init,'fini':fini})
        checks.append({'pass':passno,'package':pinrow(out/'BUILD-RECEIPT.json'),'imports':pinrow(out/'EXACT-IMPORT-IDENTITIES.json'),
                       'static':pinrow(out/'STATIC-CHECKS.json'),'arrays':pinrow(out/'CONSTRUCTOR-ARRAY-CENSUS.json'),'prx':pinrow(prx)})
    a=json.loads((pkg/'pass-1'/'EXACT-IMPORT-IDENTITIES.json').read_text())
    ha=sha(pkg/'pass-1'/prx_name);hb=sha(pkg/'pass-2'/prx_name)
    record('P9-packaging-twins-identical',ha==hb,{'pass1':ha,'pass2':hb})
    if ha!=hb:raise RuntimeError('variant '+variant['key']+' packaging twins differ')
    marker=json.loads((variant['build']/'VARIANT-MARKER.json').read_text())
    final={'status':'MAC_ONLY_L25_VARIANT_'+variant['key']+'_TWINS_AND_PACKAGE_CHECKS_PASS',
           'variant':{'key':variant['key'],'label':marker['label'],'define':marker['define'],'unsafe':variant['unsafe'],
                      'delete_listener_registration':marker['delete_listener_registration'],
                      'safety':UNSAFE_NOTE if variant['unsafe'] else 'normal production configuration','token':TOKEN},
           'prx':{'path':str(pkg/'pass-1'/prx_name),'sha256':ha,'bytes':(pkg/'pass-1'/prx_name).stat().st_size},
           'twin':{'path':str(pkg/'pass-2'/prx_name),'sha256':hb,'bytes':(pkg/'pass-2'/prx_name).stat().st_size},
           'checks':checks,'static_checks_each_pass':20,'exact_import_count':a['count'],
           'imports_unchanged':True,'import_baseline':{'path':str(L24PKG/'pass-1/EXACT-IMPORT-IDENTITIES.json'),'sha256':sha(L24PKG/'pass-1/EXACT-IMPORT-IDENTITIES.json')},
           'constructor_arrays':{'.init_array':238,'.fini_array':2},
           'runtime_object_sha256':sha(variant['build']/'pass1'/variant['artifact']),
           'runtime_twins':{'path':str(variant['build']/'TWIN-RECEIPT.json'),'sha256':sha(variant['build']/'TWIN-RECEIPT.json')},
           'copyin_probe_wiring':marker.get('copyin_probe_wiring'),
           'console_contact':False,'native_execution':False,'menu_or_spawn_proven':False,'upload_approval_pending':True,
           'mod':None}
    write(pkg/('FINAL-RECEIPT-VARIANT-'+variant['key']+'.json'),final)
    steps.append({'step':'write-final-receipt','variant':variant['key'],'path':str(pkg/('FINAL-RECEIPT-VARIANT-'+variant['key']+'.json'))})
    return final

def pair_packaging_receipt(finals,record,steps):
    a,b=finals['A'],finals['B']
    names_a={r['name'] for r in json.loads(Path(a['prx']['path']).parent.joinpath('EXACT-IMPORT-IDENTITIES.json').read_text())['imports']}
    names_b={r['name'] for r in json.loads(Path(b['prx']['path']).parent.joinpath('EXACT-IMPORT-IDENTITIES.json').read_text())['imports']}
    record('P10-variant-prx-differ',a['prx']['sha256']!=b['prx']['sha256'],{'A':a['prx']['sha256'],'B':b['prx']['sha256']})
    record('P11-import-name-sets-equal',names_a==names_b and len(names_a)==207,{'count':len(names_a)})
    if a['prx']['sha256']==b['prx']['sha256']:raise RuntimeError('variant PRXes are identical: the skip-define had no effect')
    if names_a!=names_b:raise RuntimeError('variant import name sets differ: '+repr(names_a^names_b))
    receipt={'status':'MAC_ONLY_L25_VARIANT_PAIR_PACKAGING_RECEIPT','token':TOKEN,
             'variant_A_prx':a['prx'],'variant_B_prx':b['prx'],
             'variant_B_unsafe':UNSAFE_NOTE,
             'import_name_sets_identical':True,'import_count_each':len(names_a),
             'static_checks_each':20,'constructor_arrays_each':{'.init_array':238,'.fini_array':2},
             'console_contact':False,'native_execution':False,'upload_approval_pending':True}
    write(PAIR_ROOT/'VARIANT-PAIR-PACKAGING-RECEIPT.json',receipt)
    steps.append({'step':'write-variant-pair-packaging-receipt','path':str(PAIR_ROOT/'VARIANT-PAIR-PACKAGING-RECEIPT.json')})
    return receipt

def main():
    parser=argparse.ArgumentParser(description=__doc__.split('\n')[0])
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--plan',action='store_true',help='list steps/hashes/gates; execute nothing')
    mode.add_argument('--prepare',action='store_true',help='stage per-variant package dirs (requires completed variant builds)')
    mode.add_argument('--execute',action='store_true',help='prepare + run both packaging passes per variant + finalize receipts')
    args=parser.parse_args()
    gates=[];steps=[]
    def record(name,ok,detail):
        gates.append({'gate':name,'ok':(bool(ok) if ok is not None else None),'detail':detail})
        print(json.dumps({'gate':name,'ok':(bool(ok) if ok is not None else 'PLANNED'),'detail':detail}),flush=True)
    if args.prepare or args.execute:
        for variant in VARIANTS:
            for need in ('TWIN-RECEIPT.json','pass1/'+variant['artifact'],'pass2/'+variant['artifact'],'SOURCE-SELECTION.json','VARIANT-MARKER.json','pass1/COMPOSITION-RECEIPT.json','pass2/COMPOSITION-RECEIPT.json','pass1/IMPORT-DELTA.json','CHANGE-SELECTION.json'):
                p=variant['build']/need
                record('P0-runtime-input-'+variant['key']+'-'+need.replace('/','_'),p.exists(),{'path':str(p)})
                if not p.exists():raise RuntimeError('missing variant build output (run build_variants.py --execute first): '+str(p))
    if args.plan:
        l24pins={row['path']:row['sha256'] for row in json.loads((L24PKG/'BUILD-INPUTS.json').read_text())['pins']}
        for name in COPY_FILES+('package_combined.py','inputs/NuGet.Config'):
            p=L24PKG/name
            expected=l24pins.get(str(p))
            record('P2-frozen-source-pin',p.exists() and (expected is None or sha(p)==expected),{'path':str(p),'sha256':sha(p) if p.exists() else None,'manifest_pinned':expected is not None})
        cand,_=l25_profile_candidates()
        record('P2-l25-profile-candidates',len(cand)>=3,{'candidates':sorted(cand)})
        steps.append({'step':'verify-expected-gates','static_checks_each_pass':20,'exact_import_identities':207,'init_array_slots':238,'fini_array_slots':2,
                      'gates':['P5 build receipt MAC_PACKAGE_CHECKS_PASS','P6 20/20 static checks all pass','P7 exactly 207 exact ELF import identities','P8 constructor arrays preserved 238/2','P9 pass-1/pass-2 PRX twins identical','P10 variant A/B PRX differ','P11 variant import name sets equal (207)']})
    manifests={}
    for variant in VARIANTS:
        manifests[variant['key']]=prepare_variant(variant,record,steps,args.prepare or args.execute)
    if args.execute:
        finals={}
        for variant in VARIANTS:
            run_packaging_passes(variant,record,steps)
            finals[variant['key']]=finalize_variant(variant,record,steps)
        pair_packaging_receipt(finals,record,steps)
    out={'mode':'plan' if args.plan else ('prepare' if args.prepare else 'execute'),
         'driver':str(Path(__file__).resolve()),'driver_sha256':sha(Path(__file__).resolve()),'token':TOKEN,
         'frozen_sources':str(L24PKG),'variants':[{'key':v['key'],'build':str(v['build']),'package_dir':str(v['pkg']),'artifact':v['artifact'],'unsafe':v['unsafe'],'unsafe_note':UNSAFE_NOTE if v['unsafe'] else None} for v in VARIANTS],
         'expected':{'static_checks_each_pass':20,'exact_import_identities':207,'init_array_slots':238,'fini_array_slots':2,'prx_basename':'modbridge_supplier_composition.prx'},
         'steps':steps,'gates':gates,
         'next':'build_variants.py --execute, then this driver --prepare/--execute' if args.plan else ('run this driver --execute' if args.prepare else 'complete'),
         'console_contact':False,'native_execution':False}
    target=PAIR_ROOT/'plans'/'PACKAGING-PLAN.json' if args.plan else (PAIR_ROOT/'PACKAGING-PREP-RECEIPT.json' if args.prepare else PAIR_ROOT/'PACKAGING-EXECUTION-RECEIPT.json')
    write(target,out)
    print(('PLAN ' if args.plan else ('PREPARED ' if args.prepare else 'PASS '))+json.dumps({'mode':out['mode'],'steps':len(steps),'gates':len(gates)}),flush=True)
    bad=[g for g in gates if g['ok'] is False]
    if bad:raise SystemExit('gates failed: '+json.dumps(bad))

if __name__=='__main__':main()
