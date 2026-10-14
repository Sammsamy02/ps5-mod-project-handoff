import hashlib,io,json,sys,tempfile,unittest,warnings,zipfile
from pathlib import Path
H=Path(__file__).resolve().parents[1];sys.path.insert(0,str(H))
try:import audit_provider as a
except ImportError:a=None

def fixture(rows):
 b=io.BytesIO()
 with warnings.catch_warnings():
  warnings.simplefilter('ignore')
  with zipfile.ZipFile(b,'w') as z:
   for name,value in rows:z.writestr(name,value)
 return b.getvalue()
def sha(b):return hashlib.sha256(b).hexdigest()
class AuditTests(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(a,'source auditor absent');self.temp=tempfile.TemporaryDirectory(dir=H/'tmp');self.addCleanup(self.temp.cleanup);self.d=Path(self.temp.name)
  self.blob=fixture([('p/interface.c',b'public source text\n')]);self.pins={'p/interface.c':sha(b'public source text\n')}
 def test_bound_source_is_not_a_kernel_service_admission(self):
  out=a.inspect_zip(self.blob,sha(self.blob),self.pins);self.assertEqual(out['label'],'SOURCE_BYTES_BOUND_ONLY');self.assertEqual(out['files_checked'],1);self.assertFalse(out['kernel_provider_admitted']);self.assertEqual(out['license_status'],'NOT_FOUND_IN_ARCHIVE')
 def test_archive_drift_refuses(self):
  with self.assertRaisesRegex(ValueError,'archive SHA'):a.inspect_zip(self.blob+b'x',sha(self.blob),self.pins)
 def test_member_drift_or_missing_refuses(self):
  changed=fixture([('p/interface.c',b'changed')])
  with self.assertRaisesRegex(ValueError,'member SHA'):a.inspect_zip(changed,sha(changed),self.pins)
  with self.assertRaisesRegex(ValueError,'missing'):a.inspect_zip(self.blob,sha(self.blob),{'p/absent.c':'00'*32})
 def test_duplicate_names_and_zero_pins_refuse(self):
  raw=fixture([('p/interface.c',b'x'),('p/interface.c',b'x')])
  with self.assertRaisesRegex(ValueError,'duplicate'):a.inspect_zip(raw,sha(raw),self.pins)
  with self.assertRaisesRegex(ValueError,'nonzero'):a.inspect_zip(self.blob,sha(self.blob),{})
 def test_oversized_or_nontext_member_refuses(self):
  for value in [b'x'*(64*1024+1),b'\xff\xfe']:
   raw=fixture([('p/interface.c',value)])
   with self.assertRaises(ValueError):a.inspect_zip(raw,sha(raw),{'p/interface.c':sha(value)})
 def test_license_presence_is_not_license_permission_inference(self):
  raw=fixture([('p/interface.c',b'public source text\n'),('p/LICENSE',b'Actual retained license text')]);out=a.inspect_zip(raw,sha(raw),self.pins);self.assertEqual(out['license_status'],'PRESENT_UNREVIEWED');self.assertFalse(out['redistribution_permission_established'])
 def test_stable_read_pin_and_size_refuse(self):
  p=self.d/'source.zip';p.write_bytes(self.blob)
  with self.assertRaisesRegex(ValueError,'SHA'):a.stable_read(p,1024,'00'*32)
  with self.assertRaisesRegex(ValueError,'size'):a.stable_read(p,4,sha(self.blob))
 def test_output_nooverwrite_outside_and_input_clobber_refuse(self):
  p=self.d/'receipt.json';a.write_output({'sources':[]},p)
  with self.assertRaises(ValueError):a.write_output({'sources':[]},p)
  with self.assertRaises(ValueError):a.write_output({'sources':[]},H.parent/'outside.json')
  target=self.d/'new.json'
  with self.assertRaises(ValueError):a.write_output({'sources':[{'path':str(target)}]},target)
if __name__=='__main__':unittest.main()
