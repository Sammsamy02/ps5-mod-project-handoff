#!/usr/bin/env python3
"""symbolize_crash.py — PHASE A item #5: PS5 crash-address symbolizer.

STATIC POST-MORTEM TOOL. Reads crash logs and build artifacts read-only on the
Mac. It never contacts the console. Using it changes nothing on any machine.
(Nothing runs on console without Claude review + user OK — project law; this
tool is deliberately usable entirely offline.)

What a PS5 homebrew crash actually leaves us (surveyed, see PHASEA-PLANS §5):
  * klog kernel crash blocks: signal, thread, proc, fault VA, ALL registers
    (incl. RIP), a (usually empty) backtrace, and the loaded-module list with
    each module's xotext base:end range + ELF build-id "fingerprint".
  * eboot-monitor transition logs (process start/stop only).
  * Sony error UI codes (CE-108255-1 = generic "game crashed"); no minidumps,
    no core files on retail/homebrew PS5.
  This tool turns those bytes into module+offset and, where our artifacts
  allow, function+offset with an explicit confidence tier.

Symbol sources, best first (all optional, combined when present):
  --elf UNSTRIPPED.ELF   full .symtab (EXACT) — future builds must emit one
                         (see design; today's Packager strips to .dynsym)
  --symbols-sidecar JSON packager-emitted PRX symbol sidecar: final vaddr,
                         size, name per section/symbol (SIDECAR-EXACT).
                         PAIRING LAW: the sidecar embeds the bare PRX's
                         sha256 (`prx_sha256`). When --prx is also given the
                         tool hashes that file and HARD-REFUSES (exit 2) on
                         mismatch — a sidecar from any other build would
                         mis-attribute every address. Without --prx the run
                         proceeds but is marked pairing=unverified.
  --prx BARE.prx         bare (pre-fake-sign) PRX: .dynsym anchors resolved
                         through DT_INIT/DT_FINI (EXACT for module_start/stop)
                         plus section headers (final .text/.rodata vaddrs)
  --map runtime.map      the `-r` partial-link map: per-function input
                         sections + sizes (RECONSTRUCTED, order-assumed —
                         flagged approximate until builds emit a final map)
  --game-symbols JSON    sidecar of known game-image vaddrs (e.g. Townfall
                         profile pins + UE4SS.log located addresses); nearest
                         match (SIDECAR tier; pairs with the GAME image by
                         its recorded image_sha256_prefix — the tool has no
                         --eboot input, so that pairing is reported, not
                         enforced)

Address input:
  --klog FILE            parse every crash block; symbolize RIP + fault addr
  --addr 0x.. --base 0x.. single address with explicit module base

The image-base law: klog's xotext start IS the live base (Townfall eboot
0x400000, proven). Game symbol vaddrs are image vaddrs (0-relative), so
runtime = base + image_vaddr.

Exit 0 = report produced (even "no symbols"); 2 = input error.
"""

import hashlib
import json
import re
import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------- ELF ------

def elf_sections(b):
    shoff = struct.unpack_from("<Q", b, 40)[0]
    shentsize = struct.unpack_from("<H", b, 58)[0]
    shnum = struct.unpack_from("<H", b, 60)[0]
    shstrndx = struct.unpack_from("<H", b, 62)[0]
    if shoff == 0 or shnum == 0:
        return []
    def sh(i):
        f = b[shoff + i * shentsize:shoff + (i + 1) * shentsize]
        return struct.unpack_from("<IIQQQQIIQQ", f)
    stroff = sh(shstrndx)[4]
    def nm(x):
        e = b.index(b"\0", stroff + x)
        return b[stroff + x:e].decode(errors="replace")
    out = []
    for i in range(shnum):
        n, typ, flags, addr, off, size, link, info, align, entsize = sh(i)
        out.append({"name": nm(n), "addr": addr, "off": off, "size": size,
                    "link": link, "entsize": entsize})
    return out


def elf_dynsym(b, sections):
    """Defined dynamic symbols + DT_INIT/DT_FINI anchors."""
    syms = []
    init = fini = None
    for s in sections:
        if s["name"] == ".dynsym":
            strtab = sections[s["link"]]
            cnt = s["size"] // 24
            for i in range(cnt):
                e = b[s["off"] + i * 24:s["off"] + (i + 1) * 24]
                nameo, info, other, shndx, value, sz = struct.unpack_from("<IBBHQQ", e)
                e2 = b.index(b"\0", strtab["off"] + nameo)
                nme = b[strtab["off"] + nameo:e2].decode(errors="replace")
                if shndx != 0:
                    syms.append({"name": nme, "addr": value, "size": sz})
        if s["name"] == ".dynamic":
            i = 0
            while i < s["size"]:
                tag, val = struct.unpack_from("<qQ", b, s["off"] + i)
                if tag == 0:
                    break
                if tag == 12:
                    init = val
                if tag == 13:
                    fini = val
                i += 16
    for s in syms:
        if init is not None and s["addr"] == init:
            s["name"] = s["name"] + " (=module_start/DT_INIT)"
        if fini is not None and s["addr"] == fini:
            s["name"] = s["name"] + " (=module_stop/DT_FINI)"
    return syms


def elf_symtab(b, sections):
    """Full .symtab if present (unstripped ELF)."""
    for s in sections:
        if s["name"] == ".symtab":
            strtab = sections[s["link"]]
            cnt = s["size"] // 24
            out = []
            for i in range(cnt):
                e = b[s["off"] + i * 24:s["off"] + (i + 1) * 24]
                nameo, info, other, shndx, value, sz = struct.unpack_from("<IBBHQQ", e)
                if value == 0:
                    continue
                e2 = b.index(b"\0", strtab["off"] + nameo)
                nme = b[strtab["off"] + nameo:e2].decode(errors="replace")
                out.append({"name": nme, "addr": value, "size": sz,
                            "type": info & 0xf})
            return out
    return []


# ---------------------------------------------------------------- map ------

def parse_map(path):
    """Parse an ld/lld `-Map` file. Handles BOTH shapes:
      * final-link maps: rows carry real VMAs -> exact symbol addresses
      * `-r` partial-link maps (what today's builds emit): VMA==0 everywhere;
        we keep the per-section catalog (input sections ~ one function each
        with -ffunction-sections-style names) + symbols, for reconstruction.
    Fixed columns (tokens END at cols 16/33/42/48; int() tolerates the
    surrounding spaces): [0:16) VMA, [16:33) LMA, [33:42) Size, [42:48) Align,
    [48:] name."""
    sections = []       # ordered [{name,size,symbols:[{name,size}]}]
    cur = None
    nonzero_vma_rows = 0
    for line in Path(path).read_text(errors="replace").splitlines():
        if len(line) < 48 or not line[:16].strip():
            continue
        try:
            vma = int(line[0:16], 16)
            lma = int(line[16:33], 16)
            size = int(line[33:42], 16)
        except ValueError:
            continue
        if vma:
            nonzero_vma_rows += 1
        name = line[48:].strip()
        if not name:
            continue
        if ":(." in name or name.startswith("/") or name == "<internal>:(*)":
            continue                       # object-member provenance row
        if name.startswith(".") or name.startswith(".rela"):
            cur = {"name": name, "vma": vma, "size": size,
                   "symbols": []}
            sections.append(cur)
            continue
        if cur is not None:
            cur["symbols"].append({"name": name, "vma": vma, "size": size})
    # A final-link map carries real VMAs on (nearly) every section row; a
    # `-r` partial-link map is all-zero (except stray rows). Require 5%.
    has_real = sections and nonzero_vma_rows >= max(1, len(sections) // 20)
    return {"sections": sections, "has_real_vmas": has_real,
            "nonzero_vma_rows": nonzero_vma_rows}


def text_class(name):
    for pfx in (".text", ".rodata", ".data.rel.ro", ".data", ".bss"):
        if name.startswith(pfx):
            return pfx
    return None


def reconstruct_from_map(mp, prx_sections):
    """Order-assumed reconstruction for `-r` maps: place map input sections
    cumulatively at the final section vaddr taken from the PRX section table.
    Confidence: APPROXIMATE until builds emit a final-link map."""
    out = []
    if mp["has_real_vmas"]:
        for s in mp["sections"]:
            for sym in s["symbols"]:
                if sym["vma"]:
                    out.append({"name": sym["name"], "addr": sym["vma"],
                                "size": sym["size"]})
            if s["vma"] and not s["symbols"]:
                out.append({"name": "<section> " + s["name"], "addr": s["vma"],
                            "size": s["size"]})
        return out, "MAP-EXACT"
    final_by_class = {}
    for s in prx_sections:
        c = text_class(s["name"])
        if c and s["addr"]:
            final_by_class.setdefault(c, s["addr"])
    cursor = dict(final_by_class)
    for s in mp["sections"]:
        c = text_class(s["name"])
        if c not in cursor:
            continue
        addr = cursor[c]
        cursor[c] = addr + s["size"]
        for sym in s["symbols"]:
            out.append({"name": sym["name"], "addr": addr + sym["vma"],
                        "size": sym["size"]})
        if not s["symbols"]:
            out.append({"name": "<section> " + s["name"], "addr": addr,
                        "size": s["size"]})
    return out, "MAP-RECONSTRUCTED"


# ------------------------------------------------------------- klog --------

REG_RE = re.compile(r"^#\s+([a-z0-9]+):\s+([0-9a-f]+)\s+(.*)$")
XOTEXT_RE = re.compile(r"xotext:\s+([0-9a-f]+):([0-9a-f]+)\s+nsegs:\s+(\d+)")
FINGERPRINT_RE = re.compile(r"fingerprint:\s+([0-9a-f]+)")


def parse_klog_crashes(path):
    """Extract kernel crash blocks: registers + module list. Handles the raw
    interleaved format (kernel noise lines mixed into the '#'-comment dump)."""
    text = Path(path).read_text(errors="replace")
    blocks = []
    cur = None
    pending_path = None
    for line in text.splitlines():
        if line.strip() == "#" or line.startswith("# "):
            body = line[1:].strip()
            if body.startswith("signal:") and (
                    "SIGSEGV" in body or "SIGILL" in body
                    or "SIGABRT" in body or "SIGBUS" in body):
                cur = {"signal": body.split(":", 1)[1].strip(),
                       "registers": {}, "modules": [], "notes": []}
                blocks.append(cur)
                continue
            if cur is None:
                continue
            m = REG_RE.match(line)
            if m:
                cur["registers"][m.group(1)] = int(m.group(2), 16)
                continue
            if body.startswith("/") and " " not in body:
                # module path line comes BEFORE its xotext/fingerprint pair
                pending_path = body
                continue
            m = XOTEXT_RE.search(body)
            if m:
                mod = {"start": int(m.group(1), 16),
                       "end": int(m.group(2), 16)}
                if pending_path:
                    mod["path"] = pending_path
                    pending_path = None
                cur["modules"].append(mod)
                continue
            m = FINGERPRINT_RE.search(body)
            if m and cur["modules"]:
                cur["modules"][-1]["fingerprint"] = m.group(1)
                continue
            if not body:
                continue
            key, _, val = body.partition(":")
            cur["notes"].append({key.strip(): val.strip()})
    for b in blocks:
        b["rip"] = b["registers"].get("rip")
        fa = next((n for n in b["notes"] if "fault address" in n), None)
        b["fault_address"] = None
        for n in b["notes"]:
            for k, v in n.items():
                if "fault address" in k:
                    try:
                        b["fault_address"] = int(v.strip().split()[0], 16)
                    except (ValueError, IndexError):
                        pass
    return blocks


# ------------------------------------------------------------- sidecar ------

class PairingRefused(Exception):
    """Hard refusal: PRX/sidecar pairing failed (wrong build or unpairable)."""


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_prx_symbols_sidecar(path, prx_path):
    """Load a packager-emitted PRX symbols sidecar and enforce PAIRING.

    The sidecar embeds the sha256 of the exact bare PRX it was emitted beside
    (``prx_sha256``). If the user also points --prx at a file, that file must
    hash to the same value — a mismatch means the sidecar belongs to a
    different build and every vaddr in it would mis-attribute crash
    addresses: hard refusal (PairingRefused). A sidecar with no valid
    prx_sha256 is equally refused (it cannot be paired at all). Without
    --prx there is nothing to check against: the load proceeds and the
    output is marked pairing=unverified (never silently trusted)."""
    sd = json.loads(Path(path).read_text())
    declared = sd.get("prx_sha256")
    if not isinstance(declared, str) or \
            not re.fullmatch(r"[0-9a-fA-F]{64}", declared):
        raise PairingRefused(
            "symbols sidecar %s carries no valid prx_sha256 — a "
            "packaging-time sidecar MUST embed the sha256 of the PRX it "
            "describes; refusing to symbolize against an unpairable sidecar"
            % path)
    info = {"path": str(path), "prx_sha256": declared.lower()}
    if prx_path:
        actual = sha256_file(prx_path)
        if actual != declared.lower():
            raise PairingRefused(
                "PRX/sidecar PAIRING MISMATCH: sidecar %s was emitted for "
                "prx_sha256 %s, but --prx %s hashes to %s — that is another "
                "build's PRX. Symbolizing against a mismatched sidecar would "
                "attribute crash addresses to stale vaddrs. REFUSING. Use "
                "the sidecar packaged beside THIS PRX, or point --prx at the "
                "matching build." % (path, declared.lower(), prx_path, actual))
        info["pairing"] = "verified"
    else:
        info["pairing"] = "unverified (--prx not given; cannot hash-check)"
    raw = sd.get("symbols")
    syms = []

    def as_int(x):
        return int(x, 16) if isinstance(x, str) else int(x)

    if isinstance(raw, list):          # [{"name":..,"addr":..,"size":..}]
        for ent in raw:
            sz = ent.get("size")
            syms.append({"name": str(ent["name"]),
                         "addr": as_int(ent["addr"]),
                         "size": None if sz is None else as_int(sz)})
    elif isinstance(raw, dict):        # {name: {"v":..,"size":..}} shape
        for name, ent in raw.items():
            v = ent["v"] if isinstance(ent, dict) else ent
            sz = ent.get("size") if isinstance(ent, dict) else None
            syms.append({"name": str(name), "addr": as_int(v),
                         "size": None if sz is None else as_int(sz)})
    syms.sort(key=lambda s: s["addr"])
    return syms, info


# --------------------------------------------------------- symbolize -------

def nearest(offset, syms):
    best = None
    for s in syms:
        if s["addr"] <= offset:
            if best is None or s["addr"] > best["addr"]:
                best = s
    if best is None:
        return None
    delta = offset - best["addr"]
    inside = best.get("size") and delta < best["size"]
    return {"symbol": best["name"], "symbol_vaddr": best["addr"],
            "offset_in_symbol": delta,
            "inside_symbol_bounds": bool(inside)}


def symbolize_addr(addr, modules, sources):
    owner = None
    for m in modules:
        if m["start"] <= addr < m["end"]:
            owner = m
            break
    res = {"address": "0x%x" % addr}
    if owner is None:
        res["module"] = None
        res["note"] = "address not inside any listed module xotext range"
        return res
    base = owner["start"]
    offset = addr - base
    res.update({"module": owner.get("path", "module@0x%x" % base),
                "module_base": "0x%x" % base,
                "module_end": "0x%x" % owner["end"],
                "module_offset": "0x%x" % offset})
    if owner.get("fingerprint"):
        res["module_fingerprint"] = owner["fingerprint"]
    for tier, syms in sources:
        hit = nearest(offset, syms)
        if hit:
            hit["tier"] = tier
            res.setdefault("symbols", []).append(hit)
    return res


def main(argv):
    args = argv[1:]
    json_out = "--json" in args
    args = [a for a in args if a != "--json"]

    def opt(flag, cast=str):
        if flag in args:
            i = args.index(flag)
            v = cast(args[i + 1])
            del args[i:i + 2]
            return v
        return None

    elf_p = opt("--elf")
    prx_p = opt("--prx")
    sidecar_p = opt("--symbols-sidecar")
    map_p = opt("--map")
    game_p = opt("--game-symbols")
    klog_p = opt("--klog")
    addr = opt("--addr", lambda s: int(s, 16))
    base = opt("--base", lambda s: int(s, 16))

    sources = []
    sidecar_info = None
    game_info = None
    try:
        if elf_p:
            b = Path(elf_p).read_bytes()
            secs = elf_sections(b)
            syms = elf_symtab(b, secs)
            if syms:
                sources.append(("SYMTEXACT-ELF", syms))
        if sidecar_p:
            syms, sidecar_info = load_prx_symbols_sidecar(sidecar_p, prx_p)
            if syms:
                sources.append(("SIDECAR-EXACT", syms))
        prx_secs = []
        if prx_p:
            b = Path(prx_p).read_bytes()
            prx_secs = elf_sections(b)
            dsyms = elf_dynsym(b, prx_secs)
            if dsyms:
                sources.append(("DYNSYM-EXACT", dsyms))
        if map_p:
            mp = parse_map(map_p)
            if prx_secs:
                syms, tier = reconstruct_from_map(mp, prx_secs)
                if syms:
                    sources.append((tier, syms))
            elif mp["has_real_vmas"]:
                syms, _t = reconstruct_from_map(mp, [])
                if syms:
                    sources.append(("MAP-EXACT", syms))
        if game_p:
            gd = json.loads(Path(game_p).read_text())
            game_info = {"path": str(game_p),
                         "image_sha256_prefix": gd.get("image_sha256_prefix"),
                         "note": ("pairs with the GAME image by recorded "
                                  "prefix; reported, not enforced (no "
                                  "--eboot input)")}
            game_base = int(gd.get("live_base", "0x400000"), 16)
            excluded = set(gd.get("excluded_from_matching", []))
            syms = []
            for name, ent in gd["symbols"].items():
                if name in excluded:
                    continue
                v = ent["v"] if isinstance(ent, dict) else ent
                space = ent.get("space", "image") if isinstance(ent, dict) else "image"
                symaddr = int(v, 16)
                if space == "runtime":
                    symaddr -= game_base  # normalize to module-offset space
                if symaddr < 0:
                    continue
                syms.append({"name": "%s [%s]" % (name, space),
                             "addr": symaddr, "size": None})
            if syms:
                sources.append(("GAME-SIDECAR", syms))
    except PairingRefused as e:
        print("input error: %s" % e, file=sys.stderr)
        return 2
    except (OSError, ValueError, struct.error, json.JSONDecodeError) as e:
        print("input error: %s" % e, file=sys.stderr)
        return 2

    out = {"tool": "symbolize_crash.py (phasea #5)", "sources":
           [{"tier": t, "symbols": len(s)} for t, s in sources]}
    if sidecar_info is not None:
        out["prx_symbols_sidecar"] = sidecar_info
    if game_info is not None:
        out["game_symbols_sidecar"] = game_info
    if klog_p:
        blocks = parse_klog_crashes(klog_p)
        out["klog"] = str(klog_p)
        out["crash_blocks"] = []
        for blk in blocks:
            entry = {"signal": blk["signal"],
                     "rip": "0x%x" % blk["rip"] if blk["rip"] else None,
                     "fault_address": ("0x%x" % blk["fault_address"]
                                       if blk["fault_address"] is not None else None),
                     "modules": [{"path": m.get("path"),
                                  "base": "0x%x" % m["start"],
                                  "end": "0x%x" % m["end"],
                                  "fingerprint": m.get("fingerprint")}
                                 for m in blk["modules"]]}
            targets = [blk["rip"]] if blk["rip"] else []
            if blk["fault_address"] is not None and \
                    blk["fault_address"] != blk["rip"]:
                targets.append(blk["fault_address"])
            entry["symbolized"] = [symbolize_addr(a, blk["modules"], sources)
                                   for a in targets if a]
            out["crash_blocks"].append(entry)
    elif addr is not None:
        mods = []
        if base is not None:
            # single-module synthetic mode: one module spanning a 256MB window
            mods = [{"start": base, "end": base + 0x10000000,
                     "path": "synthetic-module@base"}]
        out["single"] = symbolize_addr(addr, mods, sources)
    else:
        print(__doc__)
        return 2

    if json_out:
        print(json.dumps(out, indent=1))
    else:
        print(json.dumps(out, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
