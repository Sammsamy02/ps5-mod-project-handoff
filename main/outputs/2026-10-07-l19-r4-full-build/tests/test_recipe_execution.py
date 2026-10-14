"""Real frozen recipes: changes are bounded; an unadmitted run invokes nothing."""
import json
from pathlib import Path
import sys
import types
import unittest

OWN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OWN / 'execution'))
from admission import Refusal, ROLE, sha
from composition import Composer, compile_argv


class RecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        recipes = json.loads((OWN / 'parent-review/AFFECTED-RECIPES.json').read_text())
        cls.a = types.SimpleNamespace(recipes=recipes,
            records={role: {'path': str(OWN / 'tests' / ('selected-' + role.replace('/', '_') + '.cpp'))}
                     for role in ROLE.values()},
            pins={'profile_header': OWN / 'tests/selected/RuntimeBuildProfile.hpp',
                  'generated_header': OWN / 'tests/selected/GeneratedDeclarations.hpp'})
        cls.a.records['NativeLayoutDeclarations.hpp'] = {'path': str(OWN / 'tests/selected/NativeLayoutDeclarations.hpp')}

    def test_all147_original_flags_and_inputs_preserved(self):
        for name, recipe in self.a.recipes['recipes'].items():
            argv = compile_argv(self.a, name, OWN / 'tests/not-created')
            original = list(recipe['original_argv'])
            # Remove only the declared include/macro prefix and compare the rest.
            prefix_length = len(argv) - len(original)
            restored = [argv[0], *argv[1 + prefix_length:]]
            for flag in ('-o', '-MF'):
                restored[restored.index(flag) + 1] = original[original.index(flag) + 1]
            new_source = self.a.records[ROLE[name]]['path'] if name in ROLE else recipe['final_source']['path']
            restored[restored.index(new_source)] = recipe['old_selected_source']['path']
            self.assertEqual(restored, original, name)

    def test_program_only_explicit_generated_define(self):
        for name in self.a.recipes['recipes']:
            argv = compile_argv(self.a, name, OWN / 'tests/not-created')
            defines = [a for a in argv if 'RC_NATIVE_LAYOUT_DECLARATIONS_HEADER' in a]
            self.assertEqual(len(defines), 1 if name == 'program' else 0, name)

    def test_admission_refusal_invokes_no_compiler_linker_or_packager(self):
        calls = []
        class RefusingAdmission:
            def validate(self):
                raise Refusal('HOLD: missing current reviewed profile/GO')
        with self.assertRaisesRegex(Refusal, 'profile/GO'):
            Composer(RefusingAdmission(), lambda *args: calls.append(args)).compose(OWN / 'tests/not-created', 1)
        self.assertEqual(calls, [])
        self.assertFalse((OWN / 'tests/not-created').exists())


if __name__ == '__main__':
    unittest.main()
