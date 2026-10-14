import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

HOME=Path(__file__).resolve().parent
ROOT=HOME.parents[3]
def load(name,p):
    s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);sys.modules[name]=m;s.loader.exec_module(m);return m
PARSER=ROOT/'outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py'
ZEN=HOME.parent/'zen_identity.py'
parser=load('intake_test_parser',PARSER)
def ref(p):return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
def export():
    names=[b'/Any/P',b'Asset'];batch=struct.pack('<IIQ',2,sum(map(len,names)),0xc1640000)+bytes(16)+b''.join(len(n).to_bytes(2,'big') for n in names)+b''.join(names)
    start=60+len(batch);end=start+72
    return struct.pack('<6I9i',0,end,0,0,0,end,start,start,start,end,end,end,end,end,end)+batch+struct.pack('<QQIIQQQQQIB3s',end,0,1,0,(1<<64)-1,(1<<62)|123,(1<<64)-1,(1<<64)-1,0x1122334455667788,1,0,bytes(3))
try:import intake
except ImportError:intake=None

class IntakeTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(intake,'whole-chunk intake adapter is not implemented')
        self.tmp=tempfile.TemporaryDirectory(dir=HOME/'tmp');self.addCleanup(self.tmp.cleanup);self.d=Path(self.tmp.name)
        self.pid='0000000000000011';self.owner='0000000000000022';self.sha='a'*64
        self.store=parser.serialize_container_header(parser.ContainerHeader(0x22,{0x11:parser.StoreEntry((),()),0x99:parser.StoreEntry((),())},True,b''))
        self.payload=export();self.geos=[];self.targets=[]
        for n,(kind,data) in enumerate([(6,self.store),(1,self.payload)]):
            p=self.d/f'{kind}.bin';p.write_bytes(data)
            g={'owner_toc_sha256':self.sha,'owner_container_id_u64':self.owner,'toc_row':n,'chunk_id':int(self.owner if kind==6 else self.pid,16).to_bytes(8,'little').hex()+f'000000{kind:02x}','chunk_type':kind,'logical_length':len(data),'expected_iohash_20':parser.blake3_20(data).hex(),'logical_offset':0}
            self.geos.append(g);self.targets.append({'selector':{k:g[k] for k in ['owner_toc_sha256','owner_container_id_u64','toc_row','chunk_id']},'source':ref(p)})
        mod=self.d/'mod.json';mod.write_text(json.dumps({'packages':[{'package_id':self.pid,'decoded':{'exports':[{'name':'Asset','public_export_hash':'1122334455667788'}]}}]}))
        self.audit={'sources':{'parser':ref(PARSER),'mod_identity':ref(mod)},'required_base_export_inputs':[{'package_id':self.pid,'captured_owners':[self.geos[1]],'expected_public_hashes_from_verified_mod_imports':['1122334455667788']}],'missing_owner_stores':[{'geometry':self.geos[0]}],'downstream_store_dependency_layer':[]}
        ap=self.d/'audit.json';ap.write_text(json.dumps(self.audit));self.job={'schema':1,'audit':ref(ap),'zen_reader':ref(ZEN),'targets':self.targets}
    def runjob(self):return intake.run(self.job)
    def test_whole_chunks_decode_metadata_and_preserve_originals(self):
        before={t['source']['path']:Path(t['source']['path']).read_bytes() for t in self.targets};r=self.runjob()
        self.assertEqual(r['exports'][0]['decoded']['exports'][0]['name'],'Asset')
        self.assertTrue(r['exports'][0]['required_public_hashes'][0]['present'])
        self.assertEqual(r['exports'][0]['mod_export_projection'][0]['result'],'name_and_public_hash_match')
        self.assertFalse(r['compatible']);self.assertTrue(r['sources_unchanged'])
        self.assertEqual(r.get('items_checked'),2)
        for p,b in before.items():self.assertEqual(Path(p).read_bytes(),b)
    def test_store_projection_omits_unrequested_packages(self):
        r=self.runjob();self.assertEqual([p['package_id'] for p in r['stores'][0]['packages']],[self.pid])
        self.assertEqual(r['stores'][0]['total_packages_checked'],2)
    def test_wrong_length_and_partial_header_refuse(self):
        p=Path(self.targets[1]['source']['path']);p.write_bytes(self.payload[:-1]);self.targets[1]['source']=ref(p)
        with self.assertRaisesRegex(ValueError,'logical length'):self.runjob()
    def test_wrong_whole_iohash_refuses_even_with_updated_source_sha(self):
        p=Path(self.targets[1]['source']['path']);p.write_bytes(self.payload[:-1]+b'!');self.targets[1]['source']=ref(p)
        with self.assertRaisesRegex(ValueError,'IoHash'):self.runjob()
    def test_same_offset_wrong_owner_refuses(self):
        self.targets[1]['selector']['owner_container_id_u64']='0000000000000033'
        with self.assertRaisesRegex(ValueError,'exact audited chunk'):self.runjob()
    def test_empty_targets_refuse(self):
        self.job['targets']=[]
        with self.assertRaisesRegex(ValueError,'empty'):self.runjob()
    def test_source_sha_refuses(self):
        self.targets[1]['source']['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'source SHA'):self.runjob()
    def test_missing_owner_store_does_not_assume_zen_profile(self):
        self.job['targets']=self.targets[1:];r=self.runjob()
        self.assertEqual(r['exports'][0]['result'],'OWNER_STORE_REQUIRED');self.assertNotIn('decoded',r['exports'][0])
    def test_duplicate_target_refuses(self):
        self.job['targets']=self.targets+self.targets[:1]
        with self.assertRaisesRegex(ValueError,'duplicate'):self.runjob()
    def test_job_aggregate_bound_refuses(self):
        p=self.d/'large.bin';p.write_bytes(bytes(8*1024*1024+1));self.targets[0]['source']=ref(p)
        with self.assertRaisesRegex(ValueError,'8MiB'):self.runjob()

if __name__=='__main__':unittest.main()
