"""Small characterization tests of existing scanner admission boundaries.
Synthetic data establishes scanner behavior only, never real UE4 support.
"""
import contextlib,hashlib,importlib.util,io,json,struct,sys,tempfile,unittest
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
spec=importlib.util.spec_from_file_location('existing_engine_match',ROOT/'outputs/2026-10-08-l25-prep/phasea-tools/engine_match.py')
em=importlib.util.module_from_spec(spec);spec.loader.exec_module(em)
class ScannerContract(unittest.TestCase):
    def test_refusal_is_report_success_not_admission(self):
        # A valid little synthetic ELF with no Unreal markers must refuse,
        # even though the CLI correctly returns success for producing a report.
        with tempfile.TemporaryDirectory(dir=HERE/'tmp') as td:
            p=Path(td)/'synthetic.elf';b=bytearray(64);b[:7]=b'\x7fELF\x02\x01\x01';struct.pack_into('<H',b,16,2);struct.pack_into('<H',b,18,0x3e);struct.pack_into('<I',b,20,1);struct.pack_into('<H',b,52,64);p.write_bytes(b)
            output=io.StringIO()
            with contextlib.redirect_stdout(output):code=em.main(['engine_match.py',str(p),'--json'])
            d=json.loads(output.getvalue());self.assertEqual(code,0);self.assertEqual(d['tier'],'REFUSED');self.assertEqual(d['anchor_pack'],'none');self.assertFalse(d['full_scan_done'])
            self.assertEqual(d['image']['sha256'],hashlib.sha256(b).hexdigest())
    def test_self_container_never_parsed_as_plain_elf(self):
        with tempfile.TemporaryDirectory(dir=HERE/'tmp') as td:
            for magic in ['4f153d1d','5414f5ee']:
                p=Path(td)/'synthetic.self';p.write_bytes(bytes.fromhex(magic)+bytes(60))
                with self.assertRaises(ValueError):em.parse_image(p)
    def test_marker_outside_file_backed_load_is_ignored(self):
        with tempfile.TemporaryDirectory(dir=HERE/'tmp') as td:
            p=Path(td)/'synthetic.bin';p.write_bytes(b'prefix'+ 'UE fixture'.encode('utf-16le'))
            image={'path':str(p),'loads':[{'off':0,'filesz':6}]}
            self.assertEqual(em.scan_markers(image,[{'string':'UE fixture'}]),[False])
    def test_synthetic_ue4_model_is_self_consistent_only(self):
        versions,markers=em.load_markers(em.DEFAULT_MARKERS)
        for version,expected in [((4,7),'4.7'),((4,27),'4.27'),((5,6),'5.6')]:
            presence=[em.covers(m,versions.index(version),2,False) for m in markers]
            self.assertEqual(em.cost_solve(versions,markers,presence)['version'],expected)
if __name__=='__main__':unittest.main()
