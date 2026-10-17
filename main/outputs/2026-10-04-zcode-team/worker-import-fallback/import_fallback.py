#!/usr/bin/env python3
"""import_fallback.py -- make the composed runtime's additional imports non-fatal
by rewriting their .dynsym bind type from GLOBAL (strong) to WEAK.

Motivation (see FALLBACK-DESIGN.md):
  The frozen, console-proven aligned-entry PRX carries a 191-strong-import
  contract PLUS six already-weak undefined symbols ("__dladdr", "__dlclose",
  "__dlerror", "__dlopen", "__dlsym", "kernel_mprotect") and loads in Townfall
  every time. The composed runtime extends the contract by up to seven strong
  imports (sceKernelDlsym, sceKernelGetModuleInfo, sceKernelLoadStartModule,
  sceKernelStopUnloadModule, qsort, possibly clock_gettime and getpid); the
  198-import candidate loads but produces no output. Demoting the extension
  symbols to STB_WEAK keeps the loader's failure mode "bind 0, continue"
  instead of a load-time abort, and the runtime code guards every call site
  (see optional_imports.h).

Mechanics:
  ELF64 little-endian only (PS5 x86_64). Import names in .dynstr are the
  Packager's long form "<nid>#<library>#<module>" where <nid> is
  SceNid.Compute(name) = base64(SHA1(name + NID_SUFFIX)[:8][::-1])[:11]
  with '/' mapped to '-'. This tool matches friendly names by computing the
  NID (mirroring worker-prx-link-gaps' verified sce_nid, ground truth
  malloc->gQX+4GDQjpM, module_start->BaOKcng8g88) or accepts raw
  dynstr text with --raw.

Fail-closed contract:
  * exactly one SHT_DYNSYM section expected;
  * every requested name must appear as an UNDEFINED dynsym entry
    (shndx == 0), otherwise abort unless --allow-missing;
  * only the single st_info byte of matching entries is modified
    (GLOBAL 0x1? -> WEAK 0x2?, type nibble preserved); every other output
    byte is verified identical to the input;
  * already-WEAK matches are reported, not errors (idempotent rerun);
  * a requested name that is DEFINED (shndx != 0) is a hard error.

Usage:
  python3 import_fallback.py audit  --elf FILE [--map name=nid ...] [--raw]
  python3 import_fallback.py weakify --elf IN --out OUT
        [--names n1,n2,...] [--allow-missing] [--report report.json] [--raw]

No network, no subprocesses; pure stdlib.
"""
import argparse
import base64
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

# The five measured extensions of the composed runtime (worker-prx-packaging
# REPORT: contract grows 191 -> 196). clock_gettime/getpid are context-flagged
# "possibly present" in composition-4 (198 imports); add them via --names when
# an audit of the actual candidate proves them.
DEFAULT_TARGETS = [
    "sceKernelDlsym",
    "sceKernelGetModuleInfo",
    "sceKernelLoadStartModule",
    "sceKernelStopUnloadModule",
    "qsort",
]

# SharpProspero.Prx SceNid.Compute suffix (verified ground truth pairs).
NID_SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
NID_LONG_RE = re.compile(r"^(.{11})#(.+)#(.+)$")

STB_GLOBAL = 1
STB_WEAK = 2
SHN_UNDEF = 0
SHT_DYNSYM = 11
EHDR = struct.Struct("<16sHHIQQQIHHHHHH")
SHDR = struct.Struct("<IIQQQQIIQQ")
SYM = struct.Struct("<IBBHQQ")


def sce_nid(name: str) -> str:
    digest = hashlib.sha1(name.encode() + NID_SUFFIX).digest()
    return base64.b64encode(digest[:8][::-1]).decode()[:11].replace("/", "-")


class ElfError(Exception):
    pass


def load_sections(data: bytes):
    if len(data) < EHDR.size:
        raise ElfError("truncated ELF header")
    ident = data[:16]
    if ident[:4] != b"\x7fELF":
        raise ElfError("not an ELF file")
    if ident[4] != 2 or ident[5] != 1:
        raise ElfError("expected ELF64 little-endian")
    (_, e_type, e_machine, _, _, _, e_shoff, _, _, _, _,
     e_shentsize, e_shnum, e_shstrndx) = EHDR.unpack_from(data, 0)
    if e_shentsize != SHDR.size:
        raise ElfError(f"unexpected shentsize {e_shentsize}")
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * SHDR.size
        if off + SHDR.size > len(data):
            raise ElfError("section header table truncated")
        (sh_name, sh_type, _flags, _addr, sh_offset, sh_size,
         sh_link, sh_info, _align, sh_entsize) = SHDR.unpack_from(data, off)
        sections.append({"index": i, "name_off": sh_name, "type": sh_type,
                         "offset": sh_offset, "size": sh_size,
                         "link": sh_link, "info": sh_info,
                         "entsize": sh_entsize})
    return sections


def find_dynsym(sections):
    hits = [s for s in sections if s["type"] == SHT_DYNSYM]
    if not hits:
        raise ElfError("no SHT_DYNSYM section")
    if len(hits) != 1:
        raise ElfError(f"expected exactly one SHT_DYNSYM, found {len(hits)}")
    dynsym = hits[0]
    if dynsym["entsize"] != SYM.size:
        raise ElfError(f"unexpected dynsym entsize {dynsym['entsize']}")
    return dynsym


def dynstr_of(data: bytes, sections, dynsym):
    if dynsym["link"] >= len(sections):
        raise ElfError("dynsym sh_link out of range")
    strs = sections[dynsym["link"]]
    return data[strs["offset"]:strs["offset"] + strs["size"]]


def cstr(blob: bytes, off: int) -> str:
    end = blob.find(b"\x00", off)
    if end < 0:
        raise ElfError("unterminated dynstr entry")
    return blob[off:end].decode("utf-8", "replace")


def decode_name(text: str, raw: bool) -> str:
    """Long form '<nid>#<lib>#<mod>' -> '<nid>'; raw text otherwise."""
    if raw:
        return text
    m = NID_LONG_RE.match(text)
    return m.group(1) if m else text


def audit(data: bytes, extra_map, raw: bool):
    sections = load_sections(data)
    dynsym = find_dynsym(sections)
    strs = dynstr_of(data, sections, dynsym)
    nid_to_name = {sce_nid(n): n for n in DEFAULT_TARGETS}
    for n, nid in extra_map.items():
        nid_to_name[nid] = n
    imports = []
    for off in range(dynsym["offset"], dynsym["offset"] + dynsym["size"],
                     SYM.size):
        st_name, st_info, _other, st_shndx, _value, _size = SYM.unpack_from(
            data, off)
        if st_shndx != SHN_UNDEF or st_name == 0:
            continue
        bind = st_info >> 4
        if bind not in (STB_GLOBAL, STB_WEAK):
            continue
        text = cstr(strs, st_name)
        ident = decode_name(text, raw)
        imports.append({"dynsym_offset": off, "dynstr": text,
                        "identity": ident, "bind": ("GLOBAL" if bind ==
                                                    STB_GLOBAL else "WEAK"),
                        "friendly": nid_to_name.get(ident)})
    return imports


def weakify(data: bytes, targets, extra_map, raw: bool, allow_missing: bool):
    sections = load_sections(data)
    dynsym = find_dynsym(sections)
    strs = dynstr_of(data, sections, dynsym)
    out = bytearray(data)
    patched, already_weak, missing, defined = [], [], [], []
    want = {}
    for name in targets:
        want[sce_nid(name) if not raw else name] = name
    for nid, name in extra_map.items():
        want[nid] = name
    matched = set()
    for off in range(dynsym["offset"], dynsym["offset"] + dynsym["size"],
                     SYM.size):
        st_name, st_info, _other, st_shndx, _value, _size = SYM.unpack_from(
            data, off)
        if st_name == 0:
            continue
        text = cstr(strs, st_name)
        ident = decode_name(text, raw)
        name = want.get(ident)
        if name is None:
            continue
        bind = st_info >> 4
        if st_shndx != SHN_UNDEF:
            defined.append({"name": name, "shndx": st_shndx})
            continue
        if bind == STB_WEAK:
            already_weak.append(name)
            matched.add(name)
            continue
        if bind != STB_GLOBAL:
            missing.append({"name": name, "detail": f"bind {bind}"})
            continue
        # the single patched byte: GLOBAL nibble -> WEAK nibble, type kept
        out[off + 4] = (st_info & 0x0F) | (STB_WEAK << 4)
        patched.append({"name": name, "identity": ident,
                        "dynsym_offset": off,
                        "st_info": f"0x{st_info:02x} -> "
                                   f"0x{out[off + 4]:02x}"})
        matched.add(name)
    for name in want.values():
        if name not in matched and name not in [d["name"] for d in defined]:
            missing.append({"name": name,
                            "detail": "no UNDEF dynsym entry"})
    if defined:
        raise ElfError(f"requested symbols are DEFINED, not imports: "
                       f"{defined}")
    if missing and not allow_missing:
        raise ElfError(f"missing requested imports: {missing}")
    changed = [i for i in range(len(data)) if data[i] != out[i]]
    for i in changed:
        if not any(i == p["dynsym_offset"] + 4 for p in patched):
            raise ElfError(f"internal: byte {i} changed outside patched "
                           f"st_info fields")
    return {"patched": patched, "already_weak": sorted(already_weak),
            "missing": missing,
            "input_sha256": hashlib.sha256(data).hexdigest(),
            "output_sha256": hashlib.sha256(out).hexdigest(),
            "bytes_changed": len(changed)}, bytes(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("audit")
    a.add_argument("--elf", required=True)
    a.add_argument("--map", action="append", default=[],
                   help="friendly=nid identity hint, repeatable")
    a.add_argument("--raw", action="store_true",
                   help="dynstr holds raw names, not NID long form")
    w = sub.add_parser("weakify")
    w.add_argument("--elf", required=True)
    w.add_argument("--out", required=True)
    w.add_argument("--names", default=",".join(DEFAULT_TARGETS))
    w.add_argument("--map", action="append", default=[])
    w.add_argument("--raw", action="store_true")
    w.add_argument("--allow-missing", action="store_true")
    w.add_argument("--report")
    args = ap.parse_args(argv)
    extra_map = {}
    for item in args.map:
        if "=" not in item:
            ap.error(f"--map expects name=nid, got {item!r}")
        n, nid = item.split("=", 1)
        extra_map[n] = nid
    data = Path(args.elf).read_bytes()
    if args.cmd == "audit":
        print(json.dumps(audit(data, extra_map, args.raw), indent=2))
        return 0
    targets = [t for t in args.names.split(",") if t]
    try:
        report, out = weakify(data, targets, extra_map, args.raw,
                              args.allow_missing)
    except ElfError as exc:
        print(f"FAIL-CLOSED: {exc}", file=sys.stderr)
        return 1
    Path(args.out).write_bytes(out)
    report["requested"] = targets
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
