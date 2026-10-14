import struct
import unittest
try:
    from zen_identity import decode_package
except ImportError:
    def decode_package(*a,**k): return {'exports':[]}

def fixture():
    names=[b'/Any/P',b'Asset']; batch=struct.pack('<IIQ',2,sum(map(len,names)),0xc1640000)+bytes(16)+b''.join(len(n).to_bytes(2,'big') for n in names)+b''.join(names)
    start=60+len(batch); end=start+72
    summary=struct.pack('<6I9i',0,end,0,0,0,end,start,start,start,end,end,end,end,end,end)
    entry=struct.pack('<QQIIQQQQQIB3s',end,0,1,0,(1<<64)-1,(1<<62)|123,(1<<64)-1,(1<<64)-1,0x1122334455667788,1,0,bytes(3))
    return summary+batch+entry

class ZenTests(unittest.TestCase):
    def test_exports_decoded_but_script_class_unresolved(self):
        d=decode_package(fixture(),5)
        self.assertEqual(d['exports'][0]['name'],'Asset')
        self.assertEqual(d['exports'][0]['public_export_hash'],'1122334455667788')
        self.assertEqual(d['exports'][0]['class_ref']['kind'],'ScriptImport')
        self.assertFalse(d['exports'][0]['class_ref']['class_identity_resolved'])
    def test_bad_export_map_stride_refuses(self):
        b=bytearray(fixture()); struct.pack_into('<i',b,52,len(b)-1)
        with self.assertRaises(ValueError): decode_package(bytes(b),5)
    def test_invalid_mapped_name_refuses(self):
        b=bytearray(fixture()); off=struct.unpack_from('<i',b,32)[0];struct.pack_into('<I',b,off+16,900)
        with self.assertRaises(ValueError): decode_package(bytes(b),5)
    def test_wrong_version_or_versioned_summary_refuses(self):
        with self.assertRaises(ValueError):decode_package(fixture(),4)
        b=bytearray(fixture());struct.pack_into('<I',b,0,1)
        with self.assertRaises(ValueError):decode_package(bytes(b),5)
    def test_exact_sixty_byte_input_refuses_as_value_error(self):
        b=bytearray(60)
        struct.pack_into('<6I9i',b,0,0,60,0,0,0,60,*([60]*9))
        with self.assertRaisesRegex(ValueError,'package too short for name-count field'): decode_package(bytes(b),5)
    def test_header_end_before_name_count_refuses_as_value_error(self):
        b=bytearray(fixture()); struct.pack_into('<I',b,4,62)
        with self.assertRaisesRegex(ValueError,'header ends before name-count field'):decode_package(bytes(b),5)
    def test_truncation_refuses(self):
        with self.assertRaises(ValueError): decode_package(fixture()[:80],5)

if __name__=='__main__':unittest.main()
