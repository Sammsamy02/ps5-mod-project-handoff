import ast,copy,hashlib,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent.parent

def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path)
 result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
 return result

m=module('bi',H/'tools/build_integration.py')
d=module('selection',H/'tools/dry_run_build_selection.py')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

class Integration(unittest.TestCase):
 def test_dry_run_accepts_explicit_stage_and_fresh_receipt(self):
  result=subprocess.run([sys.executable,str(H/'tools/dry_run_build_selection.py'),'--help'],capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stderr)
  for flag in ('--stage','--out','--audit-out'):self.assertIn(flag,result.stdout)

 def test_unique_driver_anchor(self):
  with self.assertRaises(ValueError):m.runtime_driver('NEW_UNITS={}\nNEW_UNITS={}\n','0'*64)

 def test_empty_rows_refuse(self):
  with self.assertRaises(ValueError):m.refresh_rows({'files':[]},{'x':b'x'})

 def test_actual_driver_input_drift_refuses(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   row=json.loads((H/'evidence/build-integration-inputs.json').read_text())['files'][0]
   p=Path(t)/row['relative_path'];p.parent.mkdir(parents=True);p.write_text('drift')
   with self.assertRaisesRegex(ValueError,'exact build-driver/metadata input drift'):m.generate(Path(t))

 def assert_patch_roundtrip(self,originals,derived,patch_bytes):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   root=Path(t)
   for rel,b in originals.items():
    path=root/rel;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b)
   for rel in derived.keys()-originals.keys():self.assertFalse((root/rel).exists())
   applied=subprocess.run(['git','apply','-'],cwd=root,input=patch_bytes,capture_output=True)
   self.assertEqual(applied.returncode,0,applied.stderr.decode())
   for rel,b in derived.items():self.assertEqual((root/rel).read_bytes(),b,rel)
   reversed_=subprocess.run(['git','apply','--reverse','-'],cwd=root,input=patch_bytes,capture_output=True)
   self.assertEqual(reversed_.returncode,0,reversed_.stderr.decode())
   for rel,b in originals.items():self.assertEqual((root/rel).read_bytes(),b,rel)
   for rel in derived.keys()-originals.keys():self.assertFalse((root/rel).exists())

 def test_generated_patch_preserves_missing_final_newlines_and_additions(self):
  originals={'normal.txt':b'before\n','no-newline.json':b'{"before":1}'}
  derived={'normal.txt':b'after\n','no-newline.json':b'{"after":2}\n','added.hpp':b'new header without newline'}
  self.assert_patch_roundtrip(originals,derived,m.integration_patch(derived,originals).encode())

 def test_actual_stage_patch_roundtrip_all_fifteen_files(self):
  tree,receipt,files,_=d.load_stage()
  originals={row['path']:d.read(d.ROOT/row['path']) for row in receipt['originals']}
  for row in receipt['originals']:
   self.assertEqual(hashlib.sha256(originals[row['path']]).hexdigest(),row['sha256'],row['path'])
  self.assertEqual(len(originals),8);self.assertEqual(len(files),15)
  self.assert_patch_roundtrip(originals,files,(tree/'integration.patch').read_bytes())

class StageTooling(unittest.TestCase):
 def test_default_verifies_authoritative_stage(self):
  tree,receipt,files,identity=d.load_stage()
  self.assertEqual(tree,H/'derived/review-final')
  self.assertEqual(len(files),15)
  self.assertEqual(identity['path'],str(tree))
  self.assertEqual(identity['integration_receipt_sha256'],sha(tree/'INTEGRATION.json'))
  self.assertEqual(identity['integration_patch_sha256'],sha(tree/'integration.patch'))

 def test_explicit_owned_stage_identity_and_non_driver_hash_refusal(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   root=Path(t);stage=root/'derived/chosen';stage.mkdir(parents=True)
   payload=b'header';p=stage/'header.hpp';p.write_bytes(payload)
   receipt={'files':[{'path':'header.hpp','bytes':6,'sha256':hashlib.sha256(payload).hexdigest()}]}
   (stage/'INTEGRATION.json').write_text(json.dumps(receipt));(stage/'integration.patch').write_text('selected patch\n')
   with patch.object(d,'H',root):
    tree,_,files,identity=d.load_stage(stage)
    self.assertEqual(tree,stage);self.assertEqual(files,{'header.hpp':payload})
    self.assertEqual(identity['path'],str(stage))
    p.write_bytes(b'broken')
    with self.assertRaisesRegex(ValueError,'staged file SHA/size drift'):d.load_stage(stage)
    with self.assertRaisesRegex(ValueError,'owned derived stage required'):d.load_stage(root)

 def test_receipts_are_owned_and_exclusive(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   root=Path(t);(root/'evidence').mkdir();out=root/'evidence/fresh.json'
   with patch.object(d,'H',root):
    d.write_receipt(out,{'stage':'chosen'})
    with self.assertRaisesRegex(ValueError,'fresh owned evidence'):d.write_receipt(out,{'stage':'overwritten'})
    with self.assertRaisesRegex(ValueError,'fresh owned evidence'):d.write_receipt(root/'outside.json',{})
   self.assertEqual(json.loads(out.read_text()),{'stage':'chosen'})

 def test_live_original_drift_is_reported_without_rewrite(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   root=Path(t);path=root/'input.json';path.write_bytes(b'old')
   receipt={'originals':[{'path':'input.json','bytes':3,'sha256':sha(path)}]}
   with patch.object(d,'ROOT',root):
    self.assertTrue(d.audit_originals(receipt)['all_match'])
    path.write_bytes(b'changed')
    audit=d.audit_originals(receipt)
   self.assertFalse(audit['all_match']);self.assertEqual(audit['files'][0]['actual']['bytes'],7)
   self.assertEqual(path.read_bytes(),b'changed')

class ActualDriverDefinitions(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tree,cls.receipt,cls.files,_=d.load_stage()
  cls.runtime=cls.files[d.PREFIX+'build_variants.py']
  cls.packaging=cls.files[d.PREFIX+'l26_packaging_driver.py']

 def runtime_env(self):
  def staged_sha(path):
   rel=str(Path(path).relative_to(d.ROOT))
   return hashlib.sha256(self.files[rel] if rel in self.files else d.read(path)).hexdigest()
  env={'HERE':d.RT,'Path':Path,'copy':copy,'sha':staged_sha}
  exec(compile(d.selected(self.runtime,{'NEW_UNITS','make_new_units','compute_selection'}),'<actual staged runtime definitions>','exec'),env)
  return env

 def test_new_unit_uses_initializer_template_without_mutating_it(self):
  templates=[{'name':'unreal-unrealinitializer','source':'initializer.cpp','argv':['compiler','-Iinitializer-includes','-c','initializer.cpp','-o','initializer.o'],'object':'initializer.o','dependency':'initializer.d'},
             {'name':'luamod','source':'lua.cpp','argv':['compiler','-Ilua-includes','-c','lua.cpp','-o','lua.o'],'object':'lua.o','dependency':'lua.d'}]
  before=copy.deepcopy(templates);env=self.runtime_env()
  units=env['make_new_units'](templates,1,{},None)
  self.assertEqual([u['name'] for u in units],['pakmount','lua-lfb','buildstamp','tcselftest','lua-pathshim'])
  pak=units[0];source=str(d.RT/'typescan-r4/candidate/src/RuntimePakMount.cpp')
  self.assertEqual(pak['argv'],['compiler','-Iinitializer-includes','-c',source,'-o','initializer.o'])
  self.assertEqual(pak['object'],'initializer.o');self.assertEqual(pak['dependency'],'initializer.d')
  self.assertEqual(pak['source_sha256'],'a8894176856f67ad13eacafb910bfcbd214658f02bb55a6709db9785a5747945')
  self.assertEqual(templates,before)

 def test_new_unit_source_hash_refused_before_template_use(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   root=Path(t);source=root/'typescan-r4/candidate/src/RuntimePakMount.cpp';source.parent.mkdir(parents=True);source.write_text('drift')
   env={'HERE':root,'Path':Path,'copy':copy,'sha':sha}
   exec(compile(d.selected(self.runtime,{'NEW_UNITS','make_new_units'}),'<actual staged runtime definitions>','exec'),env)
   with self.assertRaisesRegex(RuntimeError,'registered NEW_UNIT source pin drift'):env['make_new_units']([],1,{},None)

 def test_incremental_refused_before_reading_build_artifacts(self):
  with self.assertRaisesRegex(RuntimeError,'full header-consumer rebuild'):
   self.runtime_env()['compute_selection'](None,incremental=True)

 def profile_fixture(self,t):
  root=Path(t);path=root/'profile/units/PakMountCodeProfile.hpp';path.parent.mkdir(parents=True)
  path.write_bytes(self.files[d.PREFIX+'profile/units/PakMountCodeProfile.hpp'])
  changes=json.loads(self.files[d.PREFIX+'profile/CHANGES.json'])
  original=str(d.RT/'profile/units/PakMountCodeProfile.hpp')
  changes['added_headers']=[str(path) if p==original else p for p in changes['added_headers']]
  changes['added_header_hashes'][str(path)]=changes['added_header_hashes'].pop(original)
  env={'Path':Path,'L26RUN':root,'sha':sha}
  exec(compile(d.selected(self.packaging,{'pakmount_profile_overlay'}),'<actual staged packaging helper>','exec'),env)
  return path,changes,env

 def test_profile_overlay_admits_exact_staged_header(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   path,changes,env=self.profile_fixture(t)
   self.assertEqual(env['pakmount_profile_overlay'](changes),{'path':str(path),'bytes':1385,'sha256':'78d137e47d16f6c2be21abe33920db0433452465ef342f45f42ca2c29ce44aee','destination':'lifecycle/PakMountCodeProfile.hpp'})

 def test_profile_overlay_refuses_missing_admission(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   _,changes,env=self.profile_fixture(t);changes['added_headers']=[]
   with self.assertRaisesRegex(RuntimeError,'missing required profile header admission'):env['pakmount_profile_overlay'](changes)

 def test_profile_overlay_refuses_missing_file(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   path,changes,env=self.profile_fixture(t);path.unlink()
   with self.assertRaisesRegex(RuntimeError,'header pin drift'):env['pakmount_profile_overlay'](changes)

 def test_profile_overlay_refuses_wrong_hash(self):
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   path,changes,env=self.profile_fixture(t);path.write_text('wrong bytes')
   with self.assertRaisesRegex(RuntimeError,'header pin drift'):env['pakmount_profile_overlay'](changes)

 def test_actual_manifest_overlay_block_appends_once_and_refuses_duplicate(self):
  # Execute the unchanged contiguous integration block from derive_manifest.
  # Full derive_manifest would inspect unrelated package/object artifacts.
  fn=next(n for n in ast.parse(self.packaging).body if isinstance(n,ast.FunctionDef) and n.name=='derive_manifest')
  start=next(i for i,n in enumerate(fn.body) if isinstance(n,ast.Assign) and any(isinstance(x,ast.Name) and x.id=='required_pak_profile' for x in n.targets))
  block=ast.fix_missing_locations(ast.Module(body=fn.body[start:start+4],type_ignores=[]))
  code=compile(block,'<actual manifest overlay integration block>','exec')
  with tempfile.TemporaryDirectory(prefix='REVIEW-',dir=H/'tests/tmp') as t:
   _,changes,env=self.profile_fixture(t);env.update(prof_changes=changes,overlays=[],m={})
   exec(code,env)
   self.assertEqual(len(env['m']['lineage_overlays']),1)
   self.assertEqual(env['m']['lineage_overlays'][0]['destination'],'lifecycle/PakMountCodeProfile.hpp')
   with self.assertRaisesRegex(RuntimeError,'duplicate PakMountCodeProfile overlay'):exec(code,env)
   self.assertEqual(len(env['m']['lineage_overlays']),1)

if __name__=='__main__':
 (H/'tests/tmp').mkdir(exist_ok=True)
 unittest.main()
