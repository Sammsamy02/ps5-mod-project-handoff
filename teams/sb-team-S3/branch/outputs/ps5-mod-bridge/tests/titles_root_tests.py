"""The desktop title provider over a console-shaped folder (companion/modbridge_titles.py); owned fixtures, no console or network.

It pins the same inventory rules the native host implements (native-host/title/*): dump and appmeta roots, source labels,
the dedupe order, the bounds and the issue/unavailable semantics.
"""
import errno
import hashlib
import http.client
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
import modbridge_host as host
import modbridge_titles as titles
from modbridge_inventory import TitleInventory
from modbridge_titles import TitlesRootProvider, decode_param_json

TOWNFALL_FOLDER = "Silent Hill Townfall PPSA33286"


def param(title_id="PPSA00001", version="01.000.000", name="Owned title", locale="en-US", extra=None):
    """A param.json shaped like the console's own (root titleId/contentVersion; names under localizedParameters)."""
    value = {"applicationCategoryType": 0, "applicationDrmType": "standard", "attribute": 536870912,
             "contentId": "JP0101-%s_00-OWNEDFIXTURE0000" % title_id, "masterVersion": "01.00",
             "pubtools": {"creationDate": "2026-09-03 12:16:50", "submission": True, "toolVersion": "3.39"},
             "userDefinedParam1": 0, "titleId": title_id}
    if version is not None:
        value["contentVersion"] = version
    localized = {}
    if locale is not None:
        localized["defaultLanguage"] = locale
    if name is not None and locale is not None:
        localized[locale] = {"titleName": name}
    localized["de-DE"] = {"titleName": "Anderer Name"}
    value["localizedParameters"] = localized
    value.update(extra or {})
    return value


def townfall():
    return param("PPSA33286", "02.003.000", "SILENT HILL: Townfall")


class Tree:
    """A temporary folder laid out like the console: data/homebrew, user/appmeta, system_data/priv/appmeta."""

    def __init__(self, test):
        self.directory = tempfile.TemporaryDirectory()
        test.addCleanup(self.cleanup)
        self.root = Path(self.directory.name).resolve()

    def cleanup(self):
        for current, directories, _ in os.walk(self.root):
            for name in directories:
                try:
                    os.chmod(os.path.join(current, name), 0o755)
                except OSError:
                    pass
        self.directory.cleanup()

    @property
    def homebrew(self):
        return self.root / "data" / "homebrew"

    @property
    def user(self):
        return self.root / "user" / "appmeta"

    @property
    def system(self):
        return self.root / "system_data" / "priv" / "appmeta"

    @staticmethod
    def put(path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value if isinstance(value, bytes) else json.dumps(value).encode("utf-8"))
        return path

    def dump(self, folder, value):
        return self.put(self.homebrew / folder / "sce_sys" / "param.json", value)

    def appmeta(self, where, folder, value):
        return self.put(where / folder / "param.json", value)

    def provider(self):
        return TitlesRootProvider(self.root)


def by_id(capture):
    return {item["title_id"]["value"]: item for item in capture["items"]}


class DecoderTests(unittest.TestCase):
    """decode_param_json follows the native decoder (native-host/title/metadata.cpp)."""

    def encode(self, value):
        return json.dumps(value).encode("utf-8")

    def assert_rejected(self, raw):
        with self.assertRaises(titles._Reject):
            decode_param_json(raw)

    def test_root_fields_and_default_locale_name_only(self):
        self.assertEqual(decode_param_json(self.encode(townfall())), ("PPSA33286", "02.003.000", "SILENT HILL: Townfall"))
        self.assertEqual(decode_param_json(b'{"titleId":"X1"}'), ("X1", None, None))

    def test_no_english_or_first_match_fallback(self):
        value = param(locale=None)
        value["localizedParameters"]["en-US"] = {"titleName": "English"}
        self.assertEqual(decode_param_json(self.encode(value))[2], None)
        value = param(locale="ja-JP", name=None)
        value["localizedParameters"]["en-US"] = {"titleName": "English"}
        self.assertEqual(decode_param_json(self.encode(value))[2], None, "the declared locale has no object")
        value = param(name="Default")
        value["localizedParameters"]["aa-AA"] = {"titleName": "Other"}
        self.assertEqual(decode_param_json(self.encode(value))[2], "Default")
        self.assertEqual(decode_param_json(b'{"titleId":"X1","localizedParameters":{"defaultLanguage":"en-US","en-US":{}}}')[2], None)
        self.assertEqual(decode_param_json(b'{"titleId":"X1","localizedParameters":{}}')[2], None)

    def test_json_key_order_is_irrelevant(self):
        raw = b'{"localizedParameters":{"en-US":{"titleName":"Late"},"defaultLanguage":"en-US"},"contentVersion":"1.0","titleId":"X1"}'
        self.assertEqual(decode_param_json(raw), ("X1", "1.0", "Late"))

    def test_title_id_rules(self):
        for good in ("A", "PPSA33286", "a.b_c-d", "9x", "A" * 64):
            self.assertEqual(decode_param_json(self.encode({"titleId": good}))[0], good)
        for bad in ("", ".a", "_a", "-a", "a b", "a/b", "A" * 65, "é", "a\0b", "a\n", "../x", None, 7, ["A"], {"a": 1}):
            self.assert_rejected(self.encode({"titleId": bad}))
        self.assert_rejected(b"{}")
        self.assert_rejected(self.encode({"TitleId": "X"}))

    def test_present_malformed_selected_fields_fail_the_entry(self):
        for field in ({"contentVersion": 5}, {"contentVersion": None}, {"contentVersion": "v" * 129}, {"contentVersion": "a\0"},
                      {"localizedParameters": []}, {"localizedParameters": None}, {"localizedParameters": "x"},
                      {"localizedParameters": {"defaultLanguage": 1}}, {"localizedParameters": {"defaultLanguage": "l" * 65}},
                      {"localizedParameters": {"defaultLanguage": "en-US", "en-US": "x"}},
                      {"localizedParameters": {"defaultLanguage": "en-US", "en-US": []}},
                      {"localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": 3}}},
                      {"localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": "n" * 257}}},
                      {"localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": "a\0b"}}}):
            self.assert_rejected(self.encode({"titleId": "X1", **field}))
        # The limits count UTF-8 bytes and are inclusive.
        self.assertEqual(decode_param_json(self.encode({"titleId": "X1", "contentVersion": "v" * 128}))[1], "v" * 128)
        self.assert_rejected(self.encode({"titleId": "X1", "contentVersion": "é" * 65}))
        self.assertEqual(decode_param_json(self.encode(param(name="n" * 256)))[2], "n" * 256)
        self.assert_rejected(self.encode(param(name="é" * 129)))
        # Ignored fields stay free-form: a malformed value that is not selected does not matter.
        self.assertEqual(decode_param_json(self.encode(param(extra={"masterVersion": 3, "gameIntent": [1, None, {"a": []}]})))[0], "PPSA00001")
        other = param()
        other["localizedParameters"]["de-DE"] = "not an object"
        self.assertEqual(decode_param_json(self.encode(other))[0], "PPSA00001")

    def test_strict_json(self):
        for raw in (b"", b"   ", b"[]", b'"x"', b"7", b"null", b'{"titleId":"X1"', b'{"titleId":"X1"} x', b"{'titleId':'X1'}",
                    b'{"titleId":"X1",}', b'{"titleId":NaN}', b'{"titleId":"X1","a":Infinity}', b'{"titleId":"X1","a":-Infinity}',
                    b'{"titleId":"X1","titleId":"X2"}', b'{"titleId":"X1","title\\u0049d":"X2"}',
                    b'{"titleId":"X1","a":{"k":1,"k":2}}', b'{"titleId":"X1","a":"\xff"}', b'{"titleId":"X1","a":"\xc0\x80"}',
                    b'{"titleId":"X1","a":"\xed\xa0\x80"}', b'{"titleId":"X1","a":"\\ud800"}', b'{"titleId":"X1","a":"\x01"}',
                    b'{"titleId":"X1"}\x00'):
            self.assert_rejected(raw)
        # Whitespace around the value and a leading UTF-8 byte order mark (the native tokenizer skips one) are fine.
        self.assertEqual(decode_param_json(b' \n\t{"titleId":"X1"} \r\n')[0], "X1")
        self.assertEqual(decode_param_json(b'\xef\xbb\xbf{"titleId":"X1"}')[0], "X1")
        self.assertEqual(decode_param_json('{"titleId":"X1","a":"\U0001f9ed"}'.encode("utf-8"))[0], "X1")
        self.assertEqual(decode_param_json(b'{"titleId":"X1","a":"\\ud83e\\udded"}')[0], "X1", "a valid surrogate pair is one scalar")

    def test_size_depth_node_and_string_limits(self):
        pad = b" " * (65536 - len(b'{"titleId":"X1"}'))
        self.assertEqual(decode_param_json(b'{"titleId":"X1"}' + pad)[0], "X1")
        self.assert_rejected(b'{"titleId":"X1"}' + pad + b" ")
        nested = lambda depth: b'{"titleId":"X1","n":' + b"[" * (depth - 1) + b"]" * (depth - 1) + b"}"
        self.assertEqual(decode_param_json(nested(16))[0], "X1", "sixteen nested containers are allowed")
        self.assert_rejected(nested(17))
        self.assert_rejected(b"[" * 5000 + b"]" * 5000)
        # 4096 nodes: the root object (1), its key and value for titleId (2) and the key for "a" (1) and the array (1) leave 4091.
        def nodes(count):
            return b'{"titleId":"X1","a":[' + b",".join([b"0"] * count) + b"]}"
        self.assertEqual(decode_param_json(nodes(4091))[0], "X1")
        self.assert_rejected(nodes(4092))
        self.assertEqual(decode_param_json(self.encode({"titleId": "X1", "a": "s" * 4096}))[0], "X1")
        self.assert_rejected(self.encode({"titleId": "X1", "a": "s" * 4097}))
        self.assert_rejected(self.encode({"titleId": "X1", "k" * 4097: 1}))

    def test_text_fields_on_the_wire(self):
        for value, expected in (("Plain", "observed"), ("", "unavailable"), (None, "unavailable"), (" lead", "error"),
                                ("trail ", "error"), ("a\tb", "error"), ("a\x7fb", "error"), ("a\u0085", "error"),
                                ("inner  space ok", "observed"), ("星 名前", "observed")):
            self.assertEqual(titles._text_field(value, 256, "homebrew_dump")["status"], expected, repr(value))
        self.assertEqual(titles._text_field("ok", 256, "homebrew_dump"), {"value": "ok", "status": "observed", "source": "homebrew_dump"})
        self.assertEqual(titles._text_field(" bad", 256, "homebrew_dump"), {"value": None, "status": "error", "source": None})


class ProviderTests(unittest.TestCase):
    def test_real_shaped_dump_folder(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        capture = tree.provider()()
        self.assertEqual({key: capture[key] for key in ("status", "complete", "source_scope", "reason", "issues")},
                         {"status": "observed", "complete": True, "source_scope": "console_title_metadata", "reason": None, "issues": []})
        self.assertEqual(capture["items"], [{
            "title_id": {"value": "PPSA33286", "status": "observed", "source": "homebrew_dump"},
            "display_name": {"value": "SILENT HILL: Townfall", "status": "observed", "source": "homebrew_dump"},
            "build_version": {"value": "02.003.000", "status": "observed", "source": "homebrew_dump"},
            "build_fingerprint": None}])

    def test_capture_is_accepted_by_the_inventory_contract(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        tree.dump("Other", param("PPSA00009", None, None))
        tree.appmeta(tree.user, "PPSA00005", param("PPSA00005", "1.0", " padded"))
        snapshot = TitleInventory(tree.provider()).snapshot()
        self.assertEqual((snapshot["status"], snapshot["complete"], snapshot["source_scope"]), ("observed", True, "console_title_metadata"))
        items = {item["title_id"]["value"]: item for item in snapshot["items"]}
        self.assertEqual(list(items), ["PPSA00005", "PPSA00009", "PPSA33286"])
        self.assertEqual(items["PPSA00009"]["display_name"], {"value": None, "status": "unavailable", "source": None})
        self.assertEqual(items["PPSA00009"]["build_version"], {"value": None, "status": "unavailable", "source": None})
        self.assertEqual(items["PPSA00005"]["display_name"], {"value": None, "status": "error", "source": None})
        self.assertEqual(items["PPSA00005"]["title_id"]["source"], "user_metadata_cache")

    def test_non_dump_folders_are_skipped_silently(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        (tree.homebrew / "etaHEN").mkdir()
        (tree.homebrew / "etaHEN" / "etaHEN.elf").write_bytes(b"\x7fELF")
        (tree.homebrew / "payloads").mkdir()
        (tree.homebrew / "bare sce_sys" / "sce_sys").mkdir(parents=True)
        (tree.homebrew / "bare sce_sys" / "sce_sys" / "icon0.png").write_bytes(b"png")
        (tree.homebrew / "loose.elf").write_bytes(b"\x7fELF")
        capture = tree.provider()()
        self.assertEqual(list(by_id(capture)), ["PPSA33286"])
        self.assertEqual((capture["status"], capture["issues"]), ("observed", []))

    def test_dump_folder_name_is_free_and_not_bound_to_the_title_id(self):
        tree = Tree(self)
        for folder in ("Silent Hill Townfall PPSA33286", "名前 with spaces & (parens) 01.018.000", "x" * 255, "a.b"):
            tree.dump(folder, param("T%d" % len(folder)))
        capture = tree.provider()()
        self.assertEqual(sorted(by_id(capture)), sorted("T%d" % n for n in (len(f) for f in ("Silent Hill Townfall PPSA33286", "名前 with spaces & (parens) 01.018.000", "x" * 255, "a.b"))))
        self.assertEqual(capture["issues"], [])

    def test_dump_folder_names_that_are_not_accepted_are_flagged(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        tree.dump("tab\there", param("PPSA00002"))
        tree.dump("bell\x07", param("PPSA00003"))
        capture = tree.provider()()
        self.assertEqual(list(by_id(capture)), ["PPSA33286"])
        self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}])

    def test_overlong_names_are_flagged_not_fatal(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        try:
            tree.dump("y" * 256, param("PPSA00002"))
        except OSError as error:
            self.skipTest("file system refuses a 256-byte name: %s" % error)
        capture = tree.provider()()
        self.assertEqual(list(by_id(capture)), ["PPSA33286"])
        self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}])

    def test_folder_name_rules_at_their_boundaries(self):
        ok = TitlesRootProvider._name_ok
        for name, dump, expected in (("x" * 255, True, True), ("x" * 256, True, False), ("é" * 128, True, False), ("é" * 127, True, True),
                                     ("a b/c", True, False), ("a\x1fb", True, False), ("a\x7fb", True, False), ("..", True, False),
                                     ("sp ace (1)", True, True), ("A" * 64, False, True), ("A" * 65, False, False),
                                     ("sp ace", False, False), ("_x", False, False), ("a.b-c_d", False, True)):
            self.assertIs(ok(name, dump), expected, (name[:20], dump))

    def test_appmeta_entries_bind_to_their_folder_name_and_carry_their_root(self):
        tree = Tree(self)
        tree.appmeta(tree.user, "PPSA00001", param("PPSA00001", "1.0", "User game"))
        tree.appmeta(tree.system, "PPSA00002", param("PPSA00002", "2.0", "System game"))
        capture = tree.provider()()
        self.assertEqual(capture["issues"], [])
        found = by_id(capture)
        self.assertEqual(found["PPSA00001"]["title_id"]["source"], "user_metadata_cache")
        self.assertEqual(found["PPSA00001"]["display_name"]["source"], "user_metadata_cache")
        self.assertEqual(found["PPSA00001"]["build_version"]["source"], "user_metadata_cache")
        self.assertEqual(found["PPSA00002"]["title_id"]["source"], "system_private_metadata_cache")
        for label, folder, value in (("mismatch", "PPSA00003", param("PPSA00004")), ("case", "ppsa00005", param("PPSA00005")),
                                     ("not an id", "has space", param("PPSA00006")), ("dotfile", ".hidden", param(".x"))):
            with self.subTest(label):
                other = Tree(self)
                other.appmeta(other.user, "PPSA00001", param("PPSA00001"))
                other.appmeta(other.user, folder, value)
                result = other.provider()()
                self.assertEqual(list(by_id(result)), ["PPSA00001"])
                self.assertEqual(result["issues"], [{"code": "provider_observation_incomplete"}])
                self.assertEqual(result["status"], "observed")

    def test_appmeta_folder_without_a_usable_param_json_is_flagged(self):
        for label, prepare in (
                ("missing", lambda folder: folder.mkdir(parents=True)),
                ("empty", lambda folder: Tree.put(folder / "param.json", b"")),
                ("malformed", lambda folder: Tree.put(folder / "param.json", b"{nope")),
                ("duplicate key", lambda folder: Tree.put(folder / "param.json", b'{"titleId":"PPSA00002","titleId":"PPSA00002"}')),
                ("no titleId", lambda folder: Tree.put(folder / "param.json", b'{"contentVersion":"1"}')),
                ("param.json is a directory", lambda folder: (folder / "param.json").mkdir(parents=True))):
            with self.subTest(label):
                tree = Tree(self)
                tree.appmeta(tree.user, "PPSA00001", param("PPSA00001"))
                prepare(tree.user / "PPSA00002")
                capture = tree.provider()()
                self.assertEqual(list(by_id(capture)), ["PPSA00001"])
                self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}])

    def test_existing_but_unusable_dump_param_is_flagged_while_absent_ones_are_not(self):
        for label, prepare, flagged in (
                ("malformed", lambda folder: Tree.put(folder / "sce_sys" / "param.json", b"{nope"), True),
                ("empty", lambda folder: Tree.put(folder / "sce_sys" / "param.json", b""), True),
                ("sce_sys is a file", lambda folder: Tree.put(folder / "sce_sys", b"x"), True),
                ("param.json is a directory", lambda folder: (folder / "sce_sys" / "param.json").mkdir(parents=True), True),
                ("no param.json", lambda folder: (folder / "sce_sys").mkdir(parents=True), False),
                ("no sce_sys", lambda folder: folder.mkdir(parents=True), False)):
            with self.subTest(label):
                tree = Tree(self)
                tree.dump(TOWNFALL_FOLDER, townfall())
                prepare(tree.homebrew / "Broken Dump")
                capture = tree.provider()()
                self.assertEqual(list(by_id(capture)), ["PPSA33286"])
                self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}] if flagged else [])

    def test_param_json_size_limit_is_inclusive_and_checked_before_reading(self):
        tree = Tree(self)
        body = b'{"titleId":"PPSA00001"}'
        tree.dump("exact", body + b" " * (65536 - len(body)))
        tree.dump("over", b'{"titleId":"PPSA00002"}' + b" " * (65537 - len(b'{"titleId":"PPSA00002"}')))
        capture = tree.provider()()
        self.assertEqual(list(by_id(capture)), ["PPSA00001"])
        self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}])
        with mock.patch.object(os, "read", side_effect=AssertionError("an oversized file must not be read")):
            over = Tree(self)
            over.dump("over", b" " * 70000)
            self.assertEqual(over.provider()()["issues"], [{"code": "provider_observation_incomplete"}])

    def test_unreadable_entries_are_skipped_and_flagged(self):
        if os.geteuid() == 0:
            self.skipTest("root ignores file modes")
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        locked_file = tree.dump("Locked File", param("PPSA00002"))
        os.chmod(locked_file, 0)
        locked_dir = tree.dump("Locked Dir", param("PPSA00003")).parent
        os.chmod(locked_dir, 0)
        locked_meta = tree.appmeta(tree.user, "PPSA00004", param("PPSA00004"))
        os.chmod(locked_meta, 0)
        capture = tree.provider()()
        self.assertEqual(list(by_id(capture)), ["PPSA33286"])
        self.assertEqual((capture["status"], capture["complete"], capture["issues"]),
                         ("observed", True, [{"code": "provider_observation_incomplete"}]))

    def test_unreadable_root_is_unavailable_not_an_error(self):
        if os.geteuid() == 0:
            self.skipTest("root ignores directory modes")
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        tree.appmeta(tree.user, "PPSA00001", param("PPSA00001"))
        os.chmod(tree.homebrew, 0)
        capture = tree.provider()()
        self.assertEqual((capture["status"], capture["issues"], list(by_id(capture))), ("observed", [], ["PPSA00001"]))

    def test_symlinks_are_never_followed(self):
        tree = Tree(self)
        outside = Tree(self)
        outside.dump("Real Game", param("PPSA77777", "9.9", "Outside"))
        tree.dump(TOWNFALL_FOLDER, townfall())
        tree.appmeta(tree.user, "PPSA00001", param("PPSA00001"))
        real = outside.homebrew / "Real Game"
        os.symlink(real, tree.homebrew / "Linked Dump")
        (tree.homebrew / "Sce Link").mkdir()
        os.symlink(real / "sce_sys", tree.homebrew / "Sce Link" / "sce_sys")
        (tree.homebrew / "Param Link" / "sce_sys").mkdir(parents=True)
        os.symlink(real / "sce_sys" / "param.json", tree.homebrew / "Param Link" / "sce_sys" / "param.json")
        os.symlink(real, tree.user / "PPSA77777")
        (tree.system / "PPSA77777").mkdir(parents=True)
        os.symlink(real / "sce_sys" / "param.json", tree.system / "PPSA77777" / "param.json")
        capture = tree.provider()()
        self.assertEqual(sorted(by_id(capture)), ["PPSA00001", "PPSA33286"])
        self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}])
        # A link in place of a whole root makes that root unavailable (silently), never a scan of its target.
        linked = Tree(self)
        linked.appmeta(linked.user, "PPSA00001", param("PPSA00001"))
        (linked.root / "data").mkdir()
        os.symlink(outside.homebrew, linked.homebrew)
        capture = linked.provider()()
        self.assertEqual((list(by_id(capture)), capture["issues"]), (["PPSA00001"], []))
        only = Tree(self)
        (only.root / "data").mkdir()
        os.symlink(outside.homebrew, only.homebrew)
        self.assertEqual(only.provider()()["status"], "unavailable")

    def test_special_files_are_flagged(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        os.mkfifo(tree.homebrew / "pipe")
        capture = tree.provider()()
        self.assertEqual(list(by_id(capture)), ["PPSA33286"])
        self.assertEqual(capture["issues"], [{"code": "provider_observation_incomplete"}])

    def test_param_json_fifo_is_flagged_and_never_blocks(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        (tree.homebrew / "Fifo" / "sce_sys").mkdir(parents=True)
        os.mkfifo(tree.homebrew / "Fifo" / "sce_sys" / "param.json")
        done = []
        thread = threading.Thread(target=lambda: done.append(tree.provider()()), daemon=True)
        thread.start()
        thread.join(5)
        self.assertEqual(len(done), 1, "scanning must not block on a FIFO")
        self.assertEqual(list(by_id(done[0])), ["PPSA33286"])
        self.assertEqual(done[0]["issues"], [{"code": "provider_observation_incomplete"}])

    def test_dedupe_prefers_dump_then_user_then_system(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, param("PPSA33286", "02.003.000", "Dump name"))
        tree.appmeta(tree.user, "PPSA33286", param("PPSA33286", "99.999.999", "User name"))
        tree.appmeta(tree.system, "PPSA33286", param("PPSA33286", "98.000.000", "System name"))
        tree.appmeta(tree.user, "PPSA00001", param("PPSA00001", "1.0", "User wins"))
        tree.appmeta(tree.system, "PPSA00001", param("PPSA00001", "9.0", "System loses"))
        tree.appmeta(tree.system, "PPSA00002", param("PPSA00002", "2.0", "System only"))
        capture = tree.provider()()
        self.assertEqual(capture["issues"], [])
        found = by_id(capture)
        self.assertEqual(list(found), ["PPSA00001", "PPSA00002", "PPSA33286"])
        self.assertEqual(found["PPSA33286"]["display_name"], {"value": "Dump name", "status": "observed", "source": "homebrew_dump"})
        self.assertEqual(found["PPSA33286"]["build_version"]["value"], "02.003.000")
        self.assertEqual(found["PPSA00001"]["display_name"]["value"], "User wins")
        self.assertEqual(found["PPSA00001"]["title_id"]["source"], "user_metadata_cache")
        self.assertEqual(found["PPSA00002"]["title_id"]["source"], "system_private_metadata_cache")

    def test_dedupe_compares_ids_ascii_case_insensitively_and_keeps_the_winners_spelling(self):
        tree = Tree(self)
        tree.dump("Dump", param("ppsa33286", "1.0", "Dump"))
        tree.appmeta(tree.user, "PPSA33286", param("PPSA33286", "2.0", "User"))
        tree.appmeta(tree.system, "Ppsa33286", param("Ppsa33286", "3.0", "System"))
        capture = tree.provider()()
        self.assertEqual([item["title_id"]["value"] for item in capture["items"]], ["ppsa33286"])
        self.assertEqual(capture["items"][0]["display_name"]["value"], "Dump")

    def test_two_dumps_of_one_title_keep_the_greater_version_then_the_smaller_digest(self):
        tree = Tree(self)
        tree.dump("Game v1", param("PPSA00001", "01.018.000", "Old"))
        tree.dump("Game v2", param("PPSA00001", "02.003.000", "New"))
        tree.dump("Game none", param("PPSA00001", None, "No version"))
        capture = tree.provider()()
        self.assertEqual([item["display_name"]["value"] for item in capture["items"]], ["New"])
        # A missing version is the empty string, which any version beats; equal versions fall to the smaller sha256 of the file.
        tied = Tree(self)
        first = tied.dump("Copy A", param("PPSA00002", "1.0", "Alpha"))
        second = tied.dump("Copy B", param("PPSA00002", "1.0", "Beta"))
        digests = {"Alpha": hashlib.sha256(first.read_bytes()).hexdigest(), "Beta": hashlib.sha256(second.read_bytes()).hexdigest()}
        expected = min(digests, key=digests.get)
        capture = tied.provider()()
        self.assertEqual([item["display_name"]["value"] for item in capture["items"]], [expected])
        self.assertEqual(capture["issues"], [])
        # Bytewise, not numeric: "9" is greater than "10".
        ordered = Tree(self)
        ordered.dump("Nine", param("PPSA00003", "9", "Nine"))
        ordered.dump("Ten", param("PPSA00003", "10", "Ten"))
        self.assertEqual(ordered.provider()()["items"][0]["display_name"]["value"], "Nine")

    def test_merge_is_independent_of_scan_order(self):
        records = [titles._Record("PPSA1", "1", "A", titles.SOURCE_USER, 1, False, "b" * 64),
                   titles._Record("ppsa1", "1", "B", titles.SOURCE_DUMP, 0, True, "c" * 64),
                   titles._Record("PPSA1", "1", "C", titles.SOURCE_DUMP, 0, True, "a" * 64),
                   titles._Record("PPSA2", None, None, titles.SOURCE_SYSTEM, 2, False, "d" * 64)]
        for ordering in (records, records[::-1], records[2:] + records[:2]):
            merged = titles.merge_records(list(ordering))
            self.assertEqual([(record.title_id, record.name) for record in merged], [("PPSA1", "C"), ("PPSA2", None)])

    def test_two_appmeta_folders_differing_only_in_case_fail_closed(self):
        records = [titles._Record("PPSA1", None, None, titles.SOURCE_USER, 1, False, "a" * 64),
                   titles._Record("ppsa1", None, None, titles.SOURCE_USER, 1, False, "b" * 64)]
        with self.assertRaises(titles._Fatal) as caught:
            titles.merge_records(records)
        self.assertEqual(caught.exception.reason, "invalid_provider_capture")
        tree = Tree(self)
        try:
            tree.appmeta(tree.user, "PPSA00001", param("PPSA00001"))
            tree.appmeta(tree.user, "ppsa00001", param("ppsa00001"))
        except OSError:
            self.skipTest("file system refuses both")
        if len(os.listdir(tree.user)) != 2:
            self.skipTest("case-insensitive file system")
        capture = tree.provider()()
        self.assertEqual((capture["status"], capture["complete"], capture["reason"], capture["items"], capture["issues"]),
                         ("error", False, "invalid_provider_capture", [], []))

    def test_sorting_is_by_case_folded_id_then_exact_id(self):
        tree = Tree(self)
        for index, title_id in enumerate(("ppsa00002", "PPSA00003", "Aaa", "aab", "PPSA00001", "9first", "B")):
            tree.dump("folder %d" % index, param(title_id))
        capture = tree.provider()()
        self.assertEqual([item["title_id"]["value"] for item in capture["items"]],
                         ["9first", "Aaa", "aab", "B", "PPSA00001", "ppsa00002", "PPSA00003"])
        self.assertEqual(titles.merge_records([titles._Record("a", None, None, titles.SOURCE_DUMP, 0, True, "1"),
                                              titles._Record("A", None, None, titles.SOURCE_USER, 1, False, "2")])[0].title_id, "a")

    def test_entry_bound_is_per_root_and_counts_plain_files(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        for index in range(255):
            (tree.homebrew / ("file%03d" % index)).write_bytes(b"x")
        self.assertEqual(len(os.listdir(tree.homebrew)), 256)
        for index in range(256):
            (tree.user.parent).mkdir(parents=True, exist_ok=True)
            tree.user.mkdir(exist_ok=True)
            (tree.user / ("plain%03d" % index)).write_bytes(b"x")
        capture = tree.provider()()
        self.assertEqual((capture["status"], capture["issues"], list(by_id(capture))), ("observed", [], ["PPSA33286"]))
        (tree.user / "plain256").write_bytes(b"x")
        capture = tree.provider()()
        self.assertEqual((capture["status"], capture["complete"], capture["reason"], capture["items"], capture["issues"]),
                         ("error", False, "inventory_resource_limit", [], []))

    LIMIT = {"status": "error", "complete": False, "reason": "inventory_resource_limit", "items": [], "issues": []}

    def assert_limit(self, capture):
        self.assertEqual({key: capture[key] for key in self.LIMIT}, self.LIMIT)

    def test_root_candidate_cap_does_not_count_silently_skipped_dump_folders(self):
        tree = Tree(self)
        for index in range(128):
            tree.dump("game %03d" % index, param("PPSA%05d" % index))
        (tree.homebrew / "zzz etaHEN").mkdir()
        (tree.homebrew / "zzz etaHEN" / "etaHEN.elf").write_bytes(b"x")
        (tree.homebrew / "zzz no param" / "sce_sys").mkdir(parents=True)
        (tree.homebrew / "zzz no sce_sys").mkdir()
        capture = tree.provider()()
        self.assertEqual((capture["status"], len(capture["items"]), capture["issues"]), ("observed", 128, []))

    def test_root_candidate_cap_counts_failed_candidates(self):
        for label, root_of, add_valid, add_failing in (
                ("dump malformed", lambda t: t.homebrew, lambda t, n: t.dump("game %03d" % n, param("PPSA%05d" % n)),
                 lambda t: t.dump("zzz broken", b"{nope")),
                ("dump unreadable shape", lambda t: t.homebrew, lambda t, n: t.dump("game %03d" % n, param("PPSA%05d" % n)),
                 lambda t: (t.homebrew / "zzz sce_sys is a file").mkdir() or Tree.put(t.homebrew / "zzz sce_sys is a file" / "sce_sys", b"x")),
                ("appmeta malformed", lambda t: t.user, lambda t, n: t.appmeta(t.user, "PPSA%05d" % n, param("PPSA%05d" % n)),
                 lambda t: t.appmeta(t.user, "PPSA99999", b"{nope")),
                ("appmeta id mismatch", lambda t: t.system, lambda t, n: t.appmeta(t.system, "PPSA%05d" % n, param("PPSA%05d" % n)),
                 lambda t: t.appmeta(t.system, "PPSA99999", param("PPSA88888"))),
                ("appmeta missing param.json", lambda t: t.user, lambda t, n: t.appmeta(t.user, "PPSA%05d" % n, param("PPSA%05d" % n)),
                 lambda t: (t.user / "PPSA99999").mkdir())):
            with self.subTest(label):
                tree = Tree(self)
                for index in range(127):
                    add_valid(tree, index)
                add_failing(tree)
                capture = tree.provider()()
                self.assertEqual((capture["status"], len(capture["items"]), capture["issues"]),
                                 ("observed", 127, [{"code": "provider_observation_incomplete"}]), "127 kept + 1 failed = 128 is within the cap")
                add_valid(tree, 127)
                self.assert_limit(tree.provider()())
                wire = TitleInventory(tree.provider()).snapshot()
                self.assertEqual((wire["status"], wire["reason"], wire["items"]), ("error", "inventory_resource_limit", []))

    def test_root_candidate_cap_127_valid_plus_two_failed_is_over(self):
        tree = Tree(self)
        for index in range(127):
            tree.dump("game %03d" % index, param("PPSA%05d" % index))
        tree.dump("zzz broken 1", b"{nope")
        self.assertEqual(tree.provider()()["status"], "observed")
        tree.dump("zzz broken 2", b"{nope")
        self.assert_limit(tree.provider()())

    def test_root_candidate_cap_ignores_entries_that_are_not_candidates(self):
        tree = Tree(self)
        for index in range(128):
            tree.appmeta(tree.user, "PPSA%05d" % index, param("PPSA%05d" % index))
        (tree.user / "has space").mkdir()
        (tree.user / ".hidden").mkdir()
        (tree.user / "note.txt").write_bytes(b"x")
        os.symlink(tree.user / "PPSA00000", tree.user / "PPSA77777")
        os.mkfifo(tree.user / "PPSA88888")
        capture = tree.provider()()
        self.assertEqual((capture["status"], len(capture["items"]), capture["issues"]),
                         ("observed", 128, [{"code": "provider_observation_incomplete"}]),
                         "names the root does not accept and top-level links are flagged, not counted")

    def test_root_candidate_cap_does_not_count_dump_folders_collapsed_into_an_earlier_one(self):
        tree = Tree(self)
        for index in range(128):
            tree.dump("game %03d" % index, param("PPSA%05d" % index, "1.0", "Original %d" % index))
        tree.dump("zzz second copy of 0", param("PPSA00000", "2.0", "Copy"))
        capture = tree.provider()()
        self.assertEqual((capture["status"], len(capture["items"]), capture["issues"]), ("observed", 128, []))
        self.assertEqual(capture["items"][0]["display_name"]["value"], "Copy", "the collapsed folder still competes on version")
        many = Tree(self)
        for index in range(200):
            many.dump("copy %03d" % index, param("PPSA%05d" % (index % 100), "1.%03d" % index, "Copy %d" % index))
        capture = many.provider()()
        self.assertEqual((capture["status"], len(capture["items"]), capture["issues"]), ("observed", 100, []))
        self.assertEqual(capture["items"][5]["build_version"]["value"], "1.105", "the greater version of the two copies wins")

    def test_root_candidate_cap_is_per_root(self):
        tree = Tree(self)
        for index in range(128):
            tree.dump("game %03d" % index, param("PPSA%05d" % index))
            tree.appmeta(tree.user, "PPSA%05d" % index, param("PPSA%05d" % index))
            tree.appmeta(tree.system, "PPSA%05d" % index, param("PPSA%05d" % index))
        capture = tree.provider()()
        self.assertEqual((capture["status"], len(capture["items"]), capture["issues"]), ("observed", 128, []))
        self.assertEqual({item["title_id"]["source"] for item in capture["items"]}, {"homebrew_dump"})

    def test_title_bound_counts_distinct_titles_after_dedupe(self):
        tree = Tree(self)
        for index in range(128):
            tree.dump("folder %03d" % index, param("PPSA%05d" % index, "1.0", "Title %d" % index))
        for index in range(0, 128, 2):
            tree.appmeta(tree.user, "PPSA%05d" % index, param("PPSA%05d" % index))
        capture = tree.provider()()
        self.assertEqual((capture["status"], len(capture["items"])), ("observed", 128))
        tree.dump("one more", param("PPSA99999"))
        capture = tree.provider()()
        self.assertEqual((capture["status"], capture["complete"], capture["reason"], capture["items"]),
                         ("error", False, "inventory_resource_limit", []))
        wire = TitleInventory(tree.provider()).snapshot()
        self.assertEqual((wire["status"], wire["reason"], wire["items"]), ("error", "inventory_resource_limit", []))

    def test_whole_pages_for_128_titles_fit_the_wire_bounds(self):
        tree = Tree(self)
        for index in range(128):
            tree.dump("folder %03d" % index, param("PPSA%05d" % index, "02.003.%03d" % index, "SILENT HILL: Townfall %d" % index))
        inventory = TitleInventory(tree.provider())
        first = json.loads(inventory.page())
        self.assertEqual((first["snapshot"]["status"], first["page"]["total_count"]), ("observed", 128))
        seen, cursor = len(first["items"]), first["page"]["next_cursor"]
        while cursor:
            page = json.loads(inventory.page(cursor))
            seen += len(page["items"])
            cursor = page["page"]["next_cursor"]
        self.assertEqual(seen, 128)

    def test_all_roots_missing_is_unavailable(self):
        tree = Tree(self)
        capture = tree.provider()()
        self.assertEqual(capture, {"status": "unavailable", "complete": False, "source_scope": "console_title_metadata",
                                   "reason": "platform_provider_unavailable", "issues": [], "items": []})
        (tree.root / "data").mkdir()
        (tree.root / "user").write_bytes(b"a file where a folder belongs")
        self.assertEqual(tree.provider()()["status"], "unavailable")
        wire = TitleInventory(tree.provider()).snapshot()
        self.assertEqual((wire["status"], wire["reason"], wire["snapshot_id"]), ("unavailable", "platform_provider_unavailable", None))

    def test_empty_existing_roots_are_observed_with_no_titles(self):
        for roots in ("homebrew", "user", "system"):
            with self.subTest(roots):
                tree = Tree(self)
                getattr(tree, roots).mkdir(parents=True)
                capture = tree.provider()()
                self.assertEqual((capture["status"], capture["complete"], capture["reason"], capture["issues"], capture["items"]),
                                 ("observed", True, None, [], []))
                wire = TitleInventory(tree.provider()).snapshot()
                self.assertEqual((wire["status"], wire["complete"], wire["items"]), ("observed", True, []))
                self.assertRegex(wire["snapshot_id"], r"^[a-f0-9]{32}$")

    def test_one_missing_root_does_not_hide_the_others(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        capture = tree.provider()()
        self.assertEqual((capture["status"], capture["issues"], len(capture["items"])), ("observed", [], 1))

    def test_scan_is_read_only(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        tree.appmeta(tree.user, "PPSA00001", param("PPSA00001"))
        before = sorted((str(path), path.stat().st_mtime_ns, path.stat().st_size) for path in tree.root.rglob("*"))
        for _ in range(2):
            tree.provider()()
        self.assertEqual(before, sorted((str(path), path.stat().st_mtime_ns, path.stat().st_size) for path in tree.root.rglob("*")))

    def test_failures_never_leak_a_host_path_or_exception_text(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        provider = tree.provider()
        with mock.patch.object(TitlesRootProvider, "_scan_root", side_effect=OSError(errno.EIO, "boom at " + str(tree.root))):
            capture = provider()
        self.assertEqual((capture["status"], capture["reason"], capture["items"], capture["issues"]), ("error", "provider_error", [], []))
        self.assertNotIn(str(tree.root), json.dumps(capture))
        snapshot = TitleInventory(provider).snapshot()
        self.assertNotIn(str(tree.root), json.dumps(snapshot))
        with mock.patch.object(TitlesRootProvider, "_scan_root", side_effect=OSError(errno.EIO, str(tree.root))):
            wire = TitleInventory(provider).page()
        self.assertNotIn(tree.root.name.encode(), wire)
        self.assertEqual(json.loads(wire)["snapshot"]["reason"], "provider_error")

    def test_constructor_requires_an_existing_directory(self):
        tree = Tree(self)
        self.assertEqual(TitlesRootProvider(tree.root).root, str(tree.root))
        self.assertEqual(TitlesRootProvider(str(tree.root)).root, str(tree.root))
        plain = tree.root / "plain"
        plain.write_bytes(b"x")
        for bad in (tree.root / "missing", plain, 7, None, b"/", ""):
            with self.subTest(repr(bad)):
                with self.assertRaises(ValueError):
                    TitlesRootProvider(bad)

    def test_removed_root_after_construction_is_unavailable(self):
        tree = Tree(self)
        sub = tree.root / "sub"
        sub.mkdir()
        provider = TitlesRootProvider(sub)
        sub.rmdir()
        self.assertEqual(provider()["status"], "unavailable")


class HostRoundTripTests(unittest.TestCase):
    def setUp(self):
        self.tree = Tree(self)
        self.tree.dump(TOWNFALL_FOLDER, townfall())
        self.tree.dump("LIZARD ASTRO BOT 01.018.000 PPSA21567", param("PPSA21567", "01.018.000", "ASTRO BOT"))
        self.tree.appmeta(self.tree.user, "PPSA00010", param("PPSA00010", "1.0", "Cached game"))
        self.private = tempfile.TemporaryDirectory()
        self.addCleanup(self.private.cleanup)
        root = Path(self.private.name).resolve()
        self.app = host.BridgeApplication(root / "private" / "credentials.json", root / "quarantine", titles_root=self.tree.root)
        self.server = host.create_server(self.app)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.shutdown)
        self.port = self.server.server_address[1]
        self.cookie = self.csrf = None

    def shutdown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        self.app.close()

    def request(self, method, path, body=None, headers=None):
        wire = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        merged = dict(headers or {})
        if self.cookie:
            merged.setdefault("Cookie", self.cookie)
        if body is not None:
            merged.setdefault("Content-Type", "application/json")
        wire.request(method, path, body=body, headers=merged)
        response = wire.getresponse()
        content = response.read()
        cookie = response.getheader("Set-Cookie")
        if cookie:
            self.cookie = cookie.split(";", 1)[0]
        wire.close()
        return response.status, content

    def bootstrap(self):
        status, body = self.request("GET", "/api/capabilities")
        self.assertEqual(status, 200)
        record = json.loads(body)
        self.csrf = record["csrf_token"]
        return record

    def collect(self, first_path="/api/detected-titles/pages", method="GET", body=None, headers=None):
        status, body = self.request(method, first_path, body, headers)
        self.assertEqual(status, 200, body)
        bodies = [body]
        page = json.loads(body)
        items = list(page["items"])
        cursor = page["page"]["next_cursor"] if page["page"] else None
        while cursor:
            status, more = self.request("GET", "/api/detected-titles/pages/" + cursor)
            self.assertEqual(status, 200)
            bodies.append(more)
            part = json.loads(more)
            items.extend(part["items"])
            cursor = part["page"]["next_cursor"]
        return page, items, bodies

    def test_pages_serve_the_merged_sorted_inventory(self):
        self.assertTrue(self.bootstrap()["capabilities"]["title_inventory_paging"])
        first, items, bodies = self.collect()
        self.assertEqual((first["schema_version"], first["kind"]), (2, "title_inventory_page"))
        self.assertEqual((first["snapshot"]["status"], first["snapshot"]["complete"], first["snapshot"]["source_scope"],
                          first["snapshot"]["reason"], first["snapshot"]["issues"]), ("observed", True, "console_title_metadata", None, []))
        self.assertEqual([item["title_id"]["value"] for item in items], ["PPSA00010", "PPSA21567", "PPSA33286"])
        self.assertEqual([item["title_id"]["source"] for item in items],
                         ["user_metadata_cache", "homebrew_dump", "homebrew_dump"])
        townfall_item = items[2]
        self.assertEqual(townfall_item["display_name"]["value"], "SILENT HILL: Townfall")
        self.assertEqual(townfall_item["build_version"], {"value": "02.003.000", "status": "observed", "source": "homebrew_dump"})
        self.assertIsNone(townfall_item["build_fingerprint"])
        status, current = self.request("GET", "/api/detected-titles/pages/current/" + first["snapshot"]["snapshot_id"])
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(current)["current"], True)
        for body in bodies + [current]:
            self.assertLessEqual(len(body), 8191)
            self.assertNotIn(str(self.tree.root).encode(), body)
            self.assertNotIn(b"/data/homebrew", body)

    def test_many_titles_page_through_cursors(self):
        for index in range(20):
            self.tree.dump("filler %02d" % index, param("PPSA1%04d" % index, "1.0", "Filler %d" % index))
        self.bootstrap()
        first, items, bodies = self.collect()
        self.assertGreater(len(bodies), 1)
        self.assertEqual(first["page"]["total_count"], 23)
        self.assertEqual(len(items), 23)
        self.assertEqual([item["title_id"]["value"] for item in items], sorted(item["title_id"]["value"] for item in items))

    def test_refresh_picks_up_a_new_folder_and_a_removed_one(self):
        self.bootstrap()
        _, before, _ = self.collect()
        self.assertEqual(len(before), 3)
        self.tree.dump("Brand New PPSA55555", param("PPSA55555", "1.0", "Brand New"))
        _, same, _ = self.collect()
        self.assertEqual(len(same), 3, "a cached capture is kept until a refresh")
        headers = {"Origin": self.app.origin, "X-CSRF-Token": self.csrf}
        refreshed, after, _ = self.collect("/api/detected-titles/pages/refresh", "POST", b"{}", headers)
        self.assertEqual([item["title_id"]["value"] for item in after], ["PPSA00010", "PPSA21567", "PPSA33286", "PPSA55555"])
        self.assertNotEqual(refreshed["snapshot"]["snapshot_id"], None)
        shutil.rmtree(self.tree.homebrew / "Brand New PPSA55555")
        _, gone, _ = self.collect("/api/detected-titles/pages/refresh", "POST", b"{}", headers)
        self.assertEqual(len(gone), 3)
        status, body = self.request("POST", "/api/detected-titles/pages/refresh", b"{}", {"X-CSRF-Token": self.csrf})
        self.assertEqual(status, 403)

    def test_issues_and_unavailable_reach_the_wire_without_paths(self):
        self.tree.appmeta(self.tree.user, "PPSA00011", b"{broken")
        self.bootstrap()
        first, items, _ = self.collect()
        self.assertEqual(first["snapshot"]["issues"], [{"code": "provider_observation_incomplete"}])
        self.assertEqual(len(items), 3)
        for path in (self.tree.homebrew, self.tree.user):
            shutil.rmtree(path)
        headers = {"Origin": self.app.origin, "X-CSRF-Token": self.csrf}
        status, body = self.request("POST", "/api/detected-titles/pages/refresh", b"{}", headers)
        self.assertEqual(status, 200)
        value = json.loads(body)
        self.assertEqual((value["snapshot"]["status"], value["snapshot"]["reason"], value["items"], value["page"]),
                         ("unavailable", "platform_provider_unavailable", [], None))
        self.assertNotIn(str(self.tree.root).encode(), body)

    def test_a_session_is_required(self):
        status, body = self.request("GET", "/api/detected-titles/pages")
        self.assertEqual(status, 401)
        self.assertEqual(json.loads(body)["code"], "session_required")


class OptionTests(unittest.TestCase):
    def test_bridge_application_titles_root_is_exclusive_with_an_inventory(self):
        tree = Tree(self)
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        root = Path(private.name).resolve()
        with self.assertRaises(ValueError):
            host.BridgeApplication(root / "a" / "c.json", root / "a" / "q", inventory=TitleInventory(), titles_root=tree.root)
        with self.assertRaises(ValueError):
            host.BridgeApplication(root / "b" / "c.json", root / "b" / "q", titles_root=tree.root / "missing")
        self.assertEqual(os.listdir(root), [], "nothing is created for a refused configuration")
        app = host.BridgeApplication(root / "c" / "c.json", root / "c" / "q", titles_root=tree.root)
        self.addCleanup(app.close)
        self.assertIsInstance(app.inventory.provider, TitlesRootProvider)
        self.assertEqual(app.inventory.provider.root, str(tree.root))
        plain = host.BridgeApplication(root / "d" / "c.json", root / "d" / "q")
        self.addCleanup(plain.close)
        self.assertIsNone(plain.inventory.provider, "the default host still has no provider")

    def run_cli(self, *arguments):
        process = subprocess.run([sys.executable, str(PROJECT / "companion" / "host.py"), *arguments],
                                 capture_output=True, text=True, timeout=30, stdin=subprocess.DEVNULL)
        return process.returncode, process.stdout, process.stderr

    def test_cli_refuses_a_bad_titles_root_with_the_safe_startup_error(self):
        tree = Tree(self)
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        root = Path(private.name).resolve()
        plain = tree.root / "plain"
        plain.write_bytes(b"x")
        for bad in (tree.root / "missing", plain):
            code, out, err = self.run_cli("--store", str(root / "s" / "c.json"), "--quarantine", str(root / "q"), "--titles-root", str(bad))
            self.assertEqual(code, 2)
            self.assertEqual(out, "")
            self.assertEqual(json.loads(err), {"error": "Local interface could not start; check its private paths and configuration."})
            self.assertNotIn(str(bad), err)
        self.assertEqual(os.listdir(root), [])

    def test_cli_passes_the_titles_root_to_the_application(self):
        tree = Tree(self)
        tree.dump(TOWNFALL_FOLDER, townfall())
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        root = Path(private.name).resolve()
        sys.path.insert(0, str(PROJECT / "companion"))
        import importlib
        cli = importlib.import_module("host")
        seen = []

        class Quiet:
            def serve_forever(self):
                raise KeyboardInterrupt()

            def server_close(self):
                pass

        def fake_server(application, port=0):
            seen.append(application.inventory.provider)
            return Quiet()

        output = io.StringIO()
        arguments = ["host.py", "--store", str(root / "c.json"), "--quarantine", str(root / "q"), "--port", "0", "--titles-root", str(tree.root)]
        with mock.patch.object(cli, "create_server", fake_server), mock.patch.object(sys, "argv", arguments), \
                mock.patch.object(sys, "stdout", output):
            self.assertEqual(cli.main(), 0)
        self.assertEqual(len(seen), 1)
        self.assertIsInstance(seen[0], TitlesRootProvider)
        self.assertEqual(seen[0].root, str(tree.root))
        self.assertEqual(json.loads(output.getvalue())["mode"], "desktop_preparation")
        # Without the option the provider stays absent.
        seen.clear()
        arguments = ["host.py", "--store", str(root / "c2.json"), "--quarantine", str(root / "q2"), "--port", "0"]
        with mock.patch.object(cli, "create_server", fake_server), mock.patch.object(sys, "argv", arguments), \
                mock.patch.object(sys, "stdout", io.StringIO()):
            self.assertEqual(cli.main(), 0)
        self.assertEqual(seen, [None])


if __name__ == "__main__":
    unittest.main()
