import contextlib,hashlib,io,json,struct,tempfile,unittest
from pathlib import Path
try:import package_binding as b
except ImportError:b=None
H=Path(__file__).resolve().parent;R=H.parents[3]
WRAPPER=R/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/install_app/make_fself.py'
def elf():
 d=bytearray(512);ident=b'\x7fELF\x02\x01\x01'+bytes(9)
 struct.pack_into('<16sHHIQQQIHHHHHH',d,0,ident,0xfe18,62,1,0x4010,64,0,0,64,56,2,64,0,0)
 struct.pack_into('<II6Q',d,64,1,5,256,0x4000,0x4000,32,32,16)
 struct.pack_into('<II6Q',d,120,1,6,320,0x8000,0x8000,16,16,16)
 d[256:288]=bytes(range(32));return bytes(d)
def genuine_wrap(data):
 ns={'__name__':'synthetic_wrapper','__file__':str(WRAPPER)};exec(compile(WRAPPER.read_bytes(),str(WRAPPER),'exec'),ns)
 e=ns['ElfFile'](ignore_shdrs=True);e.load(io.BytesIO(data));out=io.BytesIO()
 with contextlib.redirect_stdout(io.StringIO()):ns['SignedElfFile'](e).save(out)
 return out.getvalue()
class Tests(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(b,'package-binding verifier absent');self.t=tempfile.TemporaryDirectory(dir=H/'tmp');self.addCleanup(self.t.cleanup);self.d=Path(self.t.name)
  self.original=self.d/'bare.prx';self.sdk=self.d/'sdk.prx';self.pkg=self.d/'pkg.prx';self.receipt=self.d/'producer.json'
  self.original.write_bytes(elf());s=bytearray(elf());s[320]=99;self.sdk.write_bytes(s);self.pkg.write_bytes(genuine_wrap(bytes(s)))
  self.p={'schema':1,'kind':'final-layout-symbol-producer','bare_elf_sha256':hashlib.sha256(elf()).hexdigest(),'bare_elf_bytes':512,'address_convention':'final ELF virtual address; runtime module-relative only after base convention validation','input_object_sha256':'11'*32,'contract_sha256':'22'*32,'packager_assembly_sha256':'33'*32,'link_writer_assembly_sha256':'44'*32,'functions':[{'name':'actual_declared_function','addr':'0x4010','size':'0x8','tier':'FINAL-LINK-BOUNDED-FUNCTION'}],'runtime_execution_proven':False,'packaged_prx_binding':'UNBOUND; owner must validate transforms and attach exact packaged PRX SHA','coverage':'retained named defined executable FUNC symbols with nonzero bounded size only; aliases retained; zero-size/absolute/discarded/linker-generated symbols excluded'}
 def verify(self):
  self.receipt.write_text(json.dumps(self.p));return b.verify(self.receipt,hashlib.sha256(self.receipt.read_bytes()).hexdigest(),self.original,self.sdk,self.pkg)
 def test_nonexecuting_metadata_change_preserves_final_vma_without_runtime_promotion(self):
  o=self.verify();self.assertEqual(o['label'],'ELF_LAYOUT_BOUND_ONLY');self.assertEqual(o['functions'][0]['addr'],'0x4010');self.assertFalse(o['runtime_base_convention_proven']);self.assertFalse(o['capture_build_pairing']);self.assertEqual(o['symbols_admitted'],1);self.assertEqual(o['executable_loads'][0]['vaddr'],0x4000)
 def test_sdk_executable_mutation_refuses(self):
  d=bytearray(self.sdk.read_bytes());d[260]^=1;self.sdk.write_bytes(d);self.pkg.write_bytes(genuine_wrap(bytes(d)))
  with self.assertRaisesRegex(ValueError,'executable'):self.verify()
 def test_sdk_vma_or_flags_change_refuses(self):
  for offset,value,fmt in [(80,0x5000,'Q'),(68,7,'I')]:
   d=bytearray(elf());struct.pack_into('<'+fmt,d,offset,value);self.sdk.write_bytes(d);self.pkg.write_bytes(genuine_wrap(bytes(d)))
   with self.assertRaises(ValueError):self.verify()
 def test_packaged_mutation_or_appended_bytes_refuse(self):
  original=self.pkg.read_bytes()
  for d in [original[:-1],original+b'x',original[:900]+bytes([original[900]^1])+original[901:]]:
   self.pkg.write_bytes(d)
   with self.assertRaisesRegex(ValueError,'wrapper'):self.verify()
 def test_receipt_hash_length_schema_and_range_refuse(self):
  for changes in [{'bare_elf_sha256':'00'*32},{'bare_elf_bytes':511},{'schema':2},{'functions':[]},{'functions':[{'name':'bad','addr':'0x8000','size':'0x4','tier':'FINAL-LINK-BOUNDED-FUNCTION'}]},{'functions':[{'name':'bad','addr':'0x401f','size':'0x2','tier':'FINAL-LINK-BOUNDED-FUNCTION'}]},{'functions':[{'name':'bad','addr':'0x4010','size':'0x0','tier':'FINAL-LINK-BOUNDED-FUNCTION'}]},{'functions':[{'name':'bad','addr':'0x4010','size':'0x4','tier':'RECONSTRUCTED'}]}]:
   old=dict(self.p);self.p.update(changes)
   with self.assertRaises(ValueError):self.verify()
   self.p=old
 def test_truncated_or_overlapping_loads_refuse(self):
  d=bytearray(elf());struct.pack_into('<Q',d,136,0x4010)
  for raw in [elf()[:100],bytes(d)]:
   with self.assertRaises(ValueError):b.elf_loads(raw)
 def test_overlapping_file_backed_loads_refuse(self):
  d=bytearray(elf());struct.pack_into('<Q',d,128,272)
  with self.assertRaisesRegex(ValueError,'overlapping'):b.elf_loads(bytes(d))
 def test_executable_only_zero_flags_preserve_and_unknown_bits_refuse(self):
  d=bytearray(elf());struct.pack_into('<I',d,68,1);struct.pack_into('<I',d,124,0)
  loads=b.elf_loads(bytes(d));self.assertEqual([x['flags'] for x in loads],[1,0])
  struct.pack_into('<I',d,68,9)
  with self.assertRaisesRegex(ValueError,'flags'):b.elf_loads(bytes(d))
 def test_source_recheck_detects_mutation_and_wrong_pin(self):
  refs={};p=self.d/'small';p.write_bytes(b'good');b.read_stable(p,32,refs);p.write_bytes(b'evil')
  with self.assertRaises(ValueError):b.recheck(refs)
  with self.assertRaises(ValueError):b.read_stable(p,32,{},'00'*32)
 def test_wrong_producer_pin_and_duplicate_json_refuse(self):
  self.receipt.write_text(json.dumps(self.p))
  with self.assertRaises(ValueError):b.verify(self.receipt,'00'*32,self.original,self.sdk,self.pkg)
  self.receipt.write_text('{"schema":1,"schema":1}')
  with self.assertRaisesRegex(ValueError,'duplicate'):b.verify(self.receipt,hashlib.sha256(self.receipt.read_bytes()).hexdigest(),self.original,self.sdk,self.pkg)
 def test_output_nooverwrite_or_outside_folder(self):
  o=self.verify();p=self.d/'result.json';b.write_output(o,p)
  with self.assertRaises(ValueError):b.write_output(o,p)
  with self.assertRaises(ValueError):b.write_output(o,H.parent/'outside.json')
 def test_layout_only_requires_real_producer_separately(self):
  o=b.verify_layout(self.original,self.sdk,self.pkg);self.assertEqual(o['producer_status'],'INPUT_REQUIRED');self.assertEqual(o['symbols_admitted'],0)
if __name__=='__main__':unittest.main()
