#!/usr/bin/env python3
"""Host tests for paktx.py (capability #7 step 1).  Mac-only.

Synthetic game install modelled on the real Townfall layout (Mac-only facts
from the 2026-10-01 captures): IoStore v8 containers, 64 KiB stored blocks,
v5 ContainerHeader chunk (type 6) tied to the TOC by its BLAKE3-20 IoHash,
a big primary container + small secondary ones, a v11 classic .pak holding
loose config files, an unadmitted container (other TOC version), and a mod
trio (+ zero-entry companion .pak) whose packages are spread across TWO
base containers plus new packages.  The fixture TOC/UCAS writer below is
written independently of overlay_builder (only the ContainerHeader bytes
use the builder's serializer).  Nothing here touches a console.
"""
import hashlib
import importlib.util
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
TEAM = os.path.dirname(HERE)
REPO = os.environ.get("PAKTX_REPO", "/WORKSPACE/.codex/worktrees/translation-core/what")
BUILDER = os.path.join(REPO, "outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py")
REPAK = os.path.join(REPO, ".superpowers/native-dependency-spike/unreal-library-container-survey-1/"
                     "mount-research/tools/build/aarch64-apple-darwin/release/repak")
TOOL_PATH = os.path.join(TEAM, "PATCH", "tools", "paktx.py")
WORK = os.path.join(HERE, "work")
PAKS_REMOTE = "/data/game/paks"

_spec = importlib.util.spec_from_file_location("paktx", TOOL_PATH)
paktx = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(paktx)
B = paktx.load_builder(BUILDER)
sys.path.insert(0, HERE)
from fake_ftp import FakeFTP  # noqa: E402

BS = 65536


def prng(seed, n):
    out = bytearray()
    c = 0
    while len(out) < n:
        out += hashlib.sha256(b"%d:%d" % (seed, c)).digest()
        c += 1
    return bytes(out[:n])


def cid(pid, kind=1, index=0):
    return struct.pack("<Q", pid) + struct.pack(">H", index) + b"\x00" + bytes([kind])


def fstr(s):
    raw = s.encode()
    return struct.pack("<i", len(raw) + 1) + raw + b"\0"


def dir_index(files):
    """files: [(relpath under mount, toc_row)] -> canonical v8 directory index."""
    ents = []
    for rel, row in files:
        parts = rel.split("/")
        ents.append((tuple(parts[:-1]), parts[-1], row))
    dirs = {()}
    for d, _, _ in ents:
        for i in range(1, len(d) + 1):
            dirs.add(d[:i])
    od = sorted(dirs, key=lambda p: (len(p), tuple(x.encode() for x in p)))
    di = {p: i for i, p in enumerate(od)}
    of = sorted(ents, key=lambda e: (e[0], e[1].encode()))
    strings = sorted({x for p in od for x in p} | {e[1] for e in of}, key=lambda s: s.encode())
    si = {s: i for i, s in enumerate(strings)}
    kids = {}
    for p in od:
        if p:
            kids.setdefault(p[:-1], []).append(p)
    out = bytearray(fstr("../../../"))
    out += struct.pack("<i", len(od))
    INV = 0xFFFFFFFF
    for p in od:
        k = kids.get(p, [])
        firsts = [i for i, e in enumerate(of) if e[0] == p]
        sib = INV
        if p:
            s = kids[p[:-1]]
            j = s.index(p)
            sib = di[s[j + 1]] if j + 1 < len(s) else INV
        out += struct.pack("<IIII", si[p[-1]] if p else INV, di[k[0]] if k else INV, sib,
                           firsts[0] if firsts else INV)
    out += struct.pack("<i", len(of))
    for i, (d, n, row) in enumerate(of):
        nxt = i + 1 if i + 1 < len(of) and of[i + 1][0] == d else INV
        out += struct.pack("<III", si[n], nxt, row)
    out += struct.pack("<i", len(strings))
    for s in strings:
        out += fstr(s)
    return bytes(out)


def container(container_id, chunks, files, version=8):
    """chunks: [(chunk_id, payload)]; files: [(relpath, chunk_id)] -> (utoc, ucas)."""
    ids, offs, metas, blocks, ucas = [], bytearray(), bytearray(), bytearray(), bytearray()
    row_of = {}
    for row, (c, payload) in enumerate(chunks):
        row_of[c] = row
        first = len(blocks) // 12
        for i in range(0, len(payload), BS):
            part = payload[i:i + BS]
            blocks += len(ucas).to_bytes(5, "little") + len(part).to_bytes(3, "little") + \
                len(part).to_bytes(3, "little") + b"\x00"
            ucas += part + bytes((-len(part)) % 16)
        ids.append(c)
        offs += (first * BS).to_bytes(5, "big") + len(payload).to_bytes(5, "big")
        metas += B.blake3_20(payload) + b"\x00" * 4
    d = dir_index([(p, row_of[c]) for p, c in files])
    h = bytearray(144)
    h[0:16] = b"-==--==--==--==-"
    h[16] = version
    for off, val in ((20, 144), (24, len(ids)), (28, len(blocks) // 12), (32, 12), (36, 0),
                     (40, 32), (44, BS), (48, len(d)), (52, 1), (84, 0), (96, 0)):
        struct.pack_into("<I", h, off, val)
    h[56:64] = container_id
    h[80] = 8
    struct.pack_into("<Q", h, 88, (1 << 64) - 1)
    return bytes(h) + b"".join(ids) + bytes(offs) + bytes(blocks) + d + bytes(metas), bytes(ucas)


def header_bytes(container_id, pkgs):
    return B.serialize_container_header(B.ContainerHeader(
        struct.unpack("<Q", container_id)[0],
        {p: B.StoreEntry(tuple(imp), ()) for p, imp in pkgs.items()}, False, b""))


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def repak(*args):
    subprocess.run([REPAK, *args], check=True, capture_output=True,
                   env=dict(os.environ, RAYON_NUM_THREADS="1"))


def make_pak(tree_files, out, seed=636187772):
    tmp = out + ".tree"
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    for rel, data in tree_files.items():
        write(os.path.join(tmp, *rel.split("/")), data)
    repak("pack", tmp, out, "--version", "V11", "--mount-point", "../../../",
          "--path-hash-seed", str(seed), "-q")
    shutil.rmtree(tmp)


class Game:
    """Builds console/ (live install), mirror/ (+sizes, headers) and mod/."""

    def __init__(self, name, *, with_b_header=True, mod_kind="iostore", extra_mod=None,
                 remote=PAKS_REMOTE):
        self.root = os.path.join(WORK, name)
        self.remote = remote
        shutil.rmtree(self.root, ignore_errors=True)
        self.paks = os.path.join(self.root, "console", remote.lstrip("/"))
        self.mirror = os.path.join(self.root, "mirror")
        self.headers = os.path.join(self.root, "headers")
        self.mod = os.path.join(self.root, "mod")
        self.out = os.path.join(self.root, "work")
        os.makedirs(self.paks)
        self.cids = {}
        # base A: big primary (40 packages, some bulk)
        self._base("pakchunk0-ps5", 0xA0A0A0A000000001, range(0x1000, 0x1028), bulk=range(0x1000, 0x1010),
                   size=90000, header=True)
        # base B: small secondary (3 packages) -- the "DLC" shape
        self._base("pakchunk10-ps5", 0xB0B0B0B000000002, range(0x2000, 0x2003), bulk=(0x2000,),
                   size=70000, header=with_b_header)
        # base C: unrelated
        self._base("pakchunk1-ps5", 0xC0C0C0C000000003, range(0x4000, 0x4004), bulk=(), size=5000,
                   header=True)
        # an unadmitted container (TOC version 3) like Stellar Blade's global
        cg = struct.pack("<Q", 0xD0D0D0D000000004)
        u, c = container(cg, [(cid(0x5000), prng(5, 1000))], [("Engine/G.uasset", cid(0x5000))], version=3)
        write(os.path.join(self.paks, "global.utoc"), u)
        write(os.path.join(self.paks, "global.ucas"), c)
        # classic config pak (pakchunk0's companion carries loose config files)
        make_pak({"Game/Config/DefaultEngine.ini": b"[SystemSettings]\nr.Fog=1\n",
                  "Game/Config/DefaultGame.ini": b"[Game]\nx=1\n",
                  "Engine/Config/BaseEngine.ini": b"[Core]\ny=2\n"},
                 os.path.join(self.paks, "pakchunk0-ps5.pak"))
        for stem in ("pakchunk10-ps5", "pakchunk1-ps5"):
            make_pak({}, os.path.join(self.paks, stem + ".pak"))
        # mirror = every non-.ucas file; sizes.json for .ucas (the real capture shape)
        os.makedirs(self.mirror)
        self.sizes = {}
        for n in os.listdir(self.paks):
            p = os.path.join(self.paks, n)
            if n.endswith(".ucas"):
                self.sizes[n] = os.path.getsize(p)
            else:
                shutil.copyfile(p, os.path.join(self.mirror, n))
        with open(os.path.join(self.root, "sizes.json"), "w") as f:
            json.dump(self.sizes, f)
        if mod_kind == "iostore":
            self._mod()
        elif mod_kind == "configpak":
            self._config_mod()
        if extra_mod:
            extra_mod(self)

    def _base(self, stem, cid_value, pids, bulk, size, header):
        c_id = struct.pack("<Q", cid_value)
        pkgs = {p: [] for p in pids}
        hb = header_bytes(c_id, pkgs)
        chunks = [(c_id + bytes.fromhex("00000006"), hb)]
        files = []
        for p in pids:
            chunks.append((cid(p), prng(p, size + (p % 7) * 1000)))
            files.append(("Game/Content/%s/P%x.uasset" % (stem, p), cid(p)))
            if p in bulk:
                chunks.append((cid(p, 2), prng(p + 77, size // 2)))
                files.append(("Game/Content/%s/P%x.ubulk" % (stem, p), cid(p, 2)))
        u, c = container(c_id, chunks, files)
        write(os.path.join(self.paks, stem + ".utoc"), u)
        write(os.path.join(self.paks, stem + ".ucas"), c)
        if header:
            write(os.path.join(self.headers, stem + ".container-header.bin"), hb)
        self.cids[stem] = c_id

    def _mod(self, stem="zzz_Outfit_P", replace_a=(0x1001, 0x1003, 0x1005), replace_b=(0x2001,),
             new=(0x3000, 0x3001), extra_chunks=()):
        c_id = struct.pack("<Q", 0xE0E0E0E000000005)
        pkgs = {p: [0x3000] for p in replace_a + replace_b}
        pkgs.update({p: [] for p in new})
        chunks = [(c_id + bytes.fromhex("00000006"), header_bytes(c_id, pkgs))]
        files = []
        for p in replace_a:
            chunks.append((cid(p), prng(p + 1000, 30000)))
            files.append(("Game/Content/pakchunk0-ps5/P%x.uasset" % p, cid(p)))
        if 0x1003 in replace_a:
            chunks.append((cid(0x1003, 2), prng(4242, 80000)))  # spans 2 blocks
            files.append(("Game/Content/pakchunk0-ps5/P%x.ubulk" % 0x1003, cid(0x1003, 2)))
        for p in replace_b:
            chunks.append((cid(p), prng(p + 2000, 20000)))
            files.append(("Game/Content/pakchunk10-ps5/P%x.uasset" % p, cid(p)))
        for p in new:
            chunks.append((cid(p), prng(p + 3000, 15000)))
            files.append(("Game/Content/Mod/N%x.uasset" % p, cid(p)))
        chunks.extend(extra_chunks)
        u, c = container(c_id, chunks, files)
        write(os.path.join(self.mod, stem + ".utoc"), u)
        write(os.path.join(self.mod, stem + ".ucas"), c)
        make_pak({}, os.path.join(self.mod, stem + ".pak"))
        write(os.path.join(self.mod, "README.txt"), b"readme\n")
        self.mod_chunks = {c_: p_ for c_, p_ in chunks}
        self.mod_header_id = c_id + bytes.fromhex("00000006")

    def _config_mod(self):
        make_pak({"Game/Config/DefaultEngine.ini": b"[SystemSettings]\nr.Fog=0\n",
                  "Game/Config/NewMod.ini": b"[Mod]\nz=3\n"},
                 os.path.join(self.mod, "ConfigMod_P.pak"))

    def record(self, headers=True):
        rec = paktx.make_record(B, self.mirror, "TEST00001", "01.000.000", self.remote,
                                self.sizes, self.headers if headers else None)
        path = os.path.join(self.root, "record.json")
        paktx.write_json(path, rec)
        return path, rec


def snapshot(top):
    out = {}
    for r, ds, fs in os.walk(top):
        for f in fs:
            p = os.path.join(r, f)
            out[os.path.relpath(p, top)] = paktx.sha256_file(p)
    return out


def run_cli(*argv):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        code = paktx.main(["--builder", BUILDER, *argv])
    return code, json.loads(out.getvalue()), err.getvalue()


def plan_for(g, rec_path, *extra):
    return run_cli("install", "--record", rec_path, "--mirror", g.mirror, "--mod", g.mod,
                   "--work", g.out, "--repak", REPAK, *extra)


class T01Record(unittest.TestCase):
    def test_record_counts_and_identity(self):
        g = Game("t01")
        _, rec = g.record()
        self.assertEqual(len(rec["files"]), 11)
        self.assertIsNone(rec["files"]["pakchunk0-ps5.ucas"]["sha256"])
        self.assertTrue(rec["containers"]["pakchunk0-ps5"]["header"]["iohash_matches_toc"])
        self.assertEqual(rec["containers"]["pakchunk0-ps5"]["export_package_count"], 40)
        self.assertFalse(rec["containers"]["global"]["admitted"])
        self.assertEqual(rec["containers"]["global"]["refusal"], "unsupported_toc_version")
        loaded = paktx.load_record(os.path.join(g.root, "record.json"))
        self.assertEqual(loaded["record_sha256"], rec["record_sha256"])

    def test_cli_record_relative_header_then_plan(self):
        g = Game("t01d")
        out = os.path.join(g.root, "rec", "record.json")
        os.makedirs(os.path.dirname(out))
        hdir = os.path.join(g.root, "rec", "headers")
        shutil.copytree(g.headers, hdir)
        with open(os.path.join(g.root, "rec", "sizes.json"), "w") as f:
            json.dump(g.sizes, f)
        code, res, _ = run_cli("record", "--mirror", g.mirror, "--sizes", os.path.join(g.root, "rec", "sizes.json"),
                               "--headers", hdir, "--title-id", "TEST00001", "--version", "01.000.000",
                               "--paks-dir", PAKS_REMOTE, "--out", out)
        self.assertEqual((code, res["files"]), (0, 11), res)
        rec = json.load(open(out))
        self.assertEqual(rec["containers"]["pakchunk10-ps5"]["header"]["source"],
                         "headers/pakchunk10-ps5.container-header.bin")
        code, res, _ = plan_for(g, out, "--dry-run")
        self.assertEqual(code, 0, res)

    def test_record_rejects_wrong_header(self):
        g = Game("t01b")
        write(os.path.join(g.headers, "pakchunk1-ps5.container-header.bin"),
              open(os.path.join(g.headers, "pakchunk0-ps5.container-header.bin"), "rb").read())
        with self.assertRaises(paktx.Refused) as cm:
            g.record()
        self.assertEqual(cm.exception.code, "header_length_disagrees_with_toc")

    def test_tampered_record_refused(self):
        g = Game("t01c")
        path, rec = g.record()
        rec["files"]["pakchunk1-ps5.utoc"]["sha256"] = "0" * 64
        paktx.write_json(path, rec)
        with self.assertRaises(paktx.Refused) as cm:
            paktx.load_record(path)
        self.assertEqual(cm.exception.code, "record_self_hash_mismatch")


class T02MultiContainerPlan(unittest.TestCase):
    def test_split_across_two_owners(self):
        g = Game("t02")
        rp, rec = g.record()
        code, res, err = plan_for(g, rp, "--dry-run")
        self.assertEqual(code, 0, res)
        plan = json.load(open(res["plan"]))
        self.assertEqual([t["container"] for t in plan["targets"]], ["pakchunk0-ps5", "pakchunk10-ps5"])
        self.assertEqual(plan["ownership"]["primary"], "pakchunk0-ps5")
        pa = plan["targets"][0]["packages"]["zzz_Outfit_P"]
        pb = plan["targets"][1]["packages"]["zzz_Outfit_P"]
        self.assertEqual(pa["replaced"], ["%016x" % p for p in (0x1001, 0x1003, 0x1005)])
        self.assertEqual(pa["added"], ["%016x" % p for p in (0x3000, 0x3001)])
        self.assertEqual(pb["replaced"], ["%016x" % 0x2001])
        self.assertEqual(pb["added"], [])
        self.assertEqual([i["reason"] for i in plan["mod"]["ignored"]],
                         ["not_a_container", "zero_entry_companion_pak"])
        for t in plan["targets"]:
            stem = t["container"]
            base_utoc = B.parse_toc(open(os.path.join(g.paks, stem + ".utoc"), "rb").read())
            der = B.parse_toc(open(t["derived"]["local"], "rb").read(), strict_admission=False)
            B.validate_lookup(der)
            store = open(os.path.join(g.paks, stem + ".ucas"), "rb").read() + \
                open(t["sidecar"]["local"], "rb").read()
            self.assertEqual(der.partition_count, 2)
            self.assertEqual(der.partition_size, len(open(os.path.join(g.paks, stem + ".ucas"), "rb").read()))
            mine = {c for c in g.mod_chunks if c != g.mod_header_id and
                    ("%016x" % B.package_id_of_chunk(c)) in
                    (t["packages"]["zzz_Outfit_P"]["replaced"] + t["packages"]["zzz_Outfit_P"]["added"])}
            others = {c for c in g.mod_chunks if c != g.mod_header_id} - mine
            for c in mine:
                self.assertEqual(B.extract_chunk_bytes(der, der.ids.index(c), store), g.mod_chunks[c])
            for c in others:
                self.assertNotIn(c, der.ids, "chunk leaked into a non-owner container")
            hid = base_utoc.container_id + bytes.fromhex("00000006")
            for row, c in enumerate(base_utoc.ids):
                if c in mine or c == hid:
                    continue
                self.assertEqual(der.offset_length(der.ids.index(c)), base_utoc.offset_length(row))
                self.assertEqual(der.meta(der.ids.index(c)), base_utoc.meta(row))
            merged = B.parse_container_header(B.extract_chunk_bytes(der, der.ids.index(hid), store))
            base_h = B.parse_container_header(open(os.path.join(g.headers, stem + ".container-header.bin"),
                                                   "rb").read())
            expect = set(base_h.packages) | {int(x, 16) for x in t["packages"]["zzz_Outfit_P"]["added"]}
            self.assertEqual(set(merged.packages), expect)
        # every planned change is printed
        for op in plan["operations"]:
            self.assertIn(paktx.describe(op), err)
        self.assertIn("untouched: pakchunk0-ps5.ucas", err)

    def test_plan_is_deterministic_and_dry_run_writes_nothing(self):
        g = Game("t02b")
        rp, _ = g.record()
        before = snapshot(os.path.join(g.root, "console"))
        c1, r1, _ = plan_for(g, rp, "--dry-run")
        c2, r2, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((c1, c2), (0, 0))
        self.assertEqual(r1["plan_sha256"], r2["plan_sha256"])
        self.assertEqual(snapshot(os.path.join(g.root, "console")), before)

    def test_single_owner_mod_is_unprojected(self):
        def only_a(g):
            shutil.rmtree(g.mod)
            g._mod(replace_b=())
        g = Game("t02c", extra_mod=only_a)
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual(code, 0, res)
        self.assertEqual(res["targets"], ["pakchunk0-ps5"])
        plan = json.load(open(res["plan"]))
        # whole mod segment carried verbatim: sidecar starts with the mod's ucas bytes
        mod_ucas = open(os.path.join(g.mod, "zzz_Outfit_P.ucas"), "rb").read()
        side = open(plan["targets"][0]["sidecar"]["local"], "rb").read()
        self.assertEqual(side[:len(mod_ucas)], mod_ucas)


class T03InstallRestoreExact(unittest.TestCase):
    def cycle(self, g, rp, cleanup):
        console = os.path.join(g.root, "console")
        before = snapshot(console)
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual(code, 0, res)
        sha = res["plan_sha256"]
        code, res, err = plan_for(g, rp, "--local", g.paks, "--confirm", sha[:12], "--game-closed")
        self.assertEqual(code, 0, (res, err))
        code, v, _ = run_cli("verify", "--plan", res["plan"], "--local", g.paks)
        self.assertEqual(v["state"]["verdict"], "INSTALLED")
        live = snapshot(console)
        self.assertNotEqual(live, before)
        # the base .ucas files were never touched
        for n in before:
            if n.endswith(".ucas"):
                self.assertEqual(live[n], before[n])
        extra = ["--cleanup", "--allow-delete"] if cleanup else []
        code, r2, err2 = run_cli("restore", "--plan", res["plan"], "--local", g.paks,
                                 "--confirm", sha[:12], "--game-closed", *extra)
        self.assertEqual(code, 0, (r2, err2))
        after = snapshot(console)
        for n, d in before.items():
            self.assertEqual(after.get(n), d, n)
        return before, after, res["plan"]

    def test_restore_with_cleanup_is_byte_exact(self):
        g = Game("t03")
        rp, _ = g.record()
        before, after, _ = self.cycle(g, rp, cleanup=True)
        self.assertEqual(after, before)

    def test_restore_without_cleanup_leaves_only_inert_files(self):
        g = Game("t03b")
        rp, _ = g.record()
        before, after, plan = self.cycle(g, rp, cleanup=False)
        leftovers = sorted(set(after) - set(before))
        self.assertTrue(leftovers)
        for n in leftovers:
            self.assertTrue(n.endswith("_s1.ucas") or n.endswith(".retired"), n)
        code, v, _ = run_cli("verify", "--plan", plan, "--local", g.paks)
        self.assertEqual(v["state"]["verdict"], "PRISTINE_WITH_INERT_SIDECAR")
        # and a later cleanup removes exactly those
        code, r, _ = run_cli("restore", "--plan", plan, "--local", g.paks, "--confirm",
                             json.load(open(plan))["plan_sha256"][:12], "--game-closed",
                             "--cleanup", "--allow-delete")
        self.assertEqual(code, 0, r)
        self.assertEqual(snapshot(os.path.join(g.root, "console")), before)

    def test_restore_is_idempotent(self):
        g = Game("t03c")
        rp, _ = g.record()
        before, _, plan = self.cycle(g, rp, cleanup=True)
        sha = json.load(open(plan))["plan_sha256"][:12]
        code, r, _ = run_cli("restore", "--plan", plan, "--local", g.paks, "--confirm", sha,
                             "--game-closed", "--cleanup", "--allow-delete")
        self.assertEqual(code, 0, r)
        self.assertEqual(snapshot(os.path.join(g.root, "console")), before)

    def test_cycle_command(self):
        g = Game("t03d")
        rp, _ = g.record()
        before = snapshot(os.path.join(g.root, "console"))
        code, r, err = run_cli("cycle", "--record", rp, "--mirror", g.mirror, "--mod", g.mod,
                               "--work", g.out, "--local", g.paks)
        self.assertEqual(code, 0, (r, err))
        self.assertEqual(r["after_install"]["verdict"], "INSTALLED")
        self.assertEqual(r["after_restore"]["verdict"], "PRISTINE")
        self.assertEqual(snapshot(os.path.join(g.root, "console")), before)


class T04Interruption(unittest.TestCase):
    def test_every_interruption_point_restores_exactly(self):
        g = Game("t04")
        rp, rec = g.record()
        console = os.path.join(g.root, "console")
        pristine = os.path.join(g.root, "pristine")
        shutil.copytree(console, pristine)
        before = snapshot(console)
        code, res, _ = plan_for(g, rp, "--dry-run")
        plan = json.load(open(res["plan"]))
        mutating = sum(1 for op in plan["operations"] if op["op"] in ("upload", "rename"))
        self.assertEqual(mutating, 2 * 2 + 3 * 2)
        points = 0
        for k in range(mutating):
            shutil.rmtree(console)
            shutil.copytree(pristine, console)
            jpath = os.path.join(g.root, "j%d.json" % k)
            be = paktx.LocalBackend(g.paks, fail_after=k)
            with self.assertRaises(paktx.Stopped):
                paktx.execute(plan, plan["operations"], be, paktx.Journal(jpath, plan, "install"),
                              lambda s: None)
            j = json.load(open(jpath))
            self.assertEqual(j["events"][-1]["state"], "unknown_may_have_completed")
            be2 = paktx.LocalBackend(g.paks)
            ops, _ = paktx.restore_operations(plan, be2, cleanup=True)
            paktx.execute(plan, ops, be2, paktx.Journal(jpath, plan, "restore"), lambda s: None)
            # temps from an interrupted upload may be partial: they are preserved, never deleted
            after = snapshot(console)
            for n, d in before.items():
                self.assertEqual(after.get(n), d, "k=%d %s" % (k, n))
            for n in set(after) - set(before):
                self.assertTrue(".paktx-" in n and n.endswith(".tmp"), "k=%d leftover %s" % (k, n))
            points += 1
        self.assertEqual(points, mutating)
        print("\n[interruption] %d interruption points restored byte-exact" % points)


class T05Gate(unittest.TestCase):
    def _refuse(self, g, rp, code_expected):
        console = os.path.join(g.root, "console")
        before = snapshot(console)
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual(code, 0, res)
        code, res, _ = plan_for(g, rp, "--local", g.paks, "--confirm", res["plan_sha256"][:12],
                                "--game-closed")
        self.assertEqual(code, 2, res)
        self.assertEqual(res["code"], code_expected)
        self.assertEqual(snapshot(console), before, "a refused gate changed something")

    def test_changed_index_byte(self):
        g = Game("t05a")
        rp, _ = g.record()
        p = os.path.join(g.paks, "pakchunk1-ps5.utoc")
        d = bytearray(open(p, "rb").read())
        d[200] ^= 1
        write(p, bytes(d))
        self._refuse(g, rp, "recorded_file_hash_mismatch")

    def test_changed_config_pak(self):
        g = Game("t05b")
        rp, _ = g.record()
        p = os.path.join(g.paks, "pakchunk0-ps5.pak")
        d = bytearray(open(p, "rb").read())
        d[10] ^= 1
        write(p, bytes(d))
        self._refuse(g, rp, "recorded_file_hash_mismatch")

    def test_changed_ucas_size(self):
        g = Game("t05c")
        rp, _ = g.record()
        with open(os.path.join(g.paks, "pakchunk10-ps5.ucas"), "ab") as f:
            f.write(b"\0" * 16)
        self._refuse(g, rp, "recorded_file_size_mismatch")

    def test_unrecorded_container_present(self):
        g = Game("t05d")
        rp, _ = g.record()
        for ext in (".utoc", ".ucas", ".pak"):
            shutil.copyfile(os.path.join(g.mod, "zzz_Outfit_P" + ext),
                            os.path.join(g.paks, "zzz_Outfit_P" + ext))
        self._refuse(g, rp, "unrecorded_container_files_present")

    def test_reserved_name_taken(self):
        g = Game("t05e")
        rp, _ = g.record()
        write(os.path.join(g.paks, "pakchunk10-ps5_s1.ucas.paktx-x.tmp"), b"")  # inert, allowed
        code, res, _ = plan_for(g, rp, "--dry-run")
        plan = json.load(open(res["plan"]))
        write(os.path.join(g.paks, plan["targets"][0]["backup"]), b"x")
        self._refuse(g, rp, "reserved_name_exists")

    def test_mirror_differs_from_record(self):
        g = Game("t05f")
        rp, _ = g.record()
        write(os.path.join(g.mirror, "pakchunk1-ps5.utoc"), b"different")
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "mirror_differs_from_record"))

    def test_confirm_and_attestation_required(self):
        g = Game("t05g")
        rp, _ = g.record()
        before = snapshot(os.path.join(g.root, "console"))
        code, res, _ = plan_for(g, rp, "--local", g.paks, "--confirm", "deadbeefdead", "--game-closed")
        self.assertEqual((code, res["code"]), (2, "confirm_must_match_plan_sha"))
        code, res0, _ = plan_for(g, rp, "--dry-run")
        code, res, _ = plan_for(g, rp, "--local", g.paks, "--confirm", res0["plan_sha256"][:12])
        self.assertEqual((code, res["code"]), (2, "game_closed_attestation_required"))
        self.assertEqual(snapshot(os.path.join(g.root, "console")), before)


class T06Refusals(unittest.TestCase):
    def test_secondary_header_missing(self):
        g = Game("t06a", with_b_header=False)
        rp, rec = g.record()
        self.assertIsNone(rec["containers"]["pakchunk10-ps5"]["header"])
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "base_container_header_unavailable"))

    def test_shader_chunk_in_mod(self):
        def bad(g):
            shutil.rmtree(g.mod)
            g._mod(extra_chunks=[(bytes(range(11)) + b"\x09", b"x" * 100)])
        g = Game("t06b", extra_mod=bad)
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "mod_chunk_type_unsupported"))

    def test_package_owned_twice(self):
        def dup(g):
            # base C also exports 0x1001 -> ambiguous owner
            g._base("pakchunk1-ps5", 0xC0C0C0C000000003, (0x4000, 0x1001), bulk=(), size=5000, header=True)
            for n in ("pakchunk1-ps5.utoc",):
                shutil.copyfile(os.path.join(g.paks, n), os.path.join(g.mirror, n))
            g.sizes["pakchunk1-ps5.ucas"] = os.path.getsize(os.path.join(g.paks, "pakchunk1-ps5.ucas"))
        g = Game("t06c", extra_mod=dup)
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "package_owned_by_multiple_base_containers"))

    def test_new_only_mod_needs_explicit_target(self):
        def newonly(g):
            shutil.rmtree(g.mod)
            g._mod(replace_a=(), replace_b=(), new=(0x3000,))
        g = Game("t06d", extra_mod=newonly)
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "mod_replaces_no_base_package_no_primary"))
        code, res, _ = plan_for(g, rp, "--dry-run", "--new-packages-into", "pakchunk1-ps5")
        self.assertEqual(code, 0, res)
        self.assertEqual(res["targets"], ["pakchunk1-ps5"])

    def test_cooked_assets_in_classic_pak(self):
        def cooked(g):
            make_pak({"Game/Content/X.uasset": b"u", "Game/Content/X.uexp": b"e"},
                     os.path.join(g.mod, "Cooked_P.pak"))
        g = Game("t06e", mod_kind="none", extra_mod=cooked)
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "cooked_classic_pak_assets_need_conversion"))

    def test_empty_mod_folder(self):
        def empty(g):
            os.makedirs(g.mod, exist_ok=True)
        g = Game("t06f", mod_kind="none", extra_mod=empty)
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "mod_scanned_zero_files"))


class T07ClassicPak(unittest.TestCase):
    def test_config_pak_rebuild_and_exact_restore(self):
        g = Game("t07", mod_kind="configpak")
        rp, _ = g.record()
        console = os.path.join(g.root, "console")
        before = snapshot(console)
        code, res, err = plan_for(g, rp, "--dry-run")
        self.assertEqual(code, 0, (res, err))
        plan = json.load(open(res["plan"]))
        self.assertEqual(res["targets"], ["pakchunk0-ps5.pak"])
        t = plan["targets"][0]
        self.assertEqual(t["pak_rebuild"]["changed_entries"],
                         ["Game/Config/DefaultEngine.ini", "Game/Config/NewMod.ini"])
        self.assertEqual(t["pak_rebuild"]["unchanged_entries_verified"], 2)
        code, res2, err2 = plan_for(g, rp, "--local", g.paks, "--confirm", res["plan_sha256"][:12],
                                    "--game-closed")
        self.assertEqual(code, 0, (res2, err2))
        live = B.parse_pak(open(os.path.join(g.paks, "pakchunk0-ps5.pak"), "rb").read())
        e = live["entries"]["Game/Config/DefaultEngine.ini"]
        data = open(os.path.join(g.paks, "pakchunk0-ps5.pak"), "rb").read()
        self.assertEqual(data[e.offset:e.offset + e.size], b"[SystemSettings]\nr.Fog=0\n")
        code, r3, _ = run_cli("restore", "--plan", res["plan"], "--local", g.paks, "--confirm",
                              res["plan_sha256"][:12], "--game-closed", "--cleanup", "--allow-delete")
        self.assertEqual(code, 0, r3)
        self.assertEqual(snapshot(console), before)


class T08Ftp(unittest.TestCase):
    def test_install_verify_restore_over_ftp(self):
        g = Game("t08")
        rp, _ = g.record()
        console = os.path.join(g.root, "console")
        before = snapshot(console)
        srv = FakeFTP(console)
        try:
            addr = "127.0.0.1:%d" % srv.port
            code, res, _ = plan_for(g, rp, "--dry-run")
            sha = res["plan_sha256"][:12]
            code, res, err = plan_for(g, rp, "--ftp", addr, "--confirm", sha, "--game-closed")
            self.assertEqual(code, 0, (res, err))
            code, v, _ = run_cli("verify", "--plan", res["plan"], "--ftp", addr)
            self.assertEqual(v["state"]["verdict"], "INSTALLED")
            code, r, err = run_cli("restore", "--plan", res["plan"], "--ftp", addr, "--confirm", sha,
                                   "--game-closed", "--cleanup", "--allow-delete")
            self.assertEqual(code, 0, (r, err))
            self.assertEqual(snapshot(console), before)
            touched = [a for v_, a in srv.commands if v_ in ("RETR", "STOR", "RNFR", "RNTO", "DELE")]
            self.assertTrue(touched)
            for a in touched:
                self.assertFalse(a.endswith(".ucas") and "_s1" not in a, "base ucas touched: " + a)
            self.assertIn(("DELE", PAKS_REMOTE + "/pakchunk0-ps5_s1.ucas"), srv.commands)
        finally:
            srv.stop()

    def test_protected_ucas_refused(self):
        g = Game("t08b")
        srv = FakeFTP(os.path.join(g.root, "console"))
        try:
            be = paktx.FtpBackend("127.0.0.1", srv.port, PAKS_REMOTE, ["pakchunk0-ps5.ucas"])
            with self.assertRaises(paktx.Refused) as cm:
                be.sha256("pakchunk0-ps5.ucas", 1)
            self.assertEqual(cm.exception.code, "protected_base_ucas")
            with self.assertRaises(paktx.Refused):
                be.rename("pakchunk0-ps5.ucas", "x")
            be.close()
        finally:
            srv.stop()
        with self.assertRaises(paktx.Refused):
            paktx.FtpBackend("8.8.8.8", 21, PAKS_REMOTE, [])


class T10Probe(unittest.TestCase):
    def test_probe_lists_only_mod_only_objects(self):
        g = Game("t10")
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        out = os.path.join(g.root, "probe")
        code, r, _ = run_cli("probe", "--plan", res["plan"], "--mod", g.mod, "--out", out)
        self.assertEqual(code, 0, r)
        lua = open(os.path.join(out, "main.lua")).read()
        self.assertIn('"/Game/Mod/N3000.N3000","/Game/Mod/N3001.N3001"', lua)
        self.assertNotIn("pakchunk0-ps5/P", lua)  # replaced packages exist in the base too
        self.assertLess(len(lua.encode()), 4096)
        self.assertIn(res["plan_sha256"][:12], lua)

    def test_probe_runs_under_stub_runtime(self):
        g = Game("t10b")
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        out = os.path.join(g.root, "probe")
        run_cli("probe", "--plan", res["plan"], "--mod", g.mod, "--out", out)
        stub = os.path.join(out, "stub.lua")
        with open(stub, "w") as f:
            f.write('''
local loaded = {["/Game/Mod/N3001.N3001"]=true}
function StaticFindObject(p) if loaded[p] then return {IsValid=function() return true end} end return nil end
local cb
function LoopAsync(ms, f) cb = f end
dofile(arg[1])
for i=1,3 do if cb() then break end end
''')
        proc = subprocess.run(["lua", stub, os.path.join(out, "main.lua")], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("[pk] FOUND /Game/Mod/N3001.N3001", proc.stdout)
        self.assertIn("[pk] T1 found=1/2", proc.stdout)
        self.assertIn("[pk] T3 found=1/2", proc.stdout)

    def test_object_path_rules(self):
        self.assertEqual(paktx.object_path("../../../Townfall/Content/Leon/MI_Leon_Hair.uasset"),
                         "/Game/Leon/MI_Leon_Hair.MI_Leon_Hair")
        self.assertEqual(paktx.object_path("../../../Engine/Content/A/B.uasset"), "/Engine/A/B.B")
        self.assertIsNone(paktx.object_path("../../../Townfall/Plugins/X/Content/A.uasset"))
        self.assertIsNone(paktx.object_path("../../../Townfall/Content/Leon/T_Leon_N.ubulk"))


class T09Repair(unittest.TestCase):
    def test_repair_old_experiments_back_to_record(self):
        """Models the console state seen in the 2026-10-08 storage scan: a derived
        index in place with its .original-*.bak, an old sidecar, an unchanged mod
        trio added as new files, and a rebuilt config .pak with its .bak."""
        g = Game("t09")
        rp, rec = g.record()
        console = os.path.join(g.root, "console")
        pristine_files = {n: paktx.sha256_file(os.path.join(g.paks, n)) for n in os.listdir(g.paks)}
        # old experiment 1: index swap via our backup naming
        os.rename(os.path.join(g.paks, "pakchunk0-ps5.utoc"),
                  os.path.join(g.paks, "pakchunk0-ps5.utoc.original-603c4879-aaf2-4098-aeda-2ee5d3f0d0f0.bak"))
        write(os.path.join(g.paks, "pakchunk0-ps5.utoc"), b"old derived index")
        write(os.path.join(g.paks, "pakchunk0-ps5_s1.ucas"), b"old sidecar")
        # old experiment 2: rebuilt config pak
        os.rename(os.path.join(g.paks, "pakchunk0-ps5.pak"),
                  os.path.join(g.paks, "pakchunk0-ps5.pak.original-9a914e38-5a1e-48a7-ba39-0f1f15f3230d.bak"))
        write(os.path.join(g.paks, "pakchunk0-ps5.pak"), b"rebuilt config pak")
        # old experiment 3: the unchanged mod trio as new files
        for ext in (".utoc", ".ucas", ".pak"):
            shutil.copyfile(os.path.join(g.mod, "zzz_Outfit_P" + ext), os.path.join(g.paks, "zzz_Outfit_P" + ext))
        n_before = sum(len(fs) for _, _, fs in os.walk(console))
        code, res, err = run_cli("repair", "--record", rp, "--work", g.out, "--local", g.paks, "--dry-run")
        self.assertEqual(code, 0, (res, err))
        self.assertEqual((res["fixes"], res["quarantined"]), (2, 4))
        code, res2, err2 = run_cli("repair", "--record", rp, "--work", g.out, "--local", g.paks,
                                   "--confirm", res["plan_sha256"][:12], "--game-closed")
        self.assertEqual(code, 0, (res2, err2))
        live = {n: paktx.sha256_file(os.path.join(g.paks, n)) for n in os.listdir(g.paks)
                if os.path.isfile(os.path.join(g.paks, n))}
        for n, d in pristine_files.items():
            self.assertEqual(live[n], d, n)
        self.assertEqual(sum(len(fs) for _, _, fs in os.walk(console)), n_before, "repair deleted a file")
        q = [d for d in os.listdir(os.path.dirname(g.paks)) if d.startswith("paktx-quarantine-")]
        self.assertEqual(len(q), 1)
        self.assertEqual(sorted(os.listdir(os.path.join(os.path.dirname(g.paks), q[0]))),
                         ["pakchunk0-ps5_s1.ucas", "zzz_Outfit_P.pak", "zzz_Outfit_P.ucas", "zzz_Outfit_P.utoc"])
        # and now a normal install passes its gate
        code, r0, _ = plan_for(g, rp, "--dry-run")
        code, r1, e1 = plan_for(g, rp, "--local", g.paks, "--confirm", r0["plan_sha256"][:12], "--game-closed")
        self.assertEqual(code, 0, (r1, e1))

    def test_repair_refuses_without_verified_backup(self):
        g = Game("t09b")
        rp, _ = g.record()
        write(os.path.join(g.paks, "pakchunk1-ps5.utoc"), b"changed, no backup")
        code, res, _ = run_cli("repair", "--record", rp, "--work", g.out, "--local", g.paks, "--dry-run")
        self.assertEqual((code, res["code"]), (2, "no_verified_backup_for_drifted_file"))


def old_experiments(g):
    """The console state of the 2026-10-08 scan, with the raw trio in a SUBFOLDER."""
    os.rename(os.path.join(g.paks, "pakchunk0-ps5.utoc"),
              os.path.join(g.paks, "pakchunk0-ps5.utoc.original-603c4879-aaf2-4098-aeda-2ee5d3f0d0f0.bak"))
    write(os.path.join(g.paks, "pakchunk0-ps5.utoc"), b"old derived index")
    write(os.path.join(g.paks, "pakchunk0-ps5_s1.ucas"), b"old sidecar")
    os.rename(os.path.join(g.paks, "pakchunk0-ps5.pak"),
              os.path.join(g.paks, "pakchunk0-ps5.pak.original-9a914e38-5a1e-48a7-ba39-0f1f15f3230d.bak"))
    write(os.path.join(g.paks, "pakchunk0-ps5.pak"), b"rebuilt config pak")
    os.makedirs(os.path.join(g.paks, "~mods"))
    for ext in (".utoc", ".ucas", ".pak"):
        shutil.copyfile(os.path.join(g.mod, "zzz_Outfit_P" + ext),
                        os.path.join(g.paks, "~mods", "zzz_Outfit_P" + ext))
    write(os.path.join(g.paks, "~mods", "readme.txt"), b"inert")


class T11Subfolders(unittest.TestCase):
    """Review F1: pak files in subfolders of paks/ are seen by gate and repair."""

    def test_gate_refuses_mod_in_subfolder(self):
        g = Game("t11a")
        rp, _ = g.record()
        os.makedirs(os.path.join(g.paks, "~mods", "deeper"))
        for ext in (".utoc", ".ucas", ".pak"):
            shutil.copyfile(os.path.join(g.mod, "zzz_Outfit_P" + ext),
                            os.path.join(g.paks, "~mods", "deeper", "zzz_Outfit_P" + ext))
        T05Gate._refuse(self, g, rp, "unrecorded_container_files_present")
        code, res, _ = run_cli("check", "--record", rp, "--local", g.paks)
        self.assertEqual((code, res["code"]), (2, "unrecorded_container_files_present"))
        self.assertIn("~mods/deeper/zzz_Outfit_P.utoc", res["detail"])

    def test_inert_subfolder_passes_and_link_refused(self):
        g = Game("t11b")
        rp, _ = g.record()
        write(os.path.join(g.paks, "~mods", "notes.txt"), b"x")
        code, res, err = run_cli("check", "--record", rp, "--local", g.paks)
        self.assertEqual(code, 0, (res, err))
        self.assertEqual(res["verdict"], "PRISTINE")
        self.assertEqual(res["subfolders"], ["~mods"])
        self.assertEqual(res["recorded_files_checked"], 11)
        os.symlink(g.mod, os.path.join(g.paks, "elsewhere"))
        code, res, _ = run_cli("check", "--record", rp, "--local", g.paks)
        self.assertEqual((code, res["code"]), (2, "link_in_paks_tree"))

    def test_repair_quarantines_subfolder_files_keeping_structure(self):
        g = Game("t11c")
        rp, _ = g.record()
        pristine = {n: paktx.sha256_file(os.path.join(g.paks, n)) for n in os.listdir(g.paks)}
        old_experiments(g)
        console = os.path.join(g.root, "console")
        n_before = sum(len(fs) for _, _, fs in os.walk(console))
        code, res, err = run_cli("repair", "--record", rp, "--work", g.out, "--local", g.paks, "--dry-run")
        self.assertEqual(code, 0, (res, err))
        self.assertEqual((res["fixes"], res["quarantined"]), (2, 4))
        self.assertIn("QUARANTINE ~mods/zzz_Outfit_P.utoc", err)
        code, res2, err2 = run_cli("repair", "--record", rp, "--work", g.out, "--local", g.paks,
                                   "--confirm", res["plan_sha256"][:12], "--game-closed")
        self.assertEqual(code, 0, (res2, err2))
        self.assertEqual(sum(len(fs) for _, _, fs in os.walk(console)), n_before, "repair deleted a file")
        for n, d in pristine.items():
            self.assertEqual(paktx.sha256_file(os.path.join(g.paks, n)), d, n)
        q = [d for d in os.listdir(os.path.dirname(g.paks)) if d.startswith("paktx-quarantine-")]
        self.assertEqual(len(q), 1)
        qd = os.path.join(os.path.dirname(g.paks), q[0])
        self.assertEqual(sorted(os.listdir(os.path.join(qd, "~mods"))),
                         ["zzz_Outfit_P.pak", "zzz_Outfit_P.ucas", "zzz_Outfit_P.utoc"])
        self.assertEqual(os.listdir(os.path.join(g.paks, "~mods")), ["readme.txt"])
        code, r, _ = run_cli("check", "--record", rp, "--local", g.paks)
        self.assertEqual((code, r["verdict"]), (0, "PRISTINE"), r)

    def test_repair_in_steps_over_ftp_with_spaces_in_path(self):
        """The PRISTINE-PAKS.md sequence: one --only step per console OK, then check."""
        g = Game("t11d", remote="/data/homebrew/My Game ABC/game/content/paks")
        rp, _ = g.record()
        pristine = {n: paktx.sha256_file(os.path.join(g.paks, n)) for n in os.listdir(g.paks)}
        old_experiments(g)
        console = os.path.join(g.root, "console")
        srv = FakeFTP(console)
        try:
            addr = "127.0.0.1:%d" % srv.port
            code, r, _ = run_cli("check", "--record", rp, "--ftp", addr)
            self.assertEqual(code, 2, r)
            steps = ["~mods/zzz_Outfit_P.pak,~mods/zzz_Outfit_P.ucas,~mods/zzz_Outfit_P.utoc",
                     "pakchunk0-ps5.utoc,pakchunk0-ps5.pak",
                     "pakchunk0-ps5_s1.ucas"]
            for i, only in enumerate(steps):
                work = os.path.join(g.out, "step%d" % i)
                code, d, err = run_cli("repair", "--record", rp, "--work", work, "--ftp", addr,
                                       "--only", only, "--dry-run")
                self.assertEqual(code, 0, (d, err))
                self.assertIn("ONLY this step", err)
                code, d2, err2 = run_cli("repair", "--record", rp, "--work", work, "--ftp", addr,
                                         "--only", only, "--confirm", d["plan_sha256"][:12],
                                         "--game-closed")
                self.assertEqual((code, d2["result"]), (0, "repair-step-done"), (d2, err2))
            code, r, err = run_cli("check", "--record", rp, "--ftp", addr)
            self.assertEqual((code, r["verdict"]), (0, "PRISTINE"), (r, err))
            for n, d in pristine.items():
                self.assertEqual(paktx.sha256_file(os.path.join(g.paks, n)), d, n)
            verbs = {v for v, _ in srv.commands}
            self.assertNotIn("DELE", verbs)
            self.assertIn("MKD", verbs)
            for v, a in srv.commands:
                if v in ("RETR", "STOR", "RNFR", "RNTO", "DELE"):
                    self.assertFalse(a.endswith(".ucas") and "_s1" not in a and "zzz" not in a, a)
        finally:
            srv.stop()

    def test_only_and_partial_and_stem_refusals(self):
        g = Game("t11e")
        rp, rec = g.record()
        old_experiments(g)
        code, r, _ = run_cli("repair", "--record", rp, "--work", g.out, "--local", g.paks,
                             "--only", "nope.pak", "--dry-run")
        self.assertEqual((code, r["code"]), (2, "only_name_not_in_repair_plan"))
        rec2 = dict(rec)
        rec2["provenance"] = {"PARTIAL": "missing files"}
        rec2.pop("record_sha256")
        rec2["record_sha256"] = paktx.sha256_bytes(paktx.canonical(rec2))
        p2 = os.path.join(g.root, "partial.json")
        paktx.write_json(p2, rec2)
        for cmd in (["repair", "--work", g.out, "--dry-run"], ["check"]):
            code, r, _ = run_cli(cmd[0], "--record", p2, "--local", g.paks, *cmd[1:])
            self.assertEqual((code, r["code"]), (2, "record_is_partial"))
        # a record that forgot one of a container's own files never quarantines it
        os.remove(os.path.join(g.mirror, "pakchunk1-ps5.pak"))
        rp3, _ = g.record()
        code, r, _ = run_cli("repair", "--record", rp3, "--work", g.out, "--local", g.paks, "--dry-run")
        self.assertEqual((code, r["code"]), (2, "unrecorded_file_of_recorded_container"))


class FlipAfterRename(paktx.LocalBackend):
    """Corrupts one byte of `victim` right after it is renamed into place."""
    victim = None

    def rename(self, src, dst):
        super().rename(src, dst)
        if dst == self.victim:
            p = self._p(dst)
            d = bytearray(open(p, "rb").read())
            d[300] ^= 1
            open(p, "wb").write(bytes(d))


class T12AfterChange(unittest.TestCase):
    """Review F2 + F3: any failure after the first change is 'stopped' (exit 3),
    and restore gets out of the state where the live index does not verify."""

    def test_verify_failure_after_change_is_stopped_then_restore_exact(self):
        g = Game("t12a")
        rp, _ = g.record()
        console = os.path.join(g.root, "console")
        before = snapshot(console)
        code, res, _ = plan_for(g, rp, "--dry-run")
        FlipAfterRename.victim = "pakchunk10-ps5.utoc"
        orig = paktx.LocalBackend
        paktx.LocalBackend = FlipAfterRename
        try:
            code, r, err = plan_for(g, rp, "--local", g.paks, "--confirm", res["plan_sha256"][:12],
                                    "--game-closed")
        finally:
            paktx.LocalBackend = orig
        self.assertEqual((code, r["result"], r["console_changed"]), (3, "stopped", "maybe"), (r, err))
        self.assertIn("verify_hash_mismatch", r["detail"])
        self.assertIn("run restore", r["detail"])
        code, v, _ = run_cli("verify", "--plan", r["plan"], "--local", g.paks)
        self.assertEqual(v["state"]["verdict"], "PARTIAL_OR_DRIFT")
        sha = res["plan_sha256"][:12]
        code, d, err = run_cli("restore", "--plan", r["plan"], "--local", g.paks, "--dry-run")
        self.assertEqual(code, 0, (d, err))
        self.assertIn("renamed aside", err)
        code, r2, err2 = run_cli("restore", "--plan", r["plan"], "--local", g.paks, "--confirm", sha,
                                 "--game-closed", "--cleanup", "--allow-delete")
        self.assertEqual(code, 0, (r2, err2))
        self.assertEqual(r2["state"]["verdict"], "PRISTINE")
        after = snapshot(console)
        for n, dg in before.items():
            self.assertEqual(after[n], dg, n)
        extra = sorted(set(after) - set(before))
        self.assertEqual(len(extra), 1, extra)
        self.assertTrue(extra[0].endswith(".retired") and ".foreign-" in extra[0], extra)
        code, c, _ = run_cli("check", "--record", rp, "--local", g.paks)
        self.assertEqual((code, c["verdict"]), (0, "PRISTINE"))

    def test_refusal_after_change_is_stopped_and_before_change_is_refused(self):
        g = Game("t12b")
        rp, _ = g.record()
        console = os.path.join(g.root, "console")
        pristine = os.path.join(g.root, "pristine")
        shutil.copytree(console, pristine)
        before = snapshot(console)
        code, res, _ = plan_for(g, rp, "--dry-run")
        plan = json.load(open(res["plan"]))
        uploads = [op for op in plan["operations"] if op["op"] == "upload"]
        # (a) the FIRST upload's local file changed: nothing started -> Refused
        keep = open(uploads[0]["local"], "rb").read()
        open(uploads[0]["local"], "ab").write(b"x")
        be = paktx.LocalBackend(g.paks)
        with self.assertRaises(paktx.Refused) as cm:
            paktx.execute(plan, plan["operations"], be,
                          paktx.Journal(os.path.join(g.root, "ja.json"), plan, "install"), lambda s: None)
        self.assertEqual((cm.exception.code, be.attempted), ("local_artifact_changed", 0))
        self.assertEqual(snapshot(console), before)
        open(uploads[0]["local"], "wb").write(keep)
        # (b) the SECOND upload's local file changed: one upload done -> Stopped
        open(uploads[1]["local"], "ab").write(b"x")
        be = paktx.LocalBackend(g.paks)
        with self.assertRaises(paktx.Stopped) as cm:
            paktx.execute(plan, plan["operations"], be,
                          paktx.Journal(os.path.join(g.root, "jb.json"), plan, "install"), lambda s: None)
        self.assertIn("local_artifact_changed", str(cm.exception))
        j = json.load(open(os.path.join(g.root, "jb.json")))
        self.assertEqual(j["events"][-1]["state"], "stopped_after_change")

    def test_read_error_in_verify_phase_is_stopped(self):
        g = Game("t12c")
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        plan = json.load(open(res["plan"]))
        n_mut = sum(1 for op in plan["operations"] if op["op"] in ("upload", "rename"))

        class Flaky(paktx.LocalBackend):
            def sha256(self, rel, size):
                if self.attempted >= n_mut:
                    raise ConnectionResetError("data connection lost")
                return super().sha256(rel, size)
        be = Flaky(g.paks)
        with self.assertRaises(paktx.Stopped) as cm:
            paktx.execute(plan, plan["operations"], be,
                          paktx.Journal(os.path.join(g.root, "jc.json"), plan, "install"), lambda s: None)
        self.assertIn("ConnectionResetError", str(cm.exception))

    def test_lost_reply_after_each_change_restores_exactly(self):
        g = Game("t12d")
        rp, _ = g.record()
        console = os.path.join(g.root, "console")
        pristine = os.path.join(g.root, "pristine")
        shutil.copytree(console, pristine)
        before = snapshot(console)
        code, res, _ = plan_for(g, rp, "--dry-run")
        plan = json.load(open(res["plan"]))
        mutating = sum(1 for op in plan["operations"] if op["op"] in ("upload", "rename"))
        points = 0
        for k in range(mutating):
            shutil.rmtree(console)
            shutil.copytree(pristine, console)
            jpath = os.path.join(g.root, "jd%d.json" % k)
            be = paktx.LocalBackend(g.paks, fail_after_change=k)
            with self.assertRaises(paktx.Stopped):
                paktx.execute(plan, plan["operations"], be, paktx.Journal(jpath, plan, "install"),
                              lambda s: None)
            be2 = paktx.LocalBackend(g.paks)
            ops, _ = paktx.restore_operations(plan, be2, cleanup=True)
            paktx.execute(plan, ops, be2, paktx.Journal(jpath, plan, "restore"), lambda s: None)
            after = snapshot(console)
            for n, d in before.items():
                self.assertEqual(after.get(n), d, "k=%d %s" % (k, n))
            self.assertEqual(sorted(set(after) - set(before)), [], "k=%d leftovers" % k)
            points += 1
        self.assertEqual(points, mutating)
        print("\n[lost-reply-after-change] %d points restored byte-exact" % points)

    def test_unknown_live_without_backup_is_preserved(self):
        g = Game("t12e")
        rp, _ = g.record()
        code, res, _ = plan_for(g, rp, "--dry-run")
        write(os.path.join(g.paks, "pakchunk0-ps5.utoc"), b"something else")
        code, r, _ = run_cli("restore", "--plan", res["plan"], "--local", g.paks, "--dry-run")
        self.assertEqual((code, r["code"], r["console_changed"]), (2, "live_file_unknown_identity", False))
        self.assertEqual(open(os.path.join(g.paks, "pakchunk0-ps5.utoc"), "rb").read(), b"something else")


class T13OnlyInto(unittest.TestCase):
    """Review N5: the named fallback (one container only) exists and is exact."""

    def test_only_into_primary_skips_other_owner(self):
        g = Game("t13")
        rp, _ = g.record()
        code, res, err = plan_for(g, rp, "--dry-run", "--only-into", "pakchunk0-ps5")
        self.assertEqual(code, 0, (res, err))
        self.assertEqual(res["targets"], ["pakchunk0-ps5"])
        plan = json.load(open(res["plan"]))
        self.assertEqual(plan["skipped_by_only_into"]["pakchunk10-ps5"]["zzz_Outfit_P"]["replaced"],
                         ["%016x" % 0x2001])
        self.assertIn("NOT INSTALLED (--only-into pakchunk0-ps5): 1 packages", err)
        code, full, _ = plan_for(g, rp, "--dry-run")
        self.assertNotEqual(full["plan_sha256"], res["plan_sha256"])
        before = snapshot(os.path.join(g.root, "console"))
        code, r, err = run_cli("cycle", "--record", rp, "--mirror", g.mirror, "--mod", g.mod,
                               "--work", g.out, "--local", g.paks, "--only-into", "pakchunk0-ps5")
        self.assertEqual(code, 0, (r, err))
        self.assertEqual(r["after_install"]["verdict"], "INSTALLED")
        self.assertEqual(snapshot(os.path.join(g.root, "console")), before)
        code, r, _ = plan_for(g, rp, "--dry-run", "--only-into", "pakchunk1-ps5")
        self.assertEqual((code, r["code"]), (2, "only_into_is_not_a_destination"))


class T14RecordShaStable(unittest.TestCase):
    """Review N8: the record sha does not depend on where the folders sit."""

    def test_header_outside_record_folder(self):
        shas = []
        for name in ("t14a", "t14b/nested"):
            g = Game(name)
            out = os.path.join(g.root, "rec", "record.json")
            os.makedirs(os.path.dirname(out))
            rec = paktx.make_record(B, g.mirror, "TEST00001", "01.000.000", PAKS_REMOTE, g.sizes,
                                    g.headers, None, relative_to=os.path.dirname(out))
            self.assertEqual(rec["containers"]["pakchunk0-ps5"]["header"]["source"],
                             "../headers/pakchunk0-ps5.container-header.bin")
            paktx.write_json(out, rec)
            loaded = paktx.load_record(out)
            self.assertEqual(len(paktx.record_header_bytes(loaded, "pakchunk0-ps5", g.mirror, B)),
                             rec["containers"]["pakchunk0-ps5"]["header"]["bytes"])
            shas.append(rec["record_sha256"])
        self.assertEqual(shas[0], shas[1])


if __name__ == "__main__":
    os.makedirs(WORK, exist_ok=True)
    for name in ("REPAK", "BUILDER"):
        if not os.path.isfile(globals()[name]):
            print("MISSING required input %s=%s" % (name, globals()[name]))
            sys.exit(1)
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    count = suite.countTestCases()
    print("paktx host tests: %d test cases" % count)
    res = unittest.TextTestRunner(verbosity=2).run(suite)
    ran, bad = res.testsRun, len(res.failures) + len(res.errors)
    skipped = len(res.skipped)
    print("SUMMARY tests_run=%d failed=%d skipped=%d passed=%d" % (ran, bad, skipped, ran - bad - skipped))
    if ran == 0 or bad or skipped:
        print("GATE FAIL (zero tests, failures, or skips)")
        sys.exit(1)
    print("GATE PASS")
