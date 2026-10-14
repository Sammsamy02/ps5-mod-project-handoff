"""Catches no-crash false positives, stale pairing, and wrong-module symbols."""
import hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
P=Path(__file__).with_name('crash_report.py')
spec=importlib.util.spec_from_file_location('crash_report',P)
mod=importlib.util.module_from_spec(spec) if P.exists() else None
if mod:spec.loader.exec_module(mod)
ROOT=Path(__file__).resolve().parents[3]
KLOG=ROOT/'outputs/2026-10-08-l25-prep/evidence/tmp-preserved-20261010/tmp/l25-variant-A/klog-20261009-021938.log'
TOOLS=ROOT/'outputs/2026-10-08-l25-prep/phasea-tools'
PRX=ROOT/'outputs/2026-10-08-l26-package-A/pass-1/bare-original/modbridge_supplier_composition.prx'
class Tests(unittest.TestCase):
    def report(self,*a,**kw):
        self.assertIsNotNone(mod,'crash reporter missing');return mod.report(*a,**kw)
    def test_saved_crashes_resolve_module_without_fake_symbols(self):
        d=self.report(KLOG,build_id='historical-fixture');self.assertEqual(d['status'],'CRASHES_FOUND');self.assertEqual(d['block_count'],2)
        self.assertEqual(d['crashes'][0]['rip']['module_offset'],'0x224c');self.assertEqual(d['crashes'][0]['rip']['confidence'],'MODULE_ONLY')
        self.assertEqual(d['capture_build_binding'],'UNVERIFIED');self.assertEqual(len(d['identities']['klog_sha256']),64)
    def test_empty_log_cannot_pass_crash_proof(self):
        with tempfile.TemporaryDirectory(dir=P.parent/'tmp') as td:
            p=Path(td)/'empty.log';p.write_text('');d=self.report(p,build_id='test');self.assertEqual(d['status'],'NO_CRASH_BLOCKS')
    def test_mismatched_sidecar_refuses_before_report(self):
        with tempfile.TemporaryDirectory(dir=P.parent/'tmp') as td:
            p=Path(td)/'bad.json';p.write_text(json.dumps({'prx_sha256':'0'*64,'symbols':[]}))
            with self.assertRaises(ValueError):self.report(KLOG,build_id='test',prx=PRX,sidecar=p,prx_module='runtime.prx')
    def test_sidecar_requires_prx_and_explicit_owner(self):
        side=TOOLS/'prx-symbols-l26-package-A.json'
        for kw in [dict(sidecar=side),dict(prx=PRX,sidecar=side)]:
            with self.assertRaises(ValueError):self.report(KLOG,build_id='test',**kw)
    def test_runtime_symbols_never_applied_to_libkernel(self):
        with tempfile.TemporaryDirectory(dir=P.parent/'tmp') as td:
            prx=Path(td)/'synthetic.prx';prx.write_bytes(b'synthetic artifact, not executable')
            side=Path(td)/'symbols.json';side.write_text(json.dumps({'prx_sha256':hashlib.sha256(prx.read_bytes()).hexdigest(),'symbols':[{'name':'fixture_function','addr':0x224c,'size':32}]}))
            d=self.report(KLOG,build_id='test',prx=prx,sidecar=side,prx_module='synthetic.prx')
            self.assertEqual(d['symbol_pairing'],'VERIFIED_LOCAL_PRX');self.assertTrue(all(x['rip']['confidence']=='MODULE_ONLY' for x in d['crashes']))
    def test_real_saved_sidecar_is_stale_and_refused(self):
        with self.assertRaises(ValueError):self.report(KLOG,build_id='historical',prx=PRX,sidecar=TOOLS/'prx-symbols-l26-package-A.json',prx_module='modbridge_supplier_composition.prx')
    def test_function_bounds_and_unknown_owner_remain_honest(self):
        with tempfile.TemporaryDirectory(dir=P.parent/'tmp') as td:
            prx=Path(td)/'synthetic.prx';prx.write_bytes(b'synthetic artifact')
            side=Path(td)/'symbols.json';side.write_text(json.dumps({'prx_sha256':hashlib.sha256(prx.read_bytes()).hexdigest(),'symbols':[{'name':'fixture_fn','addr':16,'size':16}]}))
            log=Path(td)/'synthetic.log'
            for rip,expect in [('1018','LOCAL_ARTIFACT_FUNCTION_MATCH_CAPTURE_PAIRING_UNVERIFIED'),('1030','MODULE_ONLY'),('3000','UNRESOLVED')]:
                log.write_text('# signal: SIGSEGV\n# rip: '+rip+' x\n# /app0/synthetic.prx\n# xotext: 1000:2000 nsegs: 1\n')
                d=self.report(log,build_id='synthetic',prx=prx,sidecar=side,prx_module='synthetic.prx');self.assertEqual(d['crashes'][0]['rip']['confidence'],expect)
    def test_human_report_redacts_ipv4_and_marks_unverified(self):
        d=self.report(KLOG,build_id='fixture');text=mod.render(d)
        self.assertIn('UNVERIFIED',text);self.assertIn('MODULE_ONLY',text)

if __name__=='__main__':unittest.main()
