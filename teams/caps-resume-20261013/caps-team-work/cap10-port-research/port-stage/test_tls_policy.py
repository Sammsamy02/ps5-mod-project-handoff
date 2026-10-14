#!/usr/bin/env python3
"""Run the actual staged shell policy; this is not target runtime evidence."""
import argparse
import pathlib
import re
import subprocess
import unittest

HERE = pathlib.Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=pathlib.Path, required=True)
args, remaining = parser.parse_known_args()
SOURCE = args.source.read_text()


def policy(prospero, requested=None):
    # AC_ARG_WITH's existing default is __thread; explicit values are assigned
    # by Autoconf before its action-if-given executes.
    begin = SOURCE.find('# Prospero TLS policy: begin')
    end = SOURCE.find('# Prospero TLS policy: end')
    block = '' if begin == -1 else SOURCE[begin:end]
    # The sole m4 macro inside this shell fragment terminates configure.
    block = re.sub(r'AC_MSG_ERROR\(\[([^\]]+)\]\)',
                   lambda m: "printf '%s\\n' '" + m.group(1) + "' >&2; exit 1", block)
    script = (f'host_prospero={"yes" if prospero else "no"}\n'
              f'mono_tls_explicit={"yes" if requested is not None else "no"}\n'
              f'with_tls={requested or "__thread"}\n' + block + '\nprintf "%s\\n" "$with_tls"\n')
    return subprocess.run(['/bin/sh'], input=script, text=True, capture_output=True)


class TlsPolicyTests(unittest.TestCase):
    def test_prospero_default_selects_pthread(self):
        result = policy(True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), 'pthread')

    def test_explicit_compiler_tls_refused(self):
        result = policy(True, '__thread')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('requires --with-tls=pthread', result.stderr)

    def test_explicit_pthread_accepted(self):
        result = policy(True, 'pthread')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), 'pthread')

    def test_other_targets_retain_existing_choice(self):
        for requested, expected in [(None, '__thread'), ('__thread', '__thread'), ('pthread', 'pthread')]:
            with self.subTest(requested=requested):
                result = policy(False, requested)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), expected)


if __name__ == '__main__':
    unittest.main(argv=[__file__, *remaining])
