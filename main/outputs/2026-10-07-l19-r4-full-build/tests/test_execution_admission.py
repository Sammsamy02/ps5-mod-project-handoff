"""Refusals occur before any compiler/package boundary is reached."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

OWN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OWN / 'execution'))
from admission import Admission, Refusal, validate_recipe_set, validate_complete_profile, validate_go


class AdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipes = json.loads((OWN / 'parent-review/AFFECTED-RECIPES.json').read_text())
        cls.slots = json.loads((OWN / 'parent-review/INPUT-SUBSTITUTION-MANIFEST.json').read_text())
        cls.r3profile = json.loads((OWN.parent / '2026-10-07-l19-local-profile-r3/profiles/townfall-build-facts.json').read_text())

    def test_exact_147_recipe_and_386_slot_set(self):
        validate_recipe_set(self.recipes, self.slots)

    def test_partial_recipe_refuses(self):
        altered = dict(self.recipes, recipes=dict(self.recipes['recipes']))
        altered['recipes'].pop('program')
        with self.assertRaisesRegex(Refusal, '147'):
            validate_recipe_set(altered, self.slots)

    def test_duplicate_slot_refuses(self):
        altered = dict(self.slots, input_slots=list(self.slots['input_slots']))
        altered['input_slots'][1] = altered['input_slots'][0]
        with self.assertRaisesRegex(Refusal, 'slot'):
            validate_recipe_set(self.recipes, altered)

    def test_missing_go_refuses_before_loader(self):
        with self.assertRaisesRegex(Refusal, 'GO'):
            Admission.load(OWN / 'tests/not-an-admission.json', '0' * 64, None, None)

    def test_missing_profile_refuses(self):
        with self.assertRaisesRegex(Refusal, 'profile'):
            validate_complete_profile({})

    def test_authenticated_incomplete_r3_profile_refuses(self):
        with self.assertRaisesRegex(Refusal, 'unresolved'):
            validate_complete_profile(self.r3profile)

    def test_wrong_admission_hash_refuses_before_recipe_or_tool_read(self):
        with self.assertRaisesRegex(Refusal, 'hash'):
            Admission.load(OWN / 'parent-review/FROZEN-PREPARATION.json', '0' * 64,
                           OWN / 'inputs/R3-BUILD-GO.json', '0' * 64)

    def test_historical_go_cannot_admit_r4(self):
        historical = json.loads((OWN / 'inputs/R3-BUILD-GO.json').read_text())
        with self.assertRaisesRegex(Refusal, 'R4'):
            validate_go(historical, 'a' * 64, {})

    def test_go_binds_exact_final_inputs(self):
        a = {'source_manifest': {'sha256': '1' * 64}, 'profile_json': {'sha256': '2' * 64},
             'generated_header': {'sha256': '3' * 64}}
        go = dict(r4_full_build_go=True, local_full_build_only=True, upload_authorized=False,
                  admission_sha256='a' * 64, source_manifest_sha256='1' * 64,
                  profile_json_sha256='2' * 64, generated_header_sha256='3' * 64)
        validate_go(go, 'a' * 64, a)
        go['generated_header_sha256'] = '4' * 64
        with self.assertRaisesRegex(Refusal, 'generated_header'):
            validate_go(go, 'a' * 64, a)

    def test_actual_compiler_alias_requires_exact_resolved_identity(self):
        a = object.__new__(Admission)
        linker = Path(json.loads((OWN / 'parent-review/EXECUTION-ADMISSION.HOLD-TEMPLATE.json').read_text())['linker']['path'])
        alias = linker.parent / 'clang++'
        from admission import sha
        target = alias.resolve()
        a.pins = {'linker': linker}
        a.tool_pins = {str(target): sha(target)}
        row = {'link_target': str(alias.readlink()), 'resolved_path': str(target), 'sha256': sha(target)}
        linker_alias = linker.parent / 'ld.lld'
        a.tool_pins[str(linker)] = sha(linker)
        linker_row = {'link_target': str(linker_alias.readlink()), 'resolved_path': str(linker), 'sha256': sha(linker)}
        a.data = {'tool_aliases': {str(alias): row, str(linker_alias): linker_row},
                  'linker_invocation': str(linker_alias)}
        a.validate_tool_aliases()
        row['resolved_path'] = str(linker)
        with self.assertRaisesRegex(Refusal, 'target drift'):
            a.validate_tool_aliases()

    def test_actual_gnu_linker_invocation_and_resolved_pin_agree(self):
        import subprocess
        from admission import sha
        data = json.loads((OWN / 'parent-review/EXECUTION-ADMISSION.HOLD-TEMPLATE.json').read_text())
        canonical = Path(data['linker']['path']); alias = canonical.parent / 'ld.lld'
        failed = subprocess.run([str(canonical), '--version'], capture_output=True, text=True)
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn('generic driver', failed.stdout + failed.stderr)
        actual = subprocess.run([str(alias), '--version'], capture_output=True, text=True)
        self.assertEqual(actual.returncode, 0)
        self.assertIn('LLD', actual.stdout + actual.stderr)
        self.assertEqual(alias.resolve(), canonical)
        self.assertEqual(sha(alias), sha(canonical))

    def test_actual_binder_dependencies_require_its_consumed_headers(self):
        from admission import resolved
        a = object.__new__(Admission)
        data = json.loads((OWN / 'parent-review/EXECUTION-ADMISSION.FINAL.json').read_text())
        recipes = self.recipes
        base = json.loads(Path(recipes['base_receipt']['path']).read_text())
        a.allowed = {str(resolved(p)): h for p, h in base['source_hashes'].items()}
        a.allowed.update(data['additional_source_pins'])
        a.records = json.loads(Path(data['source_manifest']['path']).read_text())['selected_sources']
        a.allowed.update({r['path']: r['sha256'] for r in a.records.values()})
        owner = recipes['recipes']['program']['required_accepted_header']
        a.owner_header = Path(owner['path']); a.allowed[str(a.owner_header)] = owner['sha256']
        a.pins = {k: Path(data[k]['path']) for k in ('profile_header', 'generated_header')}
        dependency = OWN / 'tests/actual-attempt2-dependencies/native-layoutbinding.d'
        actual = a.check_dependencies('native-layoutbinding', dependency)
        self.assertIn(a.records['NativeLayoutActiveWidths.hpp']['path'], actual)
        self.assertIn(a.records['NativeLayoutBinding.hpp']['path'], actual)
        self.assertNotIn(str(a.pins['profile_header']), actual)
        a.allowed.pop(a.records['NativeLayoutActiveWidths.hpp']['path'])
        with self.assertRaisesRegex(Refusal, 'unpinned actual dependency'):
            a.check_dependencies('native-layoutbinding', dependency)

    def test_actual_final_three_missing_consumed_headers_refuse(self):
        from admission import resolved
        a = object.__new__(Admission)
        data = json.loads((OWN / 'parent-review/EXECUTION-ADMISSION.FINAL.json').read_text())
        base = json.loads(Path(self.recipes['base_receipt']['path']).read_text())
        a.allowed = {str(resolved(p)): h for p, h in base['source_hashes'].items()}
        a.allowed.update(data['additional_source_pins'])
        a.records = json.loads(Path(data['source_manifest']['path']).read_text())['selected_sources']
        a.allowed.update({r['path']: r['sha256'] for r in a.records.values()})
        owner = self.recipes['recipes']['program']['required_accepted_header']
        a.owner_header = Path(owner['path']); a.allowed[str(a.owner_header)] = owner['sha256']
        a.pins = {k: Path(data[k]['path']) for k in ('profile_header', 'generated_header')}
        cases = [('native-layoutbinding', role, 'binder helper')
                 for role in ('NativeLayoutBinding.hpp', 'NativeLayoutActiveWidths.hpp')]
        cases += [(name, 'RuntimeBuildProfile.hpp', 'profile header')
                  for name in ('program', 'unreal-unrealinitializer')]
        for name, role, error in cases:
            original = OWN / 'tests/actual-attempt2-dependencies' / (name + '.d')
            a.check_dependencies(name, original)
            source = original.read_text(); path = a.records[role]['path']
            self.assertEqual(source.count(path), 1, (name, role))
            with tempfile.NamedTemporaryFile(mode='w', suffix='.d', dir=OWN / 'tests') as changed:
                changed.write(source.replace(path, '')); changed.flush()
                with self.assertRaisesRegex(Refusal, error):
                    a.check_dependencies(name, changed.name)


if __name__ == '__main__':
    unittest.main()
