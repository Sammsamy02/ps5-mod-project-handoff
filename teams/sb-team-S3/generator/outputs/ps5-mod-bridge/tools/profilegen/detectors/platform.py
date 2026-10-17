"""Binding, ELF and load facts: everything that comes from the container, not from game code.

Detectors in this module (all engine-independent):

* ``binding.*``   : title id / content version / ids from ``param.json``; sha256 + size of both inputs.
* ``elf.*``       : header fields, the program-header table, RELA counts, note/dynamic locations.
* ``load.*``      : runtime load base (platform convention, see below) and the image extent.
* ``build.build_id`` : the ELF ``NT_GNU_BUILD_ID`` note.
"""
from __future__ import annotations

import hashlib
from typing import Dict, List

from ..elf import (DT_RELA, DT_RELACOUNT, DT_RELASZ, ET_SCE_DYNEXEC, NT_GNU_BUILD_ID, PT_DYNAMIC, PT_LOAD,
                   PT_NOTE)
from ..model import HIGH, MEDIUM, Context, Hex, hex_str

# The Prospero (PS5) kernel maps the main executable at a platform-fixed base.  Nothing inside
# the image encodes it (the image is linked at vaddr 0 and every relocation is image-relative),
# so it is a platform ABI constant, written here as a size (4 MiB) rather than a magic literal.
# ``--load-base`` overrides it when a console observation exists.
PS5_MAIN_MODULE_BASE = 4 * 1024 * 1024

_FLAG_TEXT = {0: "---", 1: "--X", 2: "-W-", 3: "-WX", 4: "R--", 5: "R-X", 6: "RW-", 7: "RWX"}
_PT_NAMES = {0: "NULL", 1: "LOAD", 2: "DYNAMIC", 4: "NOTE", 7: "TLS", 0x6474E550: "GNU_EH_FRAME",
             0x6474E551: "GNU_STACK", 0x6474E552: "GNU_RELRO", 0x61000001: "SCE_PROCPARAM"}


def _sha256_of(buf) -> str:
    h = hashlib.sha256()
    n = len(buf)
    step = 8 << 20
    mv = memoryview(buf)
    for i in range(0, n, step):
        h.update(mv[i:i + step])
    return h.hexdigest()


def _binding(ctx: Context) -> None:
    p = ctx.param
    if not p:
        ctx.fail("binding.param_json", "no param.json supplied; title id / content version unknown", "more-analysis")
    else:
        for path, key in (("binding.title_id", "titleId"), ("binding.content_version", "contentVersion"),
                          ("binding.master_version", "masterVersion"), ("binding.content_id", "contentId")):
            if key in p and isinstance(p[key], str):
                ctx.put(path, p[key], HIGH, "param.json." + key, ["param.json key %s = %r" % (key, p[key])])
            else:
                ctx.fail(path, "param.json has no string key %s" % key)
        names = p.get("localizedParameters")
        default = names.get("defaultLanguage") if isinstance(names, dict) else None
        entry = names.get(default) if isinstance(names, dict) and isinstance(default, str) else None
        title = entry.get("titleName") if isinstance(entry, dict) else None
        if isinstance(title, str) and title:
            ctx.put("binding.title_name", title, HIGH, "param.json.localizedParameters",
                    ["localizedParameters[%s].titleName" % default])
        if isinstance(p.get("sdkVersion"), str):
            ctx.put("binding.sdk_version", p["sdkVersion"], HIGH, "param.json.sdkVersion",
                    ["param.json sdkVersion = %s" % p["sdkVersion"]])
    raw = ctx.options.get("param_bytes")
    if raw is not None:
        ctx.put("binding.param_json.sha256", hashlib.sha256(raw).hexdigest(), HIGH, "sha256",
                ["%d bytes hashed" % len(raw)])
        ctx.put("binding.param_json.bytes", Hex(len(raw)), HIGH, "file.size", ["param.json is %d bytes" % len(raw)])
    ctx.put("binding.executable.sha256", _sha256_of(ctx.elf.data), HIGH, "sha256",
            ["%d bytes hashed" % ctx.elf.size])
    ctx.put("binding.executable.bytes", Hex(ctx.elf.size), HIGH, "file.size", ["executable is %d bytes" % ctx.elf.size])


def _elf_header(ctx: Context) -> None:
    e = ctx.elf
    ev = lambda s: ["ELF header: " + s]
    ctx.put("elf.e_type", Hex(e.e_type), HIGH, "elf.header", ev("e_type 0x%x%s" % (
        e.e_type, " (SCE_DYNEXEC, PS5 main executable)" if e.e_type == ET_SCE_DYNEXEC else "")))
    ctx.put("elf.e_machine", Hex(e.e_machine), HIGH, "elf.header", ev("e_machine %d (x86-64)" % e.e_machine))
    ctx.put("elf.e_entry", Hex(e.e_entry), HIGH, "elf.header", ev("e_entry 0x%x" % e.e_entry))
    ctx.put("elf.e_phoff", Hex(e.e_phoff), HIGH, "elf.header", ev("e_phoff 0x%x" % e.e_phoff))
    ctx.put("elf.e_phentsize", Hex(e.e_phentsize), HIGH, "elf.header", ev("e_phentsize %d" % e.e_phentsize))
    ctx.put("elf.e_phnum", Hex(e.e_phnum), HIGH, "elf.header", ev("e_phnum %d" % e.e_phnum))
    ctx.put("elf.osabi", Hex(e.osabi), HIGH, "elf.header", ev("EI_OSABI %d (9 = FreeBSD, the PS5 userland ABI)" % e.osabi))
    sh_end = e.e_shoff + e.e_shnum * e.e_shentsize
    present = e.e_shoff != 0 and e.e_shnum != 0 and sh_end <= e.size
    ctx.put("elf.section_table_present", present, HIGH, "elf.header",
            ev("e_shoff 0x%x e_shnum %d; table end 0x%x vs file size 0x%x" % (e.e_shoff, e.e_shnum, sh_end, e.size)))
    ctx.put("elf.pointer_size", Hex(8), HIGH, "elf.class", ev("EI_CLASS = ELFCLASS64"))


def _segments(ctx: Context) -> None:
    e = ctx.elf
    for s in e.segments:
        text = {
            "type": _PT_NAMES.get(s.p_type, "0x%x" % s.p_type),
            "p_type": hex_str(s.p_type), "p_flags": hex_str(s.flags), "flags": _FLAG_TEXT.get(s.flags & 7, "?"),
            "p_offset": hex_str(s.offset), "p_vaddr": hex_str(s.vaddr), "p_filesz": hex_str(s.filesz),
            "p_memsz": hex_str(s.memsz), "p_align": hex_str(s.align),
        }
        if s.p_type == PT_LOAD:
            text["file_delta"] = hex_str(s.offset - s.vaddr)
        ctx.put("elf.segments.ph%d" % s.index, text, HIGH, "elf.phdr",
                ["program header %d: %s %s off=0x%x va=0x%x filesz=0x%x memsz=0x%x" % (
                    s.index, text["type"], text["flags"], s.offset, s.vaddr, s.filesz, s.memsz)])
    groups: Dict[int, List[int]] = {}
    for s in e.loads:
        groups.setdefault(s.offset - s.vaddr, []).append(s.index)
    parts = []
    for delta, idxs in sorted(groups.items(), key=lambda kv: kv[1][0]):
        sign = "+" if delta >= 0 else "-"
        parts.append("%s0x%x for %s" % (sign, abs(delta), ",".join("ph%d" % i for i in idxs)))
    ctx.put("load.file_offset_rule",
            "file = p_offset + (vaddr - p_vaddr) per PT_LOAD; delta " + "; ".join(parts), HIGH, "elf.phdr",
            ["per-segment delta computed from PT_LOAD p_offset - p_vaddr; never seek a vaddr directly"])
    dyn = [s for s in e.segments if s.p_type == PT_DYNAMIC]
    if dyn:
        ctx.put("elf.dynamic.label", Hex(dyn[0].vaddr), HIGH, "elf.phdr", ["PT_DYNAMIC p_vaddr 0x%x" % dyn[0].vaddr])
    notes = [s for s in e.segments if s.p_type == PT_NOTE and s.filesz]
    ctx.facts["note_segments"] = [s.index for s in notes]


def _relocs(ctx: Context) -> None:
    e = ctx.elf
    dyn = e.dynamic()
    if DT_RELA in dyn:
        ctx.put("elf.dt_rela.label", Hex(dyn[DT_RELA]), HIGH, "elf.dynamic", ["DT_RELA = 0x%x" % dyn[DT_RELA]])
    size = dyn.get(DT_RELASZ)
    if size is not None:
        ctx.put("elf.rela.size_bytes", Hex(size), HIGH, "elf.dynamic", ["DT_RELASZ = 0x%x" % size])
        ctx.put("elf.rela.entries", Hex(size // 24), HIGH, "elf.dynamic", ["DT_RELASZ / 24 = %d" % (size // 24)])
    offs, adds = e.relative_relocs()
    if len(offs):
        extent = e.extent
        inside = sum(1 for a in adds[:: max(1, len(adds) // 5000)] if 0 <= a < extent)
        sampled = len(adds[:: max(1, len(adds) // 5000)])
        ctx.put("elf.rela.relative_entries", Hex(len(offs)), HIGH, "elf.rela",
                ["%d R_X86_64_RELATIVE entries parsed from the RELA table" % len(offs),
                 "DT_RELACOUNT = %s" % (hex(dyn[DT_RELACOUNT]) if DT_RELACOUNT in dyn else "absent"),
                 "%d/%d sampled addends fall inside the linked image extent (image-relative addends)" % (inside, sampled)])
    ctx.facts["relative_relocs"] = (len(offs),)


def _load_base(ctx: Context) -> None:
    e = ctx.elf
    first_load = min(e.loads, key=lambda s: s.vaddr) if e.loads else None
    override = ctx.options.get("load_base")
    ev: List[str] = []
    if first_load is not None:
        ev.append("lowest PT_LOAD p_vaddr = 0x%x (image is linked at that address, so the loader must add a base)"
                  % first_load.vaddr)
    ev.append("e_type 0x%x = %s" % (e.e_type, "SCE_DYNEXEC (PS5 main executable)" if e.e_type == ET_SCE_DYNEXEC else "not a PS5 executable"))
    n = ctx.facts.get("relative_relocs", (0,))[0]
    if n:
        ev.append("%d image-relative RELATIVE relocations: no absolute address in the file fixes the base" % n)
    if override is not None:
        ev.append("supplied on the command line (e.g. read from a console memory map)")
        ctx.put("load.base", Hex(override), HIGH, "load.cli_override", ev)
    elif e.is_ps5_dynexec:
        ev.append("PS5 platform convention: the kernel maps the main executable at the SDK-fixed 4 MiB base; "
                  "this cannot be observed from the file, only predicted")
        ctx.put("load.base", Hex(PS5_MAIN_MODULE_BASE), MEDIUM, "load.platform_convention", ev)
    else:
        ctx.fail("load.base", "not a PS5 SCE_DYNEXEC image; no platform base convention applies", "more-analysis")
        return
    base = ctx.load_base
    ctx.put("load.image_extent.vaddr_end", Hex(e.extent), HIGH, "elf.phdr",
            ["max(p_vaddr + p_memsz) over PT_LOAD = 0x%x" % e.extent])
    ctx.put("load.image_extent.start", Hex(base), ctx.get("load.base").confidence, "load.base",
            ["= load.base"])
    ctx.put("load.image_extent.end", Hex(base + e.extent), ctx.get("load.base").confidence, "load.base + extent",
            ["load.base + max(p_vaddr + p_memsz)"])


def _build_id(ctx: Context) -> None:
    e = ctx.elf
    notes = [n for n in e.notes() if n.n_type == NT_GNU_BUILD_ID and n.name == "GNU"]
    if not notes:
        ctx.fail("build.build_id", "no NT_GNU_BUILD_ID note in any PT_NOTE segment")
        return
    n = notes[0]
    desc = n.desc
    trimmed = desc
    padded = False
    if len(desc) == 20 and desc[16:] == b"\x00\x00\x00\x00":
        trimmed, padded = desc[:16], True       # Sony's 16-byte id padded to a SHA-1 sized note
    note_seg = next((s for s in e.segments if s.index == n.segment_index), None)
    ev = ["PT_NOTE %d: NT_GNU_BUILD_ID descriptor (%d bytes) = %s" % (n.segment_index, len(desc), desc.hex())]
    if padded:
        ev.append("descriptor is a 16-byte id followed by 4 zero pad bytes; the pad is dropped")
    if note_seg is not None:
        ev.append("note segment at vaddr 0x%x" % note_seg.vaddr)
        ctx.put("elf.note.gnu_build_id.label", Hex(note_seg.vaddr), HIGH, "elf.phdr",
                ["PT_NOTE p_vaddr 0x%x holds the GNU build-id note" % note_seg.vaddr])
    ev.append("ELF link identity, not UE's own BUILD_ID: used as the opaque build tag")
    ctx.put("build.build_id", trimmed.hex(), HIGH, "elf.gnu_build_id", ev)


def procparam(ctx: Context) -> None:
    pp = ctx.elf.procparam()
    if pp is None:
        return
    raw = pp["sdk_version_raw"]
    ctx.put("elf.procparam.sdk_version", "0x%016x" % raw, HIGH, "elf.sce_procparam",
            ["PT_SCE_PROCPARAM 'ORBI' block at vaddr 0x%x, size %d, %d entries" % (pp["vaddr"], pp["size"], pp["entry_count"])])
    sdk = ctx.get("binding.sdk_version")
    if sdk is not None:
        try:
            agree = (int(sdk.value, 16) >> 48) == (raw >> 48)
        except (TypeError, ValueError):
            agree = False
        ctx.put("elf.procparam.sdk_matches_param_json", agree, HIGH, "compare",
                ["procparam sdk 0x%016x vs param.json sdkVersion %s (top 16 bits compared)" % (raw, sdk.value)])


def detect(ctx: Context) -> None:
    _binding(ctx)
    _elf_header(ctx)
    _segments(ctx)
    _relocs(ctx)
    _load_base(ctx)
    _build_id(ctx)
    procparam(ctx)
