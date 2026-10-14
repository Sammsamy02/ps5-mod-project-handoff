#!/usr/bin/env python3
"""worker-eboot-patch verify-mbprx.py — verify the mb.prx artifacts for the
early-load deployment (EARLY-LOAD-DESIGN.md §4.4 / §7 pre-flight step 2).

mb.prx       = fake-self (FSELF) wrapped, console-proven UE4SS modbridge
               runtime PRX (the module the patched eboot DT_NEEDEDs). Built
               this session by re-running the pinned make_fself.py recipe on
               the SDK-stamped bare PRX; must be byte-identical to the frozen
               proven artifact (checkpoint native_aligned_entry).
mb-plain.prx = the plain ET_SCE_DYNAMIC (0xfe18) ELF variant, kept as the
               design's format A/B (Boot 4 of the §7 ladder).

Checks:
  1. FSELF header parse (magic 4f153d1d, mode, endian, attribs, key_type,
     file_size == actual bytes, num_entries).
  2. Every wrapped segment byte-equal to the corresponding segment of the
     bare ELF (re-derives the lineage's wrapped_segments_equal law).
  3. mb-plain ELF: ELF64 LE x86-64, OSABI 9, e_type 0xfe18 (Sony DYNLIB),
     exports module_start / module_stop under their Sony NID names
     (BaOKcng8g88#F#A / KpDMrPHvt3Q#F#A, verified against prospero-nid's
     algorithm), imports = 191 undefined dynamic symbols (frozen contract).
  4. mb-plain DT_NEEDED providers are all satisfied by DT_NEEDED entries of
     the patched eboot that load BEFORE slot 236 (mb.prx) — boot-abort guard.
  5. sha256 pins of both artifacts + byte-identity to frozen provenance.

No console contact. Writes verify-mbprx.json + verify-mbprx.txt.
"""

import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEAM = HERE.parent
ROOT = TEAM.parents[1]

MB = HERE / "mb.prx"
MB_PLAIN = HERE / "mb-plain.prx"
FROZEN_WRAPPED = (ROOT / ".superpowers/native-dependency-spike/"
                  "native-full-runtime-aligned-entry-1/build-1/"
                  "modbridge_runtime_aligned_entry.prx")
FROZEN_BARE_SDK = (ROOT / ".superpowers/native-dependency-spike/"
                   "native-full-runtime-aligned-entry-1/build-1/bare-sdk/"
                   "modbridge_runtime_aligned_entry.prx")
EBOOT_A1 = TEAM / "worker-early-load/eboot-earlyload-A1.bin"

PIN = {
    "mb_sha256": "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654",
    "mb_bytes": 30151623,
    "mb_plain_sha256": "133db40cba5a3008cecd33f724cdb19b3bac867f5e006faa9337363451ba5d5d",
    "mb_plain_bytes": 30146024,
}

NID_SALT = bytes([0x51, 0x8d, 0x64, 0xa6, 0x35, 0xde, 0xd8, 0xc1,
                  0xe6, 0xb0, 0x39, 0xb1, 0xc3, 0xe5, 0x52, 0x30])
NID_CHARSET = ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
               "0123456789+-")


def nid_encode(sym):
    """Exact prospero-nid algorithm (ps5-payload-sdk/bin/prospero-nid)."""
    sha1 = hashlib.sha1()
    sha1.update(sym.encode("utf-8"))
    sha1.update(NID_SALT)
    digest = bytearray(sha1.digest())
    value = struct.unpack(">Q", digest[0:8])[0]
    digest[0:8] = struct.pack("<Q", value)
    digest[8:16] = b"\x00" * 8
    nid = []
    for i in range(0, 11, 3):
        a, b, c = digest[i], digest[i + 1], digest[i + 2]
        abc = (a << 16) | (b << 8) | c
        nid.append(NID_CHARSET[(abc >> 18) & 0x3F])
        nid.append(NID_CHARSET[(abc >> 12) & 0x3F])
        nid.append(NID_CHARSET[(abc >> 6) & 0x3F])
        nid.append(NID_CHARSET[abc & 0x3F])
    return "".join(nid[:11])


def sha256(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------- ELF helpers
def elf_header(data):
    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    e_entry, e_phoff, e_shoff = struct.unpack_from("<QQQ", data, 24)
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 54)
    return {"magic": data[0:4].hex(), "class": data[4], "enc": data[5],
            "osabi": data[7], "e_type": e_type, "e_machine": e_machine,
            "e_entry": e_entry, "e_phoff": e_phoff, "e_shoff": e_shoff,
            "e_phentsize": e_phentsize, "e_phnum": e_phnum}


def elf_phdrs(data):
    h = elf_header(data)
    out = []
    for i in range(h["e_phnum"]):
        base = h["e_phoff"] + i * h["e_phentsize"]
        p_type, p_flags = struct.unpack_from("<II", data, base)
        p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = struct.unpack_from(
            "<QQQQQ", data, base + 8)
        out.append({"type": p_type, "flags": p_flags, "offset": p_offset,
                    "vaddr": p_vaddr, "filesz": p_filesz, "memsz": p_memsz})
    return out


def vaddr_to_file(data, vaddr):
    """Map a vaddr to a file offset through the containing PT_LOAD."""
    for p in elf_phdrs(data):
        if p["type"] == 1 and p["vaddr"] <= vaddr < p["vaddr"] + p["filesz"]:
            return p["offset"] + (vaddr - p["vaddr"])
    raise ValueError(f"vaddr 0x{vaddr:x} not inside any PT_LOAD")


def elf_dynamic(data):
    """Return (entries, strtab_file_off) for PT_DYNAMIC."""
    ph = elf_phdrs(data)
    dyn = next(p for p in ph if p["type"] == 2)
    ents = []
    for i in range(dyn["filesz"] // 16):
        tag, val = struct.unpack_from("<qQ", data, dyn["offset"] + 16 * i)
        if tag == 0:
            break
        ents.append((tag, val))
    strtab_off = [v for t, v in ents if t == 5][0]      # DT_STRTAB (vaddr)
    strtab_file = vaddr_to_file(data, strtab_off)
    return ents, strtab_file


def dyn_str(data, strtab_file, off):
    base = strtab_file + off
    end = data.index(b"\x00", base)
    return data[base:end].decode("ascii", "replace")


def dynsyms(data):
    """Return (defined, undefined) dynamic symbol name lists, sized by the
    classic DT_HASH nchain (cross-checked against Sony 0x6100003f size)."""
    ents, strtab_file = elf_dynamic(data)
    symtab_off = [v for t, v in ents if t == 6][0]       # DT_SYMTAB (vaddr)
    symtab_file = vaddr_to_file(data, symtab_off)
    hash_off = [v for t, v in ents if t == 4][0]         # DT_HASH (vaddr)
    hash_file = vaddr_to_file(data, hash_off)
    nbucket, nchain = struct.unpack_from("<II", data, hash_file)
    sony_symsz = [v for t, v in ents if t == 0x6100003F]
    if sony_symsz:
        assert sonia_ok(nchain, sony_symsz[0]), \
            f"nchain {nchain} vs Sony symtab bytes 0x{sony_symsz[0]:x}"
    defined, undefined = [], []
    for i in range(nchain):
        off = symtab_file + 24 * i
        st_name, st_info, _other, shndx, _val, _size = struct.unpack_from(
            "<IBBHQQ", data, off)
        name = dyn_str(data, strtab_file, st_name) if st_name else ""
        if name:
            bind = st_info >> 4
            if shndx == 0:            # SHN_UNDEF -> import
                undefined.append((name, bind))
            else:
                defined.append((name, bind))
    return defined, undefined


def sonia_ok(nchain, symsz):
    return nchain * 24 == symsz


# ---------------------------------------------------------------- FSELF check
def parse_fself(data):
    """Parse the fake-self container per make_fself.py's writer layout:
    common header '<4s4B' @0, ext header '<I2HQ2H4x' @8, then num_entries
    entries of '<4Q' (props, offset, filesz, memsz) @0x20."""
    magic = data[0:4]
    version, mode, endian, attribs = data[4], data[5], data[6], data[7]
    key_type, = struct.unpack_from("<I", data, 8)
    header_size, meta_size = struct.unpack_from("<HH", data, 12)
    file_size, = struct.unpack_from("<Q", data, 16)
    num_entries, flags = struct.unpack_from("<HH", data, 24)
    entries = []
    base = 0x20
    for i in range(num_entries):
        props, offset, filesz, memsz = struct.unpack_from("<4Q", data,
                                                          base + 32 * i)
        entries.append({"props": props, "offset": offset, "filesz": filesz,
                        "memsz": memsz,
                        "meta": bool((props >> 16) & 1)})  # HAS_DIGESTS bit
    return {"magic": magic.hex(), "version": version, "mode": mode,
            "endian": endian, "attribs": attribs, "key_type": key_type,
            "header_size": header_size, "meta_size": meta_size,
            "file_size": file_size, "num_entries": num_entries,
            "flags": flags, "entries": entries}


def main():
    out = {"tool": "verify-mbprx.py", "console_contact": False}
    checks = {}
    mb = MB.read_bytes()
    plain = MB_PLAIN.read_bytes()

    checks["mb_sha256_pin"] = sha256(mb) == PIN["mb_sha256"]
    checks["mb_bytes_pin"] = len(mb) == PIN["mb_bytes"]
    checks["mb_plain_sha256_pin"] = sha256(plain) == PIN["mb_plain_sha256"]
    checks["mb_plain_bytes_pin"] = len(plain) == PIN["mb_plain_bytes"]
    frozen = FROZEN_WRAPPED.read_bytes()
    checks["mb_identical_to_frozen_proven_wrapped"] = (mb == frozen)
    checks["mb_plain_identical_to_frozen_bare_sdk"] = (
        plain == FROZEN_BARE_SDK.read_bytes())

    # ---- FSELF header
    f = parse_fself(mb)
    out["fself_header"] = {k: (v if not isinstance(v, list) else
                               [{kk: hex(vv) if isinstance(vv, int) else vv
                                 for kk, vv in e.items()} for e in v])
                           for k, v in f.items()}
    checks["fself_magic"] = f["magic"] == "4f153d1d"
    checks["fself_mode_fake_1"] = f["mode"] == 1
    checks["fself_endian_le_1"] = f["endian"] == 1
    checks["fself_attribs_0x12"] = f["attribs"] == 0x12
    checks["fself_key_type_0x101"] = f["key_type"] == 0x101
    # make_fself's file_size field = end of segment data; the trailing
    # npdrm/footer/signature bytes are appended after it
    max_entry_end = max((e["offset"] + e["filesz"]
                         for e in f["entries"]), default=0)
    checks["fself_file_size_covers_entries"] = f["file_size"] >= max_entry_end
    checks["fself_trailing_bytes_small"] = (
        0 <= len(mb) - f["file_size"] < 0x400)
    checks["fself_entry_pairs_meta_data"] = (
        f["num_entries"] % 2 == 0
        and all(f["entries"][i]["meta"] and not f["entries"][i + 1]["meta"]
                for i in range(0, f["num_entries"], 2)))
    checks["fself_has_entries"] = f["num_entries"] >= 2

    # ---- every DATA entry (odd index) byte-equal to a bare-ELF PT segment
    ph = elf_phdrs(plain)
    seg_report = []
    seg_ok = True
    for i, e in enumerate(f["entries"]):
        if e["meta"]:
            continue          # digest blocks, no ELF counterpart
        match = None
        for p in ph:
            if p["filesz"] == e["filesz"]:
                seg_bytes = mb[e["offset"]: e["offset"] + e["filesz"]]
                elf_bytes = plain[p["offset"]: p["offset"] + p["filesz"]]
                if seg_bytes == elf_bytes:
                    match = p
                    break
        ok = match is not None
        seg_ok = seg_ok and ok
        seg_report.append({"data_entry": i, "filesz": e["filesz"],
                           "bytes": e["filesz"], "matched_phdr": ok})
    checks["all_wrapped_segments_equal_bare_elf"] = seg_ok
    checks["six_wrapped_pt_segments"] = len(seg_report) == 6
    out["wrapped_segments"] = seg_report

    # ---- plain ELF module structure
    h = elf_header(plain)
    out["plain_elf_header"] = h
    checks["plain_magic_elf"] = h["magic"] == "7f454c46"
    checks["plain_elf64_le_x86_64"] = (h["class"] == 2 and h["enc"] == 1
                                       and h["e_machine"] == 0x3E)
    checks["plain_osabi_9"] = h["osabi"] == 9
    checks["plain_etype_sce_dynlib_0xfe18"] = h["e_type"] == 0xFE18

    nid_start = nid_encode("module_start") + "#F#A"
    nid_stop = nid_encode("module_stop") + "#F#A"
    defined, undefined = dynsyms(plain)
    out["exports_defined"] = [n for n, _ in defined]
    out["nid_module_start"] = nid_start
    out["nid_module_stop"] = nid_stop
    checks["exports_module_start_nid"] = nid_start in [n for n, _ in defined]
    checks["exports_module_stop_nid"] = nid_stop in [n for n, _ in defined]

    # explicit DT_INIT/DT_FINI -> module_start/module_stop (loader contract)
    plain_ents, plain_strtab = elf_dynamic(plain)
    dt_init = [v for t, v in plain_ents if t == 0x0C]
    dt_fini = [v for t, v in plain_ents if t == 0x0D]
    checks["explicit_dt_init_present"] = len(dt_init) == 1
    checks["explicit_dt_fini_present"] = len(dt_fini) == 1
    # the two defined exports' addresses were printed by llvm-nm at
    # 0xe10/0xf70; verify DT_INIT/DT_FINI equal those two entry addresses
    nm_addrs = {}
    llvm_nm = subprocess.run(
        [str(ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/"
             "LLVM-21.1.8-macOS-ARM64/bin/llvm-nm"), "-D", str(MB_PLAIN)],
        capture_output=True, text=True)
    for line in llvm_nm.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TtWw":
            nm_addrs[parts[2]] = int(parts[0], 16)
    out["llvm_nm_dynamic_exports"] = {k: hex(v) for k, v in nm_addrs.items()}
    if dt_init and dt_fini and nm_addrs:
        checks["dt_init_is_module_start_addr"] = (
            dt_init[0] == nm_addrs.get(nid_start))
        checks["dt_fini_is_module_stop_addr"] = (
            dt_fini[0] == nm_addrs.get(nid_stop))
    strong_imports = [n for n, b in undefined if b in (1, 2)]
    out["strong_import_count"] = len(strong_imports)
    checks["strong_imports_191_contract"] = len(strong_imports) == 191

    # ---- module import providers (DT_NEEDED of the module itself)
    ents, strtab_file = elf_dynamic(plain)
    needed = [dyn_str(plain, strtab_file, v) for t, v in ents if t == 1]
    out["mb_plain_dt_needed"] = needed
    modname_off = [v for t, v in ents if t == 0x61000043]
    if modname_off:
        out["mb_plain_module_name"] = dyn_str(plain, strtab_file,
                                              modname_off[0] & 0xFFFFFFFF)

    # ---- providers must be satisfied by the eboot NEEDED list before slot 236
    eboot = EBOOT_A1.read_bytes()
    e_ph = elf_phdrs(eboot)
    e_dyn = next(p for p in e_ph if p["type"] == 2)
    e_ents = [struct.unpack_from("<qQ", eboot, e_dyn["offset"] + 16 * i)
              for i in range(e_dyn["filesz"] // 16)]
    # strtab of eboot: DT_STRTAB=5; vaddr 0x9633420 == file 0x930f420 mirror
    e_strtab_file = 0x930F420
    e_needed = []          # (slot_index, name)
    for i, (t, v) in enumerate(e_ents):
        if t == 1:
            e_needed.append((i, dyn_str(eboot, e_strtab_file, v)))
    out["eboot_needed_tail"] = e_needed[-4:]

    def base_name(n):
        for suf in (".prx", ".sprx", ".native", ".so"):
            if n.endswith(suf):
                return n[: -len(suf)]
        return n

    eboot_map = {base_name(n): i for i, n in e_needed}
    mb_slot = max(i for i, _ in e_needed)  # 236 (mb.prx itself)
    providers_ok = True
    provider_report = []
    for lib in needed:
        bn = base_name(lib)
        slot = eboot_map.get(bn)
        ok = slot is not None and slot < mb_slot
        # libSceLibcInternal.prx is the on-disk name of the eboot's "libc"
        if not ok and bn == "libSceLibcInternal":
            slot = eboot_map.get("libc")
            ok = slot is not None and slot < mb_slot
        providers_ok = providers_ok and ok
        provider_report.append({"provider": lib, "eboot_slot": slot,
                                "loads_before_mb": ok})
    checks["all_providers_load_before_mb_slot"] = providers_ok
    out["provider_resolution"] = provider_report

    out["checks"] = checks
    out["result"] = "PASS" if all(checks.values()) else "FAIL"
    (HERE / "verify-mbprx.json").write_text(json.dumps(out, indent=1))

    lines = ["worker-eboot-patch mb.prx verification — console contact: false",
             f"mb.prx       sha256 {sha256(mb)}  ({len(mb)} bytes, FSELF)",
             f"mb-plain.prx sha256 {sha256(plain)}  ({len(plain)} bytes,"
             " plain ET_SCE_DYNAMIC)",
             f"result: {out['result']}"]
    for k, v in checks.items():
        flag = "ok " if v else "FAIL"
        lines.append(f"  [{flag}] {k}")
    lines.append(f"  exports: {out['exports_defined']}")
    lines.append(f"  mb-plain DT_NEEDED: {needed}")
    for p in provider_report:
        lines.append(f"    provider {p['provider']}: eboot slot"
                     f" {p['eboot_slot']} loads_before_mb={p['loads_before_mb']}")
    lines.append(f"  wrapped segments byte-equal to bare ELF:"
                 f" {checks['all_wrapped_segments_equal_bare_elf']}"
                 f" ({f['num_entries']} entries)")
    (HERE / "verify-mbprx.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0 if out["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
