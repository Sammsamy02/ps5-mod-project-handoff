"""Unit and validation tests for the per-game profile generator (tools/profilegen).

Two kinds of test:

* synthetic: tiny ELF images assembled in memory (tests/fixtures/profilegen/synth.py) that contain
  just the instruction shapes a detector looks for, with arbitrary numbers that are NOT any game's.
  They prove the detectors follow the code rather than a remembered answer.
* Townfall validation: runs the real generator on the decrypted executable and compares with the
  oracle.  Paths come from MODBRIDGE_TOWNFALL_EBOOT / MODBRIDGE_TOWNFALL_PARAM and the test skips
  cleanly when the files are absent.
"""
import hashlib
import io
import json
import os
import re
import struct
import sys
import tempfile
import time
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT))
sys.path.insert(0, str(PROJECT / "tests" / "fixtures" / "profilegen"))

import synth  # noqa: E402
from synth import (R8, R9, R10, R11, R12, R13, R14, R15, RAX, RBP, RBX, RCX, RDI, RDX, RSI, RSP, Asm, Seg,  # noqa: E402
                   ascii0, build_elf, gnu_note, utf16)
from synth_layout import LAsm  # noqa: E402

from tools.profilegen import cli, emit_ue4ss, profile, validate, vtables  # noqa: E402
from tools.profilegen.detectors import (_book, _flow, build_axes, class_default_object, class_registration,  # noqa: E402
                                        engine_tick, engine_version, field_classes, fname_ctor, fname_pool,
                                        fname_tostring, gmalloc, gnatives, layout_merge, platform, process_event,
                                        process_event_members, property_subtypes, static_construct,
                                        static_find_object, uobject_array, uobject_base)
from tools.profilegen.elf import ElfError, ElfImage  # noqa: E402
from tools.profilegen.index import (KIND_LEA, KIND_LOAD, KIND_STORE, CallIndex, Decoder, FunctionMap,  # noqa: E402
                                    ImageIndex, RipXrefIndex, StringTable)
from tools.profilegen.model import Context, Envelope, Hex, weakest  # noqa: E402

HAVE_CAPSTONE = Decoder.available
needs_capstone = unittest.skipUnless(HAVE_CAPSTONE, "capstone is not installed")

TOWNFALL_EBOOT = os.environ.get(
    "MODBRIDGE_TOWNFALL_EBOOT",
    "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/"
    "townfall-static-executable-2026-10-02/decrypted-eboot.bin")
TOWNFALL_PARAM = os.environ.get(
    "MODBRIDGE_TOWNFALL_PARAM",
    "/WORKSPACE/ps5-claude/outputs/2026-10-04-zcode-team/worker-game-version/param.json")
FIXTURES = PROJECT / "tests" / "fixtures" / "profilegen"

CODE_VA, RO_VA, RW_VA = 0x1000, 0x6000, 0x9000
RW_FILESZ, RW_MEMSZ = 0x3000, 0x8000


# ---------------------------------------------------------------------------------------------
class Img:
    """Builder for one synthetic image: code (R-X), read-only strings (R), writable data + BSS (RW)."""

    def __init__(self) -> None:
        self.a = Asm(CODE_VA)
        self.a.int3(1)
        self.ro = bytearray(16)                 # leading NULs: every string is NUL-preceded
        self._bss = RW_VA + RW_FILESZ + 0x100
        self.rw = bytearray(RW_FILESZ)
        self.relas = []

    # data ---------------------------------------------------------------------------------
    def ro_str(self, text: str, wide: bool = False) -> int:
        va = RO_VA + len(self.ro)
        self.ro += utf16(text) if wide else ascii0(text)
        while len(self.ro) % 2:
            self.ro.append(0)
        return va

    def bss(self, size: int = 8, align: int = 16) -> int:
        self._bss = (self._bss + align - 1) & ~(align - 1)
        va = self._bss
        self._bss += size
        return va

    # code ---------------------------------------------------------------------------------
    def func(self, name: str) -> Asm:
        self.a.int3(1)
        self.a.align(16)
        self.a.label(name)
        self.a.prologue()
        return self.a

    def image(self, **kw) -> bytes:
        code = self.a.finish()
        assert len(code) < 0x4000, "code does not fit the synthetic code segment"
        segs = [Seg(CODE_VA, code, 5), Seg(RO_VA, bytes(self.ro), 4),
                Seg(RW_VA, bytes(self.rw), 6, memsz=RW_MEMSZ)]
        relas = kw.pop("relas", None)
        if relas is None and self.relas:
            relas = self.relas
        return build_elf(segs, relas=relas, **kw)


def make_ctx(image: bytes, param: dict = None, options: dict = None) -> Context:
    elf = ElfImage(image)
    return Context(elf, ImageIndex(elf), param, options)


# ---------------------------------------------------------------------------------------------
class ElfLoaderTests(unittest.TestCase):
    def setUp(self):
        im = Img()
        a = im.func("f")
        a.xor_r32(RAX).epilogue()
        im.ro_str("hello")
        self.gnu = bytes(range(16)) + b"\x00\x00\x00\x00"
        self.data = im.image(notes=[gnu_note(self.gnu)], relas=[(RW_VA + 8, CODE_VA), (RW_VA, RO_VA + 16)],
                             procparam=struct.pack("<Q", 0x60) + b"ORBI" + struct.pack("<IQ", 5, 0x1100004012590001))
        self.elf = ElfImage(self.data)

    def test_vaddr_is_never_the_file_offset(self):
        e = self.elf
        self.assertEqual(e.va_to_off(CODE_VA), CODE_VA + 0x4000)
        self.assertEqual(e.va_to_off(RO_VA + 3), RO_VA + 3 + 0x4000)
        self.assertEqual(e.off_to_va(CODE_VA + 0x4000), CODE_VA)
        # the bytes at file offset == vaddr are something else entirely
        self.assertNotEqual(self.data[CODE_VA:CODE_VA + 8], e.read(CODE_VA, 8))
        self.assertEqual(e.read(CODE_VA, 8), self.data[CODE_VA + 0x4000:CODE_VA + 0x4000 + 8])

    def test_per_segment_delta_including_a_negative_one(self):
        e = self.elf
        deltas = sorted({s.delta for s in e.loads})
        self.assertEqual(deltas[0], 0x20000 - 0x40000)       # the dynlib-like segment sits before its vaddr
        self.assertIn(0x4000, deltas)
        seg = next(s for s in e.loads if s.delta < 0)
        self.assertEqual(e.va_to_off(seg.vaddr), seg.offset)

    def test_bss_is_mapped_but_not_file_backed(self):
        e = self.elf
        bss = RW_VA + RW_FILESZ + 0x10
        self.assertTrue(e.is_mapped(bss))
        self.assertTrue(e.is_bss(bss))
        self.assertIsNone(e.va_to_off(bss))
        self.assertIsNone(e.read(bss, 4))
        self.assertTrue(e.is_writable(bss))
        self.assertFalse(e.is_exec(bss))
        self.assertTrue(e.is_exec(CODE_VA))
        self.assertFalse(e.is_mapped(0x7000000))

    def test_reads_do_not_cross_segments(self):
        e = self.elf
        end = CODE_VA + len(e.read_upto(CODE_VA, 1 << 20))
        self.assertIsNone(e.read(end - 2, 8))
        self.assertEqual(len(e.read_upto(end - 2, 8)), 2)
        self.assertEqual(e.read_upto(0x7000000, 4), b"")

    def test_strings_and_integers(self):
        e = self.elf
        hello = RO_VA + 16
        self.assertEqual(e.cstring(hello), "hello")
        self.assertIsNone(e.cstring(hello + 1, 2))           # no NUL inside 2 bytes
        self.assertEqual(e.u8(hello), ord("h"))
        self.assertEqual(e.u16(hello), struct.unpack("<H", b"he")[0])

    def test_notes_dynamic_and_relative_relocs(self):
        e = self.elf
        notes = e.notes()
        self.assertEqual(len(notes), 1)
        self.assertEqual((notes[0].name, notes[0].n_type, notes[0].desc), ("GNU", 3, self.gnu))
        dyn = e.dynamic()
        self.assertEqual(dyn[7], 0x40000 + 0x100)
        offs, adds = e.relative_relocs()
        self.assertEqual(list(offs), [RW_VA, RW_VA + 8])      # sorted by offset
        self.assertEqual(list(adds), [RO_VA + 16, CODE_VA])

    def test_procparam(self):
        pp = self.elf.procparam()
        self.assertEqual(pp["entry_count"], 5)
        self.assertEqual(pp["sdk_version_raw"], 0x1100004012590001)

    def test_ps5_identity(self):
        self.assertTrue(self.elf.is_ps5_dynexec)
        self.assertEqual(self.elf.osabi, 9)
        other = ElfImage(build_elf([Seg(CODE_VA, b"\xc3", 5)], e_type=2))
        self.assertFalse(other.is_ps5_dynexec)

    def test_rejects_bad_images(self):
        with self.assertRaises(ElfError):
            ElfImage(b"not an elf at all" * 8)
        bad = bytearray(self.data)
        bad[4] = 1                                            # ELFCLASS32
        with self.assertRaises(ElfError):
            ElfImage(bytes(bad))
        with self.assertRaises(ElfError):
            ElfImage(self.data[:0x6000])                      # a PT_LOAD runs past the end of the file

    def test_open_from_path(self):
        with tempfile.NamedTemporaryFile(suffix=".bin") as fh:
            fh.write(self.data)
            fh.flush()
            e = ElfImage.open(fh.name)
            self.assertEqual(e.extent, self.elf.extent)


# ---------------------------------------------------------------------------------------------
class StringTableTests(unittest.TestCase):
    def test_ascii_whole_string_semantics(self):
        im = Img()
        im.a.func = None
        va_none = im.ro_str("None")
        im.ro_str("NoneSuch")
        im.ro_str("xNone")
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        st = ctx.index.strings
        self.assertEqual(st.find_ascii("None"), [va_none])
        # anywhere=True admits a string that starts inside a longer one (tail merging) but must still end at a NUL
        self.assertEqual(len(st.find_ascii("None", anywhere=True)), 2)
        self.assertEqual(st.find_ascii("Missing"), [])

    def test_utf16_needs_aligned_nul_delimited_match(self):
        im = Img()
        engine = im.ro_str("Engine", wide=True)
        im.ro_str("OldEngine", wide=True)
        # an odd-aligned copy is not a string
        im.ro += b"\x00" + "Engine".encode("utf-16-le") + b"\x00\x00\x00"
        im.func("f").epilogue()
        st = make_ctx(im.image()).index.strings
        self.assertEqual(st.find_utf16("Engine"), [engine])
        self.assertGreaterEqual(len(st.find_utf16("Engine", anywhere=True)), 2)

    def test_strings_in_code_or_unreadable_segments_are_a_fallback_only(self):
        im = Img()
        im.a.align(4, fill=0)
        im.a.raw(b"\x00\x00" + "OnlyInCode".encode("utf-16-le") + b"\x00\x00")
        data_va = im.ro_str("InBoth", wide=True)
        im.a.raw(b"\x00\x00" + "InBoth".encode("utf-16-le") + b"\x00\x00")
        im.func("f").epilogue()
        st = make_ctx(im.image()).index.strings
        self.assertEqual(len(st.find_utf16("OnlyInCode")), 1)         # found by the fallback
        self.assertEqual(st.find_utf16("InBoth"), [data_va])           # the data-segment hit wins; the code copy is not returned

    def test_containing_searches_return_the_whole_string(self):
        im = Img()
        va = im.ro_str(r".\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp")
        wide = im.ro_str("Maximum number of UObjects (%d) exceeded", wide=True)
        im.func("f").epilogue()
        st = make_ctx(im.image()).index.strings
        self.assertEqual(st.find_ascii_containing("UObjectArray.cpp"), [(va, r".\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp")])
        self.assertEqual(st.find_utf16_containing("Maximum number"), [(wide, "Maximum number of UObjects (%d) exceeded")])
        self.assertEqual(st.find_ascii_prefix(r".\Runtime")[0][0], va)


@needs_capstone
class IndexTests(unittest.TestCase):
    def build(self):
        im = Img()
        self.g = im.bss(0x40)
        self.h = im.bss(8)
        self.lit = im.ro_str("lit")
        a = im.func("one")
        a.lea_rip(RAX, self.g).mov_load_rip(RCX, self.g + 0x10).mov_store_rip(self.h, RDX).lea_rip(R14, self.g)
        a.epilogue()
        a = im.func("two")
        a.lea_rip(RSI, self.lit).call("one").jmp("one").call(0x7777777)   # last call: not a function start
        a.epilogue()
        return make_ctx(im.image())

    def test_rip_relative_references_by_kind(self):
        ctx = self.build()
        x = ctx.index.xrefs
        self.assertEqual(x.count_to(self.g), 2)
        self.assertEqual(x.count_to(self.g, [KIND_LEA]), 2)
        self.assertEqual(x.count_to(self.g + 0x10, [KIND_LOAD]), 1)
        self.assertEqual(x.count_to(self.h, [KIND_STORE]), 1)
        self.assertEqual(x.count_to(self.lit), 1)
        rng = list(x.iter_range(self.g, self.g + 0x20))
        self.assertEqual(sorted(t - self.g for t, _s, _k, _r in rng), [0, 0, 0x10])
        self.assertEqual(len(x.refs_to(self.g)), 2)
        self.assertEqual(x.count_to(0x1234), 0)

    def test_function_map_aligned_starts_and_shrink_wrapping(self):
        im = Img()
        # a shrink-wrapped function: early-out test BEFORE the frame is built
        im.a.int3(1).align(16)
        im.a.label("wrapped")
        im.a.test_rr(RDI, RDI)
        im.a.raw(b"\x74\x00")                                      # je +0 (placeholder early-out)
        im.a.prologue().epilogue()
        im.func("plain").epilogue()
        ctx = make_ctx(im.image())
        fm = ctx.index.funcs
        wrapped = im.a.labels["wrapped"]
        plain = im.a.labels["plain"]
        self.assertTrue(fm.is_start(wrapped))
        self.assertTrue(fm.is_start(plain))
        self.assertFalse(fm.is_start(wrapped + 5))                 # the prologue after the early-out is not a new function
        self.assertEqual(fm.containing(wrapped + 7), wrapped)
        self.assertEqual(fm.containing(plain + 1), plain)
        self.assertEqual(fm.span(wrapped), (wrapped, plain))
        self.assertTrue(fm.usable)

    def test_call_index_only_keeps_function_start_targets(self):
        ctx = self.build()
        calls = ctx.index.calls
        one = [va for va in ctx.index.funcs.starts][0]
        callers = calls.callers(one)
        self.assertEqual(len(callers), 1)                          # the call; the jmp is not a call
        self.assertEqual(len(calls.callers(one, calls_only=False)), 2)
        self.assertEqual(calls.caller_count(0x7777777), 0)
        by_func = calls.callers_by_function(one)
        self.assertEqual(len(by_func), 1)

    def test_decoder_and_timings(self):
        ctx = self.build()
        ins = ctx.index.decoder.decode(int(ctx.index.funcs.starts[0]), 16, 2)
        self.assertEqual(ins[0].mnemonic, "push")
        ctx.index.xrefs, ctx.index.funcs, ctx.index.calls
        self.assertEqual(set(ctx.index.timings), {"index.xrefs", "index.functions", "index.calls"})


# ---------------------------------------------------------------------------------------------
class PlatformTests(unittest.TestCase):
    def image(self, **kw):
        im = Img()
        im.func("f").epilogue()
        return im.image(**kw)

    def test_binding_comes_from_param_json_and_hashes(self):
        param = {"titleId": "PPSA00001", "contentVersion": "01.002.003", "masterVersion": "01.00",
                 "contentId": "XX0000-PPSA00001_00-TESTTEST00000000", "sdkVersion": "0x0900000000000000",
                 "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": "Test Title"}}}
        raw = json.dumps(param).encode()
        ctx = make_ctx(self.image(), param, {"param_bytes": raw})
        platform.detect(ctx)
        self.assertEqual(ctx.val("binding.title_id"), "PPSA00001")
        self.assertEqual(ctx.val("binding.content_version"), "01.002.003")
        self.assertEqual(ctx.val("binding.title_name"), "Test Title")
        self.assertEqual(ctx.val("binding.param_json.sha256"), hashlib.sha256(raw).hexdigest())
        self.assertEqual(ctx.val("binding.executable.sha256"), hashlib.sha256(ctx.elf.data).hexdigest())
        self.assertEqual(int(ctx.val("binding.executable.bytes")), len(ctx.elf.data))

    def test_missing_param_is_reported_not_invented(self):
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        self.assertIsNone(ctx.get("binding.title_id"))
        self.assertIn("binding.param_json", [u.id for u in ctx.unresolved])

    def test_gnu_build_id_trims_only_the_sony_pad(self):
        sixteen = bytes(range(0x10, 0x20))
        ctx = make_ctx(self.image(notes=[gnu_note(sixteen + b"\x00" * 4)]))
        platform.detect(ctx)
        self.assertEqual(ctx.val("build.build_id"), sixteen.hex())
        self.assertIn("pad", " ".join(ctx.get("build.build_id").evidence))
        twenty = bytes(range(1, 21))
        ctx = make_ctx(self.image(notes=[gnu_note(twenty)]))
        platform.detect(ctx)
        self.assertEqual(ctx.val("build.build_id"), twenty.hex())          # a real 20-byte id is kept whole
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        self.assertIn("build.build_id", [u.id for u in ctx.unresolved])

    def test_load_base_is_a_medium_confidence_prediction_unless_overridden(self):
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        base = ctx.get("load.base")
        self.assertEqual(base.confidence, "medium")
        self.assertEqual(base.method, "load.platform_convention")
        self.assertEqual(int(base.value) % 0x4000, 0)
        self.assertEqual(int(ctx.val("load.image_extent.end")), int(base.value) + ctx.elf.extent)
        self.assertEqual(ctx.get("load.image_extent.end").confidence, "medium")
        self.assertEqual(ctx.get("load.image_extent.vaddr_end").confidence, "high")
        ctx = make_ctx(self.image(), options={"load_base": 0x10000000})
        platform.detect(ctx)
        self.assertEqual(int(ctx.val("load.base")), 0x10000000)
        self.assertEqual(ctx.get("load.base").confidence, "high")
        self.assertEqual(ctx.get("load.base").method, "load.cli_override")

    def test_non_ps5_image_gets_no_base(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image(e_type=2))
        platform.detect(ctx)
        self.assertIsNone(ctx.get("load.base"))
        self.assertIn("load.base", [u.id for u in ctx.unresolved])

    def test_file_offset_rule_and_segment_table(self):
        ctx = make_ctx(self.image(relas=[(RW_VA, CODE_VA)]))
        platform.detect(ctx)
        rule = ctx.val("load.file_offset_rule")
        self.assertIn("+0x4000 for ph0,ph1,ph2", rule)
        self.assertIn("-0x20000 for ph3", rule)
        ph0 = ctx.val("elf.segments.ph0")
        self.assertEqual((ph0["type"], ph0["flags"], ph0["file_delta"]), ("LOAD", "R-X", "0x4000"))
        self.assertEqual(ctx.val("elf.section_table_present"), False)
        self.assertEqual(int(ctx.val("elf.rela.relative_entries")), 1)
        self.assertEqual(ctx.val("elf.e_type").__class__, Hex)


# ---------------------------------------------------------------------------------------------
class BuildAxesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.table = build_axes.load_table()
        cls.markers = cls.table["markers"]
        cls.nver = len(cls.table["versions"])

    def covered(self, vi, cfg, stats):
        return [build_axes.covers(m, vi, cfg, stats) for m in self.markers]

    def test_table_shape(self):
        self.assertEqual(len(self.markers), 244)
        self.assertEqual(self.nver, 30)
        self.assertEqual({m["gate"] for m in self.markers},
                         {"", "NotShipping", "DevOnly", "TestOrShipping", "ShippingOnly", "WithStats"})

    def test_gate_rules(self):
        gated = {g: next(m for m in self.markers if m["gate"] == g) for g in ("NotShipping", "DevOnly", "TestOrShipping", "ShippingOnly", "WithStats")}
        cov = build_axes.covers
        self.assertTrue(cov(gated["NotShipping"], -1, 0, False) or gated["NotShipping"]["first"] is not None)
        # configuration 0 = development, 1 = test, 2 = shipping
        m = dict(gated["DevOnly"], first=None, last=None)
        self.assertEqual([cov(m, -1, c, False) for c in (0, 1, 2)], [True, False, False])
        m = dict(gated["NotShipping"], first=None, last=None)
        self.assertEqual([cov(m, -1, c, False) for c in (0, 1, 2)], [True, True, False])
        m = dict(gated["TestOrShipping"], first=None, last=None)
        self.assertEqual([cov(m, -1, c, False) for c in (0, 1, 2)], [False, True, True])
        m = dict(gated["ShippingOnly"], first=None, last=None)
        self.assertEqual([cov(m, -1, c, False) for c in (0, 1, 2)], [False, False, True])
        m = dict(gated["WithStats"], first=None, last=None)
        self.assertEqual([cov(m, -1, 2, s) for s in (False, True)], [False, True])

    def test_every_candidate_build_resolves_to_itself(self):
        # the property that makes the cost model sound: presence == exactly what a build contains
        for vi, cfg, st in [(25, 2, False), (24, 0, True), (20, 1, False), (28, 2, True), (10, 0, False), (29, 1, True)]:
            present = self.covered(vi, cfg, st)
            cands = build_axes.costs(self.markers, present, self.nver)
            self.assertEqual(build_axes.resolve(cands, 2)[0], cfg, (vi, cfg, st))
            self.assertEqual(build_axes.resolve(cands, 3)[0], int(st), (vi, cfg, st))

    def test_margin_rule_leaves_an_axis_undetected(self):
        # (cost, version, configuration, stats): a runner-up closer than MIN_MARGIN leaves the axis undetected
        close = [(10, 0, 0, 0), (12, 0, 1, 0), (30, 0, 2, 1)]
        self.assertIsNone(build_axes.resolve(close, 2)[0])
        clear = [(10, 0, 0, 0), (14, 0, 1, 0), (30, 0, 2, 1)]
        self.assertEqual(build_axes.resolve(clear, 2), (0, 10, 14))
        self.assertEqual(build_axes.resolve(clear, 3)[1:], (10, 30))

    def test_presence_scans_data_segments_first_and_falls_back_to_code(self):
        im = Img()
        want = ["r.Shadow.DistanceFieldPenumbraSize", "log.flushInterval.Shipping", "Cycle counters (flat)"]
        for w in want:
            im.ro_str(w, wide=True)
        im.func("f").epilogue()
        elf = ElfImage(im.image())
        markers = [m for m in self.markers if m["s"] in want]
        self.assertEqual(len(markers), 3)
        self.assertEqual(build_axes.presence(elf, markers), [True, True, True])
        # a marker that only exists inside the code segment is found by the fallback pass
        im2 = Img()
        im2.a.raw(want[0].encode("utf-16-le"))
        im2.func("f").epilogue()
        elf2 = ElfImage(im2.image())
        self.assertEqual(build_axes.presence(elf2, [m for m in self.markers if m["s"] == want[0]]), [True])

    def test_detect_reports_shipping_no_stats_from_a_synthetic_image(self):
        vi, cfg, st = 26, 2, False
        im = Img()
        for m, here in zip(self.markers, self.covered(vi, cfg, st)):
            if here:
                im.ro_str(m["s"], wide=True)
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        build_axes.detect(ctx)
        self.assertEqual(ctx.val("build.configuration"), "shipping")
        self.assertIs(ctx.val("build.non_shipping"), False)
        self.assertIs(ctx.val("build.stats"), False)
        self.assertEqual(ctx.get("build.configuration").confidence, "high")
        self.assertEqual(ctx.facts["axes.version"], "%d.%d" % tuple(self.table["versions"][vi]))
        self.assertGreater(int(ctx.val("build.configuration_margin")), 4)

    def test_too_few_markers_is_unresolved(self):
        im = Img()
        im.ro_str(self.markers[0]["s"], wide=True)
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        build_axes.detect(ctx)
        self.assertIsNone(ctx.get("build.configuration"))
        self.assertEqual({u.id for u in ctx.unresolved}, {"build.configuration", "build.stats", "build.non_shipping"})


# ---------------------------------------------------------------------------------------------
@needs_capstone
class EngineVersionTests(unittest.TestCase):
    def version_image(self, major=5, minor=3, patch=2, changelist=12345, branch="UE5", licensee=False):
        im = Img()
        s = im.ro_str(branch, wide=True)
        a = im.func("leaf_cl")
        a.mov_r32_imm(RAX, changelist).epilogue()
        a = im.func("leaf_lic")
        (a.mov_r32_imm(RAX, 1) if licensee else a.xor_r32(RAX)).epilogue()
        a = im.func("leaf_branch")
        a.lea_rip(RAX, s).epilogue()
        a = im.func("init")
        a.push(RBX).mov_rr(RBX, RDI)
        a.call("leaf_cl").mov_rr32(R14, RAX).call("leaf_lic").movzx_al(R15).shl_r32(R15, 31).or_rr32(R15, R14)
        a.call("leaf_branch").mov_rr(RSI, RAX)
        a.raw(b"\xc7\x83" + struct.pack("<iI", 0, (minor << 16) | major))     # mov dword [rbx], imm32
        a.raw(b"\x66\xc7\x83" + struct.pack("<iH", 4, patch))                  # mov word  [rbx+4], patch
        a.store32(RBX, 8, R15)
        a.pop(RBX).epilogue()
        # decoys: a lone imm32 store, and one without the changelist write
        a = im.func("decoy")
        a.raw(b"\xc7\x83" + struct.pack("<iI", 0, (9 << 16) | 4))
        a.epilogue()
        return im.image()

    def test_store_sequence_gives_version_patch_changelist_and_branch(self):
        ctx = make_ctx(self.version_image())
        ctx.facts["axes.version"] = "5.3"
        ctx.facts["axes.version_margin"] = 12
        engine_version.detect(ctx)
        self.assertEqual((int(ctx.val("build.engine.major")), int(ctx.val("build.engine.minor"))), (5, 3))
        self.assertEqual(int(ctx.val("build.engine.patch")), 2)
        self.assertEqual(int(ctx.val("build.engine.changelist")), 12345)
        self.assertEqual(ctx.val("build.engine.branch"), "UE5")
        self.assertEqual(ctx.val("build.engine.revision_string"), "5.3.2-12345+++UE5")
        self.assertEqual(ctx.get("build.engine.major").confidence, "high")          # store scan + marker fingerprint agree
        self.assertEqual(ctx.get("build.engine.patch").confidence, "medium")
        self.assertEqual(ctx.get("build.engine.revision_string").method, "engine.revision_join")

    def test_store_scan_alone_is_medium_and_disagreeing_fingerprint_is_not_high(self):
        ctx = make_ctx(self.version_image())
        engine_version.detect(ctx)
        self.assertEqual(ctx.get("build.engine.major").confidence, "medium")
        ctx = make_ctx(self.version_image())
        ctx.facts["axes.version"] = "5.5"
        engine_version.detect(ctx)
        self.assertEqual(ctx.get("build.engine.minor").confidence, "medium")
        self.assertTrue(any("disagrees" in e for e in ctx.get("build.engine.minor").evidence))

    def test_licensee_bit_does_not_leak_into_the_changelist(self):
        ctx = make_ctx(self.version_image(changelist=77, licensee=True))
        engine_version.detect(ctx)
        self.assertEqual(int(ctx.val("build.engine.changelist")), 77)
        self.assertTrue(any("licensee bit 1" in e for e in ctx.get("build.engine.changelist").evidence))

    def test_implausible_versions_are_ignored(self):
        im = Img()
        a = im.func("init")
        a.raw(b"\xc7\x83" + struct.pack("<iI", 0, (60 << 16) | 5)).raw(b"\x66\xc7\x83" + struct.pack("<iH", 4, 1))
        a.store32(RBX, 8, R15).epilogue()
        ctx = make_ctx(im.image())
        engine_version.detect(ctx)
        self.assertIsNone(ctx.get("build.engine.major"))
        self.assertIn("build.engine.major", [u.id for u in ctx.unresolved])

    def test_version_string_strategy(self):
        im = Img()
        im.ro_str("5.4.2-33043543+++UE5+Release-5.4", wide=True)
        im.ro_str("Shipping 5.1.3-12 build", wide=True)            # not a NUL-delimited FEngineVersion string
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        engine_version.detect(ctx)
        self.assertEqual((int(ctx.val("build.engine.major")), int(ctx.val("build.engine.minor"))), (5, 4))
        self.assertEqual(ctx.get("build.engine.major").method, "engine.version_string")
        self.assertIn("build.engine.patch", [u.id for u in ctx.unresolved])        # strings carry no store site

    def test_fingerprint_alone_gives_a_medium_version_without_patch_or_revision(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        ctx.facts["axes.version"] = "5.2"
        ctx.facts["axes.version_margin"] = 9
        engine_version.detect(ctx)
        self.assertEqual((int(ctx.val("build.engine.major")), int(ctx.val("build.engine.minor"))), (5, 2))
        self.assertEqual(ctx.get("build.engine.major").confidence, "medium")
        self.assertEqual(ctx.get("build.engine.major").method, "engine.marker_fingerprint")
        self.assertIsNone(ctx.get("build.engine.revision_string"))
        self.assertIn("build.engine.patch", [u.id for u in ctx.unresolved])

    def test_conflicting_candidates_are_ambiguous(self):
        im = Img()
        im.ro_str("5.4.2-1+++UE5+Release-5.4", wide=True)
        im.ro_str("5.2.0-1+++UE5+Release-5.2", wide=True)
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        engine_version.detect(ctx)
        self.assertIsNone(ctx.get("build.engine.major"))


# ---------------------------------------------------------------------------------------------
@needs_capstone
class FNameCtorTests(unittest.TestCase):
    MODULES = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]

    def image(self, targets=None, with_edx=True, edx_read=False, only=None):
        im = Img()
        lits = {m: im.ro_str(m, wide=True) for m in self.MODULES}
        a = im.func("intern")
        a.xor_r32(RAX).epilogue()
        a = im.func("ctor")
        a.push(RBX).mov_rr(RBX, RDI)
        if edx_read:
            a.mov_rr32(RAX, RDX)
        a.mov_rr(RDI, RSI).call("intern").store64(RBX, 0, RAX).pop(RBX).epilogue()
        for t in ("other_a", "other_b", "other_c", "other_d", "other_e"):
            im.func(t).epilogue()
        a = im.func("registration")
        for k, m in enumerate(only or self.MODULES):
            tgt = (targets or {}).get(m, "ctor")
            a.lea_rip(RSI, lits[m]).lea_rm(RDI, RSP, 0x20 + 8 * k)
            if with_edx:
                a.mov_r32_imm(RDX, 1)
            a.call(tgt)
        a.epilogue()
        # a decoy: the literal followed by a call without the stack out-pointer
        a = im.func("decoy")
        a.lea_rip(RSI, lits["Engine"]).call("other_a").epilogue()
        img = im.image()
        self.ctor = im.a.labels["ctor"]
        self.intern = im.a.labels["intern"]
        self.lits = lits
        self.labels = dict(im.a.labels)
        return img

    def test_five_anchors_agree_on_the_constructor(self):
        ctx = make_ctx(self.image())
        fname_ctor.detect(ctx)
        self.assertEqual(int(ctx.val("symbols.fname_ctor_wchar.label")), self.ctor)
        env = ctx.get("symbols.fname_ctor_wchar.label")
        self.assertEqual(env.confidence, "high")
        self.assertTrue(any("5 of 5" in e for e in env.evidence))
        self.assertEqual(len(ctx.val("symbols.fname_ctor_wchar.callsites").split(",")), 5)
        self.assertEqual(ctx.val("symbols.fname_ctor_wchar.anchor_modules"), ",".join(self.MODULES))
        self.assertEqual(int(ctx.val("symbols.fname_ctor_wchar.find_arg_at_callsites")), 1)
        self.assertEqual(int(ctx.val("symbols.fname_intern.label")), self.intern)
        self.assertEqual(int(ctx.val("structs.FName.size")), 8)

    def test_decoy_reference_without_the_argument_setup_does_not_vote(self):
        ctx = make_ctx(self.image())
        fname_ctor.detect(ctx)
        self.assertNotIn("0x%x" % self.labels["other_a"], str(ctx.evidence_files["fname_ctor"]["targets"]))

    def test_abi_text_follows_whether_edx_is_read(self):
        ctx = make_ctx(self.image())
        fname_ctor.detect(ctx)
        self.assertEqual(ctx.val("symbols.fname_ctor_wchar.abi"), "sysv-amd64; rdi=out FName*; rsi=const char16_t*; edx=EFindName")
        ctx = make_ctx(self.image(edx_read=True))
        fname_ctor.detect(ctx)
        self.assertTrue(ctx.val("symbols.fname_ctor_wchar.abi").endswith("edx=EFindName (read)"))

    def test_two_anchors_are_only_medium(self):
        ctx = make_ctx(self.image(only=["Engine", "Renderer"]))
        fname_ctor.detect(ctx)
        # strict mode finds 2 anchors (< 3), the relaxed pass keeps them; either way it must not claim high
        self.assertNotEqual(ctx.get("symbols.fname_ctor_wchar.label").confidence, "high")

    def test_disagreeing_anchors_are_low(self):
        ctx = make_ctx(self.image(targets={m: t for m, t in zip(self.MODULES, ["other_a", "other_b", "other_c", "other_d", "other_e"])}))
        fname_ctor.detect(ctx)
        env = ctx.get("symbols.fname_ctor_wchar.label")
        self.assertEqual(env.confidence, "low")

    def test_missing_edx_setup_falls_back_to_the_relaxed_pass(self):
        ctx = make_ctx(self.image(with_edx=False))
        fname_ctor.detect(ctx)
        env = ctx.get("symbols.fname_ctor_wchar.label")
        self.assertEqual(int(env.value), self.ctor)
        self.assertTrue(any("relaxed" in e for e in env.evidence))
        self.assertIsNone(ctx.get("symbols.fname_ctor_wchar.find_arg_at_callsites"))

    def test_no_anchor_strings_is_unresolved(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        fname_ctor.detect(ctx)
        self.assertIsNone(ctx.get("symbols.fname_ctor_wchar.label"))
        self.assertIn("symbols.fname_ctor_wchar", [u.id for u in ctx.unresolved])


# ---------------------------------------------------------------------------------------------
NAMES = ["None", "ByteProperty", "IntProperty", "BoolProperty", "FloatProperty", "ObjectProperty", "NameProperty",
         "DelegateProperty", "DoubleProperty", "ArrayProperty"]


class PoolFixture:
    """A synthetic FNamePool + allocator world whose numbers are deliberately not any game's."""

    CUR = 0x10                 # CurrentBlock
    BLOCK, ALIGN = 0x8000, 4
    NBLOCKS = 4096
    DISP_MALLOC, DISP_FREE, DISP_REALLOC = 0x38, 0x58, 0x48
    HASH, STRIDE, NONE_ID, BYTE_ID = 0x8020, 0x20, 0x9020, 0x9024

    def __init__(self, growth=True, directly_loaded=False):
        im = self.im = Img()
        base = im.bss(0x20000)
        self.flag = base + 0x10
        self.pool = base + 0x40                          # flag = pool - 0x30
        self.G = im.bss(8)
        self.SLOT = im.bss(8)
        self.INST = im.bss(0x100)
        lits = [im.ro_str(n) for n in NAMES]
        im.func("memset").epilogue()
        im.func("store").xor_r32(RAX).epilogue()
        a = im.func("malloc")
        a.push(RBX)
        if directly_loaded:
            a.mov_rr(RBX, RDI).mov_rr32(RDX, RSI).mov_load_rip(RDI, self.G)
        else:
            a.lea_rip(R14, self.G).mov_rr(RBX, RDI).mov_rr32(RDX, RSI).load64(RDI, R14, 0)
        a.load64(RAX, RDI, 0).mov_rr(RSI, RBX).call_mem(RAX, self.DISP_MALLOC).pop(RBX).epilogue()
        a = im.func("free")
        a.push(RBX).lea_rip(R14, self.G).mov_rr(RBX, RDI).load64(RDI, R14, 0).load64(RAX, RDI, 0)
        a.mov_rr(RSI, RBX).call_mem(RAX, self.DISP_FREE).pop(RBX).epilogue()
        a = im.func("realloc")
        a.push(RBX).lea_rip(R14, self.G).mov_rr(RBX, RDI).mov_rr32(RCX, RDX).load64(RDI, R14, 0).load64(RAX, RDI, 0)
        a.mov_rr(RSI, RBX).call_mem(RAX, self.DISP_REALLOC).pop(RBX).epilogue()
        for k in range(30):
            im.func("free_user%d" % k).call("free").epilogue()
        for k in range(3):
            im.func("realloc_user%d" % k).call("realloc").epilogue()
        for k in range(5):
            im.func("malloc_user%d" % k).call("malloc").epilogue()
        a = im.func("init")
        a.push(RBX).mov_rr(RBX, RDI)
        a.lea_rm(RDI, RBX, self.CUR).mov_r32_imm(RDX, 8 + 8 * self.NBLOCKS).xor_r32(RSI).call("memset")
        a.mov_r32_imm(RDI, self.BLOCK).mov_r32_imm(RSI, self.ALIGN).call("malloc").store64(RBX, self.CUR + 8, RAX)
        a.lea_rm(R14, RBX, self.HASH).add_r64_imm8(R14, self.STRIDE)
        for n, lit in zip(NAMES, lits):
            a.lea_rip(RSI, lit).mov_r32_imm(RDX, len(n)).mov_rr(RDI, RBX).call("store")
            if n == "None":
                a.store32(RBX, self.NONE_ID, RAX)
            if n == "ByteProperty":
                a.store32(RBX, self.BYTE_ID, RAX)
        a.pop(RBX).epilogue()
        a = im.func("lazy")
        a.cmp8_rip_imm(self.flag, 0).lea_rip(RDI, self.pool).call("init").mov8_rip_imm(self.flag, 1).epilogue()
        for k in range(4):                               # readers of the pool
            a = im.func("reader%d" % k)
            a.lea_rip(RCX, self.pool).shr_r32(RAX, 16).load64_sib(RAX, RCX, RAX, 8, self.CUR + 8)
            a.raw(b"\x0f\xb7\x04\x48").epilogue()        # movzx eax, word [rax + rcx*2]
        if growth:
            a = im.func("grow")
            a.push(RBX).mov_rr(RBX, RDI).load32(RCX, RBX, self.CUR).load32(RDX, RBX, self.CUR + 4)
            a.mov_r32_imm(RDI, self.BLOCK).mov_r32_imm(RSI, self.ALIGN).call("malloc")
            a.store32(RBX, self.CUR, RCX).store32_imm(RBX, self.CUR + 4, 0).store64_sib(RBX, RCX, 8, self.CUR + 8, RAX)
            a.pop(RBX).epilogue()
        # lazy create + factory (base allocator pointer)
        im.func("acc").mov_load_rip(RAX, self.SLOT).epilogue()
        im.func("gcreate").call("acc").lea_rip(R13, self.G).store64(R13, 0, RAX).epilogue()
        im.func("ctor").epilogue()
        a = im.func("factory")
        a.lea_rip(RBX, self.INST).xor_r32(RSI).mov_rr(RDI, RBX).call("ctor").mov_store_rip(self.SLOT, RBX).epilogue()
        self.labels = im.a.labels
        self.data = im.image()


@needs_capstone
class FNamePoolAndGMallocTests(unittest.TestCase):
    def run_pool(self, **kw):
        fx = PoolFixture(**kw)
        ctx = make_ctx(fx.data)
        platform.detect(ctx)
        fname_pool.detect(ctx)
        return fx, ctx

    def test_pool_layout_is_read_from_the_init_function(self):
        fx, ctx = self.run_pool()
        L = fx.labels
        self.assertEqual(int(ctx.val("symbols.fnamepool_init.label")), L["init"])
        self.assertEqual(int(ctx.val("symbols.name_pool.label")), fx.pool)
        self.assertEqual(int(ctx.val("structs.FNamePool.current_block")), PoolFixture.CUR)
        self.assertEqual(int(ctx.val("structs.FNamePool.current_byte_offset")), PoolFixture.CUR + 4)
        self.assertEqual(int(ctx.val("structs.FNamePool.blocks")), PoolFixture.CUR + 8)
        self.assertEqual(int(ctx.val("structs.FNamePool.block_size")), PoolFixture.BLOCK)
        self.assertEqual(int(ctx.val("structs.FNamePool.max_blocks")), PoolFixture.NBLOCKS)
        self.assertEqual(int(ctx.val("structs.FNamePool.none_id")), PoolFixture.NONE_ID)
        self.assertEqual(int(ctx.val("structs.FNamePool.byte_property_id")), PoolFixture.BYTE_ID)
        self.assertEqual(int(ctx.val("structs.FNamePool.hash_buckets")), PoolFixture.HASH)
        self.assertEqual(int(ctx.val("structs.FNamePool.hash_bucket_stride")), PoolFixture.STRIDE)
        self.assertEqual(int(ctx.val("structs.FNamePool.hash_bucket_count")), (PoolFixture.NONE_ID - PoolFixture.HASH) // PoolFixture.STRIDE)
        self.assertEqual(int(ctx.val("structs.FNamePool.default_name_count")), len(NAMES))
        self.assertEqual(int(ctx.val("symbols.fname_default_name_store.label")), L["store"])
        self.assertEqual(int(ctx.val("symbols.fmemory_malloc.label")), L["malloc"])
        self.assertEqual(int(ctx.val("symbols.fnamepool_reserve.label")), L["grow"])
        self.assertEqual(int(ctx.val("symbols.name_pool_init_flag.label")), fx.flag)
        self.assertEqual(ctx.val("structs.FNamePool.init_flag_offset"), "-0x30")
        self.assertEqual(int(ctx.val("structs.FNamePool.block_offset_bits")), 16)
        self.assertEqual(int(ctx.val("structs.FNamePool.entry_stride")), 2)

    def test_growth_function_raises_confidence_and_absence_lowers_it(self):
        _fx, ctx = self.run_pool()
        self.assertEqual(ctx.get("structs.FNamePool.current_byte_offset").confidence, "high")
        self.assertEqual(ctx.get("structs.FNamePool.current_byte_offset").method, "fname_pool.growth_function")
        _fx, ctx = self.run_pool(growth=False)
        env = ctx.get("structs.FNamePool.current_byte_offset")
        self.assertEqual(env.confidence, "low")
        self.assertEqual(int(env.value), PoolFixture.CUR + 4)
        self.assertEqual(env.method, "fname_pool.schema_order")
        self.assertIsNone(ctx.get("symbols.fnamepool_reserve.label"))

    def test_too_few_default_names_means_no_pool(self):
        im = Img()
        lit = im.ro_str("None")
        im.func("f").lea_rip(RSI, lit).epilogue()
        ctx = make_ctx(im.image())
        fname_pool.detect(ctx)
        self.assertIsNone(ctx.get("symbols.name_pool.label"))
        self.assertIn("symbols.name_pool", [u.id for u in ctx.unresolved])

    def test_a_name_cluster_without_the_pool_layout_shape_is_only_low(self):
        im = Img()
        pool = im.bss(0x100)
        lits = [im.ro_str(n) for n in NAMES]
        im.func("store").xor_r32(RAX).epilogue()
        a = im.func("registrar")                      # registers the names, but is not a pool constructor
        a.mov_rr(RBX, RDI)
        for n, lit in zip(NAMES, lits):
            a.lea_rip(RSI, lit).mov_r32_imm(RDX, len(n)).mov_rr(RDI, RBX).call("store")
        a.epilogue()
        im.func("caller").lea_rip(RDI, pool).call("registrar").epilogue()
        ctx = make_ctx(im.image())
        platform.detect(ctx)
        fname_pool.detect(ctx)
        self.assertEqual(ctx.get("symbols.name_pool.label").confidence, "low")
        self.assertIsNone(ctx.get("structs.FNamePool.blocks"))
        self.assertIn("structs.FNamePool.blocks", [u.id for u in ctx.unresolved])

    def test_gmalloc_is_what_malloc_and_free_load(self):
        fx = PoolFixture()
        ctx = make_ctx(fx.data)
        platform.detect(ctx)
        fname_pool.detect(ctx)
        gmalloc.detect(ctx)
        L = fx.labels
        self.assertEqual(int(ctx.val("symbols.gmalloc.label")), fx.G)
        self.assertEqual(int(ctx.val("symbols.gmalloc_effective_ptr.label")), fx.G)
        self.assertEqual(ctx.get("symbols.gmalloc.label").confidence, "high")
        self.assertEqual(int(ctx.val("symbols.fmemory_malloc.label")), L["malloc"])
        self.assertEqual(int(ctx.val("symbols.fmemory_free.label")), L["free"])    # 30 callers beat realloc's 3 and has one arg
        self.assertEqual(int(ctx.val("vtables.FMalloc.Malloc")), PoolFixture.DISP_MALLOC)
        self.assertEqual(int(ctx.val("vtables.FMalloc.Free")), PoolFixture.DISP_FREE)
        # runtime == label + base is added by the assembler; the detector only writes labels
        self.assertIsNone(ctx.get("symbols.gmalloc.runtime"))

    def test_base_allocator_slot_comes_from_the_lazy_create_function(self):
        fx = PoolFixture()
        ctx = make_ctx(fx.data)
        platform.detect(ctx)
        fname_pool.detect(ctx)
        gmalloc.detect(ctx)
        L = fx.labels
        self.assertEqual(int(ctx.val("symbols.gmalloc_base_slot.label")), fx.SLOT)
        self.assertNotEqual(fx.SLOT, fx.G)                      # two different variables, both reported
        self.assertEqual(int(ctx.val("symbols.gmalloc_factory.label")), L["factory"])
        self.assertEqual(int(ctx.val("symbols.gmalloc_instance.label")), fx.INST)
        self.assertEqual(int(ctx.val("symbols.gmalloc_instance_ctor.label")), L["ctor"])

    def test_direct_load_dispatcher_is_also_recognised(self):
        fx = PoolFixture(directly_loaded=True)
        ctx = make_ctx(fx.data)
        platform.detect(ctx)
        fname_pool.detect(ctx)
        gmalloc.detect(ctx)
        self.assertEqual(int(ctx.val("symbols.gmalloc.label")), fx.G)
        self.assertIn("load", ctx.evidence_files["gmalloc"]["dispatch"]["kind"])

    def test_allocator_family_is_the_most_frequent_identifier(self):
        im = Img()
        for _ in range(3):
            im.ro_str("FMallocBinned3 Attempt to free an unrecognized pointer %p", wide=True)
        im.ro_str("FMallocBinned Attempt to GetAllocationSizeExternal", wide=True)
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        fam = gmalloc.allocator_family(ctx)
        self.assertEqual(fam[0], "FMallocBinned3")
        self.assertEqual(fam[1]["FMallocBinned"], 1)


# ---------------------------------------------------------------------------------------------
@needs_capstone
class UObjectArrayTests(unittest.TestCase):
    OBJ = 0x18                  # ObjObjects (deliberately not the usual 0x10)
    CREATE, DELETE = 0x60, 0x70
    RECYCLE = 0x88
    NEXT_GLOBAL = 0x90

    def image(self):
        im = Img()
        self.G = im.bss(0x300)
        self.path = im.ro_str(r".\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp")
        self.msg = im.ro_str("Maximum number of UObjects (%d) exceeded", wide=True)
        obj, num = self.OBJ, self.OBJ + 0x14
        # member 1: allocate-like (this = rdi)
        a = im.func("m_alloc")
        a.push(R14).mov_rr(R14, RDI).lea_rip(RDI, self.path).lea_rip(RDX, self.msg).lea_rip(RAX, self.G)
        a.cmp8_mem_imm(R14, 0xC, 1).cmp32_mem_reg(R14, num, R15).load64(RDX, R14, obj)
        a.load32(R12, R14, obj + 0x10).store32_imm(R14, obj + 0x1C, 1)
        a.load64(RAX, R14, self.CREATE).movsxd_load(RCX, R14, self.CREATE + 8).load64_sib(RDI, RAX, R12, 8, 0)
        a.load32(RAX, R14, 0).load32(RCX, R14, 4).load32(RDX, R14, 8)
        a.pop(R14).epilogue()
        # member 2: delete-listener notification
        a = im.func("m_delete")
        a.push(R14).mov_rr(R14, RDI).lea_rip(RDI, self.path).lea_rip(RAX, self.G)
        a.load64(RAX, R14, self.DELETE).movsxd_load(RCX, R14, self.DELETE + 8).load64_sib(RDI, RAX, R13, 8, 0)
        a.pop(R14).epilogue()
        # member 3: free-like (tests the recycle flag)
        a = im.func("m_free")
        a.push(R14).mov_rr(R14, RDI).lea_rip(RDI, self.path).lea_rip(RAX, self.G)
        a.cmp8_mem_imm(R14, self.RECYCLE, 1).pop(R14).epilogue()
        # the next global starts right after the structure
        other = self.G + self.NEXT_GLOBAL
        for k in range(130):                                 # hot global: IndexToObject at many sites
            a = im.func("user%d" % k)
            a.lea_rip(RAX, self.G).cmp32_mem_reg(RAX, num, R15).load64(RCX, RAX, obj).shr_r32(RDX, 16)
            a.load64_sib(RCX, RCX, RDX, 8, 0)
            a.raw(b"\x48\x8d\x14\x52")                       # lea rdx, [rdx + rdx*2]
            a.lea_rip(RSI, other).epilogue()
        return im.image()

    def test_global_is_the_one_the_uobjectarray_functions_take_the_address_of(self):
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        uobject_array.detect(ctx)
        env = ctx.get("symbols.object_array.label")
        self.assertEqual(int(env.value), self.G)
        self.assertEqual(env.confidence, "high")
        self.assertTrue(any("IndexToObject" in e for e in env.evidence))

    def test_layout_follows_the_code_not_a_template(self):
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        uobject_array.detect(ctx)
        v = lambda p: int(ctx.val("structs." + p))
        self.assertEqual(v("FUObjectArray.ObjObjects"), self.OBJ)
        self.assertEqual(v("TUObjectArray.Objects"), 0)
        self.assertEqual(v("TUObjectArray.NumElements"), 0x14)
        self.assertEqual(v("TUObjectArray.elements_per_chunk"), 1 << 16)
        self.assertEqual(v("FUObjectArray.UObjectCreateListeners"), self.CREATE)
        self.assertEqual(v("FUObjectArray.UObjectDeleteListeners"), self.DELETE)
        self.assertEqual(v("FUObjectArray.bShouldRecycleObjectIndices"), self.RECYCLE)
        self.assertEqual(v("FUObjectArray.PrimarySerialNumber"), self.RECYCLE - 4)
        self.assertEqual(v("FUObjectArray.UEP_TotalSize"), self.NEXT_GLOBAL)
        self.assertEqual(ctx.get("structs.FUObjectArray.UEP_TotalSize").confidence, "high")    # next global agrees
        self.assertEqual(ctx.get("structs.FUObjectArray.PrimarySerialNumber").confidence, "medium")
        self.assertEqual(ctx.get("structs.FUObjectArray.UObjectCreateListeners").confidence, "medium")
        self.assertEqual(v("FUObjectItem.stride"), 24)

    def test_fields_the_code_never_touches_are_not_called_high(self):
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        uobject_array.detect(ctx)
        # PreAllocatedObjects (Objects+8) and MaxChunks (Objects+0x18) are not accessed anywhere in the fixture
        self.assertEqual(ctx.get("structs.TUObjectArray.PreAllocatedObjects").confidence, "medium")
        self.assertEqual(ctx.get("structs.TUObjectArray.MaxChunks").confidence, "medium")
        self.assertEqual(ctx.get("structs.TUObjectArray.MaxElements").confidence, "high")     # read in the allocate-like member

    def test_no_source_path_string_means_no_global(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        uobject_array.detect(ctx)
        self.assertIsNone(ctx.get("symbols.object_array.label"))
        self.assertIn("symbols.object_array", [u.id for u in ctx.unresolved])


# ---------------------------------------------------------------------------------------------
@needs_capstone
class StaticConstructAndFindTests(unittest.TestCase):
    def sco_image(self, sites=25, decoy_sites=2):
        im = Img()
        lit = im.ro_str("NewObject with empty name can't be used to create default subobjects (inside of UObject derived class)", wide=True)
        im.func("assert_fn").epilogue()
        im.func("params_ctor").epilogue()
        a = im.func("sco")
        for r in (R15, R14, R13, R12, RBX):
            a.push(r)
        a.xor_r32(RAX).pop(RBX).epilogue()
        im.func("other_a").epilogue()
        im.func("other_b").epilogue()
        for k in range(sites):
            a = im.func("newobject%d" % k)
            a.lea_rip(RSI, lit).mov_rr(RDI, R15).call("assert_fn")
            a.lea_rm(R13, RBP, -0xB0).mov_rr(RSI, R12).mov_rr(RDI, R13).call("params_ctor")
            a.mov_rr(RDI, R13).call("sco").epilogue()
        for k in range(decoy_sites):                       # same literal, different calls: not the factory pair
            a = im.func("decoy%d" % k)
            a.lea_rip(RSI, lit).mov_rr(RDI, R15).call("assert_fn").call("other_a").call("other_b").epilogue()
        self.labels = im.a.labels
        self.lit = lit
        return im.image()

    def test_dominant_call_pair_names_the_factory(self):
        ctx = make_ctx(self.sco_image())
        static_construct.detect(ctx)
        L = self.labels
        self.assertEqual(int(ctx.val("symbols.static_construct_object_internal.label")), L["sco"])
        self.assertEqual(int(ctx.val("symbols.sco_params_ctor.label")), L["params_ctor"])
        self.assertEqual(int(ctx.val("symbols.assert_if_in_constructor.label")), L["assert_fn"])
        self.assertEqual(int(ctx.val("symbols.static_construct_object_internal.anchor_string_label")), self.lit)
        env = ctx.get("symbols.static_construct_object_internal.label")
        self.assertEqual(env.confidence, "high")
        self.assertTrue(any("dominant pair" in e and "in 25 windows" in e for e in env.evidence))

    def test_few_sites_are_not_high(self):
        ctx = make_ctx(self.sco_image(sites=6))
        static_construct.detect(ctx)
        self.assertEqual(int(ctx.val("symbols.static_construct_object_internal.label")), self.labels["sco"])
        self.assertEqual(ctx.get("symbols.static_construct_object_internal.label").confidence, "medium")

    def test_no_stack_struct_pair_is_unresolved(self):
        im = Img()
        lit = im.ro_str("NewObject with empty name can't ...", wide=True)
        im.func("a1").epilogue()
        im.func("a2").epilogue()
        im.func("use").lea_rip(RSI, lit).call("a1").call("a2").epilogue()
        ctx = make_ctx(im.image())
        static_construct.detect(ctx)
        self.assertIsNone(ctx.get("symbols.static_construct_object_internal.label"))
        self.assertIn("symbols.static_construct_object_internal", [u.id for u in ctx.unresolved])

    def find_image(self):
        im = Img()
        ma = im.ro_str("Illegal call to StaticFindObjectFast() while serializing object data!", wide=True)
        mb = im.ro_str("Illegal call to StaticFindObjectFast() while garbage collecting!", wide=True)
        im.func("inner").epilogue()
        # string variant: walks rdx as UTF-16, compares rsi with -1, carries both messages
        a = im.func("find_string")
        a.push(RBX).mov_rr(RBX, RDI)
        a.cmp16_mem_imm(RDX, 0, 0).cmp16_mem_imm(RDX, 2, 0).cmp_r64_imm8(RSI, 0xFF)
        a.lea_rip(RDX, ma).lea_rip(RCX, mb).raw(b"\x90" * 0x120).call("inner").pop(RBX).epilogue()
        # fast variant: FName argument, one message, no string walk
        a = im.func("find_fast")
        a.push(RBX).mov_rr(RBX, RDI).lea_rip(RDX, ma).raw(b"\x90" * 0x120).call("inner").pop(RBX).epilogue()
        for k in range(8):
            im.func("user%d" % k).call("find_string").epilogue()
        self.labels = im.a.labels
        return im.image()

    def test_string_variant_is_picked_by_its_traits(self):
        ctx = make_ctx(self.find_image())
        static_find_object.detect(ctx)
        env = ctx.get("symbols.static_find_object.label")
        self.assertEqual(int(env.value), self.labels["find_string"])
        self.assertEqual(env.confidence, "medium")                 # one structural method: never high
        self.assertIn("rdx=const char16_t* name", ctx.val("symbols.static_find_object.abi"))
        self.assertTrue(any("rsi compared with -1" in e for e in ctx.get("symbols.static_find_object.abi").evidence))

    def test_without_the_fatal_messages_there_is_no_candidate(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        static_find_object.detect(ctx)
        self.assertIsNone(ctx.get("symbols.static_find_object.label"))


# ---------------------------------------------------------------------------------------------
@needs_capstone
class ProcessEventTests(unittest.TestCase):
    SLOT = 12
    FLAGS = 0x98                       # FunctionFlags offset used by the fixture (test byte at +0x99)
    N_VT, N_SLOT = 30, 32

    def image(self, pe_native=True, dword_test=False):
        im = Img()
        self.typeinfo = im.ro_str("typeinfo")
        a = im.func("pe")
        a.push(RBX).mov_rr(RBX, RDI)
        if pe_native and dword_test:
            a.test32_mem_imm(RSI, self.FLAGS, 0x400)
        elif pe_native:
            a.test8_mem_imm(RSI, self.FLAGS + 1, 4)
        a.load64(RAX, RDI, 0).call_mem(RAX, (self.SLOT + 1) * 8).load64(RAX, RDI, 0).call_mem(RAX, (self.SLOT + 2) * 8)
        a.raw(b"\x90" * 0xD0).pop(RBX).epilogue()
        a = im.func("override")                              # an AActor-style override: native test but no virtual calls
        a.test8_mem_imm(RSI, self.FLAGS + 1, 4).raw(b"\x90" * 0xD0).epilogue()
        stubs = []
        for k in range(self.N_SLOT):
            a = im.func("stub%d" % k)
            a.xor_r32(RAX).epilogue()
            stubs.append("stub%d" % k)
        relas = []
        off = RW_VA
        layout = im.a.labels
        for v in range(self.N_VT):
            relas.append((off + 8, RO_VA + 16))                  # typeinfo pointer (data)
            base = off + 16
            for k in range(self.N_SLOT):
                if k == self.SLOT:
                    fn = layout["override"] if v < 5 else layout["pe"]
                else:
                    fn = layout[stubs[k]] if (v + k) % 7 else layout[stubs[(k + 1) % self.N_SLOT]]
                relas.append((base + 8 * k, fn))
            off = base + 8 * self.N_SLOT + 16
        assert off < RW_VA + RW_FILESZ
        self.labels = dict(layout)
        im.relas = relas
        return im.image()

    def test_vtable_runs_are_read_from_relocations_not_file_bytes(self):
        elf = ElfImage(self.image())
        runs = vtables.find_runs(elf, 16)
        self.assertEqual(len(runs), self.N_VT)
        self.assertEqual(len(runs[0].slots), self.N_SLOT)
        self.assertEqual(runs[7].slots[self.SLOT], self.labels["pe"])
        self.assertEqual(runs[0].slots[self.SLOT], self.labels["override"])
        self.assertEqual(elf.u64(runs[0].offset), 0)             # the file bytes of a vtable slot are zero

    def test_runs_break_at_non_code_pointers(self):
        elf = ElfImage(self.image())
        runs = vtables.find_runs(elf, 4)
        self.assertTrue(all(r.offset % 8 == 0 for r in runs))
        self.assertEqual(len({r.offset for r in runs}), len(runs))

    def test_process_event_slot_and_function_come_from_content(self):
        ctx = make_ctx(self.image())
        platform.detect(ctx)
        process_event.detect(ctx)
        self.assertEqual(int(ctx.val("vtables.UObject.ProcessEvent")), self.SLOT * 8)
        self.assertEqual(int(ctx.val("vtables.UObject.ProcessEvent_slot_index")), self.SLOT)
        self.assertEqual(int(ctx.val("symbols.process_event.label")), self.labels["pe"])
        self.assertEqual(ctx.get("vtables.UObject.ProcessEvent").confidence, "high")
        self.assertEqual(int(ctx.val("structs.UFunction.FunctionFlags")), self.FLAGS)
        self.assertEqual(int(ctx.val("vtables.UObject.GetFunctionCallspace")), (self.SLOT + 1) * 8)
        self.assertEqual(int(ctx.val("vtables.UObject.CallRemoteFunction")), (self.SLOT + 2) * 8)
        self.assertEqual(int(ctx.val("vtables.UObject.dtor_prefix_slots")), 2)
        self.assertEqual(ctx.val("symbols.process_event.prologue_24"), ctx.elf.read(self.labels["pe"], 24).hex())

    def test_the_dword_form_of_the_native_test_is_recognised_too(self):
        ctx = make_ctx(self.image(dword_test=True))
        platform.detect(ctx)
        process_event.detect(ctx)
        self.assertEqual(int(ctx.val("vtables.UObject.ProcessEvent")), self.SLOT * 8)
        self.assertEqual(int(ctx.val("structs.UFunction.FunctionFlags")), self.FLAGS)

    def test_a_widely_shared_function_without_the_native_test_is_not_process_event(self):
        ctx = make_ctx(self.image(pe_native=False))
        platform.detect(ctx)
        process_event.detect(ctx)
        env = ctx.get("vtables.UObject.ProcessEvent")
        # only the override (5 of 30 vtables) tests FUNC_Native, and it has no virtual calls: not high
        self.assertTrue(env is None or env.confidence != "high")

    def test_no_vtables_is_unresolved(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        process_event.detect(ctx)
        self.assertIn("vtables.UObject.ProcessEvent", [u.id for u in ctx.unresolved])


# ---------------------------------------------------------------------------------------------
def sample_doc(**over):
    def env(v, c="high", m="test"):
        return {"value": v, "confidence": c, "method": m, "evidence": []}
    doc = {"build": {"engine": {"major": env(Hex(5)), "minor": env(Hex(3)), "revision_string": env("5.3.2-7+++UE5", "medium")},
                     "non_shipping": env(False), "stats": env(False), "configuration": env("shipping"),
                     "build_id": env("ab" * 16)},
           "vtables": {"UObject": {"ProcessEvent": env("0x60")}},
           "structs": {"FUObjectArray": {}, "TUObjectArray": {}}}
    for k, v in zip(emit_ue4ss.FUOBJECTARRAY_FIELDS, range(0, 400, 8)):
        doc["structs"]["FUObjectArray"][k] = env("0x%x" % v)
    for k, v in zip(emit_ue4ss.TUOBJECTARRAY_FIELDS, range(0, 80, 8)):
        doc["structs"]["TUObjectArray"][k] = env("0x%x" % v)
    doc.update(over)
    return doc


class EmitTests(unittest.TestCase):
    def test_header_and_formatting_rules(self):
        text = emit_ue4ss.render_vtable_layout(sample_doc())
        self.assertEqual(text, "\n".join([
            "[NativeLayout]", "Format=1", "Target=prospero-x86_64-sysv", "PointerSize=8", "EngineMajor=5", "EngineMinor=3",
            "NonShipping=0", "Stats=0", "EngineRevision=5.3.2-7+++UE5", "BuildId=" + "ab" * 16, "",
            "[UObject]", "ProcessEvent=0x60", ""]))
        self.assertNotIn("\r", text)
        self.assertTrue(text.endswith("\n") and not text.endswith("\n\n"))

    def test_member_layout_uses_uppercase_hex_and_fixed_field_order(self):
        text = emit_ue4ss.render_member_layout(sample_doc())
        self.assertIn("[FUObjectArray]\nObjFirstGCIndex=0x0\nObjLastNonGCIndex=0x8\n", text)
        self.assertIn("\n\n[TUObjectArray]\nObjects=0x0\n", text)
        self.assertRegex(text, r"UEP_TotalSize=0x[0-9A-F]+\n")
        self.assertNotRegex(text, r"=0x[0-9a-f]*[a-f]")

    def test_a_class_is_all_or_nothing(self):
        doc = sample_doc()
        del doc["structs"]["FUObjectArray"]["PrimarySerialNumber"]
        text = emit_ue4ss.render_member_layout(doc)
        self.assertNotIn("[FUObjectArray]", text)
        self.assertIn("[TUObjectArray]", text)
        doc2 = sample_doc()
        doc2["structs"]["TUObjectArray"]["NumElements"]["confidence"] = "low"
        text = emit_ue4ss.render_member_layout(doc2)
        self.assertNotIn("[TUObjectArray]", text)
        self.assertIsNone(emit_ue4ss.render_member_layout(sample_doc(structs={})))

    def test_incomplete_identity_writes_nothing(self):
        doc = sample_doc()
        del doc["build"]["build_id"]
        self.assertIsNone(emit_ue4ss.identity_header(doc))
        self.assertIsNone(emit_ue4ss.render_vtable_layout(doc))
        self.assertIsNone(emit_ue4ss.render_member_layout(doc))
        doc = sample_doc()
        doc["build"]["engine"]["revision_string"]["confidence"] = "low"
        self.assertIsNone(emit_ue4ss.identity_header(doc))

    def test_vtable_file_is_identity_only_without_a_process_event_slot(self):
        doc = sample_doc()
        del doc["vtables"]
        text = emit_ue4ss.render_vtable_layout(doc)
        self.assertNotIn("[UObject]", text)
        self.assertTrue(text.rstrip().endswith("BuildId=" + "ab" * 16))

    def test_settings_fragment_pins_axes_only_when_high(self):
        text = emit_ue4ss.render_settings_fragment(sample_doc())
        self.assertIn("[EngineVersionOverride]\nMajorVersion=5\nMinorVersion=3\nDebugBuild=false\nStats=false\n", text)
        self.assertTrue(all(line.startswith(";") or not line or "=" in line or line.startswith("[") for line in text.splitlines()))
        doc = sample_doc()
        doc["build"]["stats"]["confidence"] = "medium"
        doc["build"]["configuration"]["confidence"] = "medium"
        text = emit_ue4ss.render_settings_fragment(doc)
        self.assertNotIn("DebugBuild", text)
        self.assertNotIn("Stats", text)
        self.assertIn("MajorVersion=5", text)
        doc = sample_doc()
        doc["build"]["configuration"]["value"] = "development"
        self.assertIn("DebugBuild=true", emit_ue4ss.render_settings_fragment(doc))

    def test_emit_writes_files_and_records_size_and_hash(self):
        with tempfile.TemporaryDirectory() as d:
            records = emit_ue4ss.emit(sample_doc(), d)
            self.assertEqual({r["path"] for r in records},
                             {"ue4ss/VTableLayout.ini", "ue4ss/MemberVariableLayout.ini", "ue4ss/UE4SS-settings.ini"})
            for r in records:
                data = Path(d, r["path"]).read_bytes()
                self.assertEqual(r["bytes"], len(data))
                self.assertEqual(r["sha256"], hashlib.sha256(data).hexdigest())


class ProfileAssemblyTests(unittest.TestCase):
    def make(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image(notes=[gnu_note(bytes(range(16)) + b"\0\0\0\0")]),
                       {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        platform.detect(ctx)
        return ctx

    def test_runtime_is_label_plus_base_with_the_weaker_confidence(self):
        ctx = self.make()
        ctx.put("symbols.thing.label", Hex(0x1234), "high", "t", ["x"])
        ctx.put("symbols.other.label", Hex(0x10), "low", "t", ["x"])
        ctx.put("symbols.thing.prologue_32", "aa", "high", "bytes", [])
        doc = profile.assemble(ctx, utc="2000-01-01T00:00:00Z")
        base = int(ctx.val("load.base"))
        self.assertEqual(doc["symbols"]["thing"]["runtime"]["value"], "0x%x" % (0x1234 + base))
        self.assertEqual(doc["symbols"]["thing"]["runtime"]["confidence"], "medium")      # base is a medium prediction
        self.assertEqual(doc["symbols"]["other"]["runtime"]["confidence"], "low")
        self.assertNotIn("runtime", {k for k in doc["symbols"]["thing"] if k != "runtime"})
        self.assertNotIn("prologue_32_runtime", doc["symbols"]["thing"])

    def test_every_leaf_of_the_data_sections_is_a_full_envelope(self):
        doc = profile.assemble(self.make())
        leaves = list(profile.iter_envelopes(doc))
        self.assertGreater(len(leaves), 20)
        for path, env in leaves:
            self.assertEqual(set(env), {"value", "confidence", "method", "evidence"}, path)
            self.assertIn(env["confidence"], ("high", "medium", "low"), path)
            self.assertIsInstance(env["method"], str, path)
            self.assertTrue(all(isinstance(e, str) for e in env["evidence"]), path)
        # nothing but envelopes below the data sections
        def check(node, path):
            if isinstance(node, dict):
                if "confidence" in node and "value" in node:
                    return
                for k, v in node.items():
                    check(v, path + [k])
            else:
                self.fail("bare value at %s" % ".".join(path))
        for s in profile.DATA_SECTIONS:
            if s in doc:
                check(doc[s], [s])

    def test_status_is_generated_and_verification_null_always(self):
        doc = profile.assemble(self.make())
        self.assertEqual(doc["status"], "generated")
        self.assertIsNone(doc["verification"])
        self.assertEqual(doc["schema"], "modbridge.runtime-profile/1")
        self.assertNotIn("console-verified", json.dumps(doc))

    def test_digest_ignores_status_timestamps_emit_and_unresolved_only(self):
        ctx = self.make()
        a = profile.assemble(ctx, utc="2000-01-01T00:00:00Z")
        b = profile.assemble(ctx, utc="2030-12-31T23:59:59Z", emitted=[{"path": "x", "bytes": 1, "sha256": "0"}])
        self.assertEqual(a["profile_digest"], b["profile_digest"])
        ctx.put("build.engine.major", Hex(5), "high", "t", [])
        c = profile.assemble(ctx, utc="2000-01-01T00:00:00Z")
        self.assertNotEqual(a["profile_digest"], c["profile_digest"])

    def test_a_path_that_is_both_leaf_and_section_is_refused(self):
        ctx = self.make()
        ctx.put("build.thing", 1, "high", "t", [])
        ctx.put("build.thing.sub", 1, "high", "t", [])
        with self.assertRaises(ValueError):
            profile.assemble(ctx)

    def test_confidence_helpers(self):
        self.assertEqual(weakest("high", "low", "medium"), "low")
        e = Envelope(Hex(255), "high", "m", ["e"]).to_json()
        self.assertEqual(e["value"], "0xff")
        self.assertEqual(Envelope(Hex(-32), "high", "m").to_json()["value"], "-0x20")

    def test_version_map_row_and_missing_pc_build(self):
        ctx = self.make()
        with tempfile.TemporaryDirectory() as d:
            Path(d, "PPSA00001.json").write_text(json.dumps({"builds": [{"console_content_version": "01.000.000", "pc_build": None}]}))
            ctx.options["game_profiles_dir"] = d
            doc = profile.assemble(ctx)
        self.assertEqual(doc["version_map"]["pc_build"]["confidence"], "low")
        self.assertIn("version_map.pc_build", [u["id"] for u in doc["unresolved"]])


class ValidateTests(unittest.TestCase):
    def test_compare_resolves_into_envelopes_and_reports_differences(self):
        doc = {"symbols": {"a": {"label": {"value": "0x10", "confidence": "high", "method": "m", "evidence": []}}},
               "elf": {"segments": {"ph0": {"value": {"p_flags": "0x1"}, "confidence": "high", "method": "m", "evidence": []}}}}
        oracle = {"symbols.a.label": {"value": "0x10"}, "symbols.a.runtime": {"value": "0x20"},
                  "elf.segments.ph0.p_flags": {"value": "0x01"}, "symbols.b.label": {"value": "0x1"}}
        rows = {r["key"]: r for r in validate.compare(doc, oracle, {"symbols.b.label": "why"})}
        self.assertEqual(rows["symbols.a.label"]["match"], "match")
        self.assertEqual(rows["elf.segments.ph0.p_flags"]["match"], "match")      # 0x1 == 0x01
        self.assertEqual(rows["symbols.b.label"]["match"], "absent")
        self.assertEqual(rows["symbols.b.label"]["note"], "why")
        doc["symbols"]["a"]["label"]["value"] = "0x11"
        self.assertEqual(validate.compare(doc, oracle)[0]["match"], "differs")
        self.assertEqual(validate.summary(validate.compare(doc, oracle))["differs"], 1)
        self.assertIn("| symbols.a.label |", validate.render_markdown(validate.compare(doc, oracle)))


# ---------------------------------------------------------------------------------------------
@needs_capstone
class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        im = Img()
        im.func("f").epilogue()
        self.eboot = os.path.join(self.tmp.name, "eboot.bin")
        Path(self.eboot).write_bytes(im.image(notes=[gnu_note(bytes(range(16)) + b"\0\0\0\0")]))
        self.param = os.path.join(self.tmp.name, "param.json")
        Path(self.param).write_text(json.dumps({"titleId": "PPSA00002", "contentVersion": "01.001.000"}))

    def run_cli(self, *extra):
        out = os.path.join(self.tmp.name, "out%d" % len(os.listdir(self.tmp.name)))
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(["--eboot", self.eboot, "--param", self.param, "--out", out, "--utc", "2000-01-01T00:00:00Z", *extra])
        return rc, out, buf.getvalue()

    def test_writes_a_profile_folder_and_never_claims_verification(self):
        rc, out, text = self.run_cli()
        self.assertEqual(rc, 0)
        doc = json.loads(Path(out, "runtime-profile.json").read_text())
        self.assertEqual(doc["status"], "generated")
        self.assertIsNone(doc["verification"])
        self.assertEqual(doc["binding"]["title_id"]["value"], "PPSA00002")
        self.assertEqual(doc["build"]["build_id"]["value"], bytes(range(16)).hex())
        self.assertIn("profilegen:", text)
        # a synthetic image has no engine facts: the identity header is incomplete, so no ini is written
        self.assertEqual(doc["emit"], [])
        self.assertIn("emit.VTableLayout", [u["id"] for u in doc["unresolved"]])
        self.assertTrue(Path(out, "evidence", "timing.json").exists())

    def test_output_is_byte_reproducible_with_a_fixed_timestamp(self):
        _rc, a, _ = self.run_cli()
        _rc, b, _ = self.run_cli()
        self.assertEqual(Path(a, "runtime-profile.json").read_bytes(), Path(b, "runtime-profile.json").read_bytes())

    def test_load_base_override_and_only_filter(self):
        rc, out, _ = self.run_cli("--load-base", "0x20000000", "--only", "platform")
        doc = json.loads(Path(out, "runtime-profile.json").read_text())
        self.assertEqual(doc["load"]["base"]["value"], "0x20000000")
        self.assertEqual(doc["load"]["base"]["confidence"], "high")
        self.assertNotIn("symbols", doc)

    def test_a_verified_profile_is_never_overwritten_silently(self):
        _rc, out, _ = self.run_cli()
        path = Path(out, "runtime-profile.json")
        doc = json.loads(path.read_text())
        doc["verification"] = {"receipt_id": "r1", "result": "pass"}
        path.write_text(json.dumps(doc))
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(["--eboot", self.eboot, "--param", self.param, "--out", out, "--quiet"])
        self.assertEqual(rc, 3)
        self.assertIn("verification", json.loads(path.read_text()))
        self.assertIsNotNone(json.loads(path.read_text())["verification"])
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(["--eboot", self.eboot, "--param", self.param, "--out", out, "--quiet", "--force"])
        self.assertEqual(rc, 0)
        self.assertIsNone(json.loads(path.read_text())["verification"])

    def test_unknown_detector_name_is_rejected(self):
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(["--eboot", self.eboot, "--out", os.path.join(self.tmp.name, "y"), "--only", "nope"])
        self.assertEqual(rc, 2)
        self.assertIn("unknown detector", buf.getvalue())

    def test_bad_input_is_a_clean_error(self):
        bad = os.path.join(self.tmp.name, "bad.bin")
        Path(bad).write_bytes(b"MZ" + b"\0" * 200)
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(["--eboot", bad, "--out", os.path.join(self.tmp.name, "x")])
        self.assertEqual(rc, 2)
        self.assertIn("not an ELF", buf.getvalue())

    def test_missing_capstone_degrades_to_unresolved_not_a_crash(self):
        # a detector that needs the decoder reports DetectorUnavailable; the run continues
        from tools.profilegen import detectors
        from tools.profilegen.model import DetectorUnavailable
        ctx = make_ctx(Path(self.eboot).read_bytes())
        orig = detectors.fname_ctor.detect

        def boom(c):
            raise DetectorUnavailable("capstone is not installed")
        detectors.ORDER = [(n, boom if n == "fname_ctor" else f) for n, f in detectors.ORDER]
        try:
            detectors.run_all(ctx)
        finally:
            detectors.ORDER = [(n, orig if n == "fname_ctor" else f) for n, f in detectors.ORDER]
        self.assertIn("detector.fname_ctor", [u.id for u in ctx.unresolved])
        self.assertIsNotNone(ctx.get("build.build_id"))


# =============================================================================================
# Layout detectors (class registration, object base, ProcessEvent members, CDO, FField classes, ...).
# Fixtures are assembled in memory with arbitrary numbers; the Townfall checks are in TownfallLayoutValidation.
# =============================================================================================
class LImg(Img):
    """Img whose assembler also has the generic member-access encodings."""

    def __init__(self) -> None:
        super().__init__()
        self.a = LAsm(CODE_VA)
        self.a.int3(1)

    def span(self, ctx, name):
        f = self.a.labels[name]
        return f, ctx.index.funcs.span(f)


def flow_of(builder, frame_args=True):
    """Assemble one function with ``builder(asm)`` and return its ``Flow``."""
    im = LImg()
    a = im.func("f")
    builder(a)
    a.epilogue()
    ctx = make_ctx(im.image())
    f = im.a.labels["f"]
    st, en = ctx.index.funcs.span(f)
    return _flow.Flow(ctx.index.decoder, ctx.index.decoder.decode_function(st, en), frame_args=frame_args), im.a.labels


@needs_capstone
class FlowTests(unittest.TestCase):
    def test_arguments_flow_into_stores_by_position(self):
        flow, _l = flow_of(lambda a: a.st(RDI, 0x20, RSI, 8).st(RDI, 0x28, RDX, 4).st_imm(RDI, 0x2C, -1, 4))
        got = {(e.disp, e.size): e.value for e in flow.stores() if e.base == ("arg", 0)}
        self.assertEqual(got, {(0x20, 8): ("arg", 1), (0x28, 4): ("arg", 2), (0x2C, 4): ("imm", 0xFFFFFFFF)})

    def test_register_copies_and_spills_keep_the_tag(self):
        def build(a):
            a.mov_rr(RBX, RDI).st(RBP, -0x40, RSI, 8).ld(RAX, RBP, -0x40, 8).st(RBX, 0x10, RAX, 8)
        flow, _l = flow_of(build)
        self.assertIn(((0x10, 8), ("arg", 1)), [((e.disp, e.size), e.value) for e in flow.stores() if e.base == ("arg", 0)])

    def test_stack_arguments_are_numbered_from_zero(self):
        flow, _l = flow_of(lambda a: a.ld(RCX, RBP, 0x10, 8).st(RDI, 8, RCX, 8).ld(RCX, RBP, 0x20, 8).st(RDI, 0x10, RCX, 8))
        got = {e.disp: e.value for e in flow.stores() if e.base == ("arg", 0)}
        self.assertEqual(got, {8: ("stk", 0), 0x10: ("stk", 2)})

    def test_call_results_and_out_parameters_are_tagged_by_their_call(self):
        def build(a):
            a.mov_rr(RBX, RDI).lea_rm(RDI, RBP, -0x38).call("callee").ld(RAX, RBP, -0x38, 8).st(RBX, 0x18, RAX, 8)
            a.call("callee").st(RBX, 0x20, RAX, 8)
        im = LImg()
        a = im.func("callee")
        a.epilogue()
        b = im.func("f")
        build(b)
        b.epilogue()
        ctx = make_ctx(im.image())
        f = im.a.labels["f"]
        st, en = ctx.index.funcs.span(f)
        flow = _flow.Flow(ctx.index.decoder, ctx.index.decoder.decode_function(st, en))
        callee = im.a.labels["callee"]
        vals = {e.disp: e.value for e in flow.stores() if e.base == ("arg", 0)}
        self.assertEqual(vals[0x18][0], "out")
        self.assertEqual(vals[0x18][2], callee)
        self.assertEqual(vals[0x20][0], "ret")

    def test_a_branch_target_after_an_exit_gets_the_state_of_the_branch(self):
        def build(a):
            a.test_rr(RDI, RDI).jcc(5, "other")
            a.call("g").ld(RAX, RSI, 4, 4).epilogue()                 # clobbers rsi on this path, then leaves
            a.label("other")
            a.ld(RAX, RSI, 8, 4)                                      # reached only by the branch: rsi is still argument 1
        im = LImg()
        im.func("g").epilogue()
        a = im.func("f")
        build(a)
        a.epilogue()
        ctx = make_ctx(im.image())
        st, en = ctx.index.funcs.span(im.a.labels["f"])
        flow = _flow.Flow(ctx.index.decoder, ctx.index.decoder.decode_function(st, en))
        loads = {e.disp: e.base for e in flow.events if e.kind == "load"}
        self.assertEqual(loads[8], ("arg", 1))
        self.assertNotEqual(loads[4], ("arg", 1))

    def test_callee_saved_registers_survive_an_epilogue_pop(self):
        def build(a):
            a.mov_rr(R15, RSI).pop(R15).ret()
            a.ld(RAX, R15, 0x30, 4)
        flow, _l = flow_of(build)
        self.assertEqual([e.base for e in flow.events if e.kind == "load"], [("arg", 1)])

    def test_indexed_stores_compares_masks_and_returns_are_events(self):
        def build(a):
            a.ld(RAX, RDI, 0x71, 1).ld(RCX, RDI, 0x73, 1).not_r8(RCX).and_mem8_reg(RSI, RAX, RCX)
            a.ld(RDX, RSI, 0x40, 2).cmp_r_imm(RDX, 0xFFFF).mov_rr(RAX, RDI)
        flow, _l = flow_of(build)
        idx = [e for e in flow.events if e.kind == "idxmem"]
        self.assertEqual(len(idx), 1)
        self.assertEqual(idx[0].base, ("arg", 1))
        self.assertEqual(idx[0].value, ("ld", ("arg", 0), 0x71, 1))
        self.assertEqual(idx[0].target, ("ld", ("arg", 0), 0x73, 1))
        cmpreg = [e for e in flow.events if e.kind == "cmpreg"]
        self.assertEqual((cmpreg[0].base, cmpreg[0].disp), (("ld", ("arg", 1), 0x40, 2), 0xFFFF))
        self.assertEqual([e.value for e in flow.events if e.kind == "ret"], [("arg", 0)])

    def test_mask_events_need_a_wide_constant(self):
        def build(a):
            a.ld(RAX, RSI, 8, 8).mov_r64_imm(R10, 0x8000002008001).raw(b"\x4c\x21\xd0")      # and rax, r10
            a.ld(RCX, RSI, 16, 8).raw(b"\x48\x83\xe1\x01")                                    # and rcx, 1
        flow, _l = flow_of(build)
        masks = [e for e in flow.events if e.kind == "mask"]
        self.assertEqual(len(masks), 1)
        self.assertEqual((masks[0].base, masks[0].disp), (("ld", ("arg", 1), 8, 8), 0x8000002008001))


class BookTests(unittest.TestCase):
    def ctx(self):
        return Context(None, None)

    def test_two_independent_channels_agree_is_high(self):
        ctx = self.ctx()
        b = _book.book(ctx)
        b.observe("structs.X.A", 0x18, "chan1", "m1", ["e1"], "medium")
        b.observe("structs.X.A", 0x18, "chan2", "m2", ["e2"], "medium")
        _book.resolve(ctx)
        env = ctx.get("structs.X.A")
        self.assertEqual((int(env.value), env.confidence), (0x18, "high"))
        self.assertEqual(env.method, "m1+m2")
        self.assertTrue(any("2 independent channels" in e for e in env.evidence))

    def test_one_channel_is_medium_unless_it_is_a_solo_invariant(self):
        ctx = self.ctx()
        b = _book.book(ctx)
        b.observe("structs.X.A", 8, "c", "m", [], "medium")
        b.observe("structs.X.B", 8, "c", "m", [], "high")
        b.observe("structs.X.C", 8, "c", "m", [], "high", solo_high=True)
        b.observe("structs.X.D", 8, "c", "m", [], "low")
        b.observe("structs.X.D", 8, "c2", "m", [], "low")
        _book.resolve(ctx)
        self.assertEqual([ctx.get("structs.X." + k).confidence for k in "ABCD"], ["medium", "medium", "high", "low"])

    def test_the_same_channel_twice_is_still_one_channel(self):
        ctx = self.ctx()
        b = _book.book(ctx)
        b.observe("structs.X.A", 8, "c", "m", [], "medium")
        b.observe("structs.X.A", 8, "c", "m", [], "medium")
        _book.resolve(ctx)
        self.assertEqual(ctx.get("structs.X.A").confidence, "medium")

    def test_disagreement_publishes_nothing_and_says_why(self):
        ctx = self.ctx()
        b = _book.book(ctx)
        b.observe("structs.X.A", 0x10, "c1", "m", [], "medium")
        b.observe("structs.X.A", 0x18, "c2", "m", [], "medium")
        _book.resolve(ctx)
        self.assertIsNone(ctx.get("structs.X.A"))
        why = [u.why for u in ctx.unresolved if u.id == "structs.X.A"]
        self.assertEqual(len(why), 1)
        self.assertIn("0x10 from c1", why[0])
        self.assertIn("0x18 from c2", why[0])

    def test_expected_paths_without_a_measurement_are_unresolved_with_a_reason(self):
        ctx = self.ctx()
        b = _book.book(ctx)
        b.expect(["structs.X.A", "structs.X.B"], "generic reason")
        b.why_not("structs.X.B", "specific reason")
        _book.resolve(ctx)
        reasons = {u.id: u.why for u in ctx.unresolved}
        self.assertEqual(reasons, {"structs.X.A": "generic reason", "structs.X.B": "specific reason"})

    def test_an_envelope_written_by_another_detector_joins_as_a_channel(self):
        ctx = self.ctx()
        ctx.put("structs.X.A", Hex(0x20), "medium", "other.method", ["seen elsewhere"])
        b = _book.book(ctx)
        b.observe("structs.X.A", 0x20, "mine", "mine.method", [], "medium")
        b.expect(["structs.X.A", "structs.X.Z"], "r")
        ctx.put("structs.X.Z", Hex(4), "medium", "other", [])
        _book.resolve(ctx)
        self.assertEqual(ctx.get("structs.X.A").confidence, "high")
        self.assertEqual([u.id for u in ctx.unresolved], [])            # Z already has an envelope: not reported

    def test_resolving_twice_changes_nothing(self):
        ctx = self.ctx()
        b = _book.book(ctx)
        b.observe("structs.X.A", 8, "c", "m", [], "medium")
        b.expect(["structs.X.B"], "r")
        _book.resolve(ctx)
        _book.resolve(ctx)
        self.assertEqual(len(ctx.unresolved), 1)
        self.assertEqual(ctx.get("structs.X.A").confidence, "medium")


class ClassRegImage:
    """A class-registration helper and its call sites; every offset and size is arbitrary."""

    PS, MA, CF, CCF, CW, CN = 0x68, 0x6C, 0xA0, 0xA8, 0xB8, 0xC0       # where the helper stores its arguments
    ALLOC = 0x2C0                                                      # byte count it passes to the allocator
    OBJECT, FIELD, STRUCT = 0x30, 0x38, 0xA0                           # sizeof of three core classes
    FUNCTION = 0xD8

    def __init__(self, modules=12, per_module=10, body=True, inline=True, second_helper=False, dup_object=False):
        im = self.im = LImg()
        self.pkgs = [im.ro_str("/Script/Mod%s" % chr(65 + i), wide=True) for i in range(modules)]
        self.core = im.ro_str("/Script/CoreUObject", wide=True)
        self.engine = im.ro_str("/Script/Engine", wide=True)
        lit = {n: im.ro_str(n, wide=True) for n in ("Object", "Field", "Struct", "Function", "Actor")}
        a = im.func("fname_ctor")
        a.epilogue()
        im.func("alloc").epilogue()
        a = im.func("H")
        a.push(R12).push(R13).push(R15).mov_rr32(R13, R8).mov_rr32(R15, R9)
        a.mov_r32_imm(RSI, self.ALLOC).call("alloc").mov_rr(R12, RAX)
        if body:
            a.st(R12, self.PS, R13, 4).st(R12, self.MA, R15, 2)
            a.ld(RCX, RBP, 0x18, 8).st(R12, self.CCF, RCX, 8)
            a.mov_r32_imm(RDX, 0x80).or_r32_mem(RDX, RBP, 0x10).st(R12, self.CF, RDX, 4)
            a.call_mem(RBP, 0x48).mov_rr(R15, RAX).st(R12, self.CW, R15, 8)
            a.ld(RSI, RBP, 0x20, 8).lea_rm(RDI, R12, self.CN).mov_r32_imm(RDX, 1).call("fname_ctor")
        a.pop(R15).pop(R13).pop(R12).epilogue()
        if second_helper:
            a = im.func("H2")
            a.push(R12).mov_rr(R12, RDI).pop(R12).epilogue()
        rows = [(self.core, "Object", self.OBJECT), (self.core, "Field", self.FIELD), (self.core, "Struct", self.STRUCT),
                (self.engine, "Actor", 0x2D0)]
        if dup_object:
            rows.append((self.core, "Object", self.OBJECT + 8))
        for k in range(modules * per_module):
            pkg = self.pkgs[k % modules]
            rows.append((pkg, "Actor", 0x40 + 8 * (k % 5)))
        self.static = im.bss(8)
        for k, (pkg, name, size) in enumerate(rows):
            a = im.func("reg%d" % k)
            a.lea_rip(RDI, pkg).lea_rip(RSI, lit[name]).lea_rip(RDX, self.static)
            a.mov_r32_imm(R8, size).mov_r32_imm(R9, 8)
            a.call("H2" if (second_helper and k % 2) else "H").epilogue()
        if inline:
            a = im.func("inline_function")                       # an open-coded registration of the class named Function
            a.lea_rip(RSI, lit["Function"]).mov_rr(RBX, RAX)
            a.st_imm(RBX, self.PS, self.FUNCTION, 4).st_imm(RBX, self.MA, 8, 2).epilogue()
        self.labels = im.a.labels
        self.data = im.image()

    def run(self, fname_ctor=True):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        if fname_ctor:
            ctx.facts["fname_ctor"] = self.labels["fname_ctor"]
        class_registration.detect(ctx)
        layout_merge.detect(ctx)
        return ctx


@needs_capstone
class ClassRegistrationTests(unittest.TestCase):
    def test_the_helper_is_the_call_target_the_modules_agree_on(self):
        fx = ClassRegImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("symbols.class_registration_helper.label")), fx.labels["H"])
        self.assertEqual(ctx.get("symbols.class_registration_helper.label").confidence, "high")
        self.assertEqual(ctx.facts["class_registration_helper"], fx.labels["H"])

    def test_the_helper_body_says_where_each_argument_is_stored(self):
        fx = ClassRegImage()
        ctx = fx.run()
        want = {"structs.UStruct.PropertiesSize": fx.PS, "structs.UStruct.MinAlignment": fx.MA, "structs.UClass.ClassFlags": fx.CF,
                "structs.UClass.ClassCastFlags": fx.CCF, "structs.UClass.ClassWithin": fx.CW, "structs.UClass.ClassConfigName": fx.CN}
        for path, off in want.items():
            self.assertEqual(int(ctx.val(path)), off, path)
        self.assertEqual(ctx.get("structs.UClass.ClassFlags").confidence, "medium")        # one channel

    def test_open_coded_registrations_are_a_second_channel_for_the_size_offsets(self):
        ctx = ClassRegImage().run()
        self.assertEqual(ctx.get("structs.UStruct.PropertiesSize").confidence, "high")
        self.assertEqual(ctx.get("structs.UStruct.MinAlignment").confidence, "high")
        no_inline = ClassRegImage(inline=False).run()
        self.assertEqual(no_inline.get("structs.UStruct.PropertiesSize").confidence, "medium")

    def test_class_sizes_come_from_the_size_argument_of_each_registration(self):
        fx = ClassRegImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.UField.UEP_TotalSize")), fx.FIELD)
        self.assertEqual(int(ctx.val("structs.UStruct.UEP_TotalSize")), fx.STRUCT)
        self.assertEqual(int(ctx.val("structs.AActor.UEP_TotalSize")), 0x2D0)
        self.assertEqual(int(ctx.val("structs.UObject.sizeof")), fx.OBJECT)
        self.assertEqual(int(ctx.val("structs.UFunction.UEP_TotalSize")), fx.FUNCTION)            # from the open-coded copy
        self.assertEqual(ctx.get("structs.UField.UEP_TotalSize").confidence, "high")
        self.assertEqual(int(ctx.val("structs.UClass.UEP_TotalSize")), fx.ALLOC)
        self.assertEqual(ctx.get("structs.UClass.UEP_TotalSize").confidence, "medium")           # only the allocation size

    def test_a_class_registered_with_two_different_sizes_has_no_size(self):
        ctx = ClassRegImage(dup_object=True).run()
        self.assertIsNone(ctx.get("structs.UObject.sizeof"))

    def test_without_package_literals_nothing_is_invented(self):
        im = LImg()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        class_registration.detect(ctx)
        layout_merge.detect(ctx)
        self.assertIsNone(ctx.get("symbols.class_registration_helper.label"))
        ids = {u.id for u in ctx.unresolved}
        self.assertIn("symbols.class_registration_helper", ids)
        self.assertIn("structs.UStruct.PropertiesSize", ids)

    def test_two_helpers_with_equal_votes_are_not_a_helper(self):
        ctx = ClassRegImage(second_helper=True).run()
        self.assertIsNone(ctx.get("symbols.class_registration_helper.label"))

    def test_a_helper_that_stores_none_of_its_arguments_leaves_the_members_unresolved(self):
        fx = ClassRegImage(body=False, inline=False)
        ctx = fx.run()
        self.assertEqual(int(ctx.val("symbols.class_registration_helper.label")), fx.labels["H"])
        self.assertIsNone(ctx.get("structs.UStruct.PropertiesSize"))
        self.assertIn("stores none of its arguments", [u.why for u in ctx.unresolved if u.id == "structs.UStruct.PropertiesSize"][0])
        self.assertEqual(int(ctx.val("structs.UField.UEP_TotalSize")), fx.FIELD)         # the call-site sizes need no body

    def test_the_config_name_needs_the_fname_constructor(self):
        ctx = ClassRegImage().run(fname_ctor=False)
        self.assertIsNone(ctx.get("structs.UClass.ClassConfigName"))
        self.assertIsNotNone(ctx.get("structs.UClass.ClassFlags"))


class ObjectBaseImage:
    """FUObjectArray::AllocateUObjectIndex, a UObjectBase constructor and a second constructor variant."""

    FLAGS, IDX, CLS, NAME, OUTER = 0x0C, 0x10, 0x14, 0x20, 0x28        # UObjectBase members (arbitrary)
    ITEM_OBJ, ITEM_FLAGS, ITEM_SERIAL = 0x0, 0x10, 0x18               # FUObjectItem members
    ITEM_SIZE = 0x20

    def __init__(self, variant=True, primary=True, messages=True, omit_index_store=False, two_index_stores=False):
        im = self.im = LImg()
        self.G = im.bss(0x100)
        path = im.ro_str(r".\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp")
        m1 = im.ro_str("Maximum number of UObjects (%d) exceeded when trying to add", wide=True)
        m2 = im.ro_str("Unable to add more objects to disregard for GC pool (Max: %d)", wide=True)
        m3 = im.ro_str("Attempting to add %s at index %d but another object (0x%016llx) exists", wide=True)
        im.func("fname_ctor").epilogue()
        a = im.func("alloc_index")                                # (this = global, Object, Flags, Index, Serial)
        a.push(R12).push(R13).push(RBX).mov_rr(RBX, RSI).mov_rr32(R13, R8).mov_rr32(R15, RCX)
        if messages:
            a.lea_rip(RAX, path).lea_rip(RAX, m1).lea_rip(RAX, m2).lea_rip(RAX, m3)
        a.ld(R12, RDI, 0x40, 8)                                   # the item (address computed from the chunk table)
        a.st(R12, self.ITEM_OBJ, RBX, 8).st(R12, self.ITEM_SERIAL, R13, 4)
        a.raw(b"\xf0")
        a._op(b"\x0f\xb1", RCX, R12, self.ITEM_FLAGS, 0)          # lock cmpxchg [item+Flags], ecx
        if not omit_index_store:
            a.st(RBX, self.IDX, R15, 4)
        if two_index_stores:
            a.st(RBX, self.IDX + 0x40, R13, 4)                    # a second dword store into the object: which one is the index?
        a.pop(RBX).pop(R13).pop(R12).epilogue()
        if primary:
            a = im.func("ctor")                                    # (this, Class, Flags, InternalFlags, Outer, Name)
            a.push(RBX).mov_rr(RBX, RDI)
            a.st(RBX, self.FLAGS, RDX, 4).st_imm(RBX, self.IDX, -1, 4).st(RBX, self.CLS, RSI, 8)
            a.st(RBX, self.OUTER, R8, 8).st(RBX, self.NAME, R9, 8)
            a.lea_rip(RDI, self.G).mov_rr(RSI, RBX).call("alloc_index").pop(RBX).epilogue()
        if variant:
            a = im.func("ctor_name")                               # (this, Class, ..., const TCHAR* name)
            a.push(RBX).mov_rr(RBX, RDI).st(RBX, self.CLS, RSI, 8).st_imm(RBX, self.IDX, -1, 4)
            a.lea_rm(RDI, RBP, -0x20).mov_r32_imm(RDX, 1).call("fname_ctor").ld(RAX, RBP, -0x20, 8).st(RBX, self.NAME, RAX, 8)
            a.lea_rip(RDI, self.G).mov_rr(RSI, RBX).call("alloc_index").pop(RBX).epilogue()
        self.labels = im.a.labels
        self.data = im.image()

    def run(self, uobject_size=0x30, stride=0x20):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        ctx.facts["fname_ctor"] = self.labels["fname_ctor"]
        if uobject_size is not None:
            ctx.facts["uobject_size"] = uobject_size
        if stride is not None:
            ctx.put("structs.FUObjectItem.stride", Hex(stride), "medium", "uobject_array.index_to_object_idiom", ["57 sites"])
        uobject_base.detect(ctx)
        layout_merge.detect(ctx)
        return ctx


@needs_capstone
class UObjectBaseTests(unittest.TestCase):
    def test_constructor_stores_follow_the_arguments(self):
        fx = ObjectBaseImage()
        ctx = fx.run()
        for key, off in (("ObjectFlags", fx.FLAGS), ("InternalIndex_Private", fx.IDX), ("ClassPrivate", fx.CLS), ("NamePrivate", fx.NAME),
                         ("OuterPrivate", fx.OUTER)):
            self.assertEqual(int(ctx.val("structs.UObjectBase." + key)), off, key)

    def test_a_second_constructor_and_the_allocator_corroborate(self):
        ctx = ObjectBaseImage().run()
        self.assertEqual(ctx.get("structs.UObjectBase.ClassPrivate").confidence, "high")           # both constructors
        self.assertEqual(ctx.get("structs.UObjectBase.NamePrivate").confidence, "high")            # FName-constructor result store
        self.assertEqual(ctx.get("structs.UObjectBase.InternalIndex_Private").confidence, "high")  # constructors + allocator store
        self.assertEqual(ctx.get("structs.UObjectBase.ObjectFlags").confidence, "medium")          # only the primary constructor
        no_variant = ObjectBaseImage(variant=False, omit_index_store=True).run()
        self.assertEqual(no_variant.get("structs.UObjectBase.ClassPrivate").confidence, "medium")

    def test_an_allocator_that_stores_two_dwords_into_the_object_gives_no_index_channel(self):
        fx = ObjectBaseImage(two_index_stores=True)
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.UObjectBase.InternalIndex_Private")), fx.IDX)
        self.assertEqual(ctx.get("structs.UObjectBase.InternalIndex_Private").confidence, "high")      # the two constructors still agree
        only_primary = ObjectBaseImage(variant=False, two_index_stores=True).run()
        self.assertEqual(only_primary.get("structs.UObjectBase.InternalIndex_Private").confidence, "medium")

    def test_allocator_fills_the_item(self):
        fx = ObjectBaseImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.FUObjectItem.Object")), fx.ITEM_OBJ)
        self.assertEqual(int(ctx.val("structs.FUObjectItem.Flags")), fx.ITEM_FLAGS)
        self.assertEqual(int(ctx.val("structs.FUObjectItem.SerialNumber")), fx.ITEM_SERIAL)
        self.assertEqual(int(ctx.val("structs.FUObjectItem.UEP_TotalSize")), fx.ITEM_SIZE)
        self.assertEqual(ctx.get("structs.FUObjectItem.Object").method, "uobject_base.allocator_stores")

    def test_the_total_is_the_object_size_only_when_the_members_end_there(self):
        fx = ObjectBaseImage()
        end = fx.OUTER + 8
        ctx = fx.run(uobject_size=end)
        self.assertEqual(int(ctx.val("structs.UObjectBase.UEP_TotalSize")), end)
        self.assertEqual(ctx.get("structs.UObjectBase.UEP_TotalSize").confidence, "high")
        bigger = fx.run(uobject_size=end + 8)
        self.assertEqual(bigger.get("structs.UObjectBase.UEP_TotalSize").confidence, "low")      # UObject may add members
        unknown = fx.run(uobject_size=None)
        self.assertIsNone(unknown.get("structs.UObjectBase.UEP_TotalSize"))

    def test_without_the_fatal_messages_there_is_no_allocator_and_no_guess(self):
        ctx = ObjectBaseImage(messages=False).run()
        self.assertIsNone(ctx.get("symbols.allocate_uobject_index.label"))
        self.assertIsNone(ctx.get("structs.UObjectBase.ObjectFlags"))
        self.assertTrue(any(u.id == "structs.UObjectBase.ObjectFlags" for u in ctx.unresolved))

    def test_only_the_variant_constructor_is_not_enough_for_the_flag_and_outer_members(self):
        ctx = ObjectBaseImage(primary=False).run()
        self.assertIsNone(ctx.get("structs.UObjectBase.ObjectFlags"))
        self.assertIsNone(ctx.get("structs.UObjectBase.OuterPrivate"))
        self.assertIn("no caller", [u.why for u in ctx.unresolved if u.id == "structs.UObjectBase.OuterPrivate"][0])


class ProcessEventShape:
    """A ProcessEvent-shaped function: the loads, tests, copies and loops of UE's ProcessEvent with arbitrary offsets."""

    OBJFLAGS, OUTER = 0x14, 0x30                       # UObject members read through this / the function
    FF, SCRIPT = 0x98, 0x70                            # UFunction::FunctionFlags, UStruct::Script (Num at +8)
    EG, EGOFF = 0xD8, 0xE0                             # EventGraphFunction, EventGraphCallOffset
    PS, MA, PARMS, RET = 0x64, 0x68, 0xC4, 0xC6        # PropertiesSize, MinAlignment, ParmsSize, ReturnValueOffset
    CHILD, DTOR, INIT, FUNC = 0x58, 0x90, 0xD0, 0xE8   # ChildProperties, DestructorLink, FirstPropertyToInit, Func
    NEXT, PFLAGS, OFFSET, ADIM, ESIZE = 0x20, 0x40, 0x4C, 0x34, 0x38
    DNEXT, PCNEXT = 0x60, 0x68                         # FProperty::DestructorLinkNext / PostConstructLinkNext
    V_INIT, V_DESTROY, V_OUTER = 0x150, 0x168, 0x2A0   # virtual slots called on properties / the outer

    def __init__(self, guard=True, destructor_loop=True, init_loop=True, parms_copy=True, func_call=True, ret_check=True, alias_eg=False):
        im = self.im = LImg()
        for n in ("memset", "memcpy", "frame_ctor"):
            im.func(n).epilogue()
        a = im.func("invoke")                                   # UFunction::Invoke(Function, Context, Frame, Result)
        a.push(R15).mov_rr(R15, RDI).mov_rr(RDX, RSI).call_mem(R15, self.FUNC).pop(R15).epilogue()
        a = im.func("pe")
        a.push(R12).push(R13).push(R14).push(R15).push(RBX).mov_rr(RBX, RSP).mov_rr(R12, RDX)
        if guard:
            a.ld(RAX, RDI, self.OBJFLAGS, 4).test_r_imm(RAX, 0x20000000)
        a.test_mem_imm(RSI, self.FF + 1, 4, 1).cmp_mem_imm(RSI, self.SCRIPT + 8, 0, 4)
        # event-graph redirection
        a.ld(R15, RSI, self.EG, 8).test_rr(R15, R15).ld(RAX, RSI, self.EGOFF, 4).st(RBX, 0x3C, RAX, 4)
        a.test_mem_imm(R15, self.FF + 1, 0x80, 1)
        if not alias_eg:
            a.mov_rr(R15, RSI)                                    # the function itself (the other path)
        a.ld(RDI, R15, self.OUTER, 8).mov_rr(RDX, R15).ld(RAX, RDI, 0, 8).call_mem(RAX, self.V_OUTER)
        # frame allocation, parameter copy
        a.ldx(RDX, R15, self.PS, 4).ldx(RAX, R15, self.MA, 2).cmp_r_imm(RAX, 16, 0)
        if parms_copy:
            a.ld(RAX, R15, self.PARMS, 2).raw(b"\x48\x29\xc2")      # sub rdx, rax
            a.xor_r32(RSI).mov_rr(RDI, R13).call("memset")
            a.ld(RDX, R15, self.PARMS, 2).mov_rr(RDI, R13).mov_rr(RSI, R12).call("memcpy")
        a.ld(R12, R15, self.CHILD, 8).st(RBX, 0xE0, R12, 8)       # ChildProperties -> frame
        a.ld(RAX, R15, self.SCRIPT, 8).st(RBX, 0x78, RAX, 8)      # Script data -> frame code
        # the out-parameter loop over ChildProperties
        a.ld(RCX, R15, self.CHILD, 8).ld(RCX, RCX, self.NEXT, 8)
        a.ld(RSI, RCX, self.PFLAGS, 8).test_r_imm(RSI, 0x100).ldx(RDI, RCX, self.OFFSET, 4)
        if ret_check:
            a.ld(RAX, R15, self.RET, 2).cmp_r_imm(RAX, 0xFFFF)
        if func_call:
            a.mov_rr(RDI, R15).mov_rr(RSI, R14).call("invoke")
        if init_loop:
            a.ld(R13, R15, self.INIT, 8).ldx(RAX, R13, self.ESIZE, 4).ldx(RDX, R13, self.ADIM, 4).imul_rr(RDX, RAX)
            a.xor_r32(RSI).call("memset")
            a.ld(R13, R13, self.PCNEXT, 8).ldx(RSI, R13, self.OFFSET, 4).test_mem_imm(R13, self.PFLAGS + 1, 2, 1)
            a.mov_rr(RDI, R13).ld(RAX, R13, 0, 8).call_mem(RAX, self.V_INIT)
        if destructor_loop:
            a.ld(R12, R15, self.DTOR, 8).ld(R12, R12, self.DNEXT, 8)
            a.ldx(RAX, R12, self.ADIM, 4).ldx(RDX, R12, self.ESIZE, 4).ldx(RSI, R12, self.OFFSET, 4).ld(RCX, R15, self.PARMS, 2)
            a.ld(RAX, R12, self.PFLAGS, 8).mov_rr(RDI, R12).ld(RAX, R12, 0, 8).call_mem(RAX, self.V_DESTROY)
        a.pop(RBX).pop(R15).pop(R14).pop(R13).pop(R12).epilogue()
        self.labels = im.a.labels
        self.data = im.image()

    def run(self):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        ctx.put("structs.UFunction.FunctionFlags", Hex(self.FF), "medium", "process_event.native_test", [])
        ctx.facts["process_event"] = (12, self.labels["pe"])
        process_event_members.detect(ctx)
        layout_merge.detect(ctx)
        return ctx


@needs_capstone
class ProcessEventMembersTests(unittest.TestCase):
    def test_every_member_is_found_by_the_role_of_its_access(self):
        s = ProcessEventShape()
        ctx = s.run()
        want = {"structs.UObjectBase.ObjectFlags": s.OBJFLAGS, "structs.UObjectBase.OuterPrivate": s.OUTER, "structs.UStruct.Script": s.SCRIPT,
                "structs.UStruct.PropertiesSize": s.PS, "structs.UStruct.MinAlignment": s.MA, "structs.UStruct.ChildProperties": s.CHILD,
                "structs.UStruct.DestructorLink": s.DTOR, "structs.UFunction.ParmsSize": s.PARMS, "structs.UFunction.ReturnValueOffset": s.RET,
                "structs.UFunction.EventGraphFunction": s.EG, "structs.UFunction.EventGraphCallOffset": s.EGOFF,
                "structs.UFunction.FirstPropertyToInit": s.INIT, "structs.UFunction.Func": s.FUNC, "structs.FField.Next": s.NEXT,
                "structs.FProperty.ArrayDim": s.ADIM, "structs.FProperty.ElementSize": s.ESIZE, "structs.FProperty.PropertyFlags": s.PFLAGS,
                "structs.FProperty.Offset_Internal": s.OFFSET, "structs.FProperty.DestructorLinkNext": s.DNEXT,
                "structs.FProperty.PostConstructLinkNext": s.PCNEXT, "vtables.FProperty.DestroyValueInternal": s.V_DESTROY,
                "vtables.FProperty.InitializeValueInternal": s.V_INIT}
        for path, off in want.items():
            self.assertEqual(int(ctx.val(path)), off, path)
            self.assertEqual(ctx.get(path).confidence, "medium", path)               # one channel
        self.assertEqual(ctx.val("structs.UFunction.FunctionFlags"), Hex(s.FF))

    def test_declared_order_fits_apply_only_where_the_gap_is_exactly_as_large_as_the_declaration_needs(self):
        s = ProcessEventShape()
        ctx = s.run()
        # Offset_Internal + 4 .. DestructorLinkNext is exactly two pointers in this fixture: PropertyLinkNext and NextRef
        self.assertEqual(int(ctx.val("structs.FProperty.PropertyLinkNext")), s.OFFSET + 4)
        self.assertEqual(int(ctx.val("structs.FProperty.NextRef")), s.OFFSET + 12)
        self.assertEqual(ctx.get("structs.FProperty.NextRef").method, "layout_merge.declared_order_fit")
        # ParmsSize is not at FunctionFlags + 6 here, and SuperStruct was never measured: nothing is invented for these
        for path in ("structs.UFunction.NumParms", "structs.UStruct.Children"):
            self.assertIsNone(ctx.get(path), path)

    def test_the_redirected_function_and_the_function_are_one_kind_of_object(self):
        # the compiler may keep the event-graph pointer in the register the rest of the function uses
        plain, aliased = ProcessEventShape().run(), ProcessEventShape(alias_eg=True).run()
        for path in ("structs.UStruct.Script", "structs.UStruct.PropertiesSize", "structs.UStruct.ChildProperties", "structs.UFunction.ParmsSize",
                     "structs.UStruct.DestructorLink", "structs.UFunction.Func", "structs.FProperty.Offset_Internal"):
            self.assertEqual(int(aliased.val(path)), int(plain.val(path)), path)

    def test_property_chains_are_told_apart_by_what_the_loop_does(self):
        s = ProcessEventShape()
        ctx = s.run()
        self.assertEqual(int(ctx.val("structs.UStruct.DestructorLink")), s.DTOR)           # reads ParmsSize inside the loop
        self.assertEqual(int(ctx.val("structs.UFunction.FirstPropertyToInit")), s.INIT)    # does not
        swapped = ProcessEventShape(destructor_loop=False)
        ctx = swapped.run()
        self.assertIsNone(ctx.get("structs.UStruct.DestructorLink"))
        self.assertIsNone(ctx.get("structs.UFunction.FirstPropertyToInit"))                # two chains are needed to tell them apart
        self.assertEqual(int(ctx.val("structs.UStruct.ChildProperties")), swapped.CHILD)

    def test_missing_shapes_stay_unresolved(self):
        for kw, path in (({"guard": False}, "structs.UObjectBase.ObjectFlags"), ({"parms_copy": False}, "structs.UFunction.ParmsSize"),
                         ({"parms_copy": False}, "structs.UStruct.PropertiesSize"), ({"func_call": False}, "structs.UFunction.Func"),
                         ({"ret_check": False}, "structs.UFunction.ReturnValueOffset")):
            ctx = ProcessEventShape(**kw).run()
            self.assertIsNone(ctx.get(path), (kw, path))
            self.assertTrue(any(u.id == path for u in ctx.unresolved), (kw, path))

    def test_without_process_event_everything_is_unresolved(self):
        ctx = make_ctx(ProcessEventShape().data)
        process_event_members.detect(ctx)
        layout_merge.detect(ctx)
        self.assertIsNone(ctx.get("structs.UStruct.Script"))
        why = [u.why for u in ctx.unresolved if u.id == "structs.UStruct.Script"]
        self.assertIn("ProcessEvent was not located", why[0])


class CdoImage:
    """A UClass vtable run whose slot K is a CreateDefaultObject-shaped function."""

    CDO, SUPER, CHILD, NEXT, CLS, CAST = 0x138, 0x48, 0x58, 0x20, 0x10, 0x18       # arbitrary offsets
    K, SLOTS = 9, 20

    def __init__(self, flags=0x31, second=False, loop=True, store=True):
        im = self.im = LImg()
        im.func("static_alloc").epilogue()
        im.func("stub").epilogue()
        self.typeinfo = im.ro_str("typeinfo")
        a = im.func("cdo")
        a.push(RBX).push(R12).push(R14).push(R15).mov_rr(RBX, RDI)
        a.cmp_mem_imm(RBX, self.CDO, 0, 8)
        a.ld(R15, RBX, self.SUPER, 8).ld(R14, R15, self.CDO, 8)
        a.mov_r32_imm(RCX, flags).call("static_alloc")
        if store:
            a.st(RBX, self.CDO, RAX, 8)
        if loop:
            a.ld(R12, RBX, self.CHILD, 8).ld(R12, R12, self.NEXT, 8)
            a.ld(RCX, R12, self.CLS + 0x60, 8).ld(RCX, RCX, self.CAST + 0xD0, 8)                   # a two-level load that is never masked
            a.ld(RAX, R12, self.CLS, 8).ld(RAX, RAX, self.CAST, 8).mov_r64_imm(R10, 0x2400000A0008001).raw(b"\x4c\x21\xd0")   # and rax, r10
        a.ld(RAX, RBX, self.CDO, 8).nops(0x100).pop(R15).pop(R14).pop(R12).pop(RBX).epilogue()
        if second:
            a = im.func("cdo2")
            a.mov_rr(RBX, RDI).mov_r32_imm(RCX, 0x31).call("static_alloc").st(RBX, self.CDO + 8, RAX, 8)
            a.raw(b"\x90" * 0x100).epilogue()
        labels = im.a.labels
        relas = [(RW_VA + 8, RO_VA + 16)]
        base = RW_VA + 16
        self.vptr = base
        for k in range(self.SLOTS):
            fn = labels["cdo"] if k == self.K else (labels["cdo2"] if (second and k == self.K + 3) else labels["stub"])
            relas.append((base + 8 * k, fn))
        im.relas = relas
        self.labels = labels
        self.data = im.image()

    def run(self):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        ctx.facts["uclass_vtable"] = self.vptr
        class_default_object.detect(ctx)
        layout_merge.detect(ctx)
        return ctx


@needs_capstone
class ClassDefaultObjectTests(unittest.TestCase):
    def test_the_function_that_stores_a_cdo_allocation_back_into_this_is_found(self):
        fx = CdoImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.UClass.ClassDefaultObject")), fx.CDO)
        self.assertEqual(int(ctx.val("vtables.UClass.CreateDefaultObject")), fx.K * 8)
        self.assertEqual(int(ctx.val("symbols.create_default_object.label")), fx.labels["cdo"])

    def test_the_parent_class_pointer_is_the_one_whose_cdo_is_read(self):
        fx = CdoImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.UStruct.SuperStruct")), fx.SUPER)

    def test_the_field_loop_gives_the_chain_link_and_the_masked_class_flags(self):
        fx = CdoImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.UStruct.ChildProperties")), fx.CHILD)
        self.assertEqual(int(ctx.val("structs.FField.Next")), fx.NEXT)
        self.assertEqual(int(ctx.val("structs.FField.ClassPrivate")), fx.CLS)
        self.assertEqual(int(ctx.val("structs.FFieldClass.CastFlags")), fx.CAST)            # not the unmasked second load

    def test_the_flags_constant_is_what_identifies_the_function(self):
        ctx = CdoImage(flags=0x30).run()
        self.assertIsNone(ctx.get("structs.UClass.ClassDefaultObject"))
        self.assertTrue(any(u.id == "structs.UClass.ClassDefaultObject" for u in ctx.unresolved))

    def test_a_function_that_never_stores_the_result_is_not_it(self):
        self.assertIsNone(CdoImage(store=False).run().get("structs.UClass.ClassDefaultObject"))

    def test_two_candidates_are_ambiguous(self):
        ctx = CdoImage(second=True).run()
        self.assertIsNone(ctx.get("structs.UClass.ClassDefaultObject"))
        self.assertIn("need exactly 1", [u.why for u in ctx.unresolved if u.id == "structs.UClass.ClassDefaultObject"][0])

    def test_without_the_loop_only_the_cdo_members_are_reported(self):
        ctx = CdoImage(loop=False).run()
        self.assertIsNotNone(ctx.get("structs.UClass.ClassDefaultObject"))
        self.assertIsNone(ctx.get("structs.FField.ClassPrivate"))

    def test_without_the_uclass_vtable_nothing_is_reported(self):
        ctx = make_ctx(CdoImage().data)
        class_default_object.detect(ctx)
        self.assertEqual(ctx.values, {})


class FieldClassImage:
    """F<Type>Property class registrations, the FFieldClass constructor, ConstructFn functions and the FField constructor."""

    CF_NAME, CF_ID, CF_CAST, CF_CFLAGS, CF_SUPER, CF_DEFOBJ, CF_CONSTRUCT, CF_COUNTER = 0x0, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40
    OWNER, NEXT, NAME, FLAGS = 0x10, 0x18, 0x20, 0x28          # FField constructor stores (arbitrary)
    ADIM = 0x2C
    SIZES = {"FProperty": 0x78, "FBoolProperty": 0x80, "FStructProperty": 0x80, "FArrayProperty": 0x88, "FIntProperty": 0x78,
             "FStrProperty": 0x78}
    NAMES = list(SIZES)
    STRIDE = 0x50                                              # 0x48-byte descriptor + 8-byte guard variable

    def __init__(self, names=None, spacing=True, field_ctor=True, arraydim=True, tail=True):
        im = self.im = LImg()
        names = names or self.NAMES
        lits = {n: im.ro_str(n, wide=True) for n in names}
        im.func("fname_ctor").epilogue()
        im.func("malloc").epilogue()
        a = im.func("field_ctor")                                # FField(Owner*, Name*, Flags)
        if field_ctor:
            a.ld(RAX, RSI, 0, 8).st(RDI, self.OWNER, RAX, 8).st_imm(RDI, self.NEXT, 0, 8)
            a.ld(RAX, RDX, 0, 8).st(RDI, self.NAME, RAX, 8).st(RDI, self.FLAGS, RCX, 4)
        a.epilogue()
        a = im.func("Cf")                                        # FFieldClass(this, Name, Id, CastFlags, Super, ConstructFn)
        a.push(RBX).mov_rr(RBX, RDI)
        a.st_imm(RBX, self.CF_NAME, 0, 8).st(RBX, self.CF_ID, RDX, 8).st(RBX, self.CF_CAST, RCX, 8).st_imm(RBX, self.CF_CFLAGS, 0, 4)
        a.st(RBX, self.CF_SUPER, R8, 8).st_imm(RBX, self.CF_DEFOBJ, 0, 8).st(RBX, self.CF_CONSTRUCT, R9, 8).st_imm(RBX, self.CF_COUNTER, 0, 4)
        a.lea_rm(RDI, RBP, -0x28).mov_r32_imm(RDX, 1).call("fname_ctor").ld(RAX, RBP, -0x28, 8).st(RBX, self.CF_NAME, RAX, 8)
        a.pop(RBX).epilogue()
        self.statics = {}
        base = im.bss(1, 16)
        for k, n in enumerate(names):
            self.statics[n] = im.bss(self.STRIDE if spacing else 0x400 * (k + 1), 8)
        for n in names:
            a = im.func("construct_" + n)                        # new T(Owner, Name, Flags)
            a.push(RBX).mov_r32_imm(RDI, self.SIZES[n]).xor_r32(RSI).call("malloc").mov_rr(RBX, RAX)
            if n == "FProperty" and arraydim:
                a.st_imm(RBX, self.ADIM, 1, 8)                   # ArrayDim = 1, ElementSize = 0 in one store
            a.mov_rr(RDI, RBX).lea_rm(RSI, RBP, -0x38).mov_rr(RDX, R15).mov_r32_imm(RCX, 0).call("field_ctor")
            a.mov_rr(RAX, RBX).pop(RBX).epilogue()
        for k, n in enumerate(names):
            a = im.func("static_class_" + n)
            a.lea_rip(RSI, lits[n]).lea_rip(RDI, self.statics[n]).mov_r32_imm(RDX, 1 << k).mov_r32_imm(RCX, 1 << k)
            a.xor_r32(R8).lea_rip(R9, "construct_" + n).call("Cf").epilogue()
        self.labels = im.a.labels
        self.data = im.image()

    def run(self):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        ctx.facts["fname_ctor"] = self.labels["fname_ctor"]
        ctx.facts["fmemory_malloc"] = self.labels["malloc"]
        field_classes.detect(ctx)
        layout_merge.detect(ctx)
        return ctx


@needs_capstone
class FieldClassesTests(unittest.TestCase):
    def test_the_constructor_is_the_call_the_names_agree_on(self):
        fx = FieldClassImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("symbols.ffield_class_ctor.label")), fx.labels["Cf"])

    def test_constructor_stores_follow_the_arguments(self):
        fx = FieldClassImage()
        ctx = fx.run()
        for key, off in (("FName", fx.CF_NAME), ("Id", fx.CF_ID), ("CastFlags", fx.CF_CAST), ("SuperClass", fx.CF_SUPER),
                         ("ConstructFn", fx.CF_CONSTRUCT)):
            self.assertEqual(int(ctx.val("structs.FFieldClass." + key)), off, key)
            self.assertEqual(ctx.get("structs.FFieldClass." + key).confidence, "medium")
        for key, off in (("ClassFlags", fx.CF_CFLAGS), ("DefaultObject", fx.CF_DEFOBJ), ("UniqueNameIndexCounter", fx.CF_COUNTER)):
            self.assertEqual(int(ctx.val("structs.FFieldClass." + key)), off, key)
            self.assertEqual(ctx.get("structs.FFieldClass." + key).confidence, "low")          # named by declaration order only

    def test_total_size_needs_the_extent_and_the_static_spacing_to_agree(self):
        fx = FieldClassImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.FFieldClass.UEP_TotalSize")), 0x48)            # last dword ends at 0x44, aligned to 8
        self.assertEqual(ctx.get("structs.FFieldClass.UEP_TotalSize").confidence, "high")     # spacing 0x50 - guard => at most 0x48
        loose = FieldClassImage(spacing=False).run()
        self.assertEqual(int(loose.val("structs.FFieldClass.UEP_TotalSize")), 0x48)
        self.assertEqual(loose.get("structs.FFieldClass.UEP_TotalSize").confidence, "medium")

    def test_property_class_sizes_are_the_constant_their_construct_function_allocates(self):
        fx = FieldClassImage()
        ctx = fx.run()
        for n in ("FProperty", "FBoolProperty", "FStructProperty", "FArrayProperty"):
            self.assertEqual(int(ctx.val("structs.%s.UEP_TotalSize" % n)), fx.SIZES[n], n)
            self.assertEqual(ctx.get("structs.%s.UEP_TotalSize" % n).confidence, "high")

    def test_one_immediate_store_fixes_arraydim_and_the_order_of_elementsize(self):
        fx = FieldClassImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.FProperty.ArrayDim")), fx.ADIM)
        self.assertEqual(int(ctx.val("structs.FProperty.ElementSize")), fx.ADIM + 4)
        none = FieldClassImage(arraydim=False).run()
        self.assertIsNone(none.get("structs.FProperty.ArrayDim"))

    def test_the_field_constructor_is_recognised_by_its_stores(self):
        fx = FieldClassImage()
        ctx = fx.run()
        for key, off in (("Owner", fx.OWNER), ("Next", fx.NEXT), ("NamePrivate", fx.NAME), ("FlagsPrivate", fx.FLAGS)):
            self.assertEqual(int(ctx.val("structs.FField." + key)), off, key)

    def test_sizeof_ffield_needs_arraydim_right_at_the_end_of_the_data(self):
        fx = FieldClassImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.FField.UEP_TotalSize")), 0x30)                 # FlagsPrivate ends at 0x2c = ArrayDim
        self.assertEqual(ctx.get("structs.FField.UEP_TotalSize").confidence, "medium")
        # a gap between FlagsPrivate and ArrayDim means FField has more members: no size
        class Gap(FieldClassImage):
            ADIM = 0x34
        self.assertIsNone(Gap().run().get("structs.FField.UEP_TotalSize"))

    def test_too_few_names_or_no_common_target_is_unresolved(self):
        ctx = FieldClassImage(names=["FProperty", "FBoolProperty"]).run()
        self.assertIsNone(ctx.get("symbols.ffield_class_ctor.label"))
        self.assertTrue(any(u.id == "structs.FFieldClass.CastFlags" for u in ctx.unresolved))

    def test_without_a_field_constructor_shape_the_field_members_are_unresolved(self):
        ctx = FieldClassImage(field_ctor=False).run()
        self.assertIsNone(ctx.get("structs.FField.Owner"))
        self.assertIsNone(ctx.get("structs.FField.UEP_TotalSize"))


class SubtypeImage:
    """FProperty and five numeric/aggregate subtype vtables, their ConstructFn functions and the overriding virtuals."""

    TOTAL = 0x70                  # sizeof(FProperty) in the fixture
    NSLOT, ALIGN_SLOT, INIT_SLOT = 44, 31, 19
    STRUCT_M, ARRAY_FLAGS, ARRAY_INNER, ENUM_UNDER, ENUM_ENUM = 0x70, 0x70, 0x78, 0x70, 0x78
    BOOL = (0x70, 0x71, 0x72, 0x73)
    NUMERIC = (("FByteProperty", 1), ("FIntProperty", 4), ("FFloatProperty", 4), ("FInt64Property", 8), ("FDoubleProperty", 8))

    def __init__(self, align_consts=True, struct_extra=False, align_dup=False):
        im = self.im = LImg()
        im.func("malloc").epilogue()
        im.func("stub").epilogue()
        for n in (1, 4, 8):
            im.func("ret%d" % n).raw(b"\xb8" + bytes([n, 0, 0, 0])).epilogue()                 # mov eax, n
        a = im.func("struct_virtual")
        a.ld(RAX, RDI, self.STRUCT_M, 8).ld(RCX, RAX, 0, 8).call_mem(RCX, 0x308).epilogue()
        if struct_extra:
            a = im.func("struct_virtual2")
            a.ld(RAX, RDI, self.STRUCT_M + 8, 8).epilogue()
        a = im.func("array_virtual")
        a.ld(RCX, RDI, self.ARRAY_FLAGS, 1).ld(RAX, RDI, self.ARRAY_INNER, 8).epilogue()
        a = im.func("enum_align")
        a.ld(RAX, RDI, self.ENUM_UNDER, 8).call_mem(RAX, 0x170).epilogue()
        a = im.func("enum_other")
        a.ld(RAX, RDI, self.ENUM_ENUM, 8).epilogue()
        a = im.func("bool_other")
        for d in self.BOOL:
            a.ld(RAX, RDI, d, 1)
        a.epilogue()
        a = im.func("bool_init")                                   # *(Dest + ByteOffset) &= ~FieldMask
        a.ld(RCX, RDI, self.BOOL[3], 1).ld(RAX, RDI, self.BOOL[1], 1).not_r8(RCX).and_mem8_reg(RSI, RAX, RCX).epilogue()
        labels = im.a.labels
        names = ["FProperty", "FStructProperty", "FArrayProperty", "FEnumProperty", "FBoolProperty"] + [n for n, _a in self.NUMERIC]
        overrides = {
            "FStructProperty": {5: "struct_virtual", self.ALIGN_SLOT: "struct_virtual", 6: "struct_virtual2" if struct_extra else "struct_virtual"},
            "FArrayProperty": {5: "array_virtual", 8: "array_virtual"},
            "FEnumProperty": {self.ALIGN_SLOT: "enum_align", 6: "enum_other", 9: "enum_other"},
            "FBoolProperty": {7: "bool_other", self.INIT_SLOT: "bool_init"},
        }
        ret_for = {"FByteProperty": "ret1", "FIntProperty": "ret4", "FFloatProperty": "ret4", "FInt64Property": "ret8", "FDoubleProperty": "ret8"}
        relas = []
        self.vt = {}
        off = RW_VA
        for n in names:
            relas.append((off, RO_VA + 16))                        # typeinfo (data)
            base = off + 8
            self.vt[n] = base
            for k in range(self.NSLOT):
                fn = "stub"
                if n in overrides and k in overrides[n]:
                    fn = overrides[n][k]
                if k in ((self.ALIGN_SLOT, self.ALIGN_SLOT + 1) if align_dup else (self.ALIGN_SLOT,)) and n in ret_for and align_consts:
                    fn = ret_for[n]
                if k == self.ALIGN_SLOT and n == "FProperty":
                    fn = "ret1"
                relas.append((base + 8 * k, labels[fn]))
            off = base + 8 * self.NSLOT + 8
        assert off < RW_VA + RW_FILESZ
        # ConstructFn functions: new T; install the vtable
        sizes = {n: self.TOTAL + (0 if n in ("FProperty", "FByteProperty") else 8) for n in names}
        for n in names:
            a = im.func("construct_" + n)
            a.push(RBX).mov_r32_imm(RDI, sizes[n]).xor_r32(RSI).call("malloc").mov_rr(RBX, RAX)
            a.lea_rip(RAX, self.vt[n]).st(RBX, 0, RAX, 8).mov_rr(RAX, RBX).pop(RBX).epilogue()
        self.labels = im.a.labels
        # the vtable addresses are data addresses: the assembler needs them as labels for lea
        im.relas = relas
        self.data = im.image()
        self.names = names

    def run(self, with_fproperty=True):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        ctx.facts["fmemory_malloc"] = self.labels["malloc"]
        cfns = {n: self.labels["construct_" + n] for n in self.names if with_fproperty or n != "FProperty"}
        ctx.facts["field_class_registrations"] = {"constructor": 0, "construct_fns": cfns}
        b = _book.book(ctx)
        b.observe("structs.FProperty.UEP_TotalSize", self.TOTAL, "t", "t", [], "high", True)
        b.observe("vtables.FProperty.InitializeValueInternal", self.INIT_SLOT * 8, "t", "t", [], "medium")
        property_subtypes.detect(ctx)
        layout_merge.detect(ctx)
        return ctx


@needs_capstone
class PropertySubtypeTests(unittest.TestCase):
    def test_members_are_the_loads_the_overridden_virtuals_make_beyond_the_base(self):
        fx = SubtypeImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("structs.FStructProperty.Struct")), fx.STRUCT_M)
        self.assertEqual(int(ctx.val("structs.FArrayProperty.Inner")), fx.ARRAY_INNER)
        self.assertEqual(int(ctx.val("structs.FArrayProperty.ArrayFlags")), fx.ARRAY_FLAGS)
        self.assertEqual(int(ctx.val("structs.FEnumProperty.UnderlyingProp")), fx.ENUM_UNDER)
        self.assertEqual(int(ctx.val("structs.FEnumProperty.Enum")), fx.ENUM_ENUM)
        for key, off in zip(("FieldSize", "ByteOffset", "ByteMask", "FieldMask"), fx.BOOL):
            self.assertEqual(int(ctx.val("structs.FBoolProperty." + key)), off, key)

    def test_the_alignment_slot_is_the_one_returning_alignof_for_every_numeric_type(self):
        fx = SubtypeImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("vtables.FProperty.GetMinAlignment")), fx.ALIGN_SLOT * 8)
        self.assertEqual(ctx.get("vtables.FProperty.GetMinAlignment").confidence, "high")
        no = SubtypeImage(align_consts=False).run()
        self.assertIsNone(no.get("vtables.FProperty.GetMinAlignment"))
        dup = SubtypeImage(align_dup=True).run()                                # two slots return the same constants: not unique
        self.assertIsNone(dup.get("vtables.FProperty.GetMinAlignment"))

    def test_a_second_channel_raises_struct_underlying_prop_and_the_bool_members(self):
        fx = SubtypeImage()
        ctx = fx.run()
        self.assertEqual(ctx.get("structs.FStructProperty.Struct").confidence, "high")           # alignment override forwards to it
        self.assertEqual(ctx.get("structs.FEnumProperty.UnderlyingProp").confidence, "high")
        self.assertEqual(ctx.get("structs.FBoolProperty.ByteOffset").confidence, "high")         # index of the cleared byte
        self.assertEqual(ctx.get("structs.FBoolProperty.FieldMask").confidence, "high")          # the mask
        self.assertEqual(ctx.get("structs.FBoolProperty.ByteMask").confidence, "medium")
        self.assertEqual(ctx.get("structs.FEnumProperty.Enum").confidence, "medium")

    def test_a_second_pointer_member_makes_the_first_channel_ambiguous(self):
        ctx = SubtypeImage(struct_extra=True).run()
        # the overrides now read two pointers, so "its only member" no longer holds; only the alignment override still names Struct
        self.assertEqual(int(ctx.val("structs.FStructProperty.Struct")), SubtypeImage.STRUCT_M)
        self.assertEqual(ctx.get("structs.FStructProperty.Struct").confidence, "medium")

    def test_without_the_base_property_nothing_is_reported(self):
        ctx = SubtypeImage().run(with_fproperty=False)
        self.assertIsNone(ctx.get("structs.FStructProperty.Struct"))
        self.assertTrue(any(u.id == "structs.FStructProperty.Struct" for u in ctx.unresolved))

    def test_const_return_only_accepts_a_trivial_function(self):
        im = LImg()
        im.func("c4").raw(b"\xb8\x04\x00\x00\x00").epilogue()
        im.func("zero").xor_r32(RAX).epilogue()
        im.func("calls").call("c4").epilogue()
        im.func("two").raw(b"\xb8\x04\x00\x00\x00").raw(b"\x01\xc0").epilogue()                 # mov eax,4 ; add eax,eax
        ctx = make_ctx(im.image())
        l = im.a.labels
        self.assertEqual(property_subtypes.const_return(ctx, l["c4"]), 4)
        self.assertEqual(property_subtypes.const_return(ctx, l["zero"]), 0)
        self.assertIsNone(property_subtypes.const_return(ctx, l["calls"]))


class EngineTickImage:
    """Two vtable runs: the UEngine one holds the pure-virtual stub at slot K, the UGameEngine one the override."""

    K, N = 23, 40

    def __init__(self, stub=True, override=True, disagree=False, multi_anchor=False):
        im = self.im = LImg()
        stub_text = im.ro_str("UEngine::Tick", wide=True)
        im.ro_str("UEngine::TickWorldTravel has no world after ticking seamless travel handler.", wide=True)
        anchor = im.ro_str("causeevent=", wide=True)
        im.func("other").epilogue()
        a = im.func("tick_stub")
        if stub:
            a.lea_rip(RSI, stub_text)
        a.epilogue()
        a = im.func("game_tick")
        a.nops(0x20)
        if override:
            a.lea_rip(RSI, anchor)
        a.epilogue()
        if multi_anchor:
            a = im.func("game_tick2")
            a.lea_rip(RSI, anchor).epilogue()
        labels = im.a.labels
        relas = []
        off = RW_VA
        for v, key in enumerate(("stub", "game")):
            relas.append((off, RO_VA + 16))
            base = off + 8
            for k in range(self.N):
                fn = labels["other"]
                if k == self.K + (3 if (disagree and key == "game") else 0):
                    fn = labels["tick_stub"] if key == "stub" else labels["game_tick"]
                relas.append((base + 8 * k, fn))
            off = base + 8 * self.N + 8
        im.relas = relas
        self.labels = labels
        self.data = im.image()

    def run(self):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        engine_tick.detect(ctx)
        return ctx


@needs_capstone
class EngineTickTests(unittest.TestCase):
    def test_stub_and_override_at_one_slot_are_two_channels(self):
        fx = EngineTickImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("vtables.UEngine.Tick")), fx.K * 8)
        self.assertEqual(int(ctx.val("vtables.UEngine.Tick_slot_index")), fx.K)
        self.assertEqual(ctx.get("vtables.UEngine.Tick").confidence, "high")
        self.assertEqual(int(ctx.val("symbols.engine_tick.label")), fx.labels["game_tick"])
        self.assertEqual(ctx.get("symbols.engine_tick.label").confidence, "high")
        self.assertEqual(ctx.val("symbols.engine_tick.prologue_23"), ctx.elf.read(fx.labels["game_tick"], 23).hex())
        self.assertIsNotNone(ctx.get("symbols.engine_tick.anchor_xref_label"))

    def test_one_channel_is_medium(self):
        for kw in ({"stub": False}, {"override": False}):
            ctx = EngineTickImage(**kw).run()
            self.assertEqual(int(ctx.val("vtables.UEngine.Tick")), EngineTickImage.K * 8)
            self.assertEqual(ctx.get("vtables.UEngine.Tick").confidence, "medium")

    def test_channels_that_disagree_on_the_slot_publish_no_slot(self):
        ctx = EngineTickImage(disagree=True).run()
        self.assertIsNone(ctx.get("vtables.UEngine.Tick"))
        self.assertTrue(any(u.id == "vtables.UEngine.Tick" for u in ctx.unresolved))
        self.assertIsNotNone(ctx.get("symbols.engine_tick.label"))                        # the function itself is still the anchor's

    def test_the_longer_string_that_merely_contains_the_stub_name_does_not_count(self):
        ctx = EngineTickImage(stub=False, override=False).run()
        self.assertIsNone(ctx.get("vtables.UEngine.Tick"))

    def test_two_functions_with_the_anchor_are_not_the_tick(self):
        ctx = EngineTickImage(multi_anchor=True).run()
        self.assertIsNone(ctx.get("symbols.engine_tick.label"))
        self.assertTrue(any(u.id == "symbols.engine_tick" for u in ctx.unresolved))

    def test_no_anchor_strings_is_unresolved(self):
        im = LImg()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        engine_tick.detect(ctx)
        self.assertEqual({u.id for u in ctx.unresolved}, {"vtables.UEngine.Tick", "symbols.engine_tick"})


class GNativesImage:
    N_SITES, N_STORES = 14, 40

    def __init__(self, sites=None, stores=None, rival=False, writable_only=False):
        im = self.im = LImg()
        self.G = im.bss(0x800, 16)
        self.H = im.bss(0x800, 16)
        n_sites = self.N_SITES if sites is None else sites
        n_stores = self.N_STORES if stores is None else stores
        a = im.func("step")                                       # FFrame::Step shape: B = *Code++; GNatives[B](...)
        a.lea_rip(RCX, self.G).ld(RAX, RDX, 0, 1).call_sib(RCX, RAX, 8).epilogue()
        for k in range(n_sites):
            a = im.func("exec%d" % k)
            a.lea_rip(R8, self.G).ld(RAX, RDX, 0, 1).call_sib(R8, RAX, 8).epilogue()
        if rival:
            for k in range(n_sites):
                a = im.func("rival%d" % k)
                a.lea_rip(R8, self.H).ld(RAX, RDX, 0, 1).call_sib(R8, RAX, 8).epilogue()
        a = im.func("init")
        for k in range(n_stores):
            a.mov_store_rip(self.G + 8 * k, RAX)
        a.epilogue()
        for k in range(120):
            im.func("caller%d" % k).call("step").epilogue()
        self.labels = im.a.labels
        self.data = im.image()

    def run(self):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        gnatives.detect(ctx)
        return ctx


@needs_capstone
class GNativesTests(unittest.TestCase):
    def test_dispatch_sites_and_registration_stores_find_the_table(self):
        fx = GNativesImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("symbols.gnatives.label")), fx.G)
        self.assertEqual(ctx.get("symbols.gnatives.label").confidence, "high")
        self.assertEqual(int(ctx.val("symbols.gnatives.entries")), 256)
        self.assertEqual(int(ctx.val("symbols.fframe_step.label")), fx.labels["step"])

    def test_without_the_registration_stores_it_is_medium(self):
        ctx = GNativesImage(stores=2).run()
        self.assertEqual(ctx.get("symbols.gnatives.label").confidence, "medium")

    def test_too_few_dispatch_sites_is_unresolved(self):
        ctx = GNativesImage(sites=3).run()
        self.assertIsNone(ctx.get("symbols.gnatives.label"))
        self.assertTrue(any(u.id == "symbols.gnatives" for u in ctx.unresolved))

    def test_two_equally_popular_tables_are_ambiguous(self):
        ctx = GNativesImage(rival=True).run()
        self.assertIsNone(ctx.get("symbols.gnatives.label"))
        self.assertIn("compete", [u.why for u in ctx.unresolved if u.id == "symbols.gnatives"][0])


class FNameToStringImage:
    """FString FName::ToString() const (sret) forwarding to void FName::ToString(FString&) const, both reading the pool."""

    BLOCKS = 0x18

    def __init__(self, forwards=True, cluster_caller=True, extra_users=0, swapped=True):
        im = self.im = LImg()
        self.pool = im.bss(0x400, 16)
        path = im.ro_str(r".\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp")
        m2 = im.ro_str("Unable to add more objects to disregard for GC pool (Max: %d)", wide=True)
        im.func("widen").epilogue()
        a = im.func("to_string_out")                              # (FName* this, FString* out)
        a.push(RBX).mov_rr(RBX, RSI).ld(R14, RDI, 0, 4).cmp_mem_imm(RDI, 4, 0, 4)
        a.lea_rip(RCX, self.pool).ld_sib(RAX, RCX, RAX, 8, self.BLOCKS).mov_rr(RDI, RBX).call("widen").pop(RBX).epilogue()
        a = im.func("to_string_value")                            # (FString* sret, FName* this)
        a.push(RBX).mov_rr(RBX, RDI).cmp_mem_imm(RSI, 4, 0, 4).ld(R14, RSI, 0, 4)
        a.lea_rip(RCX, self.pool).ld_sib(RAX, RCX, RAX, 8, self.BLOCKS)
        if forwards:
            if swapped == "half":                              # the FName goes first but the return slot is not passed on
                a.mov_rr(RDI, RSI).call("to_string_out")
            elif swapped:
                a.mov_rr(RDI, RSI).mov_rr(RSI, RBX).call("to_string_out")
            else:
                a.call("to_string_out")
        a.mov_rr(RAX, RBX).pop(RBX).epilogue()
        if extra_users:
            a = im.func("to_string_value2")
            a.push(RBX).mov_rr(RBX, RDI).cmp_mem_imm(RSI, 4, 0, 4).ld(R14, RSI, 0, 4)
            a.lea_rip(RCX, self.pool).ld_sib(RAX, RCX, RAX, 8, self.BLOCKS).mov_rr(RAX, RBX).pop(RBX).epilogue()
            for k in range(extra_users):
                im.func("user2_%d" % k).call("to_string_value2").epilogue()
        for k in range(30):
            im.func("user%d" % k).call("to_string_value").epilogue()
        if cluster_caller:
            a = im.func("alloc_index")                            # a UObjectArray.cpp member that prints Object->GetFName().ToString()
            a.lea_rip(RDI, path).lea_rip(RDX, m2).call("to_string_value").epilogue()
        self.labels = im.a.labels
        self.data = im.image()

    def run(self, blocks=True):
        ctx = make_ctx(self.data)
        platform.detect(ctx)
        ctx.facts["fname_pool"] = self.pool
        if blocks:
            ctx.put("structs.FNamePool.blocks", Hex(self.BLOCKS), "high", "t", [])
        fname_tostring.detect(ctx)
        return ctx


@needs_capstone
class FNameToStringTests(unittest.TestCase):
    def test_both_overloads_are_found_and_linked(self):
        fx = FNameToStringImage()
        ctx = fx.run()
        self.assertEqual(int(ctx.val("symbols.fname_to_string_value.label")), fx.labels["to_string_value"])
        self.assertEqual(int(ctx.val("symbols.fname_to_string.label")), fx.labels["to_string_out"])
        self.assertEqual(ctx.get("symbols.fname_to_string.label").confidence, "high")
        self.assertIn("rdi = const FName*, rsi = FString*", ctx.val("symbols.fname_to_string.abi"))
        self.assertIn("hidden return slot", ctx.val("symbols.fname_to_string_value.abi"))

    def test_no_object_array_caller_is_medium(self):
        ctx = FNameToStringImage(cluster_caller=False).run()
        self.assertEqual(ctx.get("symbols.fname_to_string.label").confidence, "medium")

    def test_the_out_form_must_be_called_with_the_arguments_swapped(self):
        for kw in ({"swapped": False}, {"swapped": "half"}, {"forwards": False}):
            ctx = FNameToStringImage(**kw).run()
            self.assertIsNone(ctx.get("symbols.fname_to_string.label"), kw)
            self.assertIsNotNone(ctx.get("symbols.fname_to_string_value.label"), kw)
            self.assertTrue(any(u.id == "symbols.fname_to_string" for u in ctx.unresolved), kw)

    def test_a_second_candidate_with_a_comparable_caller_count_lowers_the_confidence(self):
        for users, conf in ((3, "medium"), (24, "low")):
            fx = FNameToStringImage(extra_users=users)
            ctx = fx.run()
            self.assertEqual(int(ctx.val("symbols.fname_to_string_value.label")), fx.labels["to_string_value"], users)
            self.assertEqual(ctx.get("symbols.fname_to_string_value.label").confidence, conf, users)

    def test_without_the_pool_there_is_nothing_to_find(self):
        ctx = FNameToStringImage().run(blocks=False)
        self.assertIsNone(ctx.get("symbols.fname_to_string.label"))
        self.assertTrue(any(u.id == "symbols.fname_to_string" for u in ctx.unresolved))


@needs_capstone
class DetectorSelectionTests(unittest.TestCase):
    def test_selecting_a_layout_detector_also_resolves_the_book(self):
        from tools.profilegen import detectors
        im = LImg()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        detectors.run_all(ctx, only={"class_registration"})
        self.assertIn("detector.layout_merge", ctx.timings)                              # layout_merge follows its observers
        self.assertTrue(any(u.id == "structs.UStruct.PropertiesSize" for u in ctx.unresolved))
        self.assertNotIn("detector.engine_tick", ctx.timings)

    def test_the_new_detectors_are_registered_after_the_ones_whose_facts_they_use(self):
        from tools.profilegen import detectors
        names = [n for n, _fn in detectors.ORDER]
        for later in ("class_registration", "uobject_base", "process_event_members", "class_default_object", "field_classes",
                      "property_subtypes", "engine_tick", "fname_tostring", "gnatives"):
            self.assertGreater(names.index(later), names.index("process_event"), later)
        self.assertEqual(names[-1], "layout_merge")
        self.assertGreater(names.index("class_default_object"), names.index("class_registration"))     # needs the UClass vtable
        self.assertGreater(names.index("property_subtypes"), names.index("field_classes"))             # needs the ConstructFn list

    def test_a_layout_detector_that_cannot_run_is_reported_not_fatal(self):
        from tools.profilegen import detectors
        im = LImg()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        ctx.index._decoder = None
        saved = Decoder.available
        Decoder.available = False
        try:
            detectors.run_all(ctx, only={"gnatives", "engine_tick", "uobject_base"})
        finally:
            Decoder.available = saved
        ids = {u.id for u in ctx.unresolved}
        for name in ("gnatives", "engine_tick", "uobject_base"):
            self.assertIn("detector." + name, ids)


@needs_capstone
class LayoutMergeTests(unittest.TestCase):
    """Declared-order fits: members that no analysed code reads but that fill a measured gap exactly."""

    def merge(self, obs, caps=None):
        ctx = Context(None, None)
        b = _book.book(ctx)
        for path, value in obs.items():
            b.observe(path, value, "t", "t", [], (caps or {}).get(path, "medium"))
        layout_merge.infer(ctx)
        _book.resolve(ctx)
        return ctx

    def val(self, ctx, path):
        env = ctx.get(path)
        return None if env is None else (int(env.value), env.confidence)

    def test_children_fits_between_super_struct_and_child_properties(self):
        ctx = self.merge({"structs.UStruct.SuperStruct": 0x40, "structs.UStruct.ChildProperties": 0x50})
        self.assertEqual(self.val(ctx, "structs.UStruct.Children"), (0x48, "medium"))
        self.assertIn("8-byte slot", ctx.get("structs.UStruct.Children").evidence[0])
        wide = self.merge({"structs.UStruct.SuperStruct": 0x40, "structs.UStruct.ChildProperties": 0x58})
        self.assertIsNone(wide.get("structs.UStruct.Children"))                                  # a gap of two slots: not pinned down

    def test_property_and_ref_links_fit_between_script_and_destructor_link(self):
        ctx = self.merge({"structs.UStruct.Script": 0x60, "structs.UStruct.DestructorLink": 0x80})
        self.assertEqual(self.val(ctx, "structs.UStruct.PropertyLink"), (0x70, "medium"))
        self.assertEqual(self.val(ctx, "structs.UStruct.RefLink"), (0x78, "medium"))
        self.assertEqual(self.val(ctx, "structs.UStruct.PostConstructLink"), (0x88, "low"))      # one measured neighbour only
        far = self.merge({"structs.UStruct.Script": 0x60, "structs.UStruct.DestructorLink": 0x90})
        self.assertIsNone(far.get("structs.UStruct.PropertyLink"))

    def test_function_members_between_measured_neighbours(self):
        ctx = self.merge({"structs.UFunction.FunctionFlags": 0xB0, "structs.UFunction.ParmsSize": 0xB6,
                          "structs.UFunction.ReturnValueOffset": 0xB8, "structs.UFunction.FirstPropertyToInit": 0xC0})
        self.assertEqual(self.val(ctx, "structs.UFunction.NumParms"), (0xB4, "medium"))
        self.assertEqual(self.val(ctx, "structs.UFunction.RPCId"), (0xBA, "medium"))
        self.assertEqual(self.val(ctx, "structs.UFunction.RPCResponseId"), (0xBC, "medium"))
        moved = self.merge({"structs.UFunction.FunctionFlags": 0xB0, "structs.UFunction.ParmsSize": 0xB8,
                            "structs.UFunction.ReturnValueOffset": 0xBA, "structs.UFunction.FirstPropertyToInit": 0xD0})
        for k in ("NumParms", "RPCId", "RPCResponseId"):
            self.assertIsNone(moved.get("structs.UFunction." + k), k)

    def test_ufield_next_is_the_one_member_by_which_ufield_outgrows_uobject(self):
        ctx = self.merge({"structs.UObjectBase.UEP_TotalSize": 0x28, "structs.UField.UEP_TotalSize": 0x30})
        self.assertEqual(self.val(ctx, "structs.UField.Next"), (0x28, "medium"))
        bigger = self.merge({"structs.UObjectBase.UEP_TotalSize": 0x28, "structs.UField.UEP_TotalSize": 0x38})
        self.assertIsNone(bigger.get("structs.UField.Next"))

    def test_fproperty_members_between_and_after_measured_ones(self):
        ctx = self.merge({"structs.FProperty.PropertyFlags": 0x38, "structs.FProperty.Offset_Internal": 0x44,
                          "structs.FProperty.DestructorLinkNext": 0x58, "structs.FProperty.PostConstructLinkNext": 0x60,
                          "structs.FProperty.UEP_TotalSize": 0x70})
        self.assertEqual(self.val(ctx, "structs.FProperty.RepIndex"), (0x40, "medium"))
        self.assertEqual(self.val(ctx, "structs.FProperty.PropertyLinkNext"), (0x48, "medium"))
        self.assertEqual(self.val(ctx, "structs.FProperty.NextRef"), (0x50, "medium"))
        self.assertEqual(self.val(ctx, "structs.FProperty.RepNotifyFunc"), (0x68, "medium"))
        taller = self.merge({"structs.FProperty.PostConstructLinkNext": 0x60, "structs.FProperty.UEP_TotalSize": 0x80})
        self.assertIsNone(taller.get("structs.FProperty.RepNotifyFunc"))

    def test_item_members(self):
        ctx = self.merge({"structs.FUObjectItem.Flags": 0x8, "structs.FUObjectItem.SerialNumber": 0x10, "structs.FUObjectItem.UEP_TotalSize": 0x18})
        self.assertEqual(self.val(ctx, "structs.FUObjectItem.ClusterRootIndex"), (0xC, "medium"))
        self.assertEqual(self.val(ctx, "structs.FUObjectItem.RefCount"), (0x14, "medium"))

    def test_a_neighbour_below_medium_does_not_pin_anything_down(self):
        ctx = self.merge({"structs.UStruct.SuperStruct": 0x40, "structs.UStruct.ChildProperties": 0x50},
                         caps={"structs.UStruct.ChildProperties": "low"})
        self.assertIsNone(ctx.get("structs.UStruct.Children"))

    def test_a_fit_that_disagrees_with_a_measurement_publishes_nothing(self):
        ctx = Context(None, None)
        b = _book.book(ctx)
        b.observe("structs.UStruct.SuperStruct", 0x40, "t", "t", [], "medium")
        b.observe("structs.UStruct.ChildProperties", 0x50, "t", "t", [], "medium")
        b.observe("structs.UStruct.Children", 0x60, "other", "t", [], "medium")           # some other detector saw a different offset
        layout_merge.infer(ctx)
        _book.resolve(ctx)
        self.assertIsNone(ctx.get("structs.UStruct.Children"))
        self.assertIn("channels disagree", [u.why for u in ctx.unresolved if u.id == "structs.UStruct.Children"][0])

    def test_every_accepted_key_of_the_scoped_classes_is_measured_or_reported(self):
        ctx = Context(None, None)
        layout_merge.detect(ctx)
        ids = {u.id for u in ctx.unresolved}
        schema = emit_ue4ss.native_schema()["classes"]
        for cls in layout_merge.SCOPE:
            for key in schema[cls]["keys"]:
                if key not in layout_merge.SKIP_KEYS:
                    self.assertIn("structs.%s.%s" % (cls, key), ids)
        self.assertNotIn("structs.UnqiueNameIndexCounter", ids)
        self.assertEqual(len(ids), len(ctx.unresolved))                                       # one entry per id


def ext_doc(**over):
    """sample_doc plus the reflection classes the layout detectors measure."""
    def env(v, c="high", m="test"):
        return {"value": v, "confidence": c, "method": m, "evidence": []}
    doc = sample_doc()
    doc["structs"].update({
        "UObjectBase": {"ObjectFlags": env("0x8"), "InternalIndex_Private": env("0xc"), "ClassPrivate": env("0x10"),
                        "NamePrivate": env("0x18", "low"), "OuterPrivate": env("0x20", "medium"), "UEP_TotalSize": env("0x28")},
        "UStruct": {"SuperStruct": env("0x40"), "Script": env("0x60", "medium"), "UEP_TotalSize": env("0xb0", "low")},
        "UClass": {"ClassFlags": env("0xd4"), "ClassDefaultObject": env("0x110", "medium"), "UEP_TotalSize": env("0x200")},
        "FProperty": {"ArrayDim": env("0x2c"), "Offset_Internal": env("0x44"), "RepIndex": env("0x200"), "UEP_TotalSize": env("0x70")},
        "AActor": {"UEP_TotalSize": env("0x2a8")},                                  # a size only: no class block
        "FField": {"Owner": env("0x10"), "NotAKey": env("0x18"), "UEP_TotalSize": env("0x30")},
    })
    doc["vtables"].update({"UEngine": {"Tick": env("0x310")}, "FProperty": {"DestroyValueInternal": env("0x140", "medium"),
                                                                             "InitializeValueInternal": env("0x158", "low")}})
    doc["unresolved"] = [{"id": "structs.UStruct.Children", "why": "not read by any analysed function", "needs": "more-analysis"}]
    doc.update(over)
    return doc


def parse_native_ini(text):
    """The port's file contract (NativeLayoutFiles.hpp), enough to check what the generator writes."""
    lines = text.split("\n")
    assert lines[-1] == "" and "\r" not in text
    sections, cur = {}, None
    for n, line in enumerate(lines[:-1], 1):
        if not line:
            continue
        if line[0] in ";#":
            continue
        if line.startswith("["):
            assert line.endswith("]"), (n, line)
            cur = line[1:-1]
            assert cur not in sections, "duplicate section %s" % cur
            sections[cur] = {}
            continue
        key, _eq, val = line.partition("=")
        assert _eq and cur is not None and key.strip() == key and val.strip() == val, (n, line)
        assert key not in sections[cur], "duplicate key %s.%s" % (cur, key)
        sections[cur][key] = val
    return sections


class ExtendedEmitTests(unittest.TestCase):
    def test_the_default_render_is_the_original_pair_only(self):
        doc = ext_doc()
        member, vtable = emit_ue4ss.render_member_layout(doc), emit_ue4ss.render_vtable_layout(doc)
        self.assertNotIn("[UObjectBase]", member)
        self.assertNotIn("[UEngine]", vtable)
        extended = emit_ue4ss.render_member_layout(doc, extended=True)
        self.assertTrue(extended.startswith(member.rstrip("\n")))                     # the golden-comparable part is a prefix

    def test_only_medium_or_better_members_of_classes_with_a_size_are_written(self):
        sec = parse_native_ini(emit_ue4ss.render_member_layout(ext_doc(), extended=True))
        self.assertEqual(sec["UObjectBase"], {"ObjectFlags": "0x8", "InternalIndex_Private": "0xC", "ClassPrivate": "0x10",
                                              "OuterPrivate": "0x20", "UEP_TotalSize": "0x28"})          # NamePrivate is low
        self.assertEqual(sec["UClass"], {"ClassFlags": "0xD4", "ClassDefaultObject": "0x110", "UEP_TotalSize": "0x200"})
        self.assertNotIn("UStruct", sec)                                              # its size is low: the port would refuse the class
        self.assertNotIn("AActor", sec)                                               # only a size, nothing to bind
        self.assertEqual(sec["FField"], {"Owner": "0x10", "UEP_TotalSize": "0x30"})  # NotAKey is not a key the port accepts
        self.assertEqual(sec["FProperty"], {"ArrayDim": "0x2C", "Offset_Internal": "0x44", "UEP_TotalSize": "0x70"})   # RepIndex is beyond the size

    def test_everything_left_out_is_listed_in_whole_line_comments_with_the_reason(self):
        text = emit_ue4ss.render_member_layout(ext_doc(), extended=True)
        comments = [l for l in text.split("\n") if l.startswith(";")]
        joined = "\n".join(comments)
        self.assertIn("UObjectBase.NamePrivate", joined)
        self.assertIn("confidence low", joined)
        self.assertIn("UStruct.*", joined)
        self.assertIn("UEP_TotalSize is confidence low", joined)
        self.assertIn("FProperty.RepIndex", joined)
        self.assertIn("outside the class size", joined)
        self.assertIn("UStruct.Children", joined)
        self.assertIn("not read by any analysed function", joined)
        self.assertTrue(all(len(l) < 200 for l in comments))
        parse_native_ini(text)                                                        # comments never break the contract

    def test_every_class_written_carries_a_positive_size_and_every_member_lies_inside_it(self):
        sec = parse_native_ini(emit_ue4ss.render_member_layout(ext_doc(), extended=True))
        for cls, entries in sec.items():
            if cls == "NativeLayout":
                continue
            total = int(entries["UEP_TotalSize"], 16)
            self.assertGreater(total, 0, cls)
            for k, v in entries.items():
                if k != "UEP_TotalSize":
                    self.assertLess(int(v, 16), total, (cls, k))

    def test_vtable_sections_only_for_classes_the_port_binds_and_only_medium_or_better(self):
        text = emit_ue4ss.render_vtable_layout(ext_doc(), extended=True)
        sec = parse_native_ini(text)
        self.assertEqual(sec["UEngine"], {"Tick": "0x310"})
        self.assertEqual(sec["FProperty"], {"DestroyValueInternal": "0x140"})        # InitializeValueInternal is low
        self.assertIn("FProperty.InitializeValueInternal", text)
        self.assertEqual(sec["UObject"], {"ProcessEvent": "0x60"})
        saved = emit_ue4ss.EXTENDED_VTABLES
        try:
            emit_ue4ss.EXTENDED_VTABLES = [("UClass", "CreateDefaultObject")]
            doc = ext_doc()
            doc["vtables"]["UClass"] = {"CreateDefaultObject": {"value": "0x400", "confidence": "high", "method": "t", "evidence": []}}
            out = emit_ue4ss.render_vtable_layout(doc, extended=True)
        finally:
            emit_ue4ss.EXTENDED_VTABLES = saved
        self.assertNotIn("[UClass]", out)                                             # the port has no such vtable section
        self.assertIn("no vtable section", out)

    def test_every_key_written_is_canonical_in_the_port_schema(self):
        schema = emit_ue4ss.native_schema()
        sec = parse_native_ini(emit_ue4ss.render_member_layout(ext_doc(), extended=True))
        for cls, entries in sec.items():
            if cls in ("NativeLayout", "FUObjectArray", "TUObjectArray"):
                continue
            for k in entries:
                self.assertIn(k, schema["classes"][cls]["keys"], (cls, k))
                self.assertNotIn(k, schema["classes"][cls]["aliases"], (cls, k))

    def test_no_new_class_means_no_comment_block(self):
        doc = sample_doc()
        text = emit_ue4ss.render_member_layout(doc, extended=True)
        self.assertEqual(text, emit_ue4ss.render_member_layout(doc))

    def test_emit_writes_the_extended_files(self):
        with tempfile.TemporaryDirectory() as d:
            emit_ue4ss.emit(ext_doc(), d)
            sec = parse_native_ini(Path(d, "ue4ss", "MemberVariableLayout.ini").read_text())
            self.assertIn("UObjectBase", sec)
            self.assertIn("UEngine", parse_native_ini(Path(d, "ue4ss", "VTableLayout.ini").read_text()))


# ---------------------------------------------------------------------------------------------
class HardRuleTests(unittest.TestCase):
    """The generator must contain no hand-pinned per-game values (hard rule)."""

    # Townfall values (as hex) that must not appear in the generator source.  Platform constants that
    # every PS5 title shares are written as sizes/expressions there, not as these literals.
    FORBIDDEN_HEX = [
        "9765ec0", "9365ec0", "9765ea0", "9365ea0", "97ba110", "93ba110", "970add8", "930add8", "9750950", "9350950",
        "97089b8", "93089b8", "82c26e0", "7ec26e0", "4b7a20", "b7a20", "4b7a90", "b7a90", "63d590", "23d590",
        "890aa0", "490aa0", "859830", "459830", "88a680", "48a680", "63d600", "23d600", "63a5d0", "23a5d0",
        "646090", "246090", "893530", "493530", "893580", "493580", "33ac8c0", "2fac8c0", "782706e", "799db12",
        "75eefc0", "763151e", "79f02e2", "7999084", "1ef7db", "7826782", "bc5f250", "bc602c8", "c0602c8",
        "400000", "40b660", "b660", "b3f0", "40b3f0", "afd40", "4afd40",
        # addresses the layout detectors find on the same image (label and runtime forms)
        "93b8900", "97b8900", "23f0c0", "63f0c0", "23f150", "63f150", "30d860", "70d860", "2a6e4c0", "6a6e4c0", "2facbae", "33acbae",
        "3222a0", "7222a0", "483b40", "883b40", "485430", "885430", "326ac0", "726ac0", "363990", "763990", "3645f0", "7645f0",
        "3e1b50", "7e1b50", "4577a0", "8577a0", "7ed7a28", "8477708", "82315c0", "7eea438", "3ffed0", "437450", "3ea720",
    ]
    # offsets the layout detectors measure on the same image (only matched in 0x form); the very common round ones (0x8 .. 0x78,
    # 0xb0, 0xe0) are left out because ordinary code uses them for unrelated sizes
    FORBIDDEN_LAYOUT = ["310", "1d8", "158", "110", "140", "170", "d4", "d8", "c8", "b4", "b6", "ba", "bc", "2c", "44", "5c",
                        "9e8", "2a8", "3b8", "340", "388", "2b0"]
    FORBIDDEN_SHORT = ["268", "270", "278"]               # only matched in 0x form (they occur as plain numbers elsewhere)
    FORBIDDEN_TEXT = ["04bb4330a7d71dcefe1cc2c4d19769d5", "5.6.1-0+++UE5", "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc",
                      "c9c3a6877db124b2a805f9317318ed4e0fd91011039f5c37011e0055e89c5834", "PPSA33286", "Townfall",
                      "TOWNFALL", "SIEJ0000", "02.003.000"]

    def sources(self):
        root = PROJECT / "tools" / "profilegen"
        for p in sorted(root.rglob("*")):
            if p.suffix in (".py", ".json") and "__pycache__" not in p.parts:
                yield p

    def test_no_townfall_value_appears_in_the_generator_source(self):
        found = []
        for p in self.sources():
            text = p.read_text(encoding="utf-8")
            low = text.lower()
            for h in self.FORBIDDEN_HEX:
                if re.search(r"(?<![0-9a-f])(0x)?0*%s(?![0-9a-f])" % re.escape(h), low):
                    found.append((p.name, h))
            for h in self.FORBIDDEN_SHORT + self.FORBIDDEN_LAYOUT:
                if re.search(r"0x0*%s(?![0-9a-f])" % h, low):
                    found.append((p.name, "0x" + h))
            for t in self.FORBIDDEN_TEXT:
                if t.lower() in low:
                    found.append((p.name, t))
        self.assertEqual(found, [], "hand-pinned per-game values in the generator source")

    def test_the_scan_itself_would_catch_a_planted_value(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d, "planted.py")
            p.write_text("ADDR = 0x%s\n" % "23D590")
            low = p.read_text().lower()
            self.assertTrue(re.search(r"(?<![0-9a-f])(0x)?0*%s(?![0-9a-f])" % "23d590", low))

    def test_platform_base_is_not_a_hex_literal_but_still_the_ps5_value(self):
        self.assertEqual(platform.PS5_MAIN_MODULE_BASE, 4 * 1024 * 1024)

    def test_generator_has_no_per_title_branching(self):
        for p in self.sources():
            if p.suffix != ".py":
                continue
            text = p.read_text(encoding="utf-8")
            self.assertNotRegex(text, r"title_?id\s*==", p.name)
            self.assertNotRegex(text, r"sha256\s*==\s*[\"']", p.name)


# ---------------------------------------------------------------------------------------------
TOWNFALL_PRESENT = os.path.exists(TOWNFALL_EBOOT) and os.path.exists(TOWNFALL_PARAM)


@unittest.skipUnless(TOWNFALL_PRESENT, "Townfall executable / param.json not available (set MODBRIDGE_TOWNFALL_EBOOT / _PARAM)")
class TownfallValidation(unittest.TestCase):
    """Runs the generator on the real decrypted executable and compares every oracle value."""

    @classmethod
    def setUpClass(cls):
        if not HAVE_CAPSTONE:
            raise unittest.SkipTest("capstone is not installed")
        cls.tmp = tempfile.TemporaryDirectory()
        t0 = time.time()
        cls.result = cli.generate(TOWNFALL_EBOOT, TOWNFALL_PARAM, os.path.join(cls.tmp.name, "profile"),
                                  game_profiles=str(PROJECT / "examples" / "game-profiles"), utc="2000-01-01T00:00:00Z",
                                  strict=True)
        cls.elapsed = time.time() - t0
        cls.doc = cls.result.doc
        cls.out = os.path.join(cls.tmp.name, "profile")
        cls.oracle = json.loads((FIXTURES / "townfall-oracle.json").read_text())
        cls.notes = json.loads((FIXTURES / "townfall-oracle-notes.json").read_text())
        cls.rows = {r["key"]: r for r in validate.compare(cls.doc, cls.oracle, cls.notes, validate.emit_extras(cls.doc))}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    # keys where the generator measures something else than the oracle, each with the value it must measure
    DIVERGENCES = {
        "symbols.gmalloc.label": "0x9350950",                     # what FMemory::Malloc/Free load (ZCode seed: base allocator pointer)
        "symbols.gmalloc.runtime": "0x9750950",
        "structs.FUObjectArray.UObjectCreateListeners": "0x58",   # code puts the TArrays/serial/flag elsewhere than the
        "structs.FUObjectArray.UObjectDeleteListeners": "0x68",   # Windows-derived template the oracle's staged ini used
        "structs.FUObjectArray.PrimarySerialNumber": "0x90",
        "structs.FUObjectArray.bShouldRecycleObjectIndices": "0x94",
        "structs.FUObjectArray.UEP_TotalSize": "0x98",
        "structs.FNameEntry.header": "u16 LE: bit0 wide; length = header >> 6",     # measured only partially
        "emit.MemberVariableLayout.sha256": None,                 # follows from the five fields above
    }
    # keys the generator does not produce (support constants and out-of-scope symbols)
    NOT_PRODUCED = {
        "symbols.gmalloc_vtable.label", "symbols.gmalloc_vtable.runtime",
        "symbols.fname_core.label", "symbols.gmalloc_malloc_impl.label",
        "symbols.gmalloc_slow_path.label", "symbols.process_event_empty_stub_slot76.label",
        "symbols.static_find_object.address_applicable", "symbols.fname_to_string.resolved",
        "ue4ss.settings.Hooks.HookProcessInternal", "ue4ss.settings.Hooks.HookProcessLocalScriptFunction",
    }

    def test_every_oracle_key_is_accounted_for(self):
        self.assertEqual(len(self.oracle), 169)
        for key, row in self.rows.items():
            if key in self.NOT_PRODUCED:
                self.assertEqual(row["match"], "absent", key)
            elif key in self.DIVERGENCES:
                self.assertEqual(row["match"], "differs", key)
            else:
                self.assertEqual(row["match"], "match", "%s: generated %r, oracle %r" % (key, row["generated"], row["oracle"]))

    def test_divergences_are_the_documented_measured_values(self):
        for key, expected in self.DIVERGENCES.items():
            if expected is not None:
                self.assertEqual(self.rows[key]["generated"], expected, key)
            self.assertTrue(self.rows[key]["note"], "divergence %s needs an explanation in the notes file" % key)

    def test_headline_oracle_values_exactly(self):
        leaf = lambda p: validate.resolve(self.doc, p)["value"]
        self.assertEqual(leaf("load.base"), "0x400000")
        self.assertEqual(self.doc["load"]["base"]["confidence"], "medium")
        self.assertEqual(self.doc["load"]["base"]["method"], "load.platform_convention")
        self.assertEqual((leaf("build.engine.major"), leaf("build.engine.minor"), leaf("build.engine.patch")), ("0x5", "0x6", "0x1"))
        self.assertEqual(leaf("build.engine.revision_string"), "5.6.1-0+++UE5")
        self.assertEqual(leaf("build.build_id"), "04bb4330a7d71dcefe1cc2c4d19769d5")
        self.assertEqual((leaf("build.configuration"), leaf("build.non_shipping"), leaf("build.stats")), ("shipping", False, False))
        self.assertEqual((leaf("build.configuration_margin"), leaf("build.stats_margin")), ("0x3c", "0xc"))
        self.assertEqual((leaf("structs.FNamePool.current_block"), leaf("structs.FNamePool.current_byte_offset"),
                          leaf("structs.FNamePool.blocks")), ("0x8", "0xc", "0x10"))
        for sym, runtime in (("name_pool", "0x9765ec0"), ("object_array", "0x97ba110"), ("fname_ctor_wchar", "0x63d590"),
                             ("static_construct_object_internal", "0x890aa0"), ("fmemory_malloc", "0x4b7a20"),
                             ("fmemory_free", "0x4b7a90"), ("process_event", "0x859830"), ("gmalloc_base_slot", "0x970add8"),
                             ("gmalloc_effective_ptr", "0x9750950")):
            self.assertEqual(leaf("symbols.%s.runtime" % sym), runtime, sym)
        self.assertEqual(leaf("vtables.UObject.ProcessEvent"), "0x268")

    def test_gmalloc_reports_both_candidates_and_prefers_what_malloc_and_free_load(self):
        g = self.doc["symbols"]
        self.assertEqual(g["gmalloc"]["label"]["value"], g["gmalloc_effective_ptr"]["label"]["value"])
        self.assertNotEqual(g["gmalloc"]["label"]["value"], g["gmalloc_base_slot"]["label"]["value"])
        self.assertEqual(g["gmalloc_base_slot"]["label"]["value"], "0x930add8")        # ZCode's seed, derived independently
        ev = " ".join(g["gmalloc"]["label"]["evidence"])
        self.assertIn("FMemory::Malloc", ev)
        self.assertIn("FMemory::Free", ev)

    def test_static_find_object_is_found_with_a_stated_confidence(self):
        env = self.doc["symbols"]["static_find_object"]["label"]
        self.assertEqual(env["value"], "0x48a680")
        self.assertEqual(env["confidence"], "medium")
        self.assertEqual(self.doc["symbols"]["static_find_object"]["runtime"]["value"], "0x88a680")

    def test_generated_ini_files(self):
        vt = Path(self.out, "ue4ss", "VTableLayout.ini").read_bytes()
        legacy_vt = emit_ue4ss.render_vtable_layout(self.doc).encode("ascii")
        self.assertEqual(hashlib.sha256(legacy_vt).hexdigest(), "37a7d86036106d1042c10c10e11f66edef4ba7c2b2a2948c8aaeea719ee9a8a5",
                         "the original sections are byte-identical to ZCode's staged VTableLayout.ini")
        self.assertTrue(vt.startswith(legacy_vt.rstrip(b"\n")))
        mv = Path(self.out, "ue4ss", "MemberVariableLayout.ini").read_text()
        legacy_mv = emit_ue4ss.render_member_layout(self.doc)
        self.assertTrue(mv.startswith(legacy_mv.rstrip("\n")))
        golden_fields = {"UObjectCreateListeners": "0x68", "UObjectDeleteListeners": "0x78", "PrimarySerialNumber": "0xB0",
                         "bShouldRecycleObjectIndices": "0xB4"}
        patched = legacy_mv
        for k, v in golden_fields.items():
            patched = re.sub(r"(?m)^%s=0x[0-9A-F]+$" % k, "%s=%s" % (k, v), patched)
        patched = patched.replace("UEP_TotalSize=0x98", "UEP_TotalSize=0xB8")
        self.assertEqual(hashlib.sha256(patched.encode()).hexdigest(),
                         "f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32",
                         "with the five divergent fields set to the template values the original classes are byte-identical to the staged file")
        st = Path(self.out, "ue4ss", "UE4SS-settings.ini").read_text()
        self.assertIn("[EngineVersionOverride]\nMajorVersion=5\nMinorVersion=6\n", st)
        self.assertEqual({e["path"] for e in self.doc["emit"]},
                         {"ue4ss/VTableLayout.ini", "ue4ss/MemberVariableLayout.ini", "ue4ss/UE4SS-settings.ini"})

    def test_envelopes_and_status(self):
        self.assertEqual(self.doc["status"], "generated")
        self.assertIsNone(self.doc["verification"])
        n = 0
        for path, env in profile.iter_envelopes(self.doc):
            n += 1
            self.assertIn(env["confidence"], ("high", "medium", "low"), path)
            self.assertTrue(env["method"], path)
            if env["confidence"] != "low" and not path.startswith(("elf.", "binding.")):
                self.assertTrue(env["evidence"], "%s has no evidence" % path)
        self.assertGreater(n, 150)

    def test_runtime_equals_label_plus_base_everywhere(self):
        base = int(self.doc["load"]["base"]["value"], 16)
        for name, sym in self.doc["symbols"].items():
            if "label" in sym and "runtime" in sym:
                self.assertEqual(int(sym["runtime"]["value"], 16), int(sym["label"]["value"], 16) + base, name)

    def test_runtime_is_reasonable(self):
        self.assertLess(self.elapsed, 60, "generation took %.1fs" % self.elapsed)
        print("\n[profilegen] Townfall generation: %.1fs  %s" % (self.elapsed, {k: round(v, 1) for k, v in self.result.timings.items() if v >= 1}),
              file=sys.stderr)

    def test_no_detector_crashed(self):
        crashed = [u for u in self.doc["unresolved"] if u["id"].startswith("detector.")]
        self.assertEqual(crashed, [])
        self.assertEqual(sorted(u["id"] for u in self.doc["unresolved"]), sorted(self.UNRESOLVED))
        for u in self.doc["unresolved"]:
            self.assertTrue(u["why"], u["id"])

    # ---- layout detectors -----------------------------------------------------------------------------------------
    LAYOUT_KNOWN = json.loads((FIXTURES / "townfall-layout-known.json").read_text())
    # measured on this image by another offline analysis but left unresolved here (the profile says why)
    KNOWN_NOT_DERIVED = {"structs.UClass.Interfaces", "vtables.UEngine.LoadMap", "vtables.AGameModeBase.InitGameState",
                         "vtables.FProperty.CopyValuesInternal"}
    # the PS5 image differs from the UE4SS 5.06 Windows template in exactly these (tail-padding reuse, one more destructor slot, ...)
    TEMPLATE_DIFFERS = {"structs.FProperty.ArrayDim", "structs.FProperty.ElementSize", "structs.UGameViewportClient.UEP_TotalSize",
                        "vtables.FProperty.DestroyValueInternal", "vtables.FProperty.GetMinAlignment", "vtables.FProperty.InitializeValueInternal",
                        "vtables.UClass.CreateDefaultObject", "vtables.UEngine.Tick"}
    SOLO_HIGH_METHODS = ("class_registration.sizeof", "class_registration.inline_immediates", "field_classes.allocation_size",
                         "uobject_base.extent", "engine_tick.anchor_string", "bytes", "strings", "fname_tostring", "gnatives.dispatch_table",
                         "class_registration.package_literal_calls", "field_classes.name_registrations", "property_subtypes.alignment_constants")

    def test_every_value_another_offline_analysis_measured_is_reproduced(self):
        known = self.LAYOUT_KNOWN["static"]
        derived = 0
        for path, spec in known.items():
            env = validate.resolve(self.doc, path)
            if path in self.KNOWN_NOT_DERIVED:
                self.assertIsNone(env, path)
                self.assertIn(path, [u["id"] for u in self.doc["unresolved"]], path)
                continue
            self.assertIsNotNone(env, path)
            self.assertEqual(int(env["value"], 16), int(spec["value"], 16), "%s: generated %s, known %s (%s)" % (
                path, env["value"], spec["value"], spec["source"]))
            derived += 1
        self.assertGreaterEqual(derived, 70)

    def test_where_the_windows_template_differs_the_measured_value_is_what_the_image_shows(self):
        differs = set()
        for path, t in self.LAYOUT_KNOWN["template"].items():
            env = validate.resolve(self.doc, path)
            if env is not None and int(env["value"], 16) != int(t, 16):
                differs.add(path)
        self.assertEqual(differs, self.TEMPLATE_DIFFERS)
        # each of them is also pinned by the other analysis (so the difference is measured twice, not assumed)
        for path in self.TEMPLATE_DIFFERS & set(self.LAYOUT_KNOWN["static"]):
            self.assertEqual(int(validate.resolve(self.doc, path)["value"], 16), int(self.LAYOUT_KNOWN["static"][path]["value"], 16))

    def test_every_new_value_has_evidence_and_high_means_two_channels_or_a_structural_invariant(self):
        new = 0
        for path, env in profile.iter_envelopes(self.doc):
            if not path.startswith(("structs.U", "structs.F", "structs.A", "vtables.UEngine", "vtables.FProperty", "vtables.UClass",
                                    "symbols.engine_tick", "symbols.fname_to_string", "symbols.gnatives", "symbols.fframe_step",
                                    "symbols.class_registration_helper", "symbols.ffield_class_ctor", "symbols.allocate_uobject_index",
                                    "symbols.create_default_object", "symbols.uclass_vtable")) or path.startswith("structs.FUObjectArray") or \
                    path.startswith("structs.FName") or path.startswith("structs.FUObjectItem.stride"):
                continue
            new += 1
            self.assertIn(env["confidence"], ("high", "medium", "low"), path)
            self.assertTrue(env["method"], path)
            self.assertTrue(env["evidence"], path)
            if env["confidence"] == "high" and path.startswith("structs."):
                text = " ".join(env["evidence"])
                self.assertTrue("independent channels agree" in text or any(m in env["method"] for m in self.SOLO_HIGH_METHODS),
                                "%s is high without a second channel or a structural invariant: %s" % (path, env["method"]))
        self.assertGreater(new, 100)

    def test_selected_confidences(self):
        c = lambda p: validate.resolve(self.doc, p)["confidence"]
        for p in ("structs.UObjectBase.ObjectFlags", "structs.UObjectBase.InternalIndex_Private", "structs.UObjectBase.ClassPrivate",
                  "structs.UObjectBase.NamePrivate", "structs.UObjectBase.OuterPrivate", "structs.UObjectBase.UEP_TotalSize",
                  "structs.UStruct.PropertiesSize", "structs.UStruct.MinAlignment", "structs.UStruct.ChildProperties", "structs.FField.Next",
                  "structs.FFieldClass.CastFlags", "structs.FFieldClass.UEP_TotalSize", "structs.FProperty.ArrayDim",
                  "structs.FStructProperty.Struct", "structs.FEnumProperty.UnderlyingProp", "structs.FBoolProperty.ByteOffset",
                  "structs.FBoolProperty.FieldMask", "vtables.UEngine.Tick", "vtables.FProperty.GetMinAlignment", "symbols.engine_tick.label",
                  "symbols.gnatives.label", "symbols.fname_to_string.label", "symbols.fname_to_string_value.label"):
            self.assertEqual(c(p), "high", p)
        for p in ("structs.UFunction.FunctionFlags", "structs.UFunction.Func", "structs.UClass.ClassDefaultObject", "structs.UStruct.SuperStruct",
                  "structs.UStruct.Children", "structs.UField.Next", "structs.FField.UEP_TotalSize", "structs.UClass.ClassFlags",
                  "vtables.FProperty.DestroyValueInternal"):
            self.assertEqual(c(p), "medium", p)
        self.assertEqual(c("structs.UStruct.PostConstructLink"), "low")
        self.assertEqual(c("structs.FFieldClass.ClassFlags"), "low")

    def test_new_symbols_carry_runtime_addresses(self):
        for name, runtime in (("engine_tick", "0x33ac8c0"), ("gnatives", "0x97b8900"), ("fname_to_string", "0x63f150"),
                              ("fname_to_string_value", "0x63f0c0"), ("fframe_step", "0x8577a0")):
            self.assertEqual(self.doc["symbols"][name]["runtime"]["value"], runtime, name)

    def test_generated_layout_files_follow_the_port_contract(self):
        schema = emit_ue4ss.native_schema()["classes"]
        text = Path(self.out, "ue4ss", "MemberVariableLayout.ini").read_text()
        sec = parse_native_ini(text)
        written = 0
        for cls, entries in sec.items():
            if cls in ("NativeLayout", "FUObjectArray", "TUObjectArray"):
                continue
            total = int(entries["UEP_TotalSize"], 16)
            self.assertGreater(total, 0, cls)
            for key, val in entries.items():
                self.assertIn(key, schema[cls]["keys"], (cls, key))
                env = validate.resolve(self.doc, "structs.%s.%s" % (cls, key))
                self.assertIsNotNone(env, (cls, key))
                self.assertIn(env["confidence"], ("high", "medium"), (cls, key))
                self.assertEqual(int(val, 16), int(env["value"], 16), (cls, key))
                if key != "UEP_TotalSize":
                    self.assertLess(int(val, 16), total, (cls, key))
                    written += 1
        self.assertGreater(written, 60)
        # nothing below medium was written, and the omissions are listed
        self.assertNotIn("PostConstructLink=", text)
        self.assertNotIn("ClassFlags=0x18", text)
        comments = "\n".join(l for l in text.split("\n") if l.startswith(";"))
        self.assertIn("UStruct.PostConstructLink", comments)
        self.assertIn("UClass.Interfaces", comments)
        self.assertIn("FFieldClass.ClassFlags", comments)
        vt = parse_native_ini(Path(self.out, "ue4ss", "VTableLayout.ini").read_text())
        self.assertEqual(vt["UObject"], {"ProcessEvent": "0x268"})
        self.assertEqual(vt["UEngine"], {"Tick": "0x310"})
        self.assertEqual(vt["FProperty"], {"DestroyValueInternal": "0x140", "InitializeValueInternal": "0x158",
                                           "GetMinAlignment": "0x170"})   # GetMinAlignment now emitted (Team S3)

    # everything the generator could not derive on this image, each with its reason in the profile (see the README table)
    UNRESOLVED = ["version_map.pc_build", "vtables.FProperty.CopyValuesInternal", "vtables.UEngine.LoadMap",
                  "vtables.AGameModeBase.InitGameState", "structs.UFunction.RepOffset", "structs.UStruct.ScriptAndPropertyObjectReferences",
                  "structs.UStruct.ScriptObjectReferences", "structs.UStruct.StructStateFlags", "structs.UStruct.UnresolvedScriptProperties"] + [
        "structs.UClass." + k for k in (
            "AllFunctionsCache", "AllFunctionsCacheLock", "ClassAddReferencedObjects", "ClassGeneratedBy", "ClassUnique", "FirstOwnedClassRep",
            "FuncMap", "FuncMapLock", "InterfaceFuncMap", "Interfaces", "NativeFunctionLookupTable", "NetFields", "ParentFuncMap",
            "PropertiesStartOffset", "ReferenceSchema", "ReferenceTokenStream", "SparseClassData", "SparseClassDataStruct", "SuperFuncMap",
            "UberGraphFramePointerProperty", "bCooked", "bLayoutChanging", "bNeedsDynamicSubobjectInstancing",
            "bNeedsPostLoadSubobjectInstancing")] + [
        "structs.FUObjectItem." + k for k in ("ClusterAndFlags", "ClusterIndex", "FlagsAndRefCount", "ObjectPtrLow", "RemoteId")]


if __name__ == "__main__":
    unittest.main()
