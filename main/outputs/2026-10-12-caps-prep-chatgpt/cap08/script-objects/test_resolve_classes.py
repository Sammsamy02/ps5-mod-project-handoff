import importlib.util
from pathlib import Path
import struct
import sys
import unittest
try:
    from resolve_classes import resolve_classes
except ImportError:
    def resolve_classes(*a):return []

SOURCE=Path.cwd()/'.superpowers/native-dependency-spike/zen-script-import-resolver-1/zen_script_objects.py'
spec=importlib.util.spec_from_file_location('saved_script_reader_fixture',SOURCE);reader=importlib.util.module_from_spec(spec);sys.modules[spec.name]=reader;spec.loader.exec_module(reader)
NULL=(1<<64)-1

def table(cycle=False):
    names=[b'/Script/Example',b'TestClass'];batch=struct.pack('<IIQ',2,sum(map(len,names)),0xc1640000)+bytes(16)+b''.join(len(n).to_bytes(2,'big') for n in names)+b''.join(names)
    root=(1<<62)|1;cls=(1<<62)|2
    records=struct.pack('<IIQQQ',2<<30,0,root,cls if cycle else NULL,NULL)+struct.pack('<IIQQQ',(2<<30)|1,0,cls,root,NULL)
    return reader.read_script_objects(batch+struct.pack('<I',2)+records)

def identities(raw=(1<<62)|2):
    return {'packages':[{'package_id':'0000000000000001','decoded':{'exports':[{'name':'Asset','class_ref':{'raw':f'{raw:016x}','kind':'ScriptImport'}}]}}]}

class ResolutionTests(unittest.TestCase):
    def test_exact_index_and_outer_chain_resolve_without_guesses(self):
        rows=resolve_classes(identities(),table())
        self.assertEqual(rows[0]['class_object_outer_chain_path'],'/Script/Example/TestClass')
        self.assertTrue(rows[0]['class_metadata_identity_resolved'])
        self.assertFalse(rows[0]['compatible'])
    def test_missing_script_index_is_explicit_unresolved(self):
        rows=resolve_classes(identities((1<<62)|99),table())
        self.assertFalse(rows[0]['class_metadata_identity_resolved'])
        self.assertEqual(rows[0]['reason'],'script_index_absent_from_captured_table')
    def test_cyclic_outer_refuses(self):
        with self.assertRaises(ValueError):resolve_classes(identities(),table(True))
    def test_zero_subjects_refuses(self):
        with self.assertRaises(ValueError):resolve_classes({'packages':[]},table())

if __name__=='__main__':unittest.main()
