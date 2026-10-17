import importlib.util,unittest,hashlib
from pathlib import Path
H=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('stage_current',H/'tools/stage_current.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Stage(unittest.TestCase):
 def test_exact_and_inverse(self):
  source=b'one\ntwo\n';recipe={'bytes':len(source),'sha256':hashlib.sha256(source).hexdigest(),'derived_sha256':hashlib.sha256(b'one\nthree\n').hexdigest(),'hunks':[{'start':0,'old':'one\ntwo\n','new':'one\nthree\n'}]}
  self.assertEqual(m.transform(source,recipe),b'one\nthree\n');self.assertEqual(source,b'one\ntwo\n')
 def test_sha_drift(self):
  with self.assertRaises(ValueError):m.transform(b'changed',{'bytes':7,'sha256':'0'*64,'hunks':[]})
 def test_empty(self):
  with self.assertRaises(ValueError):m.transform(b'',{})
 def test_wrong_anchor(self):
  source=b'one\n';r={'bytes':4,'sha256':hashlib.sha256(source).hexdigest(),'derived_sha256':'0'*64,'hunks':[{'start':0,'old':'two\n','new':'x\n'}]}
  with self.assertRaises(ValueError):m.transform(source,r)
if __name__=='__main__':unittest.main()
