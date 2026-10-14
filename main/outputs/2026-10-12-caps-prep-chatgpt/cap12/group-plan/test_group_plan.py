import hashlib,importlib.util,json,os,struct,subprocess,sys,tempfile,unittest
from pathlib import Path
H=Path(__file__).resolve().parent
try:import group_plan as g
except ImportError:g=None
CID='010000000000000000000009'
def pack(compressed=12,ids=None):
 fields=[('20s',[b'A'*20]),('20s',[b'a'*20,b'b'*20,b'c'*20]),('12s',[bytes.fromhex(ids or CID)]),('II',[(0,2)]),('Q',[5,3|(8<<34),4<<34]),('IIII',[(0,3,12,compressed)]),('I',[2,0,1])]
 b=struct.pack('<I',1)
 for fmt,xs in fields:
  b+=struct.pack('<I',len(xs))+b''.join(struct.pack('<'+fmt,*(x if isinstance(x,tuple) else (x,))) for x in xs)
 return b
class Tests(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(g,'group planner missing');(H/'tmp').mkdir(exist_ok=True);self.t=tempfile.TemporaryDirectory(dir=H/'tmp');self.addCleanup(self.t.cleanup);self.d=Path(self.t.name)
  self.parser=H.parents[2]/'2026-10-08-l25-prep/pak-overlay/overlay_builder.py'
  spec=importlib.util.spec_from_file_location('gp_fixture',self.parser);self.b=importlib.util.module_from_spec(spec);exec(compile(self.parser.read_bytes(),str(self.parser),'exec'),self.b.__dict__)
  self.payload=self.d/'chosen.bin';self.toc=self.d/'fixture.utoc';self.receipt=self.d/'receipt.json';self.cat=self.d/'catalogue.json';self.make_payload()
  self.row={'toc_row':3,'chunk_id':CID,'chunk_type_raw':9,'chunk_type':9,'logical_offset':64,'logical_length':12,'metadata_content_hash_20_bytes':'ab'*20,'metadata_flags':0,'blocks':[{'block_index':1,'physical_block_offset':32,'stored_bytes':12,'decoded_bytes':12,'compression_method_index':0,'logical_slice_begin_in_decoded_block':0,'logical_slice_end_in_decoded_block':12}]}
  self.owner={'source':{'path':'discovery-only.utoc','sha256':'11'*32,'bytes':200},'toc_version':8,'container_id_bytes_hex':'22'*8,'shader_related_rows':[self.row]};self.catalogue={'schema':1,'inputs':[self.owner]}
 def make_payload(self,compressed=12,ids=None,data=None):
  self.payload.write_bytes(pack(compressed,ids) if data is None else data);n=self.payload.stat().st_size
  raw=bytearray((H.parent/'synthetic-intake-fixture/fixture.utoc').read_bytes());raw=raw[:178]+raw[190:];struct.pack_into('<I',raw,28,1);struct.pack_into('<I',raw,44,512);raw[156:166]=bytes(5)+n.to_bytes(5,'big');raw[166:178]=bytes(5)+n.to_bytes(3,'little')*2+bytes(1);raw[178:198]=self.b.blake3_20(self.payload.read_bytes());self.toc.write_bytes(raw);self.type8id=raw[144:156].hex()
  def ref(p):return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
  self.receipt.write_text(json.dumps({'schema':1,'result':'LOCAL_PAYLOAD_VERIFIED','items_checked':1,'sources':{'parser':ref(self.parser),'toc':ref(self.toc)},'chunks':[{'chunk_id':self.type8id,'toc_row':0,'chunk_type':8,'bytes':n,'sha256':ref(self.payload)['sha256'],'iohash_20':raw[178:198].hex(),'iohash_verified':True}]}))
 def runplan(self,maps=None,groups=None,owners=None,pin=None):
  self.cat.write_text(json.dumps(self.catalogue));return g.plan(self.payload,self.receipt,self.type8id,self.cat,pin or hashlib.sha256(self.cat.read_bytes()).hexdigest(),maps or [],groups if groups is not None else [CID],owners or {})
 def test_map_nonidentity_and_disordered_not_writer_admitted(self):
  p=self.runplan(maps=[(b'A'*20).hex()],groups=[]);r=p['groups'][0];self.assertEqual(r['members'][0]['shader_index'],2);self.assertEqual(len(r['members']),3);self.assertFalse(r['writer_layout_candidate']);self.assertEqual(r['identity_status'],'ID_DERIVATION_UNCHECKED');self.assertEqual(r['owner']['toc_row'],3)
 def test_raw09_only(self):
  for ending in ['c9','0c']:
   cid=CID[:-2]+ending;self.make_payload(ids=cid)
   with self.assertRaisesRegex(ValueError,'raw09'):self.runplan(groups=[cid])
 def test_missing_owner(self):
  self.owner['shader_related_rows']=[];self.assertEqual(self.runplan()['groups'][0]['status'],'INPUT_REQUIRED')
 def test_ambiguous_requires_exact_owner(self):
  other=json.loads(json.dumps(self.owner));other['container_id_bytes_hex']='33'*8;self.catalogue['inputs'].append(other)
  self.assertEqual(self.runplan()['groups'][0]['status'],'OWNER_SELECTION_REQUIRED')
  p=self.runplan(owners={CID:{'toc_sha256':'11'*32,'container_id':'33'*8,'toc_row':3}});self.assertEqual(p['groups'][0]['owner']['container_id'],'33'*8)
 def test_same_offset_wrong_owner_refuses(self):
  with self.assertRaisesRegex(ValueError,'owner selection'):self.runplan(owners={CID:{'toc_sha256':'ff'*32,'container_id':'22'*8,'toc_row':3}})
 def test_length_matches_compressed_not_uncompressed(self):
  self.make_payload(7)
  with self.assertRaisesRegex(ValueError,'compressed size'):self.runplan()
  self.row['logical_length']=7;self.row['blocks'][0].update(stored_bytes=7,decoded_bytes=7,logical_slice_end_in_decoded_block=7);self.assertEqual(self.runplan()['groups'][0]['group_codec_status'],'CODEC_REQUIRED')
 def test_transport_separate_from_group_codec(self):
  self.row['blocks'][0]['compression_method_index']=1;p=self.runplan()['groups'][0];self.assertEqual(p['transport_status'],'TRANSPORT_CODEC_REQUIRED');self.assertEqual(p['group_codec_status'],'STORED_SIZE_PROFILE_ONLY')
 def test_empty_unknown_and_selection_bound(self):
  for maps,groups in [([],[]),([],['00'*11+'09']),(['00'*20],[]),([(b'A'*20).hex()]*17,[])]:
   with self.assertRaises(ValueError):self.runplan(maps,groups)
 def test_group_size_bound(self):
  self.make_payload(4*1024*1024+1)
  with self.assertRaisesRegex(ValueError,'size bound'):self.runplan()
 def test_total_selected_size_bound(self):
  ids=[bytes.fromhex(f'{i:022x}09') for i in range(3)]
  fields=[('20s',[b'A'*20]),('20s',[b'a'*20,b'b'*20,b'c'*20]),('12s',ids),('II',[(0,3)]),('Q',[0,16,32]),('IIII',[(i,1,3*1024*1024,3*1024*1024) for i in range(3)]),('I',[0,1,2])]
  data=struct.pack('<I',1)
  for fmt,xs in fields:data+=struct.pack('<I',len(xs))+b''.join(struct.pack('<'+fmt,*(x if isinstance(x,tuple) else (x,))) for x in xs)
  self.make_payload(data=data)
  with self.assertRaisesRegex(ValueError,'size bound'):self.runplan(maps=[(b'A'*20).hex()],groups=[])
 def test_output_cap(self):
  p=self.runplan();p['huge']='x'*(8*1024*1024)
  with self.assertRaisesRegex(ValueError,'output bound'):g.write_output(p,self.d/'large.json')
 def test_pin_and_cap(self):
  with self.assertRaisesRegex(ValueError,'SHA'):self.runplan(pin='00'*32)
  huge=self.d/'huge';huge.write_bytes(b'x'*(8*1024*1024+1))
  with self.assertRaises(ValueError):g.stable_read(huge,8*1024*1024)
 def test_drift_refuses(self):
  from unittest.mock import patch
  original=g.load_manifest
  def changed(*a):
   m=original(*a);self.cat.write_text('changed');return m
  with patch.object(g,'load_manifest',side_effect=changed):
   with self.assertRaises(ValueError):self.runplan()
 def test_output_refusal_and_preservation(self):
  p=self.runplan();before=self.payload.read_bytes();out=self.d/'out.json';g.write_output(p,out);saved=out.read_bytes()
  with self.assertRaises(ValueError):g.write_output(p,out)
  with self.assertRaises(ValueError):g.write_output(p,H.parent/'outside.json')
  self.assertEqual(self.payload.read_bytes(),before);self.assertEqual(out.read_bytes(),saved)
if __name__=='__main__':unittest.main()
