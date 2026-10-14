"""Gate controls: an asserted twin receipt cannot replace measured inputs."""
import copy,json,tempfile,unittest
from pathlib import Path
from prepare_inputs import record,validate_compositions
HERE=Path(__file__).resolve().parent
class PairTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(dir=HERE,prefix='pair-control-');self.base=Path(self.temp.name)
  self.objects=[]
  for n in (1,2):
   p=self.base/f'runtime{n}.o';p.write_bytes(b'qualified-pair');self.objects.append(p)
  self.runtime=record(self.objects[0]);self.paths=[]
  names=['unreal-unrealinitializer','native-thread-name-threadnameruntime']+[f'fixed{i}' for i in range(372)]
  for n,p in enumerate(self.objects,1):
   compiler=self.base/f'compile{n}.json';compiler.write_text(json.dumps({'status':'FRESH_COMBINED_TARGET_COMPILE_PASS','results':[{'name':x} for x in names[:2]]}))
   aux=self.base/f'aux{n}.json';aux.write_text('{}')
   obj=record(p);obj['size']=obj.pop('bytes')
   d={'status':'ACTUAL_FINAL_RELOCATABLE_LINK_PASS_NOT_PACKAGE_ADMISSION','pass':n,'selected_count':374,'full_link_inputs':391,'native_execution':False,'menu_success':False,'object':obj,'compile_receipt':record(compiler),'link_command':record(aux),'dependency_graph':record(aux),'import_delta':record(aux),'selected_units':[{'name':name,'source_sha256':'same-source','object_sha256':'same-object'} for name in names]}
   path=self.base/f'pass{n}.json';path.write_text(json.dumps(d));self.paths.append(path)
 def tearDown(self):self.temp.cleanup()
 def mutate(self,fn):
  d=json.loads(self.paths[1].read_text());fn(d);self.paths[1].write_text(json.dumps(d))
 def test_complete_actual_pair(self):self.assertEqual(len(validate_compositions(self.paths,self.runtime)),2)
 def test_same_artifact_is_not_second_link(self):
  self.mutate(lambda d:d.update(object=json.loads(self.paths[0].read_text())['object']))
  with self.assertRaisesRegex(ValueError,'same artifact'):validate_compositions(self.paths,self.runtime)
 def test_runtime_bytes_changed(self):
  self.objects[1].write_bytes(b'unqualified');self.mutate(lambda d:d.update(object={**record(self.objects[1]),'size':self.objects[1].stat().st_size}))
  with self.assertRaisesRegex(ValueError,'runtime pair differs'):validate_compositions(self.paths,self.runtime)
 def test_incomplete_composition(self):
  self.mutate(lambda d:d.update(full_link_inputs=390))
  with self.assertRaisesRegex(ValueError,'incomplete'):validate_compositions(self.paths,self.runtime)
 def test_changed_individual_object(self):
  self.mutate(lambda d:d['selected_units'][20].update(object_sha256='drift'))
  with self.assertRaisesRegex(ValueError,'individual object'):validate_compositions(self.paths,self.runtime)
 def test_native_claim_refused(self):
  self.mutate(lambda d:d.update(menu_success=True))
  with self.assertRaisesRegex(ValueError,'native claim'):validate_compositions(self.paths,self.runtime)
 def test_missing_fresh_initializer(self):
  compiler=json.loads(self.paths[1].read_text())['compile_receipt']['path'];Path(compiler).write_text(json.dumps({'status':'FRESH_COMBINED_TARGET_COMPILE_PASS','results':[{'name':'native-thread-name-threadnameruntime'}]}))
  self.mutate(lambda d:d.update(compile_receipt=record(compiler)))
  with self.assertRaisesRegex(ValueError,'freshly compile'):validate_compositions(self.paths,self.runtime)
if __name__=='__main__':unittest.main()
