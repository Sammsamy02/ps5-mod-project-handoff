"""Robustness tests for tools/profilegen: what happens when an input is not the one game it was validated on.

The generator was validated on one title.  These tests (all synthetic, all always run unless capstone is missing)
check three things:

1. Unusable inputs are refused cleanly, before anything is written: an encrypted SELF (exit 5), a missing or
   malformed ``param.json``, a stripped or odd ELF (no program headers, overlapping or zero-size segments, wrong
   machine or class), a truncated file.  Header and segment-table mutations never crash a detector.
2. A different engine version is reported as that version or left unresolved, never as the validated one.
3. Every assumption listed in the README ("Assumptions that may not hold on another title") is violated by a
   synthetic image, and the affected values come out unresolved or capped, never wrong.

The fixtures are self-contained (they use only tests/fixtures/profilegen/synth.py) and use arbitrary numbers.
"""
import hashlib
import io
import json
import os
import random
import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT))
sys.path.insert(0, str(PROJECT / "tests" / "fixtures" / "profilegen"))

from synth import (R12, R13, R14, R15, RAX, RBP, RBX, RCX, RDI, RDX, RSI, RSP, Asm, Seg, ascii0, build_elf,  # noqa: E402
                   gnu_note, utf16)

from tools.profilegen import cli, detectors, emit_seedtable, emit_ue4ss, profile  # noqa: E402
from tools.profilegen.detectors import (build_axes, engine_version, fname_ctor, fname_pool, gmalloc,  # noqa: E402
                                        platform, process_event, static_construct, static_find_object,
                                        uobject_array)
from tools.profilegen.elf import ElfError, ElfImage, EncryptedSelfError  # noqa: E402
from tools.profilegen.index import Decoder, ImageIndex  # noqa: E402
from tools.profilegen.model import Context  # noqa: E402

needs_capstone = unittest.skipUnless(Decoder.available, "capstone is not installed")

CODE_VA, RO_VA, RW_VA = 0x1000, 0x6000, 0x9000
RW_FILESZ, RW_MEMSZ = 0x3000, 0x8000
TOWNFALL_EBOOT = os.environ.get(
    "MODBRIDGE_TOWNFALL_EBOOT",
    "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/"
    "townfall-static-executable-2026-10-02/decrypted-eboot.bin")


# ---------------------------------------------------------------------------------------------
class Img:
    """One synthetic image.  ``pad`` puts int3 padding before each function, ``frame`` gives it a ``push rbp; mov rbp,rsp`` prologue;
    switching both off removes everything the function map is built from."""

    def __init__(self, pad: bool = True, frame: bool = True) -> None:
        self.pad, self.frame = pad, frame
        self.a = Asm(CODE_VA)
        self.a.int3(1)
        self.ro = bytearray(16)
        self._bss = RW_VA + RW_FILESZ + 0x100
        self.relas = []

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

    def func(self, name: str) -> Asm:
        if self.pad:
            self.a.int3(1)
            self.a.align(16)
        self.a.label(name)
        if self.frame:
            self.a.prologue()
        return self.a

    def image(self, **kw) -> bytes:
        code = self.a.finish()
        assert len(code) < 0x4000
        segs = [Seg(CODE_VA, code, 5), Seg(RO_VA, bytes(self.ro), 4), Seg(RW_VA, bytes(self.rw()), 6, memsz=RW_MEMSZ)]
        relas = kw.pop("relas", None)
        if relas is None and self.relas:
            relas = self.relas
        return build_elf(segs, relas=relas, **kw)

    def rw(self) -> bytes:
        return bytes(RW_FILESZ)


def make_ctx(image: bytes, param=None, options=None) -> Context:
    elf = ElfImage(image)
    return Context(elf, ImageIndex(elf), param, options)


def only_the_truth(test: unittest.TestCase, ctx: Context, truth: dict, where: str = "") -> None:
    """Every value the detectors resolved must be the true one (a wrong value fails whatever its confidence)."""
    for path, want in truth.items():
        env = ctx.get(path)
        if env is not None:
            test.assertEqual(int(env.value), want, "%s%s resolved to a wrong value %r (%s)" % (where, path, env.value, env.confidence))


def unresolved_ids(ctx: Context) -> set:
    return {u.id for u in ctx.unresolved}


def not_high(test: unittest.TestCase, ctx: Context, path: str, where: str = "") -> None:
    env = ctx.get(path)
    test.assertTrue(env is None or env.confidence != "high", "%s%s is %s without the evidence its method needs" % (
        where, path, env and env.confidence))


# ---------------------------------------------------------------------------------------------
# fixtures
NAMES = ["None", "ByteProperty", "IntProperty", "BoolProperty", "FloatProperty", "ObjectProperty", "NameProperty",
         "DelegateProperty", "DoubleProperty", "ArrayProperty", "StructProperty", "VectorProperty", "RotatorProperty",
         "StrProperty", "TextProperty", "InterfaceProperty", "MulticastDelegateProperty", "WeakObjectProperty",
         "LazyObjectProperty", "SoftObjectProperty"]


class Pool:
    """FNamePool::Init + FMemory::Malloc/Free + lazy-create world.  ``layout_names`` of the 20 default names are registered in the
    function that also holds the pool layout (memset, block allocation, id stores); the rest in a separate registrar."""

    CUR, BLOCK, ALIGN, NBLOCKS = 0x10, 0x8000, 4, 4096
    DISP_MALLOC, DISP_FREE = 0x38, 0x58
    HASH, STRIDE, NONE_ID, BYTE_ID = 0x8020, 0x20, 0x9020, 0x9024

    def __init__(self, pad=True, frame=True, layout_names=None, registrars=1, malloc_is_dispatcher=True):
        im = self.im = Img(pad, frame)
        base = im.bss(0x20000)
        self.flag, self.pool = base + 0x10, base + 0x40
        self.G, self.SLOT, self.INST = im.bss(8), im.bss(8), im.bss(0x100)
        lits = [im.ro_str(n) for n in NAMES]
        im.func("memset").epilogue()
        im.func("store").xor_r32(RAX).epilogue()
        a = im.func("malloc")
        if malloc_is_dispatcher:
            a.push(RBX).lea_rip(R14, self.G).mov_rr(RBX, RDI).mov_rr32(RDX, RSI).load64(RDI, R14, 0)
            a.load64(RAX, RDI, 0).mov_rr(RSI, RBX).call_mem(RAX, self.DISP_MALLOC).pop(RBX).epilogue()
        else:                                           # not the global -> vtable -> call shape: some other allocator wrapper
            a.push(RBX).mov_rr(RBX, RDI).mov_r32_imm(RAX, 0x1000).xor_r32(RDX).pop(RBX).epilogue()
        a = im.func("free")
        a.push(RBX).lea_rip(R14, self.G).mov_rr(RBX, RDI).load64(RDI, R14, 0).load64(RAX, RDI, 0)
        a.mov_rr(RSI, RBX).call_mem(RAX, self.DISP_FREE).pop(RBX).epilogue()
        for k in range(30):
            im.func("free_user%d" % k).call("free").epilogue()
        for k in range(5):
            im.func("malloc_user%d" % k).call("malloc").epilogue()
        n_layout = len(NAMES) if layout_names is None else layout_names
        groups = [list(zip(NAMES, lits))[:n_layout]]
        rest = list(zip(NAMES, lits))[n_layout:]
        for i in range(registrars if rest else 0):
            groups.append(rest[i::registrars])
        for gi, group in enumerate(groups):
            a = im.func("init" if gi == 0 else "reg%d" % gi)
            a.push(RBX).mov_rr(RBX, RDI)
            if gi == 0:
                a.lea_rm(RDI, RBX, self.CUR).mov_r32_imm(RDX, 8 + 8 * self.NBLOCKS).xor_r32(RSI).call("memset")
                a.mov_r32_imm(RDI, self.BLOCK).mov_r32_imm(RSI, self.ALIGN).call("malloc").store64(RBX, self.CUR + 8, RAX)
                a.lea_rm(R14, RBX, self.HASH).add_r64_imm8(R14, self.STRIDE)
            for n, lit in group:
                a.lea_rip(RSI, lit).mov_r32_imm(RDX, len(n)).mov_rr(RDI, RBX).call("store")
                if n == "None":
                    a.store32(RBX, self.NONE_ID, RAX)
                if n == "ByteProperty":
                    a.store32(RBX, self.BYTE_ID, RAX)
            a.pop(RBX).epilogue()
        a = im.func("lazy")
        a.cmp8_rip_imm(self.flag, 0)
        for gi in range(len(groups)):
            a.lea_rip(RDI, self.pool).call("init" if gi == 0 else "reg%d" % gi)
        a.mov8_rip_imm(self.flag, 1).epilogue()
        for k in range(4):
            a = im.func("reader%d" % k)
            a.lea_rip(RCX, self.pool).shr_r32(RAX, 16).load64_sib(RAX, RCX, RAX, 8, self.CUR + 8)
            a.raw(b"\x0f\xb7\x04\x48").epilogue()
        a = im.func("grow")
        a.push(RBX).mov_rr(RBX, RDI).load32(RCX, RBX, self.CUR).load32(RDX, RBX, self.CUR + 4)
        a.mov_r32_imm(RDI, self.BLOCK).mov_r32_imm(RSI, self.ALIGN).call("malloc")
        a.store32(RBX, self.CUR, RCX).store32_imm(RBX, self.CUR + 4, 0).store64_sib(RBX, RCX, 8, self.CUR + 8, RAX)
        a.pop(RBX).epilogue()
        im.func("acc").mov_load_rip(RAX, self.SLOT).epilogue()
        im.func("gcreate").call("acc").lea_rip(R13, self.G).store64(R13, 0, RAX).epilogue()
        im.func("ctor").epilogue()
        a = im.func("factory")
        a.lea_rip(RBX, self.INST).xor_r32(RSI).mov_rr(RDI, RBX).call("ctor").mov_store_rip(self.SLOT, RBX).epilogue()
        self.labels = im.a.labels

    def image(self, **kw) -> bytes:
        return self.im.image(**kw)

    @property
    def truth(self) -> dict:
        L = self.labels
        return {"symbols.name_pool.label": self.pool, "symbols.fnamepool_init.label": L["init"],
                "symbols.fmemory_malloc.label": L["malloc"], "symbols.fmemory_free.label": L["free"],
                "symbols.gmalloc.label": self.G, "symbols.gmalloc_base_slot.label": self.SLOT,
                "symbols.name_pool_init_flag.label": self.flag, "symbols.fnamepool_reserve.label": L["grow"],
                "structs.FNamePool.blocks": self.CUR + 8, "structs.FNamePool.none_id": self.NONE_ID,
                "structs.FNamePool.byte_property_id": self.BYTE_ID, "structs.FNamePool.current_block": self.CUR,
                "structs.FNamePool.block_size": self.BLOCK, "vtables.FMalloc.Malloc": self.DISP_MALLOC,
                "vtables.FMalloc.Free": self.DISP_FREE}


def run_pool(fx: Pool) -> Context:
    ctx = make_ctx(fx.image())
    platform.detect(ctx)
    fname_pool.detect(ctx)
    gmalloc.detect(ctx)
    return ctx


class ObjArray:
    """FUObjectArray member functions + 130 IndexToObject users.  ``num_delta`` is the distance of the bounds-checked u32 from
    the chunk-table pointer (UE: 0x14); ``chunked`` False drops the ``index >> 16`` step; ``strings`` keeps the anchor strings."""

    OBJ, CREATE, DELETE, RECYCLE, NEXT = 0x18, 0x60, 0x70, 0x88, 0x90

    def __init__(self, num_delta=0x14, shift=16, chunked=True, strings=True, anchor_functions=3, pad=True, frame=True,
                 bounds_check=True):
        im = self.im = Img(pad, frame)
        self.G = im.bss(0x300)
        path = im.ro_str(r".\Runtime\CoreUObject\Private\UObject\UObjectArray.cpp") if strings else im.ro_str("Private\\Other\\File.cpp")
        msg = im.ro_str("Maximum number of UObjects (%d) exceeded", wide=True) if strings else im.ro_str("an unrelated message", wide=True)
        obj, num = self.OBJ, self.OBJ + num_delta
        a = im.func("m_alloc")
        a.push(R14).mov_rr(R14, RDI).lea_rip(RDI, path).lea_rip(RDX, msg).lea_rip(RAX, self.G)
        a.cmp8_mem_imm(R14, 0xC, 1).cmp32_mem_reg(R14, num, R15).load64(RDX, R14, obj)
        a.load32(R12, R14, obj + 0x10).store32_imm(R14, obj + 0x1C, 1)
        a.load64(RAX, R14, self.CREATE).movsxd_load(RCX, R14, self.CREATE + 8).load64_sib(RDI, RAX, R12, 8, 0)
        a.load32(RAX, R14, 0).load32(RCX, R14, 4).load32(RDX, R14, 8)
        a.pop(R14).epilogue()
        if anchor_functions >= 2:
            a = im.func("m_delete")
            a.push(R14).mov_rr(R14, RDI).lea_rip(RDI, path).lea_rip(RAX, self.G)
            a.load64(RAX, R14, self.DELETE).movsxd_load(RCX, R14, self.DELETE + 8).load64_sib(RDI, RAX, R13, 8, 0)
            a.pop(R14).epilogue()
        if anchor_functions >= 3:
            a = im.func("m_free")
            a.push(R14).mov_rr(R14, RDI).lea_rip(RDI, path).lea_rip(RAX, self.G)
            a.cmp8_mem_imm(R14, self.RECYCLE, 1).pop(R14).epilogue()
        other = self.G + self.NEXT
        for k in range(130):
            a = im.func("user%d" % k)
            a.lea_rip(RAX, self.G)
            if bounds_check:
                a.cmp32_mem_reg(RAX, num, R15)
            a.load64(RCX, RAX, obj)
            if chunked:
                a.shr_r32(RDX, shift)
            a.load64_sib(RCX, RCX, RDX, 8, 0)
            a.raw(b"\x48\x8d\x14\x52")
            a.lea_rip(RSI, other).epilogue()

    def image(self, **kw) -> bytes:
        return self.im.image(**kw)


def run_uoa(fx: ObjArray) -> Context:
    ctx = make_ctx(fx.image())
    platform.detect(ctx)
    uobject_array.detect(ctx)
    return ctx


class Vtables:
    """30 vtable runs of 32 slots in which ProcessEvent (native test + two adjacent virtual calls) sits at slot 12.
    ``prefix`` selects what slots 0 and 1 look like: ``pair`` (two distinct adjacent functions, the Itanium complete +
    deleting destructors), ``far`` (two distinct functions 0x400 bytes apart), ``same`` (one function twice)."""

    SLOT, FLAGS, N_VT, N_SLOT = 12, 0x98, 30, 32

    def __init__(self, prefix="pair", pad=True, frame=True, native_test=True):
        im = self.im = Img(pad, frame)
        a = im.func("pe")
        a.push(RBX).mov_rr(RBX, RDI)
        if native_test:
            a.test8_mem_imm(RSI, self.FLAGS + 1, 4)
        a.load64(RAX, RDI, 0).call_mem(RAX, (self.SLOT + 1) * 8).load64(RAX, RDI, 0).call_mem(RAX, (self.SLOT + 2) * 8)
        a.raw(b"\x90" * 0xD0).pop(RBX).epilogue()
        names = []
        for k in range(self.N_SLOT):
            a = im.func("stub%d" % k)
            a.xor_r32(RAX).epilogue()
            if prefix == "far" and k == 0:
                a.raw(b"\x90" * 0x400)
            names.append("stub%d" % k)
        lay = im.a.labels
        relas, off = [], RW_VA
        for v in range(self.N_VT):
            relas.append((off + 8, RO_VA + 16))
            base = off + 16
            for k in range(self.N_SLOT):
                if k == self.SLOT:
                    fn = lay["pe"]
                elif k == 1 and prefix == "same":
                    fn = lay[names[0]]
                else:
                    fn = lay[names[k]]
                relas.append((base + 8 * k, fn))
            off = base + 8 * self.N_SLOT + 16
        assert off < RW_VA + RW_FILESZ
        self.labels = dict(lay)
        im.relas = relas

    def image(self, **kw) -> bytes:
        return self.im.image(**kw)


def run_pe(fx: Vtables) -> Context:
    ctx = make_ctx(fx.image())
    platform.detect(ctx)
    process_event.detect(ctx)
    return ctx


def sco_image(pad=True, frame=True, sites=25, string=True):
    im = Img(pad, frame)
    lit = im.ro_str("NewObject with empty name can't be used to create default subobjects (inside of UObject derived class)", wide=True) \
        if string else im.ro_str("some other message", wide=True)
    im.func("assert_fn").epilogue()
    im.func("params_ctor").epilogue()
    a = im.func("sco")
    for r in (R15, R14, R13, R12, RBX):
        a.push(r)
    a.xor_r32(RAX).pop(RBX).epilogue()
    for k in range(sites):
        a = im.func("newobject%d" % k)
        a.lea_rip(RSI, lit).mov_rr(RDI, R15).call("assert_fn")
        a.lea_rm(R13, RBP, -0xB0).mov_rr(RSI, R12).mov_rr(RDI, R13).call("params_ctor")
        a.mov_rr(RDI, R13).call("sco").epilogue()
    return im.image(), dict(im.a.labels)


def find_image(pad=True, frame=True, messages=True):
    im = Img(pad, frame)
    ma = im.ro_str("Illegal call to StaticFindObjectFast() while serializing object data!", wide=True) if messages \
        else im.ro_str("unrelated text", wide=True)
    mb = im.ro_str("Illegal call to StaticFindObjectFast() while garbage collecting!", wide=True) if messages \
        else im.ro_str("more unrelated text", wide=True)
    im.func("inner").epilogue()
    a = im.func("find_string")
    a.push(RBX).mov_rr(RBX, RDI)
    a.cmp16_mem_imm(RDX, 0, 0).cmp16_mem_imm(RDX, 2, 0).cmp_r64_imm8(RSI, 0xFF)
    a.lea_rip(RDX, ma).lea_rip(RCX, mb).raw(b"\x90" * 0x120).call("inner").pop(RBX).epilogue()
    for k in range(8):
        im.func("user%d" % k).call("find_string").epilogue()
    return im.image(), dict(im.a.labels)


MODULES = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore"]


def ctor_image(pad=True, frame=True, anchors=True):
    im = Img(pad, frame)
    lits = {m: im.ro_str(m if anchors else m + "X", wide=True) for m in MODULES}
    im.func("intern").xor_r32(RAX).epilogue()
    a = im.func("ctor")
    a.push(RBX).mov_rr(RBX, RDI).mov_rr(RDI, RSI).call("intern").store64(RBX, 0, RAX).pop(RBX).epilogue()
    a = im.func("registration")
    for k, m in enumerate(MODULES):
        a.lea_rip(RSI, lits[m]).lea_rm(RDI, RSP, 0x20 + 8 * k).mov_r32_imm(RDX, 1).call("ctor")
    a.epilogue()
    return im.image(), dict(im.a.labels)


def version_image(major=5, minor=3, patch=2, changelist=12345, branch="UE5", pad=True, frame=True, extra_markers=()):
    im = Img(pad, frame)
    for m in extra_markers:
        im.ro_str(m, wide=True)
    s = im.ro_str(branch, wide=True)
    im.func("leaf_cl").mov_r32_imm(RAX, changelist).epilogue()
    im.func("leaf_lic").xor_r32(RAX).epilogue()
    im.func("leaf_branch").lea_rip(RAX, s).epilogue()
    a = im.func("init")
    a.push(RBX).mov_rr(RBX, RDI)
    a.call("leaf_cl").mov_rr32(R14, RAX).call("leaf_lic").movzx_al(R15).shl_r32(R15, 31).or_rr32(R15, R14)
    a.call("leaf_branch").mov_rr(RSI, RAX)
    a.raw(b"\xc7\x83" + struct.pack("<iI", 0, (minor << 16) | major))
    a.raw(b"\x66\xc7\x83" + struct.pack("<iH", 4, patch))
    a.store32(RBX, 8, R15)
    a.pop(RBX).epilogue()
    return im.image(notes=[gnu_note(bytes(range(16)) + bytes(4))])


# ---------------------------------------------------------------------------------------------
# ELF image editing (the tests build a good image, then damage one thing)
def phdr_table(img: bytes):
    phoff, = struct.unpack_from("<Q", img, 32)
    entsize, num = struct.unpack_from("<HH", img, 54)
    return phoff, entsize, num


def read_phdrs(img: bytes):
    phoff, entsize, num = phdr_table(img)
    return [list(struct.unpack_from("<IIQQQQQQ", img, phoff + i * entsize)) for i in range(num)]


def write_phdrs(img: bytes, entries) -> bytes:
    """Replace the program header table (the count may change; the padding before the first segment has room)."""
    buf = bytearray(img)
    phoff, entsize, _num = phdr_table(img)
    for i, e in enumerate(entries):
        buf[phoff + i * entsize:phoff + (i + 1) * entsize] = struct.pack("<IIQQQQQQ", *e)
    struct.pack_into("<H", buf, 56, len(entries))
    return bytes(buf)


def good_image() -> bytes:
    return Pool().image(notes=[gnu_note(bytes(range(16)) + bytes(4))],
                        relas=[(RW_VA + 8 * k, CODE_VA + 16 * k) for k in range(40)],
                        procparam=struct.pack("<Q", 0x60) + b"ORBI" + struct.pack("<IQ", 5, 0x1100004012590001))


class WorkDir(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def path(self, name: str) -> str:
        return os.path.join(self.tmp.name, name)

    def write(self, name: str, data) -> str:
        p = self.path(name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        Path(p).write_bytes(data if isinstance(data, bytes) else data.encode("utf-8"))
        return p

    def run_cli(self, *args):
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(list(args))
        return rc, buf.getvalue()

    def generate(self, image: bytes, param: dict = None, **kw):
        eboot = self.write("e%d.bin" % len(os.listdir(self.tmp.name)), image)
        pp = self.write("p%d.json" % len(os.listdir(self.tmp.name)), json.dumps(param)) if param is not None else None
        out = self.path("out%d" % len(os.listdir(self.tmp.name)))
        return cli.generate(eboot, pp, out, utc="2000-01-01T00:00:00Z", **kw), out


# ---------------------------------------------------------------------------------------------
class EncryptedSelfTests(WorkDir):
    MAGICS = (b"\x4f\x15\x3d\x1d", b"\x54\x14\xf5\xee")
    REAL_SELFS = (
        "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-06-recovery-fakesign/tf-eboot.self",
        "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-06-recovery-fakesign/astro-eboot-PRISTINE-orig.self")

    def test_the_elf_loader_refuses_the_self_magics(self):
        for magic in self.MAGICS:
            with self.assertRaises(EncryptedSelfError) as cm:
                ElfImage(magic + bytes(200))
            self.assertIn("encrypted PS4/PS5 SELF", str(cm.exception))
            self.assertIn(" ".join("%02x" % b for b in magic), str(cm.exception))
            self.assertIsInstance(cm.exception, ElfError)

    def test_cli_refuses_with_its_own_exit_code_before_any_analysis(self):
        for k, magic in enumerate(self.MAGICS):
            eboot = self.write("self%d.bin" % k, magic + os.urandom(4096))
            out = self.path("never%d" % k)
            calls = []
            saved = (cli.ElfImage.open, detectors.run_all)
            cli.ElfImage.open = staticmethod(lambda *a, **kw: calls.append("open") or saved[0](*a, **kw))
            detectors.run_all = lambda *a, **kw: calls.append("run_all")
            try:
                rc, text = self.run_cli("--eboot", eboot, "--param", self.path("no-such-param.json"), "--out", out)
            finally:
                cli.ElfImage.open, detectors.run_all = saved
            self.assertEqual(rc, cli.EXIT_ENCRYPTED_SELF)
            self.assertEqual(rc, 5)
            self.assertEqual(calls, [], "the executable was opened for analysis")
            self.assertIn("encrypted PS4/PS5 SELF", text)
            self.assertIn("decrypt it first", text)
            self.assertNotIn("param.json", text)                  # the SELF check comes before the param.json read
            self.assertFalse(os.path.exists(out))

    def test_a_self_with_an_elf_inside_is_still_refused(self):
        eboot = self.write("self2.bin", self.MAGICS[0] + bytes(60) + Img().image())
        rc, _t = self.run_cli("--eboot", eboot, "--out", self.path("o"))
        self.assertEqual(rc, 5)

    def test_real_encrypted_selfs_on_this_machine_are_refused(self):
        found = [p for p in self.REAL_SELFS if os.path.exists(p)]
        if not found:
            self.skipTest("no real encrypted SELF available")
        for p in found:
            out = self.path("o_" + os.path.basename(p))
            rc, text = self.run_cli("--eboot", p, "--out", out)
            self.assertEqual(rc, 5, text)
            self.assertFalse(os.path.exists(out))

    def test_a_decrypted_elf_is_unaffected(self):
        im = Img()
        im.func("f").epilogue()
        eboot = self.write("ok.bin", im.image())
        rc, _t = self.run_cli("--eboot", eboot, "--out", self.path("o"), "--quiet")
        self.assertEqual(rc, 0)


class ParamJsonTests(WorkDir):
    def setUp(self):
        super().setUp()
        im = Img()
        im.func("f").epilogue()
        self.eboot = self.write("eboot.bin", im.image())

    def check_refused(self, param_path, words):
        out = self.path("o%d" % len(os.listdir(self.tmp.name)))
        rc, text = self.run_cli("--eboot", self.eboot, "--param", param_path, "--out", out)
        self.assertEqual(rc, 2, text)
        for w in words:
            self.assertIn(w, text)
        self.assertFalse(os.path.exists(out), "a partial profile was written for a bad param.json")

    def test_missing_param_json_is_a_clear_error_with_no_output(self):
        self.check_refused(self.path("nope.json"), ["param.json not found"])

    def test_missing_param_leaves_an_existing_folder_and_profile_untouched(self):
        out = self.path("existing")
        os.makedirs(out)
        Path(out, "runtime-profile.json").write_text('{"status": "generated", "verification": null, "keep": 1}')
        before = Path(out, "runtime-profile.json").read_bytes()
        rc, text = self.run_cli("--eboot", self.eboot, "--param", self.path("nope.json"), "--out", out)
        self.assertEqual(rc, 2)
        self.assertEqual(Path(out, "runtime-profile.json").read_bytes(), before)
        self.assertEqual(sorted(os.listdir(out)), ["runtime-profile.json"])

    def test_malformed_empty_non_object_and_non_utf8_param_are_refused(self):
        self.check_refused(self.write("bad.json", "{not json"), ["not valid UTF-8 JSON"])
        self.check_refused(self.write("empty.json", b""), ["not valid UTF-8 JSON"])
        self.check_refused(self.write("list.json", "[1, 2]"), ["must be a JSON object"])
        self.check_refused(self.write("str.json", '"PPSA00001"'), ["must be a JSON object"])
        self.check_refused(self.write("latin.json", b'{"titleId": "\xff\xfe"}'), ["not valid UTF-8 JSON"])
        self.check_refused(self.tmp.name, ["not a regular file"])

    def test_leaving_param_out_is_allowed_and_says_what_is_unknown(self):
        out = self.path("noparam")
        rc, _t = self.run_cli("--eboot", self.eboot, "--out", out, "--quiet")
        self.assertEqual(rc, 0)
        doc = json.loads(Path(out, "runtime-profile.json").read_text())
        self.assertIn("binding.param_json", [u["id"] for u in doc["unresolved"]])
        self.assertNotIn("title_id", doc.get("binding", {}))
        self.assertTrue(doc["profile_id"].startswith("unknown/unknown/"))

    @needs_capstone
    def test_param_values_of_the_wrong_type_never_crash_the_binding(self):
        cases = [{"titleId": 5, "contentVersion": None},
                 {"titleId": "PPSA00001", "localizedParameters": {"defaultLanguage": "en-US", "en-US": "oops"}},
                 {"titleId": "PPSA00001", "localizedParameters": []},
                 {"titleId": "PPSA00001", "localizedParameters": {"defaultLanguage": ["x"], "en-US": {"titleName": "x"}}},
                 {"titleId": "PPSA00001", "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": 5}}},
                 {"titleId": ["a"], "contentVersion": {"x": 1}, "sdkVersion": 7}, {}]
        for case in cases:
            res, out = self.generate(Img().image() if False else Pool().image(), case, strict=True)
            doc = res.doc
            self.assertFalse([u for u in doc["unresolved"] if u["id"].startswith("detector.")], case)
            tid = doc["binding"].get("title_id")
            self.assertTrue(tid is None or isinstance(tid["value"], str), case)
            name = doc["binding"].get("title_name")
            self.assertTrue(name is None or isinstance(name["value"], str), case)


class ExecutableInputTests(WorkDir):
    def test_missing_directory_empty_and_garbage_executables(self):
        for name, args in (("missing", [self.path("nope.bin")]), ("dir", [self.tmp.name]),
                           ("empty", [self.write("empty.bin", b"")]), ("short", [self.write("short.bin", b"\x7fELF")]),
                           ("pe", [self.write("mz.bin", b"MZ" + bytes(500))])):
            out = self.path("o_" + name)
            rc, text = self.run_cli("--eboot", args[0], "--out", out)
            self.assertEqual(rc, 2, (name, text))
            self.assertFalse(os.path.exists(out), name)
        rc, text = self.run_cli("--eboot", self.path("nope.bin"), "--out", self.path("x"))
        self.assertIn("executable not found", text)

    def test_output_path_that_is_a_file_is_refused(self):
        im = Img()
        im.func("f").epilogue()
        out = self.write("afile", b"x")
        rc, text = self.run_cli("--eboot", self.write("e.bin", im.image()), "--out", out)
        self.assertEqual(rc, 2)
        self.assertIn("not a directory", text)
        self.assertEqual(Path(out).read_bytes(), b"x")

    def test_bad_load_base_values_are_option_errors_not_tracebacks(self):
        im = Img()
        im.func("f").epilogue()
        eboot = self.write("e.bin", im.image())
        for bad in ("zzz", "-0x1000", "0x400001", "0x" + "f" * 20):
            rc, text = self.run_cli("--eboot", eboot, "--out", self.path("o"), "--load-base=" + bad)
            self.assertEqual(rc, 2, bad)
            self.assertIn("--load-base", text)
            self.assertNotIn("Traceback", text)
        rc, _t = self.run_cli("--eboot", eboot, "--out", self.path("good"), "--load-base", "0x20000000", "--quiet")
        self.assertEqual(rc, 0)


class ElfShapeTests(WorkDir):
    def setUp(self):
        super().setUp()
        self.good = good_image()
        self.phdrs = read_phdrs(self.good)

    def refused(self, image, words=()):
        with self.assertRaises(ElfError) as cm:
            ElfImage(image)
        for w in words:
            self.assertIn(w, str(cm.exception))
        # and through the CLI: exit 2, nothing written
        out = self.path("o%d" % len(os.listdir(self.tmp.name)))
        rc, text = self.run_cli("--eboot", self.write("x%d.bin" % len(os.listdir(self.tmp.name)), image), "--out", out)
        self.assertEqual(rc, 2, text)
        self.assertFalse(os.path.exists(out))
        self.assertNotIn("Traceback", text)

    def test_no_program_headers(self):
        buf = bytearray(self.good)
        struct.pack_into("<H", buf, 56, 0)
        self.refused(bytes(buf), ["no program headers"])
        struct.pack_into("<HH", buf, 54, 0, 0)               # e_phentsize 0, e_phnum 0
        self.refused(bytes(buf), ["no program headers"])

    def test_program_header_table_outside_the_file(self):
        buf = bytearray(self.good)
        struct.pack_into("<Q", buf, 32, len(buf) + 0x1000)
        self.refused(bytes(buf), ["program header table out of range"])

    def test_no_loadable_segment(self):
        only_other = [e for e in self.phdrs if e[0] != 1] or [[4, 4, 0x30000, 0x50000, 0x50000, 8, 8, 4]]
        self.refused(write_phdrs(self.good, only_other), ["no loadable segment"])

    def test_wrong_machine(self):
        for machine in (183, 3, 40, 0):                       # aarch64, i386, arm, none
            buf = bytearray(self.good)
            struct.pack_into("<H", buf, 18, machine)
            self.refused(bytes(buf), ["not x86-64"])

    def test_32_bit_class_and_big_endian(self):
        buf = bytearray(self.good)
        buf[4] = 1
        self.refused(bytes(buf), ["ELF64 little-endian"])
        buf = bytearray(self.good)
        buf[5] = 2
        self.refused(bytes(buf), ["ELF64 little-endian"])

    def test_overlapping_segments_are_refused(self):
        loads = [e for e in self.phdrs if e[0] == 1 and e[6] > 0]
        extra = list(loads[0])
        extra[3] = extra[4] = extra[3] + 0x10                  # same bytes, shifted by 0x10: overlaps in memory
        self.refused(write_phdrs(self.good, self.phdrs + [extra]), ["overlap"])
        nested = list(loads[1])
        nested[3] = nested[4] = nested[3] + 4
        nested[5] = nested[6] = 8
        self.refused(write_phdrs(self.good, self.phdrs + [nested]), ["overlap"])

    def test_touching_segments_are_fine(self):
        a = [1, 5, 0x5000, 0x100000, 0x100000, 0x100, 0x100, 0x1000]
        b = [1, 4, 0x5100, 0x100100, 0x100100, 0x100, 0x100, 0x1000]
        img = write_phdrs(self.good, self.phdrs + [a, b])
        e = ElfImage(img)
        self.assertIsNone(e.segment_of(0x100200))
        self.assertEqual(e.segment_of(0x1000ff).vaddr, 0x100000)
        self.assertEqual(e.segment_of(0x100100).vaddr, 0x100100)

    def test_zero_size_load_segments_are_ignored(self):
        zero = [1, 4, 0x5000, 0x200000, 0x200000, 0, 0, 0x1000]
        e = ElfImage(write_phdrs(self.good, self.phdrs + [zero]))
        base = ElfImage(self.good)
        self.assertEqual([s.index for s in e.loads], [s.index for s in base.loads])
        self.assertIsNone(e.segment_of(0x200000))
        # a PT_LOAD with data but no memory is inconsistent
        bad = [1, 4, 0x5000, 0x200000, 0x200000, 0x40, 0x10, 0x1000]
        self.refused(write_phdrs(self.good, self.phdrs + [bad]), ["p_memsz"])

    def test_absurd_addresses_are_refused(self):
        bad = [1, 4, 0x5000, 1 << 62, 1 << 62, 0x40, 0x40, 0x1000]
        self.refused(write_phdrs(self.good, self.phdrs + [bad]), ["plausible"])
        bad = [1, 4, 0x5000, 0xFFFFFFFFFFFFF000, 0xFFFFFFFFFFFFF000, 0x40, 0x2000, 0x1000]
        self.refused(write_phdrs(self.good, self.phdrs + [bad]), ["plausible"])

    def test_any_segment_running_past_the_end_is_a_truncated_file(self):
        for kind in (2, 4, 0x61000001, 7):                    # dynamic, note, sce procparam, tls
            seg = [kind, 4, len(self.good) - 4, 0x300000, 0x300000, 0x100, 0x100, 8]
            self.refused(write_phdrs(self.good, self.phdrs + [seg]), ["runs past the end"])

    def test_segments_in_any_order_give_the_same_mapping_and_the_same_values(self):
        fx = ObjArray()
        img = fx.image(notes=[gnu_note(bytes(range(16)) + bytes(4))], relas=[(RW_VA + 8 * k, CODE_VA + 16 * k) for k in range(8)])
        base = ElfImage(img)
        rng = random.Random(7)
        entries = read_phdrs(img)
        for _ in range(6):
            shuffled = entries[:]
            rng.shuffle(shuffled)
            e = ElfImage(write_phdrs(img, shuffled))
            for va in (CODE_VA, CODE_VA + 0x30, RO_VA + 5, RW_VA, RW_VA + RW_FILESZ + 4, 0x40000 + 0x108, 0x1234567):
                self.assertEqual(e.va_to_off(va), base.va_to_off(va), hex(va))
                self.assertEqual(e.read(va, 4), base.read(va, 4), hex(va))
                self.assertEqual(e.is_exec(va), base.is_exec(va), hex(va))
            self.assertEqual(sorted(e.relative_relocs()[0]), sorted(base.relative_relocs()[0]))
            if Decoder.available:
                ctx = Context(e, ImageIndex(e))
                platform.detect(ctx)
                uobject_array.detect(ctx)
                self.assertEqual(int(ctx.val("symbols.object_array.label")), fx.G)
                self.assertEqual(ctx.get("symbols.object_array.label").confidence, "high")

    def test_broken_note_and_dynamic_contents_do_not_crash(self):
        img = bytearray(good_image())
        entries = read_phdrs(bytes(img))
        note = next(e for e in entries if e[0] == 4)
        dyn = next(e for e in entries if e[0] == 2)
        struct.pack_into("<III", img, note[2], 0xFFFFFFF0, 0xFFFFFFF0, 3)      # namesz/descsz far beyond the segment
        struct.pack_into("<qQ", img, dyn[2], 7, 0xFFFFFFFFFFF)                 # DT_RELA pointing nowhere
        struct.pack_into("<qQ", img, dyn[2] + 16, 8, 1 << 60)                  # DT_RELASZ huge
        e = ElfImage(bytes(img))
        self.assertEqual(e.notes(), [])
        self.assertEqual(len(e.relative_relocs()[0]), 0)
        ctx = Context(e, ImageIndex(e))
        platform.detect(ctx)
        self.assertIn("build.build_id", unresolved_ids(ctx))


class TruncationTests(WorkDir):
    KEYS = ("symbols.", "structs.", "vtables.", "build.build_id", "build.engine")

    def leaves(self, doc):
        return {p: e["value"] for p, e in profile.iter_envelopes(doc) if p.startswith(self.KEYS)}

    @needs_capstone
    def test_every_cut_is_a_refusal_or_a_subset_of_the_true_values(self):
        img = good_image() + bytes(8192)            # trailing padding after the last segment: cuts inside it are still valid images
        full, _o = self.generate(img, {"titleId": "PPSA00001", "contentVersion": "01.000.000"}, strict=True)
        want = self.leaves(full.doc)
        self.assertGreater(len(want), 30)                 # the fixture resolves enough to make the comparison meaningful
        cuts = sorted(set(list(range(0, 0x140, 13)) + list(range(0x140, len(img), 3137)) +
                          [len(img) - 1, len(img) - 4096, 0x4fff, 0x5000, 0x5001, 0x7fff, 0xe000, 0x10000, 0x20000, 0x20100]))
        refused = kept = 0
        for cut in cuts:
            with self.subTest(cut=cut):
                eboot = self.write("cut.bin", img[:cut])
                try:
                    res = cli.generate(eboot, None, None, utc="2000-01-01T00:00:00Z")
                except (ElfError, cli.InputError):
                    refused += 1
                    continue
                kept += 1
                crashed = [u for u in res.doc["unresolved"] if u["id"].startswith("detector.") and "crashed" in u["why"]]
                self.assertEqual(crashed, [])
                for path, value in self.leaves(res.doc).items():
                    self.assertEqual(value, want.get(path), "%s changed from %r to %r when the file was cut at %#x" % (
                        path, want.get(path), value, cut))
        self.assertGreater(refused, 10)
        self.assertGreater(kept, 3)                       # cuts after the last segment still analyse (and agree)

    def test_a_cut_through_the_first_megabytes_of_a_real_image_is_refused(self):
        if not os.path.exists(TOWNFALL_EBOOT):
            self.skipTest("the retained decrypted executable is not available")
        with open(TOWNFALL_EBOOT, "rb") as fh:
            head = fh.read(8 << 20)
        eboot = self.write("cut.bin", head)
        out = self.path("o")
        rc, text = self.run_cli("--eboot", eboot, "--out", out)
        self.assertEqual(rc, 2, text)
        self.assertIn("runs past the end", text)
        self.assertFalse(os.path.exists(out))

    def test_zero_length_and_header_only_files(self):
        for n in (0, 1, 4, 16, 63, 64, 120):
            with self.assertRaises((ElfError, ValueError)):
                ElfImage(good_image()[:n])


class HeaderFuzzTests(unittest.TestCase):
    """Seeded byte mutations of the ELF header, the program-header table, the dynamic table, the notes and the process
    parameter block: the image is refused cleanly or analysed without a detector crash, and every emitter copes with the result."""

    @needs_capstone
    def test_mutations_never_crash_a_detector_or_an_emitter(self):
        good = bytearray(good_image())
        phoff, entsize, num = phdr_table(bytes(good))
        entries = read_phdrs(bytes(good))
        spans = [(0, 64), (phoff, phoff + num * entsize)]
        for e in entries:
            if e[0] in (2, 4, 0x61000001):
                spans.append((e[2], e[2] + min(e[5], 0x120)))
        rng = random.Random(20261007)
        refused = analysed = 0
        crashes = []
        for n in range(350):
            img = bytearray(good)
            for _ in range(rng.choice([1, 1, 2, 4])):
                lo, hi = rng.choice(spans)
                img[rng.randrange(lo, hi)] = rng.randrange(256)
            try:
                elf = ElfImage(bytes(img))
            except ElfError:
                refused += 1
                continue
            analysed += 1
            ctx = Context(elf, ImageIndex(elf))
            detectors.run_all(ctx, strict=False)
            crashes += [(n, u.id, u.why) for u in ctx.unresolved if u.id.startswith("detector.") and "crashed" in u.why]
            doc = profile.assemble(ctx)
            emit_ue4ss.render_vtable_layout(doc)
            emit_ue4ss.render_member_layout(doc)
            emit_ue4ss.render_settings_fragment(doc)
            try:
                emit_seedtable.render_profile(doc)
            except emit_seedtable.SeedTableRefused:
                pass
        self.assertEqual(crashes, [], "detector crashes on mutated headers: %s" % crashes[:3])
        self.assertGreater(refused, 20)
        self.assertGreater(analysed, 50)


    @needs_capstone
    def test_mutated_code_strings_and_relocations_never_crash_a_detector(self):
        relas = [(RW_VA + 8 * k, CODE_VA + 16 * k) for k in range(40)]
        images = [Pool().image(relas=relas), ObjArray().image(relas=relas), Vtables("pair").image()]
        rng = random.Random(4242)
        crashes = []
        for n, good in enumerate(images):
            elf = ElfImage(good)
            lo = min(i for i, b in enumerate(good) if b)
            spans = [(s.offset, s.offset + min(s.filesz, 0x4000)) for s in elf.loads if s.filesz]
            for it in range(60):
                img = bytearray(good)
                for _ in range(rng.choice([1, 4, 16])):
                    a, b = rng.choice(spans)
                    img[rng.randrange(a, b)] = rng.randrange(256)
                ctx = make_ctx(bytes(img))
                detectors.run_all(ctx, strict=False)
                crashes += [(n, it, u.id, u.why) for u in ctx.unresolved if u.id.startswith("detector.") and "crashed" in u.why]
                profile.assemble(ctx)
        self.assertEqual(crashes, [], "detector crashes on mutated code/data: %s" % crashes[:3])


# ---------------------------------------------------------------------------------------------
class OtherEngineVersionTests(WorkDir):
    """A title on another engine version reports that version or nothing, never the validated one."""

    @staticmethod
    def markers_for(version, config=2, stats=False):
        table = build_axes.load_table()
        vi = [tuple(v) for v in table["versions"]].index(version)
        return [m["s"] for m in table["markers"] if build_axes.covers(m, vi, config, stats)]

    @needs_capstone
    def test_full_pipeline_reports_the_images_own_version(self):
        for version in ((5, 3), (5, 4), (5, 1), (4, 27)):
            with self.subTest(version=version):
                img = version_image(version[0], version[1], 2, 777, "UE%d" % version[0], extra_markers=self.markers_for(version))
                res, out = self.generate(img, {"titleId": "PPSA00001", "contentVersion": "01.000.000"}, strict=True)
                b = res.doc["build"]
                self.assertEqual((int(b["engine"]["major"]["value"], 16), int(b["engine"]["minor"]["value"], 16)), version)
                self.assertEqual(b["engine"]["revision_string"]["value"], "%d.%d.2-777+++UE%d" % (version + (version[0],)))
                self.assertEqual(b["engine"]["major"]["confidence"], "high")      # store sequence + marker fingerprint agree
                ini = Path(out, "ue4ss", "VTableLayout.ini").read_text()
                self.assertIn("EngineMajor=%d\nEngineMinor=%d\n" % version, ini)
                self.assertIn("EngineRevision=%d.%d.2-777+++UE%d" % (version + (version[0],)), ini)
                settings = Path(out, "ue4ss", "UE4SS-settings.ini").read_text()
                self.assertIn("MajorVersion=%d\nMinorVersion=%d\n" % version, settings)
                if version != (5, 6):
                    for name in ("VTableLayout.ini", "MemberVariableLayout.ini", "UE4SS-settings.ini"):
                        p = Path(out, "ue4ss", name)
                        if p.exists():
                            self.assertNotIn("5.6", p.read_text(), name)
                            self.assertNotIn("EngineMinor=6", p.read_text(), name)

    @needs_capstone
    def test_minor_zero_has_no_store_scan_and_falls_back_to_the_fingerprint(self):
        # the store pattern needs a non-zero minor: a x.0 build gets its version from the markers (medium) or not at all
        img = version_image(5, 0, 1, 5, "UE5", extra_markers=self.markers_for((5, 0)))
        res, _out = self.generate(img, {"titleId": "PPSA00001", "contentVersion": "01.000.000"}, strict=True)
        major = res.doc["build"]["engine"].get("major")
        self.assertTrue(major is None or (int(major["value"], 16), int(res.doc["build"]["engine"]["minor"]["value"], 16)) == (5, 0))
        if major is not None:
            self.assertEqual(major["confidence"], "medium")
            self.assertNotIn("revision_string", res.doc["build"]["engine"])

    def test_a_version_string_alone_names_its_own_version(self):
        for text, version in (("5.4.2-33043543+++UE5+Release-5.4", (5, 4)), ("5.3.0-1+++UE5+Release-5.3", (5, 3)),
                              ("4.27.2-1+++UE4+Release-4.27", (4, 27))):
            im = Img()
            im.ro_str(text, wide=True)
            im.func("f").epilogue()
            ctx = make_ctx(im.image())
            engine_version.detect(ctx)
            self.assertEqual((int(ctx.val("build.engine.major")), int(ctx.val("build.engine.minor"))), version)
            self.assertNotEqual(ctx.get("build.engine.major").confidence, "high")        # one channel

    @needs_capstone
    def test_fingerprint_versus_store_disagreement_is_not_called_high(self):
        # the markers describe 5.4, the store sequence says 5.3: the store sequence is reported, at medium, with the conflict
        img = version_image(5, 3, 1, 5, "UE5", extra_markers=self.markers_for((5, 4)))
        ctx = make_ctx(img)
        build_axes.detect(ctx)
        engine_version.detect(ctx)
        self.assertEqual((int(ctx.val("build.engine.major")), int(ctx.val("build.engine.minor"))), (5, 3))
        self.assertEqual(ctx.get("build.engine.minor").confidence, "medium")
        self.assertTrue(any("disagrees" in e for e in ctx.get("build.engine.minor").evidence))

    def test_marker_fingerprint_alone_never_names_another_version(self):
        table = build_axes.load_table()
        versions = [tuple(v) for v in table["versions"]]
        for version in versions[::2] + [(5, 3), (5, 4), (5, 6)]:
            for config, stats in ((2, False), (0, True)):
                im = Img()
                for m in self.markers_for(version, config, stats):
                    im.ro_str(m, wide=True)
                im.func("f").epilogue()
                ctx = make_ctx(im.image())
                build_axes.detect(ctx)
                got = ctx.facts.get("axes.version")
                self.assertTrue(got is None or got == "%d.%d" % version, "markers of %s %s read as %s" % (version, (config, stats), got))

    @needs_capstone
    def test_no_version_evidence_at_all_leaves_the_version_unresolved(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        platform.detect(ctx)
        build_axes.detect(ctx)
        engine_version.detect(ctx)
        self.assertIsNone(ctx.get("build.engine.major"))
        self.assertIn("build.engine.major", unresolved_ids(ctx))


# ---------------------------------------------------------------------------------------------
@needs_capstone
class FunctionMapAssumptionTests(WorkDir):
    """README: "Frame-pointer clang code with 55 48 89 e5 prologues or int3 padding between functions (function map)".
    Padding alone or prologues alone are enough; neither leaves function boundaries unknown."""

    VARIANTS = {"padding only": (True, False), "prologue only": (False, True)}
    NEITHER = (False, False)

    def test_either_half_of_the_assumption_is_enough(self):
        for name, (pad, frame) in self.VARIANTS.items():
            with self.subTest(variant=name):
                fx = Pool(pad, frame)
                ctx = run_pool(fx)
                only_the_truth(self, ctx, fx.truth, name + ": ")
                self.assertEqual(int(ctx.val("symbols.name_pool.label")), fx.pool)
                self.assertEqual(int(ctx.val("symbols.gmalloc.label")), fx.G)
                fx = ObjArray(pad=pad, frame=frame)
                self.assertEqual(int(run_uoa(fx).val("symbols.object_array.label")), fx.G)
                img, L = sco_image(pad, frame)
                ctx = make_ctx(img)
                static_construct.detect(ctx)
                self.assertEqual(int(ctx.val("symbols.static_construct_object_internal.label")), L["sco"])

    def test_neither_leaves_boundary_dependent_values_unresolved_or_capped(self):
        pad, frame = self.NEITHER
        # FNamePool / GMalloc: Init is "the function with the most default names": no functions, no Init
        fx = Pool(pad, frame)
        ctx = run_pool(fx)
        only_the_truth(self, ctx, fx.truth)
        for path in ("symbols.name_pool.label", "symbols.gmalloc.label", "symbols.fmemory_malloc.label", "structs.FNamePool.blocks"):
            self.assertIsNone(ctx.get(path), path)
        self.assertIn("symbols.name_pool", unresolved_ids(ctx))
        # GUObjectArray: the anchor functions cannot be told apart
        fx = ObjArray(pad=pad, frame=frame)
        ctx = run_uoa(fx)
        self.assertIsNone(ctx.get("symbols.object_array.label"))
        self.assertIsNone(ctx.get("structs.TUObjectArray.Objects"))
        # StaticConstructObject: the stack-struct call pair is still seen, but the "is a function start" check fails
        img, L = sco_image(pad, frame)
        ctx = make_ctx(img)
        static_construct.detect(ctx)
        only_the_truth(self, ctx, {"symbols.static_construct_object_internal.label": L["sco"],
                                   "symbols.sco_params_ctor.label": L["params_ctor"],
                                   "symbols.assert_if_in_constructor.label": L["assert_fn"]})
        self.assertEqual(ctx.get("symbols.static_construct_object_internal.label").confidence, "low")
        # StaticFindObject carries no boundary-free channel
        img, L = find_image(pad, frame)
        ctx = make_ctx(img)
        static_find_object.detect(ctx)
        self.assertIsNone(ctx.get("symbols.static_find_object.label"))
        # FName constructor: the call target is still the consensus, but is not a verified function start
        img, L = ctor_image(pad, frame)
        ctx = make_ctx(img)
        fname_ctor.detect(ctx)
        only_the_truth(self, ctx, {"symbols.fname_ctor_wchar.label": L["ctor"]})
        self.assertEqual(ctx.get("symbols.fname_ctor_wchar.label").confidence, "low")
        # engine version: major.minor.patch come from the store, the changelist replay needs the function start
        ctx = make_ctx(version_image(5, 3, 2, 99, "UE5", pad=pad, frame=frame))
        engine_version.detect(ctx)
        self.assertEqual((int(ctx.val("build.engine.major")), int(ctx.val("build.engine.minor"))), (5, 3))
        self.assertIsNone(ctx.get("build.engine.revision_string"))

    def test_the_profile_says_the_function_map_was_unusable(self):
        res, _out = self.generate(Pool(*self.NEITHER).image(), {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        row = [u for u in res.doc["unresolved"] if u["id"] == "analysis.function_map"]
        self.assertEqual(len(row), 1)
        self.assertIn("function starts", row[0]["why"])
        res, _out = self.generate(Pool().image(), {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        self.assertNotIn("analysis.function_map", [u["id"] for u in res.doc["unresolved"]])


@needs_capstone
class VtableAbiAssumptionTests(unittest.TestCase):
    """README: "Itanium vtables (two destructor slots)"."""

    def test_itanium_pair_prefix_is_recognised(self):
        ctx = run_pe(Vtables("pair"))
        self.assertEqual(int(ctx.val("vtables.UObject.dtor_prefix_slots")), 2)
        self.assertEqual(ctx.get("vtables.UObject.dtor_prefix_slots").confidence, "medium")

    def test_other_prefixes_do_not_claim_two_destructor_slots(self):
        for prefix in ("far", "same"):
            with self.subTest(prefix=prefix):
                fx = Vtables(prefix)
                ctx = run_pe(fx)
                self.assertIsNone(ctx.get("vtables.UObject.dtor_prefix_slots"))
                self.assertIn("vtables.UObject.dtor_prefix_slots", unresolved_ids(ctx))
                # the slot is measured from the start of the run, so it is still right for any destructor layout
                self.assertEqual(int(ctx.val("vtables.UObject.ProcessEvent")), fx.SLOT * 8)
                self.assertEqual(int(ctx.val("symbols.process_event.label")), fx.labels["pe"])

    def test_a_minority_of_pair_shaped_runs_is_not_enough(self):
        fx = Vtables("far")
        img = bytearray(fx.image())
        # turn 9 of the 30 runs into pairs by pointing slot 1 at the function right after slot 0's padding
        elf = ElfImage(bytes(img))
        offs, adds = elf.relative_relocs()
        fn0, fn1 = fx.labels["stub0"], fx.labels["stub1"]
        self.assertGreater(fn1 - fn0, 0x100)
        near = fx.labels["stub0"] + 1
        # rewrite RELA addends in the file: slot-1 relocations of the first 9 vtables now point 1 byte after slot 0's function
        phdrs = read_phdrs(bytes(img))
        rela_seg = next(e for e in phdrs if e[0] == 1 and e[1] == 0)
        count = 0
        for i in range(len(offs)):
            if adds[i] == fn1 and count < 9:
                pos = rela_seg[2] + 0x100 + i * 24 + 16
                struct.pack_into("<q", img, pos, near)
                count += 1
        ctx = make_ctx(bytes(img))
        platform.detect(ctx)
        process_event.detect(ctx)
        self.assertEqual(count, 9)
        self.assertIsNone(ctx.get("vtables.UObject.dtor_prefix_slots"))


@needs_capstone
class AnchorStringAssumptionTests(WorkDir):
    """README: "fatal log messages and __FILE__ strings kept in shipping builds (UObjectArray and StaticFindObject anchors)"."""

    def test_stripped_strings_leave_the_anchored_symbols_unresolved(self):
        fx = ObjArray(strings=False)
        ctx = run_uoa(fx)
        self.assertIsNone(ctx.get("symbols.object_array.label"))
        self.assertIn("symbols.object_array", unresolved_ids(ctx))
        self.assertFalse([p for p in ctx.values if p.startswith("structs.")])
        img, _L = find_image(messages=False)
        ctx = make_ctx(img)
        static_find_object.detect(ctx)
        self.assertIsNone(ctx.get("symbols.static_find_object.label"))
        self.assertIn("symbols.static_find_object", unresolved_ids(ctx))
        img, _L = sco_image(string=False)
        ctx = make_ctx(img)
        static_construct.detect(ctx)
        self.assertIsNone(ctx.get("symbols.static_construct_object_internal.label"))
        self.assertIn("symbols.static_construct_object_internal", unresolved_ids(ctx))

    def test_one_surviving_anchor_function_caps_the_confidence(self):
        fx = ObjArray(anchor_functions=1)
        ctx = run_uoa(fx)
        env = ctx.get("symbols.object_array.label")
        self.assertEqual(int(env.value), fx.G)
        self.assertNotEqual(env.confidence, "high")
        # the layout that rests on member functions that were not found is not called measured
        for path in ("structs.FUObjectArray.UObjectCreateListeners", "structs.FUObjectArray.UObjectDeleteListeners",
                     "structs.FUObjectArray.bShouldRecycleObjectIndices"):
            env = ctx.get(path)
            self.assertTrue(env is None or env.confidence != "high", path)

    def test_a_few_remaining_new_object_sites_are_not_high(self):
        img, L = sco_image(sites=3)
        ctx = make_ctx(img)
        static_construct.detect(ctx)
        only_the_truth(self, ctx, {"symbols.static_construct_object_internal.label": L["sco"]})
        not_high(self, ctx, "symbols.static_construct_object_internal.label")

    def test_without_the_module_name_anchors_the_name_constructor_is_unresolved(self):
        img, _L = ctor_image(anchors=False)
        ctx = make_ctx(img)
        fname_ctor.detect(ctx)
        self.assertIsNone(ctx.get("symbols.fname_ctor_wchar.label"))
        self.assertIn("symbols.fname_ctor_wchar", unresolved_ids(ctx))

    def test_through_the_cli_nothing_anchored_is_invented(self):
        res, out = self.generate(ObjArray(strings=False).image(), {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        self.assertNotIn("object_array", res.doc.get("symbols", {}))
        self.assertIn("symbols.object_array", [u["id"] for u in res.doc["unresolved"]])
        # and SeedTable.h cannot be made from it (no FNamePool label): a refusal, not a header with invented labels
        with self.assertRaises(emit_seedtable.SeedTableRefused):
            emit_seedtable.render_profile(res.doc)


@needs_capstone
class DefaultNameAssumptionTests(unittest.TestCase):
    """README: "default-name literals registered in one function (FNamePool)"."""

    def test_baseline_single_function(self):
        fx = Pool()
        ctx = run_pool(fx)
        only_the_truth(self, ctx, fx.truth)
        self.assertEqual(ctx.get("symbols.name_pool.label").confidence, "medium")
        self.assertEqual(ctx.get("symbols.fnamepool_init.label").confidence, "high")
        self.assertEqual(int(ctx.val("symbols.gmalloc.label")), fx.G)

    def test_names_registered_by_a_bigger_function_than_the_one_with_the_layout(self):
        fx = Pool(layout_names=8)                       # Init holds 8 names (and the layout); a registrar holds the other 12
        ctx = run_pool(fx)
        only_the_truth(self, ctx, {k: v for k, v in fx.truth.items() if k != "symbols.fnamepool_init.label"})
        init = ctx.get("symbols.fnamepool_init.label")
        if init is not None:
            self.assertNotEqual(int(init.value), fx.labels["init"])              # the registrar was picked...
            self.assertEqual(init.confidence, "low")                            # ...and says it cannot be Init
        self.assertEqual(ctx.get("symbols.name_pool.label").confidence, "low")
        self.assertIsNone(ctx.get("structs.FNamePool.blocks"))
        self.assertIsNone(ctx.get("structs.FNamePool.none_id"))
        self.assertIsNone(ctx.get("symbols.fmemory_malloc.label"))
        self.assertIsNone(ctx.get("symbols.gmalloc.label"))                     # no FMemory::Malloc, no GMalloc
        self.assertIn("structs.FNamePool.blocks", unresolved_ids(ctx))
        self.assertIn("symbols.gmalloc", unresolved_ids(ctx))
        for path in ("symbols.fname_default_name_store.label", "structs.FNamePool.default_name_count"):
            env = ctx.get(path)
            self.assertTrue(env is None or env.confidence == "low", path)

    def test_names_spread_over_many_small_functions_are_unresolved(self):
        fx = Pool(layout_names=5, registrars=3)         # 5 + 5 + 5 + 5 names: nobody reaches the 8-name floor
        ctx = run_pool(fx)
        self.assertIsNone(ctx.get("symbols.name_pool.label"))
        self.assertIsNone(ctx.get("symbols.fnamepool_init.label"))
        self.assertIn("symbols.name_pool", unresolved_ids(ctx))
        self.assertIsNone(ctx.get("symbols.gmalloc.label"))
        only_the_truth(self, ctx, fx.truth)


@needs_capstone
class ChunkedArrayAssumptionTests(unittest.TestCase):
    """README: "UE 5.x FChunkedFixedUObjectArray (2 pointers + 4 int32)"."""

    SCHEMA_DERIVED = ("PreAllocatedObjects", "MaxElements", "MaxChunks", "NumChunks", "UEP_TotalSize")

    def test_declared_shape_is_measured_and_trusted(self):
        fx = ObjArray()
        ctx = run_uoa(fx)
        self.assertEqual(int(ctx.val("structs.TUObjectArray.NumElements")), 0x14)
        self.assertEqual(ctx.get("structs.TUObjectArray.NumElements").confidence, "high")
        self.assertEqual(int(ctx.val("structs.TUObjectArray.MaxElements")), 0x10)

    def test_another_member_order_reports_what_was_measured_and_nothing_else(self):
        fx = ObjArray(num_delta=0x10)                   # the bounds-checked u32 sits where the declaration puts MaxElements
        ctx = run_uoa(fx)
        self.assertEqual(int(ctx.val("symbols.object_array.label")), fx.G)
        self.assertEqual(int(ctx.val("structs.TUObjectArray.Objects")), 0)
        env = ctx.get("structs.TUObjectArray.NumElements")
        self.assertEqual(int(env.value), 0x10)                                  # the measured distance, not the declared 0x14
        self.assertEqual(env.confidence, "medium")
        for name in self.SCHEMA_DERIVED:
            self.assertIsNone(ctx.get("structs.TUObjectArray." + name), name)
            self.assertIn("structs.TUObjectArray." + name, unresolved_ids(ctx))
        # the structure after the array starts at an unknown place
        for name in ("UObjectCreateListeners", "UObjectDeleteListeners", "PrimarySerialNumber", "bShouldRecycleObjectIndices", "UEP_TotalSize"):
            self.assertIsNone(ctx.get("structs.FUObjectArray." + name), name)
        # what precedes the array does not depend on it
        self.assertEqual(int(ctx.val("structs.FUObjectArray.ObjObjects")), fx.OBJ)

    def test_no_bounds_check_means_the_declared_order_is_only_an_assumption(self):
        fx = ObjArray(bounds_check=False)
        ctx = run_uoa(fx)
        self.assertEqual(int(ctx.val("structs.TUObjectArray.Objects")), 0)
        for name in ("NumElements",) + self.SCHEMA_DERIVED:
            env = ctx.get("structs.TUObjectArray." + name)
            self.assertTrue(env is None or env.confidence == "low", "%s %s" % (name, env and env.confidence))
        for name in ("UObjectCreateListeners", "UObjectDeleteListeners", "PrimarySerialNumber", "bShouldRecycleObjectIndices"):
            env = ctx.get("structs.FUObjectArray." + name)
            self.assertTrue(env is None or env.confidence == "low", name)

    def test_a_flat_array_without_the_chunk_step_has_no_layout(self):
        fx = ObjArray(chunked=False)
        ctx = run_uoa(fx)
        self.assertEqual(int(ctx.val("symbols.object_array.label")), fx.G)
        self.assertNotEqual(ctx.get("symbols.object_array.label").confidence, "high")     # no IndexToObject idiom to corroborate it
        self.assertIsNone(ctx.get("structs.TUObjectArray.Objects"))
        self.assertIsNone(ctx.get("structs.TUObjectArray.NumElements"))
        self.assertIn("structs.TUObjectArray.Objects", unresolved_ids(ctx))

    def test_a_different_chunk_size_is_measured(self):
        fx = ObjArray(shift=14)
        ctx = run_uoa(fx)
        self.assertEqual(int(ctx.val("structs.TUObjectArray.elements_per_chunk")), 1 << 14)


@needs_capstone
class OtherAssumptionTests(WorkDir):
    """Assumptions the detectors state in their own documentation (README lists the five above)."""

    def test_malloc_that_is_not_a_global_vtable_dispatcher_leaves_gmalloc_unresolved(self):
        fx = Pool(malloc_is_dispatcher=False)
        ctx = run_pool(fx)
        self.assertIsNone(ctx.get("symbols.gmalloc.label"))
        self.assertIsNone(ctx.get("vtables.FMalloc.Malloc"))
        self.assertIn("symbols.gmalloc", unresolved_ids(ctx))
        self.assertEqual(int(ctx.val("symbols.name_pool.label")), fx.pool)       # the pool itself does not depend on it

    def test_processevent_needs_the_native_flag_test(self):
        ctx = run_pe(Vtables("pair", native_test=False))
        self.assertIsNone(ctx.get("symbols.process_event.label"))
        self.assertIn("vtables.UObject.ProcessEvent", unresolved_ids(ctx))

    def test_no_relocation_table_means_no_vtables(self):
        im = Img()
        im.func("f").epilogue()
        ctx = make_ctx(im.image())
        platform.detect(ctx)
        process_event.detect(ctx)
        self.assertIn("vtables.UObject.ProcessEvent", unresolved_ids(ctx))
        self.assertIsNone(ctx.get("vtables.UObject.ProcessEvent"))

    def test_a_non_ps5_image_gets_no_load_base_and_no_seed_table(self):
        res, _out = self.generate(Pool().image(e_type=2), {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        self.assertNotIn("base", res.doc.get("load", {}))
        self.assertIn("load.base", [u["id"] for u in res.doc["unresolved"]])
        with self.assertRaises(emit_seedtable.SeedTableRefused):
            emit_seedtable.render_profile(res.doc)

    def test_a_ps5_library_is_not_the_main_executable(self):
        # e_type 0xFE18 (SCE_DYNLIB): the "main module at the fixed base" convention does not apply to it
        res, _out = self.generate(Pool().image(e_type=0xFE18), {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        self.assertNotIn("base", res.doc.get("load", {}))
        self.assertIn("load.base", [u["id"] for u in res.doc["unresolved"]])
        for sym_ in res.doc.get("symbols", {}).values():
            self.assertNotIn("runtime", sym_)

    def test_images_without_code_or_without_data_do_not_crash(self):
        param = {"titleId": "PPSA00001", "contentVersion": "01.000.000"}
        only_data = build_elf([Seg(RO_VA, ascii0("None") + bytes(64), 4), Seg(RW_VA, bytes(64), 6, memsz=0x1000)])
        only_code = build_elf([Seg(CODE_VA, b"\xcc" * 0x200, 5)])
        empty_code = build_elf([Seg(CODE_VA, b"", 5, memsz=0x100), Seg(RO_VA, bytes(16), 4)])
        for name, img in (("data only", only_data), ("code only", only_code), ("bss-only code", empty_code)):
            res, out = self.generate(img, param, strict=True)
            self.assertEqual(res.doc["verification"], None, name)
            self.assertFalse([u for u in res.doc["unresolved"] if u["id"].startswith("detector.")], name)
            self.assertNotIn("object_array", res.doc.get("symbols", {}), name)

    def test_a_detector_that_raises_is_recorded_not_fatal(self):
        saved = detectors.ORDER

        def boom(ctx):
            raise RuntimeError("synthetic failure")
        detectors.ORDER = [("platform", platform.detect), ("boom", boom)]
        try:
            res, _out = self.generate(Pool().image(), {"titleId": "PPSA00001", "contentVersion": "01.000.000"})
        finally:
            detectors.ORDER = saved
        self.assertIn("detector.boom", [u["id"] for u in res.doc["unresolved"]])
        self.assertIsNone(res.doc["verification"])
        self.assertEqual(res.doc["status"], "generated")


class MutationSafetyTests(WorkDir):
    """A pipeline run never edits its inputs and never writes a verification block."""

    @needs_capstone
    def test_inputs_are_left_byte_identical_and_the_profile_is_never_verified(self):
        img = good_image()
        eboot = self.write("e.bin", img)
        param = self.write("p.json", json.dumps({"titleId": "PPSA00001", "contentVersion": "01.000.000"}))
        before = (hashlib.sha256(Path(eboot).read_bytes()).hexdigest(), hashlib.sha256(Path(param).read_bytes()).hexdigest(),
                  os.stat(eboot).st_mtime_ns, os.stat(param).st_mtime_ns)
        res = cli.generate(eboot, param, self.path("o"), utc="2000-01-01T00:00:00Z", seedtable=self.path("o/runtime/SeedTable.h"))
        after = (hashlib.sha256(Path(eboot).read_bytes()).hexdigest(), hashlib.sha256(Path(param).read_bytes()).hexdigest(),
                 os.stat(eboot).st_mtime_ns, os.stat(param).st_mtime_ns)
        self.assertEqual(before, after)
        doc = json.loads(Path(self.path("o"), "runtime-profile.json").read_text())
        self.assertEqual((doc["status"], doc["verification"]), ("generated", None))
        for f in Path(self.path("o")).rglob("*"):
            if f.is_file():
                self.assertNotRegex(f.read_text(encoding="utf-8", errors="replace").lower().replace("re-verified", ""),
                                    r"console[-_ ]?verified", str(f))


if __name__ == "__main__":
    unittest.main()
