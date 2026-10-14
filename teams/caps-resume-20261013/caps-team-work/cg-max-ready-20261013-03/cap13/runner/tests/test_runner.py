import importlib.util,pathlib,sys,tempfile,unittest
P=pathlib.Path(__file__).resolve().parents[1]/'runner.py'
spec=importlib.util.spec_from_file_location('runner',P)
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
GOOD="print('CHECK: observed behavior PASS');print('RESULT: PASS (1 checks, 0 failed)')"
class RunnerTests(unittest.TestCase):
 def case(self,code=GOOD,**kw):return dict(id='test',argv=[sys.executable,'-c',code],timeout=1,min_checks=1,**kw)
 def run_case(self,case):
  with tempfile.TemporaryDirectory() as td:return runner.run_case(case,pathlib.Path(td))
 def test_success_requires_observation(self):self.assertEqual(self.run_case(self.case())['status'],'PASS')
 def test_nonzero_child_cannot_pass(self):self.assertEqual(self.run_case(self.case(GOOD+';sys=__import__("sys");sys.exit(7)'))['status'],'FAIL')
 def test_no_checks_cannot_pass(self):self.assertEqual(self.run_case(self.case("print('RESULT: PASS (0 checks, 0 failed)')"))['status'],'FAIL')
 def test_failed_check_cannot_pass(self):self.assertEqual(self.run_case(self.case(GOOD+";print('CHECK: behavior FAIL')"))['status'],'FAIL')
 def test_inconsistent_summary_cannot_pass(self):self.assertEqual(self.run_case(self.case("print('CHECK: observed behavior PASS');print('RESULT: PASS (0 checks, 0 failed)')"))['status'],'FAIL')
 def test_unrecognized_failed_check_cannot_pass(self):
  code="print('CHECK: failure FAIL: unresolved import');print('CHECK: harmless PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code))['status'],'FAIL')
 def test_embedded_fail_word_cannot_be_counted_as_pass(self):
  code="print('CHECK: operation FAIL PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code))['status'],'FAIL')
 def test_embedded_check_marker_cannot_be_ignored(self):
  code="print('WARNING CHECK: operation FAIL');print('CHECK: harmless PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code))['status'],'FAIL')
 def test_embedded_result_marker_cannot_be_ignored(self):
  code="print('WARNING RESULT: FAIL (1 checks, 1 failed)');print('CHECK: harmless PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code))['status'],'FAIL')
 def test_real_e2e_annotation_is_accepted(self):
  code="print('CHECK: refusal names the TLS rule                           PASS | reason=static TLS refused');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code))['status'],'PASS')
 def test_failed_e2e_annotation_is_rejected(self):
  code="print('CHECK: import resolved                                    FAIL | reason=unresolved import');print('CHECK: harmless PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code))['status'],'FAIL')
 def test_required_observation_is_missing(self):
  self.assertEqual(self.run_case(self.case(required_text=['loader refused (fail-closed)']))['status'],'FAIL')
 def test_required_observation_outside_passing_check_is_missing(self):
  code="print('DEBUG: loader refused (fail-closed) was not observed');print('CHECK: unrelated PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code,required_text=['loader refused (fail-closed)']))['status'],'FAIL')
 def test_required_observation_is_present(self):
  code="print('CHECK: loader refused (fail-closed) PASS');print('RESULT: PASS (1 checks, 0 failed)')"
  self.assertEqual(self.run_case(self.case(code,required_text=['loader refused (fail-closed)']))['status'],'PASS')
 def test_missing_input_never_executes_child(self):
  with tempfile.TemporaryDirectory() as td:
   marker=pathlib.Path(td)/'executed'
   r=runner.run_case(self.case(f"open({str(marker)!r},'w').write('bad')",input=str(pathlib.Path(td)/'missing.dll')),pathlib.Path(td))
   self.assertEqual(r['status'],'MISSING');self.assertFalse(marker.exists())
 def test_input_mutation_fails(self):
  with tempfile.TemporaryDirectory() as td:
   dll=pathlib.Path(td)/'input.dll';dll.write_bytes(b'original')
   r=runner.run_case(self.case(f"open({str(dll)!r},'wb').write(b'changed');"+GOOD,input=str(dll)),pathlib.Path(td))
   self.assertEqual(r['status'],'FAIL');self.assertNotEqual(r['sha_before'],r['sha_after'])
 def test_timeout_executes_once(self):
  with tempfile.TemporaryDirectory() as td:
   marker=pathlib.Path(td)/'attempts'
   r=runner.run_case(self.case(f"open({str(marker)!r},'a').write('x');__import__('time').sleep(5)"),pathlib.Path(td))
   self.assertEqual(r['status'],'TIMEOUT');self.assertEqual(marker.read_text(),'x')
 def test_build_nonzero_stops_cases(self):
  with tempfile.TemporaryDirectory() as td:
   marker=pathlib.Path(td)/'executed'
   result=runner.run_suite({'build':{'argv':[sys.executable,'-c','raise SystemExit(8)'],'timeout':1},'cases':[self.case(f"open({str(marker)!r},'w').write('x');"+GOOD)]},pathlib.Path(td))
   self.assertFalse(result['passed']);self.assertFalse(marker.exists())
 def test_empty_or_duplicate_suite_refused(self):
  for cases in [[],[self.case(),self.case()]]:
   with tempfile.TemporaryDirectory() as td:self.assertFalse(runner.run_suite({'cases':cases},pathlib.Path(td))['passed'])
if __name__=='__main__':unittest.main()
