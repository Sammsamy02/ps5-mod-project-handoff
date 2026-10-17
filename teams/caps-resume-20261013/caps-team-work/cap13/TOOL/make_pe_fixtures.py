#!/usr/bin/env python3
"""make_pe_fixtures.py -- hand-crafted Windows PE test fixtures for capability #13.

Mac-only; builds byte-deterministic PE images with `struct` (no compiler, no
cross-toolchain, nothing is ever executed). Every emitted file is a clearly
labeled FIXTURE, not a real mod.

Fixtures:
  1. cap13-ue4ss-native-mod-FIXTURE.dll
     x64 PE32+ DLL shaped like a minimal UE4SS native (C++) mod:
       - exports start_mod / stop_mod (the UE4SS native-mod lifecycle entry
         points that capability #9's host currently refuses)
       - imports UE4SS.dll!is_ue4ss_initialized and
         KERNEL32.dll!DisableThreadLibraryCalls
       - DllMain calls both imports (see report for the loader-lock caveat)
       - start_mod calls is_ue4ss_initialized again, returns 0
     The imported UE4SS symbol is REAL: 'is_ue4ss_initialized' is one of the
     six unmangled C-API exports observed in the actual UE4SS.dll v3.0.1
     build staged from the Townfall mod intake (see REPORT/mod-selection.md).
  2. cap13-native-x86-FIXTURE.dll
     32-bit PE32 variant used to exercise architecture detection and the
     unsupported-arch classification (32-bit images cannot load into the
     64-bit PS5 host process).
  3. NOT-A-PE-FIXTURE.dll
     Plain-text bytes with a .dll name; must classify as 'unknown'.

Usage: python3 make_pe_fixtures.py [--outdir FIXTURES] [--manifest FILE]
"""

import argparse
import hashlib
import json
import os
import struct
import sys

FIXTURE_MARKER = (
    b"CAP13 FIXTURE: hand-crafted minimal PE for PE-analyzer testing; "
    b"not a real mod; not for distribution.\x00"
)

# Real C-API export observed in staged UE4SS.dll (see REPORT/mod-selection.md)
UE4SS_CAPI_FUNCTION = "is_ue4ss_initialized"
KERNEL32_FUNCTION = "DisableThreadLibraryCalls"

SECTION_ALIGN = 0x1000
FILE_ALIGN = 0x200


def align(n: int, a: int) -> int:
    return (n + a - 1) & ~(a - 1)


class PEBuilder(object):
    """Deterministic PE32+ (x64) or PE32 (x86) DLL with fixed 3-section layout."""

    def __init__(self, pe32plus: bool):
        self.pe32plus = pe32plus
        self.thunk_fmt = "<Q" if pe32plus else "<I"
        self.thunk_size = 8 if pe32plus else 4
        self.machine = 0x8664 if pe32plus else 0x014C
        self.image_base = 0x180000000 if pe32plus else 0x10000000

        # Fixed layout (identical shape for both variants)
        self.headers_raw = 0x400          # SizeOfHeaders
        self.sec = {
            ".text":  {"rva": 0x1000, "raw": 0x400, "rsize": 0x200, "vsize": 0x1000,
                       "chars": 0x60000020},
            ".rdata": {"rva": 0x2000, "raw": 0x600, "rsize": 0x200, "vsize": 0x1000,
                       "chars": 0x40000040},
            ".idata": {"rva": 0x3000, "raw": 0x800, "rsize": 0x200, "vsize": 0x1000,
                       "chars": 0xC0000040},
        }
        self.size_of_image = 0x4000

    # -- placement -----------------------------------------------------------

    def rva(self, sec: str, off: int) -> int:
        return self.sec[sec]["rva"] + off

    def fileoff(self, sec: str, off: int) -> int:
        return self.sec[sec]["raw"] + off

    # -- .text code ----------------------------------------------------------

    def build_text(self) -> bytes:
        """Returns (text_bytes, layout) with rip-relative displacements resolved."""
        t = bytearray(0x200)

        def put(off: int, data: bytes) -> None:
            t[off:off + len(data)] = data

        if self.pe32plus:
            # DllMain(hinstDLL in rcx, fdwReason rdx, lpReserved r8)
            dllmain = [
                (0x00, bytes.fromhex("4883ec28")),        # sub rsp, 0x28
                (0x04, bytes.fromhex("ff1500000000")),    # call [rip+rel32] -> DisableThreadLibraryCalls
                (0x0A, bytes.fromhex("31c9")),            # xor ecx, ecx
                (0x0C, bytes.fromhex("ff1500000000")),    # call [rip+rel32] -> is_ue4ss_initialized
                (0x12, bytes.fromhex("b801000000")),      # mov eax, 1   (return TRUE)
                (0x17, bytes.fromhex("4883c428")),        # add rsp, 0x28
                (0x1B, bytes.fromhex("c3")),              # ret
            ]
            # int start_mod(void)
            start_mod = [
                (0x00, bytes.fromhex("31c9")),            # xor ecx, ecx
                (0x02, bytes.fromhex("ff1500000000")),    # call [rip+rel32] -> is_ue4ss_initialized
                (0x08, bytes.fromhex("31c0")),            # xor eax, eax  (return 0)
                (0x0A, bytes.fromhex("c3")),              # ret
            ]
            # void stop_mod(void)
            stop_mod = [
                (0x00, bytes.fromhex("31c0")),            # xor eax, eax
                (0x02, bytes.fromhex("c3")),              # ret
            ]
            e_off, s_off, p_off = 0x00, 0x20, 0x30
        else:
            # x86 cdecl-ish stubs; imports called through absolute IAT addresses
            # (fixed-up by the loader at load time; fixture is never executed).
            dllmain = [
                (0x00, bytes.fromhex("ff1500000000")),    # call dword [abs IAT] -> DisableThreadLibraryCalls
                (0x06, bytes.fromhex("ff1500000000")),    # call dword [abs IAT] -> is_ue4ss_initialized
                (0x0C, bytes.fromhex("b801000000")),      # mov eax, 1
                (0x11, bytes.fromhex("c20400")),          # ret 12  (stdcall DllMain)
            ]
            start_mod = [
                (0x00, bytes.fromhex("ff1500000000")),    # call dword [abs IAT]
                (0x06, bytes.fromhex("31c0")),            # xor eax, eax
                (0x08, bytes.fromhex("c3")),              # ret
            ]
            stop_mod = [
                (0x00, bytes.fromhex("31c0")),
                (0x02, bytes.fromhex("c3")),
            ]
            e_off, s_off, p_off = 0x00, 0x20, 0x30

        layout = {"dllmain_rva": self.rva(".text", e_off),
                  "start_mod_rva": self.rva(".text", s_off),
                  "stop_mod_rva": self.rva(".text", p_off)}

        for off, code in dllmain:
            put(e_off + off, code)
        for off, code in start_mod:
            put(s_off + off, code)
        for off, code in stop_mod:
            put(p_off + off, code)

        # IAT slot RVAs (filled by caller before build())
        return bytes(t), layout

    # -- import/export tables --------------------------------------------------

    def build_rdata_and_idata(self, layout):
        """Lay out descriptors, hint/name data, INT/IAT. Returns (rdata, idata, info)."""
        ord_len = len(UE4SS_CAPI_FUNCTION.encode()) + 1 + 2
        k32_len = len(KERNEL32_FUNCTION.encode()) + 1 + 2

        # --- .idata: INT + IAT arrays -------------------------------------
        # UE4SS INT/IAT: 2 slots + NULL ; K32: 1 slot + NULL
        idata = bytearray(0x200)
        int_ue_off, iat_ue_off = 0x00, 0x20
        int_k_off, iat_k_off = 0x40, 0x60
        # --- .rdata placement plan ----------------------------------------
        # descriptors (3*20=60) @0x00, export dir (40) @0x40,
        # export name ptrs (2*4) @0x68, ordinals (2*2) @0x70,
        # then strings: dll self-name, start_mod, stop_mod, UE4SS.dll,
        # KERNEL32.dll, hint/name entries, marker.
        rdata = bytearray(0x200)

        def rstr(off: int, s: str) -> bytes:
            b = s.encode() + b"\x00"
            rdata[off:off + len(b)] = b
            return b

        # strings at fixed offsets
        s_dll_self, o_dll_self = rstr, 0x80
        rdata[0x80:0x80]  # no-op
        self_name = (b"cap13-ue4ss-native-mod-FIXTURE.dll\x00" if self.pe32plus
                     else b"cap13-native-x86-FIXTURE.dll\x00")
        rdata[0x80:0x80 + len(self_name)] = self_name
        o_start_mod = 0x80 + len(self_name)
        nb = b"start_mod\x00"
        rdata[o_start_mod:o_start_mod + len(nb)] = nb
        o_stop_mod = o_start_mod + len(nb)
        nb = b"stop_mod\x00"
        rdata[o_stop_mod:o_stop_mod + len(nb)] = nb
        o_ue4ss = o_stop_mod + len(nb)
        nb = b"UE4SS.dll\x00"
        rdata[o_ue4ss:o_ue4ss + len(nb)] = nb
        o_k32 = o_ue4ss + len(nb)
        nb = b"KERNEL32.dll\x00"
        rdata[o_k32:o_k32 + len(nb)] = nb
        o_hn_ue = o_k32 + len(nb)
        nb = b"\x00\x00" + UE4SS_CAPI_FUNCTION.encode() + b"\x00"   # hint 0 + name
        rdata[o_hn_ue:o_hn_ue + len(nb)] = nb
        o_hn_k = o_hn_ue + len(nb)
        nb = b"\x00\x00" + KERNEL32_FUNCTION.encode() + b"\x00"
        rdata[o_hn_k:o_hn_k + len(nb)] = nb
        o_marker = o_hn_k + len(nb)
        rdata[o_marker:o_marker + len(FIXTURE_MARKER)] = FIXTURE_MARKER

        # hint/name RVAs
        hn_ue_rva = self.rva(".rdata", o_hn_ue)
        hn_k_rva = self.rva(".rdata", o_hn_k)

        # INT/IAT entries: unbound -> point at hint/name RVAs (as linkers emit)
        struct.pack_into(self.thunk_fmt, idata, int_ue_off + 0 * self.thunk_size, hn_ue_rva)
        struct.pack_into(self.thunk_fmt, idata, iat_ue_off + 0 * self.thunk_size, hn_ue_rva)
        struct.pack_into(self.thunk_fmt, idata, int_k_off, hn_k_rva)
        struct.pack_into(self.thunk_fmt, idata, iat_k_off, hn_k_rva)
        # NULL terminators already zero

        iat_ue_rva = self.rva(".idata", iat_ue_off)
        iat_k_rva = self.rva(".idata", iat_k_off)
        int_ue_rva = self.rva(".idata", int_ue_off)
        int_k_rva = self.rva(".idata", int_k_off)
        ue4ss_name_rva = self.rva(".rdata", o_ue4ss)
        k32_name_rva = self.rva(".rdata", o_k32)

        # import descriptors: UE4SS first, then KERNEL32, then NULL
        desc = struct.pack("<IIIII", int_ue_rva, 0, 0, ue4ss_name_rva, iat_ue_rva)
        desc += struct.pack("<IIIII", int_k_rva, 0, 0, k32_name_rva, iat_k_rva)
        desc += struct.pack("<IIIII", 0, 0, 0, 0, 0)
        rdata[0x00:0x3C] = desc

        # export directory @0x40
        name_ptr_rva = self.rva(".rdata", 0x68)
        ordinal_rva = self.rva(".rdata", 0x70)
        eat_rva = self.rva(".rdata", 0x78)
        struct.pack_into("<IIHHIIIIIII", rdata, 0x40,
                         0,                      # characteristics
                         0,                      # timestamp (deterministic)
                         0, 0,                   # version
                         self.rva(".rdata", 0x80),   # self name RVA
                         1,                      # ordinal base
                         2,                      # address table entries
                         2,                      # number of name pointers
                         eat_rva, name_ptr_rva, ordinal_rva)
        struct.pack_into("<II", rdata, 0x68,
                         self.rva(".rdata", o_start_mod),
                         self.rva(".rdata", o_stop_mod))
        struct.pack_into("<HH", rdata, 0x70, 0, 1)   # ordinals for the two names
        struct.pack_into("<II", rdata, 0x78,
                         layout["start_mod_rva"], layout["stop_mod_rva"])

        info = {
            "iat_ue4ss_rva": iat_ue_rva,
            "iat_kernel32_rva": iat_k_rva,
            "import_dir_rva": self.rva(".rdata", 0x00),
            "import_dir_size": 60,
            "export_dir_rva": self.rva(".rdata", 0x40),
            "export_dir_size": 40 + 8 + 4 + 4,
        }
        return bytes(rdata), bytes(idata), info


def build(pe32plus: bool) -> bytes:
    b = PEBuilder(pe32plus)
    text, layout = b.build_text()

    # Resolve rip-relative displacements now that IAT RVAs are known
    t = bytearray(text)
    if pe32plus:
        iat_k = b.rva(".idata", 0x60)
        iat_ue = b.rva(".idata", 0x20)
        # DllMain @0x00: call [rip+rel] at 0x04 (next instr 0x0A) -> K32 slot
        rel = iat_k - (b.rva(".text", 0x04) + 6)
        struct.pack_into("<i", t, 0x04 + 2, rel)
        # call at 0x0C (next 0x12) -> UE4SS slot
        rel = iat_ue - (b.rva(".text", 0x0C) + 6)
        struct.pack_into("<i", t, 0x0C + 2, rel)
        # start_mod @0x20: call at 0x22 (next 0x28) -> UE4SS slot
        rel = iat_ue - (b.rva(".text", 0x22) + 6)
        struct.pack_into("<i", t, 0x22 + 2, rel)
    else:
        iat_k = b.image_base + b.rva(".idata", 0x60)
        iat_ue = b.image_base + b.rva(".idata", 0x20)
        struct.pack_into("<I", t, 0x00 + 2, iat_k)   # DllMain call #1
        struct.pack_into("<I", t, 0x06 + 2, iat_ue)  # DllMain call #2
        struct.pack_into("<I", t, 0x20 + 2, iat_ue)  # start_mod call
    text = bytes(t)

    rdata, idata, info = b.build_rdata_and_idata(layout)

    # ---- DOS header + stub -------------------------------------------------
    dos = bytearray(0x80)
    dos[0:2] = b"MZ"
    dos[0x3C:0x40] = struct.pack("<I", 0x80)
    stub = (b"\x0e\x1f\xba\x0e\x00\xb4\x09\xcd\x21\xb8\x01\x4c\xcd\x21"
            b"CAP13 FIXTURE (not a real mod).$")
    assert 0x40 + len(stub) <= 0x80, "DOS stub must not overflow the 0x80-byte DOS area"
    dos[0x40:0x40 + len(stub)] = stub
    assert len(dos) == 0x80

    # ---- COFF header --------------------------------------------------------
    num_sections = 3
    opt_size = 240 if pe32plus else 224
    coff = struct.pack("<HHIIIHH",
                       b.machine,
                       num_sections,
                       0,          # TimeDateStamp: deterministic build
                       0, 0,       # symbol table (none)
                       opt_size,
                       0x2022)     # EXECUTABLE_IMAGE | DLL | LARGE_ADDRESS_AWARE

    # ---- Optional header ----------------------------------------------------
    ep = layout["dllmain_rva"]
    if pe32plus:
        opt = struct.pack("<HBBIIIII",
                          0x20B, 0, 0,
                          len(text), 0x600, 0,
                          ep, b.rva(".text", 0))
        opt += struct.pack("<Q", b.image_base)
        opt += struct.pack("<II", SECTION_ALIGN, FILE_ALIGN)
        opt += struct.pack("<HHHHHHIIIIHH",
                           6, 0,       # OS version
                           0, 0,       # image version
                           6, 0,       # subsystem version
                           0,          # Win32VersionValue
                           b.size_of_image,
                           b.headers_raw,
                           0,          # checksum (0 = verifier skipped)
                           2,          # Subsystem: WINDOWS_GUI
                           0x0100)     # DllCharacteristics: NX_COMPAT only (no .reloc -> no ASLR claim)
        opt += struct.pack("<QQQQ", 0x100000, 0x1000, 0x100000, 0x1000)
        opt += struct.pack("<II", 0, 16)
        dirs = [(0, 0)] * 16
        dirs[0] = (info["export_dir_rva"], info["export_dir_size"])     # EXPORT
        dirs[1] = (info["import_dir_rva"], info["import_dir_size"])     # IMPORT
        dirs[12] = (b.rva(".idata", 0x20), 0x28)                        # IAT
        for rva, size in dirs:
            opt += struct.pack("<II", rva, size)
        assert len(opt) == 240, len(opt)
    else:
        opt = struct.pack("<HBBIIIIII",
                          0x10B, 0, 0,
                          len(text), 0x600, 0,
                          ep, b.rva(".text", 0), b.rva(".text", 0))
        opt += struct.pack("<I", b.image_base)
        opt += struct.pack("<II", SECTION_ALIGN, FILE_ALIGN)
        opt += struct.pack("<HHHHHHIIIIHH",
                           6, 0, 0, 0, 6, 0, 0,
                           b.size_of_image, b.headers_raw, 0,
                           2, 0x0100)
        opt += struct.pack("<IIII", 0x100000, 0x1000, 0x100000, 0x1000)
        opt += struct.pack("<II", 0, 16)
        dirs = [(0, 0)] * 16
        dirs[0] = (info["export_dir_rva"], info["export_dir_size"])
        dirs[1] = (info["import_dir_rva"], info["import_dir_size"])
        dirs[12] = (b.rva(".idata", 0x20), 0x18)
        for rva, size in dirs:
            opt += struct.pack("<II", rva, size)
        assert len(opt) == 224, len(opt)

    # ---- Section headers ----------------------------------------------------
    sec_hdr = b""
    names = {".text": bytearray(b".text\x00\x00\x00"),
             ".rdata": bytearray(b".rdata\x00\x00"),
             ".idata": bytearray(b".idata\x00\x00")}
    for name in (".text", ".rdata", ".idata"):
        s = b.sec[name]
        sec_hdr += bytes(names[name])
        sec_hdr += struct.pack("<IIII", s["vsize"], s["rva"], s["rsize"], s["raw"])
        sec_hdr += struct.pack("<I", 0)          # PointerToRelocations
        sec_hdr += struct.pack("<I", 0)          # PointerToLinenumbers
        sec_hdr += struct.pack("<HH", 0, 0)      # NumberOfRelocations/Linenumbers
        sec_hdr += struct.pack("<I", s["chars"])
    assert len(sec_hdr) == 120

    image = bytearray(b.headers_raw)
    image[0:len(dos)] = dos
    off = len(dos)
    image[off:off + 4] = b"PE\x00\x00"
    off += 4
    image[off:off + len(coff)] = coff
    off += len(coff)
    image[off:off + len(opt)] = opt
    off += len(opt)
    image[off:off + len(sec_hdr)] = sec_hdr

    for name, content in ((".text", text), (".rdata", rdata), (".idata", idata)):
        s = b.sec[name]
        image[s["raw"]:s["raw"] + s["rsize"]] = content[:s["rsize"]]

    return bytes(image), info, layout


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--outdir", default=os.path.join(os.path.dirname(__file__), "..", "FIXTURES"))
    ap.add_argument("--manifest", default=None)
    args = ap.parse_args()
    outdir = os.path.abspath(args.outdir)
    os.makedirs(outdir, exist_ok=True)

    manifest = {"generator": "make_pe_fixtures.py (hand-crafted struct-built PE, deterministic)",
                "fixtures": []}

    files = [
        ("cap13-ue4ss-native-mod-FIXTURE.dll", True),
        ("cap13-native-x86-FIXTURE.dll", False),
    ]
    for fname, plus in files:
        blob, info, layout = build(plus)
        path = os.path.join(outdir, fname)
        with open(path, "wb") as f:
            f.write(blob)
        manifest["fixtures"].append({
            "file": fname,
            "kind": "x64 PE32+ DLL fixture" if plus else "x86 PE32 DLL fixture",
            "bytes": len(blob),
            "sha256": hashlib.sha256(blob).hexdigest(),
            "layout": {"text": layout, "tables": info},
        })
        print("wrote %s (%d bytes, sha256 %s)" % (path, len(blob),
                                                  manifest["fixtures"][-1]["sha256"]))

    notape = os.path.join(outdir, "NOT-A-PE-FIXTURE.dll")
    with open(notape, "wb") as f:
        f.write(b"CAP13 FIXTURE: deliberately NOT a PE image; used to test the "
                b"'unknown' classification path.\n")
    manifest["fixtures"].append({
        "file": os.path.basename(notape),
        "kind": "non-PE text with .dll extension (unknown-classification fixture)",
        "bytes": os.path.getsize(notape),
        "sha256": hashlib.sha256(open(notape, "rb").read()).hexdigest(),
    })
    print("wrote %s" % notape)

    if args.manifest:
        with open(args.manifest, "w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        print("wrote %s" % args.manifest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
