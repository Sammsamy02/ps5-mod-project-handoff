#!/usr/bin/env python3
"""Prepare a fresh local packaging controller; never uploads or contacts PS5."""
from pathlib import Path
import copy,hashlib,json
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
BASE=ROOT/'outputs/2026-10-08-menu-complete-package';BUILD=ROOT/'outputs/2026-10-08-l22-build-r3';OUT=ROOT/'outputs/2026-10-08-l22-package-r2'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def main():
    twin=json.loads((BUILD/'TWIN-RECEIPT.json').read_text());assert twin['status']=='MAC_ONLY_IDENTICAL_L22_RUNTIME_PAIR'
    OUT.mkdir(exist_ok=False)
    (OUT/'inputs').mkdir()
    (OUT/'inputs/NuGet.Config').write_bytes((BASE/'inputs/NuGet.Config').read_bytes())
    for name in ['prep_guard.py','package_prx_l19.py','check_import_identity.py','extended_sections.py','test_import_identity.py','test_extended_sections.py']:
        (OUT/name).write_bytes((BASE/name).read_bytes())
    text=(BASE/'package_combined.py').read_text()
    anchor="        write(HERE/'STAGED-SOURCE-PINS.json',{'inputs_sha256':sha(HERE/'BUILD-INPUTS.json'),"
    replacement="""        # l22: only the reviewed local profile headers overlay the frozen bootstrap.
        for row in inputs['l22_lineage_overlays']:
            (stage/row['destination']).write_bytes(pin(row).read_bytes())
"""+anchor
    assert text.count(anchor)==1;text=text.replace(anchor,replacement)
    (OUT/'package_combined.py').write_text(text)
    inputs=json.loads((BASE/'BUILD-INPUTS.json').read_text())
    inputs.update(runtime_source_selection=pin(BUILD/'SOURCE-SELECTION.json'),runtime_object=pin(BUILD/'pass1/runtime-l22.o'),runtime_independent_twin_receipt=pin(BUILD/'TWIN-RECEIPT.json'),runtime_composition_receipts=[pin(BUILD/f'pass{i}/COMPOSITION-RECEIPT.json') for i in [1,2]],package_script=pin(OUT/'package_prx_l19.py'),controller=pin(OUT/'package_combined.py'),parent_build_inputs=pin(BASE/'BUILD-INPUTS.json'))
    changes=json.loads((HERE/'profile/CHANGES.json').read_text());overlays=[]
    for row in changes['files']:
        if Path(row['original']).name in ['RuntimeBuildProfile.hpp','CompiledNativeLayoutDeclarations.hpp']:
            overlays.append({**pin(row['candidate']),'destination':Path(row['original']).name})
    # Added profile helper headers may be included by the copied profile itself.
    for path in changes.get('added_headers',[]):
        overlays.append({**pin(path),'destination':Path(path).name})
    assert {'RuntimeBuildProfile.hpp','CompiledNativeLayoutDeclarations.hpp'} <= {r['destination'] for r in overlays}
    inputs['l22_lineage_overlays']=overlays
    inputs['changed_runtime_units']=json.loads((BUILD/'CHANGE-SELECTION.json').read_text())
    inputs.pop('changed_runtime_unit',None)
    inputs['pins'] += [pin(OUT/'inputs/NuGet.Config')]+[pin(OUT/n) for n in ['prep_guard.py','check_import_identity.py','extended_sections.py']]+overlays+[pin(BUILD/'TWIN-RECEIPT.json')]
    # Raw import names must remain exactly the preceding admitted runtime set.
    after=(BUILD/'pass1/undefined.stdout').read_text();names=sorted({s.split()[-1] for s in after.splitlines() if s.split() and s.split()[0]=='U'})
    assert names==inputs['runtime_strong_undefined']
    inputs['native_menu_proven']=False;inputs['console_actions_authorized']=False
    write(OUT/'BUILD-INPUTS.json',inputs)
    write(OUT/'PREPARATION-RECEIPT.json',{'scope':'l22 local-only','parent_inputs':pin(BASE/'BUILD-INPUTS.json'),'inputs':pin(OUT/'BUILD-INPUTS.json'),'controller':pin(OUT/'package_combined.py'),'bootstrap_header_overlays':overlays,'runtime_imports_unchanged':True,'console_contact':False})
    print(json.dumps({'package_directory':str(OUT),'profile_overlays':len(overlays),'imports_unchanged':True}))
if __name__=='__main__':main()
