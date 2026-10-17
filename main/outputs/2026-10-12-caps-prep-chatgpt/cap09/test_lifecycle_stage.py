import hashlib,unittest
from pathlib import Path
from stage_lifecycle_adapter import derive
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
SOURCE=ROOT/'.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/cppmods'
class ExactOnceTests(unittest.TestCase):
    def setUp(self):
        self.source=(SOURCE/'KismetDebuggerMod/src/dllmain.cpp').read_bytes()
        self.sha=hashlib.sha256(self.source).hexdigest()
    def test_real_source_exports_renamed_once(self):
        out=derive(self.source,self.sha)
        self.assertNotEqual(out,self.source)
        self.assertIn(b'mb_cppmod_source_start_mod()',out)
        self.assertIn(b'mb_cppmod_source_uninstall_mod(',out)
        self.assertEqual(out.replace(b'mb_cppmod_source_start_mod()',b'start_mod()').replace(b'mb_cppmod_source_uninstall_mod(',b'uninstall_mod('),self.source)
    def test_source_drift_refused(self):
        with self.assertRaisesRegex(ValueError,'source_hash_mismatch'):derive(self.source+b'\n',self.sha)
    def test_already_applied_refused(self):
        changed=self.source.replace(b'start_mod()',b'mb_cppmod_source_start_mod()')
        with self.assertRaisesRegex(ValueError,'already_derived'):derive(changed,hashlib.sha256(changed).hexdigest())
    def test_missing_export_refused_even_if_hash_matches(self):
        missing=self.source.replace(b'uninstall_mod(',b'other_name(')
        with self.assertRaisesRegex(ValueError,'export_anchor_count'):derive(missing,hashlib.sha256(missing).hexdigest())
if __name__=='__main__':unittest.main(verbosity=2)
