#!/usr/bin/env python3
"""crashcap.py - PS5 crash report from a klog capture (cap05, Mac-only, read-only).

What the console already records for every crashed game process (PS5 kernel
fatal-signal dump, streamed by klogsrv and saved by the Mac klog tap that the
harvest already picks up):
  reason/signal, thread, process, fault address, ALL registers, last-branch
  records, a frame-pointer backtrace of return addresses, and the full loaded
  module list (path, xotext start:end, fingerprint = the module's GNU build-id).

This tool turns that into a plain report:
  * every address -> module + offset, using the block's OWN module list;
  * a module is BOUND to a local artifact only when the kernel fingerprint
    equals the artifact's GNU build-id note (no name matching, no guessing);
  * the address convention is CHECKED per bound module, not assumed:
    xotext start must be page aligned and xotext extent must equal the
    artifact's executable PT_LOAD memsz rounded up to its alignment; then
    vaddr = addr - xotext_start + exec_vaddr. A mismatch -> module+offset only;
  * bound runtime modules get exact function names from final_symbols.py
    output (byte-verified final layout) with in-bounds flags;
  * every backtrace return address in a module whose bytes we have is
    CALL-EDGE checked: the bytes just before it must be a call instruction;
    for a direct call the call target must be the start of the function that
    holds the next-inner frame (an independent second source for both the
    symbol table and the base convention);
  * last lines of the harvested runtime logs (NUL-interleaved logs decoded by
    the decode_session_log.py rule: drop NUL bytes).

No console contact. Inputs are never modified. Output paths must not exist.

Exit codes: 0 report written; 1 an --expect-* check failed; 2 input error;
3 nothing scanned (empty klog) - a scan of zero lines is a failed gate.
"""
import argparse
import bisect
import hashlib
import json
import os
import re
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import elf64  # noqa: E402

BANNER = "A user thread receives a fatal signal"
HEX16 = re.compile(r"^[0-9a-f]{16}$")
REG_PAIR = re.compile(r"\b([a-z][a-z0-9]{0,5})\s*:\s+([0-9a-f]{8,16})\b")
XOTEXT = re.compile(r"xotext:\s+([0-9a-f]+):([0-9a-f]+)\s+nsegs:\s+(\d+)")
FPRINT = re.compile(r"fingerprint:\s+([0-9a-f]+)")
FAULT_VA = re.compile(r"addr\(VA\):\s*(0x[0-9a-fA-F]+)")
FAULT_ADDR = re.compile(r"^fault address:\s+([0-9a-f]+)")
IPV4 = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")
LBR_NAMES = {"lbf", "lbt", "lb0", "lb1", "lb2", "lb3", "lb4", "lb5", "lb6", "lb7"}
MAX_NOISE_IN_LIBS = 400
# FreeBSD signal numbers (public sys/signal.h numbering; the PS5 kernel prints them)
SIGNAMES = {4: "SIGILL", 5: "SIGTRAP", 6: "SIGABRT", 8: "SIGFPE", 10: "SIGBUS", 11: "SIGSEGV"}


def sha256_path(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


_DECODER = None
DEFAULT_DECODER = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/decode_session_log.py"


def load_decoder(path):
    """Use the project's sanctioned decoder (harvest/decode_session_log.py) when
    present; it is imported read-only with bytecode writing disabled."""
    global _DECODER
    if not path or not os.path.isfile(path):
        return None
    import importlib.util
    spec = importlib.util.spec_from_file_location("decode_session_log_ro", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    _DECODER = mod
    return mod


def read_text_lines(path):
    if _DECODER is not None:
        decoded, _stats, _notes = _DECODER.decode_file(path)
        return decoded.decode("utf-8", "replace").splitlines()
    with open(path, "rb") as f:
        raw = f.read()
    # Fallback = the same DECODE RULE (decode_session_log.py): NUL bytes only
    # occur as wide-run high halves; dropping them reconstructs the text.
    for bom in (b"\xef\xbb\xbf",):
        if raw.startswith(bom):
            raw = raw[len(bom):]
    return raw.replace(b"\0", b"").decode("utf-8", "replace").splitlines()


# --------------------------------------------------------------- klog parse --
def parse_klog(lines):
    blocks = []
    cur = None
    section = None
    noise_since_lib = 0

    def close():
        nonlocal cur
        if cur is not None:
            # [cap05 launch 2026-10-13] On a DATA page fault the kernel's "fault address" line is the faulting
            # DATA address (console-proven: reason "page fault (user write data, page not present)", fault address
            # 0xc08, rip inside the planted function, rax 0xc08). Earlier retained blocks had fault address == rip
            # (instruction fetch). Two signals must agree before it counts as the data VA: the reason names a
            # data access AND the address differs from rip. Otherwise it stays the instruction address.
            fpc, rip = cur.get("fault_pc"), cur.get("registers", {}).get("rip")
            data_fault = any("page fault" in r and " data" in r for r in cur.get("reasons", []))
            if fpc is not None and cur.get("fault_va") is None and data_fault and rip is not None and fpc != rip:
                cur["fault_va"] = fpc
                cur["fault_va_source"] = "fault-address line (data page fault, differs from rip)"
                cur["fault_pc"] = None
            blocks.append(cur)
        cur = None

    for ln, line in enumerate(lines, 1):
        s = line.rstrip("\r")
        if cur is not None and not s.startswith("#") and section in ("regs", "bt", "libs"):
            # another writer's text glued in FRONT of a dump line on the same
            # physical line (seen in retained klogs: "[dce-ih:598 ERR]<15># rcx: ...")
            idx = s.find("# ")
            if idx > 0:
                s = s[idx:]
                cur["salvaged_lines"] += 1
        if not s.startswith("#"):
            if cur is not None:
                cur["noise_lines"] += 1
                if "has nonsleeping lock" in s:
                    cur["copyin_note"] = s.strip()[:120]
                if section in ("libs", "after_bt"):
                    noise_since_lib += 1
                    if noise_since_lib > MAX_NOISE_IN_LIBS:
                        close()
                        section = None
            continue
        body = s[1:].strip()
        if body == BANNER:
            close()
            cur = {"line": ln, "signal": None, "reasons": [], "thread_id": None, "thread_name": None,
                   "pid": None, "proc_name": None, "fault_va": None, "fault_pc": None, "fault_note": None,
                   "registers": {}, "lbr": {}, "backtrace": [], "modules": [], "noise_lines": 0, "salvaged_lines": 0,
                   "copyin_note": None, "end_line": ln}
            section = "head"
            noise_since_lib = 0
            continue
        if cur is None:
            continue
        cur["end_line"] = ln
        if body == "":
            if section == "bt":
                section = "after_bt"
                noise_since_lib = 0
            continue
        if body == "registers:":
            section = "regs"
            continue
        if body == "backtrace:":
            section = "bt"
            continue
        if body.startswith("Omitting module list"):
            cur["notes"] = cur.get("notes", []) + [body]
            close()
            section = None
            continue
        if body == "dynamic libraries:":
            section = "libs"
            noise_since_lib = 0
            continue
        if section == "bt" and HEX16.match(body):
            cur["backtrace"].append(int(body, 16))
            continue
        if section == "libs":
            if body.startswith("/") or body.startswith("<"):
                cur["modules"].append({"path": body, "start": None, "end": None, "nsegs": None,
                                       "fingerprint": None})
                noise_since_lib = 0
                continue
            m = XOTEXT.search(body)
            if m and cur["modules"]:
                mod = cur["modules"][-1]
                mod["start"], mod["end"], mod["nsegs"] = int(m.group(1), 16), int(m.group(2), 16), int(m.group(3))
                continue
            m = FPRINT.search(body)
            if m and cur["modules"]:
                cur["modules"][-1]["fingerprint"] = m.group(1).lower()
                continue
            # any other '#' line ends the library list (e.g. the next dump's prose)
            close()
            section = None
            continue
        if section == "regs":
            pairs = REG_PAIR.findall(body)
            if pairs:
                for name, val in pairs:
                    (cur["lbr"] if name in LBR_NAMES else cur["registers"])[name] = int(val, 16)
                continue
        # header fields (may be interleaved anywhere before the register dump)
        key, _, val = body.partition(":")
        key = key.strip().lower()
        val = val.strip()
        if key == "signal":
            m = re.match(r"(\d+)", val)
            if m:
                num = int(m.group(1))
                nm = re.search(r"\((SIG[A-Z]+)\)", val)
                cur["signal"] = "%d (%s)" % (num, nm.group(1) if nm else SIGNAMES.get(num, "?"))
            else:
                cur["signal"] = val[:40]
        elif key == "reason":
            for part in body.split("# reason:"):
                part = part.replace("reason:", "", 1).strip()
                if part:
                    cur["reasons"].append(part)
            m = FAULT_VA.search(body)
            if m:
                cur["fault_va"] = int(m.group(1), 16)
        elif key == "thread id":
            cur["thread_id"] = val
        elif key == "thread name":
            cur["thread_name"] = val
        elif key == "proc id":
            cur["pid"] = val
        elif key == "proc name":
            cur["proc_name"] = val
        elif key == "fault address":
            # the kernel's "fault address" line names the faulting INSTRUCTION
            # (it equals rip in every retained SIGSEGV block); the data address
            # is the "addr(VA)" value inside the reason line.
            m = FAULT_ADDR.match(body)
            if m:
                cur["fault_pc"] = int(m.group(1), 16)
                cur["fault_note"] = body[len("fault address:"):].strip()
        else:
            m = FAULT_VA.search(body)
            if m and cur["fault_va"] is None:
                cur["fault_va"] = int(m.group(1), 16)
    close()
    return blocks


# ------------------------------------------------------------ artifacts -----
class Artifact:
    """A local module image bound by GNU build-id. Optional exact symbols."""

    def __init__(self, label, build_id, exec_vaddr, exec_memsz, exec_align, elf=None, symbols=None,
                 tier=None, identity=None):
        self.label = label
        self.build_id = build_id.lower()
        self.exec_vaddr = exec_vaddr
        self.exec_memsz = exec_memsz
        self.exec_align = exec_align or 0x4000
        self.elf = elf
        self.symbols = symbols or []      # sorted [(addr, size, name)]
        self.addrs = [s[0] for s in self.symbols]
        self.tier = tier
        self.identity = identity or {}
        self.plt = {}
        if elf is not None:
            self.plt = plt_imports(elf)

    def lookup(self, va):
        i = bisect.bisect_right(self.addrs, va) - 1
        best = None
        j = i
        while j >= 0 and j > i - 64:
            a, sz, nm = self.symbols[j]
            if a <= va < a + sz:
                if best is None or sz < best[1]:
                    best = (a, sz, nm)
            j -= 1
        if best:
            return {"symbol": best[2], "symbol_addr": best[0], "offset": va - best[0], "size": best[1],
                    "inside": True}
        if i >= 0:
            a, sz, nm = self.symbols[i]
            return {"symbol": nm, "symbol_addr": a, "offset": va - a, "size": sz, "inside": False}
        return None

    def func_start_of(self, va):
        hit = self.lookup(va)
        if hit and hit["inside"]:
            return hit["symbol_addr"]
        return None

    def code(self, va, n):
        if self.elf is None:
            return None
        try:
            return self.elf.read_va(va, n)
        except elf64.ElfError:
            return None


def plt_imports(elf):
    """{plt_stub_va: import_name} from .plt FF 25 stubs + .rela.plt + .dynsym."""
    out = {}
    try:
        secs = {s["name"]: s for s in elf.sections()}
        plt, relp = secs.get(".plt"), secs.get(".rela.plt")
        if not plt or not relp:
            return out
        dyn = elf.symbols(kind=11)
        slot_to_name = {}
        data = elf.read(relp["offset"], relp["size"])
        for k in range(0, len(data) - 23, 24):
            r_off = int.from_bytes(data[k:k + 8], "little")
            info = int.from_bytes(data[k + 8:k + 16], "little")
            si = info >> 32
            if si < len(dyn):
                slot_to_name[r_off] = dyn[si]["name"].split("#")[0]
        code = elf.read(plt["offset"], plt["size"])
        for k in range(0, len(code) - 5):
            if code[k] == 0xFF and code[k + 1] == 0x25:
                disp = int.from_bytes(code[k + 2:k + 6], "little", signed=True)
                slot = plt["addr"] + k + 6 + disp
                if slot in slot_to_name:
                    out[plt["addr"] + k] = slot_to_name[slot]
    except (elf64.ElfError, ValueError, IndexError):
        return {}
    return out


def load_final_symbols(path):
    with open(path) as f:
        d = json.load(f)
    if d.get("schema") != "modbridge.final-symbols/v1":
        raise ValueError("%s: not a modbridge.final-symbols/v1 file" % path)
    bare = d["bare_prx"]
    elf = None
    bare_check = "not-rechecked (bare PRX missing)"
    if bare.get("path") and os.path.exists(bare["path"]):
        if sha256_path(bare["path"]) != bare["sha256"]:
            raise ValueError("%s: bare PRX %s changed since final_symbols ran (sha mismatch) - refusing"
                             % (path, bare["path"]))
        elf = elf64.Elf(bare["path"])
        bid = elf.gnu_build_id()
        if (bid or "").lower() != bare["build_id"].lower():
            raise ValueError("%s: bare PRX build-id %s != recorded %s" % (path, bid, bare["build_id"]))
        bare_check = "sha256+build-id rechecked"
    syms = sorted(((a, sz, nm) for nm, a, sz in d["symbols"]), key=lambda t: (t[0], -t[1]))
    if not syms:
        raise ValueError("%s: zero symbols" % path)
    xl = bare["exec_load"]
    ident = {"final_symbols": os.path.abspath(path), "bare_sha256": bare["sha256"],
             "packaged_sha256": (d.get("packaged_prx") or {}).get("sha256"), "bare_check": bare_check,
             "sections_verified": d["layout"]["sections_verified"]}
    return Artifact(os.path.basename(bare.get("path") or path), bare["build_id"], xl["vaddr"], xl["memsz"],
                    xl["align"], elf=elf, symbols=syms, tier=d.get("tier", "OBJ-LAYOUT-BYTE-VERIFIED"),
                    identity=ident)


def load_image(path):
    elf = elf64.Elf(path)
    bid = elf.gnu_build_id()
    if not bid:
        raise ValueError("%s: no GNU build-id note; cannot bind to a crash fingerprint" % path)
    xls = elf.exec_loads()
    if len(xls) != 1:
        raise ValueError("%s: expected one executable PT_LOAD, found %d" % (path, len(xls)))
    xl = xls[0]
    syms = []
    tier = None
    try:
        st = elf.symbols()
        if st:
            syms = [(s["value"], s["size"], s["name"]) for s in st if s["type"] == 2 and s["size"]]
            tier = "IMAGE-SYMTAB"
    except elf64.ElfError:
        syms = []
    syms.sort(key=lambda t: (t[0], -t[1]))
    return Artifact(os.path.basename(path), bid, xl["vaddr"], xl["memsz"], xl["align"], elf=elf,
                    symbols=syms, tier=tier, identity={"image": os.path.abspath(path)})


# ----------------------------------------------------------- call edges -----
def modrm_len(b, k):
    """Length of ModRM(+SIB+disp) starting at b[k]; None if out of range."""
    if k >= len(b):
        return None
    modrm = b[k]
    mod, rm = modrm >> 6, modrm & 7
    n = 1
    if mod != 3 and rm == 4:
        if k + 1 >= len(b):
            return None
        sib = b[k + 1]
        n += 1
        if mod == 0 and (sib & 7) == 5:
            n += 4
    if mod == 0 and rm == 5:
        n += 4
    elif mod == 1:
        n += 1
    elif mod == 2:
        n += 4
    return n


def call_before(art, ret):
    """Decode the call instruction that ends exactly at `ret` (a vaddr)."""
    pre = art.code(ret - 8, 8)
    if pre is None:
        return {"kind": "NO-BYTES"}
    if pre[3] == 0xE8:
        disp = int.from_bytes(pre[4:8], "little", signed=True)
        return {"kind": "DIRECT", "target": ret + disp, "length": 5}
    for L in (2, 3, 4, 6, 7, 8):
        seq = pre[8 - L:]
        k = 0
        if 0x40 <= seq[0] <= 0x4F:
            k = 1
        if k >= len(seq) or seq[k] != 0xFF or k + 1 >= len(seq):
            continue
        if (seq[k + 1] >> 3) & 7 != 2:
            continue
        n = modrm_len(seq, k + 1)
        if n is not None and k + 1 + n == L:
            return {"kind": "INDIRECT", "length": L}
    return {"kind": "NOT-A-CALL"}


# --------------------------------------------------------------- symbolize --
def owner_of(addr, modules):
    for m in modules:
        if m["start"] is not None and m["start"] <= addr < m["end"]:
            return m
    return None


def bind(mod, arts):
    fp = (mod.get("fingerprint") or "").lower()
    if not fp:
        return None, "no fingerprint in klog"
    art = arts.get(fp)
    if art is None:
        return None, "no local artifact with build-id %s" % fp
    align = art.exec_align or 0x4000
    want = (art.exec_memsz + align - 1) // align * align
    have = mod["end"] - mod["start"]
    if mod["start"] % align != 0 or have != want:
        return None, ("CONVENTION-MISMATCH: xotext extent 0x%x vs artifact exec memsz rounded 0x%x "
                      "(start 0x%x, align 0x%x) - module+offset only" % (have, want, mod["start"], align))
    return art, "bound: fingerprint == build-id; xotext extent == exec PT_LOAD memsz rounded to 0x%x" % align


def symbolize(addr, modules, arts, cache):
    r = {"addr": addr}
    m = owner_of(addr, modules)
    if m is None:
        r["module"] = None
        return r
    r["module"] = m["path"]
    r["module_start"] = m["start"]
    r["offset"] = addr - m["start"]
    key = (m["path"], m["start"])
    if key not in cache:
        cache[key] = bind(m, arts)
    art, why = cache[key]
    r["binding"] = why
    if art is None:
        return r
    va = addr - m["start"] + art.exec_vaddr
    r["va"] = va
    r["artifact"] = art.label
    if art.symbols:
        hit = art.lookup(va)
        if hit:
            r.update({"symbol": hit["symbol"], "symbol_va": hit["symbol_addr"], "sym_offset": hit["offset"],
                      "inside": hit["inside"], "tier": art.tier})
    if va in art.plt:
        r["plt_import"] = art.plt[va]
    r["_art"] = art
    return r


def check_edges(frames, rip_sym):
    """frames: symbolized backtrace (outermost last). Adds 'edge' to each."""
    inner = rip_sym
    confirmed = checked = 0
    for fr in frames:
        art = fr.get("_art")
        if art is None or art.elf is None:
            fr["edge"] = {"verdict": "NOT-CHECKED", "why": "no bytes for this module"}
            inner = fr
            continue
        c = call_before(art, fr["va"])
        checked += 1
        e = {"call": c["kind"]}
        if c["kind"] == "DIRECT":
            tgt = c["target"]
            e["target_va"] = tgt
            if tgt in art.plt:
                e["target"] = "import " + art.plt[tgt]
                e["verdict"] = "CALL-TO-IMPORT"
                confirmed += 1
            else:
                th = art.lookup(tgt) if art.symbols else None
                if th:
                    e["target"] = th["symbol"] + ("" if th["offset"] == 0 else "+0x%x" % th["offset"])
                inner_start = None
                if inner is not None and inner.get("_art") is art:
                    inner_start = art.func_start_of(inner["va"]) if art.symbols else None
                    if inner_start is None and not art.symbols:
                        # no symbols: the callee's start IS the call target (function-start discovery)
                        e["verdict"] = "CALL (target = callee start, no symbols)"
                        confirmed += 1
                        fr["edge"] = e
                        inner = fr
                        continue
                if inner_start is not None and inner_start == tgt:
                    e["verdict"] = "CONFIRMED (call target == start of next-inner frame's function)"
                    confirmed += 1
                elif inner_start is not None:
                    e["verdict"] = "CALL-TARGET-DIFFERS (tail call, thunk or skipped frame)"
                else:
                    e["verdict"] = "CALL (next-inner frame in another module or unsymbolized)"
                    confirmed += 1
        elif c["kind"] == "INDIRECT":
            e["verdict"] = "INDIRECT-CALL (target not static)"
            confirmed += 1
        elif c["kind"] == "NOT-A-CALL":
            e["verdict"] = "NOT-A-CALL-SITE (stale stack slot, or wrong base/symbols)"
        else:
            e["verdict"] = "NO-BYTES"
        fr["edge"] = e
        inner = fr
    return checked, confirmed


# ------------------------------------------------------------------ report --
_DEMANGLE_CACHE = {}


def demangle(names):
    todo = [n for n in names if n and n.startswith("_Z") and n not in _DEMANGLE_CACHE]
    if todo:
        try:
            p = subprocess.run(["c++filt", "-n"], input="\n".join(todo) + "\n", capture_output=True,
                               text=True, timeout=30)
            outs = p.stdout.splitlines()
            if len(outs) == len(todo):
                _DEMANGLE_CACHE.update(zip(todo, outs))
        except (OSError, subprocess.SubprocessError):
            pass
    return {n: _DEMANGLE_CACHE.get(n, n) for n in names}


def short(name, width=110):
    if name is None:
        return None
    return name if len(name) <= width else name[:width - 3] + "..."


def where(r, dm):
    if r.get("module") is None:
        return "0x%x (not inside any loaded module)" % r["addr"]
    base = os.path.basename(r["module"])
    s = "%s+0x%x" % (base, r["offset"])
    if r.get("plt_import"):
        s += " [import stub %s]" % r["plt_import"]
    if r.get("symbol"):
        nm = short(dm.get(r["symbol"], r["symbol"]))
        if r.get("inside"):
            s += " `%s`+0x%x" % (nm, r["sym_offset"])
        else:
            s += " (outside known functions; nearest below `%s`+0x%x)" % (nm, r["sym_offset"])
    return s


def classify(block, rip, frames, arts_bound):
    """Plain-language 'who' line. Generic: 'our runtime' = modules bound to a
    final-symbols artifact (supplied by the caller), never matched by name."""
    def ours(r):
        a = r.get("_art")
        return a is not None and a.tier == "OBJ-LAYOUT-BYTE-VERIFIED"
    first_ours = next((f for f in frames if ours(f)), None)
    if rip and ours(rip):
        return "The crash happened INSIDE our runtime (the PRX we ship)."
    if first_ours is not None:
        mod = os.path.basename(rip["module"]) if rip and rip.get("module") else "an unknown address"
        return ("The crash surfaced in %s, but it was CALLED FROM our runtime (first runtime frame "
                "below)." % mod)
    if rip and rip.get("module"):
        return ("The crash happened in %s; our runtime does not appear in the backtrace."
                % os.path.basename(rip["module"]))
    return "The crash address is outside every loaded module."


def build_report(args, klog_inputs, all_blocks, arts, logs, stats):
    out = []
    a = out.append
    a("# Crash report (cap05 crashcap)")
    a("")
    a("Mac-only analysis of console-recorded data. Source of every crash fact: the klog capture "
      "named below (PS5 kernel fatal-signal dump). Nothing here was sent to or read from the console.")
    a("")
    for k in klog_inputs:
        a("- klog: `%s` sha256 `%s` (%d lines scanned)" % (k["path"], k["sha256"][:16], k["lines"]))
    for art in arts.values():
        a("- bound artifact: `%s` build-id `%s` tier %s %s" % (art.label, art.build_id, art.tier or "bytes-only",
                                                         json.dumps(art.identity, sort_keys=True)))
    a("")
    a("**%d crash block(s) found.** Addresses symbolized: %d (exact function: %d). Call edges checked: %d "
      "(consistent: %d)." % (stats["blocks"], stats["addrs"], stats["exact"], stats["edges_checked"],
                              stats["edges_ok"]))
    a("")
    if not all_blocks:
        a("NO CRASH BLOCKS in the scanned klog lines. (This is not proof the game never crashed: "
          "the klog tap must have been running during the whole session.)")
        a("")
    for bi, (kpath, blk, rip, fault, frames, lbr, who) in enumerate(all_blocks, 1):
        names = [r.get("symbol") for r in [rip, fault] + frames + lbr if r]
        dm = demangle([n for n in names if n])
        a("## Crash %d - %s, process `%s` thread `%s` (pid %s), klog line %d" % (
            bi, blk.get("signal") or "; ".join(blk["reasons"][:1]) or "unknown reason", blk.get("proc_name"), blk.get("thread_name"),
            blk.get("pid"), blk["line"]))
        a("")
        a("**Who:** %s" % who)
        a("")
        if blk["reasons"]:
            a("- kernel reason: %s" % " | ".join(r[:140] for r in blk["reasons"]))
        if rip:
            a("- RIP (where it stopped): `0x%x` = %s" % (rip["addr"], where(rip, dm)))
        if blk.get("fault_va") is not None:
            a("- faulting DATA address (kernel addr(VA)): `0x%x`%s" % (
                blk["fault_va"], (" = " + where(fault, dm)) if fault else " (same as RIP)"))
        if blk.get("fault_pc") is not None and blk["fault_pc"] != blk["registers"].get("rip"):
            a("- kernel 'fault address' (instruction) `0x%x` differs from RIP" % blk["fault_pc"])
        if blk.get("copyin_note"):
            a("- kernel note: `%s` (normal inside crash dumps)" % blk["copyin_note"])
        a("")
        a("### Backtrace (innermost first)")
        a("")
        a("| # | address | where | call-edge check |")
        a("|---|---|---|---|")
        for i, fr in enumerate(frames):
            e = fr.get("edge", {})
            v = e.get("verdict", "")
            if e.get("target"):
                v += "; calls `%s`" % short(demangle([e["target"].split("+")[0]]).get(e["target"].split("+")[0],
                                                                                       e["target"]), 70)
            a("| %d | `0x%x` | %s | %s |" % (i, fr["addr"], where(fr, dm).replace("|", "\\|"), v.replace("|", "\\|")))
        if not frames:
            a("| - | - | kernel printed no backtrace frames | - |")
        a("")
        a("### Registers")
        a("")
        regs = blk["registers"]
        missing = [r for r in ("rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "r8", "r9", "r10", "r11",
                               "r12", "r13", "r14", "r15", "rip") if r not in regs]
        if missing:
            a("Not recoverable from this klog (line damaged by interleaved output): %s" % ", ".join(missing))
            a("")
        if blk.get("salvaged_lines"):
            a("(%d dump line(s) had other text glued in front; the dump part was recovered.)" % blk["salvaged_lines"])
            a("")
        a("```")
        line = []
        for name in sorted(regs, key=lambda n: (len(n), n)):
            line.append("%-3s %016x" % (name, regs[name]))
            if len(line) == 4:
                a("  ".join(line))
                line = []
        if line:
            a("  ".join(line))
        a("```")
        if lbr:
            a("")
            a("Last-branch records (CPU; from -> to, newest first as printed):")
            a("")
            for r in lbr:
                a("- %s `0x%x` = %s" % (r["name"], r["addr"], where(r, dm)))
        a("")
        mods_used = sorted({r["module"] for r in [rip, fault] + frames if r and r.get("module")})
        a("### Modules involved")
        a("")
        for m in blk["modules"]:
            if m["path"] in mods_used:
                bnd = next((r.get("binding") for r in [rip, fault] + frames
                            if r and r.get("module") == m["path"] and r.get("binding")), "")
                a("- `%s` xotext `0x%x:0x%x` fingerprint `%s` - %s" % (
                    m["path"], m["start"], m["end"], m.get("fingerprint"), bnd))
        a("- (%d modules loaded in total per this dump)" % len(blk["modules"]))
        a("")
    for lg in logs:
        a("## Last %d lines of %s" % (len(lg["tail"]), lg["name"]))
        a("")
        a("`%s` sha256 `%s` (%d lines; NUL-interleaving decoded)" % (lg["path"], lg["sha256"][:16], lg["lines"]))
        a("")
        a("```")
        for t in lg["tail"]:
            a(IPV4.sub("x.x.x.x", t)[:300])
        a("```")
        a("")
    return "\n".join(out) + "\n"


def strip_private(r):
    if r is None:
        return None
    return {k: (v if not isinstance(v, int) or k in ("inside",) else v) for k, v in r.items() if k != "_art"}


def main(argv=None):
    ap = argparse.ArgumentParser(description="PS5 crash report from klog (cap05)")
    ap.add_argument("--klog", action="append", required=True)
    ap.add_argument("--symbols", action="append", default=[], help="final_symbols.py JSON (our runtime)")
    ap.add_argument("--image", action="append", default=[], help="any module ELF image (bound by build-id)")
    ap.add_argument("--log", action="append", default=[], help="NAME=PATH runtime log to tail")
    ap.add_argument("--tail", type=int, default=40)
    ap.add_argument("--out")
    ap.add_argument("--json")
    ap.add_argument("--expect-crash", action="store_true")
    ap.add_argument("--expect-rip-symbol")
    ap.add_argument("--expect-fault-va")
    ap.add_argument("--decoder", default=DEFAULT_DECODER,
                    help="path to harvest/decode_session_log.py ('' = built-in NUL-strip rule)")
    a = ap.parse_args(argv)
    try:
        load_decoder(a.decoder)
    except Exception as e:  # a broken decoder must not silently change results
        print("crashcap: input error: decoder %s failed to load: %s" % (a.decoder, e), file=sys.stderr)
        return 2
    for p in (a.out, a.json):
        if p and os.path.exists(p):
            print("crashcap: refusing to overwrite %s" % p, file=sys.stderr)
            return 2
    try:
        arts = {}
        for p in a.symbols:
            art = load_final_symbols(p)
            arts[art.build_id] = art
        for p in a.image:
            art = load_image(p)
            arts.setdefault(art.build_id, art)
        klog_inputs = []
        parsed = []
        total_lines = 0
        for kp in a.klog:
            lines = read_text_lines(kp)
            total_lines += len(lines)
            klog_inputs.append({"path": os.path.abspath(kp), "sha256": sha256_path(kp), "lines": len(lines)})
            for blk in parse_klog(lines):
                parsed.append((kp, blk))
        logs = []
        for spec in a.log:
            name, _, path = spec.partition("=")
            if not path:
                name, path = os.path.basename(spec), spec
            lines = read_text_lines(path)
            tail = [t for t in lines if t.strip()][-a.tail:]
            logs.append({"name": name, "path": os.path.abspath(path), "sha256": sha256_path(path),
                         "lines": len(lines), "tail": tail})
    except (OSError, ValueError, KeyError, elf64.ElfError) as e:
        print("crashcap: input error: %s" % e, file=sys.stderr)
        return 2
    if total_lines == 0:
        print("crashcap: FAIL scanned 0 klog lines (empty capture) - a gate that scans nothing fails",
              file=sys.stderr)
        return 3

    stats = {"blocks": 0, "addrs": 0, "exact": 0, "edges_checked": 0, "edges_ok": 0}
    all_blocks = []
    jblocks = []
    for kp, blk in parsed:
        cache = {}
        mods = [m for m in blk["modules"] if m["start"] is not None]
        rip = symbolize(blk["registers"]["rip"], mods, arts, cache) if "rip" in blk["registers"] else None
        fault = None
        if blk["fault_va"] is not None and blk["fault_va"] != blk["registers"].get("rip"):
            fault = symbolize(blk["fault_va"], mods, arts, cache)
        frames = [symbolize(x, mods, arts, cache) for x in blk["backtrace"] if x]
        lbr = []
        for nm in ("lbf", "lbt", "lb0", "lb1", "lb2", "lb3", "lb4", "lb5", "lb6", "lb7"):
            v = blk["lbr"].get(nm)
            if v is not None and v != 0xFFFFFFFFFFFFFFFF and v != 0:
                r = symbolize(v, mods, arts, cache)
                r["name"] = nm
                lbr.append(r)
        ch, ok = check_edges(frames, rip)
        who = classify(blk, rip, frames, arts)
        everything = [r for r in [rip, fault] + frames + lbr if r]
        stats["blocks"] += 1
        stats["addrs"] += len(everything)
        stats["exact"] += sum(1 for r in everything if r.get("inside"))
        stats["edges_checked"] += ch
        stats["edges_ok"] += ok
        all_blocks.append((kp, blk, rip, fault, frames, lbr, who))
        jb = dict(blk)
        jb["klog"] = os.path.abspath(kp)
        jb["who"] = who
        jb["rip_sym"] = strip_private(rip)
        jb["fault_sym"] = strip_private(fault)
        jb["frames_sym"] = [strip_private(f) for f in frames]
        jb["lbr_sym"] = [strip_private(f) for f in lbr]
        jblocks.append(jb)

    report = build_report(a, klog_inputs, all_blocks, arts, logs, stats)
    if a.out:
        with open(a.out, "x") as f:
            f.write(report)
    else:
        sys.stdout.write(report)
    if a.json:
        with open(a.json, "x") as f:
            json.dump({"schema": "modbridge.crashcap/v1", "console_contact": False, "klogs": klog_inputs,
                       "stats": stats, "blocks": jblocks,
                       "logs": [{k: v for k, v in lg.items()} for lg in logs]}, f, indent=1, default=str)
    print("crashcap: scanned %d klog lines in %d file(s); crash blocks=%d; addresses=%d (exact function %d); "
          "call-edges checked=%d consistent=%d; logs tailed=%d"
          % (total_lines, len(klog_inputs), stats["blocks"], stats["addrs"], stats["exact"],
             stats["edges_checked"], stats["edges_ok"], len(logs)), file=sys.stderr)

    rc = 0
    if a.expect_crash and stats["blocks"] == 0:
        print("crashcap: EXPECT FAIL: no crash block", file=sys.stderr)
        rc = 1
    if a.expect_rip_symbol:
        hits = [b for b in jblocks if b["rip_sym"] and a.expect_rip_symbol in str(
            demangle([b["rip_sym"].get("symbol") or ""]).get(b["rip_sym"].get("symbol") or "", ""))
            and b["rip_sym"].get("inside")]
        if not hits:
            print("crashcap: EXPECT FAIL: no crash with RIP inside a function matching %r"
                  % a.expect_rip_symbol, file=sys.stderr)
            rc = 1
        else:
            print("crashcap: EXPECT PASS: RIP inside %r (%d block(s))" % (a.expect_rip_symbol, len(hits)),
                  file=sys.stderr)
    if a.expect_fault_va:
        want = int(a.expect_fault_va, 16)
        if not any(b["fault_va"] == want for b in jblocks):
            print("crashcap: EXPECT FAIL: no crash with fault VA 0x%x" % want, file=sys.stderr)
            rc = 1
        else:
            print("crashcap: EXPECT PASS: fault VA 0x%x" % want, file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())
