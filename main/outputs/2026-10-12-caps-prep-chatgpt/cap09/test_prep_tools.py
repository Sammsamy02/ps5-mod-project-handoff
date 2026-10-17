"""Offline tests; writes nothing, does not compile or execute managed/native code."""
from pathlib import Path
import sys, unittest
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(HERE.parent/'cap10'),str(HERE.parent/'cap11')]
from surface_inventory import scan_text
from coverage_gate import assess
from probe_preflight import validate, validate_host_plan

class SurfaceTests(unittest.TestCase):
    def test_comments_and_strings_cannot_inflate_call_inventory(self):
        src='// ImGui::Fake()\nconst char *s="ImGui::Fake()";\nImGui::Button("x");\n/* Unreal::Hook::RegisterFakeCallback(); */\nUnreal::Hook::RegisterEngineTickPreCallback(fn);\nframe->GetScript();\n'
        rows=scan_text(src,'real.cpp')
        self.assertEqual([(x['family'],x['symbol'],x['line']) for x in rows], [('imgui','Button',3),('hook','RegisterEngineTickPreCallback',5),('accessor_candidate','GetScript',6)])
    def test_hook_address_registration_is_not_missed(self):
        rows=scan_text('hook = &Unreal::Hook::RegisterProcessEventPreCallback;','hooks.cpp')
        self.assertEqual([x['symbol'] for x in rows],['RegisterProcessEventPreCallback'])
    def test_data_table_mutation_is_visible(self):
        rows=scan_text('GNatives_Internal[i] = &hook_expr;','table.cpp')
        self.assertTrue(any(x['family']=='data_table_write_candidate' for x in rows))
    def test_zero_required_calls_fails_closed(self):
        self.assertFalse(assess([], {})['passed'])
    def test_unknown_api_is_a_gap(self):
        self.assertEqual(assess(['Text','Button'],{'Text':'implemented'})['missing'],['Button'])
    def test_documented_mapping_is_not_implemented(self):
        self.assertFalse(assess(['Text'],{'Text':'design-only'})['passed'])
    def test_complete_explicit_coverage_passes_only_declared_subset(self):
        self.assertTrue(assess(['Text'],{'Text':'implemented'})['passed'])

class ProbeTests(unittest.TestCase):
    def setUp(self):
        self.cs=(HERE.parent/'cap10/Probe.cs').read_text(); self.header=(HERE.parent/'cap10/native_bridge_contract.h').read_text()
    def test_current_source_contract_consistent(self):self.assertEqual(validate(self.cs,self.header),[])
    def test_changed_import_name_refused(self):self.assertTrue(validate(self.cs.replace('modbridge_cap10_runtime_probe','unknown_export'),self.header))
    def test_wrong_width_refused(self):self.assertTrue(validate(self.cs.replace('out int reply','out long reply'),self.header))
    def test_unknown_extra_import_refused(self):self.assertTrue(validate(self.cs+'\n[DllImport("evil")] private static extern int Extra();',self.header))
    def test_missing_native_declaration_refused(self):self.assertTrue(validate(self.cs,self.header.replace('int32_t modbridge_cap10_abi_version(void) MODBRIDGE_CAP10_NOEXCEPT;','')))
    def test_exact_import_spelling_required(self):
        cs=self.cs.replace(', ExactSpelling = true', '')
        self.assertTrue(validate(cs,self.header), 'missing exact-name policy accepted')
    def test_empty_inputs_refused(self):self.assertTrue(validate('',''))

class HostSourceTests(unittest.TestCase):
    def test_existing_declared_api_found(self):self.assertEqual(validate_host_plan({'required_apis':[{'file':'host.h','symbol':'actual_api'}]}, {'host.h':'void actual_api(void);'}),[])
    def test_invented_api_refused(self):self.assertTrue(validate_host_plan({'required_apis':[{'file':'host.h','symbol':'imaginary_api'}]}, {'host.h':'void actual_api(void);'}))
    def test_absent_header_refused(self):self.assertTrue(validate_host_plan({'required_apis':[{'file':'absent.h','symbol':'actual_api'}]}, {}))
    def test_zero_api_plan_refused(self):self.assertTrue(validate_host_plan({'required_apis':[]}, {}))

if __name__=='__main__':unittest.main(verbosity=2)
