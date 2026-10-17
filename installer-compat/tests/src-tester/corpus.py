#!/usr/bin/env python3
"""Build the adversarial fixture corpus under /tmp/sideproject/tester/corpus/.

All fixtures are HOSTILE INPUTS for testing the sibling modules. Bounded so
the host itself is never at risk (bomb caps at ~50MB uncompressed etc.).
Mac-only. Run:  python3 corpus.py build   (idempotent)
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import TESTER, ZipKit  # noqa: E402

CORPUS = os.path.join(TESTER, "corpus")


def build_installer_cases():
    """zips + config files aimed at installer-core."""
    d = os.path.join(CORPUS, "installer")
    os.makedirs(d, exist_ok=True)
    z = ZipKit(d)
    out = {}

    good = {
        "mods/GoodMod/init.lua": "-- good mod\n",
        "mods/GoodMod/manifest.json": (
            '{"name":"GoodMod","version":"1.0.0","game":"demo",'
            '"files":["init.lua"],"api":["FindFirstOf","Log"]}\n'),
    }
    out["good"] = z.normal("good.zip", good)
    out["truncated"] = z.truncated("truncated.zip", good)
    out["not_a_zip"] = z.not_a_zip("not_a_zip.zip")
    out["not_a_zip_lua"] = z.not_a_zip("fake.zip.lua", b"\x00\x01\x02garbage")
    out["empty"] = z.empty("empty.zip")
    out["bomb"] = z.bomb("bomb.zip")
    out["many_files"] = z.many_files("many_files.zip", 10000)
    out["unicode"] = z.unicode_paths("unicode.zip")
    out["empty_files"] = z.empty_files("empty_files.zip")
    out["nested"] = z.nested_zip("nested.zip", depth=20)
    out["symlink"] = z.with_symlink("symlink.zip")
    out["traversal"] = z.path_traversal("traversal.zip")
    out["dup_names"] = z.dup_names("dup_names.zip")
    out["backslash"] = z.dir_backslash("backslash.zip")
    out["long_name"] = z.huge_entry_name("longname.zip")

    # config weirdness — each is a manifest candidate
    cfg = os.path.join(d, "configs")
    os.makedirs(cfg, exist_ok=True)
    configs = {
        "empty.json": "",
        "whitespace.json": "   \n\t ",
        "bom.json": '﻿{"name":"Bom"}',
        "crlf.json": ('{\r\n"name":"Crlf",\r\n"version":"1.0"\r\n}'),
        "comment.json": "// comment\n{/* c */}",
        "not_json.txt": "just some text\n",
        "binary.json": ("MZ\x90\x00" * 16),
        "dupkeys.json": '{"name":"A","name":"B"}',
        "wrongtypes.json": '{"name":[1,2,3],"version":{"a":1},"files":"nope"}',
        "nulls.json": '{"name":null,"version":null}',
        "huge_numbers.json": '{"version":1e999}',
        "unicode.json": '{"name":"mod über alles ✓"}',
        "deep.json": ("[" * 400) + "]" * 400,
        "nan_like.json": '{"version":NaN}',
        "trailing_comma.json": '{"name":"x",}',
        "two_docs.json": '{"name":"a"}{"name":"b"}',
        "lf_only_names.json": '{"name":"a\\nb"}',
        "very_long_name.json": '{"name":"' + "n" * 10000 + '"}',
    }
    for fn, content in configs.items():
        p = os.path.join(cfg, fn)
        mode = "wb" if isinstance(content, bytes) else "w"
        with open(p, mode) as f:
            f.write(content)

    # zip whose manifest is inside but named oddly
    out["manifest_case"] = z.normal("manifest_case.zip", {
        "init.lua": "-- root-level mod\n",
        "MANIFEST.JSON": '{"name":"case"}',
    })
    # zip with mod at top level vs nested — placement ambiguity
    out["flat"] = z.normal("flat.zip", {"init.lua": "-- flat\n",
                                        "manifest.json": '{"name":"flat"}'})
    out["double_nested"] = z.normal("double_nested.zip", {
        "GoodMod/contents/init.lua": "-- nested once more\n",
        "GoodMod/contents/manifest.json": '{"name":"dn"}',
    })
    return out


def build_detector_cases():
    """Archives/files aimed at the mod-type detector."""
    d = os.path.join(CORPUS, "detector")
    os.makedirs(d, exist_ok=True)
    z = ZipKit(d)
    out = {}

    PE = b"MZ\x90\x00" + b"\x00" * 60 + b"PE\x00\x00" + b"\x00" * 200
    ELF = b"\x7fELF" + b"\x02\x01\x01\x00" + b"\x00" * 120

    out["lua_is_text"] = z.normal("lua_is_text.zip", {
        "mod/init.lua": "this is definitely not lua code, just prose about cats\n"
                        "no syntax at all here\n",
    })
    out["lua_is_pe"] = z.normal("lua_is_pe.zip", {
        "mod/init.lua": PE,          # windows dll renamed .lua
        "mod/other.lua": ELF,        # linux so renamed .lua
    })
    out["lua_is_zip"] = z.normal("lua_is_zip.zip", {
        "mod/init.lua": open(out_lua_zip_payload(), "rb").read()
        if os.path.exists(out_lua_zip_payload()) else b"PK\x03\x04payload",
    })
    out["dll_renamed"] = z.normal("dll_renamed.zip", {
        "mod/native.dll.lua": PE,
        "mod/plugin.lua.dll": PE,
    })
    out["real_lua_dll_mixed"] = z.normal("real_lua_dll_mixed.zip", {
        "mod/init.lua": "local x = FindFirstOf('Item')\n",
        "mod/extension/thing.lua": "-- more lua\n",
        "mod/bin/helper.dll": PE,
    })
    out["empty_mod"] = z.normal("empty_mod.zip", {"mod/": ""})
    out["empty_mod2"] = z.normal("empty_mod2.zip", {})
    out["many_files"] = z.many_files("many_files.zip", 10000)
    out["no_lua"] = z.normal("no_lua.zip", {
        "mod/readme.txt": "not a mod, just docs\n",
        "mod/img.png": b"\x89PNG\r\n\x1a\n" + b"\x00" * 64,
    })
    out["lua_with_bom"] = z.normal("lua_with_bom.zip", {
        "mod/init.lua": "﻿-- bom then code\nlocal x = 1\n",
    })
    out["lua_binary_junk"] = z.normal("lua_binary_junk.zip", {
        "mod/init.lua": b"\x00\xff\xfe garbage \x01\x02 not source",
    })
    # script loadable via loadstring at runtime — static detector blind spot
    out["obfuscated_loader"] = z.normal("obfuscated_loader.zip", {
        "mod/init.lua":
            'local s = "\\108\\111\\97\\100"'  # "load"
            '\nlocal f = _G[s]\n'
            'f("return FindFirstOf(\'Item\')")()\n',
    })
    # API access through dynamic dispatch — string-indexed
    out["dynamic_dispatch"] = z.normal("dynamic_dispatch.zip", {
        "mod/init.lua":
            'local api = {"FindFirstOf", "Log"}\n'
            'for _, name in ipairs(api) do\n'
            '  _G[name]("Item")\n'
            'end\n',
    })
    out["dot_bypass"] = z.normal("dot_bypass.zip", {
        "mod/init.lua":
            'local g = _G\nlocal n = "Find" .. "FirstOf"\ng[n]("Item")\n',
    })
    return out


def out_lua_zip_payload():
    return os.path.join(CORPUS, "detector", "_payload.zip")


def build_lua_sources():
    """Bare .lua files for compat-checker (syntax/obfuscation/dynamic)."""
    d = os.path.join(CORPUS, "lua")
    os.makedirs(d, exist_ok=True)
    files = {
        "clean.lua": (
            'local mod = {}\n'
            'function mod.onLoad()\n'
            '  local it = FindFirstOf("Item")\n'
            '  while it do\n'
            '    it = FindNextOf(it)\n'
            '  end\n'
            'end\n'
            'return mod\n'),
        "syntax_err.lua": (
            'local x = 1\n'
            'if x then\n'
            '  local y = = 2\n'          # '=' '=' -> syntax error
            'end\n'),
        "syntax_err_late.lua": "local ok = 1\n" + ("-- pad\n" * 500) + "if then end\n",
        "obfuscated_indexed.lua": (
            'local names = {"FindFirstOf","FindNextOf","Log"}\n'
            'for i=1,#names do _G[names[i]]("x") end\n'),
        "obfuscated_loadstring.lua": (
            'local f = _G["lo"..("ad" )] or load\n'
            'f("\\108\\111\\97\\100")("return 1")()\n'),
        "obfuscated_charcode.lua": (
            'local s = string.char(70,105,110,100,70,105,114,115,116,79,102)\n'
            '_G[s]("Item")\n'),
        "dynamic_dispatch_env.lua": (
            'local env = setmetatable({}, {__index=_G})\n'
            'env.api = "FindFirstOf"\n'
            'env[env.api]("Item")\n'),
        "metamethod_bypass.lua": (
            'local t = setmetatable({}, {__index=function(_,k)\n'
            '  return _G[k:reverse()] end})\n'
            't["fOtsriFdniF"]("Item")\n'),   # reversed "FindFirstOf"
        "empty.lua": "",
        "only_comments.lua": "-- nothing executable\n--[[ really nothing ]]\n",
        "huge_line.lua": "local x = " + "1+" * 20000 + "1\n",
        "deep_nesting.lua": "x = " + "(" * 900 + "1" + ")" * 900 + "\n",
        "goto_spaghetti.lua": (
            '::top::\ngoto mid\n::mid::\ngoto top\n'),
        "utf16_lookalike.lua": (
            '-- looks fine\n'
            'local FindFirstOf = FindFirstOf -- fullwidth colon\uff1a\n'
            'FindFirstOf\uff08"Item"\uff09\n'),  # fullwidth parens = syntax error
        "shebang.lua": '#!/usr/bin/env lua\nprint("hi")\n',
        "crlf.lua": 'local x = 1\r\nif x then\r\n  Log("y")\r\nend\r\n',
        "null_bytes.lua": b'local x = 1\x00\nLog("n")\n',
        "api_comment_only.lua": (
            '-- TODO: call FindFirstOf later\n'
            '-- FindNextOf, Log\n'
            'return {}\n'),
        "string_in_comments_trap.lua": (
            '-- the string "FindFirstOf" appears here only in a comment\n'
            'local s = [[ FindNextOf ]]\n'
            'return s\n'),
    }
    for fn, content in files.items():
        mode = "wb" if isinstance(content, bytes) else "w"
        with open(os.path.join(d, fn), mode) as f:
            f.write(content)
    return {fn: os.path.join(d, fn) for fn in files}


def build_uninstall_cases():
    """Manifest fixtures for the uninstaller."""
    d = os.path.join(CORPUS, "uninstall")
    os.makedirs(d, exist_ok=True)
    files = {
        "missing_files.json": (
            '{"name":"Ghost","files":["gone/a.lua","gone/b.lua",'
            '"also/missing.txt"]}'),
        "circular_a.json": '{"name":"A","depends":["B"]}',
        "circular_b.json": '{"name":"B","depends":["A"]}',
        "self_dep.json": '{"name":"Self","depends":["Self"]}',
        "dep_on_present.json": '{"name":"Child","depends":["Parent"]}',
        "dep_on_missing.json": '{"name":"Orphan","depends":["Nobody"]}',
        "empty_manifest.json": "{}",
        "null_files.json": '{"name":"N","files":null}',
        "abs_paths.json": (
            '{"name":"Abs","files":["/etc/passwd","/tmp/evil.txt"]}'),
        "parent_dirs.json": (
            '{"name":"Up","files":["../../escape.lua","ok.lua"]}'),
        "dup_files.json": '{"name":"D","files":["a.lua","a.lua","a.lua"]}',
        "dir_and_file.json": (
            '{"name":"Mix","files":["dir/","dir/file.lua"]}'),
        "weird_unicode.json": (
            '{"name":"Uni","files":["mods/日本語/mod.lua","mods/über/x.bin"]}'),
        "huge_list.json": '{"name":"Huge","files":['
                          + ",".join(f'"f{i}.lua"' for i in range(5000)) + "]}",
    }
    for fn, content in files.items():
        with open(os.path.join(d, fn), "w") as f:
            f.write(content)
    return {fn: os.path.join(d, fn) for fn in files}


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "build":
        # payload zip for lua_is_zip detector case
        os.makedirs(os.path.join(CORPUS, "detector"), exist_ok=True)
        pk = ZipKit(os.path.join(CORPUS, "detector"))
        pk.normal("_payload.zip", {"inner/init.lua": "-- inner\n"})
        a = build_installer_cases()
        b = build_detector_cases()
        c = build_lua_sources()
        e = build_uninstall_cases()
        total = len(a) + len(b) + len(c) + len(e)
        print(f"corpus built: {total} fixtures under {CORPUS}")


if __name__ == "__main__":
    main()
