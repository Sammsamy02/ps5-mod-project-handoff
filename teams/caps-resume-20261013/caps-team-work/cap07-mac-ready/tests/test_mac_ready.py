import sys
sys.dont_write_bytecode = True
import copy, hashlib, importlib.util, json, os, subprocess, tempfile, unittest
from pathlib import Path
from unittest.mock import patch

HOME = Path(__file__).resolve().parent.parent

def load(name, file):
    spec = importlib.util.spec_from_file_location(name, file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

rebinder = load('rebind', HOME / 'tools/prepare_compile_stage.py')
compiler = load('compiler', HOME / 'tools/compile_target.py')
selection = load('selection', HOME / 'tools/dry_run_build_selection.py')

class MacReady(unittest.TestCase):
    def setUp(self):
        self.decision = json.loads((HOME / 'evidence/DEPENDENCY-REBIND.json').read_bytes())
        self.files = selection.load_stage(HOME / 'derived/review-final')[2]

    def test_narrow_rebind_preserves_every_other_metadata_value(self):
        derived = rebinder.rebind(self.files, self.decision)
        before = json.loads(self.files[rebinder.REL])
        after = json.loads(derived[rebinder.REL])
        for row in self.decision['headers']:
            self.assertEqual(after['added_header_hashes'][row['path']], row['actual_sha256'])
            after['added_header_hashes'][row['path']] = row['declared_sha256']
        self.assertEqual(before, after)
        self.assertEqual({k: v for k, v in self.files.items() if k != rebinder.REL}, {k: v for k, v in derived.items() if k != rebinder.REL})

    def test_rebind_refuses_changed_prior_declared_pin(self):
        decision = copy.deepcopy(self.decision)
        decision['headers'][0]['declared_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'prior declared pin mismatch'):
            rebinder.rebind(self.files, decision)

    def test_rebind_refuses_changed_snapshot_pin(self):
        decision = copy.deepcopy(self.decision)
        decision['headers'][0]['actual_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'snapshot drift'):
            rebinder.rebind(self.files, decision)

    def test_rebind_refuses_unreviewed_header(self):
        decision = copy.deepcopy(self.decision)
        decision['headers'][0]['path'] = '/unknown.hpp'
        with self.assertRaisesRegex(ValueError, 'two audited stale dependency pins'):
            rebinder.rebind(self.files, decision)

    def test_final_complete_patch_roundtrip(self):
        stage, manifest, files, _ = selection.load_stage(HOME / 'derived/target-ready')
        self.assertEqual(len(manifest['originals']), 8)
        self.assertEqual(len(files), 15)
        with tempfile.TemporaryDirectory(dir=HOME / 'tests/tmp') as temporary:
            root = Path(temporary)
            original = {}
            for row in manifest['originals']:
                data = (selection.ROOT / row['path']).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), row['sha256'])
                original[row['path']] = data
                path = root / row['path']
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            patch_bytes = (stage / 'integration.patch').read_bytes()
            for reverse, expected in ((False, files), (True, original)):
                command = ['git', 'apply'] + (['--reverse'] if reverse else []) + ['-']
                result = subprocess.run(command, cwd=root, input=patch_bytes, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                for relative, data in expected.items():
                    self.assertEqual((root / relative).read_bytes(), data)
            self.assertTrue(all(not (root / path).exists() for path in files.keys() - original.keys()))

    def test_real_compile_preparation_has_current_routes_and_immutable_headers(self):
        with tempfile.TemporaryDirectory(dir=HOME / 'tests/tmp') as temporary:
            out = Path(temporary) / 'target/prepared'
            with patch.object(compiler, 'HOME', HOME):
                # Select an actual cache consumer without invoking a compiler;
                # outputs still use the exclusive owned target directory.
                out = HOME / 'target/tooling-prepare-check'
                relative_include = Path(os.path.relpath(HOME / 'evidence/header-audit'))
                result = compiler.prepare(HOME / 'derived/target-ready', out, ['native-thread-name-threadnameruntime'], extra_include=[relative_include])
                self.assertEqual(result['max_compiler_jobs'], 1)
                self.assertEqual(len(result['commands']), 1)
                self.assertEqual(result['commands'][0]['name'], 'native-thread-name-threadnameruntime')
                self.assertTrue(all(p['matches'] for p in result['source_header_audit']))
                self.assertEqual(len(result['audited_real_header_snapshots']), 2)
                self.assertTrue(all(not row['violations'] for row in result['header_routes']))
                self.assertNotIn('-shared', result['commands'][0]['argv'])
                self.assertIn('-c', result['commands'][0]['argv'])
                self.assertTrue(result['commands'][0]['argv'][0].endswith('prospero-clang++'))
                self.assertEqual(result['include_dirs'][0], str((HOME / 'evidence/header-audit').resolve()))
                with self.assertRaisesRegex(ValueError, 'fresh target output'):
                    compiler.prepare(HOME / 'derived/target-ready', out, ['native-thread-name-threadnameruntime'])
                import shutil
                shutil.rmtree(out)

if __name__ == '__main__':
    unittest.main()
