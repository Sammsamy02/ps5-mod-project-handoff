import copy,json,struct,unittest
from pathlib import Path
from check_import_identity import verify
ROOT=Path(__file__).resolve().parents[2]

class ExactImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        folder=ROOT/'outputs/2026-10-07-l19-r4-full-build/prx-candidate-l19-2'
        cls.blob=(folder/'bare-sdk/modbridge_supplier_composition.prx').read_bytes()
        cls.contract=json.loads((folder/'supplier-contract.json').read_text())
        cls.result=verify(cls.blob,cls.contract)
    def test_frozen_206_rows(self):self.assertEqual(self.result['count'],206)
    def test_module_version_mismatch(self):
        blob=bytearray(self.blob);pos=next(r for r in self.result['imports'] if r['name']=='sceKernelOpen')['module']['value_offset']
        value=struct.unpack_from('<Q',blob,pos)[0];struct.pack_into('<Q',blob,pos,value^(1<<32))
        with self.assertRaises(ValueError):verify(blob,self.contract)
    def test_library_version_mismatch(self):
        blob=bytearray(self.blob);pos=next(r for r in self.result['imports'] if r['name']=='sceKernelOpen')['library']['value_offset']
        value=struct.unpack_from('<Q',blob,pos)[0];struct.pack_into('<Q',blob,pos,value^(1<<32))
        with self.assertRaises(ValueError):verify(blob,self.contract)
    def test_missing_getter_refuses(self):
        with self.assertRaises(ValueError):verify(self.blob,self.contract,require_getter=True)

if __name__=='__main__':unittest.main()
