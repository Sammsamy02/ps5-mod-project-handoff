import sys
sys.dont_write_bytecode=True
import hashlib,importlib.util,tempfile,unittest
from pathlib import Path
H=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('producer_stage',H/'stage.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class StageTests(unittest.TestCase):
 def test_real_pinned_inputs_stage_only_and_inverse_exact(self):
  originals={k:p.read_bytes() for k,p in m.SOURCES.items()}
  for key,raw in originals.items():
   derived=m.transform(key,raw)
   self.assertNotEqual(raw,derived)
   self.assertEqual(m.inverse(key,derived),raw)
   self.assertEqual(m.SOURCES[key].read_bytes(),raw)
  dw=m.transform('writer',originals['writer']).decode();pg=m.transform('harness',originals['harness']).decode()
  self.assertIn('SymbolValue(resolution, importByName, SectionAddr, obj, sym, linkerDefined)',dw)
  self.assertIn('resolution.DroppedSections.Contains',dw)
  self.assertIn('sym.Size > section.Size - sym.Value',dw)
  self.assertIn('bare_elf_sha256',pg)
  self.assertIn('Assembly.GetExecutingAssembly().Location',pg)
 def test_optional_sink_preserves_the_original_final_writer_call(self):
  raw=m.SOURCES['writer'].read_text();derived=m.transform('writer',raw.encode()).decode()
  old_call=raw.split('        return WriteFile(',1)[1].split(m.TAIL,1)[0]+m.TAIL
  new_call=derived.split('        byte[] finalBytes = WriteFile(',1)[1].split(m.TAIL,1)[0]+m.TAIL
  self.assertEqual(old_call,new_call)
  self.assertIn('finalSymbolSink = null)',derived)
  self.assertIn('if (finalSymbolSink is not null)',m.POST)
  self.assertTrue(m.POST.rstrip().endswith('return finalBytes;'))
  self.assertNotIn('finalBytes[',m.POST)
  self.assertNotIn('WriteFile(',m.POST)
 def test_final_ranges_refuse_unbounded_and_nonfunction_candidates(self):
  derived=m.transform('writer',m.SOURCES['writer'].read_bytes()).decode()
  for guard in ('sym.IsUndefined', 'sym.Type != SymType.Func', 'sym.Size == 0',
                'resolution.DroppedSections.Contains((obj, sym.SectionIndex))',
                '!section.IsAlloc || !section.IsExecutable || section.IsNoBits',
                'sym.Value > section.Size || sym.Size > section.Size - sym.Value',
                'sym.Size > ulong.MaxValue - address'):
   self.assertIn(guard,derived)
  self.assertIn('ulong address = SymbolValue(resolution, importByName, SectionAddr, obj, sym, linkerDefined);',derived)
  self.assertNotIn('reconstruct_from_map',derived)
 def test_producer_hashes_exact_persisted_buffer_and_remains_package_unbound(self):
  derived=m.transform('harness',m.SOURCES['harness'].read_bytes()).decode()
  self.assertLess(derived.index(m.WRITE),derived.index('byte[] producerBytes ='))
  self.assertIn('bare_elf_sha256 = Convert.ToHexStringLower(SHA256.HashData(bytes))',derived)
  self.assertIn('UNBOUND; owner must validate transforms and attach exact packaged PRX SHA',derived)
  self.assertIn('File.Exists(output + ".final-symbols.json")',derived)
  self.assertIn('FileMode.CreateNew',m.HARNESS_POST)
 def test_source_drift_refuses_before_output(self):
  for key,p in m.SOURCES.items():
   with self.assertRaisesRegex(ValueError,'source hash'):m.transform(key,p.read_bytes()+b'\n')
 def test_missing_or_duplicate_exact_anchor_refuses(self):
  for key in m.SOURCES:
   text=m.SOURCES[key].read_text();old,new=m.EDITS[key][0]
   for broken in (text.replace(old,'',1),text.replace(old,old+old,1)):
    with self.assertRaisesRegex(ValueError,'anchor count'):m.replace_exact(broken,m.EDITS[key])
if __name__=='__main__':unittest.main()
