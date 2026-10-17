"""cap05 host tests (Mac-only). Real inputs are read-only; every write goes to
cap05/tests/tmp (deleted per test) or cap05/tests/out."""
import contextlib
import glob
import hashlib
import io
import json
import os
import shutil
import struct
import sys
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
CAP = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(CAP, "tools"))
import crashcap  # noqa: E402
import elf64  # noqa: E402
import final_symbols  # noqa: E402

O = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs"
L25 = O + "/2026-10-08-l25-prep"
PRX = "modbridge_supplier_composition.prx"
R11_FROZEN = O + "/2026-10-08-l26-frozen/R11-watch-world-uenum-bfd95db6"
L3 = L25 + "/evidence/launch-l26-B6b-launch3-20261012"
L14 = L25 + "/evidence/launch-l26-R11-launch14-20261013"
EBOOT = O + "/2026-10-04-zcode-controller/eboot-backup/eboot-original.bin"
FS_R11 = CAP + "/evidence/final-symbols-R11.json"
FS_B6B = CAP + "/evidence/final-symbols-B6b.json"
TMP = os.path.join(HERE, "tmp")
R11_SHA = "bfd95db6c44a91f4fdc36592dd884a075f1e63d66c98cd50530910ab83b30c3b"
B6B_SHA = "41728f484b4f05451c4591b2609b7abe9c52fd5308e94568350d6843b5c05ffb"
ARCHIVE = O + "/2026-10-08-l26-build-variants/archive-r6"


class BuildNotFound(RuntimeError):
    pass


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def locate_build(sha, frozen_prx=None, hint=None):
    """Find a build BY ITS PRX SHA, never by folder name. The package folder that
    holds the object + bare PRX is package-A while that build is current and
    moves to archive-r6/<label>/ when the next build starts, so every package-A
    and archive pass-1 folder is searched; only the one whose PRX hashes to `sha`
    is accepted. `frozen_prx` (if given) is the frozen, sha-checked copy used as
    the packaged PRX. Raises BuildNotFound (loud) if nothing matches."""
    if frozen_prx is not None:
        if not os.path.isfile(frozen_prx) or _sha256(frozen_prx) != sha:
            raise BuildNotFound("frozen PRX missing or sha != %s: %s" % (sha[:8], frozen_prx))
    want_size = os.path.getsize(frozen_prx) if frozen_prx else None
    cands = []
    if hint:
        cands.append(hint)
    cands.append(O + "/2026-10-08-l26-package-A/pass-1")
    cands += sorted(glob.glob(ARCHIVE + "/*/2026-10-08-l26-package-A/pass-1"))
    seen = set()
    for c in cands:
        if c in seen:
            continue
        seen.add(c)
        prx = os.path.join(c, PRX)
        if not os.path.isfile(prx) or (want_size is not None and os.path.getsize(prx) != want_size):
            continue
        if _sha256(prx) != sha:
            continue
        need = [c + "/build/combined-supplier.o", c + "/bare-original/" + PRX,
                os.path.dirname(c) + "/" + PRX + ".symbols.json"]
        missing = [n for n in need if not os.path.isfile(n)]
        if missing:
            raise BuildNotFound("build %s found at %s but files are missing: %s" % (sha[:8], c, missing))
        return {"pass1": c, "pkg": os.path.dirname(c), "packaged": frozen_prx or prx}
    raise BuildNotFound("no package folder holds a PRX with sha %s (searched %d folders: package-A + archive-r6)"
                        % (sha, len(seen)))


_R11 = locate_build(R11_SHA, frozen_prx=R11_FROZEN + "/" + PRX)
R11 = _R11["pass1"]                 # object + bare-original of the R11 build
R11_PKG = _R11["pkg"]               # holds the packaging sidecar
R11_PACKAGED = _R11["packaged"]     # frozen packaged PRX (sha-checked)
B6B = locate_build(B6B_SHA, hint=ARCHIVE + "/B6b-mem-diag/2026-10-08-l26-package-A/pass-1")["pass1"]
sys.stderr.write("located R11 %s: packaged=%s object/bare=%s\nlocated B6b %s: %s\n"
                 % (R11_SHA[:8], R11_PACKAGED, R11, B6B_SHA[:8], B6B))


def run_cc(args):
    err = io.StringIO()
    out = io.StringIO()
    with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
        rc = crashcap.main(args)
    return rc, out.getvalue(), err.getvalue()


def run_fs(args):
    err = io.StringIO()
    out = io.StringIO()
    with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
        rc = final_symbols.main(args)
    return rc, out.getvalue(), err.getvalue()


class Tmp(unittest.TestCase):
    def setUp(self):
        shutil.rmtree(TMP, ignore_errors=True)
        os.makedirs(TMP)

    def tearDown(self):
        shutil.rmtree(TMP, ignore_errors=True)

    def p(self, name):
        return os.path.join(TMP, name)


# ------------------------------------------------------------ final symbols --
class FinalSymbols(Tmp):
    def test_r11_exact_layout_and_binding(self):
        rc, out, err = run_fs(["--object", R11 + "/build/combined-supplier.o", "--bare", R11 + "/bare-original/" + PRX,
                               "--packaged", R11_PACKAGED, "--out", self.p("r11.json")])
        self.assertEqual(rc, 0, err)
        d = json.load(open(self.p("r11.json")))
        self.assertEqual(d["packaged_prx"]["sha256"], R11_SHA)          # = build ledger R11 row
        self.assertEqual(d["bare_prx"]["build_id"], "7423ffe671bfefbc444e887935731e9700000000")
        self.assertGreater(d["layout"]["sections_verified"], 70000)
        self.assertGreater(d["symbol_count"], 70000)
        # second source: the packaging sidecar's own build_id (emitted by another tool)
        with open(R11_PKG + "/" + PRX + ".symbols.json") as f:
            head = f.read(4096)
        self.assertIn('"build_id":"%s"' % d["bare_prx"]["build_id"], head.replace(" ", ""))
        self.assertIn(R11_SHA, head)

    def test_wrong_object_for_bare_fails_closed(self):
        rc, out, err = run_fs(["--object", B6B + "/build/combined-supplier.o", "--bare", R11 + "/bare-original/" + PRX,
                               "--out", self.p("x.json")])
        self.assertEqual(rc, 2)
        self.assertIn("MISMATCH", err)
        self.assertFalse(os.path.exists(self.p("x.json")))

    def test_wrong_packaged_for_bare_fails_closed(self):
        rc, out, err = run_fs(["--object", R11 + "/build/combined-supplier.o", "--bare", R11 + "/bare-original/" + PRX,
                               "--packaged", B6B + "/" + PRX, "--out", self.p("x.json")])
        self.assertEqual(rc, 2)
        self.assertIn("different build", err)

    def test_one_flipped_code_byte_fails_closed(self):
        bare = self.p("bare-tampered.prx")
        shutil.copyfile(R11 + "/bare-original/" + PRX, bare)
        d = json.load(open(FS_R11))
        name, addr, size = next(s for s in d["symbols"] if s[0] == "__cxa_throw")
        e = elf64.Elf(bare)
        off = e.vaddr_to_offset(addr)        # first byte of __cxa_throw (push rbp; never a reloc)
        e.close()
        with open(bare, "r+b") as f:
            f.seek(off)
            b = f.read(1)
            f.seek(off)
            f.write(bytes([b[0] ^ 0xFF]))
        rc, out, err = run_fs(["--object", R11 + "/build/combined-supplier.o", "--bare", bare,
                               "--out", self.p("x.json")])
        self.assertEqual(rc, 2)
        self.assertIn("BYTE MISMATCH", err)

    def test_refuses_overwrite(self):
        open(self.p("exists.json"), "w").close()
        rc, out, err = run_fs(["--object", R11 + "/build/combined-supplier.o", "--bare", R11 + "/bare-original/" + PRX,
                               "--out", self.p("exists.json")])
        self.assertEqual(rc, 2)

    def test_shifted_layout_is_detected(self):
        # negative control of the verification itself: the same rule 0x10 off must fail
        obj = elf64.Elf(R11 + "/build/combined-supplier.o")
        bare = elf64.Elf(R11 + "/bare-original/" + PRX)
        with self.assertRaises(elf64.ElfError):
            final_symbols.verify_and_place(obj, bare, 0x20)


class LocateBuild(Tmp):
    def test_r11_found_by_sha(self):
        self.assertEqual(_sha256(R11 + "/" + PRX), R11_SHA)
        self.assertEqual(_sha256(R11_PACKAGED), R11_SHA)
        self.assertTrue(R11_PACKAGED.startswith(R11_FROZEN))

    def test_frozen_copy_with_wrong_sha_fails_loudly(self):
        with self.assertRaises(BuildNotFound):
            locate_build(B6B_SHA, frozen_prx=R11_FROZEN + "/" + PRX)

    def test_unknown_build_fails_loudly(self):
        fake = self.p("fake.prx")
        with open(fake, "wb") as f:
            f.write(b"not a build")
        with self.assertRaises(BuildNotFound) as cm:
            locate_build(_sha256(fake), frozen_prx=fake)
        self.assertIn("no package folder holds", str(cm.exception))


class ElfBasics(unittest.TestCase):
    def test_build_ids_equal_kernel_fingerprints(self):
        # B6b bare build-id == fingerprint the kernel printed for our PRX in launch 3;
        # game image build-id == fingerprint printed for /app0/eboot.bin.
        lines = crashcap.read_text_lines(L3 + "/klog-20261012-162139.log")
        blk = crashcap.parse_klog(lines)[0]
        fps = {m["path"]: m["fingerprint"] for m in blk["modules"]}
        self.assertEqual(elf64.Elf(B6B + "/bare-original/" + PRX).gnu_build_id(),
                         fps["/data/plugins/" + PRX])
        self.assertEqual(elf64.Elf(EBOOT).gnu_build_id(), fps["/app0/eboot.bin"])

    def test_xotext_extent_equals_exec_segment(self):
        lines = crashcap.read_text_lines(L3 + "/klog-20261012-162139.log")
        blk = crashcap.parse_klog(lines)[0]
        mods = {m["path"]: m for m in blk["modules"]}
        for path, img in (("/data/plugins/" + PRX, B6B + "/bare-original/" + PRX), ("/app0/eboot.bin", EBOOT)):
            xl = elf64.Elf(img).exec_loads()[0]
            self.assertEqual(xl["vaddr"], 0)
            m = mods[path]
            self.assertEqual(m["end"] - m["start"], (xl["memsz"] + 0x3FFF) // 0x4000 * 0x4000, path)


# ----------------------------------------------------------------- crashcap --
class KlogParser(unittest.TestCase):
    def test_every_retained_banner_parses(self):
        files = [L3 + "/klog-20261012-162139.log",
                 O + "/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/klog-20261008-173728.log",
                 O + "/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/klog-20261007-170208.log",
                 L25 + "/evidence/runtime/l25-variant-A/klog-20261011-204957.log"]
        total = 0
        for f in files:
            lines = crashcap.read_text_lines(f)
            n = sum(1 for l in lines if l.strip() == "# " + crashcap.BANNER)
            blocks = crashcap.parse_klog(lines)
            self.assertEqual(len(blocks), n, f)
            for b in blocks:
                self.assertIn("rip", b["registers"])
                self.assertEqual(len([k for k in b["registers"] if k.startswith("r")]), 17, (f, b["line"]))  # 16 GPR + rip (glued lines salvaged)
                self.assertIn("eflags", b["registers"])
            total += n
        self.assertGreaterEqual(total, 14)

    def test_sigsegv_block_fields(self):
        lines = crashcap.read_text_lines(
            O + "/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/klog-20261008-173728.log")
        b = crashcap.parse_klog(lines)[1]
        self.assertEqual(b["signal"], "11 (SIGSEGV)")
        self.assertEqual(b["thread_name"], "RTHeartBeat 0")
        self.assertEqual(b["fault_va"], 0xc80004000)          # data address (addr(VA))
        self.assertEqual(b["fault_pc"], b["registers"]["rip"])  # 'fault address' = instruction
        self.assertEqual(b["registers"]["r8"], 0)               # 'r8 :' spacing form

    def test_decoder_matches_project_decoder_on_nul_log(self):
        log = L3 + "/harvest-l26-B6b-launch3-20261012-ue4ss.log"
        with open(log, "rb") as f:
            self.assertIn(b"\0", f.read())                        # really NUL-interleaved
        crashcap.load_decoder(crashcap.DEFAULT_DECODER)
        a = crashcap.read_text_lines(log)
        crashcap._DECODER = None
        b = crashcap.read_text_lines(log)                         # built-in fallback rule
        self.assertEqual(a, b)
        self.assertGreater(len(a), 100)


class CrashcapReal(Tmp):
    def test_launch3_real_crash_fully_symbolized(self):
        rc, out, err = run_cc(["--klog", L3 + "/klog-20261012-162139.log", "--symbols", FS_B6B, "--image", EBOOT,
                               "--log", "ue4ss=" + L3 + "/harvest-l26-B6b-launch3-20261012-ue4ss.log",
                               "--out", self.p("r.md"), "--json", self.p("r.json"), "--expect-crash"])
        self.assertEqual(rc, 0, err)
        d = json.load(open(self.p("r.json")))
        self.assertEqual(d["stats"]["blocks"], 1)
        blk = d["blocks"][0]
        ours = [f for f in blk["frames_sym"] if f.get("tier") == "OBJ-LAYOUT-BYTE-VERIFIED"]
        self.assertEqual(len(ours), 18)
        self.assertTrue(all(f["inside"] for f in ours))
        verdicts = [f["edge"]["verdict"] for f in blk["frames_sym"]]
        self.assertGreaterEqual(sum(v.startswith("CONFIRMED") for v in verdicts), 13)
        self.assertEqual(sum(v.startswith("NOT-A-CALL") for v in verdicts), 0)
        self.assertEqual(sum(v.startswith("CALL-TARGET-DIFFERS") for v in verdicts), 0)
        self.assertIn("ThunkILm61", ours[-1]["symbol"])           # engine-called entry point
        self.assertEqual(ours[7]["symbol"], "_Znwm")              # operator new -> __cxa_throw
        md = open(self.p("r.md")).read()
        self.assertIn("CALLED FROM our runtime", md)
        self.assertIn("Last 40 lines of ue4ss", md)
        self.assertIn("scanned 6950 klog lines", err)

    def test_other_build_symbols_never_attach(self):
        # R11 symbols against the B6b crash: fingerprint differs -> module+offset only
        rc, out, err = run_cc(["--klog", L3 + "/klog-20261012-162139.log", "--symbols", FS_R11,
                               "--json", self.p("r.json"), "--out", self.p("r.md")])
        self.assertEqual(rc, 0, err)
        d = json.load(open(self.p("r.json")))
        self.assertEqual(d["stats"]["exact"], 0)
        prx = [f for f in d["blocks"][0]["frames_sym"] if (f.get("module") or "").endswith(PRX)]
        self.assertEqual(len(prx), 18)
        self.assertTrue(all("no local artifact with build-id 32f8da09" in f["binding"] for f in prx))

    def test_clean_launch14_has_no_crash(self):
        rc, out, err = run_cc(["--klog", L14 + "/klog-20261013-014916.log", "--symbols", FS_R11,
                               "--out", self.p("r.md")])
        self.assertEqual(rc, 0, err)
        self.assertIn("crash blocks=0", err)
        self.assertIn("NO CRASH BLOCKS", open(self.p("r.md")).read())
        rc, out, err = run_cc(["--klog", L14 + "/klog-20261013-014916.log", "--expect-crash"])
        self.assertEqual(rc, 1)

    def test_empty_klog_is_a_failed_gate(self):
        open(self.p("empty.log"), "w").close()
        rc, out, err = run_cc(["--klog", self.p("empty.log")])
        self.assertEqual(rc, 3)
        self.assertIn("scanned 0 klog lines", err)

    def test_refuses_overwrite(self):
        open(self.p("r.md"), "w").close()
        rc, out, err = run_cc(["--klog", L14 + "/klog-20261013-014916.log", "--out", self.p("r.md")])
        self.assertEqual(rc, 2)


class CrashcapSynthetic(Tmp):
    """The expected shape of the cap05 launch crash, built against the REAL R11
    artifact bytes and symbols (the cap05 function itself does not exist in R11,
    so a real R11 function stands in for it)."""

    @classmethod
    def setUpClass(cls):
        d = json.load(open(FS_R11))
        cls.memsz = d["bare_prx"]["exec_load"]["memsz"]
        syms = {s[0]: (s[1], s[2]) for s in d["symbols"]}
        cls.throw = syms["__cxa_throw"]
        new_addr, new_size = syms["_Znwm"]
        e = elf64.Elf(R11 + "/bare-original/" + PRX)
        code = e.read_va(new_addr, new_size)
        cls.ret = None
        for k in range(len(code) - 5):
            if code[k] == 0xE8:
                tgt = new_addr + k + 5 + struct.unpack_from("<i", code, k + 1)[0]
                if tgt == cls.throw[0]:
                    cls.ret = new_addr + k + 5
                    break
        cls.new_addr = new_addr
        e.close()
        assert cls.ret, "operator new in R11 has no direct call to __cxa_throw"

    def klog(self, base=0x80a8c000, extent=None, fp="7423ffe671bfefbc444e887935731e9700000000", frames=None):
        extent = extent if extent is not None else (self.memsz + 0x3FFF) // 0x4000 * 0x4000
        rip = base + self.throw[0] + 4
        frames = frames if frames is not None else [base + self.ret]
        L = ["<118>[noise] before",
             "#", "# A user thread receives a fatal signal", "#",
             "# signal: 11 (SIGSEGV)",
             "<118>[SceShellUI] interleaved noise line",
             "# thread ID: 101", "# thread name: TestWorker", "# proc ID: 99", "# proc name: eboot.bin",
             "# reason: page fault# reason: Unmapped page access, Protection fault addr(VA): 0x0000000000000c08",
             "# fault address: %016x (xotext:\"%s\"+0x%x)" % (rip, PRX, rip - base),
             "#", "# registers:",
             "# rax: 0000000000000000  rbx: 0000000000000001",
             "# r8 : 0000000000000000  r9 : 0000000000000000",
             "# rip: %016x  eflags: 00010246" % rip,
             "# lbf: ffffffffffffffff  lbt: ffffffffffffffff",
             "#", "# backtrace:", "copyin: TestWorker has nonsleeping lock"]
        L += ["# %016x" % f for f in frames] + ["# 0000000000000000", "#", "# dynamic libraries:",
              "# /app0/eboot.bin", "#  xotext: 0000000000400000:000000000787c000 nsegs: 4",
              "#  fingerprint: 04bb4330a7d71dcefe1cc2c4d19769d500000000",
              "# /data/plugins/" + PRX, "#  xotext: %016x:%016x nsegs: 4" % (base, base + extent),
              "#  fingerprint: " + fp, "<118>[Syscore App] App Crash : PID=0x63"]
        path = self.p("synthetic-klog.log")
        with open(path, "w") as f:
            f.write("\n".join(L) + "\n")
        return path

    def test_expected_launch_shape_passes(self):
        k = self.klog()
        rc, out, err = run_cc(["--klog", k, "--symbols", FS_R11, "--json", self.p("r.json"), "--out", self.p("r.md"),
                               "--expect-crash", "--expect-rip-symbol", "__cxa_throw", "--expect-fault-va", "0xc08"])
        self.assertEqual(rc, 0, err)
        self.assertIn("EXPECT PASS: RIP inside '__cxa_throw'", err)
        self.assertIn("EXPECT PASS: fault VA 0xc08", err)
        d = json.load(open(self.p("r.json")))
        fr = d["blocks"][0]["frames_sym"][0]
        self.assertEqual(fr["symbol"], "_Znwm")
        self.assertTrue(fr["edge"]["verdict"].startswith("CONFIRMED"), fr["edge"])
        self.assertIn("INSIDE our runtime", open(self.p("r.md")).read())

    def test_wrong_rip_symbol_expectation_fails(self):
        rc, out, err = run_cc(["--klog", self.klog(), "--symbols", FS_R11, "--out", self.p("r.md"),
                               "--expect-rip-symbol", "modbridge_cap05_crashtest_fault"])
        self.assertEqual(rc, 1)

    def test_extent_mismatch_refuses_symbols(self):
        k = self.klog(extent=(self.memsz + 0x3FFF) // 0x4000 * 0x4000 + 0x4000)
        rc, out, err = run_cc(["--klog", k, "--symbols", FS_R11, "--json", self.p("r.json"), "--out", self.p("r.md"),
                               "--expect-rip-symbol", "__cxa_throw"])
        self.assertEqual(rc, 1)
        d = json.load(open(self.p("r.json")))
        self.assertIn("CONVENTION-MISMATCH", d["blocks"][0]["rip_sym"]["binding"])
        self.assertNotIn("symbol", d["blocks"][0]["rip_sym"])

    def test_unaligned_base_refuses_symbols(self):
        k = self.klog(base=0x80a8c000 + 0x1000)
        rc, out, err = run_cc(["--klog", k, "--symbols", FS_R11, "--json", self.p("r.json"), "--out", self.p("r.md")])
        d = json.load(open(self.p("r.json")))
        self.assertIn("CONVENTION-MISMATCH", d["blocks"][0]["rip_sym"]["binding"])

    def test_stale_slot_is_flagged(self):
        base = 0x80a8c000
        k = self.klog(frames=[base + self.new_addr])     # a function START is never a return address
        rc, out, err = run_cc(["--klog", k, "--symbols", FS_R11, "--json", self.p("r.json"), "--out", self.p("r.md")])
        d = json.load(open(self.p("r.json")))
        self.assertTrue(d["blocks"][0]["frames_sym"][0]["edge"]["verdict"].startswith("NOT-A-CALL"),
                        d["blocks"][0]["frames_sym"][0]["edge"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
