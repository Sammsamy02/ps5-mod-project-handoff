#!/usr/bin/env python3
"""Adversarial suite for mod-types detector. Mac-only, host-tested."""

import io
import json
import os
import subprocess
import sys
import tarfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import ZipKit, DETECTOR, CORPUS  # noqa: E402

ENV = dict(os.environ, PYTHONIOENCODING="utf-8")

PE = b"MZ\x90\x00" + b"\x00" * 60 + b"PE\x00\x00" + b"\x00" * 200
ELF = b"\x7fELF" + b"\x02\x01\x01\x00" + b"\x00" * 120


def classify(sb, path):
    p = subprocess.run([sys.executable, DETECTOR, "--compact", path],
                       capture_output=True, text=True, timeout=180, env=ENV)
    try:
        data = json.loads(p.stdout)
    except ValueError:
        return None, p
    if isinstance(data, list) and data:
        data = data[0]
    return data, p


def register(h):
    def must(sb, name, checker):
        data, proc = classify(sb, name)
        A.ok(data is not None,
             f"classifier crashed on {name}: rc={proc.returncode} "
             f"{proc.stderr[:200]}")
        checker(data)

    # ------------------------------------------------- misleading names
    @h.add("det.lua_actually_text", "detector")
    def _(sb=None):
        z = ZipKit(sb.path)
        p = z.normal("t.zip", {"mod/init.lua":
                               "this is definitely not lua code, just prose\n"})
        must(sb, p, lambda d: A.ok(
            d["label"] in ("unknown", "config-only", "ue4ss-lua"),
            f"unexpected label for prose .lua: {d['label']}"))

    @h.add("det.lua_actually_pe_in_layout", "detector")
    def _(sb=None):
        """PE binary renamed init.lua INSIDE Mods/<n>/Scripts/ — the structure
        regex alone will call it ue4ss-lua; check it at least doesn't crash
        and record what the module claims."""
        z = ZipKit(sb.path)
        p = z.normal("t.zip", {"Mods/Fake/Scripts/main.lua": PE})
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed on PE-as-lua")
        # record actual behavior for the report
        print(f"    [observed] PE-as-main.lua -> label={data['label']} "
              f"flags={data['flags'].get('windows_dll_present')}")

    @h.add("det.dll_renamed_lua", "detector")
    def _(sb=None):
        z = ZipKit(sb.path)
        p = z.normal("t.zip", {"mod/native.dll.lua": PE,
                               "mod/plugin.lua.dll": PE})
        must(sb, p, lambda d: A.ok(
            d["flags"].get("windows_dll_present") is True or
            "ue4ss-cpp" in d["mod_types"],
            f"renamed dlls not flagged: {d['mod_types']} {d['flags']}"))

    @h.add("det.lua_is_zip_payload", "detector")
    def _(sb=None):
        z = ZipKit(sb.path)
        inner = io.BytesIO()
        with zipfile.ZipFile(inner, "w") as iz:
            iz.writestr("Mods/Real/Scripts/main.lua",
                        "FindFirstOf('Item')\n")
        p = z.normal("t.zip", {"weird/init.lua": inner.getvalue()})
        must(sb, p, lambda d: None)  # must not crash; classification is bonus

    # ------------------------------------------------------ empty mods
    @h.add("det.empty_mod", "detector")
    def _(sb=None):
        z = ZipKit(sb.path)
        p = z.normal("t.zip", {"mod/": ""})
        must(sb, p, lambda d: A.ok(
            d["label"] in ("unknown", "config-only"),
            f"empty mod classified as {d['label']}"))

    @h.add("det.empty_archive", "detector")
    def _(sb=None):
        p = os.path.join(sb.path, "t.zip")
        open(p, "wb").close()
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed on empty file")
        A.contains(json.dumps(data), "error", "empty file silently classified")

    @h.add("det.all_empty_files", "detector")
    def _(sb=None):
        p = os.path.join(CORPUS, "installer", "empty_files.zip")
        must(sb, p, lambda d: None)

    # ------------------------------------------------------ bulk inputs
    @h.add("det.ten_thousand_files", "detector")
    def _(sb=None):
        p = os.path.join(CORPUS, "detector", "many_files.zip")
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed on 10k files")
        A.ok(data.get("entries_scanned", 0) <= 5000,
             f"scanned {data.get('entries_scanned')} > 5000 limit")
        notes = " ".join(data.get("notes", []))
        A.contains(notes, "entry limit", "truncation not surfaced in notes")

    @h.add("det.deep_nested_zip", "detector")
    def _(sb=None):
        p = os.path.join(CORPUS, "installer", "nested.zip.layer19")
        data, proc = classify(sb, p)
        A.ok(data is not None, f"crashed rc={proc.returncode}")

    # ------------------------------------------- obfuscation/dynamics
    @h.add("det.obfuscated_loader", "detector")
    def _(sb=None):
        p = os.path.join(CORPUS, "detector", "obfuscated_loader.zip")
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed")
        print(f"    [observed] obfuscated loader -> {data['label']} "
              f"evidence={data['evidence']}")

    @h.add("det.dynamic_dispatch", "detector")
    def _(sb=None):
        p = os.path.join(CORPUS, "detector", "dynamic_dispatch.zip")
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed")
        print(f"    [observed] dynamic dispatch -> {data['label']} "
              f"evidence={data['evidence']}")

    @h.add("det.dot_bypass", "detector")
    def _(sb=None):
        p = os.path.join(CORPUS, "detector", "dot_bypass.zip")
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed")

    # ----------------------------------------------- other formats
    @h.add("det.tar_and_tgz", "detector")
    def _(sb=None):
        tp = os.path.join(sb.path, "t.tar")
        with tarfile.open(tp, "w") as tf:
            b = b"FindFirstOf('Item')\n"
            ti = tarfile.TarInfo("Mods/T/Scripts/main.lua")
            ti.size = len(b)
            tf.addfile(ti, io.BytesIO(b))
        must(sb, tp, lambda d: A.eq(d["label"], "ue4ss-lua",
                                    f"tar mod labeled {d['label']}"))
        gz = os.path.join(sb.path, "t.tar.gz")
        with tarfile.open(gz, "w:gz") as tf:
            tf.addfile(ti, io.BytesIO(b))
        must(sb, gz, lambda d: A.eq(d["label"], "ue4ss-lua",
                                    f"tar.gz mod labeled {d['label']}"))

    @h.add("det.tar_device_entries", "detector")
    def _(sb=None):
        tp = os.path.join(sb.path, "dev.tar")
        with tarfile.open(tp, "w") as tf:
            b = b"x"
            ti = tarfile.TarInfo("Mods/T/Scripts/main.lua")
            ti.size = 1
            tf.addfile(ti, io.BytesIO(b))
            dv = tarfile.TarInfo("dev/node")
            dv.type = tarfile.CHRTYPE  # device entry
            dv.devmajor = 1
            dv.devminor = 3
            tf.addfile(dv)
            ln = tarfile.TarInfo("link")
            ln.type = tarfile.SYMTYPE
            ln.linkname = "/etc/passwd"
            tf.addfile(ln)
        data, proc = classify(sb, tp)
        A.ok(data is not None, "crashed on tar with device/symlink entries")
        A.ok("error" not in data or data.get("label"),
             f"unexpected error: {data}")

    @h.add("det.rar_7z_sniffed_unsupported", "detector")
    def _(sb=None):
        rar = os.path.join(sb.path, "t.rar")
        with open(rar, "wb") as f:
            f.write(b"Rar!\x1a\x07\x01\x00" + b"\x00" * 64)
        z7 = os.path.join(sb.path, "t.7z")
        with open(z7, "wb") as f:
            f.write(b"7z\xbc\xaf\x27\x1c" + b"\x00" * 64)
        for p in (rar, z7):
            data, proc = classify(sb, p)
            A.ok(data is not None, "crashed on rar/7z")
            A.contains(json.dumps(data), "unsupported",
                       f"{p} not reported unsupported")

    @h.add("det.not_an_archive", "detector")
    def _(sb=None):
        p = os.path.join(sb.path, "t.zip")
        with open(p, "wb") as f:
            f.write(b"MZ\x90\x00 garbage not an archive")
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed")
        A.contains(json.dumps(data), "error", "garbage silently classified")

    @h.add("det.missing_file", "detector")
    def _(sb=None):
        data, proc = classify(sb, os.path.join(sb.path, "nope.zip"))
        A.ok(data is not None, "crashed on missing file")
        A.contains(json.dumps(data), "error", "missing file not an error")

    # ------------------------------------------- requirements parsing
    @h.add("det.requirements_adversarial_text", "detector")
    def _(sb=None):
        cases = [
            ("Requires UE4SS 3.0.1 or later.", "ue4ss", "3.0.1"),
            ("Requires the \"Enhanced Inventory\" mod.", "mod",
             "enhancedinventory"),
            ("Requires EXNOTIK plugin.", "plugin", None),
            ("Works with game version 1.2.3.", "game", "1.2.3"),
            ("nexusmods.com/games/cyberpunk2077/mods/12345", "nexus",
             "12345"),
            # negative: mention without cue word should NOT become a req
            ("UE4SS 2.5 was released yesterday.", "nue4ss", None),
        ]
        for text, kind, expect in cases:
            z = ZipKit(sb.path)
            p = z.normal("r.zip", {"Readme.txt": text,
                                   "Mods/R/Scripts/main.lua": "x\n"})
            data, _ = classify(sb, p)
            A.ok(data is not None, f"classifier crashed: {text!r}")
            r = data["requirements"]
            if kind == "ue4ss":
                A.ok(r["ue4ss_version"] and r["ue4ss_version"]["version"]
                     == expect, f"ue4ss req not parsed: {text!r} -> {r}")
            elif kind == "nue4ss":
                A.is_none(r["ue4ss_version"],
                          f"mention without cue became requirement: {text!r}")
            elif kind == "mod":
                A.ok(any(expect in (d.get("name") or "").lower().replace(" ", "")
                         for d in r["other_mods"]),
                     f"named mod dep not parsed: {text!r} -> {r['other_mods']}")
            elif kind == "plugin":
                A.ok(len(r["plugins"]) >= 1,
                     f"plugin dep not parsed: {text!r} -> {r['plugins']}")
            elif kind == "game":
                A.ok(r["game_version"] and r["game_version"]["version"]
                     == expect, f"game req not parsed: {text!r} -> {r}")
            elif kind == "nexus":
                A.ok(any(d.get("nexus_id") == expect for d in r["other_mods"]),
                     f"nexus id not parsed: {text!r} -> {r['other_mods']}")

    @h.add("det.huge_metadata_file", "detector")
    def _(sb=None):
        z = ZipKit(sb.path)
        p = z.normal("t.zip", {"Readme.txt": "Requires UE4SS 3.0.1.\n" + "x" * 5000000,
                               "Mods/R/Scripts/main.lua": "y\n"})
        data, proc = classify(sb, p)
        A.ok(data is not None, "crashed on 5MB readme")
