"""Bounded immutable web asset generation using only owned temporary fixtures."""
import hashlib
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
try:
    import web_assets
except ImportError:
    web_assets = None


def write_fixture(directory, *, script=b"console.log('fixture');\n"):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'index.html').write_bytes(
        b'<!doctype html><link rel="stylesheet" href="/style.css">'
        b'<main id="app"></main><script src="/app.js"></script>')
    (directory / 'app.js').write_bytes(script)
    (directory / 'style.css').write_bytes(b'body{color:#123}\n')


class WebAssetTests(unittest.TestCase):
    def module(self):
        self.assertIsNotNone(web_assets, 'web_assets.py is missing')
        return web_assets

    def test_only_named_assets_are_embedded_byte_for_byte_and_hashed(self):
        module = self.module()
        with tempfile.TemporaryDirectory() as temporary:
            source = pathlib.Path(temporary) / 'web'
            write_fixture(source, script=b'/*' + b'x' * 9000 + b'*/')
            (source / 'unused.txt').write_bytes(b'ignored and never embedded')
            assets = module.inspect_assets(source)
            self.assertEqual(list(assets), ['index.html', 'app.js', 'style.css'])
            self.assertGreater(len(assets['app.js']), 8192)
            identity = module.asset_identity(assets)
            for name, content in assets.items():
                self.assertEqual(identity[name], {
                    'size': len(content), 'sha256': hashlib.sha256(content).hexdigest()})
            generated = pathlib.Path(temporary) / 'generated.c'
            module.write_c_assets(assets, generated)
            text = generated.read_text()
            self.assertIn('http_asset_index_html', text)
            self.assertIn('http_asset_app_js', text)
            self.assertIn('http_asset_style_css', text)
            for value in (b'<!doctype html>', b'xxx', b'body{color:#123}'):
                triplet = ', '.join(f'0x{byte:02x}' for byte in value[:3])
                self.assertIn(triplet, text)

    def test_missing_symlink_invalid_encoding_and_bad_index_fail_closed(self):
        module = self.module()
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            missing = root / 'missing'
            write_fixture(missing)
            (missing / 'style.css').unlink()
            with self.assertRaises(ValueError):
                module.inspect_assets(missing)

            symlinked = root / 'symlinked'
            write_fixture(symlinked)
            original = root / 'external.js'
            original.write_bytes(b'console.log(1)')
            (symlinked / 'app.js').unlink()
            (symlinked / 'app.js').symlink_to(original)
            with self.assertRaises(ValueError):
                module.inspect_assets(symlinked)

            bad_text = root / 'bad-text'
            write_fixture(bad_text)
            (bad_text / 'app.js').write_bytes(b'\xff')
            with self.assertRaises(ValueError):
                module.inspect_assets(bad_text)

            bad_index = root / 'bad-index'
            write_fixture(bad_index)
            (bad_index / 'index.html').write_text('<main>missing links</main>')
            with self.assertRaises(ValueError):
                module.inspect_assets(bad_index)

    def test_asset_total_cap_is_enforced(self):
        module = self.module()
        with tempfile.TemporaryDirectory() as temporary:
            source = pathlib.Path(temporary) / 'web'
            write_fixture(source, script=b'x' * (module.MAX_ASSET_BYTES // 2 + 1))
            (source / 'style.css').write_bytes(b'y' * (module.MAX_ASSET_BYTES // 2 + 1))
            with self.assertRaises(ValueError):
                module.inspect_assets(source)


if __name__ == '__main__':
    unittest.main()
