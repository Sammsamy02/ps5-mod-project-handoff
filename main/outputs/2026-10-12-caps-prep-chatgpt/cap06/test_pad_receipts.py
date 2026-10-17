"""Real preserved event replay; removal of association must not prove delivery."""
import importlib.util,tempfile,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
P=HERE/'audit_pad_receipts.py'
spec=importlib.util.spec_from_file_location('audit_pad',P)
mod=importlib.util.module_from_spec(spec) if P.exists() else None
if mod:spec.loader.exec_module(mod)
CAPTURE=ROOT/'outputs/2026-10-08-l25-prep/evidence/tmp-preserved-20261010/tmp/zzdiag2-diag.jsonl'
class Tests(unittest.TestCase):
    def audit(self,path):
        self.assertIsNotNone(mod,'pad receipt audit missing');return mod.audit(path)
    def test_preserved_provider_delivery_never_implies_menu_action(self):
        d=self.audit(CAPTURE)
        self.assertGreater(d['provider_chain_count'],0);self.assertEqual(d['verdict'],'INCONCLUSIVE')
        self.assertEqual(d['expected_action_observation'],'UNPROVED')
        first=d['provider_chains'][0];self.assertEqual(first['line_refs'],[35443,35444,35445,35446,35447])
        self.assertEqual(first['pid'],126);self.assertEqual(first['seq_range'],[1114,1118])
    def test_unassociated_real_edge_and_dispatch_are_inconclusive(self):
        lines=CAPTURE.read_text().splitlines()
        with tempfile.TemporaryDirectory(dir=HERE/'tmp') as td:
            path=Path(td)/'cut-actual-receipt.jsonl'
            path.write_text(lines[35442]+'\n'+lines[35446]+'\n')
            d=self.audit(path);self.assertEqual(d['provider_chain_count'],0);self.assertEqual(d['verdict'],'INCONCLUSIVE')
if __name__=='__main__':unittest.main()
