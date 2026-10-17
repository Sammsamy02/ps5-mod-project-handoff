#!/usr/bin/env python3
"""l26 per-variant packaging driver (l25 machinery repointed by agent CM).

Stages and runs the proven l24 packaging pipeline (package_combined.py +
package_prx_l19.py + prep_guard.py + check_import_identity.py +
extended_sections.py) once per l26 runtime variant:

  outputs/2026-10-08-l26-package-A/  <- variant A (normal registration)
  [L26][P2, agent FX 2026-10-10] variant B packaging is DEAD BY DEFAULT
  (env opt-in L26_PACK_B=1 only, mirroring build_variants.py L26_BUILD_B);
  FINAL-RECEIPT-VARIANT-B.json and the P10/P11 pair gates run only when B
  is explicitly opted in AND built.

Per variant the driver:
  1. copies the five frozen l24 packaging scripts byte-identical plus
     inputs/NuGet.Config (the packager restores dotnet with it);
  2. writes an adapted controller whose ONLY difference from the frozen
     l24 controller is the b.note(...) pass label (variant + safety text);
  3. derives BUILD-INPUTS.json from the frozen l24 manifest, substituting
     the l26 variant runtime objects/receipts (built on the l25-A BASE by
     build_variants.py), the profile lineage overlay (l26 tree candidates
     carrying the l25 FMap/FSet container promotion forward), the l26 part
     CHANGES.json files, and the copied script pins;
  4. --execute runs the controller twice (pass-1, pass-2) per variant and
     then enforces the l24 acceptance set: 20/20 static checks, preserved
     constructor arrays (.init_array 238 / .fini_array 2), exactly 207
     exact ELF import identities, identical pass-1/pass-2 PRX twins;
  5. writes FINAL-RECEIPT-VARIANT-<key>.json per ACTIVE variant plus, when
     both A and B were packaged, a cross-variant
     VARIANT-PAIR-PACKAGING-RECEIPT.json (A and B PRXes must differ; both
     must carry the identical 207-name import set). Default runs package A
     only and skip the pair receipt (P2);
  6. [agent HH 2026-10-08] at final-receipt time emits the PRX SYMBOLS
     SIDECAR <package-dir>/<prx-name>.symbols.json (PHASEA-PLANS #5 BUILD
     GAP closure; consumer contract = phasea-tools/symbolize_crash.py
     --symbols-sidecar): prx_sha256 is the packaged PRX's actual sha256
     (the pairing key the tool hard-refuses on mismatch), plus build_id,
     prx_bytes, tier-honest symbol sources (.dynsym anchors exact; the -r
     runtime.map reconstruction stays MAP-RECONSTRUCTED) and the game-side
     anchors block; gates P12 (valid JSON + required keys) and P13
     (prx_sha256 matches both packaging twins) fail the build like any
     other gate.

--plan lists every step, input hash and gate without executing anything.
No console/network operations. Requires build_variants.py --execute to have
completed (runtime twin variants) before --prepare/--execute.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.dont_write_bytecode=True
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pinrow(p):p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')

L24PKG=ROOT/'outputs/2026-10-08-l24-package'
L26RUN=HERE
PAIR_ROOT=ROOT/'outputs/2026-10-08-l26-build-variants'
TOKEN='MODBRIDGE_L25_SKIP_DELETE_LISTENER'
UNSAFE_NOTE=('UNSAFE FOR LONG-TERM USE: PRX built with '+TOKEN+
 ' so the delete-listener registration is skipped and stale object pointers '
 'are NOT cleared. Diagnostic A/B isolation only; never deploy long-term.')
VARIANTS=(
 {'key':'A','build':PAIR_ROOT/'l26-A','pkg':ROOT/'outputs/2026-10-08-l26-package-A','artifact':'runtime-l26-A.o',
  'note':'l26 variant A production (engine-side delete-listener registration via admission-verified engine FUObjectArray::Add/RemoveUObjectDeleteListener — variant C, fail-closed prologue-signature admission; B skip off); profile lineage carries the l25 container promotion; l23-admitted 207 import identities; local packaging only; no console contact.',
  'unsafe':False},
)
# [L26][P2] (agent FX, 2026-10-10; parked patch set under
# outputs/2026-10-08-l25-prep/parked/P2-variant-b-removal/): variant B
# packaging is DEAD BY DEFAULT - env opt-in L26_PACK_B=1 only (mirrors the
# build driver's L26_BUILD_B). Existing historical package-B dirs
# (2026-10-08-l26-package-B*) are FROZEN provenance (the frozen known-good
# record documents the B sibling PRX aa29c765) and are never touched.
# LANDMARK for env-appended variants: EO's NB packaging entry
# (parallel-candidate/03-packaging-nb-env.patch) appends after this tuple -
# rebase it onto this block (its old anchor was the removed B entry).
B_PKG_VARIANT={'key':'B','build':PAIR_ROOT/'l26-B','pkg':ROOT/'outputs/2026-10-08-l26-package-B','artifact':'runtime-l26-B.o',
  'note':'l26 variant B UNSAFE DIAGNOSTIC (delete-listener registration compiled out via -D'+TOKEN+'; stale object pointers not cleared); profile lineage carries the l25 container promotion; l23-admitted 207 import identities; local packaging only; no console contact.',
  'unsafe':True}
if os.environ.get('L26_PACK_B')=='1':
    VARIANTS=VARIANTS+(B_PKG_VARIANT,)
FROZEN_NOTE_LINE="    b.note('l24 retained93 profile (unchanged from l23); typescan-r3 refusal-rework change; l23-admitted 207 import identities; local packaging only.')"
COPY_FILES=('package_prx_l19.py','prep_guard.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py')
# manifest['pins'] rows under these l24 prefixes are remapped; anything else
# under them is a hard error so nothing silently keeps a stale l24 pin.
REMAP_SCRIPTS={'package_prx_l19.py','prep_guard.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py','package_combined.py','inputs/NuGet.Config'}

def l26_profile_candidates():
    d=json.loads((L26RUN/'profile/CHANGES.json').read_text())
    return {Path(r['candidate']).name:r for r in d['files']},d

def pakmount_profile_overlay(changes):
    path=L26RUN/'profile/units/PakMountCodeProfile.hpp'
    expected=(changes.get('added_header_hashes') or {}).get(str(path))
    if not expected or str(path) not in changes.get('added_headers',[]):raise RuntimeError('PakMountCodeProfile missing required profile header admission')
    if not path.is_file() or sha(path)!=expected:raise RuntimeError('PakMountCodeProfile header pin drift')
    return {'path':str(path),'sha256':expected,'bytes':path.stat().st_size,'destination':'lifecycle/PakMountCodeProfile.hpp'}

def derive_manifest(variant,record,steps,execute):
    m=json.loads((L24PKG/'BUILD-INPUTS.json').read_text())
    # [B7 round 10, 2026-10-12] declared Lua-as-C++ census delta (same delta build_variants gates as
    # G7-import-delta-declared-lua-as-cpp): Lua errors throw instead of longjmp, so the runtime object no
    # longer references exactly setjmp + longjmp. Nothing else may change.
    if os.environ.get('L26_LUA_AS_CPP')=='1':
        gone=['longjmp','setjmp']
        if not set(gone)<=set(m['runtime_strong_undefined']):raise RuntimeError('B7 census delta: l24 census lacks setjmp/longjmp')
        m['runtime_strong_undefined']=[x for x in m['runtime_strong_undefined'] if x not in gone]
        m['b7_lua_as_cpp_census_delta']={'removed':gone,'gate':'build_variants G7-import-delta-declared-lua-as-cpp'}
    bdir=variant['build'];pkg=variant['pkg']
    def pending(p):return {'path':str(p),'sha256':'PENDING-BUILD','bytes':'PENDING-BUILD'}
    def pin_or_pending(p,rel):return pinrow(p) if p.exists() else (record('P-await-runtime-artifacts',None,{'missing':str(p)}),pending(p))[1]
    m['runtime_source_selection']=pin_or_pending(bdir/'SOURCE-SELECTION.json','sel')
    m['runtime_object']=pin_or_pending(bdir/'pass1'/variant['artifact'],'rt')
    m['runtime_independent_twin_receipt']=pin_or_pending(bdir/'TWIN-RECEIPT.json','twin')
    m['runtime_composition_receipts']=[pin_or_pending(bdir/f'pass{n}'/'COMPOSITION-RECEIPT.json','comp') for n in (1,2)]
    m['runtime_objects_by_pass']={str(n):pin_or_pending(bdir/f'pass{n}'/variant['artifact'],'rt') for n in (1,2)}
    # profile lineage: the staged lineage must carry the l26 tree profile
    # headers (currently the l25 promotion carried forward byte-identical;
    # any l26 profile edits land as refreshed candidate hashes here).
    cand,prof_changes=l26_profile_candidates()
    overlays=[]
    for row in m['lineage_overlays']:
        name=row['destination']
        if name not in cand:raise RuntimeError('l26 profile candidate missing for overlay '+name)
        c=cand[name]
        assert sha(c['candidate'])==c['candidate_sha256']
        overlays.append({'path':c['candidate'],'sha256':c['candidate_sha256'],'bytes':Path(c['candidate']).stat().st_size,'destination':name})
    # [L26/L27 pin admission, agent-EQ 2026-10-09] NEW overlay destinations
    # from the profile candidates (not present in the l24 overlay set): the
    # six-pin CodeSignatureProfile.hpp must land in stage/lifecycle where
    # the bridge TU resolves it (it shadows the outer-headers 3-pin copy the
    # controller stages there first), and the kMaxPins=8 CodeImageCache.hpp
    # at the lineage root (Metadata layout must match the runtime side's
    # vfs-aliased candidate in every TU calling named_entry).
    for name,dest in (('CodeSignatureProfile.hpp','lifecycle/CodeSignatureProfile.hpp'),
                      ('CodeImageCache.hpp','CodeImageCache.hpp')):
        if name in cand and not any(o['destination']==dest for o in overlays):
            c=cand[name]
            assert sha(c['candidate'])==c['candidate_sha256']
            overlays.append({'path':c['candidate'],'sha256':c['candidate_sha256'],'bytes':Path(c['candidate']).stat().st_size,'destination':dest})
    required_pak_profile=pakmount_profile_overlay(prof_changes)
    if any(o['destination']==required_pak_profile['destination'] for o in overlays):raise RuntimeError('duplicate PakMountCodeProfile overlay')
    overlays.append(required_pak_profile)
    m['lineage_overlays']=overlays
    m.pop('l24_profile_digest',None);m.pop('l24_profile_changes',None)
    m['l26_profile_digest']=prof_changes.get('profile_digest')
    m['l26_profile_promoted_facts']=prof_changes.get('new_promoted_facts')
    m['l26_profile_lineage_note']='carried forward from the l25 promotion via the l26 tree candidates unless the profile agent lands l26 edits'
    # [L26/L27 pin admission, agent-EQ 2026-10-09] outer_headers: the frozen
    # l24 rows must still match their files byte-exactly (the six-pin
    # CodeSignatureProfile.hpp now arrives via the NEW lifecycle/ overlay
    # destination above, which the controller writes AFTER the outer-headers
    # copies and therefore shadows the 3-pin one). The l26 profile headers
    # EngineListenerOpsProfile.hpp + EngineToStringProfile.hpp are appended
    # as NEW outer_headers rows (pinned to the typescan-r4 CHANGES
    # added_header hashes) so the controller stages them into stage/lifecycle
    # where the bridge TU's six-pin CodeSignatureProfile.hpp includes them.
    # Any drift of the three FROZEN l24 outer headers remains fatal below.
    ts_changes=json.loads((L26RUN/'typescan-r4/CHANGES.json').read_text())
    ts_header_hashes=ts_changes.get('added_header_hashes') or {}
    new_outer=[]
    for row in m['outer_headers']:
        if sha(row['path'])!=row['sha256']:raise RuntimeError('frozen l24 outer header drifted: '+row['path'])
        new_outer.append(row)
    for header in ('EngineListenerOpsProfile.hpp','EngineToStringProfile.hpp'):
        p=L26RUN/'typescan-r4/include'/header
        if not p.exists():raise RuntimeError('l26 profile header missing: '+str(p))
        expected=ts_header_hashes.get(str(p))
        if expected is None or sha(p)!=expected:raise RuntimeError('l26 profile header does not match its typescan-r4 CHANGES added_header hash: '+str(p))
        new_outer.append(pinrow(p))
    m['outer_headers']=new_outer
    # lineage_source_pins are informational in the l24 manifest; re-pin from
    # disk so they describe the lineage ACTUALLY staged (overlay-2 carried the
    # kMaxPins 4->8 CodeImageCache.hpp bump for the six-pin profile).
    m['lineage_source_pins']=[(pinrow(r['path']) if Path(r['path']).exists() else r) for r in m.get('lineage_source_pins',[])]
    changes=[];added=[]
    for part in ('profile','typescan-r4','thread'):
        d=json.loads((L26RUN/part/'CHANGES.json').read_text());changes.extend(d['files']);added.extend(d.get('added_headers',[]))
    m['changed_runtime_units']={'files':changes,'added_headers':added,'note':'l26 twin-variant runtime changes on the l25-A BASE (in-flight logwriter/UFunction fixes + carried l25 lineage)'}
    marker=json.loads((bdir/'VARIANT-MARKER.json').read_text()) if (bdir/'VARIANT-MARKER.json').exists() else {'variant':variant['key'],'pending':True}
    m['l26_variant']=marker
    m['final_import_baseline']=pinrow(L24PKG/'pass-1/EXACT-IMPORT-IDENTITIES.json')
    m['parent_build_inputs']=pinrow(L24PKG/'BUILD-INPUTS.json')
    m['package_script']=pin_or_pending(pkg/'package_prx_l19.py','script')
    m['controller']=pin_or_pending(pkg/'package_combined.py','controller')
    # copyin-probe bridge wiring (agent Q, WIRING.md File 2): the staged
    # packaging lineage must compile Q's patched runtime_start_bridge.cpp.
    # The controller writes stage/'lifecycle/runtime_start_bridge.cpp' from
    # inputs['outer_bridge'], so substituting this one field performs the
    # wiring; observation-only, no import changes.
    qman=json.loads((L26RUN/'copyin-probe/MANIFEST.json').read_text())
    qb=qman['base_files']['runtime_start_bridge']
    if m['outer_bridge']['sha256']!=qb['base_sha256']:
        raise RuntimeError('l24 outer_bridge no longer matches the base agent Q patched: expected '+qb['base_sha256'])
    if not (Path(qb['candidate_path']).exists() and sha(qb['candidate_path'])==qb['candidate_sha256']):
        raise RuntimeError('agent-Q bridge candidate pin drift')
    m['outer_bridge']=pinrow(qb['candidate_path'])
    m['copyin_probe_wiring']={'runtime_start_bridge':{'base':{'path':qb['base_path'],'sha256':qb['base_sha256']},
        'patch':{'path':str(L26RUN/'copyin-probe'/qb['patch']),'sha256':qb['patch_sha256']},
        'wired_outer_bridge':{'path':qb['candidate_path'],'sha256':qb['candidate_sha256']},
        'ue4ss_side':'composed by build_variants.py (see copyin_probe_wiring in the variant build receipts / VARIANT-MARKER.json)',
        'import_delta':qman.get('import_delta'),'console_contact':False}}

    # [L27][FX fault-ledger desaturation] CopyinSink.hpp overlay (EQ
    # pattern): the wired bridge candidate includes the desaturated
    # FaultLedger header, so the staged lineage must compile THAT candidate,
    # never the frozen l19 overlay-2 copy (2018c9bd...). The controller
    # writes stage/<destination> AFTER the lineage copytree, so destination
    # 'CopyinSink.hpp' shadows the frozen root file the copytree placed
    # there. A missing manifest entry is a HARD error: silently compiling
    # the pre-desaturation ledger is exactly the silent-drift class this
    # driver refuses.
    if 'copyinsink' not in qman['base_files']:
        raise RuntimeError('copyin-probe MANIFEST missing the copyinsink base_files entry (fault-ledger candidate wiring, agent FX); refusing to stage the frozen pre-desaturation CopyinSink.hpp')
    qs=qman['base_files']['copyinsink']
    lineage_sink=Path(qs['base_path'])
    if not (lineage_sink.exists() and sha(lineage_sink)==qs['base_sha256']):
        raise RuntimeError('frozen lineage CopyinSink.hpp no longer matches the copyinsink base pin: expected '+qs['base_sha256'])
    if not (Path(qs['candidate_path']).exists() and sha(qs['candidate_path'])==qs['candidate_sha256']):
        raise RuntimeError('fault-ledger candidate CopyinSink.hpp pin drift (expected '+qs['candidate_sha256']+')')
    m['lineage_overlays'].append({'path':qs['candidate_path'],'sha256':qs['candidate_sha256'],
                                  'bytes':Path(qs['candidate_path']).stat().st_size,'destination':'CopyinSink.hpp'})
    # keep lineage_source_pins honest: the staged sink is now the candidate.
    m['lineage_source_pins']=[(pinrow(qs['candidate_path'])
                               if Path(r.get('path','')).name=='CopyinSink.hpp' and r.get('sha256')==qs['base_sha256']
                               else r) for r in m['lineage_source_pins']]
    m['fault_ledger_overlay']={'candidate':{'path':qs['candidate_path'],'sha256':qs['candidate_sha256']},
        'replaces_frozen_lineage':{'path':qs['base_path'],'sha256':qs['base_sha256']},
        'note':'[L27][FX] fault-ledger desaturation: invalid record() rejected loudly (never saturates), capacity saturation accessor-only (the admission gate does not consult it), real recorded faults still refuse; [ST] copyin_fault_ledger_st self-test line from the bridge',
        'console_contact':False}
    # pin remapping
    l24build=str(ROOT/'outputs/2026-10-08-l24-build-r2')+'/'
    l24pkg=str(L24PKG)+'/'
    l24run=str(ROOT/'outputs/2026-10-08-l24-runtime')+'/'
    l26map={'TWIN-RECEIPT.json':pin_or_pending(bdir/'TWIN-RECEIPT.json','twin'),
            'CHANGE-SELECTION.json':pin_or_pending(bdir/'CHANGE-SELECTION.json','cs'),
            'pass1/runtime-l24.o':pin_or_pending(bdir/'pass1'/variant['artifact'],'rt'),
            'pass2/runtime-l24.o':pin_or_pending(bdir/'pass2'/variant['artifact'],'rt'),
            'pass1/COMPOSITION-RECEIPT.json':pin_or_pending(bdir/'pass1'/'COMPOSITION-RECEIPT.json','comp'),
            'pass2/COMPOSITION-RECEIPT.json':pin_or_pending(bdir/'pass2'/'COMPOSITION-RECEIPT.json','comp'),
            'pass1/IMPORT-DELTA.json':pin_or_pending(bdir/'pass1'/'IMPORT-DELTA.json','id')}
    runmap={'profile/CHANGES.json':pinrow(L26RUN/'profile/CHANGES.json'),
            'typescan-r3/CHANGES.json':pinrow(L26RUN/'typescan-r4/CHANGES.json'),
            'thread/CHANGES.json':pinrow(L26RUN/'thread/CHANGES.json')}
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
            if rel in l26map:newpins.append(l26map[rel])
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
        receipt={'status':'MAC_ONLY_L26_PACKAGE_PREPARED','variant':variant['key'],'unsafe':variant['unsafe'],
                 'unsafe_note':UNSAFE_NOTE if variant['unsafe'] else None,'token':TOKEN,
                 'manifest':{'path':str(pkg/'BUILD-INPUTS.json'),'sha256':sha(pkg/'BUILD-INPUTS.json')},
                 'controller_diff':diff_note,'runtime_inputs':{'twin_receipt':manifest['runtime_independent_twin_receipt'],
                 'runtime_object':manifest['runtime_object'],'composition_receipts':manifest['runtime_composition_receipts']},
                 'lineage_overlays':manifest['lineage_overlays'],'copyin_probe_wiring':manifest['copyin_probe_wiring'],
                 'console_contact':False}
        write(pkg/'PREPARATION-RECEIPT.json',receipt)
    steps.append({'step':'write-build-inputs-manifest','variant':variant['key'],'path':str(pkg/'BUILD-INPUTS.json'),
                  'substituted_keys':['runtime_source_selection','runtime_object','runtime_independent_twin_receipt','runtime_composition_receipts','runtime_objects_by_pass','lineage_overlays','changed_runtime_units','package_script','controller','pins','final_import_baseline','parent_build_inputs','l26_variant']})
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
    checks=[];pkg_failed=[];arrays_seen=[]
    for passno in ('1','2'):
        out=pkg/('pass-'+passno)
        receipt=json.loads((out/'BUILD-RECEIPT.json').read_text())
        static=json.loads((out/'STATIC-CHECKS.json').read_text())
        ids=json.loads((out/'EXACT-IMPORT-IDENTITIES.json').read_text())
        arrays=json.loads((out/'CONSTRUCTOR-ARRAY-CENSUS.json').read_text())
        prx=out/prx_name
        # [gate law 2026-10-12] P5-P8 used to be recorded but never raised, so a
        # failed check still produced a PASS FINAL-RECEIPT (verify_build.sh
        # trusts that receipt). Every failure is now collected and refused
        # before the receipt is written.
        p5=receipt['status']=='MAC_PACKAGE_CHECKS_PASS'
        p6=static['status']=='STATIC_CHECKS_PASSED' and len(static['checks'])==20 and all(c['pass'] for c in static['checks'].values())
        p7=ids['status'].startswith('PASS') and ids['count']==207
        record('P5-build-receipt-'+passno,p5,{'status':receipt['status']})
        record('P6-static-checks-'+passno,p6,{'count':len(static['checks'])})
        record('P7-import-identities-'+passno,p7,{'count':ids['count']})
        init=arrays['combined']['.init_array']['slots'];fini=arrays['combined']['.fini_array']['slots']
        # P8 re-baseline 2026-10-10 (root, Claude Q4d): 238 -> 239. The +1
        # legacy .ctors slot (1432->1440 B) is attributable to the Claude
        # round-2 NEW_UNITS TUs (lua-lfb LuaFNameBridge.cpp + tcselftest
        # ToolchainSelfTest.cpp are new global-ctor contributors; fini
        # unchanged; census status itself PASS in both builds).
        p8=arrays['status'].startswith('PASS') and init==240 and fini==2
        record('P8-constructor-arrays-'+passno,p8,{'init':init,'fini':fini})
        arrays_seen.append((init,fini))
        pkg_failed+=[g+'-'+passno for g,ok in (('P5',p5),('P6',p6),('P7',p7),('P8',p8)) if not ok]
        checks.append({'pass':passno,'package':pinrow(out/'BUILD-RECEIPT.json'),'imports':pinrow(out/'EXACT-IMPORT-IDENTITIES.json'),
                       'static':pinrow(out/'STATIC-CHECKS.json'),'arrays':pinrow(out/'CONSTRUCTOR-ARRAY-CENSUS.json'),'prx':pinrow(prx)})
    if pkg_failed:raise RuntimeError('variant '+variant['key']+' packaging checks failed (no FINAL-RECEIPT written): '+', '.join(pkg_failed))
    a=json.loads((pkg/'pass-1'/'EXACT-IMPORT-IDENTITIES.json').read_text())
    ha=sha(pkg/'pass-1'/prx_name);hb=sha(pkg/'pass-2'/prx_name)
    record('P9-packaging-twins-identical',ha==hb,{'pass1':ha,'pass2':hb})
    if ha!=hb:raise RuntimeError('variant '+variant['key']+' packaging twins differ')
    marker=json.loads((variant['build']/'VARIANT-MARKER.json').read_text())
    sidecar=emit_symbols_sidecar(variant,record,steps)
    final={'status':'MAC_ONLY_L26_VARIANT_'+variant['key']+'_TWINS_AND_PACKAGE_CHECKS_PASS',
           'variant':{'key':variant['key'],'label':marker['label'],'define':marker['define'],'unsafe':variant['unsafe'],
                      'delete_listener_registration':marker['delete_listener_registration'],
                      'safety':UNSAFE_NOTE if variant['unsafe'] else 'normal production configuration','token':TOKEN},
           'prx':{'path':str(pkg/'pass-1'/prx_name),'sha256':ha,'bytes':(pkg/'pass-1'/prx_name).stat().st_size},
           'twin':{'path':str(pkg/'pass-2'/prx_name),'sha256':hb,'bytes':(pkg/'pass-2'/prx_name).stat().st_size},
           'checks':checks,'static_checks_each_pass':20,'exact_import_count':a['count'],
           'imports_unchanged':True,'import_baseline':{'path':str(L24PKG/'pass-1/EXACT-IMPORT-IDENTITIES.json'),'sha256':sha(L24PKG/'pass-1/EXACT-IMPORT-IDENTITIES.json')},
           'constructor_arrays':{'.init_array':arrays_seen[0][0],'.fini_array':arrays_seen[0][1]},
           'runtime_object_sha256':sha(variant['build']/'pass1'/variant['artifact']),
           'runtime_twins':{'path':str(variant['build']/'TWIN-RECEIPT.json'),'sha256':sha(variant['build']/'TWIN-RECEIPT.json')},
           'copyin_probe_wiring':marker.get('copyin_probe_wiring'),
           'symbols_sidecar':sidecar,
           'console_contact':False,'native_execution':False,'menu_or_spawn_proven':False,'upload_approval_pending':True,
           'mod':None}
    write(pkg/('FINAL-RECEIPT-VARIANT-'+variant['key']+'.json'),final)
    steps.append({'step':'write-final-receipt','variant':variant['key'],'path':str(pkg/('FINAL-RECEIPT-VARIANT-'+variant['key']+'.json'))})
    return final

def pair_packaging_receipt(finals,record,steps):
    # [L26][P2] the pair packaging receipt exists ONLY when both A and B
    # were packaged (variant B is env-gated dead by default). A default
    # A-only run records P10/P11 as planned-skip and writes no pair file.
    if 'A' not in finals or 'B' not in finals:
        record('P10-variant-prx-differ',None,{'note':'pair packaging receipt skipped: variant B not in the active packaging set (agent-FX P2 drop)'})
        record('P11-import-name-sets-equal',None,{'note':'pair packaging receipt skipped: variant B not in the active packaging set (agent-FX P2 drop)'})
        steps.append({'step':'write-variant-pair-packaging-receipt','skipped':True,
                      'note':'variant B absent; VARIANT-PAIR-PACKAGING-RECEIPT.json not written this run'})
        return None
    a,b=finals['A'],finals['B']
    names_a={r['name'] for r in json.loads(Path(a['prx']['path']).parent.joinpath('EXACT-IMPORT-IDENTITIES.json').read_text())['imports']}
    names_b={r['name'] for r in json.loads(Path(b['prx']['path']).parent.joinpath('EXACT-IMPORT-IDENTITIES.json').read_text())['imports']}
    record('P10-variant-prx-differ',a['prx']['sha256']!=b['prx']['sha256'],{'A':a['prx']['sha256'],'B':b['prx']['sha256']})
    record('P11-import-name-sets-equal',names_a==names_b and len(names_a)==207,{'count':len(names_a)})
    if a['prx']['sha256']==b['prx']['sha256']:raise RuntimeError('variant PRXes are identical: the skip-define had no effect')
    if names_a!=names_b:raise RuntimeError('variant import name sets differ: '+repr(names_a^names_b))
    receipt={'status':'MAC_ONLY_L26_VARIANT_PAIR_PACKAGING_RECEIPT','token':TOKEN,
             'variant_A_prx':a['prx'],'variant_B_prx':b['prx'],
             'variant_B_unsafe':UNSAFE_NOTE,
             'import_name_sets_identical':True,'import_count_each':len(names_a),
             'static_checks_each':20,'constructor_arrays_each':{'.init_array':238,'.fini_array':2},
             'console_contact':False,'native_execution':False,'upload_approval_pending':True}
    write(PAIR_ROOT/'VARIANT-PAIR-PACKAGING-RECEIPT.json',receipt)
    steps.append({'step':'write-variant-pair-packaging-receipt','path':str(PAIR_ROOT/'VARIANT-PAIR-PACKAGING-RECEIPT.json')})
    return receipt

# ---- [agent HH 2026-10-10] PRX symbols sidecar (PHASEA-PLANS #5 BUILD GAP) --
# Consumer contract = outputs/2026-10-08-l25-prep/phasea-tools/symbolize_crash.py
# --symbols-sidecar: prx_sha256 (64-hex) is the pairing key it hard-refuses on
# mismatch; symbols=[{name,addr,size}] (addr/size hex-str or int; extra keys
# tolerated). Map-reconstruction logic is REUSED from that tool (parse_map /
# reconstruct_from_map / elf_sections / elf_dynsym) — never rewritten here.
PHASEA_TOOLS=ROOT/'outputs/2026-10-08-l25-prep/phasea-tools'
SIDECAR_REQUIRED_KEYS=('schema','kind','emitted_by','prx','prx_sha256','build_id','symbols','sources')
_SYMZ=None
def symbolize_crash_mod():
    global _SYMZ
    if _SYMZ is None:
        import importlib.util
        spec=importlib.util.spec_from_file_location('symbolize_crash',PHASEA_TOOLS/'symbolize_crash.py')
        _SYMZ=importlib.util.module_from_spec(spec);spec.loader.exec_module(_SYMZ)
    return _SYMZ

def elf_build_id(b,sections):
    import struct
    for s in sections:
        if s['name']=='.note.gnu.build-id' and s['size']>=16:
            namesz,descsz,ntype=struct.unpack_from('<III',b,s['off'])
            name=b[s['off']+12:s['off']+12+namesz].rstrip(b'\0')
            if name==b'GNU' and descsz:
                return b[s['off']+12+namesz:s['off']+12+namesz+descsz].hex()
    return None

def validate_symbols_sidecar(path,prx_paths):
    """P12/P13 core check: sidecar present, valid JSON, required keys, valid
    64-hex prx_sha256, and prx_sha256 == sha256 of EVERY given PRX file.
    Returns (ok, details) — never raises on bad sidecars (gates record it)."""
    import re
    p=Path(path);det={'path':str(p)}
    if not p.exists():return False,{**det,'error':'sidecar not present'}
    try:sd=json.loads(p.read_text())
    except (OSError,ValueError) as e:return False,{**det,'error':'invalid JSON: %s'%e}
    det['keys']=sorted(sd.keys())
    missing=[k for k in SIDECAR_REQUIRED_KEYS if k not in sd]
    if missing:return False,{**det,'error':'missing required keys: %s'%missing}
    decl=sd['prx_sha256']
    if not isinstance(decl,str) or not re.fullmatch(r'[0-9a-fA-F]{64}',decl):
        return False,{**det,'error':'prx_sha256 not a 64-hex string (unpairable)'}
    det['prx_sha256']=decl.lower();det['prx_hashes']={}
    for pp in prx_paths:
        try:actual=sha(pp)
        except OSError as e:return False,{**det,'error':'cannot hash PRX %s: %s'%(pp,e)}
        det['prx_hashes'][str(pp)]=actual
        if actual!=decl.lower():
            return False,{**det,'error':'PAIRING MISMATCH vs %s (%s != %s)'%(pp,actual,decl.lower())}
    return True,det

def emit_symbols_sidecar(variant,record,steps,prx_name='modbridge_supplier_composition.prx'):
    """Emit <pkg>/<prx_name>.symbols.json at final-receipt time + gates P12/P13.

    Tier honesty (PHASEA-PLANS #5): symbols[] carries ONLY exact anchors
    (.dynsym from the bare ELF, DT_INIT/DT_FINI-named; a future final-link
    map with real VMAs would be promoted here as MAP-EXACT). The -r
    partial-link runtime.map reconstruction (VMA=0, order-assumed) is
    embedded under map_reconstructed[] with tier MAP-RECONSTRUCTED — a
    distinct key the consumer does NOT auto-trust as SIDECAR-EXACT; live
    reproduction: symbolize_crash.py --map <runtime.map> --prx <bare prx>.
    prx_sha256 pairs against the PACKAGED (faked-signed) pass-1 PRX; the
    pass-2 twin is byte-identical by gate P9 and re-checked by P13."""
    sc=symbolize_crash_mod()
    pkg=variant['pkg']
    prx1=pkg/'pass-1'/prx_name;prx2=pkg/'pass-2'/prx_name;bare=pkg/'pass-1'/'bare-original'/prx_name
    maps={n:variant['build']/('pass%d'%n)/'runtime.map' for n in (1,2)}
    for p in (prx1,prx2,bare,*maps.values()):
        if not p.exists():raise RuntimeError('symbols sidecar input missing: '+str(p))
    b=bare.read_bytes();secs=sc.elf_sections(b)
    dyn=sc.elf_dynsym(b,secs)
    if not dyn:raise RuntimeError('bare PRX exposes no .dynsym anchors: '+str(bare))
    bid=elf_build_id(b,secs)
    mp=sc.parse_map(maps[1]);rows,tier=sc.reconstruct_from_map(mp,secs)
    exact=[{'name':s['name'],'addr':'0x%x'%s['addr'],
            'size':(None if s['size'] is None else '0x%x'%s['size']),
            'tier':'DYNSYM-EXACT'} for s in dyn]
    if tier=='MAP-EXACT':
        exact.extend({'name':r['name'],'addr':'0x%x'%r['addr'],
                      'size':(None if r['size'] is None else '0x%x'%r['size']),
                      'tier':'MAP-EXACT'} for r in rows)
    mapblock={'tier':tier,'count':len(rows),
              'maps':{str(n):pinrow(maps[n]) for n in (1,2)},
              'sections_used':[{'name':s['name'],'addr':'0x%x'%s['addr'],'size':'0x%x'%s['size']}
                               for s in secs if s['addr']],
              'note':('order-assumed reconstruction: -r partial-link map (VMA=0) input '
                      'sections placed cumulatively at the bare PRX final section vaddrs; '
                      'approximate, NOT SIDECAR-EXACT' if tier=='MAP-RECONSTRUCTED' else
                      'final-link map with real VMAs; rows promoted to symbols[]'),
              'symbols':[{'name':r['name'],'addr':'0x%x'%r['addr'],
                          'size':(None if r['size'] is None else '0x%x'%r['size'])} for r in rows]}
    prx_sha=sha(prx1)
    sidecar_path=pkg/(prx_name+'.symbols.json')
    sd={'schema':1,'kind':'prx-symbols-sidecar',
       'emitted_by':'l26_packaging_driver.emit_symbols_sidecar (agent HH 2026-10-10; PHASEA-PLANS #5 BUILD GAP closure; consumer: phasea-tools/symbolize_crash.py --symbols-sidecar)',
       'variant':{'key':variant['key'],'unsafe':variant['unsafe']},
       'prx':prx_name,'prx_sha256':prx_sha,'prx_bytes':prx1.stat().st_size,
       'bare_prx':{'path':str(bare),'sha256':sha(bare),'bytes':len(b),
                   'note':'symbol extraction source (the packaged PRX is faked-signed SELF, not ELF)'},
       'build_id':bid,
       'build_id_source':'.note.gnu.build-id of the bare PRX' if bid else 'absent',
       'pairing_law':('prx_sha256 is the sha256 of the packaged pass-1 PRX; the pass-2 twin is '
                      'byte-identical (gate P9, re-checked by P13). symbolize_crash.py '
                      '--symbols-sidecar <this file> --prx <packaged PRX> hard-refuses on mismatch.'),
       'symbols':exact,'map_reconstructed':mapblock,
       'sources':{'dynsym_exact':{'count':len(dyn),'tier':'DYNSYM-EXACT'},
                  'map':{'tier':tier,'count':len(rows),
                         'note':'see map_reconstructed; consume with --map for tier honesty'},
                  'game_side_anchors':{'path':str(PHASEA_TOOLS/'game-symbols-townfall.json'),
                                       'note':('separate GAME-image sidecar (pairs with the game '
                                               'eboot by its recorded image_sha256_prefix, NOT with '
                                               'this PRX); pass --game-symbols separately')}},
       'console_contact':False}
    sidecar_path.write_text(json.dumps(sd,separators=(',',':'))+'\n')
    steps.append({'step':'emit-prx-symbols-sidecar','variant':variant['key'],'path':str(sidecar_path),
                  'prx_sha256':prx_sha,'build_id':bid,'symbols_exact':len(exact),
                  'map_reconstructed':{'tier':tier,'count':len(rows)}})
    ok12,det12=validate_symbols_sidecar(sidecar_path,[prx1])
    record('P12-symbols-sidecar-valid',ok12,det12)
    if not ok12:raise RuntimeError('P12 symbols sidecar gate failed: '+str(sidecar_path))
    ok13,det13=validate_symbols_sidecar(sidecar_path,[prx1,prx2])
    record('P13-symbols-sidecar-prx-pairing',ok13,{'prx_sha256':det13.get('prx_sha256'),
              'prx_hashes':det13.get('prx_hashes',{}),'twins_matched':ok13})
    if not ok13:raise RuntimeError('P13 symbols sidecar pairing gate failed: '+str(sidecar_path))
    return {'path':str(sidecar_path),'sha256':sha(sidecar_path),'bytes':sidecar_path.stat().st_size,
            'prx_sha256':prx_sha,'build_id':bid,'symbols_exact':len(exact),
            'map_reconstructed':{'tier':tier,'count':len(rows)},
            'gates':['P12-symbols-sidecar-valid','P13-symbols-sidecar-prx-pairing']}

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
        cand,_=l26_profile_candidates()
        record('P2-l26-profile-candidates',len(cand)>=3,{'candidates':sorted(cand)})
        pair_gates=([] if not any(v['key']=='B' for v in VARIANTS)
                    else ['P10 variant A/B PRX differ','P11 variant import name sets equal (207)'])
        steps.append({'step':'verify-expected-gates','static_checks_each_pass':20,'exact_import_identities':207,'init_array_slots':240,'fini_array_slots':2,
                      'gates':['P5 build receipt MAC_PACKAGE_CHECKS_PASS','P6 20/20 static checks all pass','P7 exactly 207 exact ELF import identities','P8 constructor arrays preserved 238/2','P9 pass-1/pass-2 PRX twins identical']+pair_gates+
                               ['P12 per-variant symbols sidecar <pkg>/<prx>.symbols.json valid (JSON + required keys + prx_sha256 == packaged pass-1 PRX)',
                                'P13 symbols sidecar prx_sha256 matches BOTH packaging twins'],
                      'pair_gates_note':('A/B pair gates active (B opted in)' if pair_gates else 'P10/P11 skipped: variant B not in the active packaging set (agent-FX P2 drop)')})
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
         'expected':{'static_checks_each_pass':20,'exact_import_identities':207,'init_array_slots':240,'fini_array_slots':2,'prx_basename':'modbridge_supplier_composition.prx'},
         'steps':steps,'gates':gates,
         'next':'build_variants.py --execute, then this driver --prepare/--execute' if args.plan else ('run this driver --execute' if args.prepare else 'complete'),
         'console_contact':False,'native_execution':False}
    target=PAIR_ROOT/'plans'/'PACKAGING-PLAN.json' if args.plan else (PAIR_ROOT/'PACKAGING-PREP-RECEIPT.json' if args.prepare else PAIR_ROOT/'PACKAGING-EXECUTION-RECEIPT.json')
    write(target,out)
    bad=[g for g in gates if g['ok'] is False]
    # [gate law 2026-10-12] verdict printed AFTER the failed-gate check (it used
    # to print PASS first); a run that checked zero gates is a failure.
    if not gates:raise SystemExit('FAIL: zero gates recorded')
    print(('PLAN ' if args.plan else ('PREPARED ' if args.prepare else ('FAIL ' if bad else 'PASS ')))+json.dumps({'mode':out['mode'],'steps':len(steps),'gates':len(gates),'failed':len(bad)}),flush=True)
    if bad:raise SystemExit('gates failed: '+json.dumps(bad))

if __name__=='__main__':main()
