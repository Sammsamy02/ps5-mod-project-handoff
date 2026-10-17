"""Synthetic receipt fixtures exercise gating, never PS5 compatibility."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

try:
    from directive_gate import evaluate
except ModuleNotFoundError:
    def evaluate(*args):
        return {'result': 'MISSING_IMPLEMENTATION', 'directives': []}

HERE = Path(__file__).resolve().parent

def ref(path):
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

class GateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=HERE / 'tmp')
        self.p = Path(self.tmp.name)
        source = self.p / 'captured.bin'; source.write_bytes(b'fixture export bytes')
        extractor = self.p / 'extractor.txt'; extractor.write_text('synthetic test extractor; no engine proof')
        self.source, self.extractor = ref(source), ref(extractor)
        body = struct.pack('<ii',0,0)+struct.pack('<ii',1,8)+b'\x12'*20
        store=self.p/'store.bin'; store.write_bytes(struct.pack('<IIQI',0x496f436e,5,9,1)+struct.pack('<Q',1)+struct.pack('<I',len(body))+body)
        self.classification={'packages':[{'package_id':'0000000000000001','virtual_path':'/Any/A.uasset','directive':'SUBSTITUTE_BASE_EXPORT','base_replaced':True}], 'summary':{'mod_packages':1}}
        c=self.p/'classification.json'; c.write_text(json.dumps(self.classification)); self.c=ref(c)
        self.manifest={'schema':1,'classification':self.c,'catalogue_complete':True,'containers':[{'container_id':'0000000000000009','store':ref(store)}], 'packages':{}}
        self.manifest['packages']['0000000000000001']={'store_container':'0000000000000009','identity':self.receipt('identity',{'package_id':'0000000000000001','mod_export':'A','base_export':'A','mod_class':'Material','base_class':'Material'}),'intent':self.receipt('intent',{'package_id':'0000000000000001','allow_visual_change':True}), 'compatibility':self.receipt('compatibility',{'package_id':'0000000000000001','target_package_id':'0000000000000001','source_class':'Material','target_class':'Material','import_compatible':True})}
    def tearDown(self): self.tmp.cleanup()
    def receipt(self, kind, facts):
        p=self.p/(kind+str(len(list(self.p.iterdir())))+'.json')
        p.write_text(json.dumps({'schema':1,'kind':kind,'facts':facts,'sources':[self.source],'extractor':self.extractor}))
        return ref(p)
    def run_gate(self): return evaluate(self.classification,self.manifest)
    def test_complete_receipts_emit_suppression(self):
        result=self.run_gate(); self.assertEqual(result['result'],'VALIDATED_DIRECTIVES'); self.assertEqual(result['directives'][0]['action'],'suppress_override')
    def test_missing_store_is_unknown_never_added(self):
        self.manifest['containers']=[]
        result=self.run_gate(); self.assertEqual(result['result'],'REVIEW_REQUIRED'); self.assertEqual(result['directives'],[])
    def test_incomplete_catalogue_blocks_all(self):
        self.manifest['catalogue_complete']=False
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_stale_classification_blocks(self):
        self.classification['packages'][0]['virtual_path']='/tampered'
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_changed_source_blocks_receipt(self):
        Path(self.source['path']).write_bytes(b'changed')
        self.assertEqual(self.run_gate()['directives'],[]); self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_class_mismatch_blocks(self):
        self.manifest['packages']['0000000000000001']['identity']=self.receipt('identity',{'package_id':'0000000000000001','mod_export':'A','base_export':'A','mod_class':'Material','base_class':'Texture'})
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_empty_analysis_refuses(self):
        self.classification['packages']=[]
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_unknown_candidate_blocks_even_with_good_other_row(self):
        self.classification['packages'].append({'package_id':'0000000000000002','directive':'HIDE_VIA_BASE_INVISIBLE'})
        p=Path(self.c['path']); p.write_text(json.dumps(self.classification)); self.manifest['classification']=ref(p)
        result=self.run_gate(); self.assertEqual(result['result'],'REVIEW_REQUIRED'); self.assertEqual(result['directives'],[])
    def test_duplicate_rows_refuse(self):
        self.classification['packages']*=2
        p=Path(self.c['path']); p.write_text(json.dumps(self.classification)); self.manifest['classification']=ref(p)
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_hide_requires_target_receipts(self):
        self.classification['packages'][0]['directive']='HIDE_VIA_BASE_INVISIBLE'
        p=Path(self.c['path']); p.write_text(json.dumps(self.classification)); self.manifest['classification']=ref(p)
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')

    def test_missing_suppression_compatibility_blocks(self):
        del self.manifest['packages']['0000000000000001']['compatibility']
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_malformed_store_negative_count_blocks(self):
        entry=self.manifest['containers'][0]['store']; path=Path(entry['path']); raw=bytearray(path.read_bytes()); struct.pack_into('<i',raw,32,-1); path.write_bytes(raw); self.manifest['containers'][0]['store']=ref(path)
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')
    def test_valid_hide_requires_native_target_and_import_receipts(self):
        self.classification['packages'][0]['directive']='HIDE_VIA_BASE_INVISIBLE'
        p=Path(self.c['path']); p.write_text(json.dumps(self.classification)); self.manifest['classification']=ref(p)
        row=self.manifest['packages']['0000000000000001']
        row['intent']=self.receipt('intent',{'package_id':'0000000000000001','allow_visual_change':True,'hide_intent_verified':True})
        row['target']=self.receipt('target',{'package_id':'0000000000000001','target_package_id':'0000000000000001','target_container_id':'0000000000000009','export_name':'Hidden','public_export_hash':'abcdefabcdefabcd','class':'Material'})
        row['compatibility']=self.receipt('compatibility',{'package_id':'0000000000000001','target_package_id':'0000000000000001','target_public_export_hash':'abcdefabcdefabcd','source_class':'Material','target_class':'Material','import_compatible':True})
        self.assertEqual(self.run_gate()['result'],'VALIDATED_DIRECTIVES')
        row['compatibility']=self.receipt('compatibility',{'package_id':'0000000000000001','target_package_id':'0000000000000001','target_public_export_hash':'ffffffffffffffff','source_class':'Material','target_class':'Material','import_compatible':True})
        self.assertEqual(self.run_gate()['result'],'REVIEW_REQUIRED')

if __name__ == '__main__': unittest.main()
