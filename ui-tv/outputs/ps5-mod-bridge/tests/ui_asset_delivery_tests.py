"""Owned asset fixtures and desktop loopback only; no native build or providers."""
import hashlib
import http.client
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
import modbridge_host

spec = importlib.util.spec_from_file_location("ui_delivery_web_assets", PROJECT / "native-host/web_assets.py")
web_assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(web_assets)
MODULES = ("controller-keyboard.js", "config.js", "installer-service.js", "installer-mock.js", "installer-ui.js", "app.js")


def fixture(root, scripts=MODULES):
    root.mkdir(parents=True, exist_ok=True)
    (root / "index.html").write_text('<!doctype html><link rel="stylesheet" href="/style.css">' + ''.join(f'<script src="/{name}" defer></script>' for name in scripts), encoding="utf-8")
    (root / "style.css").write_text("body { color: white; }", encoding="utf-8")
    for index, name in enumerate(MODULES):
        text = 'globalThis.assetOrder = [];' if index == 0 else ''
        (root / name).write_text(text + f'globalThis.assetOrder.push({json.dumps(name)});', encoding="utf-8")


class NativeBundleDeliveryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "web"
        fixture(self.root)

    def test_referenced_modules_run_in_source_order_with_only_three_native_assets(self):
        before = {path.name: path.read_bytes() for path in self.root.iterdir()}
        assets = web_assets.inspect_assets(self.root)
        self.assertEqual(tuple(assets), ("index.html", "app.js", "style.css"))
        self.assertEqual(assets["index.html"].count(b"<script"), 1)
        self.assertIn(b'src="/app.js"', assets["index.html"])
        for name in MODULES[:-1]:
            self.assertNotIn(name.encode(), assets["index.html"])
            self.assertIn(before[name], assets["app.js"])
        bundle = Path(self.temporary.name) / "bundle.js"
        bundle.write_bytes(assets["app.js"] + b'\nconsole.log(JSON.stringify(assetOrder));')
        result = subprocess.run(["node", str(bundle)], check=True, capture_output=True, text=True)
        self.assertEqual(json.loads(result.stdout), list(MODULES))
        self.assertEqual(before, {path.name: path.read_bytes() for path in self.root.iterdir()})
        emitted = Path(self.temporary.name) / "assets.c"
        identity = web_assets.write_c_assets(assets, emitted)
        self.assertEqual(set(identity), {"index.html", "app.js", "style.css"})
        self.assertEqual(identity["app.js"]["sha256"], hashlib.sha256(assets["app.js"]).hexdigest())
        self.assertEqual(emitted.read_text().count("const struct http_static_asset http_asset_"), 3)

    def test_real_keyboard_bytes_are_embedded_in_the_actual_app_bundle(self):
        assets = web_assets.inspect_assets(PROJECT / "web")
        self.assertTrue((PROJECT / "web/controller-keyboard.js").read_bytes() in assets["app.js"], "Actual keyboard source is missing from the native bundle")
        self.assertNotIn(b'/controller-keyboard.js', assets["index.html"])

    def test_only_referenced_modules_are_read_and_their_declared_order_is_preserved(self):
        (self.root / "index.html").write_text('<link href="/style.css"><script src="/config.js"></script><script src="/app.js"></script>')
        (self.root / "controller-keyboard.js").unlink()
        (self.root / "config.js").write_text("globalThis.binding = {ready:true};")
        (self.root / "app.js").write_text("if(!binding.ready)throw Error('dependency missing');")
        assets = web_assets.inspect_assets(self.root)
        result = subprocess.run(["node", "-e", assets["app.js"].decode()], check=False, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode())

    def test_unknown_external_inline_traversal_and_duplicate_script_refs_refuse(self):
        for tag in ('<script src="/unknown.js"></script>', '<script src="https://example.com/app.js"></script>', '<script src="//example.com/app.js"></script>', '<script src="/../app.js"></script>', '<script src="/%61pp.js"></script>', '<script>run()</script>', '<script src="/config.js">run()</script>', '<script src="/app.js"></script>'):
            with self.subTest(tag=tag):
                fixture(self.root)
                index = self.root / "index.html"
                index.write_text(index.read_text() + tag)
                with self.assertRaises(ValueError):
                    web_assets.inspect_assets(self.root)

    def test_missing_symlink_non_utf8_and_nul_module_refuse(self):
        for failure in ("missing", "symlink", "encoding", "nul"):
            with self.subTest(failure=failure):
                fixture(self.root)
                source = self.root / "config.js"
                if failure == "missing":
                    source.unlink()
                elif failure == "symlink":
                    target = Path(self.temporary.name) / "outside.js"
                    target.write_bytes(b"/* outside */")
                    source.unlink()
                    source.symlink_to(target)
                elif failure == "encoding":
                    source.write_bytes(b"\xff")
                else:
                    source.write_bytes(b"text\x00")
                with self.assertRaises(ValueError):
                    web_assets.inspect_assets(self.root)
                if source.is_symlink():
                    source.unlink()

    def test_each_module_and_the_final_three_asset_total_keep_the_original_size_bound(self):
        for size in (web_assets.MAX_ASSET_BYTES + 1, web_assets.MAX_ASSET_BYTES - 100):
            with self.subTest(size=size):
                fixture(self.root)
                (self.root / "config.js").write_bytes(b"x" * size)
                with self.assertRaises(ValueError):
                    web_assets.inspect_assets(self.root)

    def test_legacy_three_assets_are_byte_identical(self):
        fixture(self.root, scripts=("app.js",))
        expected = {name: (self.root / name).read_bytes() for name in ("index.html", "app.js", "style.css")}
        self.assertEqual(web_assets.inspect_assets(self.root), expected)


class DesktopModuleDeliveryTests(unittest.TestCase):
    def setUp(self):
        self.application = SimpleNamespace(origin="", state_lock=threading.Lock(), sessions={})
        self.server = modbridge_host.create_server(self.application)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.close)

    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)

    def request(self, target, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_address[1], timeout=3)
        connection.request("GET", target, headers=headers or {})
        response = connection.getresponse()
        result = (response.status, response.read(), dict(response.getheaders()))
        connection.close()
        return result

    def test_desktop_serves_every_named_module_without_a_login_or_provider(self):
        for name in MODULES[:-1]:
            with self.subTest(name=name):
                status, body, headers = self.request("/" + name)
                self.assertEqual(status, 200)
                self.assertEqual(body, (PROJECT / "web" / name).read_bytes())
                self.assertEqual(headers["Content-Type"], "text/javascript; charset=utf-8")
                self.assertIn("script-src 'self'", headers["Content-Security-Policy"])
                self.assertNotIn("Set-Cookie", headers)

    def test_unknown_js_is_not_served_and_external_origin_rules_remain(self):
        self.assertEqual(self.request("/unknown.js")[0], 404)
        self.assertEqual(self.request("/../config.js")[0], 404)
        self.assertEqual(self.request("/config.js?unexpected=1")[0], 404)
        self.assertEqual(self.request("/config.js", {"Origin": "https://example.com"})[0], 403)


if __name__ == "__main__":
    unittest.main()
