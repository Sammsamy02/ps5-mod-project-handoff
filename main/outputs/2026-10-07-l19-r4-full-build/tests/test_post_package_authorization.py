import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

OWN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OWN / 'parent-review'))
from post_package_resume import authorize_post_package
from admission import sha, read
from test_composed_selection import ComposedSelection


class PostPackageAuthorization(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.admission = self.root / 'post-admission.json'
        self.go = self.root / 'post-go.json'
        compiled_path = OWN / 'parent-review/EXECUTION-ADMISSION.FINAL.v5.json'
        compiled = read(compiled_path)
        code = dict(compiled['execution_source_pins'])
        wrapper = OWN / 'parent-review/post_package_resume.py'
        code[str(wrapper)] = sha(wrapper)
        self.data = {'scope': 'post-package-only', 'full_build_enabled': False,
                     'no_compile_link_package': True,
                     'compilation_admission': {'path': str(compiled_path), 'sha256': sha(compiled_path)},
                     'compilation_go': {'path': str(OWN / 'execution-state/r4-freeze-v5/BUILD-GO.json'),
                                        'sha256': sha(OWN / 'execution-state/r4-freeze-v5/BUILD-GO.json')},
                     'execution_source_pins': code,
                     **{k: compiled[k] for k in ('source_manifest', 'profile_json', 'generated_header')}}

    def authorize(self, data=None, go_change=None, missing_go=False):
        self.admission.write_text(json.dumps(self.data if data is None else data))
        go = {'r4_post_package_go': True, 'r4_full_build_go': False,
              'scope': 'post-package-only', 'admission_sha256': sha(self.admission),
              'execution_source_pins': self.data['execution_source_pins']}
        if go_change:
            go.update(go_change)
        self.go.write_text(json.dumps(go))
        return authorize_post_package(self.admission, sha(self.admission),
                                      self.go.with_name('missing.json') if missing_go else self.go,
                                      sha(self.go))

    def test_standalone_post_authorization_preserves_compilation_admission(self):
        result = self.authorize()
        self.assertEqual(self.data['compilation_admission'], result['compilation_admission'])
        self.assertFalse(result['full_build_enabled'])

    def test_missing_go_refuses(self):
        with self.assertRaisesRegex(ValueError, 'missing|regular'):
            self.authorize(missing_go=True)

    def test_full_build_go_refuses(self):
        with self.assertRaisesRegex(ValueError, 'post-package-only GO'):
            self.authorize(go_change={'r4_full_build_go': True})

    def test_wrong_go_admission_hash_refuses(self):
        with self.assertRaisesRegex(ValueError, 'GO admission mismatch'):
            self.authorize(go_change={'admission_sha256': '0' * 64})

    def test_missing_wrapper_pin_refuses(self):
        data = copy.deepcopy(self.data)
        del data['execution_source_pins'][str(OWN / 'parent-review/post_package_resume.py')]
        with self.assertRaisesRegex(ValueError, 'source catalog mismatch'):
            self.authorize(data)

    def test_wrong_profile_identity_refuses(self):
        data = copy.deepcopy(self.data)
        data['profile_json']['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'profile_json identity mismatch'):
            self.authorize(data)


if __name__ == '__main__':
    unittest.main()
