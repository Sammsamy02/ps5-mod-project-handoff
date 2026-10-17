"""Offline validation and static registrar retention for the optional service."""
import hashlib
import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


class ModServiceBuildTests(unittest.TestCase):
    def setUp(self):
        path = ROOT / 'mod_service/build_config.py'
        self.assertTrue(path.is_file(), 'optional mod service build validation is missing')
        spec = importlib.util.spec_from_file_location('service_build_config', path)
        self.build = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.build)
        self.temporary = tempfile.TemporaryDirectory(prefix='mod-service-build-')
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name).resolve()
        self.archive = self.root / 'service.a'
        self.archive.write_bytes(b'!<arch>\n')
        self.profile = self.root / 'profile.json'
        self.config = self.root / 'config.json'
        self.descriptor = {'schema_version': 1, 'target': 'host',
                           'service_archive': self.archive_row(self.archive)}
        self.settings = {'state_dir': '/data/modforge', 'mod_root_base': '/data/modbridge'}
        self.write_inputs()

    @staticmethod
    def archive_row(path):
        return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

    def write_inputs(self):
        self.profile.write_text(json.dumps(self.descriptor))
        self.config.write_text(json.dumps(self.settings))

    def prepare(self, target='host'):
        return self.build.prepare_mod_service(target, self.profile, self.config)

    def test_disabled_by_default_and_options_must_be_paired(self):
        disabled = self.build.prepare_mod_service('host', None, None)
        self.assertEqual(disabled['identity'], {'enabled': False})
        self.assertEqual(self.build.mod_service_header(disabled),
                         '#define MB_HOST_MOD_SERVICE 0\n#define MB_HOST_MOD_SERVICE_CONFIG_JSON "{}"\n')
        self.assertEqual(self.build.mod_service_link_arguments(disabled), [])
        for profile, config in ((self.profile, None), (None, self.config)):
            with self.assertRaisesRegex(ValueError, 'both'):
                self.build.prepare_mod_service('host', profile, config)

    def test_config_is_canonical_and_identity_pins_every_input(self):
        self.settings.update(data_dir='/data/modforge/share', limits={'max_jobs': 2})
        self.write_inputs()
        selected = self.prepare()
        canonical = '{"data_dir":"/data/modforge/share","limits":{"max_jobs":2},"mod_root_base":"/data/modbridge","state_dir":"/data/modforge"}'
        self.assertEqual(selected['config_json'], canonical)
        identity = selected['identity']
        self.assertEqual(identity['target'], 'host')
        self.assertTrue(identity['enabled'])
        for key, path in (('profile', self.profile), ('configuration', self.config), ('service_archive', self.archive)):
            self.assertEqual(identity[key]['path'], str(path))
            self.assertEqual(identity[key]['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(identity['config_sha256'], hashlib.sha256(canonical.encode()).hexdigest())
        self.assertIn(json.dumps(canonical), self.build.mod_service_header(selected))

    def test_rejects_unknown_or_duplicate_fields_and_target_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'target'):
            self.prepare('native')
        for extra in ({'link_flags': ['-bad']}, {'shell': 'no'}, {'schema_version': True}):
            with self.subTest(extra=extra):
                self.profile.write_text(json.dumps({**self.descriptor, **extra}))
                with self.assertRaises(ValueError):
                    self.prepare()
        self.write_inputs()
        self.config.write_text('{"state_dir":"/data/first","state_dir":"/data/second","mod_root_base":"/data/mods"}')
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.prepare()

    def test_configuration_rejects_noncanonical_paths_and_unknown_limits(self):
        for value in ('relative', '/', '/data//mods', '/data/../mods', '/data/./mods',
                      '/data/mods/', '/data/evil\0value', '/data/line\nbreak', '/data/' + 'x' * 1025):
            with self.subTest(value=value):
                self.settings['state_dir'] = value
                self.write_inputs()
                with self.assertRaises(ValueError):
                    self.prepare()
        self.settings = {'state_dir': '/data/modforge', 'mod_root_base': '/data/modbridge'}
        for extra in ({'state_dir': '/data/modbridge'}, {'browser_path': '/data/evil'},
                      {'limits': {'unknown': 1}}, {'limits': {'max_jobs': True}},
                      {'limits': {'max_jobs': 0}}, {'limits': {'max_jobs': 1.5}},
                      {'limits': {'max_entry_bytes': -1}}, {'limits': []}):
            with self.subTest(extra=extra):
                self.config.write_text(json.dumps({**self.settings, **extra}))
                with self.assertRaises(ValueError):
                    self.prepare()
        self.config.write_text(' ' * (64 * 1024 + 1))
        with self.assertRaisesRegex(ValueError, 'size'):
            self.prepare()

    def test_missing_modified_thin_and_duplicate_archives_are_refused(self):
        self.archive.unlink()
        with self.assertRaises((OSError, ValueError)):
            self.prepare()
        self.archive.write_bytes(b'!<arch>\nchanged')
        with self.assertRaisesRegex(ValueError, 'SHA256'):
            self.prepare()
        self.archive.write_bytes(b'!<thin>\n')
        self.descriptor['service_archive'] = self.archive_row(self.archive)
        self.write_inputs()
        with self.assertRaisesRegex(ValueError, 'static archive'):
            self.prepare()
        self.archive.write_bytes(b'!<arch>\n')
        self.descriptor['service_archive'] = self.archive_row(self.archive)
        self.descriptor['dependency_archives'] = [self.archive_row(self.archive)]
        self.write_inputs()
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.prepare()

    def test_reverification_catches_changes_after_initial_validation(self):
        selected = self.prepare()
        for path in (self.archive, self.profile, self.config):
            with self.subTest(path=path):
                original = path.read_bytes()
                path.write_bytes(original + b' ')
                with self.assertRaisesRegex(ValueError, 'changed|SHA256'):
                    self.build.verify_mod_service_inputs(selected)
                path.write_bytes(original)
        self.build.verify_mod_service_inputs(selected)

    def test_build_cli_refuses_unpaired_service_options_before_output(self):
        output = self.root / 'output'
        for option, path in (('--mod-service-profile', self.profile), ('--mod-service-config', self.config)):
            with self.subTest(option=option):
                completed = subprocess.run([sys.executable, str(ROOT / 'build.py'),
                    '--profile', 'desktop_fixture', option, str(path), '--output-root', str(output)],
                    capture_output=True, text=True)
                self.assertEqual(completed.returncode, 2)
                self.assertIn('requires both --mod-service-profile and --mod-service-config', completed.stderr)
                self.assertFalse(output.exists())

    def test_native_linker_forces_primary_before_dependencies(self):
        dep = self.root / 'dep.a'
        dep.write_bytes(b'!<arch>\n')
        self.descriptor['target'] = 'native'
        self.descriptor['dependency_archives'] = [self.archive_row(dep)]
        self.write_inputs()
        selected = self.prepare('native')
        self.assertEqual(self.build.mod_service_link_arguments(selected),
                         ['-Wl,--whole-archive', str(self.archive), '-Wl,--no-whole-archive', str(dep)])

    @unittest.skipUnless(shutil.which('cc') and shutil.which('ar'), 'C toolchain required')
    def test_unreferenced_registrar_survives_static_link_and_uses_dependency(self):
        sources = {
            'entry': 'int registered = 0; int service_result(void) { return registered; }',
            'registrar': 'extern int registered; int dependency(void); __attribute__((constructor)) static void register_method(void) { registered = dependency(); }',
            'dep': 'int dependency(void) { return 7; }',
            'main': 'int service_result(void); int main(void) { return service_result() == 7 ? 0 : 42; }',
        }
        for name, source in sources.items():
            path = self.root / (name + '.c')
            path.write_text(source)
            subprocess.run(['cc', '-c', str(path), '-o', str(path.with_suffix('.o'))], check=True, capture_output=True)
        self.archive.unlink()
        dep = self.root / 'dep.a'
        subprocess.run(['ar', 'rcs', str(self.archive), str(self.root / 'entry.o'), str(self.root / 'registrar.o')], check=True)
        subprocess.run(['ar', 'rcs', str(dep), str(self.root / 'dep.o')], check=True)
        self.descriptor['service_archive'] = self.archive_row(self.archive)
        self.descriptor['dependency_archives'] = [self.archive_row(dep)]
        self.write_inputs()
        ordinary = self.root / 'ordinary'
        subprocess.run(['cc', str(self.root / 'main.o'), str(self.archive), str(dep), '-o', str(ordinary)], check=True, capture_output=True)
        self.assertEqual(subprocess.run([str(ordinary)]).returncode, 42)
        retained = self.root / 'retained'
        subprocess.run(['cc', str(self.root / 'main.o'), *self.build.mod_service_link_arguments(self.prepare()), '-o', str(retained)], check=True, capture_output=True)
        self.assertEqual(subprocess.run([str(retained)]).returncode, 0)


if __name__ == '__main__':
    unittest.main()
