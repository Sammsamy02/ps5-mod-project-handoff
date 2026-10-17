import hashlib,importlib.util,json,struct,sys,tempfile,unittest
from pathlib import Path
H=Path(__file__).resolve().parent
try:import group_bytes as g
except ImportError:g=None
CID='010000000000000000000009'
def pack(compressed=12,prefix=0):
 fields=[('20s',[b'A'*20]),('20s',[b'a'*20,b'b'*20,b'c'*20]),('12s',[bytes.fromhex(CID)]),('II',[(0,2)]),('Q',[5|(prefix<<34),3|(8<<34),4<<34]),('IIII',[(0,3,12,compressed)]),('I',[2,0,1])]
 b=struct.pack('<I',1)
 for fmt,xs in fields:b+=struct.pack('<I',len(xs))+b''.join(struct.pack('<'+fmt,*(x if isinstance(x,tuple) else (x,))) for x in xs)
 return b
class Tests(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(g,'group byte adapter missing');(H/'tmp').mkdir(exist_ok=True);self.t=tempfile.TemporaryDirectory(dir=H/'tmp');self.addCleanup(self.t.cleanup);self.d=Path(self.t.name)
  self.parser=H.parents[2]/'2026-10-08-l25-prep/pak-overlay/overlay_builder.py';s=importlib.util.spec_from_file_location('gb_fixture_overlay',self.parser);self.b=importlib.util.module_from_spec(s);exec(compile(self.parser.read_bytes(),str(self.parser),'exec'),self.b.__dict__)
  self.mp=self.d/'manifest.bin';self.gp=self.d/'group.bin';self.mr=self.d/'manifest-receipt.json';self.gr=self.d/'group-receipt.json';self.make()
 def make_receipt(self,p,receipt,cid,typ):
  n=p.stat().st_size;raw=bytearray((H.parent/'synthetic-intake-fixture/fixture.utoc').read_bytes());raw=raw[:178]+raw[190:];struct.pack_into('<I',raw,28,1);struct.pack_into('<I',raw,44,512);raw[144:156]=bytes.fromhex(cid);raw[156:166]=bytes(5)+n.to_bytes(5,'big');raw[166:178]=bytes(5)+n.to_bytes(3,'little')*2+bytes(1);raw[178:198]=self.b.blake3_20(p.read_bytes());toc=self.d/(p.stem+'.utoc');toc.write_bytes(raw)
  def ref(p):return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
  r={'schema':1,'result':'LOCAL_PAYLOAD_VERIFIED','items_checked':1,'sources':{'parser':ref(self.parser),'toc':ref(toc),'ucas':{'path':'NEVER-OPEN.ucas','sha256':'00'*32,'kind':'slice','physical_base':100}},'chunks':[{'chunk_id':cid,'toc_row':0,'chunk_type':typ,'bytes':n,'sha256':ref(p)['sha256'],'iohash_20':raw[178:198].hex(),'iohash_verified':True,'source_scope':'slice','source_physical_base':100}],'limits':'Synthetic local slice; no original full UCAS claim'}
  receipt.write_text(json.dumps(r));return {'toc_sha256':ref(toc)['sha256'],'container_id':raw[56:64].hex(),'toc_row':0}
 def make(self,compressed=12,prefix=0):
  self.mp.write_bytes(pack(compressed,prefix));self.gp.write_bytes(bytes(range(compressed)));self.mid='010000000000000000000008';self.make_receipt(self.mp,self.mr,self.mid,8);self.owner=self.make_receipt(self.gp,self.gr,CID,9)
 def runverify(self,indices=None,cid=CID,owner=None):return g.verify_group(self.mp,self.mr,self.mid,self.gp,self.gr,cid,self.owner if owner is None else owner,selected_shader_indices=indices or [])
 def mutate_receipt(self,edit):
  r=json.loads(self.gr.read_text());edit(r);self.gr.write_text(json.dumps(r))
 def test_sorted_disordered_views_whole_integrity_and_sources_preserved(self):
  before=self.gp.read_bytes();o=self.runverify();self.assertTrue(o['whole_buffer_verified']);self.assertEqual([v['shader_index'] for v in o['byte_views']],[0,2,1]);self.assertEqual([v['content_sha256'] for v in o['byte_views']],[hashlib.sha256(before[i:i+4]).hexdigest() for i in [0,4,8]]);self.assertFalse(o['writer_order_candidate']);self.assertFalse(o['stored_shader_hash_verified']);self.assertEqual(self.gp.read_bytes(),before)
 def test_prefix_unassigned_and_selected_nonidentity(self):
  self.make(prefix=2);o=self.runverify([2]);self.assertEqual(o['unassigned_prefix_bytes'],2);self.assertEqual([(v['offset'],v['end']) for v in o['byte_views']],[(4,8)])
 def test_compressed_buffer_verified_but_no_views(self):
  self.make(compressed=7);o=self.runverify([2]);self.assertTrue(o['whole_buffer_verified']);self.assertEqual(o['group_codec_status'],'CODEC_REQUIRED');self.assertEqual(o['byte_views'],[])
 def test_partial_length_refuses(self):
  self.gp.write_bytes(self.gp.read_bytes()[:-1])
  with self.assertRaises(ValueError):self.runverify()
 def test_receipt_sha_and_actual_iohash_refuse(self):
  self.mutate_receipt(lambda r:r['chunks'][0].update(sha256='00'*32))
  with self.assertRaises(ValueError):self.runverify()
  self.make();self.gp.write_bytes(b'x'*12);self.mutate_receipt(lambda r:r['chunks'][0].update(sha256=hashlib.sha256(self.gp.read_bytes()).hexdigest()))
  with self.assertRaisesRegex(ValueError,'BLAKE3'):self.runverify()
 def test_manifest_compressed_length_refuses_even_when_group_intact(self):
  self.mp.write_bytes(pack(7));self.make_receipt(self.mp,self.mr,self.mid,8)
  with self.assertRaisesRegex(ValueError,'compressed_size'):self.runverify()
 def test_owner_missing_wrong_container_same_offset_and_row_refuse(self):
  for owner in [{},dict(self.owner,container_id='ff'*8),dict(self.owner,toc_sha256='ff'*32),dict(self.owner,toc_row=1)]:
   with self.assertRaises(ValueError):self.runverify(owner=owner)
 def test_raw_c9_and_0c_refuse(self):
  for raw in ['c9','0c']:
   with self.assertRaisesRegex(ValueError,'raw09'):self.runverify(cid=CID[:-2]+raw)
 def test_duplicate_receipt_and_type_refuse(self):
  self.mutate_receipt(lambda r:(r['chunks'].append(dict(r['chunks'][0])),r.update(items_checked=2)))
  with self.assertRaises(ValueError):self.runverify()
  self.make();self.mutate_receipt(lambda r:r['chunks'][0].update(chunk_type=8))
  with self.assertRaises(ValueError):self.runverify()
 def test_member_selection_bound_unknown_duplicate_and_empty_explicit(self):
  for ids in [[99],[2,2],list(range(17))]:
   with self.assertRaises(ValueError):self.runverify(ids)
 def test_lookup_seed_zero_refuses(self):
  r=json.loads(self.gr.read_text());p=Path(r['sources']['toc']['path']);raw=bytearray(p.read_bytes());struct.pack_into('<I',raw,84,1);raw=raw[:178]+struct.pack('<i',0)+raw[178:];p.write_bytes(raw);r['sources']['toc']['sha256']=hashlib.sha256(raw).hexdigest();self.owner['toc_sha256']=r['sources']['toc']['sha256'];self.gr.write_text(json.dumps(r))
  with self.assertRaises(ValueError):self.runverify()
 def test_input_drift_after_manifest_refuses(self):
  from unittest.mock import patch
  original=g.load_manifest
  def mutate():
   m,ref=original();actual=m.verify_and_parse
   def wrapper(*a):
    out=actual(*a);self.mp.write_bytes(b'changed');return out
   m.verify_and_parse=wrapper;return m,ref
  with patch.object(g,'load_manifest',side_effect=mutate):
   with self.assertRaises(ValueError):self.runverify()
 def test_dependency_replacement_never_executes_changed_bytes(self):
  from unittest.mock import patch
  local=self.d/'manifest_reader.py';local.write_bytes(g.MANIFEST.read_bytes());marker=self.d/'bad';original=g.importlib.util.spec_from_file_location
  def replace(name,path):
   local.write_bytes(local.read_bytes()+('\nopen('+repr(str(marker))+",'w').write('bad')\n").encode());return original(name,path)
  with patch.object(g,'MANIFEST',local),patch.object(g.importlib.util,'spec_from_file_location',side_effect=replace):
   with self.assertRaises(ValueError):self.runverify()
  self.assertFalse(marker.exists())
 def test_output_refusal_and_bound(self):
  o=self.runverify();p=self.d/'out.json';g.write_output(o,p)
  with self.assertRaises(ValueError):g.write_output(o,p)
  with self.assertRaises(ValueError):g.write_output(o,H.parent/'outside.json')
  o['huge']='x'*(8*1024*1024)
  with self.assertRaises(ValueError):g.write_output(o,self.d/'large.json')
if __name__=='__main__':unittest.main()
