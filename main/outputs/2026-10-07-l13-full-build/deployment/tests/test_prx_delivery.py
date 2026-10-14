import hashlib, json, os
from pathlib import Path
import subprocess, sys, tempfile, unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'prx_delivery.py'
REMOTE = '/data/plugins/modbridge_supplier_composition.prx'
URL = 'ftp://ps5-host-not-configured.invalid:2121//data/plugins/modbridge_supplier_composition.prx'
MAGIC = bytes.fromhex('4f153d1d')
STUB = r'''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
args=sys.argv[1:]
with Path(os.environ['STUB_TRACE']).open('a') as f: f.write(json.dumps(args)+'\n')
remote=Path(os.environ['STUB_REMOTE'])
if '--upload-file' in args:
    content=Path(args[args.index('--upload-file')+1]).read_bytes()
    remote.write_bytes(content+(b'corrupt' if os.environ.get('STUB_CORRUPT_PUT')=='1' else b''))
else:
    Path(args[args.index('--output')+1]).write_bytes(remote.read_bytes())
    hdr=Path(args[args.index('--dump-header')+1])
    hdr.write_text('226 SELF transfer mode disabled\r\n226 Transfer complete\r\n' if os.environ.get('STUB_ACK','1')=='1' else '226 Transfer complete\r\n')
'''
class DeliveryTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='l13 delivery tests '); self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name); self.stub=self.root/'curl-stub';self.stub.write_text(STUB);self.stub.chmod(0o755)
        self.old=MAGIC+b'old-signed-runtime';self.new=MAGIC+b'new-signed-runtime'
        self.remote=self.root/'remote.prx';self.remote.write_bytes(self.old)
        self.candidate=self.root/'candidate.prx';self.candidate.write_bytes(self.new)
        self.manifest=self.root/'manifest.json';self.manifest.write_text(json.dumps({'schema':1,'remote_path':REMOTE,'candidate':{'path':str(self.candidate),'sha256':hashlib.sha256(self.new).hexdigest(),'bytes':len(self.new)},'expected_remote_sha256':hashlib.sha256(self.old).hexdigest()}))
        self.trace=self.root/'trace.jsonl';self.jobs=self.root/'jobs'
        self.env=dict(os.environ,STUB_TRACE=str(self.trace),STUB_REMOTE=str(self.remote))
    def run_cli(self,*args):
        return subprocess.run([sys.executable,str(SCRIPT),*args,'--curl',str(self.stub),'--jobs',str(self.jobs)],env=self.env,text=True,capture_output=True)
    def calls(self): return [json.loads(x) for x in self.trace.read_text().splitlines()] if self.trace.exists() else []
    def upload(self):
        r=self.run_cli('upload','--manifest',str(self.manifest),'--execute');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        return next(self.jobs.glob('upload-*/backup.json'))
    def assert_commands(self,calls):
        self.assertEqual(sum('--upload-file' in c for c in calls),1)
        for c in calls:
            self.assertEqual(c[0],'--disable');self.assertEqual(c[-1],URL)
            self.assertFalse(any('retry' in a or 'DELE' in a or 'MKD' in a for a in c))
            if '--output' in c:
                self.assertEqual(c[c.index('--quote')+1],'SITE SELF');self.assertIn('--dump-header',c)
    def test_default_hold_makes_no_calls(self):
        r=self.run_cli();self.assertEqual(r.returncode,0,r.stderr);self.assertIn('HOLD',r.stdout);self.assertEqual(self.calls(),[])
    def test_upload_plan_makes_no_calls(self):
        r=self.run_cli('upload','--manifest',str(self.manifest));self.assertEqual(r.returncode,0,r.stderr);self.assertIn('HOLD',r.stdout);self.assertEqual(self.calls(),[])
    def test_upload_uses_same_connection_decrypt_off_and_single_target(self):
        backup=self.upload();self.assertEqual(self.remote.read_bytes(),self.new);self.assertEqual(len(self.calls()),3);self.assert_commands(self.calls())
        b=json.loads(backup.read_text());self.assertEqual(Path(b['preimage']['path']).read_bytes(),self.old)
        receipt=json.loads((backup.parent/'receipt.json').read_text());self.assertEqual(receipt['status'],'verified');self.assertEqual(receipt['postimage']['sha256'],hashlib.sha256(self.new).hexdigest())
    def test_rollback_restores_verified_backup_and_preserves_both_phase_receipts(self):
        backup=self.upload();self.trace.unlink();r=self.run_cli('rollback','--backup-receipt',str(backup),'--execute');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertEqual(self.remote.read_bytes(),self.old);self.assert_commands(self.calls());self.assertEqual(len(list(self.jobs.glob('*/receipt.json'))),2)
    def test_corrupted_backup_is_rejected_before_any_curl(self):
        backup=self.upload();b=json.loads(backup.read_text());p=Path(b['preimage']['path']);p.chmod(0o600);p.write_bytes(MAGIC+b'corrupt')
        self.trace.unlink();r=self.run_cli('rollback','--backup-receipt',str(backup),'--execute');self.assertNotEqual(r.returncode,0);self.assertEqual(self.calls(),[]);self.assertEqual(self.remote.read_bytes(),self.new)
    def test_wrong_preimage_stops_before_upload(self):
        self.remote.write_bytes(MAGIC+b'other-version');r=self.run_cli('upload','--manifest',str(self.manifest),'--execute');self.assertNotEqual(r.returncode,0);self.assertEqual(len(self.calls()),1);self.assertNotIn('--upload-file',self.calls()[0])
    def test_missing_decrypt_off_ack_stops_before_upload(self):
        self.env['STUB_ACK']='0';r=self.run_cli('upload','--manifest',str(self.manifest),'--execute');self.assertNotEqual(r.returncode,0);self.assertEqual(len(self.calls()),1);self.assertEqual(self.remote.read_bytes(),self.old)
    def test_post_hash_mismatch_is_saved_without_retry_or_automatic_rollback(self):
        self.env['STUB_CORRUPT_PUT']='1';r=self.run_cli('upload','--manifest',str(self.manifest),'--execute');self.assertNotEqual(r.returncode,0);self.assertEqual(len(self.calls()),3);self.assert_commands(self.calls())
        receipt=json.loads(next(self.jobs.glob('upload-*/receipt.json')).read_text());self.assertEqual(receipt['status'],'failed');self.assertIn('postimage',receipt);self.assertTrue(next(self.jobs.glob('upload-*/backup.json')).is_file())
    def test_manifest_cannot_change_remote_path(self):
        m=json.loads(self.manifest.read_text());m['remote_path']='/data/plugins/other.prx';self.manifest.write_text(json.dumps(m));r=self.run_cli('upload','--manifest',str(self.manifest),'--execute');self.assertNotEqual(r.returncode,0);self.assertEqual(self.calls(),[])
if __name__=='__main__': unittest.main()
