"""Hash-backed generation membership keeps real compile provenance separate."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

OWN = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(OWN / 'execution'), str(OWN)]
from admission import Refusal, read, sha
from full_pipeline import load_odr_generation


def pin(path):
    return {'path': str(path.resolve()), 'sha256': sha(path)}


class GenerationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=OWN / 'tests')
        self.dir = Path(self.temp.name)
        self.run = OWN / 'runs/run-l19-1'
        r3 = OWN.parent / '2026-10-07-l19-r3-full-build'
        self.data = {'schema': 1, 'run_directory': str(self.run),
                     'baseline_composition': pin(r3 / 'runs/run-l19-2/receipt.json'),
                     'baseline_ODR': pin(r3 / 'receipts/odr-l11.json'),
                     'current_composition': pin(self.run / 'receipt.json'),
                     'actual_map': pin(self.run / 'runtime.map'),
                     'original_checker': pin(OWN / 'check_odr_emitters_l19.py'),
                     'eligible_R3_generation_units': read(r3 / 'runs/run-l19-2/receipt.json')['freshly_compiled_units']}

    def tearDown(self):
        self.temp.cleanup()

    def load(self):
        f = self.dir / 'generation.json'; f.write_text(json.dumps(self.data))
        return load_odr_generation(f, sha(f), self.run)

    def mutate_current(self, mutate):
        current = read(self.data['current_composition']['path']); mutate(current)
        f = self.dir / 'current-fixture.json'; f.write_text(json.dumps(current)); self.data['current_composition'] = pin(f)

    def test_exact26_members_actual3_plus23_byte_identical_providers(self):
        paths = self.load()
        self.assertEqual(len(paths), 26)
        self.assertEqual(len([p for p in paths if p.startswith(str(self.run) + '/')]), 3)
        current = read(self.data['current_composition']['path'])
        self.assertNotIn(current['selected_runtime_units']['unreal-ffield']['object'], paths)

    def test_wrong_object_hash_refuses(self):
        self.mutate_current(lambda v: v['selected_runtime_units']['native-loadercontext'].update(object_sha256='0' * 64))
        with self.assertRaisesRegex(Refusal, 'generation object'): self.load()

    def test_wrong_source_hash_refuses(self):
        self.mutate_current(lambda v: v['selected_runtime_units']['native-loadercontext'].update(source_sha256='0' * 64))
        with self.assertRaisesRegex(Refusal, 'generation source'): self.load()

    def test_missing_provider_refuses(self):
        self.mutate_current(lambda v: v['selected_runtime_units'].pop('native-loadercontext'))
        with self.assertRaisesRegex(Refusal, 'generation provider missing'): self.load()

    def test_extra_provider_refuses(self):
        self.data['eligible_R3_generation_units'] = self.data['eligible_R3_generation_units'] + ['unreal-ffield']
        with self.assertRaisesRegex(Refusal, 'exact R3 generation'): self.load()

    def test_map_drift_refuses(self):
        self.data['actual_map']['sha256'] = '0' * 64
        with self.assertRaisesRegex(Refusal, 'actual ODR map'): self.load()

    def test_baseline_ODR_refusal(self):
        self.data['baseline_ODR']['sha256'] = '0' * 64
        with self.assertRaisesRegex(Refusal, 'R3 ODR'): self.load()


if __name__ == '__main__': unittest.main()
