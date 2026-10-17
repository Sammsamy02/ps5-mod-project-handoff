"""Freeze explicit new sources and actual corrected twin-runtime receipts.

This prepares a local build manifest only. It cannot grant console actions or
replace the independent acceptance still pending for lifecycle changes.
"""
import argparse,hashlib,importlib.util,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.path.insert(0,str(HERE));from prep_guard import pin

def record(path):
    p=Path(path).resolve();return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}

def validate_compositions(paths,runtime):
    if len(paths)!=2:raise ValueError('exactly two full-link receipts required')
    receipts=[]
    for expected_pass,path in enumerate(paths,1):
        data=json.loads(Path(path).read_text())
        if data.get('status')!='ACTUAL_FINAL_RELOCATABLE_LINK_PASS_NOT_PACKAGE_ADMISSION':
            raise ValueError('full link did not pass')
        if (data.get('pass'),data.get('selected_count'),data.get('full_link_inputs'))!=(expected_pass,374,391):
            raise ValueError('wrong pass or incomplete actual composition')
        if data.get('native_execution') is not False or data.get('menu_success') is not False:
            raise ValueError('host receipt makes unsupported native claim')
        obj=data['object'];pin(obj)
        if obj['sha256']!=runtime['sha256'] or obj['size']!=runtime['bytes']:
            raise ValueError('full runtime pair differs')
        for key in ('compile_receipt','link_command','dependency_graph','import_delta'):pin(data[key])
        receipts.append(data)
    if Path(receipts[0]['object']['path']).resolve()==Path(receipts[1]['object']['path']).resolve():
        raise ValueError('same artifact cannot prove a second full link')
    expected={'unreal-unrealinitializer','native-thread-name-threadnameruntime'}
    compile_two=json.loads(pin(receipts[1]['compile_receipt']).read_text())
    if compile_two.get('status')!='FRESH_COMBINED_TARGET_COMPILE_PASS' or {r['name'] for r in compile_two['results']}!=expected:
        raise ValueError('second pass must freshly compile both final changed implementations')
    a={r['name']:r for r in receipts[0]['selected_units']}
    b={r['name']:r for r in receipts[1]['selected_units']}
    if len(a)!=374 or a.keys()!=b.keys():raise ValueError('selected unit census differs')
    for name in a:
        if a[name]['source_sha256']!=b[name]['source_sha256'] or a[name]['object_sha256']!=b[name]['object_sha256']:
            raise ValueError('individual object/source pair differs: '+name)
    return [record(p) for p in paths]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--selection',required=True,type=Path)
    parser.add_argument('--runtime',required=True,type=Path)
    parser.add_argument('--twin-receipt',required=True,type=Path)
    parser.add_argument('--composition-receipt',required=True,action='append',type=Path)
    args=parser.parse_args()
    destination=HERE/'BUILD-INPUTS.json'
    if destination.exists():raise ValueError('never overwrite a build-input freeze')
    selection=json.loads(args.selection.read_text())
    for role in ('source_pins','receipt_pins','artifact_pins'):
        for row in selection[role]:pin(row)
    spec=importlib.util.spec_from_file_location('original_copy',HERE/'package_prx_l19.py')
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    runtime=record(args.runtime);twin=record(args.twin_receipt)
    compositions=validate_compositions(args.composition_receipt,runtime)
    # Strictly use corrected artifacts passed explicitly by the controller.
    strong,weak,_=m.readelf_undefined(m.PIN['llvm']/'llvm-readelf',args.runtime)
    if 'scePthreadGetthreadid' not in strong or 'bridge_native_thread_import_ready' not in strong:
        raise ValueError('final current-thread/owned-row implementation not selected')
    outer=ROOT/'outputs/2026-10-08-combined-outer-lifecycle'
    outer_selection=json.loads((outer/'SOURCE-SELECTION-v2.json').read_text())
    for key in ('selected_entry','selected_bridge'):pin(outer_selection[key])
    if json.loads((outer/'target-v2/TARGET-RECEIPT.json').read_text())['status']!='PASS':
        raise ValueError('actual revised outer SDK compile missing')
    lineage=ROOT/'outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2'
    lineage_pins=[record(p) for p in sorted(lineage.rglob('*')) if p.is_file()]
    captures={name:record(m.PIN['composition_dir']/name) for name in
        ('sdknm-libSceLibcInternal.so.stdout','sdknm-libScePad.so.stdout')}
    captures['sdknm-libkernel.so.stdout']=record(ROOT/'outputs/2026-10-08-native-thread-import-evidence/sdk-libkernel-defined.stdout')
    historical_receipts={key:record(ROOT/'outputs/2026-10-07-l19-r4-full-build/receipts'/name)
                         for key,name in (('bootstrap_import_evidence','l19-bootstrap-import-extension.json'),
                                          ('unlink_import_evidence','l19-unlink-import-evidence.json'))}
    for key,row in historical_receipts.items():
        if row['sha256']!=m.PIN[key+'_sha256']:raise ValueError('historical receipt drift: '+key)
    tool_pins=[record(m.PIN[key]) for key in ('import_contract','base_receipt','frozen_prx','finalize_sdk','make_fself','dotnet','bootstrap_import_base_contract')]
    tool_pins.extend(record(m.PIN['llvm']/name) for name in ('llvm-nm','llvm-readelf','llvm-config','ld.lld'))
    tool_pins.extend(record(m.PIN['sdk']/'bin'/name) for name in ('prospero-clang','prospero-clang++'))
    tool_pins.extend(record(m.PIN['sdk']/'target/lib'/name) for name in ('libkernel.so','libScePad.so','libSceLibcInternal.so'))
    tools_source=ROOT/'.superpowers/native-dependency-spike/native-runtime-import-probe-1/packager/source/tools'
    for project in ('SharpProspero.Link','SharpProspero.Prx'):
        tool_pins.extend(record(p) for p in sorted((tools_source/project).glob('*')) if p.suffix in ('.cs','.csproj'))
    tool_pins.append(record(HERE/'inputs/NuGet.Config'))
    original_mod=ROOT/'outputs/2026-10-07-l16-full-build/console-test-20261007T183202Z-2318a17f/provenance/TownfallItemSpawner/Scripts/main.lua'
    if record(original_mod)['sha256']!='f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658':
        raise ValueError('original PC mod drift')
    data={'scope':'LOCAL_COMBINED_PACKAGE_ONLY','runtime_source_selection':record(args.selection),
          'runtime_object':runtime,'runtime_independent_twin_receipt':twin,
          'runtime_composition_receipts':compositions,
          'runtime_strong_undefined':sorted(strong),'runtime_weak_undefined':sorted(weak),
          'outer_entry':outer_selection['selected_entry'],'outer_bridge':outer_selection['selected_bridge'],
          'outer_headers':outer_selection['outer_headers'],'lineage_source_pins':lineage_pins,
          'historical_receipts':historical_receipts,
          'lineage_tree_pin':record(lineage/'import_check.c'),'SDK_captures':captures,
          'diagnostic_source':record(ROOT/'outputs/2026-10-04-zcode-team/worker-diagnostic-sink/staging/diagnostic.c'),
          'import_evidence':record(ROOT/'outputs/2026-10-08-native-thread-import-evidence/IMPORT-EVIDENCE.json'),
          'import_check':record(ROOT/'outputs/2026-10-08-thread-import-admission/overlay/native_thread_import_check.c'),
          'package_script':record(HERE/'package_prx_l19.py'),'controller':record(HERE/'package_combined.py'),
          'pins':tool_pins+compositions+[twin,record(original_mod),record(HERE/'prep_guard.py'),record(HERE/'check_import_identity.py'),
                           record(ROOT/'outputs/2026-10-08-thread-import-admission/import_admission.py'),
                           record(outer/'FINAL-RECEIPT-v2.json')],
          'independent_cleanup_acceptance':False,'console_actions_authorized':False,'native_menu_proven':False}
    destination.write_text(json.dumps(data,indent=2)+'\n')
    print(json.dumps({'inputs':str(destination),'sha256':record(destination)['sha256'],'raw_undefined':len(strong),
                      'scope':data['scope'],'console_actions_authorized':False}))

if __name__=='__main__':main()
