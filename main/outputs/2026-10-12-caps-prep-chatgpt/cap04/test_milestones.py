"""Catches accepting late/incomplete/mixed-run timing as first-map proof."""
import copy
import importlib.util
from pathlib import Path
import unittest
P=Path(__file__).with_name('verify_milestones.py')
spec=importlib.util.spec_from_file_location('milestones',P)
mod=importlib.util.module_from_spec(spec) if P.exists() else None
if mod: spec.loader.exec_module(mod)

def fixture():
    return {'schema':1,'events':[dict(kind=k,t=t,run_id='run1',clock_id='monotonic1',build_sha256='a'*64,source='decoded-session.log',line=i+1,**extra) for i,(k,t,extra) in enumerate([
        ('coverage',0,{'from_process_start':True,'lossless':True,'through':10}),
        ('module_start',1,{}),('observer_armed',2,{}),('loadmap_entry',3,{'ordinal':1}),('begin_play',4,{})])]}
class Tests(unittest.TestCase):
    def verdict(self,d):
        self.assertIsNotNone(mod,'milestone verifier missing'); return mod.verify(d)['verdict']
    def test_complete_common_clock_first_entry_passes(self): self.assertEqual(self.verdict(fixture()),'PASS')
    def test_late_arm_fails_with_complete_coverage(self):
        d=fixture();d['events'][2]['t']=3.5;self.assertEqual(self.verdict(d),'FAIL')
    def test_late_callback_without_start_coverage_inconclusive(self):
        d=fixture();d['events'][0]['from_process_start']=False;self.assertEqual(self.verdict(d),'INCONCLUSIVE')
    def test_mixed_identity_or_clock_refuses(self):
        for field in ['run_id','clock_id','build_sha256']:
            d=fixture();d['events'][-1][field]='other';self.assertEqual(self.verdict(d),'INCONCLUSIVE')
    def test_no_silent_first_map_inference(self):
        for change in ['remove','second','duplicate']:
            d=fixture()
            if change=='remove':d['events'][3].pop('ordinal')
            if change=='second':d['events'][3]['ordinal']=2
            if change=='duplicate':d['events'].append(copy.deepcopy(d['events'][3]))
            self.assertEqual(self.verdict(d),'INCONCLUSIVE')
    def test_missing_source_nonfinite_or_short_coverage_refuses(self):
        for change in ['source','nan','short','loss']:
            d=fixture()
            if change=='source': d['events'][2].pop('line')
            if change=='nan': d['events'][2]['t']=float('nan')
            if change=='short': d['events'][0]['through']=3.5
            if change=='loss': d['events'][0]['lossless']=False
            self.assertEqual(self.verdict(d),'INCONCLUSIVE')
    def test_empty_is_inconclusive(self):self.assertEqual(self.verdict({'schema':1,'events':[]}),'INCONCLUSIVE')
    def test_raw_historical_log_cannot_prove_first_map(self):
        self.assertIsNotNone(mod,'verifier missing')
        path=Path(__file__).resolve().parents[3]/'outputs/2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log'
        self.assertTrue(hasattr(mod,'audit_raw'),'raw-log adapter missing')
        d=mod.audit_raw(path);self.assertEqual(d['verdict'],'INCONCLUSIVE');self.assertGreater(d['decoded_lines'],0)
        self.assertEqual(len(d['input_sha256']),64)
if __name__=='__main__':unittest.main()
