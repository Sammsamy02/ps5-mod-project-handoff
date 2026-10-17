#!/usr/bin/env python3
import sys
sys.dont_write_bytecode = True
import unittest, importlib.util, hashlib, tempfile, re
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('observer_stager',HERE/'stage_observer.py')
stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
class SourceStageTests(unittest.TestCase):
 def test_cumulative_stage_and_inverse(self):
  raw=stage.SOURCE.read_bytes(); derived=stage.transform(raw)
  self.assertTrue(raw != derived, "cumulative transformation missing")
  self.assertEqual(stage.inverse(derived),raw)
  self.assertEqual(hashlib.sha256(raw).hexdigest(),stage.SOURCE_SHA)
  for marker in ['Cap10BridgeAdmissionScope admission{this, m_shutdown_requested.get()}', 'std::atomic<std::uint64_t> issued', 'compare_exchange_weak', 'WindowKind::NoNativeCalls', 'record_entry(Call::Abi', 'record_exit(Call::Abi', 'record_entry(Call::Probe', 'finish_probe(-4, false)', 'finish_probe(-3, false)', 'finish_probe(0, true)', 'finish_probe(-5, false)']:
   self.assertIn(marker,derived.decode())
 def test_empty_drift_reapply_refused(self):
  raw=stage.SOURCE.read_bytes()
  for bad in [b'',raw+b' ',stage.transform(raw)]:
   with self.assertRaises(ValueError):stage.transform(bad)
 def test_anchor_missing_duplicate_refused_even_with_matching_test_hash(self):
  raw=stage.SOURCE.read_bytes(); saved=stage.SOURCE_SHA
  try:
   for old,new in [(stage.INCLUDE,b''),(stage.INIT,stage.INIT*2),(stage.CALLBACK,b'')]:
    changed=raw.replace(old,new);stage.SOURCE_SHA=hashlib.sha256(changed).hexdigest()
    with self.assertRaises(ValueError):stage.transform(changed)
  finally:stage.SOURCE_SHA=saved
 def test_new_write_overwrite_outside_and_live_refused(self):
  with tempfile.TemporaryDirectory(dir=HERE) as tmp:
   target=Path(tmp)/'derived.cpp';stage.write_derived(target,b'isolated');self.assertEqual(target.read_bytes(),b'isolated')
   with self.assertRaises(ValueError):stage.write_derived(target,b'overwrite')
   with self.assertRaises(ValueError):stage.write_derived(HERE.parent/'outside.cpp',b'outside')
   with self.assertRaises(ValueError):stage.write_derived(stage.SOURCE,b'live')
 def test_header_and_recording_contract_source(self):
  self.assertTrue((HERE/'Cap10InvocationObservation.hpp').exists(), 'internal interface missing')
  h=(HERE/'Cap10InvocationObservation.hpp').read_text(); f=(HERE/'observer_fragment.inc').read_text()
  for s in ['RunImports','NoNativeCalls','noexcept','std::uint64_t serial','const void* provider_tls']:
   self.assertIn(s,h)
  for s in ['if (window.active)', 'window.poisoned = true', 'value == std::numeric_limits<std::uint64_t>::max()', 'window.admission == cap10_bridge_state.admission', 'cancel_window();', 'window = {};', 'expected_cookie', 'probe_reply_valid', 'state.shutdown->load(std::memory_order_acquire)']:
   self.assertIn(s,f)
  self.assertNotIn('mono_runtime_invoke',f)
  self.assertIsNone(re.search(r'\bs_program\b',f))
if __name__=='__main__':unittest.main()
