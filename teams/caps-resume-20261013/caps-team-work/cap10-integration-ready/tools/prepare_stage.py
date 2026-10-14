"""Prepare only the owned #7+#10 stage; never import a build driver or compile."""
import argparse,ast,copy,difflib,hashlib,importlib.util,json,re,shutil,subprocess
from pathlib import Path
OWN=Path(__file__).resolve().parents[1];TEAM=OWN.parent;MAIN=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
RTREL=Path('outputs/2026-10-08-l26-runtime');STAGE=OWN/'derived/target-ready';RT=STAGE/RTREL
spec=importlib.util.spec_from_file_location('cap10_integration',OWN/'tools/cap10_integration.py');c10=importlib.util.module_from_spec(spec);spec.loader.exec_module(c10)
LLVM=MAIN/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
L24=MAIN/'outputs/2026-10-08-l24-package';BASE=MAIN/'outputs/2026-10-08-l25-build-variants/l25-A'
provenance=[]
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,indent=2)+'\n')
def copy_input(p,d):
 p=Path(p);d=Path(d);d.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,d)
 provenance.append({'source':c10.pin(p),'staged':c10.pin(d)})
 return c10.pin(d)
def sub(s,old,new,count=1):
 if s.count(old)!=count:raise ValueError('exact patch anchor count: '+repr(old[:90])+' '+str(s.count(old)))
 return s.replace(old,new)
def nm(path,label):
 args=[str(LLVM/'llvm-nm'),'--extern-only','--format=posix',str(path)]
 p=subprocess.run(args,text=True,capture_output=True,check=True);out=OWN/f'evidence/{label}.nm';out.write_text(p.stdout)
 return {x.split()[0]:x.split()[1] for x in p.stdout.splitlines() if len(x.split())>=2},c10.pin(out)
def rel(row):
 row=dict(row);p=Path(row['path']);row['path']=str(p.relative_to(RT)) if p.is_relative_to(RT) else str(p);return row

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--owner-dir',type=Path);parser.add_argument('--runtime-archive',type=Path);args=parser.parse_args()
 if bool(args.owner_dir)!=bool(args.runtime_archive):parser.error('--owner-dir and --runtime-archive are a bound reviewed input pair')
 for p in (TEAM/'cap07-mac-ready/derived/target-ready').rglob('*'):
  if p.is_file():
   dest=STAGE/p.relative_to(TEAM/'cap07-mac-ready/derived/target-ready')
   if dest==RT/'typescan-r4/candidate/src/UE4SSProgram.cpp':continue
   copy_input(p,dest)
 # The composed callback was staged separately and supplied to #7's compiler.
 callback=json.loads((OWN/'evidence/CALLBACK-STAGE.json').read_text());c10.checked(callback['source']);c10.checked(callback['composed']);c10.checked(callback['patch'])
 for p in (TEAM/'cap10/native').iterdir():
  if p.suffix in ('.hpp','.cpp'):
   source=args.owner_dir/p.name if p.name in ('MonoOwner.cpp','MonoOwner.hpp') and args.owner_dir else p
   copy_input(source,RT/'cap10/native'/p.name)
 port=json.loads((TEAM/'cap10-port-closure/HANDOFF-MANIFEST.json').read_text())
 headers=[]
 for row in port['files']:
  if row['packet_path'].startswith('handoff/runtime-headers/'):
   c10.checked({'path':row['source'],'sha256':row['sha256']})
   headers.append(copy_input(row['source'],RT/'cap10/mono-headers'/Path(row['packet_path']).relative_to('handoff/runtime-headers')))
 artifacts=[copy_input(TEAM/'cap10-cross-aot/artifacts/mscorlib.full-interp.ps5.o',RT/'cap10/lib/mscorlib.full-interp.ps5.o'),copy_input(args.runtime_archive or TEAM/'cap10-port-closure/target-work/mono/mini/.libs/libmonosgen-2.0.a',RT/'cap10/lib/libmonosgen-2.0.a')]
 assets=[]
 for p,destination in [(TEAM/'cap10-cross-aot/managed-input/mscorlib.dll','lib/mono/4.5/mscorlib.dll'),(TEAM/'cap10-port-closure/target-work/data/config','etc/mono/config')]:
  r=copy_input(p,RT/'cap10/runtime-assets'/destination);r['destination']=destination;assets.append(r)
 # Embedded Probe.dll is already pinned in ProbeImage.hpp; its config is
 # retained as explicit managed provenance, not loaded from a guessed path.
 probe=copy_input(TEAM/'cap10/managed/Probe.dll',RT/'cap10/managed/Probe.dll')
 hostconfig=RT/'cap10/managed/Probe.dll.config'
 if hostconfig.exists():hostconfig.unlink()
 oldmanifest=json.loads((L24/'BUILD-INPUTS.json').read_text());entry=oldmanifest['outer_entry'];c10.checked(entry)
 roots=re.findall(r'^#define\s+MODBRIDGE_MOD_ROOT\s+"([^"\n]+)"',Path(entry['path']).read_text(),re.M)
 assert len(roots)==1 and roots[0].startswith('/')
 link=json.loads((BASE/'pass1/LINK-COMMAND.json').read_text())
 for row in link['fixed_inputs']:c10.checked(row)
 baseobj=BASE/'pass1/runtime-l25-A.o';fragment=OWN/'evidence/cap10-with-base-fixed.o'
 prog=TEAM/'cap07-mac-ready/target/program-cap10-composed-fixed/program.o'
 defs,basedefs=nm(baseobj,'base-global-symbols');frag,fragsym=nm(fragment,'fragment-global-symbols');program,progsym=nm(prog,'program-global-symbols')
 baseline=c10.nm_undefined((BASE/'pass1/undefined.stdout').read_text())
 all_defs={n for table in (defs,frag,program) for n,t in table.items() if t not in ('U','w','v')}
 raw=(baseline|{(t,n) for table in (frag,program) for n,t in table.items() if t in ('U','w','v')})
 raw={(t,n) for t,n in raw if n not in all_defs}
 rawproof={'qualification':'Computed expected contract from actual BASE runtime plus real target component/production callback symbols under exact BASE fixed libraries. Actual complete link must equal this set; this is not a final PRX contract. No guessed provider identities.',
  'baseline':c10.pin(BASE/'pass1/undefined.stdout'),'base_object':c10.pin(baseobj),'fragment_object':c10.pin(fragment),'program_object':c10.pin(prog),'base_symbols':basedefs,'fragment_symbols':fragsym,'program_symbols':progsym,'symbols':sorted(raw)}
 write(OWN/'evidence/RAW-IMPORT-CONTRACT.json',rawproof)
 baseline_contract=json.loads((L24/'pass-1/supplier-contract.json').read_text());known={x['ImportSymbol']['Name'] for x in baseline_contract['contract']['imports']}
 boundaries={'__init_array_start','__init_array_end','__fini_array_start','__fini_array_end','__dso_handle','__eh_frame_start','__eh_frame_end','__eh_frame_hdr_start','__eh_frame_hdr_end','_DYNAMIC'}
 needed=sorted({n for t,n in raw if t=='U'}-known-boundaries)
 providers=[]
 for family in ('libSceLibcInternal','libkernel'):
  p=MAIN/'.superpowers/native-observations/user-return-20261003T140907Z-3bf558aa'/f'{family}.sprx';provider=c10.pin(p)
  familyrow=next(r['ImportSymbol'] for r in baseline_contract['contract']['imports'] if r['ImportSymbol']['Soname']==family+'.prx')
  provider['identity_family']={k:familyrow[k] for k in ('ModuleName','ModuleVersion','LibraryName','LibraryVersion')}
  provider['identity_family_evidence']=c10.pin(L24/'pass-1/supplier-contract.json');providers.append(provider)
 sdk=MAIN/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib';sdkexports={};sdk_evidence={};captures={}
 for family in ('libSceLibcInternal','libkernel','libSceNet'):
  table,receipt=nm(sdk/f'{family}.so','sdk-'+family);captures[Path(receipt['path']).name]=copy_input(receipt['path'],RT/'cap10/sdk-evidence'/Path(receipt['path']).name)
  for name,typ in table.items():
   if typ not in ('U','w','v'):sdkexports.setdefault(name,[]).append(family+'.prx')
 for name,families in sdkexports.items():
  if len(families)==1:sdk_evidence[name]='sdk-'+families[0].removesuffix('.prx')+'.nm'
 platform_candidates=[n for n in needed if sdkexports.get(n)]
 owned_component_requirements=[n for n in needed if not sdkexports.get(n)]
 providerrows,gaps=c10.resolve_providers(platform_candidates,{'providers':providers,'sdk_exports':sdkexports})
 report={'qualification':'Exact final rows require real saved provider export bytes, matching SDK declaration. Final actual combined object and final PRX identities remain independently enforced. Missing identities are hard failures.',
  'baseline_contract':c10.pin(L24/'pass-1/supplier-contract.json'),'raw_contract':c10.pin(OWN/'evidence/RAW-IMPORT-CONTRACT.json'),'component_platform_candidates':platform_candidates,'component_owned_requirements':owned_component_requirements,'additional_rows':providerrows,'gaps':gaps,'sdk_evidence':sdk_evidence,
  'expected_final_count':None,'final_rows':None,'scope':'component diagnostic only; actual final provider additions computed after real current/bootstrap object definitions'}
 write(RT/'cap10/PROVIDER-READINESS.json',report);write(OWN/'evidence/PROVIDER-READINESS.json',report)
 # Driver minimally delegates contract-specific operations to the helper.
 helper=copy_input(OWN/'tools/cap10_integration.py',RT/'cap10_integration.py')
 b=(RT/'build_variants.py').read_text()
 b=sub(b,'TOKEN=\'MODBRIDGE_L25_SKIP_DELETE_LISTENER\'',"import cap10_integration as cap10\nCAP10=cap10.load(HERE)\nTOKEN='MODBRIDGE_L25_SKIP_DELETE_LISTENER'")
 b=sub(b,'def make_new_units(units,passno,sel,objects):',"NEW_UNITS.update(CAP10['units'])\n\ndef make_new_units(units,passno,sel,objects):")
 b=sub(b,'    # Z9 loud no-op guard:',"    include_dirs=sorted(set(include_dirs)|set(CAP10['include_dirs']))\n    added+= [r['path'] for r in CAP10['header_pins']]\n    # Z9 loud no-op guard:")
 b=sub(b,"        if u['name'] in sel.get('lua_cpp_units',()):argv=lua_cpp_argv(argv)","        if u['name'] in sel.get('lua_cpp_units',()):argv=lua_cpp_argv(argv)\n        argv=cap10.compile_argv(argv,u['name'],CAP10)")
 b=sub(b,"        cmd[cmd.index('-o'):0]=[str(u['object']) for u in units if u['name'] in NEW_UNITS]","        cmd=cap10.wire_link(cmd,units,NEW_UNITS,CAP10)\n        cap10.link_census(cmd,units,CAP10,391+len(NEW_UNITS)+len(CAP10['artifacts']))")
 b=b.replace('391+len(NEW_UNITS)',"391+len(NEW_UNITS)+len(CAP10['artifacts'])")
 b=b.replace("391+len(NEW_UNITS)+len(CAP10['artifacts'])+len(CAP10['artifacts'])","391+len(NEW_UNITS)+len(CAP10['artifacts'])")
 start=b.index('        if LUA_AS_CPP:\n            expected=(symbols(before)');end=b.index("        write(pdir/'LINK-COMMAND.json'",start)
 b=b[:start]+"        raw_contract=cap10.verify_raw(after,predicted_raw)\n        write(pdir/'CAP10-RAW-CONTRACT.json',predicted_raw)\n        record('G7-cap10-exact-raw-import-contract',True,raw_contract)\n"+b[end:]
 b=sub(b,"        budgeted_run(cmd,pdir,'link')","        predicted_raw=cap10.link_import_contract(cmd)\n        budgeted_run(cmd,pdir,'link')")
 b=sub(b,"'import_delta':pin(pdir/'IMPORT-DELTA.json'),","'import_delta':pin(pdir/'IMPORT-DELTA.json'),'cap10_raw_contract':pin(pdir/'CAP10-RAW-CONTRACT.json'),")
 b=sub(b,"'fixed_inputs':link['fixed_inputs']}","'fixed_inputs':link['fixed_inputs']+CAP10['artifacts'],'cap10_integration':CAP10['manifest']}")
 b=sub(b,"selection['artifact_pins']+=[x['runtime'] for x in results]","selection['artifact_pins']+=[x['runtime'] for x in results]+CAP10['artifacts']+CAP10['runtime_assets']\n        selection['source_pins']+=CAP10['pinned_inputs']+[CAP10['manifest']]")
 b=sub(b,"'new_imports':[]", "'new_imports':p1['undefined_delta'][0],'cap10_integration':CAP10['manifest']")
 b=b.replace("'selected_units':len(sel['base']['selected_units'])","'selected_units':len(sel['base']['selected_units'])+len(NEW_UNITS)")
 b=b.replace("'retained_units':len(sel['base']['selected_units'])-len(sel['affected'])","'retained_units':len(sel['base']['selected_units'])+len(NEW_UNITS)-len(sel['affected'])")
 b=sub(b,"'G7-import-delta-declared-lua-as-cpp' if LUA_AS_CPP else 'G7-import-delta-none'","'G7-cap10-exact-raw-import-contract'")
 b=sub(b,"'note':('llvm-nm -u must equal BASE minus exactly '+repr(LUA_CPP_IMPORT_DELTA['removed'])+' (enforced at execute)') if LUA_AS_CPP else 'llvm-nm -u equality with BASE enforced at execute'","'note':'llvm-nm -u must exactly equal the independently computed ELF/AR input closure, including any selected Lua-as-C++ object changes'")
 (RT/'build_variants.py').write_text(b)
 # Package script's existing function contracts are retained. New rows come
 # from exact provider bytes; inherited rows continue their original path.
 p=(L24/'package_prx_l19.py').read_text();p=sub(p,'import argparse','import cap10_integration as cap10\nimport argparse');p=sub(p,'def derive_contract(b: Build, out_contract: Path) -> dict:',"CAP10_PROVIDER_ROWS = {}\n\ndef derive_contract(b: Build, out_contract: Path) -> dict:")
 p=sub(p,'donors = sorted(set(ALL_EXTENDED_IMPORTS.values()))','donors = sorted({v for n,v in ALL_EXTENDED_IMPORTS.items() if n not in CAP10_PROVIDER_ROWS})')
 a=p.index('        donor = by_name[donor_name]',p.index('def derive_contract'));z=p.index('        rows.append(new_row)',a)
 legacy=p[a:z];p=p[:a]+"        if name in CAP10_PROVIDER_ROWS:\n            new_row = json.loads(json.dumps(CAP10_PROVIDER_ROWS[name]))\n        else:\n"+''.join('    '+line+'\n' for line in legacy.splitlines())+p[z:]
 # Compute actual final requirements only after all actual bootstrap objects
 # exist, before the partial link and before Packager.
 anchor='    # 3.2 partial link: bootstrap first, then the composed runtime object'
 p=sub(p,anchor,"    extra = cap10.prepare_package_inputs(globals(),b,objects,PIN[\"composition_dir\"] / PIN[\"composition_object_name\"])\n    if extra: unit_records[\"cap10_import_references\"] = extra\n"+anchor)
 # Diagnostics and compiled requirements keep each measured symbol kind.
 p=p.replace('"name": name, "binding": "GLOBAL", "symbol_type": "FUNC",','"name": name, "binding": "GLOBAL", "symbol_type": new_row["symbol_type"],')
 p=p.replace('"declared_symbol_types": ["FUNC"],','"declared_symbol_types": [new_row["symbol_type"]],')
 p=p.replace('{"name": name, "binding": "GLOBAL", "symbol_type": "FUNC"})','{"name": name, "binding": "GLOBAL", "symbol_type": new_row["symbol_type"]})')
 p=sub(p,'"evidence": new_row["evidence"]})','"evidence": new_row["evidence"]})') if False else p
 p=sub(p,'f"x.ExplicitType == SymType.Func) != {187 + added}"','f"x.ExplicitType == SymType.Func) != {187 + added - sum(r[\'symbol_type\']==\'OBJECT\' for r in CAP10_PROVIDER_ROWS.values())}"')
 anchor='    sub(\'"__eh_frame_hdr_start", "__eh_frame_hdr_end" };\','
 p=sub(p,anchor,"    sub('x.ExplicitType == SymType.Object) != 4',\n        f\"x.ExplicitType == SymType.Object) != {4 + sum(r['symbol_type']=='OBJECT' for r in CAP10_PROVIDER_ROWS.values())}\",1,\n        'exact measured OBJECT extension count')\n"+anchor)
 p=sub(p,'lines += [f".globl {name}", f".type {name},@function", f".quad {name}"]',"kind = 'object' if CAP10_PROVIDER_ROWS.get(name,{}).get('symbol_type')=='OBJECT' else 'function'\n        lines += [f\".globl {name}\", f\".type {name},@{kind}\", f\".quad {name}\"]")
 p=sub(p,'f"{donor_sonames[v].removesuffix(\'.prx\')} "',"f\"{(CAP10_PROVIDER_ROWS[n]['ImportSymbol']['Soname'] if n in CAP10_PROVIDER_ROWS else donor_sonames[v]).removesuffix('.prx')} \"")
 (RT/'cap10/package_prx_l19.py').write_text(p)
 controller=(L24/'package_combined.py').read_text();controller=sub(controller,'from extended_sections import array_census','from extended_sections import array_census\nimport cap10_integration as cap10')
 anchor="        raise ValueError('generated owned relocation check does not match actual shim order')"
 controller=sub(controller,anchor,anchor+"\n    cap10.install_package(m,inputs)\n    generated=admission.generate(list(m.ALL_EXTENDED_IMPORTS))")
 controller=sub(controller,"(stage/'native_thread_import_check.c').write_bytes(pin(inputs['import_check']).read_bytes())","(stage/'native_thread_import_check.c').write_text(generated['source'])")
 controller=sub(controller,"            before=array_census(combined_blob);after=array_census(packaged_blob)","            constructor_contract=cap10.verify_constructor_inputs(packaging,staged_runtime,array_census)\n            write(out/'CAP10-CONSTRUCTOR-CONTRACT.json',constructor_contract)\n            before=array_census(combined_blob);after=array_census(packaged_blob)")
 controller=sub(controller,"            if exact['count']!=207:raise ValueError('expected exactly original 206 plus one declared getter')","            if exact['count']!=m.PIN['cap10_expected_final_count']:raise ValueError('cap10 exact final import count drift')")
 (RT/'cap10/package_combined.py').write_text(controller)
 # l26 package driver uses staged exact controller/helper and manifest.
 p=(RT/'l26_packaging_driver.py').read_text();p=sub(p,"L24PKG=", "import cap10_integration as cap10\nCAP10=cap10.load(HERE)\nL24PKG=",1)
 p=sub(p,"    return m\n\ndef prepare_variant", "    return cap10.package_manifest(m,bdir,variant,CAP10,execute)\n\ndef prepare_variant")
 p=p.replace("(L24PKG/'package_combined.py').read_text()","Path(CAP10['package_controller']['path']).read_text()")
 p=sub(p,"sha(pkg/name)==sha(L24PKG/name)","sha(pkg/name)==sha(CAP10['package_script']['path'] if name=='package_prx_l19.py' else L24PKG/name)")
 p=sub(p,"src=L24PKG/name;dst=pkg/name;shutil.copyfile(src,dst)","src=Path(CAP10['package_script']['path']) if name=='package_prx_l19.py' else L24PKG/name;dst=pkg/name;shutil.copyfile(src,dst)")
 p=sub(p,"'path':str(L24PKG/n),'sha256':sha(L24PKG/n)","'path':str(CAP10['package_script']['path'] if n=='package_prx_l19.py' else L24PKG/n),'sha256':sha(CAP10['package_script']['path'] if n=='package_prx_l19.py' else L24PKG/n)")
 p=sub(p,"'source':{'path':str(L24PKG/'package_combined.py'),'sha256':sha(L24PKG/'package_combined.py')}","'source':CAP10['package_controller']")
 p=sub(p,"        shutil.copyfile(L24PKG/'inputs/NuGet.Config',pkg/'inputs/NuGet.Config')","        shutil.copyfile(L24PKG/'inputs/NuGet.Config',pkg/'inputs/NuGet.Config')\n        shutil.copyfile(CAP10['package_helper']['path'],pkg/'cap10_integration.py')\n        write(pkg/'CAP10-RUNTIME-ASSETS.json',cap10.stage_assets(pkg,CAP10))")
 # Reuse admission must also cover new helper and both packaged runtime assets.
 p=sub(p,"        record('P1-package-dir-reuse-verified',ok,{'path':str(pkg)})","        ok=ok and (pkg/'cap10_integration.py').exists() and sha(pkg/'cap10_integration.py')==CAP10['package_helper']['sha256']\n        cap10.stage_assets(pkg,CAP10)\n        record('P1-package-dir-reuse-verified',ok,{'path':str(pkg)})")
 p=sub(p,"        p7=ids['status'].startswith('PASS') and ids['count']==207","        contract=cap10.require_package_contract(pinrow(out/'CAP10-PACKAGE-CONTRACT.json'))\n        constructor=json.loads((out/'CAP10-CONSTRUCTOR-CONTRACT.json').read_text())\n        counts=cap10.final_package_counts(ids,arrays,contract,constructor)\n        p7=ids['status'].startswith('PASS') and ids['count']==counts['imports']")
 p=sub(p,"init==240 and fini==2","init==counts['init'] and fini==counts['fini']")
 p=sub(p,"'imports_unchanged':True", "'imports_unchanged':False,'cap10_integration':CAP10['manifest'],'runtime_assets':pinrow(pkg/'CAP10-RUNTIME-ASSETS.json')")
 p=sub(p,"len(names_a)==207","len(names_a)==a['exact_import_count']==b['exact_import_count']")
 p=sub(p,"if names_a!=names_b:raise RuntimeError('variant import name sets differ: '+repr(names_a^names_b))","if names_a!=names_b or len(names_a)!=a['exact_import_count'] or len(names_b)!=b['exact_import_count']:raise RuntimeError('variant import name sets/counts differ from exact per-variant contracts: '+repr(names_a^names_b))")
 p=sub(p,"'constructor_arrays_each':{'.init_array':238,'.fini_array':2}","'constructor_arrays_each':a['constructor_arrays']")
 p=p.replace("'exact_import_identities':207","'exact_import_identities':'COMPUTED_FROM_ACTUAL_INPUTS'").replace("'init_array_slots':240","'init_array_slots':'EXACT_INPUT_SUM'").replace("'fini_array_slots':2","'fini_array_slots':'EXACT_INPUT_SUM'")
 p=p.replace('P7 exactly 207 exact ELF import identities','P7 exact ELF identities/types equal computed actual-input contract').replace('P8 constructor arrays preserved 238/2','P8 constructor arrays equal exact actual-input sum').replace('P11 variant import name sets equal (207)','P11 variant import name sets/counts equal their exact contracts')
 (RT/'l26_packaging_driver.py').write_text(p)
 # Preserve every unrelated Claude row/metadata byte semantically; update
 # only the candidate hash of the existing program replacement row.
 changes=RT/'typescan-r4/CHANGES.json';d=json.loads(changes.read_text());rows=[x for x in d['files'] if Path(x['candidate']).name=='UE4SSProgram.cpp'];assert len(rows)==1
 rows[0]['candidate_sha256']=callback['composed']['sha256'];write(changes,d)
 natives=[c10.pin(p) for p in sorted((RT/'cap10/native').glob('*')) if p.is_file()]
 package_script=c10.pin(RT/'cap10/package_prx_l19.py');package_controller=c10.pin(RT/'cap10/package_combined.py')
 inputs={'version':1,'owner_runtime_pair':{'owner':rel(c10.pin(RT/'cap10/native/MonoOwner.cpp')),'header':rel(c10.pin(RT/'cap10/native/MonoOwner.hpp')),'runtime':rel(artifacts[1])},'root':roots[0],'prefix_source':entry,'units':{'cap10-mono-owner':{'source_pin':rel(c10.pin(RT/'cap10/native/MonoOwner.cpp')),'template':'program','note':'genuine target Mono owner, always fresh'},'cap10-invocation-boundary':{'source_pin':rel(c10.pin(RT/'cap10/native/InvocationBoundary.cpp')),'template':'program','note':'managed invoke boundary, always fresh'}},
  'include_dirs':['cap10/native','cap10/mono-headers','cap10/mono-headers/mono/eglib'],'header_pins':[rel(x) for x in headers+natives if Path(x['path']).suffix=='.h' or Path(x['path']).suffix=='.hpp'],
  'artifacts':[rel(x) for x in artifacts],'base_fixed_inputs':link['fixed_inputs'],'raw_symbols':sorted(raw),'runtime_assets':[rel(x) for x in assets],
  'provider_report':rel(c10.pin(RT/'cap10/PROVIDER-READINESS.json')),'package_helper':rel(helper),'package_script':rel(package_script),'package_controller':rel(package_controller),
  'providers':providers,'sdk_exports':sdkexports,'sdk_captures':{name:rel(row) for name,row in captures.items()},'sdk_evidence':sdk_evidence,'baseline_contract':c10.pin(L24/'pass-1/supplier-contract.json'),
  'pinned_inputs':[rel(x) for x in headers+natives+artifacts+assets+[probe]]+[entry]+link['fixed_inputs']+providers+[c10.pin(L24/'pass-1/supplier-contract.json')]}
 write(RT/'cap10/INTEGRATION-INPUTS.json',inputs)
 for f in [RT/'build_variants.py',RT/'l26_packaging_driver.py',RT/'cap10/package_prx_l19.py',RT/'cap10/package_combined.py',RT/'cap10_integration.py']:compile(f.read_text(),str(f),'exec')
 write(OWN/'evidence/STAGE-INPUTS.json',provenance)
 print(json.dumps({'headers':len(headers),'raw_symbols':len(raw),'component_platform_candidates':len(platform_candidates),'component_owned_requirements':len(owned_component_requirements),'admitted_new_rows':len(providerrows),'gaps':[x['name'] for x in gaps],'expected_final_count':report['expected_final_count'],'stage':str(STAGE)}))
if __name__=='__main__':main()
