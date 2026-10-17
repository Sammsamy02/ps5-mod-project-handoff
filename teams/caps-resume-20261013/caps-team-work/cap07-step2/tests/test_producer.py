import unittest,tempfile,subprocess,os
from pathlib import Path
H=Path(__file__).resolve().parent
class Producer(unittest.TestCase):
 def test_discovery(self):
  with tempfile.TemporaryDirectory(dir=H/'tmp') as t:
   p=Path(t);(p/'b.pak').write_bytes(b'x');(p/'a.pak').write_bytes(b'x')
   r=subprocess.run([str(H/'tmp/producer'),str(p)],capture_output=True,text=True)
   self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout.splitlines(),[str(p/'a.pak'),str(p/'b.pak')])
 def test_symlink_refused(self):
  with tempfile.TemporaryDirectory(dir=H/'tmp') as t:
   p=Path(t);(p/'a.pak').symlink_to('/not-an-input');r=subprocess.run([str(H/'tmp/producer'),str(p)],capture_output=True,text=True);self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,'')
 def test_partial_trio_refused(self):
  with tempfile.TemporaryDirectory(dir=H/'tmp') as t:
   p=Path(t);(p/'a.pak').write_bytes(b'x');(p/'a.utoc').write_bytes(b'x');r=subprocess.run([str(H/'tmp/producer'),str(p)],capture_output=True,text=True);self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,'')
 def test_selection_cap(self):
  with tempfile.TemporaryDirectory(dir=H/'tmp') as t:
   p=Path(t)
   for i in range(17):(p/f'{i}.pak').write_bytes(b'x')
   r=subprocess.run([str(H/'tmp/producer'),str(p)],capture_output=True,text=True);self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,'')
 def test_missing_directory_refuses(self):
  with tempfile.TemporaryDirectory(dir=H/'tmp') as t:
   r=subprocess.run([str(H/'tmp/producer'),str(Path(t)/'missing')],capture_output=True,text=True)
   self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,'')
 def test_sidecar_directory_refuses(self):
  with tempfile.TemporaryDirectory(dir=H/'tmp') as t:
   p=Path(t);(p/'a.pak').write_bytes(b'x');(p/'a.utoc').mkdir();(p/'a.ucas').write_bytes(b'x')
   r=subprocess.run([str(H/'tmp/producer'),str(p)],capture_output=True,text=True)
   self.assertNotEqual(r.returncode,0);self.assertEqual(r.stdout,'')
if __name__=='__main__':unittest.main()
