import sys
sys.dont_write_bytecode=True
import unittest,hashlib
from pathlib import Path
import assembly_preflight
BASE=Path(__file__).resolve().parents[1]
class RealArtifact(unittest.TestCase):
 def test_compiled_probe_contract(self):
  p=BASE/'managed/Probe.dll'; self.assertTrue(p.exists(), 'actual compiler output missing')
  r=assembly_preflight.inspect(p.read_bytes()); self.assertEqual(r['verdict'],'METADATA_CONTRACT_MATCH'); self.assertEqual(r['assembly_references'],['mscorlib'])
if __name__=='__main__':unittest.main()
