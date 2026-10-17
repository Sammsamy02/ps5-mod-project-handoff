import struct,unittest
from extended_sections import array_census

def elf(extended=False,duplicate=False):
    names=b'\0.shstrtab\0.init_array\0.fini_array\0'
    count=5 if duplicate else 4;blob=bytearray(64+64*count+len(names)+24)
    blob[:6]=b'\x7fELF\x02\x01';struct.pack_into('<Q',blob,40,64)
    struct.pack_into('<HHH',blob,58,64,0 if extended else count,0xffff if extended else 1)
    rows=[(0,0,0,0,0,count if extended else 0,1 if extended else 0,0,0,0),
          (1,3,0,0,64+64*count,len(names),0,0,1,0),
          (11,14,0,0,len(blob)-24,8,0,0,8,8),
          (23,15,0,0,len(blob)-16,8,0,0,8,8)]
    if duplicate:rows.append((11,14,0,0,len(blob)-8,8,0,0,8,8))
    for i,row in enumerate(rows):struct.pack_into('<II4QII2Q',blob,64+i*64,*row)
    blob[64+64*count:64+64*count+len(names)]=names
    return blob
class Sections(unittest.TestCase):
 def test_normal(self):self.assertEqual(array_census(elf())['.init_array']['slots'],1)
 def test_extended(self):self.assertEqual(array_census(elf(True))['.fini_array']['slots'],1)
 def test_duplicate_names_not_lost(self):self.assertEqual(array_census(elf(True,True))['.init_array']['slots'],2)
 def test_legacy_ctor_alias(self):
  b=elf(True,True);needle=b'.init_array';start=b.find(needle);b[start:start+len(needle)]=b'.ctors'+b'\0'*(len(needle)-6)
  self.assertEqual(array_census(b)['.init_array']['slots'],2)
 def test_bad_extended_count(self):
  b=elf(True);struct.pack_into('<Q',b,64+32,900)
  with self.assertRaises(ValueError):array_census(b)
 def test_bad_name_index(self):
  b=elf(True);struct.pack_into('<I',b,64+40,900)
  with self.assertRaises(ValueError):array_census(b)
 def test_truncated_section(self):
  with self.assertRaises(ValueError):array_census(elf(True)[:-30])
if __name__=='__main__':unittest.main()
