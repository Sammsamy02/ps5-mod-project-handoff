import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
try:
    from local_shader_intake import extract_verified
except ImportError:
    def extract_verified(*a,**k): return b''

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('existing_builder_for_test',HERE.parents[2]/'outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py')
builder=importlib.util.module_from_spec(spec); spec.loader.exec_module(builder)

class Fixture:
    ids=(bytes.fromhex('010000000000000000000008'),)
    block_size=4; block_count=2; partition_count=1; partition_size=(1<<64)-1
    def offset_length(self,row): return 1,6
    def block(self,i): return [(8,4,4,0),(24,4,4,0)][i]
    def meta(self,row): return builder.blake3_20(b'bcdefg')+bytes(4)

class IntakeTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(dir=HERE/'tmp'); self.p=Path(self.tmp.name)/'provided.ucas'
        self.p.write_bytes(b'\0'*8+b'abcd'+b'\0'*12+b'efgh')
        self.sha=hashlib.sha256(self.p.read_bytes()).hexdigest()
    def tearDown(self): self.tmp.cleanup()
    def extract(self,toc=None,**kwargs):
        with self.p.open('rb') as f:
            return extract_verified(toc or Fixture(),0,f,self.sha,builder.blake3_20,**kwargs)
    def test_exact_payload_hash_and_discontiguous_geometry(self): self.assertEqual(self.extract(),b'bcdefg')
    def test_wrong_iohash_refuses(self):
        t=Fixture(); t.meta=lambda row:bytes(24)
        with self.assertRaises(ValueError): self.extract(t)
    def test_wrong_source_hash_refuses(self):
        self.sha='0'*64
        with self.assertRaises(ValueError): self.extract()
    def test_source_and_chunk_caps_refuse(self):
        with self.assertRaises(ValueError): self.extract(max_source_bytes=10)
        with self.assertRaises(ValueError): self.extract(max_chunk_bytes=5)
    def test_physical_eof_refuses(self):
        self.p.write_bytes(self.p.read_bytes()[:-2]); self.sha=hashlib.sha256(self.p.read_bytes()).hexdigest()
        with self.assertRaises(ValueError): self.extract()
    def test_hash_bound_local_slice_has_explicit_physical_origin(self):
        import inspect
        self.assertIn('physical_base',inspect.signature(extract_verified).parameters)
        self.p.write_bytes(self.p.read_bytes()[8:]); self.sha=hashlib.sha256(self.p.read_bytes()).hexdigest()
        self.assertEqual(self.extract(physical_base=8),b'bcdefg')
    def test_seed_zero_toc_is_refused_before_extracting(self):
        import json
        import os
        import struct
        import subprocess
        import sys
        fixture=HERE/'synthetic-intake-fixture'
        raw=bytearray((fixture/'fixture.utoc').read_bytes())
        struct.pack_into('<I',raw,84,1)
        # One zero seed gives no row for its bucket, despite valid array geometry.
        broken=self.p.parent/'broken.utoc'; broken.write_bytes(raw[:166]+bytes(4)+raw[166:])
        job=json.loads((fixture/'job.json').read_text())
        job['toc']={'path':str(broken),'sha256':hashlib.sha256(broken.read_bytes()).hexdigest()}
        jp=self.p.parent/'bad-lookup-job.json'; jp.write_text(json.dumps(job))
        out=self.p.parent/'refused-output'
        run=subprocess.run([sys.executable,str(HERE/'local_shader_intake.py'),str(jp),'--out-dir',str(out)],capture_output=True,text=True,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
        self.assertNotEqual(run.returncode,0)
        self.assertIn('chunk_lookup_does_not_resolve_all_rows',run.stderr)
        self.assertFalse(out.exists())
    def test_nonshader_row_refuses(self):
        t=Fixture(); t.ids=(bytes.fromhex('010000000000000000000001'),)
        with self.assertRaises(ValueError): self.extract(t)

if __name__=='__main__': unittest.main()
