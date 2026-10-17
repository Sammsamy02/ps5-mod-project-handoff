import unittest
from pathlib import Path
H=Path(__file__).resolve().parent.parent
class NativeStage(unittest.TestCase):
 def test_actual_adapter_bound_reader_and_snapshot(self):
  s=(H/'PATCH/NativePakMountAdapter.inl').read_text()
  self.assertIn('PropertyReadBytes(',s)
  self.assertIn('bridge_code_cache_signature(',s)
  self.assertIn('snapshot_execute',s)
  self.assertIn('(*GMalloc)->Free(',s)
  self.assertIn('bridge.to_fstring()',s)
  self.assertNotIn('mounted_pak_count(uint64_t p',s)
 def test_current_source_patch_and_automatic_lifecycle(self):
  s=(H/'PATCH/current-source.patch').read_text()
  self.assertIn('NativePakMountAdapter.inl',s)
  self.assertIn('RunNativePakMountOnce',s)
  self.assertIn('early4::note_first_pump(true)',s)
  self.assertIn('PakMountCodeProfile.hpp',s)
if __name__=='__main__':unittest.main()
