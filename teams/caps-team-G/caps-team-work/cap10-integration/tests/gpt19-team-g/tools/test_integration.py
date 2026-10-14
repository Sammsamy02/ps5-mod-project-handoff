import copy,hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
OWN=Path(__file__).resolve().parents[1]
MAIN=Path('/WORKSPACE/.codex/worktrees/translation-core/what')

class IntegrationTests(unittest.TestCase):
 def test_provider_identity_comes_from_actual_elf(self):
  self.assertTrue((OWN/'tools/cap10_integration.py').is_file(),'missing additive integration helper')
  spec=importlib.util.spec_from_file_location('cap10_integration',OWN/'tools/cap10_integration.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  contract=json.loads((MAIN/'outputs/2026-10-08-l24-package/pass-1/supplier-contract.json').read_text())
  for name in ('__cxa_atexit','__stdoutp','sceKernelOpen','scePthreadGetthreadid'):
   row=next(x for x in contract['contract']['imports'] if x['ImportSymbol']['Name']==name)
   evidence=next((x for x in row['evidence'] if x.get('provider_elf_bytes_verified')),None)
   if evidence is None: continue
   actual=m.provider_identity(name,m.pin(Path(evidence['source'])))
   self.assertEqual(actual['ImportSymbol'],row['ImportSymbol'])
   self.assertEqual(actual['symbol_type'],row['symbol_type'])



class IntegrationBase(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  spec=importlib.util.spec_from_file_location('cap10_integration',OWN/'tools/cap10_integration.py');cls.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(cls.m)

class ContractTests(IntegrationBase):
 def test_compile_paths_derive_from_pinned_active_root(self):
  self.assertTrue(hasattr(self.m,'compile_argv'),'compile integration missing')
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'entry.c';p.write_text('#define MODBRIDGE_MOD_ROOT "/data/modbridge/roots/PPSA33286"\n')
   c={'prefix_source':self.m.pin(p),'root':'/data/modbridge/roots/PPSA33286','include_dirs':[d],'units':{'cap10-mono-owner':{},'cap10-invocation-boundary':{}}}
   out=self.m.compile_argv(['clang++','-c','owner.cpp'],'cap10-mono-owner',c)
   self.assertIn('-DMODBRIDGE_CAP10_CORLIB_DIR="/data/modbridge/roots/PPSA33286/lib"',out)
   self.assertIn('-DMODBRIDGE_CAP10_CONFIG_DIR="/data/modbridge/roots/PPSA33286/etc"',out)
   self.assertEqual(self.m.compile_argv(['clang++'],'luamod',c),['clang++'])
   p.write_text(p.read_text()+'/* drift */')
   with self.assertRaisesRegex(ValueError,'drift'):self.m.compile_argv(['clang++'],'program',c)
 def test_link_preserves_fixed_inputs_with_exact_census_and_order(self):
  self.assertTrue(hasattr(self.m,'wire_link'),'link integration missing')
  with tempfile.TemporaryDirectory() as d:
   d=str(Path(d).resolve())
   paths={name:Path(d)/(name+'.o' if name!='mono' else 'mono.a') for name in ('base','owner','boundary','corlib','mono','fixed')}
   for k,p in paths.items():p.write_text(k)
   c={'units':{'cap10-mono-owner':{},'cap10-invocation-boundary':{}},'artifacts':[self.m.pin(paths['corlib']),self.m.pin(paths['mono'])],'base_fixed_inputs':[self.m.pin(paths['fixed'])]}
   units=[{'name':'base','object':str(paths['base'])},{'name':'cap10-mono-owner','object':str(paths['owner'])},{'name':'cap10-invocation-boundary','object':str(paths['boundary'])}]
   cmd=['ld','-r',str(paths['base']),str(paths['fixed']),'-o',str(Path(d)/'out.o')]
   out=self.m.wire_link(cmd,units,{'cap10-mono-owner':{},'cap10-invocation-boundary':{}},c)
   self.assertEqual(out[3:7],[str(paths[x]) for x in ('owner','boundary','corlib','mono')])
   self.m.link_census(out,units,c,6)
   with self.assertRaisesRegex(ValueError,'census'):self.m.link_census(out+[str(paths['mono'])],units,c,6)
   reordered=list(out);a=reordered.index(str(paths['mono']));b=reordered.index(str(paths['corlib']));reordered[a],reordered[b]=reordered[b],reordered[a]
   with self.assertRaisesRegex(ValueError,'order'):self.m.link_census(reordered,units,c,6)
   paths['mono'].write_text('drift')
   with self.assertRaisesRegex(ValueError,'drift'):self.m.wire_link(cmd,units,{'cap10-mono-owner':{},'cap10-invocation-boundary':{}},c)
 def test_raw_contract_rejects_added_missing_zero_and_applies_lua_delta(self):
  self.assertTrue(hasattr(self.m,'verify_raw'),'raw integration missing')
  c={'raw_symbols':[['U','fopen'],['U','setjmp'],['w','__dlopen']]}
  self.m.verify_raw('U fopen\nU setjmp\nw __dlopen\n',c)
  for text in ('','U fopen\n','U fopen\nU setjmp\nw __dlopen\nU invented\n'):
   with self.assertRaisesRegex(ValueError,'raw import'):self.m.verify_raw(text,c)
  self.m.verify_raw('U fopen\nw __dlopen\n',c,{'removed':[('U','setjmp')],'added':[]})
 def test_provider_missing_and_drift_fail(self):
  self.assertTrue(hasattr(self.m,'resolve_providers'),'provider gate missing')
  d=json.loads((MAIN/'outputs/2026-10-08-l24-package/pass-1/supplier-contract.json').read_text())
  libc=next(x for x in d['contract']['imports'] if x['ImportSymbol']['Name']=='__cxa_atexit')['evidence'][0]
  c={'providers':[{'path':libc['source'],'sha256':libc['sha256']}],'sdk_exports':{'puts':['libSceLibcInternal.prx'],'sceNetPoolCreate':['libSceNet.prx']}}
  rows,gaps=self.m.resolve_providers(['puts','sceNetPoolCreate'],c)
  self.assertEqual([x['ImportSymbol']['Name'] for x in rows],['puts']);self.assertEqual([x['name'] for x in gaps],['sceNetPoolCreate'])
  c['providers'][0]['sha256']='0'*64
  with self.assertRaisesRegex(ValueError,'drift'):self.m.resolve_providers(['puts'],c)
 def test_package_asset_copy_is_exact_and_missing_asset_fails(self):
  self.assertTrue(hasattr(self.m,'stage_assets'),'runtime asset integration missing')
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'corlib';p.write_bytes(b'real IL fixture')
   row=self.m.pin(p);row['destination']='lib/mono/4.5/mscorlib.dll'
   c={'runtime_assets':[row],'root':'/data/modbridge/roots/PPSA33286'}
   out=self.m.stage_assets(Path(d)/'package',c)
   self.assertEqual((Path(d)/'package/runtime-assets/lib/mono/4.5/mscorlib.dll').read_bytes(),p.read_bytes());self.assertEqual(out['root'],c['root'])
   p.unlink()
   with self.assertRaisesRegex(ValueError,'missing'):self.m.stage_assets(Path(d)/'other',c)

class StagedDriverTests(unittest.TestCase):
 def test_staged_drivers_call_real_integration_helpers(self):
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime'
  self.assertIn('cap10.wire_link', (rt/'build_variants.py').read_text())
  self.assertIn('cap10.verify_raw', (rt/'build_variants.py').read_text())
  self.assertIn('cap10.package_manifest', (rt/'l26_packaging_driver.py').read_text())
  self.assertIn('cap10.install_package', (rt/'cap10/package_combined.py').read_text())

class ActualInputResolutionTests(IntegrationBase):
 def test_predict_actual_fragment_archive_resolution(self):
  self.assertTrue(hasattr(self.m,'link_import_contract'),'actual-input resolver missing')
  receipt=json.loads((OWN/'evidence/PARTIAL-LINK.json').read_text())
  predicted=self.m.link_import_contract(receipt['argv'])
  expected=self.m.nm_undefined((OWN/'evidence/undefined.stdout').read_text())
  self.assertEqual({tuple(x) for x in predicted['raw_symbols']},expected)
 def test_predict_actual_base_archive_resolution(self):
  self.assertTrue(hasattr(self.m,'link_import_contract'),'actual-input resolver missing')
  base=MAIN/'outputs/2026-10-08-l25-build-variants/l25-A/pass1'
  cmd=json.loads((base/'LINK-COMMAND.json').read_text())['argv']
  predicted=self.m.link_import_contract(cmd)
  self.assertEqual({tuple(x) for x in predicted['raw_symbols']},self.m.nm_undefined((base/'undefined.stdout').read_text()))

def extracted(path,names,env):
 import ast
 tree=ast.parse(Path(path).read_text());defs=[n for n in tree.body if isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef)) and n.name in names]
 if {n.name for n in defs}!=set(names):raise AssertionError('missing actual driver definitions')
 exec(compile(ast.Module(body=defs,type_ignores=[]),str(path),'exec'),env)
 return env

class RealDriverDefinitionTests(IntegrationBase):
 def test_actual_make_new_units_clones_real_program_argv_with_pins(self):
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime';c=self.m.load(rt)
  base=json.loads((MAIN/'outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json').read_text())
  env={'copy':copy,'sha':lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest(),'NEW_UNITS':c['units']}
  extracted(rt/'build_variants.py',{'make_new_units'},env)
  made=env['make_new_units'](base['selected_units'],1,{},Path('/unused'))
  self.assertEqual(len(made),2);self.assertEqual([x['name'] for x in made],list(c['units']))
  for u in made:
   spec=c['units'][u['name']];self.assertEqual(u['source_sha256'],spec['source_sha256']);self.assertEqual(u['argv'].count(str(spec['source'])),1)
   argv=self.m.compile_argv(u['argv'],u['name'],c);self.assertIn('-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1',argv)
   self.assertNotIn('MODBRIDGE_CAP10_MONO_HOST_VALIDATION',' '.join(argv))
  env['NEW_UNITS']=copy.deepcopy(c['units']);env['NEW_UNITS']['cap10-mono-owner']['source_sha256']='0'*64
  with self.assertRaisesRegex(RuntimeError,'pin drift'):env['make_new_units'](base['selected_units'],1,{},Path('/unused'))
 def test_actual_supplier_typed_function_preserves_object_kind(self):
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime'
  env={'ALL_EXTENDED_IMPORTS':{'puts':'puts','environ':'environ'},'CAP10_PROVIDER_ROWS':{'puts':{'symbol_type':'FUNC'},'environ':{'symbol_type':'OBJECT'}}}
  extracted(rt/'cap10/package_prx_l19.py',{'supplier_import_shim'},env)
  actual=env['supplier_import_shim']();self.assertIn('.type puts,@function',actual);self.assertIn('.type environ,@object',actual)
 def test_actual_legacy_combined_definitions_accounted_before_platform_admission(self):
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime';c=self.m.load(rt)
  package=json.loads((MAIN/'outputs/2026-10-08-l24-package/pass-1/PACKAGE-RESULT.json').read_text())
  baseline=self.m.checked(package['combined_object'])
  boundaries=['__init_array_start','__init_array_end','__fini_array_start','__fini_array_end','__dso_handle','__eh_frame_start','__eh_frame_end','__eh_frame_hdr_start','__eh_frame_hdr_end','_DYNAMIC']
  report=self.m.package_input_contract([],baseline,c,boundaries,['__dladdr','__dlclose','__dlerror','__dlopen','__dlsym','kernel_mprotect'])
  self.assertEqual(report['gaps'],[]);self.assertEqual(report['additional_rows'],[]);self.assertEqual(report['expected_final_count'],207)
  self.assertFalse(any(x.startswith('bridge_') or x=='diagnostic_event' for x in report['new_strong_requirements']))
 def test_actual_fragment_and_bootstrap_diagnostic_retains_missing_owned_and_net(self):
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime';c=self.m.load(rt)
  package=json.loads((MAIN/'outputs/2026-10-08-l24-package/pass-1/PACKAGE-RESULT.json').read_text())
  objects=[self.m.checked({'path':r['object'],'sha256':r['object_sha256']}) for r in package['bootstrap_units'].values()]
  # The existing BASE runtime supplies the genuine NativeModSource definition.
  # This diagnostic intentionally omits the new callback, so its five owned
  # observation/export symbols remain visible as failures, never platform rows.
  objects.append(MAIN/'outputs/2026-10-08-l25-build-variants/l25-A/pass1/runtime-l25-A.o')
  bounds=['__init_array_start','__init_array_end','__fini_array_start','__fini_array_end','__dso_handle','__eh_frame_start','__eh_frame_end','__eh_frame_hdr_start','__eh_frame_hdr_end','_DYNAMIC']
  report=self.m.package_input_contract(objects,OWN/'evidence/cap10-with-base-fixed.o',c,bounds,['__dladdr','__dlclose','__dlerror','__dlopen','__dlsym','kernel_mprotect'])
  gaps={x['name'] for x in report['gaps']}
  self.assertIn('sceNetPoolCreate',gaps);self.assertIn('modbridge_cap10_runtime_probe',gaps)
  self.assertFalse(any(x.startswith('bridge_') or x=='diagnostic_event' for x in report['new_strong_requirements']))
  self.assertTrue(any(x['symbol_type']=='OBJECT' for x in report['additional_rows']))
  with self.assertRaisesRegex(ValueError,'identity gate unresolved'):self.m.require_package_contract(report)

class FinalizerDefinitionTests(IntegrationBase):
 def fixture(self,root):
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime';c=self.m.load(rt)
  pkg=root/'pkg';build=root/'build';pkg.mkdir();(build/'pass1').mkdir(parents=True)
  def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d)+'\n')
  parent=MAIN/'outputs/2026-10-08-l24-package/pass-1'
  contract=json.loads((parent/'supplier-contract.json').read_text())
  rows=contract['contract']['imports'];expected={'gaps':[],'final_rows':rows,'additional_rows':[],'expected_final_count':len(rows)}
  for passno in (1,2):
   out=pkg/f'pass-{passno}';out.mkdir()
   for name in ('STATIC-CHECKS.json','EXACT-IMPORT-IDENTITIES.json','CONSTRUCTOR-ARRAY-CENSUS.json'):
    (out/name).write_bytes((parent/name).read_bytes())
   arrays=json.loads((out/'CONSTRUCTOR-ARRAY-CENSUS.json').read_text())
   write(out/'CAP10-CONSTRUCTOR-CONTRACT.json',{'status':'PASS_EXACT_INPUT_ARRAY_SUM','expected':{n:arrays['combined'][n]['slots'] for n in ('.init_array','.fini_array')}})
   write(out/'CAP10-PACKAGE-CONTRACT.json',expected);write(out/'BUILD-RECEIPT.json',{'status':'MAC_PACKAGE_CHECKS_PASS'})
   # Temporary bytes exercise finalizer receipt/twin behavior only; no
   # compiler, packager, real PRX generation or native execution occurs.
   (out/'modbridge_supplier_composition.prx').write_bytes(b'fixture package bytes')
  write(build/'VARIANT-MARKER.json',{'label':'fixture','define':'fixture','delete_listener_registration':'fixture'})
  write(build/'TWIN-RECEIPT.json',{'fixture':True});(build/'pass1/runtime.o').write_bytes(b'fixture runtime');write(pkg/'CAP10-RUNTIME-ASSETS.json',{'fixture':True})
  env={'Path':Path,'json':json,'sha':lambda p:self.m.pin(p)['sha256'],'pinrow':self.m.pin,'write':write,'cap10':self.m,'CAP10':c,'L24PKG':MAIN/'outputs/2026-10-08-l24-package','TOKEN':'fixture','UNSAFE_NOTE':'fixture',
   'emit_symbols_sidecar':lambda *args:{'fixture':'unrelated sidecar bypassed'}}
  extracted(rt/'l26_packaging_driver.py',{'finalize_variant'},env)
  return env,{'pkg':pkg,'build':build,'key':'A','unsafe':False,'artifact':'runtime.o'},write
 def test_actual_finalizer_positive_exact_counts(self):
  with tempfile.TemporaryDirectory() as d:
   env,v,write=self.fixture(Path(d));gates=[]
   final=env['finalize_variant'](v,lambda *x:gates.append(x),[])
   self.assertEqual(final['exact_import_count'],207);self.assertTrue((v['pkg']/'FINAL-RECEIPT-VARIANT-A.json').exists());self.assertTrue(all(x[1] for x in gates))
 def test_actual_finalizer_rejects_zero_missing_duplicate_and_constructor_drift(self):
  for fault in ('zero','missing','duplicate','constructor','static','twin'):
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as d:
    env,v,write=self.fixture(Path(d));out=v['pkg']/'pass-1'
    if fault in ('zero','missing','duplicate'):
     p=out/'EXACT-IMPORT-IDENTITIES.json';ids=json.loads(p.read_text())
     if fault=='zero':ids['count']=0
     elif fault=='missing':ids['imports'].pop()
     else:ids['imports'].append(ids['imports'][0])
     write(p,ids)
    elif fault=='constructor':
     p=out/'CONSTRUCTOR-ARRAY-CENSUS.json';x=json.loads(p.read_text());x['combined']['.init_array']['slots']+=1;write(p,x)
    elif fault=='static':
     p=out/'STATIC-CHECKS.json';x=json.loads(p.read_text());next(iter(x['checks'].values()))['pass']=False;write(p,x)
    else:(out/'modbridge_supplier_composition.prx').write_bytes(b'different fixture')
    with self.assertRaises((ValueError,RuntimeError)):env['finalize_variant'](v,lambda *x:None,[])
    self.assertFalse((v['pkg']/'FINAL-RECEIPT-VARIANT-A.json').exists())

class ModuleSyntaxTests(unittest.TestCase):
 def test_all_staged_python_compiles_without_running(self):
  root=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime'
  for path in root.rglob('*.py'):
   with self.subTest(path=path.name):compile(path.read_text(),str(path),'exec')

class ExactTypeTests(IntegrationBase):
 def test_final_import_type_must_match_saved_provider_kind(self):
  parent=MAIN/'outputs/2026-10-08-l24-package/pass-1'
  ids=json.loads((parent/'EXACT-IMPORT-IDENTITIES.json').read_text());arrays=json.loads((parent/'CONSTRUCTOR-ARRAY-CENSUS.json').read_text())
  rows=json.loads((parent/'supplier-contract.json').read_text())['contract']['imports']
  contract={'final_rows':rows};constructor={'status':'PASS_EXACT_INPUT_ARRAY_SUM','expected':{n:arrays['combined'][n]['slots'] for n in ('.init_array','.fini_array')}}
  self.m.final_package_counts(ids,arrays,contract,constructor)
  row=next(x for x in ids['imports'] if x['name']=='__stdoutp');row['type']=2
  with self.assertRaisesRegex(ValueError,'kind'):self.m.final_package_counts(ids,arrays,contract,constructor)
 def test_actual_constructor_input_sum_matches_saved_combined(self):
  env={'struct':__import__('struct')};extracted(MAIN/'outputs/2026-10-08-l24-package/extended_sections.py',{'sections','array_census'},env)
  parent=MAIN/'outputs/2026-10-08-l24-package/pass-1';p=json.loads((parent/'PACKAGE-RESULT.json').read_text())
  manifest=json.loads((MAIN/'outputs/2026-10-08-l24-package/BUILD-INPUTS.json').read_text())
  receipt=self.m.verify_constructor_inputs(p,self.m.checked(manifest['runtime_object']),env['array_census'])
  actual=json.loads((parent/'CONSTRUCTOR-ARRAY-CENSUS.json').read_text())
  self.assertEqual(receipt['expected'],{n:actual['combined'][n]['slots'] for n in ('.init_array','.fini_array')})

class PackagePlanDefinitionTests(IntegrationBase):
 def test_actual_prepare_plan_declares_the_actual_adapted_sources(self):
  import ast
  rt=OWN/'derived/target-ready/outputs/2026-10-08-l26-runtime';c=self.m.load(rt);source=rt/'l26_packaging_driver.py'
  tree=ast.parse(source.read_text());copy_files=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(x,ast.Name) and x.id=='COPY_FILES' for x in n.targets))
  env={'Path':Path,'json':json,'L24PKG':MAIN/'outputs/2026-10-08-l24-package','CAP10':c,'COPY_FILES':copy_files,'sha':lambda p:self.m.pin(p)['sha256'],'FROZEN_NOTE_LINE':"    b.note('l24 retained93 profile (unchanged from l23); typescan-r3 refusal-rework change; l23-admitted 207 import identities; local packaging only.')",'derive_manifest':lambda *args:{'fixture':'manifest source routing tested separately'}}
  extracted(source,{'prepare_variant'},env)
  with tempfile.TemporaryDirectory() as d:
   steps=[];env['prepare_variant']({'pkg':Path(d)/'pkg','key':'A','note':'fixture'},lambda *x:None,steps,False)
   copy_step=next(x for x in steps if x['step']=='copy-frozen-packaging-scripts');script=next(x for x in copy_step['files'] if x['dest'].endswith('/package_prx_l19.py'))
   self.assertEqual(script['source']['sha256'],c['package_script']['sha256'])
   controller=next(x for x in steps if x['step']=='write-adapted-controller');self.assertEqual(controller['source']['sha256'],c['package_controller']['sha256'])
