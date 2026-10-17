import unittest
try:
    from shader_catalogue import shader_rows
except ImportError:
    def shader_rows(*a): return []

class FixtureToc:
    ids=(bytes.fromhex('010000000000000000000008'),bytes.fromhex('020000000000000000000009'),bytes.fromhex('030000000000000000000001'))
    block_size=64; block_count=3
    def offset_length(self,row): return [(48,32),(128,10),(0,4)][row]
    def block(self,i): return [(1000,64,64,0),(2000,32,32,0),(3000,10,10,0)][i]
    def meta(self,row): return bytes([row+1])*20+bytes(4)

class CatalogueTests(unittest.TestCase):
    def test_logical_span_is_not_physical_offset(self):
        rows=shader_rows(FixtureToc())
        self.assertEqual(len(rows),2)
        self.assertEqual(rows[0]['logical_offset'],48)
        self.assertEqual(rows[0]['blocks'][0]['physical_block_offset'],1000)
        self.assertEqual(rows[0]['blocks'][1]['physical_block_offset'],2000)
        self.assertEqual(rows[0]['type_name'],'ShaderCodeLibrary')
        self.assertNotIn('shader_instruction_offset',rows[0])
    def test_chunk_outside_block_descriptor_refuses(self):
        toc=FixtureToc(); toc.offset_length=lambda row:(190,10)
        with self.assertRaises(ValueError): shader_rows(toc)
    def test_empty_toc_refuses(self):
        toc=FixtureToc(); toc.ids=()
        with self.assertRaises(ValueError): shader_rows(toc)

if __name__=='__main__': unittest.main()
