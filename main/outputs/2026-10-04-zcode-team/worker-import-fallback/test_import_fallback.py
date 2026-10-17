#!/usr/bin/env python3
"""Self-contained tests for import_fallback.weakify on a synthetic ELF64.

Builds a minimal ELF64 object whose .dynsym contains:
  * GLOBAL UND  sceKernelDlsym   (long form nid#A#B)  -> must become WEAK
  * GLOBAL UND  qsort            (long form nid#A#B)  -> must become WEAK
  * GLOBAL UND  sceKernelOpen    (control, NOT a target) -> must stay GLOBAL
  * WEAK   UND  __dlsym          (frozen-contract weak import) -> untouched
  * GLOBAL DEFINED module_start-like symbol (shndx != 0, nid of
    "module_start") -> untouched DEFINED export
  * GLOBAL UND  malloc           (requested via --names override but absent)
    -> triggers fail-closed missing-name abort unless allow_missing

Asserts: exactly one byte changes per patched entry (the st_info bind nibble),
all other bytes identical, type nibble preserved, idempotent on rerun.
Run: python3 test_import_fallback.py
"""
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from import_fallback import (EHDR, SHDR, SYM, SHT_DYNSYM, STB_GLOBAL,
                             STB_WEAK, sce_nid, weakify)

TMP = HERE / "test-tmp"


def build_elf():
    # --- .dynstr ---------------------------------------------------------
    names = [("sceKernelDlsym", "#A#B"), ("qsort", "#A#B"),
             ("sceKernelOpen", "#A#B"), ("__dlsym", "#A#B"),
             ("module_start", "#A#B")]
    blob = b"\x00"
    offs = {}
    for name, suffix in names:
        offs[name] = len(blob)
        blob += sce_nid(name).encode() + suffix.encode() + b"\x00"
    dynstr = blob

    # --- .dynsym ---------------------------------------------------------
    # layout: null, dlSym(GLOBAL UND), qsort(GLOBAL UND), open(GLOBAL UND),
    #         dlsym(WEAK UND), module_start(GLOBAL DEFINED shndx=4)
    entries = [(0, 0, 0, 0, 0, 0)]
    entries.append((offs["sceKernelDlsym"], (STB_GLOBAL << 4) | 2, 0, 0, 0, 0))
    entries.append((offs["qsort"], (STB_GLOBAL << 4) | 2, 0, 0, 0, 0))
    entries.append((offs["sceKernelOpen"], (STB_GLOBAL << 4) | 2, 0, 0, 0, 0))
    entries.append((offs["__dlsym"], (STB_WEAK << 4) | 2, 0, 0, 0, 0))
    entries.append((offs["module_start"], (STB_GLOBAL << 4) | 2, 0, 4,
                    0x15A0, 0x20))
    dynsym = b"".join(SYM.pack(*e) for e in entries)

    # --- section layout: NULL, .dynsym, .dynstr, .shstrtab ----------------
    shstr = b"\x00.dynsym\x00.dynstr\x00.shstrtab\x00"
    shstr_offs = {".dynsym": 1, ".dynstr": 9, ".shstrtab": 17}

    off = EHDR.size
    align16 = lambda o: (o + 15) & ~15
    off = align16(off)
    dynsym_off = off; off += len(dynsym)
    dynstr_off = off; off += len(dynstr)
    shstr_off = off; off += len(shstr)
    off = align16(off)
    shoff = off

    eh = EHDR.pack(b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\x00" * 8,
                   1, 0x3E, 1, 0, 0, shoff, 0, EHDR.size, 0, 0,
                   SHDR.size, 4, 3)
    body = (dynsym + dynstr + shstr)
    pad = shoff - (EHDR.size + len(body))
    sections = [SHDR.pack(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                SHDR.pack(shstr_offs[".dynsym"], SHT_DYNSYM, 2, 0,
                          dynsym_off, len(dynsym), 2, 1, 8, SYM.size),
                SHDR.pack(shstr_offs[".dynstr"], 3, 0x2, 0,
                          dynstr_off, len(dynstr), 0, 0, 1, 0),
                SHDR.pack(shstr_offs[".shstrtab"], 3, 0, 0,
                          shstr_off, len(shstr), 0, 0, 1, 0)]
    return eh + body + b"\x00" * pad + b"".join(sections)


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)
    print(f"  ok: {msg}")


def main():
    TMP.mkdir(exist_ok=True)
    elf = build_elf()
    src = TMP / "synthetic.elf"
    src.write_bytes(elf)

    print("1) weakify patches only the two requested GLOBAL UND entries")
    report, out = weakify(elf, ["sceKernelDlsym", "qsort"], {}, raw=False,
                          allow_missing=False)
    check(len(report["patched"]) == 2, "two entries patched")
    check(sorted(p["name"] for p in report["patched"]) ==
          ["qsort", "sceKernelDlsym"], "patched names exact")
    check(report["bytes_changed"] == 2, "exactly two bytes changed")
    diff = [i for i in range(len(elf)) if elf[i] != out[i]]
    sym0 = EHDR.size  # first real dynsym entry after null symbol
    check(all(i - 4 in (sym0 + SYM.size * 1, sym0 + SYM.size * 2)
              for i in diff), "changed bytes are the two st_info fields")
    for i in diff:
        old, new = elf[i], out[i]
        check((old >> 4) == STB_GLOBAL and (new >> 4) == STB_WEAK and
              (old & 0xF) == (new & 0xF),
              f"st_info at {i}: bind GLOBAL->WEAK, type nibble preserved")

    print("2) control symbols untouched")
    dynsyms = [SYM.unpack_from(out, sym0 + SYM.size * k) for k in range(6)]
    check(dynsyms[3][1] >> 4 == STB_GLOBAL,
          "sceKernelOpen (control) still GLOBAL")
    check(dynsyms[4][1] >> 4 == STB_WEAK, "__dlsym still WEAK (untouched)")
    check(dynsyms[5][1] == elf[sym0 + SYM.size * 5 + 4] and
          dynsyms[5][3] == 4, "DEFINED module_start export untouched")

    print("3) idempotent rerun")
    report2, out2 = weakify(out, ["sceKernelDlsym", "qsort"], {}, raw=False,
                            allow_missing=False)
    check(report2["patched"] == [] and
          sorted(report2["already_weak"]) == ["qsort", "sceKernelDlsym"],
          "second run: already_weak, no new patches")
    check(out2 == out, "second run output byte-identical")

    print("4) fail-closed on a requested-but-absent import")
    try:
        weakify(elf, ["sceKernelDlsym", "malloc"], {}, raw=False,
                allow_missing=False)
        raise AssertionError("missing malloc should have aborted")
    except Exception as exc:
        check("missing requested imports" in str(exc) and
              "'name': 'malloc'" in str(exc),
              "aborts naming the absent symbol")
    report3, _ = weakify(elf, ["sceKernelDlsym", "malloc"], {}, raw=False,
                         allow_missing=True)
    check([p["name"] for p in report3["patched"]] == ["sceKernelDlsym"] and
          any(m["name"] == "malloc" for m in report3["missing"]),
          "--allow-missing reports the gap without aborting")

    print("5) fail-closed refuses DEFINED requested symbol")
    try:
        weakify(elf, ["module_start"], {}, raw=False, allow_missing=True)
        raise AssertionError("DEFINED symbol should have been refused")
    except Exception as exc:
        check("DEFINED" in str(exc), "refuses to weakify a defined symbol")

    print("6) CLI end-to-end + audit subcommand")
    dst = TMP / "synthetic.weak.elf"
    rep = TMP / "report.json"
    rc = subprocess.run([sys.executable, str(HERE / "import_fallback.py"),
                         "weakify", "--elf", str(src), "--out", str(dst),
                         "--names", "sceKernelDlsym,qsort",
                         "--report", str(rep)]).returncode
    check(rc == 0 and dst.read_bytes() == out and rep.exists(),
          "CLI writes identical output and report")
    import json
    aud = json.loads(subprocess.run(
        [sys.executable, str(HERE / "import_fallback.py"), "audit",
         "--elf", str(dst),
         "--map", f"sceKernelOpen={sce_nid('sceKernelOpen')}"],
        capture_output=True, text=True).stdout)
    binds = {i["friendly"]: i["bind"] for i in aud if i["friendly"]}
    check(binds.get("sceKernelDlsym") == "WEAK" and
          binds.get("qsort") == "WEAK" and
          binds.get("sceKernelOpen") == "GLOBAL",
          "audit decodes NID long form and reports post-patch binds")

    print("ALL TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
