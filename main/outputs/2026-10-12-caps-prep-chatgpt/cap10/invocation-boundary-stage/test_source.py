#!/usr/bin/env python3
import sys
sys.dont_write_bytecode=True
import unittest,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
class BoundarySourceChecks(unittest.TestCase):
 def code(self):
  self.assertTrue((HERE/'InvocationBoundary.cpp').exists(),'actual invocation helper absent')
  return (HERE/'InvocationBoundary.cpp').read_text()
 def test_real_public_gc_pair_and_no_host_probes(self):
  s=self.code()
  for word in ['mono_threads_enter_gc_unsafe_region(&stack_marker)', 'mono_threads_exit_gc_unsafe_region(cookie, &stack_marker)', 'MODBRIDGE_CAP10_ADMITTED_MONO_PORT', '#error']:
   self.assertIn(word,s)
  for banned in ['mono_thread_current','mono_thread_attach','get_program','mono_jit_init','mono_jit_cleanup']:
   self.assertNotIn(banned,s)
 def test_real_invocations_inside_both_window_kinds(self):
  s=self.code();self.assertEqual(s.count('mono_runtime_invoke('),2)
  self.assertRegex(s,r'WindowKind::RunImports[\s\S]+mono_runtime_invoke\(method, nullptr, params, &exception\)')
  self.assertRegex(s,r'WindowKind::NoNativeCalls[\s\S]+mono_runtime_invoke\(method, nullptr, nullptr, &exception\)')
  self.assertLess(s.index('const bool threw = exception != nullptr'),s.index('window.finish(out.native)'))
 def test_actual_pinned_handle_ownership_and_class_before_unbox(self):
  s=self.code()
  for word in ['mono_gchandle_new(object, true)','owned = true','if (owned) mono_gchandle_free(value)','mono_gchandle_get_target(value)','mono_object_get_class(target) != int32_class','mono_object_get_class(target) == expected_exception','std::memcpy(&out.value, storage, sizeof(out.value))']:
   self.assertIn(word,s)
  self.assertLess(s.index('mono_object_get_class(target) != int32_class'),s.index('mono_object_unbox(target)'))
  handle=s[s.index('struct PinnedHandle'):s.index('struct NativeWindow')]
  self.assertNotIn('value == 0',handle)
 def test_no_arity_signature_claim_or_ambient_resolver(self):
  s=self.code();h=(HERE/'InvocationBoundary.hpp').read_text()
  for word in ['mono_class_get_image(probe) != image','mono_method_get_class(run) != probe','mono_method_get_class(throw_method) != probe','HARD PRECONDITIONS','signature']:
   self.assertIn(word,s+h)
  for banned in ['dlsym','__Internal','mono_dl_fallback_register','mono_class_get_method_from_name','mono_signature_']:
   self.assertNotIn(banned,s)
 def test_actual_recovery_and_cleanup_source(self):
  s=self.code()
  for word in ['first_cookie == recovery_cookie','invoke_run(run, first_cookie','invoke_throw(throw_method','invoke_run(run, recovery_cookie','~GcRegion() noexcept','~PinnedHandle() noexcept','~NativeWindow() noexcept','Cap10Observation::abort(token)']:
   self.assertIn(word,s)
  self.assertLess(s.index('invoke_throw(throw_method'),s.index('invoke_run(run, recovery_cookie'))
if __name__=='__main__':unittest.main()
