import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

OWN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OWN / 'execution'))
from admission import sha
from post_package_resume import select_composed_object


class ComposedSelection(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.run = Path(self.temp.name) / 'run'
        self.run.mkdir()
        self.object = self.run / 'coherent-tick-file-witness-runtime.o'
        self.object.write_bytes(b'composed metadata fixture')
        self.data = {'runtime_object': str(self.object), 'runtime_sha256': sha(self.object)}
        self.receipt = self.run / 'receipt.json'

    def select(self, data=None, digest=None):
        self.receipt.write_text(json.dumps(self.data if data is None else data))
        return select_composed_object(self.run, self.receipt, digest or sha(self.receipt), self.object.name)

    def test_actual_ambiguous_pass2_selects_recorded_composition(self):
        run = OWN / 'runs/run-l19-2'
        self.assertEqual(2, len([p for p in run.glob('*.o') if p.stat().st_size > 8 * 1024 * 1024]))
        receipt = run / 'receipt.json'
        self.assertEqual(run / self.object.name,
                         select_composed_object(run, receipt, sha(receipt), self.object.name))

    def test_missing_runtime_field_refuses(self):
        data = copy.deepcopy(self.data)
        del data['runtime_object']
        with self.assertRaisesRegex(ValueError, 'runtime object metadata'):
            self.select(data)

    def test_missing_runtime_file_refuses(self):
        self.object.unlink()
        with self.assertRaisesRegex(ValueError, 'missing|regular'):
            self.select()

    def test_wrong_receipt_hash_refuses(self):
        with self.assertRaisesRegex(ValueError, 'hash|sha|drift'):
            self.select(digest='0' * 64)

    def test_wrong_runtime_hash_refuses(self):
        data = copy.deepcopy(self.data)
        data['runtime_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'hash|sha|drift'):
            self.select(data)

    def test_outside_run_path_refuses(self):
        data = copy.deepcopy(self.data)
        data['runtime_object'] = str(self.run.parent / self.object.name)
        with self.assertRaisesRegex(ValueError, 'runtime object path'):
            self.select(data)

    def test_input_object_substitution_refuses(self):
        other = self.run / 'unreal-hooks.o'
        other.write_bytes(b'unit metadata fixture')
        data = {'runtime_object': str(other), 'runtime_sha256': sha(other)}
        with self.assertRaisesRegex(ValueError, 'runtime object path'):
            self.select(data)

    def test_receipt_outside_run_refuses(self):
        self.receipt = self.run.parent / 'receipt.json'
        with self.assertRaisesRegex(ValueError, 'composition receipt path'):
            self.select()


if __name__ == '__main__':
    unittest.main()
