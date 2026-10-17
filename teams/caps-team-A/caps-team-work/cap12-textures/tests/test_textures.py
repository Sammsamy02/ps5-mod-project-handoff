#!/usr/bin/env python3
"""Host tests for capability #12 DERIVED TEXTURES (Team A, cap12-textures). Mac-only.

U-series: the exact R<->B block swap (bcn_swap), the two independent decoders
(bcn_decode = Python reference, metal_bcn_check = the Mac's GPU), the texture
layout reader (texture_package), the per-slot default reader
(material_defaults), the planner's swap route and slot proof (param_roles),
the derive gate (reparent_plan.make_derive_ok / derived_consistency) and the
hair check (hair_mask_check).
X-series: paktx install --substitute with a derived texture on a synthetic game
whose packages are REAL bytes (Leon's MI_Leon_Eye + its 3 textures incl. their
BulkData chunks + M_Leon_Body; the real fetched M_MRO_Cloth_NonVT header as the
base parent), made by derive_substitutes.py; install / re-read / restore and
planted faults (tampered chunk, proof, plan, consumers, base override, format).

Every gate counts what it checked and the run FAILS when a count is zero.
"""
import copy
import hashlib
import io
import json
import os
import random
import shutil
import struct
import subprocess
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
TEAM = os.path.dirname(HERE)
TOOLS = os.path.join(TEAM, "PATCH", "tools")
sys.path.insert(0, HERE)
import test_paktx as T  # noqa: E402  (the staged paktx + pinned builder)

sys.path.insert(1, TOOLS)
import bcn_decode as D  # noqa: E402
import bcn_swap as S  # noqa: E402
import blake3 as FB3  # noqa: E402
import cityhash  # noqa: E402
import container_header as FCH  # noqa: E402
import hair_mask_check as HMC  # noqa: E402
import material_defaults as MD  # noqa: E402
import param_roles as PR  # noqa: E402
import reparent_plan as RP  # noqa: E402
import texture_package as TP  # noqa: E402
import utoc as FUTOC  # noqa: E402
import zen as FZEN  # noqa: E402
import ps5_tiling as PT  # noqa: E402
import derive_substitutes as DS  # noqa: E402

paktx, B = T.paktx, T.B
REPO = T.REPO
L25 = os.path.join(REPO, "outputs/2026-10-08-l25-prep")
EV = os.path.join(L25, "evidence")
FET = os.path.join(EV, "cap12-fetched-20261013")
GEN = os.path.join(TOOLS, "derive_substitutes.py")
LEON = os.path.join(REPO, ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted",
                    "zzz_Leon_Townfall_2K_P")
PARENT = "/Game/Townfall/Characters/Materials/M_MRO_Cloth_NonVT"
PARENT_HDR = os.path.join(FET, "headers-r2", "9f07c896e8869042.hdr.bin")
WORK = os.path.join(HERE, "work", "tex")
BIN = os.path.join(HERE, "work", "bin")
GPU = os.path.join(BIN, "metal_bcn_check")
LAW = cityhash.ue_name_hash
MI = "/Game/Leon/MI_Leon_Eye"
OLD = "/Game/Leon/M_Leon_Body"
ORM = "/Game/Leon/Textures/T_Leon_Eye_ORM"
TEX = ["/Game/Leon/Textures/T_Leon_Eye_N", "/Game/Leon/Textures/T_Leon_Eye_D", ORM]
DECIDE = ["--accept-decision", "accept-unproven-opaque"]
TEAMB = "/WORKSPACE/.codex/worktrees/translation-core/caps-team-B/caps-team-work/cap12-layout-invisible"
TILEREF = os.path.join(TEAMB, "PATCH", "tools")
CENSUS = json.load(open(os.path.join(TOOLS, "ps5_layout_census.json")))
GAME_CHUNKS = os.path.join(EV, "cap12-layout-fetch-20261013", "chunks", "pakchunk0-ps5")
PROBE, ORACLE = DS.load_oracle(TILEREF)


def choose(f, w, h, n):
    return PT.choose(f, w, h, n, CENSUS)


def keep(fmt, data):
    return data, {"format": fmt, "blocks": None, "endpoint_order_fixed": 0, "bc7_rotation_remapped": 0}
GATES = {}


def count(name, n=1):
    GATES[name] = GATES.get(name, 0) + n


def compile_gpu():
    if not os.path.isfile(GPU):
        os.makedirs(BIN, exist_ok=True)
        subprocess.run(["swiftc", "-O", os.path.join(TOOLS, "metal_bcn_check.swift"), "-o", GPU],
                       check=True, capture_output=True, timeout=600)


def gpu_compare(fmt, a, b):
    d = os.path.join(HERE, "work")
    pa, pb = os.path.join(d, "gpu-a.bin"), os.path.join(d, "gpu-b.bin")
    open(pa, "wb").write(a)
    open(pb, "wb").write(b)
    p = subprocess.run([GPU, "compare", fmt, pa, pb], capture_output=True, text=True, timeout=600)
    return p.returncode, json.loads(p.stdout.strip().splitlines()[-1])


def gpu_decode(fmt, a):
    d = os.path.join(HERE, "work")
    pa, po = os.path.join(d, "gpu-a.bin"), os.path.join(d, "gpu-o.bin")
    open(pa, "wb").write(a)
    p = subprocess.run([GPU, "decode", fmt, pa, po], capture_output=True, text=True, timeout=600)
    assert p.returncode == 0, p.stdout
    return open(po, "rb").read()


def rand_blocks(fmt, n, seed):
    rng = random.Random(seed)
    bb = S.BLOCK_BYTES[fmt]
    out = []
    for k in range(n):
        if fmt == "PF_BC7":
            m = k % 8
            v = rng.getrandbits(128) & ~((1 << (m + 1)) - 1) | (1 << m)
            out.append(v.to_bytes(16, "little"))
        else:
            out.append(rng.getrandbits(8 * bb).to_bytes(bb, "little"))
    return out


def exact_blocks(fmt, n, seed):
    """Random blocks the swap accepts (BC2/BC3 decoder-dependent blocks dropped)."""
    keep = []
    for b in rand_blocks(fmt, 2 * n, seed):
        try:
            S.swap_rb(fmt, b)
            keep.append(b)
        except S.SwapRefused:
            pass
        if len(keep) == n:
            break
    return b"".join(keep)


_LEON = {}


def leon():
    """Real Leon chunks {name: {kind: bytes}} + store entries (read-only)."""
    if not _LEON:
        t = FUTOC.load(LEON + ".utoc")

        def rd(o, n):
            with open(LEON + ".ucas", "rb") as fh:
                fh.seek(o)
                return fh.read(n)
        _c, st = FCH.parse(t.read_chunk(t.rows_of_type(6)[0], rd))
        by = {}
        for i, c in enumerate(t.chunk_ids):
            by.setdefault(int.from_bytes(c[:8], "little"), {})[c[11]] = i
        _LEON["_store"] = st
        _LEON["_toc"] = t
        _LEON["_rd"] = rd
        for name in [MI, OLD] + TEX:
            pid = LAW(name)
            _LEON[name] = (pid, {k: t.read_chunk(i, rd) for k, i in by[pid].items()}, list(st[pid]["imports"]))
    return _LEON


def leon_texture(name):
    lv = leon()
    t, rd = lv["_toc"], lv["_rd"]
    pid = LAW(name)
    rows = {c[11]: i for i, c in enumerate(t.chunk_ids) if int.from_bytes(c[:8], "little") == pid}
    return t.read_chunk(rows[1], rd), (t.read_chunk(rows[2], rd) if 2 in rows else None)


# ---------------------------------------------------------------------------
# U-series
# ---------------------------------------------------------------------------
class U01SwapExactBothDecoders(unittest.TestCase):
    def test_every_format_every_texel(self):
        for fmt in S.SUPPORTED:
            data = exact_blocks(fmt, 6000, 11)
            out, facts = S.swap_rb(fmt, data)
            py = D.swap_proof(fmt, data, out)
            self.assertTrue(py["exact"], (fmt, py))
            rc, gp = gpu_compare(fmt, data, out)
            self.assertEqual(rc, 0, gp)
            self.assertTrue(gp["exact"], gp)
            self.assertEqual(gp["texels_compared"], py["texels_compared"])
            self.assertGreater(py["texels_where_r_differs_from_b"], 0)
            count("swap: blocks proven by the Python decoder", py["blocks"])
            count("swap: texels proven by the GPU decoder", gp["texels_compared"])
            count("swap: formats proven", 1)
        self.assertEqual(GATES["swap: formats proven"], len(S.SUPPORTED))

    def test_endpoint_order_fix_and_rotation_are_exercised(self):
        _o, f1 = S.swap_rb("PF_DXT1", b"".join(rand_blocks("PF_DXT1", 4000, 3)))
        self.assertGreater(f1["endpoint_order_fixed"], 0)
        _o, f7 = S.swap_rb("PF_BC7", b"".join(rand_blocks("PF_BC7", 4000, 4)))
        self.assertGreater(f7["bc7_rotation_remapped"], 0)
        count("swap: special cases exercised", f1["endpoint_order_fixed"] + f7["bc7_rotation_remapped"])

    def test_swap_twice_is_identity(self):
        for fmt in S.SUPPORTED:
            data = exact_blocks(fmt, 2000, 5)
            once, _ = S.swap_rb(fmt, data)
            twice, _ = S.swap_rb(fmt, once)
            self.assertEqual(twice, data, fmt)
            count("swap: involution checked (blocks)", len(data) // S.BLOCK_BYTES[fmt])


class U02DecodersCrossChecked(unittest.TestCase):
    def test_bc7_python_tables_equal_gpu(self):
        data = b"".join(rand_blocks("PF_BC7", 16000, 21))
        py = b"".join(D.decode("PF_BC7", data))
        self.assertEqual(py, gpu_decode("PF_BC7", data))
        count("decoders: BC7 texels identical (all 8 modes, all partitions)", len(py) // 4)

    def test_bc1_bc2_bc3_modes_agree_rgb_within_rounding(self):
        for fmt in ("PF_DXT1", "PF_DXT3", "PF_DXT5"):
            data = exact_blocks(fmt, 6000, 22)
            py = b"".join(D.decode(fmt, data))
            gp = gpu_decode(fmt, data)
            self.assertEqual(len(py), len(gp))
            worst = max(abs(py[i] - gp[i]) for i in range(len(py)) if i % 4 != 3)
            self.assertLessEqual(worst, 3, fmt)
            if fmt != "PF_DXT5":
                self.assertEqual(py[3::4], gp[3::4], fmt + " alpha/mode")
            count("decoders: BC1/2/3 texels compared", len(py) // 4)


class U03PlantedSwapFaults(unittest.TestCase):
    def test_naive_bc1_swap_is_caught_by_both(self):
        data = b"".join(rand_blocks("PF_DXT1", 5000, 31))
        naive = bytearray(data)
        for o in range(0, len(data), 8):
            for k in (0, 2):
                c = data[o + k] | (data[o + k + 1] << 8)
                naive[o + k:o + k + 2] = (((c & 31) << 11) | (c & 0x7E0) | (c >> 11)).to_bytes(2, "little")
        py = D.swap_proof("PF_DXT1", data, bytes(naive))
        self.assertFalse(py["exact"])
        rc, gp = gpu_compare("PF_DXT1", data, bytes(naive))
        self.assertNotEqual(rc, 0)
        self.assertFalse(gp["exact"])
        self.assertGreater(gp["mismatched_texels"], 0)
        count("faults: decode mismatch caught (both decoders)", 1)

    def test_wrong_channel_order_is_caught_by_both(self):
        # identity (no swap) and a G<->B swap instead of R<->B
        for fmt in ("PF_DXT1", "PF_BC7", "PF_B8G8R8A8"):
            data = exact_blocks(fmt, 3000, 32)
            wrong = [data]
            if fmt == "PF_B8G8R8A8":
                w = bytearray(data)
                w[0::4], w[1::4] = data[1::4], data[0::4]
                wrong.append(bytes(w))
            if fmt == "PF_BC7":
                w = bytearray()
                for o in range(0, len(data), 16):
                    blk = data[o:o + 16]
                    m = S.bc7_mode(blk)
                    ns, pb, rb, isb, cb = S.BC7_MODES[m]
                    v = int.from_bytes(blk, "little")
                    st = m + 1 + pb + rb + isb
                    wd = 2 * ns * cb
                    mk = (1 << wd) - 1
                    g, bl = (v >> (st + wd)) & mk, (v >> (st + 2 * wd)) & mk
                    v &= ~((mk << (st + wd)) | (mk << (st + 2 * wd)))
                    v |= (bl << (st + wd)) | (g << (st + 2 * wd))
                    w += v.to_bytes(16, "little")
                wrong.append(bytes(w))
            for x in wrong:
                self.assertFalse(D.swap_proof(fmt, data, x)["exact"], fmt)
                rc, gp = gpu_compare(fmt, data, x)
                self.assertFalse(gp["exact"], fmt)
                count("faults: wrong channel order caught (both decoders)", 1)

    def test_unsupported_or_ambiguous_input_refused(self):
        for fmt in ("PF_BC5", "PF_BC4", "PF_BC6H", "PF_FloatRGBA", "PF_G8", "PF_ASTC_4x4"):
            with self.assertRaises(S.SwapRefused):
                S.swap_rb(fmt, bytes(64))
            count("faults: unsupported format refused", 1)
        with self.assertRaises(S.SwapRefused):
            S.swap_rb("PF_DXT1", bytes(12))
        with self.assertRaises(S.SwapRefused):
            S.swap_rb("PF_BC7", bytes(16))          # reserved mode
        flip = bytes(8) + struct.pack("<HHI", 0x0001, 0x0800, 0xFFFFFFFF)  # c0 < c1, order flips, index 3
        with self.assertRaises(S.SwapRefused):
            S.swap_rb("PF_DXT5", flip)
        # c0 == c1 with index 3, and c0 < c1 without a flip: exact under BOTH readings -> accepted
        for ok in (bytes(8) + struct.pack("<HHI", 0x1234, 0x1234, 0xC0000000),
                   bytes(8) + struct.pack("<HHI", 0x0001, 0x0002, 0xFFFFFFFF)):
            out, _ = S.swap_rb("PF_DXT5", ok)
            self.assertTrue(D.swap_proof("PF_DXT5", ok, out)["exact"])
            rc, gp = gpu_compare("PF_DXT5", ok, out)
            self.assertTrue(gp["exact"], gp)
        count("faults: malformed/ambiguous blocks refused", 3)


class U04TextureLayout(unittest.TestCase):
    def test_all_leon_textures_two_source_layout(self):
        lv = leon()
        t, rd = lv["_toc"], lv["_rd"]
        by = {}
        for i, c in enumerate(t.chunk_ids):
            by.setdefault(int.from_bytes(c[:8], "little"), {})[c[11]] = i
        n = 0
        for pid, rows in by.items():
            if 1 not in rows:
                continue
            ch = t.read_chunk(rows[1], rd)
            if "/Textures/" not in FZEN.ZenPackage(ch).name:
                continue
            bulk = t.read_chunk(rows[2], rd) if 2 in rows else None
            lay = TP.layout(ch, None if bulk is None else len(bulk))
            self.assertEqual(sum(m["size"] for m in lay["mips"] if m["where"] == "bulk"), len(bulk))
            n += 1
        self.assertEqual(n, 34)
        count("layout: real Leon textures proven (bulk map + mip table)", n)

    def _corrupt(self, chunk, bulk, fn):
        c, b = bytearray(chunk), (bytearray(bulk) if bulk is not None else None)
        fn(c, b)
        with self.assertRaises(TP.LayoutError):
            TP.layout(bytes(c), None if b is None else len(b))
        count("layout: planted corruption refused", 1)

    def test_planted_corruptions_refused(self):
        chunk, bulk = leon_texture("/Game/Leon/Textures/T_Leon_Eye_ORM")
        lay = TP.layout(chunk, len(bulk))
        p = FZEN.ZenPackage(chunk)
        entries, bpos = TP.bulk_map(chunk)
        eo = lay["export_offset"]
        inl = [m for m in lay["mips"] if m["where"] == "inline"][0]
        exp = chunk[eo:]
        s = exp.find(b"PF_")

        def bulk_size(c, b):
            struct.pack_into("<q", c, bpos + 8 + 16, entries[0][2] + 8)

        def inline_offset(c, b):
            k = [i for i, e in enumerate(entries) if not e[3] & TP.F_SEPARATE][0]
            struct.pack_into("<q", c, bpos + 8 + 32 * k, entries[k][0] + 4)

        def index_field(c, b):
            struct.pack_into("<I", c, inl["offset"] - 4, 0)

        def fname_format(c, b):
            struct.pack_into("<I", c, eo + s - 48, p.names.index("None"))

        def trailing(c, b):
            c += b"\0\0\0\0"

        def virtual(c, b):
            q = len(c) - 4 - 8 - 4
            struct.pack_into("<I", c, q, 1)

        def skip(c, b):
            struct.pack_into("<q", c, eo + s - 40, struct.unpack_from("<q", c, eo + s - 40)[0] + 4)

        def compressed(c, b):
            struct.pack_into("<I", c, bpos + 8 + 24, entries[0][3] | 0x2)

        def placeholder(c, b):
            c[eo + s - 20] = 1
        for fn in (bulk_size, inline_offset, index_field, fname_format, trailing, virtual, skip, compressed,
                   placeholder):
            self._corrupt(chunk, bulk, fn)
        with self.assertRaises(TP.LayoutError):
            TP.layout(chunk, len(bulk) - 16)
        with self.assertRaises(TP.LayoutError):
            TP.layout(chunk, None)
        count("layout: planted corruption refused", 2)

    def test_derive_changes_only_texel_payloads(self):
        for name in ("/Game/Leon/Textures/T_Leon_Eye_ORM", "/Game/Leon/Textures/T_Leon_Lash_ORM"):
            chunk, bulk = leon_texture(name)
            nc, nb, lay, facts = TP.derive(chunk, bulk, S.swap_rb)
            hs = struct.unpack_from("<I", chunk, 4)[0]
            self.assertEqual(nc[:hs], chunk[:hs])
            self.assertEqual(len(nc), len(chunk))
            self.assertEqual(len(nb), len(bulk))
            self.assertNotEqual(nb, bulk)
            for m, a, b in zip(lay["mips"], TP.payload(lay, chunk, bulk), TP.payload(lay, nc, nb)):
                self.assertEqual(b, S.swap_rb(lay["format"], a)[0])
            before = b"".join(TP.payload(lay, chunk, bulk))
            after = b"".join(TP.payload(lay, nc, nb))
            self.assertTrue(D.swap_proof(lay["format"], before, after)["exact"])
            rc, gp = gpu_compare(lay["format"], before, after)
            self.assertTrue(gp["exact"], gp)
            count("layout: derived textures proven on every mip (both decoders)", 1)


def whole_and_registry():
    if not hasattr(whole_and_registry, "cache"):
        import asset_registry
        import material_class as mc
        checked = [r for p in ("round1-check.json", "round2-check.json", "round3-check.json")
                   for r in json.load(open(os.path.join(EV, "cap12-20261012", p)))["checked"] if "verified" in r]
        headers = {r["package"]: r for r in checked}
        reg = asset_registry.load(os.path.join(
            REPO, ".superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall",
            "AssetRegistry.bin"))
        whole = RP.fetched_packages([os.path.join(FET, "headers-r%d" % i) for i in (1, 2, 3)], checked)
        binds = {}
        for pkg, z in whole.items():
            b, _ = PR.texture_bindings(z)
            if not b:
                continue
            m, chain = mc.master_of(pkg, reg, headers)
            for node in (chain if m else [pkg]):
                d = binds.setdefault(node, {})
                for q, t in b.items():
                    d.setdefault(q, set()).add(t)
        whole_and_registry.cache = (whole, reg, binds)
    return whole_and_registry.cache


class U05SlotDefaults(unittest.TestCase):
    def tc(self, reg):
        return lambda t: PR.tex_class((reg.asset(t) or {}).get("tags"))

    def test_real_parent_pairing_cross_checked(self):
        whole, reg, binds = whole_and_registry()
        pairs, why = MD.slot_defaults(whole[PARENT])
        self.assertIsNone(why)
        self.assertEqual(pairs["MRO"].rsplit("/", 1)[-1], "Default_MRO")
        self.assertEqual(pairs["Damage - Mask"].rsplit("/", 1)[-1], "VT_Default_Black_Linear")
        n, bad = MD.validate(pairs, binds[PARENT], self.tc(reg))
        self.assertIsNone(bad)
        self.assertEqual(n, 4)
        self.assertEqual(reg.asset(pairs["MRO"])["tags"]["VirtualTextureStreaming"], "False")
        count("defaults: parent slots cross-checked against the game's bindings", n)

    def test_shifted_or_ambiguous_pairing_refused(self):
        whole, reg, binds = whole_and_registry()
        z = whole[PARENT]
        pairs, _ = MD.slot_defaults(z)
        shifted = dict(pairs)
        shifted["MRO"], shifted["Normal"] = pairs["Normal"], pairs["MRO"]
        n, bad = MD.validate(shifted, binds[PARENT], self.tc(reg))
        self.assertIsNotNone(bad)
        # a second default-texture array makes the reader refuse
        top = [e for e in z.exports if e["outer"][0] == "Null"][0]
        exp = bytearray(z.export_bytes(top))
        idx = {n_: i for i, n_ in enumerate(z.names)}
        k = None
        for o in range(len(exp) - 24):
            if struct.unpack_from("<IIII", exp, o + 4) == (idx[pairs["Param"]], 0, idx["T_Foam_001"], 0):
                k = o
                break
        self.assertIsNotNone(k)
        n9 = struct.unpack_from("<I", exp, k)[0]
        arr = bytes(exp[k:k + 4 + 20 * n9])
        tail = len(exp) - len(arr) - 64
        exp[tail:tail + len(arr)] = arr

        class P:
            pass
        p2 = P()
        p2.exports = [dict(top)]
        p2.names = z.names
        p2.imported_packages = z.imported_packages
        p2.export_bytes = lambda e: bytes(exp)
        got, why = MD.slot_defaults(p2)
        self.assertIsNone(got)
        self.assertIn("arrays", why)
        count("defaults: planted pairing faults refused", 2)


def synth(bad=None, derive=True, slot_default=None):
    tags = {
        "/G/T_A_D": {"CompressionSettings": "TC_BC7", "SRGB": "True", "VirtualTextureStreaming": "False"},
        "/G/T_A_N": {"CompressionSettings": "TC_Normalmap", "SRGB": "False", "VirtualTextureStreaming": "False"},
        "/G/T_A_MRO": {"CompressionSettings": "TC_BC7", "SRGB": "False", "VirtualTextureStreaming": "False"},
        "/G/T_B_MRO": {"CompressionSettings": "TC_BC7", "SRGB": "False", "VirtualTextureStreaming": "False"},
        "/G/T_C_Masks": {"CompressionSettings": "TC_BC7", "SRGB": "False", "VirtualTextureStreaming": "False"},
    }
    defaults = [tags["/G/T_A_D"], tags["/G/T_A_N"], tags["/G/T_A_MRO"]]
    mod_bind = {"BaseColor": "/M/T_X_D", "Normal": "/M/T_X_N", "ORM": "/M/T_X_ORM"}
    fmts = {"/M/T_X_D": "PF_DXT1", "/M/T_X_N": "PF_BC5", "/M/T_X_ORM": "PF_DXT1"}
    base_names = ["BaseColour", "Normal", "MRO"]
    base_bind = {"BaseColour": {"/G/T_A_D"}, "Normal": {"/G/T_A_N"}, "MRO": {"/G/T_A_MRO", "/G/T_B_MRO"}}
    dok = (lambda t, c="keep": None) if derive else None
    if bad == "mod_texture_name":
        mod_bind["ORM"] = "/M/T_X_Mask"
        fmts["/M/T_X_Mask"] = "PF_DXT1"
    elif bad == "game_texture_name":
        base_bind["MRO"] = {"/G/T_A_MRO", "/G/T_C_Masks"}
    elif bad == "game_texture_srgb":
        tags["/G/T_B_MRO"] = dict(tags["/G/T_B_MRO"], SRGB="True")
    elif bad == "not_mirror":
        base_names[2] = "RMO"
        base_bind["RMO"] = base_bind.pop("MRO")
    elif bad == "two_slots":
        base_names.append("mro")
        base_bind["mro"] = {"/G/T_A_MRO"}
    elif bad == "format":
        fmts["/M/T_X_ORM"] = "PF_BC5"
    elif bad == "derive_ok":
        dok = (lambda t, c="keep": "consumer can draw" if c == "swap_rb" else None)
    elif bad == "virtual_slot":
        tags["/G/T_B_MRO"] = dict(tags["/G/T_B_MRO"], VirtualTextureStreaming="True")
    mapping, lost = PR.map_params(["BaseColor", "Normal", "ORM"], mod_bind, fmts, base_names, base_bind,
                                  lambda t: tags.get(t), {"BaseColor", "Normal", "ORM", "MI_X"},
                                  mod_tex=lambda t: {"proven": True}, defaults=defaults, derive_ok=dok,
                                  slot_default=slot_default)
    return mapping, lost


class U06PlannerSwapRoute(unittest.TestCase):
    def test_positive_two_sources_each_side(self):
        mapping, lost = synth()
        self.assertEqual(lost, {})
        m = mapping["ORM"]
        self.assertEqual((m["to"], m["how"]), ("MRO", "derived:swap_rb"))
        self.assertEqual(m["derive"], {"texture": "/M/T_X_ORM", "op": "swap_rb", "from_letters": "ORM",
                                       "to_letters": "MRO", "format": "PF_DXT1"})
        self.assertEqual(m["evidence"]["mod_letters"]["texture_name"], "ORM")
        count("planner: swap route positive", 1)

    def test_one_source_or_wrong_order_is_lost(self):
        for bad in ("mod_texture_name", "game_texture_name", "game_texture_srgb", "not_mirror", "two_slots",
                    "format", "derive_ok", "virtual_slot"):
            mapping, lost = synth(bad)
            self.assertNotIn("ORM", mapping, bad)
            self.assertIn("ORM", lost, bad)
            count("planner: planted fault -> ORM lost", 1)
        mapping, lost = synth(derive=False)
        self.assertIn("would need a channel swizzle", lost["ORM"])   # round-4 wording when off
        count("planner: planted fault -> ORM lost", 1)

    def test_slot_own_default(self):
        vt = {"CompressionSettings": "TC_BC7", "SRGB": "False", "VirtualTextureStreaming": "True"}
        srgb = {"CompressionSettings": "TC_BC7", "SRGB": "True", "VirtualTextureStreaming": "False"}
        plain = {"CompressionSettings": "TC_BC7", "SRGB": "False", "VirtualTextureStreaming": "False"}
        for d, ok in ((vt, False), (srgb, False), (plain, True)):
            mapping, lost = synth(slot_default=lambda q, d=d: d if q == "MRO" else None)
            self.assertEqual("ORM" in mapping, ok, d)
            count("planner: per-slot default cases", 1)
        self.assertEqual(PR.rb_mirror("ORM"), "MRO")
        self.assertEqual(PR.rb_mirror("ABCD"), "CBAD")
        self.assertIsNone(PR.rb_mirror("XY"))


class U07DeriveGate(unittest.TestCase):
    def setUp(self):
        import mod_material_audit as mma
        self.pk, _ = mma.load_mod(LEON + ".utoc", LEON + ".ucas")
        self.audit = json.load(open(os.path.join(EV, "cap12-20261012", "leon-material-audit-5hdr.json")))
        self.toc = FUTOC.load(LEON + ".utoc")
        self.rd = mma.reader(LEON + ".ucas")

    def test_gate_real_leon(self):
        dok = RP.make_derive_ok(self.pk, self.audit, {}, self.toc, self.rd, CENSUS)
        self.assertIsNone(dok("/Game/Leon/Textures/T_Leon_UpperBody_ORM"))
        self.assertIsNone(dok("/Game/Leon/Textures/T_Leon_UpperBody_N", "keep"))
        self.assertIn("not derivable", dok("/Game/Leon/Textures/T_Leon_UpperBody_N"))      # BC5 has no blue
        bad = copy.deepcopy(CENSUS)
        bad["rows"] = [r for r in bad["rows"] if not (r["element_bytes"] == 8 and r["ext_data"] == 9)]
        why = RP.make_derive_ok(self.pk, self.audit, {}, self.toc, self.rd, bad)("/Game/Leon/Textures/T_Leon_UpperBody_D",
                                                                                "keep")
        self.assertIn("not settled by game data", why)
        count("derive gate: real cases", 4)

    def test_gate_refusals(self):
        tex = "/Game/Leon/Textures/T_Leon_UpperBody_ORM"
        a2 = copy.deepcopy(self.audit)
        for m in a2["materials"]:
            if m["package"] == "/Game/Leon/M_Leon_Body":
                m["verdict"] = "HAS_TARGET_SHADERS"
        self.assertIn("can draw", RP.make_derive_ok(self.pk, a2, {}, self.toc, self.rd, CENSUS)(tex))
        self.assertIn("base package", RP.make_derive_ok(self.pk, self.audit, {LAW(tex): {}}, self.toc,
                                                        self.rd, CENSUS)(tex))
        few = {tex: self.pk[tex]}
        self.assertIn("no mod package imports it", RP.make_derive_ok(few, self.audit, {}, self.toc, self.rd,
                                                                     CENSUS)(tex))
        a3 = copy.deepcopy(self.audit)
        a3["materials"] = [m for m in a3["materials"] if m["package"] != "/Game/Leon/M_Leon_Body"]
        self.assertIn("not an audited mod material", RP.make_derive_ok(self.pk, a3, {}, self.toc, self.rd,
                                                                       CENSUS)(tex))
        count("derive gate: planted refusals", 4)

    def test_consistency_post_pass(self):
        base = "/Game/Leon/Textures/T_Leon_UpperBody_"
        full = [{"texture": base + "ORM", "op": "swap_rb", "channels": "swap_rb", "layout": "ps5"},
                {"texture": base + "D", "op": "keep", "channels": "keep", "layout": "ps5"},
                {"texture": base + "N", "op": "keep", "channels": "keep", "layout": "ps5"}]
        row = {"mod_package": "/Game/Leon/MI_Leon_UpperBody", "action": "reparent", "texture_derivations": full}
        RP.derived_consistency([row], self.pk)                           # every bound texture derived: ok
        missing = dict(row, texture_derivations=full[:2])
        with self.assertRaises(RP.PlanRefused):                         # a bound texture without a PS5 copy
            RP.derived_consistency([missing], self.pk)
        other = dict(row, texture_derivations=[dict(full[0], op="keep", channels="keep")] + full[1:])
        with self.assertRaises(RP.PlanRefused):                         # two rows, two operations, one copy
            RP.derived_consistency([row, other], self.pk)
        count("derive gate: consistency cases", 3)


class U08HairCheck(unittest.TestCase):
    def test_letters_and_stats(self):
        self.assertEqual(HMC.mask_letters("IDRO Mask"), "IDRO")
        self.assertEqual(HMC.mask_letters("/X/T_Hair_IDRO"), "IDRO")
        self.assertIsNone(HMC.mask_letters("Hair Diffuse + Alpha"))

        def mode6(r, g, b, a):   # constant-colour BC7 mode 6 block (7-bit endpoints + p-bit 0)
            v = 1 << 6
            pos = 7
            for ch in (r, g, b, a):
                for _e in range(2):
                    v |= (ch >> 1) << pos
                    pos += 7
            return v.to_bytes(16, "little")
        data = b"".join(mode6(200, 10, 254, 0 if k % 2 else 254) for k in range(64))
        st = HMC.channel_stats("PF_BC7", data, 16)
        self.assertEqual(st["R"]["constant"], 200)
        self.assertEqual(st["B"]["constant"], 254)
        self.assertIsNone(st["A"]["constant"])
        self.assertGreaterEqual(st["A"]["clear"], 0.4)
        self.assertGreaterEqual(st["A"]["solid"], 0.4)
        count("hair: statistics cases", 4)

    def test_real_hair_refused_with_reasons(self):
        out = os.path.join(HERE, "work", "hair-check.json")
        fp = os.path.join(HERE, "work", "hair-fetch.json")
        p = subprocess.run(["sh", os.path.join(TEAM, "run_hair_check.sh"),
                            os.path.join(TEAM, "evidence", "reparent-plan-r5-textures.json"), out, fp],
                           capture_output=True, text=True, timeout=600)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        res = json.load(open(out))
        self.assertEqual(res["rows_checked"], 1)
        row = res["rows"][0]
        self.assertEqual(row["verdict"], "REFUSED")
        self.assertEqual(row["parent"], "/Game/Townfall/Characters/Materials/M_Hair")
        miss = " | ".join(row["missing"])
        for k in ("NO alpha channel", "cannot tell opacity from occlusion", "AOAdjust", "not on the Mac"):
            self.assertIn(k, miss)
        self.assertEqual(json.load(open(fp))["summary"]["rows"], 8)
        count("hair: real verdict reasons", 4)


# ---------------------------------------------------------------------------
# U09 / U10: PS5 tile layout (ps5_tiling + texture_package.derive_ps5)
# ---------------------------------------------------------------------------
def public_equations():
    """Parse the public addrlib gfx10SwizzlePattern.h (copy in Team B's evidence) -> {(mode, eb): 'X0 Y0 ...'}."""
    import re as _re
    src = open(os.path.join(TEAMB, "evidence", "public-addrlib", "gfx10SwizzlePattern.h")).read()

    def table(name):
        body = src[src.index("const UINT_64 %s[]" % name):]
        body = body[body.index("{") + 1:]
        rows = []
        for line in body.splitlines()[1:]:
            line = line.strip()
            if line.startswith("};"):
                break
            if line.startswith("{"):
                rows.append([t.strip() for t in line[1:line.index("}")].split(",") if t.strip()])
        return rows
    n01, n2, n3 = table("GFX10_SW_PATTERN_NIBBLE01"), table("GFX10_SW_PATTERN_NIBBLE2"), table("GFX10_SW_PATTERN_NIBBLE3")
    out = {}
    for mode, name, nib in ((5, "GFX10_SW_4K_S_PATINFO", 2), (9, "GFX10_SW_64K_S_PATINFO", 3)):
        body = src[src.index("const ADDR_SW_PATINFO %s[]" % name):]
        for eb in (8, 16):
            m = _re.search(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*\}\s*,\s*// 1 pipes %d bpe" % eb, body)
            _mx, i01, i2, i3, _i4 = map(int, m.groups())
            toks = n01[i01] + n2[i2] + (n3[i3] if nib == 3 else [])
            out[(mode, eb)] = " ".join(t for t in toks if t != "0")
    return out


def block_means(fmt, data, w, h):
    bb = D.BLOCK[fmt]
    vals = []
    for o in range(0, len(data), bb):
        t = D.decode_block(fmt, data[o:o + bb])
        vals.append(sum(t[c] for c in range(64) if c % 4 != 3) / 48.0)
    return [vals[y * w:(y + 1) * w] for y in range(h)]


def corr(a, b):
    xs = [v for r in a for v in r]
    ys = [v for r in b for v in r]
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxy = sum((p - mx) * (q - my) for p, q in zip(xs, ys))
    sxx = sum((p - mx) ** 2 for p in xs)
    syy = sum((q - my) ** 2 for q in ys)
    return sxy / (sxx * syy) ** 0.5 if sxx and syy else 0.0


def down(g):
    return [[(g[2 * y][2 * x] + g[2 * y][2 * x + 1] + g[2 * y + 1][2 * x] + g[2 * y + 1][2 * x + 1]) / 4
             for x in range(len(g[0]) // 2)] for y in range(len(g) // 2)]


def game_texture(pid):
    ch = open(os.path.join(GAME_CHUNKS, pid + "00000001.bin"), "rb").read()
    bp = os.path.join(GAME_CHUNKS, pid + "00000002.bin")
    return ch, (open(bp, "rb").read() if os.path.isfile(bp) else None)


class U09Tiling(unittest.TestCase):
    def test_equations_equal_public_table(self):
        pub = public_equations()
        self.assertEqual(pub, PT.EQUATIONS)
        for (mode, eb) in PT.EQUATIONS:
            self.assertEqual(PT.tile_dims(mode, eb), PT.block_dims(mode, eb))
        count("tiling: equations equal the public addrlib table", len(pub))

    def test_tile_equals_oracle_and_round_trips(self):
        rng = random.Random(77)
        n = 0
        for fmt in ("PF_DXT1", "PF_BC5", "PF_DXT5"):
            eb = PT.ELEMENT_BYTES[fmt]
            for (w, h) in ((128, 128), (512, 512), (1024, 2048), (2048, 1024)):
                nm = max(w, h).bit_length()
                ch = choose(fmt, w, h, nm)
                lin = [bytes(rng.getrandbits(8) for _ in range(max(1, (w // 4) >> i) * max(1, (h // 4) >> i) * eb))
                       for i in range(nm)]
                mine = PT.tile(fmt, w, h, lin, ch)
                self.assertEqual(mine, ORACLE.tile_chain(lin, fmt, w, h, 0, ch["mode"]), (fmt, w, h))
                self.assertEqual(PT.detile(fmt, w, h, mine, ch), lin)
                self.assertEqual(ORACLE.detile_chain(mine, fmt, w, h, 0, ch["mode"]), lin)
                n += 1
        count("tiling: random chains byte-equal to the oracle + round trip (both de-tilers)", n)

    def test_game_pixels_read_coherent(self):
        """Own de-tiler on the game's own tiled textures: each mip is a half-size copy of the one above."""
        n = 0
        for pid in ("1250ffcc464b542e", "81896aa070f1307b", "86737b633647e773"):     # 3 DXT1 2048^2 (64KB)
            ch, bl = game_texture(pid)
            lay = TP.layout(ch, len(bl), allow_ps5=True)
            w, h = lay["size"]
            c = {"mode": 9, "element_bytes": 8, "chain": PT.chain_layout(9, 8, w // 4, h // 4, len(lay["mips"]))}
            lin = PT.detile("PF_DXT1", w, h, TP.payload(lay, ch, bl), c)
            for k in (2, 3, 4):     # mip 3 and 4 come from the tail block
                a = block_means("PF_DXT1", lin[k - 1], (w // 4) >> (k - 1), (h // 4) >> (k - 1))
                b = block_means("PF_DXT1", lin[k], (w // 4) >> k, (h // 4) >> k)
                self.assertGreater(corr(down(a), b), 0.9, (pid, k))
                n += 1
        count("tiling: game mip pairs coherent under the own de-tiler", n)

    def test_census_rule_and_leon_choices(self):
        rows, ev = PT.census_check(CENSUS)
        self.assertGreaterEqual(rows, 100)
        for k in ((8, 9), (8, 5), (16, 9), (16, 5)):
            self.assertTrue(any(key[:2] == k for key in ev), k)
        want = {("PF_DXT1", 2048, 2048): ("SW_64KB_S", 9), ("PF_BC5", 2048, 2048): ("SW_64KB_S", 8),
                ("PF_DXT1", 128, 128): ("SW_4KB_S", 7), ("PF_BC5", 128, 128): ("SW_4KB_S", 6),
                ("PF_DXT5", 1024, 2048): ("SW_64KB_S", 9), ("PF_DXT1", 512, 512): ("SW_64KB_S", 9)}
        for (f, w, h), (m, t) in want.items():
            ch = choose(f, w, h, max(w, h).bit_length())
            self.assertEqual((ch["mode_name"], ch["num_mips_in_tail"]), (m, t), (f, w, h))
        count("tiling: census rows agreeing with the rule", rows)

    def test_leon_textures_four_gates(self):
        n = 0
        for name, tr in ((ORM, S.swap_rb), (TEX[0], keep), (TEX[1], keep),
                         ("/Game/Leon/Textures/T_Leon_Lash_N", keep), ("/Game/Leon/Textures/T_Leon_Lash_ORM", S.swap_rb)):
            ch, bl = leon_texture(name)
            nc, nb, lay, _f, c, lin = TP.derive_ps5(ch, bl, tr, choose)
            pay = TP.payload(lay, nc, nb)
            rec = DS.tiling_gates(PT, PROBE, ORACLE, c, lay["format"], lay["size"], lin, pay,
                                  nc[lay["export_offset"]:lay["export_offset"] + lay["export_size"]],
                                  FZEN.ZenPackage(nc).names)
            self.assertTrue(rec["gate4_probe_verdict"])
            n += 1
        count("tiling: Leon textures through the four gates", n)


class U10TilingFaults(unittest.TestCase):
    def setUp(self):
        ch, bl = leon_texture("/Game/Leon/Textures/T_Leon_Eye_D")
        self.nc, self.nb, self.lay, _f, self.choice, self.lin = TP.derive_ps5(ch, bl, keep, choose)
        self.pay = TP.payload(self.lay, self.nc, self.nb)
        self.exp = self.nc[self.lay["export_offset"]:self.lay["export_offset"] + self.lay["export_size"]]
        self.names = FZEN.ZenPackage(self.nc).names

    def gates(self, pay=None, choice=None, exp=None):
        return DS.tiling_gates(PT, PROBE, ORACLE, choice or self.choice, "PF_DXT1", self.lay["size"], self.lin,
                               pay or self.pay, exp or self.exp, self.names)

    def test_wrong_mode(self):
        w, h = self.lay["size"]
        wrong = dict(self.choice, mode=5, mode_name="SW_4KB_S",
                     chain=PT.chain_layout(5, 8, w // 4, h // 4, len(self.lin)))
        wrong["num_mips_in_tail"] = len(self.lin) - wrong["chain"]["first_tail"]
        with self.assertRaises(DS.Refused):                    # data tiled as 4KB, gates expect 64KB
            self.gates(pay=PT.tile("PF_DXT1", w, h, self.lin, wrong))
        with self.assertRaises(DS.Refused):                    # gates told 4KB, data/header are 64KB
            self.gates(choice=wrong)
        bad = bytearray(self.nc)                               # header claims ExtData 5 for 64KB data
        s = self.lay["export_offset"] + self.lay["pf_offset"] + self.lay["pf_len"]
        struct.pack_into("<I", bad, s, 5)
        with self.assertRaises(TP.LayoutError):
            TP.layout(bytes(bad), len(self.nb), allow_ps5=True)
        count("tiling faults: wrong mode refused", 3)

    def test_wrong_tail(self):
        bad = bytearray(self.nc)
        s = self.lay["export_offset"] + self.lay["pf_offset"] + self.lay["pf_len"]
        struct.pack_into("<I", bad, s + 4, self.choice["num_mips_in_tail"] - 1)
        with self.assertRaises(TP.LayoutError):
            TP.layout(bytes(bad), len(self.nb), allow_ps5=True)
        f = self.choice["chain"]["first_tail"]
        moved = copy.deepcopy(self.choice)
        moved["chain"]["tail"][f + 1] = (moved["chain"]["tail"][f + 1][0] + 8, moved["chain"]["tail"][f + 1][1])
        w, h = self.lay["size"]
        with self.assertRaises(DS.Refused):                   # tail mip placed off the public origin
            self.gates(pay=PT.tile("PF_DXT1", w, h, self.lin, moved))
        count("tiling faults: wrong tail refused", 2)

    def test_detile_mismatch(self):
        pay = list(self.pay)
        b = bytearray(pay[0])
        b[12345] ^= 0x10
        pay[0] = bytes(b)
        with self.assertRaises(DS.Refused):                   # a moved / changed block
            self.gates(pay=pay)
        f = self.choice["chain"]["first_tail"]
        w, h = self.lay["size"]
        ones = PT.tile("PF_DXT1", w, h, [b"\xff" * len(x) for x in self.lin], self.choice)
        unused = ones[f].index(0)                              # a tail-block byte no mip uses
        pay = list(self.pay)
        t = bytearray(pay[f])
        t[unused] ^= 0xFF                                     # de-tile still exact, but the oracle
        pay[f] = bytes(t)                                     # (zero padding) disagrees
        self.assertEqual(PT.detile("PF_DXT1", self.lay["size"][0], self.lay["size"][1], pay, self.choice), self.lin)
        with self.assertRaises(DS.Refused):
            self.gates(pay=pay)
        count("tiling faults: de-tile mismatch refused", 2)

    def test_census_faults(self):
        bad = copy.deepcopy(CENSUS)
        bad["rows"][0]["num_mips_in_tail"] += 1
        with self.assertRaises(PT.TilingRefused):
            PT.choose("PF_DXT1", 512, 512, 10, bad)
        few = dict(CENSUS, rows=CENSUS["rows"][:10])
        with self.assertRaises(PT.TilingRefused):
            PT.choose("PF_DXT1", 512, 512, 10, few)
        nopix = copy.deepcopy(CENSUS)
        for r in nopix["rows"]:
            if r["element_bytes"] == 16 and r["ext_data"] == 5:
                r["pixel_verdicts"] = []
        with self.assertRaises(PT.TilingRefused):
            PT.choose("PF_BC5", 128, 128, 8, nopix)
        PT.choose("PF_BC5", 512, 512, 10, nopix)             # other modes unaffected
        with self.assertRaises(PT.TilingRefused):
            PT.choose("PF_DXT1", 512, 512, 9, CENSUS)          # not a full chain
        with self.assertRaises(PT.TilingRefused):
            PT.choose("PF_BC4", 512, 384, 10, CENSUS)          # not power of two
        with self.assertRaises(PT.TilingRefused):              # fit class the game data does not show
            PT.choose("PF_DXT1", 256, 256, 9, CENSUS)
        unver = copy.deepcopy(CENSUS)
        for r in unver["rows"]:
            if r["element_bytes"] == 8 and r["ext_data"] == 9:
                r["verified"] = False
        with self.assertRaises(PT.TilingRefused):              # unverified rows cannot settle a mode
            PT.choose("PF_DXT1", 2048, 2048, 12, unver)
        contra = copy.deepcopy(CENSUS)
        contra["rows"][3]["pixel_verdicts"] = ["SW_64KB_S (CONTRADICTS the header, which claims LINEAR)"]
        with self.assertRaises(PT.TilingRefused):              # any contradicting game texture fails the census
            PT.choose("PF_DXT1", 2048, 2048, 12, contra)
        two = copy.deepcopy(CENSUS)
        for r in two["rows"]:
            if r["element_bytes"] == 8 and r["ext_data"] == 9:
                r["pixel_verdicts"] = r["pixel_verdicts"] + ["undecided"]
        with self.assertRaises(PT.TilingRefused):              # two different verdicts do not count
            PT.choose("PF_DXT1", 2048, 2048, 12, two)
        count("tiling faults: census / shape refused", 9)


# ---------------------------------------------------------------------------
# X-series: paktx with a derived texture (synthetic game, real bytes)
# ---------------------------------------------------------------------------
def rel_of(name, ext=".uasset"):
    return name.replace("/Game/", "Townfall/Content/", 1).lstrip("/") + ext


class TexGame:
    def __init__(self, name, *, drawing_importer=False, base_has_texture=False, plan_texture=ORM,
                 plan_format="PF_DXT1", rename=True):
        self.root = os.path.join(WORK, name)
        shutil.rmtree(self.root, ignore_errors=True)
        self.paks = os.path.join(self.root, "console", T.PAKS_REMOTE.lstrip("/"))
        self.mirror = os.path.join(self.root, "mirror")
        self.headers = os.path.join(self.root, "headers")
        self.fetched = os.path.join(self.root, "fetched")
        self.mod = os.path.join(self.root, "mod")
        self.out = os.path.join(self.root, "work")
        for d in (self.paks, self.mirror, self.headers, self.fetched, self.mod):
            os.makedirs(d)
        pdata = open(PARENT_HDR, "rb").read()
        ppkg = FZEN.ZenPackage(pdata)
        self.to_pid = LAW(PARENT)
        pimps = [LAW(n) for n in ppkg.imported_packages]
        pk0 = {self.to_pid: pimps}
        pk0.update({p: [] for p in pimps + list(range(0x1000, 0x1004))})
        if base_has_texture:
            pk0[LAW(ORM)] = []
        self._base("pakchunk0-ps5", 0xA0A0A0A000000001, pk0, {self.to_pid: pdata})
        self._base("pakchunk10-ps5", 0xB0B0B0B000000002, {0x2000: [], 0x2001: []}, {})
        self.sizes = {}
        for n in os.listdir(self.paks):
            p = os.path.join(self.paks, n)
            if n.endswith(".ucas"):
                self.sizes[n] = os.path.getsize(p)
            else:
                shutil.copyfile(p, os.path.join(self.mirror, n))
        with open(os.path.join(self.fetched, "%016x.hdr.bin" % self.to_pid), "wb") as f:
            f.write(pdata)
        self._mod(drawing_importer)
        self._plan(plan_texture, plan_format, rename)

    def _base(self, stem, cv, pkgs, real):
        c_id = struct.pack("<Q", cv)
        hb = B.serialize_container_header(B.ContainerHeader(
            cv, {p: B.StoreEntry(tuple(i), ()) for p, i in pkgs.items()}, False, b""))
        chunks = [(c_id + bytes.fromhex("00000006"), hb)]
        files = []
        for p in sorted(pkgs):
            chunks.append((T.cid(p), real.get(p) or T.prng(p, 60000 + p % 5000)))
            nm = rel_of(PARENT) if p in real else "Townfall/Content/%s/P%016x.uasset" % (stem, p)
            files.append((nm, T.cid(p)))
        u, c = T.container(c_id, chunks, files)
        T.write(os.path.join(self.paks, stem + ".utoc"), u)
        T.write(os.path.join(self.paks, stem + ".ucas"), c)
        T.write(os.path.join(self.headers, stem + ".container-header.bin"), hb)
        T.write(os.path.join(self.root, "stores", stem + ".container-header.bin"), hb)

    def _mod(self, drawing_importer):
        lv = leon()
        c_id = struct.pack("<Q", 0xE0E0E0E000000005)
        pkgs, chunks, files = {}, [], []
        for name in [MI, OLD] + TEX:
            pid, kinds, imps = lv[name]
            if name == OLD:
                imps = [i for i in imps if i in {lv[t][0] for t in TEX}]
            pkgs[pid] = imps
            for k, data in sorted(kinds.items()):
                chunks.append((T.cid(pid, kind=k), data))
                files.append((rel_of(name, ".uasset" if k == 1 else ".ubulk"), T.cid(pid, kind=k)))
        for rp_ in (0x1001, 0x1002):
            pkgs[rp_] = []
            chunks.append((T.cid(rp_), T.prng(0x9000 + rp_, 4000)))
            files.append(("Townfall/Content/pakchunk0-ps5/P%016x.uasset" % rp_, T.cid(rp_)))
        shm = {LAW(OLD): (bytes(range(20)),)} if drawing_importer else {}
        if drawing_importer:  # make the drawing importer import the derived texture
            pkgs[LAW(OLD)] = sorted(set(pkgs[LAW(OLD)]) | {LAW(ORM)})
        hb = B.serialize_container_header(B.ContainerHeader(
            0xE0E0E0E000000005, {p: B.StoreEntry(tuple(i), shm.get(p, ())) for p, i in pkgs.items()}, False, b""))
        chunks.insert(0, (c_id + bytes.fromhex("00000006"), hb))
        u, c = T.container(c_id, chunks, files)
        self.mod_utoc = os.path.join(self.mod, "zzz_Leon_Test_P.utoc")
        self.mod_ucas = os.path.join(self.mod, "zzz_Leon_Test_P.ucas")
        T.write(self.mod_utoc, u)
        T.write(self.mod_ucas, c)

    def _plan(self, tex, fmt, rename):
        pid = LAW(MI)
        self.reparent_plan = os.path.join(self.root, "reparent-plan.json")
        ren = {"BaseColor": "BaseColour"}
        if rename:
            ren["ORM"] = "MRO"
        row = {"mod_package": MI, "action": "reparent", "old_parent": OLD, "new_parent": PARENT,
               "new_export": PARENT.rsplit("/", 1)[1], "param_renames": ren,
               "mod_class": {"unproven_opaque": True},
               "texture_derivations": self._ders(tex, fmt),
               "look_notes": ["test"],
               "store_entry_repoint": {"package_id": "%016x" % pid, "from_package_id": "%016x" % LAW(OLD),
                                       "to_package_id": "%016x" % self.to_pid}}
        with open(self.reparent_plan, "w") as f:
            json.dump({"label": "Mac-only", "rows": [row, {"mod_package": OLD, "action": "known-checker"}]}, f)

    @staticmethod
    def _ders(tex, fmt):
        """keep for every bound texture, the R<->B swap for `tex` (ORM normally; a planted fault may name
        another texture and format)."""
        bound = {"BaseColor": (TEX[1], "PF_DXT1", "BaseColour"), "Normal": (TEX[0], "PF_BC5", "Normal"),
                 "ORM": (ORM, "PF_DXT1", "MRO")}
        out = []
        for prm, (t, f, slot) in bound.items():
            if t == tex:
                out.append({"texture": t, "op": "swap_rb", "channels": "swap_rb", "layout": "ps5",
                            "from_letters": "ORM", "to_letters": "MRO", "format": fmt, "parameter": prm,
                            "slot": slot})
            else:
                out.append({"texture": t, "op": "keep", "channels": "keep", "layout": "ps5", "format": f,
                            "parameter": prm, "slot": slot})
        return out

    def record(self):
        rec = paktx.make_record(B, self.mirror, "TEST00012", "01.000.000", T.PAKS_REMOTE, self.sizes, self.headers)
        path = os.path.join(self.root, "record.json")
        paktx.write_json(path, rec)
        return path

    def generate(self, decisions=("--accept-unproven-opaque",), out="manifest"):
        o = os.path.join(self.root, out)
        cmd = [sys.executable, "-I", GEN, "--plan", self.reparent_plan, "--mod-utoc", self.mod_utoc,
               "--mod-ucas", self.mod_ucas, "--mirror", self.mirror, "--headers", self.fetched,
               "--base-store"] + [os.path.join(self.root, "stores", s + ".container-header.bin")
                                  for s in ("pakchunk0-ps5", "pakchunk10-ps5")] + \
              ["--material-fallback", TOOLS, "--tiling-reference", TILEREF, "--out", o] + list(decisions)
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
        return p.returncode, p.stdout + p.stderr, os.path.join(o, "manifest.json")


def run_cli(*argv):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        code = paktx.main(["--builder", T.BUILDER, *argv])
    return code, json.loads(out.getvalue()), err.getvalue()


def plan_sub(g, rp, man, *extra, decide=True):
    return run_cli("install", "--record", rp, "--mirror", g.mirror, "--mod", g.mod, "--work", g.out,
                   "--substitute", man, "--readers", TOOLS, "--tiling-reference", TILEREF,
                   *(DECIDE if decide else []),
                   "--reparent-plan", g.reparent_plan, *extra)


def reseal(man_path, edit):
    doc = json.load(open(man_path))
    edit(doc)
    doc["manifest_sha256"] = paktx.manifest_self_sha(doc)
    with open(man_path, "w") as f:
        json.dump(doc, f, indent=1, sort_keys=True)


class XBase(unittest.TestCase):
    game_kw = {}

    def setUp(self):
        self.g = TexGame(self.id().rsplit(".", 1)[1][:40], **self.game_kw)
        self.rp = self.g.record()
        code, out, self.man = self.g.generate()
        self.assertEqual(code, 0, out)
        self.console = os.path.join(self.g.root, "console")
        self.before = T.snapshot(self.console)

    def refused(self, want, *extra, man=None):
        code, res, err = plan_sub(self.g, self.rp, man or self.man, "--dry-run", *extra)
        self.assertEqual(code, 2, (res, err[-800:]))
        self.assertEqual(res["code"], want, res)
        self.assertIs(res["console_changed"], False)
        self.assertEqual(T.snapshot(self.console), self.before)
        count("paktx: planted fault refused (console unchanged)", 1)
        return res

    def tex_row(self, ch="swap_rb"):
        return [t for t in json.load(open(self.man))["textures"] if t["channels"] == ch][0]

    def tex_index(self, ch="swap_rb"):
        return [i for i, t in enumerate(json.load(open(self.man))["textures"]) if t["channels"] == ch][0]


class X01Positive(XBase):
    def test_install_reread_restore(self):
        man = json.load(open(self.man))
        self.assertEqual(man["schema"], "paktx-substitute/2")
        self.assertEqual(sorted(x["package"] for x in man["textures"]), sorted(TEX))
        t = [x for x in man["textures"] if x["channels"] == "swap_rb"][0]
        self.assertEqual((t["package"], t["format"], t["from_letters"], t["to_letters"]),
                         (ORM, "PF_DXT1", "ORM", "MRO"))
        self.assertTrue(t["proof"]["python"]["exact"] and t["proof"]["gpu"]["exact"])
        for x in man["textures"]:
            self.assertEqual((x["layout"]["form"], x["layout"]["mode"]), ("ps5", "SW_64KB_S"))
            self.assertIn("pixels agree", x["proof"]["tiling"]["gate4_probe_verdict"])
        self.assertEqual(man["rows"][0]["reparent"]["renamed_parameters"], [["BaseColor", "BaseColour"],
                                                                           ["ORM", "MRO"]])
        mod_before = T.snapshot(self.g.mod)
        code, res, err = plan_sub(self.g, self.rp, self.man, "--dry-run")
        self.assertEqual(code, 0, (res, err[-2000:]))
        plan = json.load(open(res["plan"]))
        sub = plan["substitution"]
        self.assertEqual(len(sub["textures"]), 3)
        self.assertEqual(sub["post_build"][0]["substituted_chunks_verified"], 7)
        self.assertIn("DERIVED TEXTURE", err)
        sha = res["plan_sha256"]
        code, res, err = plan_sub(self.g, self.rp, self.man, "--local", self.g.paks, "--confirm", sha[:12],
                                  "--game-closed")
        self.assertEqual(code, 0, (res, err[-1500:]))
        code, v, _ = run_cli("verify", "--plan", res["plan"], "--local", self.g.paks)
        self.assertEqual(v["state"]["verdict"], "INSTALLED")
        # independent re-read of the INSTALLED files (second reader set) + re-proof from shipped bytes
        toc = FUTOC.load(os.path.join(self.g.paks, "pakchunk0-ps5.utoc"))
        side = open(os.path.join(self.g.paks, "pakchunk0-ps5_s1.ucas"), "rb").read()
        base = toc.partition_size

        def rd(o, n):
            return side[o - base:o - base + n]
        got = {}
        for c in t["chunks"]:
            row = toc.chunk_ids.index(bytes.fromhex(c["chunk_id"]))
            data = toc.read_chunk(row, rd)
            self.assertEqual(FB3.iohash20(data), toc.metas[row][0])
            self.assertEqual(data, open(os.path.join(os.path.dirname(self.man), c["derived"]["path"]), "rb").read())
            got[c["kind"]] = data
        oc, ob = leon_texture(ORM)
        olay = TP.layout(oc, len(ob))
        lay = TP.layout(got["export"], len(got["bulk"]), allow_ps5=True)
        self.assertEqual((lay["ps5"]["ext_data"], lay["ps5"]["num_mips_in_tail"]), (9, 9))
        w, h = lay["size"]
        lin = ORACLE.detile_chain(TP.payload(lay, got["export"], got["bulk"]), "PF_DXT1", w, h, 0, 9)
        self.assertTrue(D.swap_proof("PF_DXT1", b"".join(TP.payload(olay, oc, ob)), b"".join(lin))["exact"])
        hrow = toc.chunk_ids.index(toc.container_id.to_bytes(8, "little") + bytes.fromhex("00000006"))
        _c, st = FCH.parse(toc.read_chunk(hrow, rd))
        self.assertEqual(list(st[LAW(ORM)]["imports"]), leon()[ORM][2])
        self.assertEqual(st[LAW(ORM)]["shader_maps"], ())
        mi = FZEN.ZenPackage(toc.read_chunk(toc.chunk_ids.index(T.cid(LAW(MI))), rd))
        self.assertIn("MRO", mi.names)
        self.assertNotIn("ORM", mi.names)
        count("paktx: installed derived texture chunks re-read + re-proven", len(got))
        code, r2, err2 = run_cli("restore", "--plan", res["plan"], "--local", self.g.paks, "--confirm",
                                 sha[:12], "--game-closed", "--cleanup", "--allow-delete")
        self.assertEqual(code, 0, (r2, err2))
        self.assertEqual(T.snapshot(self.console), self.before)
        self.assertEqual(T.snapshot(self.g.mod), mod_before)
        count("paktx: byte-exact restore", 1)

    def test_deterministic_manifest(self):
        code, out, man2 = self.g.generate(out="manifest-again")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.load(open(man2))["manifest_sha256"], json.load(open(self.man))["manifest_sha256"])
        count("paktx: manifest determinism", 1)


class X02TamperedChunks(XBase):
    def test_derived_bulk_changed_without_reseal(self):
        c = [x for x in self.tex_row()["chunks"] if x["kind"] == "bulk"][0]
        p = os.path.join(os.path.dirname(self.man), c["derived"]["path"])
        d = bytearray(open(p, "rb").read())
        d[100] ^= 0x40
        open(p, "wb").write(bytes(d))
        self.refused("substitute_derived_sha_mismatch")

    def test_derived_bytes_resealed_are_not_reproduced(self):
        c = [x for x in self.tex_row()["chunks"] if x["kind"] == "bulk"][0]
        p = os.path.join(os.path.dirname(self.man), c["derived"]["path"])
        d = bytearray(open(p, "rb").read())
        d[100] ^= 0x40
        d = bytes(d)
        open(p, "wb").write(d)
        k = self.tex_index()

        def edit(doc):
            for x in doc["textures"][k]["chunks"]:
                if x["kind"] == "bulk":
                    x["derived"].update(sha256=hashlib.sha256(d).hexdigest(), iohash=FB3.iohash20(d))
        reseal(self.man, edit)
        self.refused("substitute_texture_bytes_not_reproduced")

    def test_iohash_claim_wrong(self):
        k = self.tex_index()
        reseal(self.man, lambda doc: doc["textures"][k]["chunks"][0]["derived"].__setitem__("iohash", "0" * 40))
        self.refused("substitute_derived_iohash_mismatch")

    def test_header_changed(self):
        c = [x for x in self.tex_row()["chunks"] if x["kind"] == "export"][0]
        p = os.path.join(os.path.dirname(self.man), c["derived"]["path"])
        d = bytearray(open(p, "rb").read())
        xoff = struct.unpack_from("<i", d, 32)[0]
        d[xoff + 56] ^= 1                    # the export's public hash in the export map (not allowed to change)
        d = bytes(d)
        open(p, "wb").write(d)
        k = self.tex_index()

        def edit(doc):
            for x in doc["textures"][k]["chunks"]:
                if x["kind"] == "export":
                    x["derived"].update(sha256=hashlib.sha256(d).hexdigest(), iohash=FB3.iohash20(d))
        reseal(self.man, edit)
        self.refused("substitute_texture_header_changed")

    def test_gpu_proof_claim_false(self):
        k = self.tex_index()
        reseal(self.man, lambda doc: doc["textures"][k]["proof"]["gpu"].__setitem__("exact", False))
        self.refused("substitute_texture_proof_failed")

    def test_original_claim_wrong(self):
        reseal(self.man, lambda doc: doc["textures"][0]["chunks"][1]["original"].__setitem__("sha256", "0" * 64))
        self.refused("substitute_original_mismatch")

    def test_writer_not_pinned(self):
        reseal(self.man, lambda doc: doc["texture_tools_sha256"].__setitem__("bcn_swap.py", "0" * 64))
        self.refused("substitute_texture_writer_not_pinned")


class X03PlanBinding(XBase):
    def test_texture_dropped_from_manifest(self):
        reseal(self.man, lambda doc: doc.__setitem__("textures", []))
        self.refused("substitute_manifest_has_zero_textures")

    def test_texture_disagrees_with_plan(self):
        k = self.tex_index()
        reseal(self.man, lambda doc: doc["textures"][k].__setitem__("to_letters", "RMO"))
        self.refused("substitute_textures_disagree_with_plan")

    def test_old_schema_with_texture_plan(self):
        def edit(doc):
            doc["schema"] = "paktx-substitute/1"
            del doc["textures"]
            del doc["texture_tools_sha256"]
        reseal(self.man, edit)
        self.refused("substitute_plan_needs_textures")

    def test_consumer_rename_missing(self):
        reseal(self.man, lambda doc: doc["rows"][0]["reparent"].__setitem__(
            "renamed_parameters", [["BaseColor", "BaseColour"]]))
        self.refused("substitute_row_disagrees_with_plan")


class X05InstancePointsAtDerivedOnly(XBase):
    def test_paktx_refuses_instance_with_an_underived_mod_texture(self):
        doc = json.load(open(self.g.reparent_plan))
        doc["rows"][0]["texture_derivations"] = [d for d in doc["rows"][0]["texture_derivations"]
                                                 if d["texture"] != TEX[1]]
        json.dump(doc, open(self.g.reparent_plan, "w"))
        psha = paktx.sha256_file(self.g.reparent_plan)

        def edit(m):
            m["source_plan"]["sha256"] = psha
            m["textures"] = [t for t in m["textures"] if t["package"] != TEX[1]]
        reseal(self.man, edit)
        self.refused("substitute_instance_imports_underived_texture")

    def test_generator_refuses_plan_without_derivations(self):
        doc = json.load(open(self.g.reparent_plan))
        doc["rows"][0].pop("texture_derivations")
        json.dump(doc, open(self.g.reparent_plan, "w"))
        code, out, _ = self.g.generate(out="m-noder")
        self.assertEqual(code, 1, out)
        self.assertIn("has no derived PS5 copy in the plan", out)
        count("generator: planted fault refused", 1)


class X04GeneratorRefusals(unittest.TestCase):
    def test_unsupported_format_and_rename_and_base_and_drawing_importer(self):
        cases = [("fmt", dict(plan_texture=TEX[0], plan_format="PF_BC5"), "not derivable"),
                 ("base", dict(base_has_texture=True), "id of a base package"),
                 ("draw", dict(drawing_importer=True), "can draw"),
                 ("norename", dict(rename=False), "does not rename ORM to MRO")]
        for name, kw, want in cases:
            g = TexGame("x04-" + name, **kw)
            code, out, _ = g.generate()
            self.assertEqual(code, 1, out)
            self.assertIn(want, out, name)
            count("generator: planted fault refused", 1)

    def test_drawing_importer_refused_by_paktx_too(self):
        clean = TexGame("x04-clean")
        rp = clean.record()
        code, out, man = clean.generate()
        self.assertEqual(code, 0, out)
        bad = TexGame("x04-drawmod", drawing_importer=True)
        # the clean manifest against the mod whose importer draws: identity re-bound, everything else equal
        md = os.path.join(bad.root, "manifest")
        shutil.copytree(os.path.dirname(man), md)
        mp = os.path.join(md, "manifest.json")
        reseal(mp, lambda doc: doc["mod"].update(utoc_sha256=paktx.sha256_file(bad.mod_utoc),
                                                ucas_sha256=paktx.sha256_file(bad.mod_ucas)))
        rpb = bad.record()
        code, res, err = plan_sub(bad, rpb, mp, "--dry-run")
        self.assertEqual(code, 2, (res, err[-800:]))
        self.assertEqual(res["code"], "substitute_texture_importer_can_draw", res)
        count("paktx: planted fault refused (console unchanged)", 1)


if __name__ == "__main__":
    os.makedirs(WORK, exist_ok=True)
    for name, p in (("BUILDER", T.BUILDER), ("LEON", LEON + ".ucas"), ("PARENT_HDR", PARENT_HDR),
                    ("TILEREF", os.path.join(TILEREF, "tile_reference.py")), ("GAME_CHUNKS", GAME_CHUNKS)):
        if not os.path.exists(p):
            print("MISSING required input %s=%s" % (name, p))
            sys.exit(1)
    compile_gpu()
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    print("cap12 derived-texture host tests: %d test cases" % suite.countTestCases())
    res = unittest.TextTestRunner(verbosity=2).run(suite)
    ran, bad, skipped = res.testsRun, len(res.failures) + len(res.errors), len(res.skipped)
    for k in sorted(GATES):
        print("  [gate] %s: checked %d" % (k, GATES[k]))
    want = ["swap: blocks proven by the Python decoder", "swap: texels proven by the GPU decoder",
            "decoders: BC7 texels identical (all 8 modes, all partitions)",
            "faults: decode mismatch caught (both decoders)", "faults: wrong channel order caught (both decoders)",
            "faults: unsupported format refused", "layout: real Leon textures proven (bulk map + mip table)",
            "layout: planted corruption refused", "defaults: parent slots cross-checked against the game's bindings",
            "planner: planted fault -> ORM lost", "derive gate: planted refusals", "hair: real verdict reasons",
            "paktx: planted fault refused (console unchanged)", "paktx: installed derived texture chunks re-read + "
            "re-proven", "paktx: byte-exact restore", "generator: planted fault refused",
            "tiling: equations equal the public addrlib table",
            "tiling: random chains byte-equal to the oracle + round trip (both de-tilers)",
            "tiling: game mip pairs coherent under the own de-tiler", "tiling: census rows agreeing with the rule",
            "tiling: Leon textures through the four gates", "tiling faults: wrong mode refused",
            "tiling faults: wrong tail refused", "tiling faults: de-tile mismatch refused",
            "tiling faults: census / shape refused"]
    zero = [k for k in want if not GATES.get(k)]
    print("SUMMARY tests_run=%d failed=%d skipped=%d passed=%d" % (ran, bad, skipped, ran - bad - skipped))
    if ran == 0 or bad or skipped or ran != suite.countTestCases() or zero:
        print("GATE FAIL (zero tests, failures, skips, count mismatch, or zero-count gates %s)" % zero)
        sys.exit(1)
    print("GATE PASS")
