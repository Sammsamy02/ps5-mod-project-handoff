import hashlib,importlib.util,json,os
from pathlib import Path
import struct,subprocess,sys,tempfile,unittest
H=Path(__file__).resolve().parent
try:import manifest_v1 as m
except ImportError:m=None
def pack(fields):
 widths=['20s','20s','12s','II','Q','IIII','I'];b=struct.pack('<I',fields.get('version',1))
 for k,fmt in zip(['maps','hashes','ids','map_entries','codes','groups','indices'],widths):
  xs=fields[k];b+=struct.pack('<I',len(xs));b+=b''.join(struct.pack('<'+fmt,*(x if isinstance(x,tuple) else (x,))) for x in xs)
 return b

def fixture():
 return {'maps':[b'A'*20,b'B'*20],'hashes':[b'a'*20,b'b'*20,b'c'*20],'ids':[bytes.fromhex('010000000000000000000009')],'map_entries':[(0,2),(1,2)],'codes':[5,3|(8<<34),0|(4<<34)],'groups':[(0,3,12,12)],'indices':[2,0,1]}

class ParserTests(unittest.TestCase):
 def setUp(self):self.assertIsNotNone(m,'source-specified parser missing')
 def test_nonidentity_map_and_shared_overlapping_ranges(self):
  d=m.parse(pack(fixture()));self.assertEqual(d['maps'][0]['shader_indices'],[2,0]);self.assertEqual(d['maps'][1]['shader_indices'],[0,1]);self.assertEqual(d['maps'][0]['shader_hashes'],[(b'c'*20).hex(),(b'a'*20).hex()])
 def test_bitfields_and_group_spans_follow_offsets(self):
  d=m.parse(pack(fixture()));self.assertEqual([e['frequency_raw'] for e in d['shaders']],[5,3,0]);self.assertEqual([(e['shader_index'],e['offset'],e['end']) for e in d['groups'][0]['spans']],[(0,0,4),(2,4,8),(1,8,12)]);self.assertFalse(d['shader_payload_hashes_verified'])
 def test_truncation_and_count_bomb_refuse(self):
  for data in [pack(fixture())[:-1],struct.pack('<II',1,0xffffffff)]:
   with self.assertRaises(ValueError):m.parse(data)
 def test_version_trailing_and_empty_refuse(self):
  for data in [struct.pack('<I',2)+pack(fixture())[4:],pack(fixture())+b'!',struct.pack('<I',1)+bytes(28)]:
   with self.assertRaises(ValueError):m.parse(data)
 def test_parallel_count_mismatch(self):
  f=fixture();f['maps']=f['maps'][:1]
  with self.assertRaisesRegex(ValueError,'count'):m.parse(pack(f))
 def test_map_and_shader_index_ranges_refuse(self):
  for change in ['map','index']:
   f=fixture()
   if change=='map':f['map_entries'][0]=(2,2)
   else:f['indices'][0]=3
   with self.assertRaises(ValueError):m.parse(pack(f))
 def test_group_membership_and_group_index_refuse(self):
  f=fixture();f['codes'][0]|=1<<4
  with self.assertRaisesRegex(ValueError,'group'):m.parse(pack(f))
 def test_duplicate_offset_and_bad_sizes_refuse(self):
  for change in ['offset','size']:
   f=fixture()
   if change=='offset':f['codes'][2]=0
   else:f['groups']=[(0,3,8,8)]
   with self.assertRaises(ValueError):m.parse(pack(f))
 def test_compression_is_only_reported_not_attempted(self):
  f=fixture();f['groups']=[(0,3,12,7)];d=m.parse(pack(f));self.assertTrue(d['groups'][0]['compressed']);self.assertFalse(d['type9_read'])

class ReceiptTests(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(m,'source-specified parser missing');(H/'tmp').mkdir(exist_ok=True);self.tmp=tempfile.TemporaryDirectory(dir=H/'tmp');self.addCleanup(self.tmp.cleanup);self.d=Path(self.tmp.name)
  self.parser=H.parents[2]/'2026-10-08-l25-prep/pak-overlay/overlay_builder.py'
  # parents[2] is outputs; the actual pinned original parser, not a stub.
  s=importlib.util.spec_from_file_location('mv1_fixture_builder',self.parser);self.b=importlib.util.module_from_spec(s);s.loader.exec_module(self.b)
  self.payload=self.d/'chosen.bin';self.payload.write_bytes(pack(fixture()));n=self.payload.stat().st_size
  raw=bytearray((H.parent/'synthetic-intake-fixture/fixture.utoc').read_bytes());raw=raw[:178]+raw[190:];struct.pack_into('<I',raw,28,1);struct.pack_into('<I',raw,44,512);raw[156:166]=(0).to_bytes(5,'big')+n.to_bytes(5,'big');raw[166:178]=(0).to_bytes(5,'little')+n.to_bytes(3,'little')+n.to_bytes(3,'little')+bytes(1);raw[178:198]=self.b.blake3_20(self.payload.read_bytes())
  self.toc=self.d/'fixture.utoc';self.toc.write_bytes(raw);self.cid=raw[144:156].hex()
  def ref(p):return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
  self.r={'schema':1,'result':'LOCAL_PAYLOAD_VERIFIED','items_checked':1,'sources':{'parser':ref(self.parser),'toc':ref(self.toc),'ucas':{'path':'never-read.ucas','sha256':'0'*64}},'chunks':[{'chunk_id':self.cid,'toc_row':0,'chunk_type':8,'bytes':n,'sha256':ref(self.payload)['sha256'],'iohash_20':raw[178:198].hex(),'iohash_verified':True}]};self.rp=self.d/'receipt.json'
 def runreceipt(self):
  self.rp.write_text(json.dumps(self.r));return m.verify_and_parse(self.payload,self.rp,self.cid)
 def test_bound_synthetic_receipt_replay_no_ucas_read_sources_preserved(self):
  before=self.payload.read_bytes();d=self.runreceipt();self.assertEqual(d['metadata']['maps'][0]['shader_indices'],[2,0]);self.assertEqual(self.payload.read_bytes(),before);self.assertTrue(d['sources_unchanged'])
 def test_duplicate_or_non_type8_receipt_refuses(self):
  self.r['chunks']*=2
  with self.assertRaises(ValueError):self.runreceipt()
  self.r['chunks']=self.r['chunks'][:1];self.r['chunks'][0]['chunk_type']=9
  with self.assertRaises(ValueError):self.runreceipt()
 def test_wrong_payload_sha_and_iohash_refuse(self):
  self.r['chunks'][0]['sha256']='0'*64
  with self.assertRaises(ValueError):self.runreceipt()
  self.r['chunks'][0]['sha256']=hashlib.sha256(self.payload.read_bytes()).hexdigest();self.r['chunks'][0]['iohash_20']='0'*40
  with self.assertRaises(ValueError):self.runreceipt()
  self.r['chunks'][0]['iohash_20']=self.b.blake3_20(self.payload.read_bytes()).hex()
  self.payload.write_bytes(self.payload.read_bytes()[:-1]+b'!');self.r['chunks'][0]['sha256']=hashlib.sha256(self.payload.read_bytes()).hexdigest()
  with self.assertRaisesRegex(ValueError,'actual whole chunk BLAKE3'):self.runreceipt()
 def test_wrong_toc_row_and_parser_pin_refuse(self):
  self.r['chunks'][0]['toc_row']=1
  with self.assertRaises(ValueError):self.runreceipt()
  self.r['chunks'][0]['toc_row']=0;self.r['sources']['parser']['sha256']='0'*64
  with self.assertRaises(ValueError):self.runreceipt()
 def test_parser_replacement_after_hash_never_executes_changed_bytes(self):
  from unittest.mock import patch
  local=self.d/'local-parser.py';local.write_bytes(self.parser.read_bytes());self.r['sources']['parser']['path']=str(local)
  marker=self.d/'unverified-execution';original=m.importlib.util.spec_from_file_location
  def replace_after_hash(name,path,*args,**kwargs):
   if name=='manifest_v1_pinned_overlay':
    local.write_bytes(local.read_bytes()+("\nopen("+repr(str(marker))+", 'w').write('executed')\n").encode())
   return original(name,path,*args,**kwargs)
  with patch.object(m.importlib.util,'spec_from_file_location',side_effect=replace_after_hash):
   with self.assertRaises(ValueError):self.runreceipt()
  self.assertFalse(marker.exists(),'unverified replacement parser bytes executed')
 def test_cli_existing_output_refuses_without_clobber(self):
  self.rp.write_text(json.dumps(self.r));out=self.d/'already.json';out.write_text('keep')
  run=subprocess.run([sys.executable,str(H/'manifest_v1.py'),'--payload',str(self.payload),'--receipt',str(self.rp),'--chunk-id',self.cid,'--out',str(out)],capture_output=True,text=True,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
  self.assertNotEqual(run.returncode,0);self.assertEqual(out.read_text(),'keep')

if __name__=='__main__':unittest.main()
