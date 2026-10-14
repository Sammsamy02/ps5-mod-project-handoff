import sys
sys.dont_write_bytecode=True
import unittest,importlib.util,hashlib
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
class ProductionSource(unittest.TestCase):
 def read(self,name):
  p=BASE/name;self.assertTrue(p.exists(),'missing actual source: '+name);return p.read_text()
 def test_owner_real_init_identity_resolver_cleanup(self):
  s=self.read('native/MonoOwner.cpp')
  for word in ['mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP)','mono_jit_init_version','mono_image_open_from_data_with_name','mono_assembly_load_from_full','mono_assembly_get_image(assembly) != image','mono_dllmap_insert','mono_dl_fallback_register','mono_dl_fallback_unregister','mono_jit_cleanup','ProbeImage::bytes','invoke_boundaries']:
   self.assertIn(word,s)
 def test_genuine_signature_validation(self):
  s=self.read('native/InvocationBoundary.cpp')
  for word in ['MONO_METHOD_ATTR_STATIC','mono_signature_is_instance','mono_signature_explicit_this','mono_signature_get_param_count','mono_signature_get_return_type','mono_type_is_byref','MONO_TYPE_I4','MONO_TYPE_VOID','mono_signature_get_params']:
   self.assertIn(word,s)
 def test_current_source_transform_and_inverse(self):
  self.read('tools/stage_current.py')
  spec=importlib.util.spec_from_file_location('current',BASE/'tools/stage_current.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  raw=m.SOURCE.read_bytes();new=m.transform(raw);self.assertTrue(new!=raw);self.assertTrue(m.inverse(new)==raw)
  self.assertIn(b'run_embedded_probe_once()',new)
  for bad in [b'',raw+b' ',new]:
   with self.assertRaises(ValueError):m.transform(bad)
 def test_strict_mono_route_and_unregister_patch_inverse(self):
  self.read('tools/stage_mono_loader.py')
  spec=importlib.util.spec_from_file_location('loader',BASE/'tools/stage_mono_loader.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  raw=m.SOURCE.read_bytes();new=m.transform(raw);self.assertTrue(m.inverse(new)==raw)
  self.assertIn(b'fallback_handlers = g_slist_remove',new)
  self.assertIn(b'modbridge-owned:',new)
  for bad in [b'',raw+b' ',new]:
   with self.assertRaises(ValueError):m.transform(bad)
 def test_embedded_real_artifact_identity(self):
  s=self.read('native/ProbeImage.hpp');dll=(BASE/'managed/Probe.dll').read_bytes()
  self.assertIn(hashlib.sha256(dll).hexdigest(),s)
  import re
  array=s.split('bytes[] = {')[1].split('};')[0]
  self.assertEqual(bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',array)),dll)
if __name__=='__main__':unittest.main()
