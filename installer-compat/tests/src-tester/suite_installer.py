#!/usr/bin/env python3
"""Adversarial + integration suite for installer-core. Mac-only, host-tested.

Runs the module's real CLI (subprocess) against hostile fixtures from
corpus/ plus purpose-built ones. Every filesystem effect is confined to
framework Sandbox dirs (tester/runs/).

CONTEXT (found during review, proven dynamically):
  BUG-1 line 1013: `config_writes` is never defined -> NameError whenever an
      install would modify loadlist.txt (i.e. every first install).
  BUG-2 need_dir(): the leaf directory is added to seen_dirs before the
      chain walk, so mod dirs are never mkdir-planned -> first file write
      fails ENOENT even when BUG-1 is dodged.
  Tests that need a successful install therefore use seed_tree(): pre-create
  the mod dir tree, pre-seed loadlist/ploader with the EXACT bytes the
  installer would produce, and pre-place a manifest stub (ownership guard).
  This routes past both blockers WITHOUT editing the module and lets the
  journal/rollback/verify/tamper machinery be exercised for real.

Framework: python3 framework.py suite_installer.py
"""

import json
import os
import subprocess
import sys
import threading
import time
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import ZipKit, INSTALLER, CORPUS  # noqa: E402

TITLE = "PPSA01325"
ENV = dict(os.environ, PYTHONIOENCODING="utf-8")


class Target:
    def __init__(self, sb):
        self.sb = sb
        self.roots = os.path.join(sb.path, "roots")
        self.plugins = os.path.join(sb.path, "plugins")
        self.state = os.path.join(sb.path, "state")

    def run(self, *args, timeout=120):
        p = subprocess.run(
            [sys.executable, INSTALLER, *args],
            capture_output=True, text=True, timeout=timeout, env=ENV)
        return p.returncode, p.stdout, p.stderr

    def install(self, zip_path, metadata, extra=(), timeout=120):
        mdp = os.path.join(self.sb.path, "meta.json")
        with open(mdp, "w") as f:
            json.dump(metadata, f)
        return self.run("install", "--archive", zip_path,
                        "--metadata", mdp, "--title", TITLE,
                        "--roots", self.roots, "--plugins", self.plugins,
                        "--state", self.state, *extra, timeout=timeout)

    def verify(self, mod):
        return self.run("verify", "--title", TITLE, "--mod", mod,
                        "--roots", self.roots, "--plugins", self.plugins,
                        "--state", self.state)

    def recover(self):
        return self.run("recover", "--roots", self.roots,
                        "--plugins", self.plugins, "--state", self.state)

    def _read(self, rel):
        p = os.path.join(self.roots, TITLE, rel) \
            if rel.startswith("loadlist") else rel
        if rel == "ploader":
            p = os.path.join(self.plugins, "ploader.ini")
        if not os.path.exists(p):
            return None
        with open(p, "rb") as f:
            return f.read()

    def loadlist(self):
        return self._read("loadlist.txt")

    def ploader(self):
        return self._read("ploader")

    def journals(self):
        d = os.path.join(self.state, "journal")
        if not os.path.isdir(d):
            return []
        return sorted(os.listdir(d))


def role_of(rel):
    low = rel.lower()
    if low.startswith("scripts/") and low.endswith(".lua"):
        return "lua"
    if low.startswith("config/") or low.endswith(".ini") or low.endswith(".cfg"):
        return "config"
    return "asset"


def mod_entries(mod, main=b"print('hello')\n", extra=None):
    e = {"%s/Scripts/main.lua" % mod: main}
    if extra:
        e.update({"%s/%s" % (mod, k): v for k, v in extra.items()})
    return e


def write_zip(sb, name, entries):
    z = ZipKit(sb.path)
    return z.normal(name, entries)


def md(name="MyMod", version="1.0.0", **kw):
    d = {"name": name, "version": version}
    d.update(kw)
    return d


def loadlist_text(entries, mod, version, title=TITLE):
    rows = []
    for rel in sorted(entries):
        rows.append("file\t%s\tue4ss/Mods/%s/%s\t%d"
                    % (role_of(rel), mod, rel, len(entries[rel])))
    return "\n".join(["modforge-loadlist\t1", "title\t%s" % title,
                      "mod\t%s\t%s" % (mod, version)] + rows) + "\n"


def seed_tree(t, entries, mod="MyMod", version="1.0.0", full=True,
              prx="modbridge_supplier_composition.prx", mode="2"):
    """Pre-seed configs (and optionally dirs+manifest stub). Used to test
    behavior over pre-existing state; fresh installs work on the current
    module, so most tests no longer need this."""
    title_root = os.path.join(t.roots, TITLE)
    os.makedirs(title_root, exist_ok=True)
    with open(os.path.join(title_root, "loadlist.txt"), "wb") as f:
        f.write(loadlist_text(entries, mod, version).encode("utf-8"))
    os.makedirs(t.plugins, exist_ok=True)
    with open(os.path.join(t.plugins, "ploader.ini"), "w") as f:
        f.write("[%s]\n%s=%s\n" % (TITLE, prx, mode))
    if full:
        mod_dir = os.path.join(title_root, "ue4ss", "Mods", mod)
        dirs = set()
        for rel in entries:
            parts = rel.split("/")[:-1]
            for i in range(1, len(parts) + 1):
                dirs.add(os.path.join(mod_dir, *parts[:i]))
        dirs.add(mod_dir)
        for d in sorted(dirs, key=len):
            os.makedirs(d, exist_ok=True)
        os.makedirs(os.path.join(title_root, "manifests"), exist_ok=True)
        with open(os.path.join(title_root, "manifests", mod + ".json"),
                  "w") as f:
            json.dump({"schema": "modbridge-manifest/1", "mod_dir": mod,
                       "files": [
                           {"install_path": os.path.join(
                               mod_dir, *rel.split("/")),
                            "original_sha256": "0" * 64}
                           for rel in sorted(entries)]}, f)


def register(h):
    # =================================================================malformed
    @h.add("inst.reject.truncated", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.install(os.path.join(CORPUS, "installer",
                                              "truncated.zip"), md())
        A.ok(rc in (1, 2), f"truncated zip: rc={rc} err={err[:200]}")

    @h.add("inst.reject.not_a_zip", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.install(os.path.join(CORPUS, "installer",
                                              "not_a_zip.zip"), md())
        A.eq(rc, 2, f"garbage bytes: rc={rc} {err[:200]}")

    @h.add("inst.reject.empty_zip", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.install(os.path.join(CORPUS, "installer",
                                              "empty.zip"), md())
        A.eq(rc, 2, f"empty zip rc={rc} {err[:120]}")

    @h.add("inst.reject.symlinks", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.install(os.path.join(CORPUS, "installer",
                                              "symlink.zip"), md())
        A.ok(rc != 0, "symlink zip accepted")
        A.ok(not any(os.path.islink(os.path.join(r, f))
                     for r, _d, fs in os.walk(sb.path) for f in fs),
             "a symlink was materialized on disk")

    @h.add("inst.reject.traversal", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.install(os.path.join(CORPUS, "installer",
                                              "traversal.zip"), md())
        A.ok(rc != 0, "traversal zip accepted")
        A.ok(not os.path.exists(os.path.join(sb.path, "escape.txt")),
             "../escape.txt escaped the sandbox root")

    @h.add("inst.reject.too_many_entries", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.install(os.path.join(CORPUS, "installer",
                                              "many_files.zip"), md())
        A.eq(rc, 2, f"10k-entry zip rc={rc} (limit 4096)")

    @h.add("inst.reject.no_main_lua", "installer")
    def _(sb=None):
        t = Target(sb)
        p = write_zip(sb, "nolua.zip", {"docs/readme.txt": "hi\n"})
        rc, out, err = t.install(p, md())
        A.eq(rc, 2, "archive without Scripts/main.lua accepted")

    @h.add("inst.reject.encrypted", "installer")
    def _(sb=None):
        t = Target(sb)
        base = write_zip(sb, "enc_base.zip",
                         {"MyMod/Scripts/main.lua": "x\n"})
        p = os.path.join(sb.path, "enc.zip")
        with open(base, "rb") as f:
            data = bytearray(f.read())
        data[6] |= 0x01  # local header flag bit
        # also flip the flag in every central-directory entry (PK\x01\x02,
        # general purpose bit flag sits 8 bytes into the record)
        i = 0
        while True:
            i = data.find(b"PK\x01\x02", i)
            if i == -1:
                break
            data[i + 8] |= 0x01
            i += 4
        with open(p, "wb") as f:
            f.write(bytes(data))
        rc, out, err = t.install(p, md())
        A.ok(rc != 0, "encrypted-flag zip accepted")

    @h.add("inst.reject.backslash_names", "installer")
    def _(sb=None):
        """Backslash names must be treated as separators (Windows zips), not
        literal filenames containing '\\'."""
        t = Target(sb)
        p = write_zip(sb, "bs.zip", {"MyMod/Scripts/main.lua": "x\n",
                                     "MyMod\\win\\extra.txt": "y\n"})
        seed_tree(t, mod_entries("MyMod"), full=False)  # dirs only, no files
        rc, out, err = t.install(p, md())
        # 'MyMod\win\extra.txt' -> normalized MyMod/win/extra.txt (skipped?
        # it's inside mod root). Either accepted-as-split or rejected — but
        # never a literal backslash filename on disk.
        bs = [f for r, _d, fs in os.walk(t.roots) for f in fs if "\\" in f]
        A.ok(not bs, f"literal backslash filenames created: {bs[:3]}")

    # ================================================================configs
    @h.add("inst.config.bom_crlf_loadlist", "installer")
    def _(sb=None):
        t = Target(sb)
        title_root = os.path.join(t.roots, TITLE)
        os.makedirs(title_root, exist_ok=True)
        with open(os.path.join(title_root, "loadlist.txt"), "wb") as f:
            f.write(b"\xef\xbb\xbfmodforge-loadlist\t1\r\ntitle\tother\r\n")
        z = write_zip(sb, "m.zip", mod_entries("MyMod"))
        rc, out, err = t.install(z, md())
        A.ok(rc != 0, f"weird loadlist accepted rc={rc}")
        A.contains(err + out, "unrecognized",
                   "missing unrecognized-format error")

    @h.add("inst.config.foreign_title_loadlist", "installer")
    def _(sb=None):
        t = Target(sb)
        title_root = os.path.join(t.roots, TITLE)
        os.makedirs(title_root, exist_ok=True)
        with open(os.path.join(title_root, "loadlist.txt"), "w") as f:
            f.write("modforge-loadlist\t1\ntitle\tOTHERGAME\nmod\tZ\t1\n")
        z = write_zip(sb, "m.zip", mod_entries("MyMod"))
        rc, out, err = t.install(z, md())
        A.ok(rc != 0, "loadlist for another title was silently adopted")

    @h.add("inst.config.ploader_garbage", "installer")
    def _(sb=None):
        t = Target(sb)
        os.makedirs(t.plugins, exist_ok=True)
        raw = b"\x00\x01garbage\xff no sections at all"
        with open(os.path.join(t.plugins, "ploader.ini"), "wb") as f:
            f.write(raw)
        z = write_zip(sb, "m.zip", mod_entries("MyMod"))
        rc, out, err = t.install(z, md())
        A.ok(rc in (0, 1, 2), f"rc={rc}")
        pl = t.ploader()
        if rc != 0 and pl is not None:
            A.ok(raw in pl or b"garbage" in pl,
                 "garbage ploader.ini was destroyed")

    @h.add("inst.metadata.weird", "installer")
    def _(sb=None):
        t = Target(sb)
        z = write_zip(sb, "m.zip", mod_entries("MyMod"))
        for bad in [{"name": ""}, {"name": "  "}, {"name": "../../etc"},
                    {"name": 123}, {"version": None},
                    {"name": "ok", "version": ""},
                    {"name": "a" * 300},
                    {"name": "ok", "version": "1", "ploader_prx":
                     {"name": "../evil.prx"}},
                    {"name": "ok", "version": "1", "ploader_prx":
                     {"name": "x.prx", "mode": "9"}},
                    {"name": "ok", "version": "1", "ue4ss_settings":
                     {"[general]": {"x": "y"}}},
                    {"name": "ok", "version": "1", "ue4ss_settings":
                     {"general": {"bad\nkey": "y"}}}]:
            rc, out, err = t.install(z, bad)
            A.ok(rc == 2, f"metadata {str(bad)[:60]} accepted rc={rc}")

    # ============================================primary path (blocker bugs)
    @h.add("inst.happy.fresh_install_must_succeed", "installer")
    def _(sb=None):
        """Fresh-roots install of a clean mod must exit 0. (Failed at review
        time 13:49 due to config_writes NameError + need_dir ENOENT; the
        module authors fixed both at 13:52 — this test pins the fix.)"""
        t = Target(sb)
        z = write_zip(sb, "m.zip", mod_entries("MyMod"))
        rc, out, err = t.install(z, md())
        A.eq(rc, 0, f"fresh install failed rc={rc}: {err[:300]}")
        rc2, out2, err2 = t.verify("MyMod")
        A.eq(rc2, 0, f"verify after fresh install rc={rc2}: {err2[:200]}")
        A.contains(t.loadlist() or b"", b"mod\tMyMod\t1.0.0",
                   "loadlist row missing after fresh install")
        A.contains(t.ploader() or b"", b"modbridge_supplier_composition.prx=2",
                   "ploader entry missing after fresh install")

    @h.add("inst.happy.preseeded_exact_configs_noop", "installer")
    def _(sb=None):
        """With configs pre-seeded to the exact planned content, install must
        still succeed and treat config writes as no-ops."""
        t = Target(sb)
        entries = mod_entries("MyMod")
        z = write_zip(sb, "m.zip", entries)
        seed_tree(t, entries, full=False)  # configs only, no dirs/manifest
        rc, out, err = t.install(z, md())
        A.eq(rc, 0, f"install with pre-seeded configs failed rc={rc}: "
                    f"{err[:300]}")

    # =====================================================transaction/recovery
    @h.add("inst.tx.crash_midway_recovers", "installer")
    def _(sb=None):
        t = Target(sb)
        entries = mod_entries("MyMod", extra={
            "assets/big1.bin": b"1" * 200000,
            "assets/big2.bin": b"2" * 200000,
            "assets/big3.bin": b"3" * 200000})
        z = write_zip(sb, "m.zip", entries)
        rc, out, err = t.install(z, md(), extra=("--crash-after", "6"))
        A.eq(rc, 86, f"crash injection rc={rc}: {err[:200]}")
        rc3, out3, err3 = t.recover()
        A.eq(rc3, 0, f"recover rc={rc3}: {err3[:200]}")
        mainp = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "MyMod",
                             "Scripts", "main.lua")
        A.ok(not os.path.exists(mainp),
             "crashed install's files survived recover")
        A.eq(t.journals(), [], "journal survived recover")
        ll = t.loadlist() or ""
        A.ok("mod\tMyMod" not in ll,
             "crashed install's loadlist row survived recover")
        # reinstall cleanly over the rolled-back state
        rc4, out4, err4 = t.install(z, md())
        A.eq(rc4, 0, f"reinstall after recovery failed rc={rc4}: "
                     f"{err4[:300]}")

    @h.add("inst.tx.corrupt_journal_quarantined", "installer")
    def _(sb=None):
        t = Target(sb)
        jd = os.path.join(t.state, "journal")
        os.makedirs(jd, exist_ok=True)
        with open(os.path.join(jd, "garbage.json"), "wb") as f:
            f.write(b"{not json at all")
        rc, out, err = t.recover()
        A.eq(rc, 0, f"recover rc={rc}: {err[:200]}")
        left = os.listdir(jd)
        A.ok(all("corrupt" in n for n in left),
             f"corrupt journal not quarantined: {left}")

    @h.add("inst.tx.ownership_guard", "installer")
    def _(sb=None):
        t = Target(sb)
        pre = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "MyMod")
        os.makedirs(os.path.join(pre, "Scripts"), exist_ok=True)
        with open(os.path.join(pre, "Scripts", "main.lua"), "w") as f:
            f.write("-- precious hand-installed mod\n")
        z = write_zip(sb, "m.zip", mod_entries("MyMod",
                                               main=b"-- evil replacement\n"))
        seed_tree(t, mod_entries("MyMod", main=b"-- evil replacement\n"),
                  full=False)
        rc, out, err = t.install(z, md())
        A.eq(rc, 2, "installer overwrote an unmanaged mod dir")
        with open(os.path.join(pre, "Scripts", "main.lua")) as f:
            A.contains(f.read(), "precious",
                       "hand-installed file clobbered")

    # ============================================================unicode/paths
    @h.add("inst.unicode.names", "installer")
    def _(sb=None):
        t = Target(sb)
        entries = mod_entries("MyMod", extra={
            "Scripts/über.lua": "-- unicode name\n",
            "Scripts/日本語.lua": "-- japanese\n"})
        z = write_zip(sb, "m.zip", entries)
        rc, out, err = t.install(z, md())
        A.eq(rc, 0, f"unicode filenames failed rc={rc}: {err[:300]}")

    @h.add("inst.unicode.nfc_nfd_collision", "installer")
    def _(sb=None):
        """Two names differing only by NFC/NFD collide on macOS. Installer
        must land deterministically: controlled rejection or a clean install
        with verify-consistent state — never a silent mixed state."""
        t = Target(sb)
        nfc = "Scripts/caf\u00e9.lua"
        nfd = "Scripts/cafe\u0301.lua"
        entries = {"MyMod/Scripts/main.lua": "x\n",
                   "MyMod/" + nfc: "-- nfc\n",
                   "MyMod/" + nfd: "-- nfd\n"}
        z = write_zip(sb, "nfc.zip", entries)
        rc, out, err = t.install(z, md())
        A.ok(rc in (0, 2, 3), f"rc={rc} {err[:200]}")
        if rc == 0:
            rc2, out2, _ = t.verify("MyMod")
            A.eq(rc2, 0, "install reported ok but verify found problems "
                         "(nfc/nfd mixed state)")

    # ============================================================dup members
    @h.add("inst.dup.same_dest_members", "installer")
    def _(sb=None):
        """Two members normalizing to the same install path: ideally rejected
        at inspection (exit 2); acceptable: caught by verification (exit 3)
        with full rollback."""
        t = Target(sb)
        p = os.path.join(sb.path, "dupdest.zip")
        with zipfile.ZipFile(p, "w") as zz:
            zz.writestr("MyMod/Scripts/main.lua", "-- version one\n")
            zz.writestr("MyMod/Scripts/./main.lua", "-- version two\n")
        rc, out, err = t.install(p, md())
        A.ok(rc in (2, 3), f"same-dest members not caught properly rc={rc} "
                           f"{err[:200]}")

    # ============================================================concurrency
    @h.add("inst.concurrent.same_mod_current_behavior", "installer")
    def _(sb=None):
        """Two concurrent installs of the same mod name with different
        content. The module has no lock (no flock/fcntl import anywhere);
        race outcomes: one wins / both fail cleanly — never a lying state
        (verify agrees with reality) and journals always resolve."""
        t = Target(sb)
        entries_a = mod_entries("MyMod", main=b"-- A\n")
        entries_b = mod_entries("MyMod", main=b"-- B\n")
        za = write_zip(sb, "a.zip", entries_a)
        zb = write_zip(sb, "b.zip", entries_b)
        results = []

        def run(zp):
            mdp = os.path.join(sb.path, "meta_%s" % os.path.basename(zp))
            with open(mdp, "w") as f:
                json.dump(md(), f)
            results.append(t.run(
                "install", "--archive", zp, "--metadata", mdp,
                "--title", TITLE, "--roots", t.roots, "--plugins",
                t.plugins, "--state", t.state, timeout=300))

        th1 = threading.Thread(target=run, args=(za,))
        th2 = threading.Thread(target=run, args=(zb,))
        th1.start()
        time.sleep(0.3)
        th2.start()
        th1.join(300)
        th2.join(300)
        rc3, _o, _e = t.recover()
        A.eq(rc3, 0, "recover after concurrent installs failed")
        A.eq(t.journals(), [], f"journals left unresolved: {t.journals()}")
        A.ok(all(r[0] in (0, 1, 2, 3, 4, 86) for r in results),
             f"weird exit codes: {[r[0] for r in results]}")
        # any surviving install must verify consistently
        for _r in results:
            rcv, outv, _ev = t.verify("MyMod")
            A.ok(rcv in (0, 2, 3),
                 f"verify rc {rcv} inconsistent after race")
            break

    # ================================================================tamper
    @h.add("inst.tamper.verify_catches", "installer")
    def _(sb=None):
        t = Target(sb)
        entries = mod_entries("MyMod")
        z = write_zip(sb, "m.zip", entries)
        rc, out, err = t.install(z, md())
        A.eq(rc, 0, f"install failed rc={rc}: {err[:300]}")
        mainp = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "MyMod",
                             "Scripts", "main.lua")
        with open(mainp, "w") as f:
            f.write("print('tampered!')\n")
        rc2, out2, _ = t.verify("MyMod")
        A.eq(rc2, 3, "verify did not catch tampering")
        A.contains(out2, "tampered", "verify report lacks tamper detail")

    @h.add("inst.tamper.missing_file", "installer")
    def _(sb=None):
        t = Target(sb)
        entries = mod_entries("MyMod")
        z = write_zip(sb, "m.zip", entries)
        seed_tree(t, entries)
        rc, *_ = t.install(z, md())
        A.eq(rc, 0, "install failed")
        mainp = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "MyMod",
                             "Scripts", "main.lua")
        os.remove(mainp)
        rc2, out2, _ = t.verify("MyMod")
        A.eq(rc2, 3, "verify passed with deleted file")
        A.contains(out2, "missing", "verify report lacks missing detail")

    @h.add("inst.tamper.loadlist_row_removed", "installer")
    def _(sb=None):
        t = Target(sb)
        entries = mod_entries("MyMod")
        z = write_zip(sb, "m.zip", entries)
        seed_tree(t, entries)
        rc, *_ = t.install(z, md())
        A.eq(rc, 0, "install failed")
        ll = os.path.join(t.roots, TITLE, "loadlist.txt")
        txt = open(ll).read().replace("mod\tMyMod\t1.0.0\n", "")
        open(ll, "w").write(txt)
        rc2, out2, _ = t.verify("MyMod")
        A.eq(rc2, 3, "verify passed with loadlist row removed")

    # ===============================================================reinstall
    @h.add("inst.reinstall.over_managed", "installer")
    def _(sb=None):
        t = Target(sb)
        entries = mod_entries("MyMod")
        z = write_zip(sb, "m.zip", entries)
        rc, *_ = t.install(z, md())
        A.eq(rc, 0, "initial install failed")
        entries2 = mod_entries("MyMod", main=b"print('v2')\n")
        z2 = write_zip(sb, "m2.zip", entries2)
        rc2, out2, err2 = t.install(z2, md(version="2.0"))
        A.ok(rc2 in (0, 2, 3), f"reinstall rc={rc2} {err2[:200]}")
        if rc2 == 0:
            mainp = os.path.join(t.roots, TITLE, "ue4ss", "Mods", "MyMod",
                                 "Scripts", "main.lua")
            A.eq(open(mainp, "rb").read(), b"print('v2')\n",
                 "reinstall did not update main.lua")

    # ===============================================================verify CLI
    @h.add("inst.verify.no_manifest", "installer")
    def _(sb=None):
        t = Target(sb)
        rc, out, err = t.verify("NoSuchMod")
        A.eq(rc, 2, f"verify of absent mod rc={rc}")
        A.contains(err, "no manifest", "unhelpful error for absent mod")
