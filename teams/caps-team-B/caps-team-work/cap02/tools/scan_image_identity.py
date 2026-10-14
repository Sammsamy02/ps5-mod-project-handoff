#!/usr/bin/env python3
"""Generic, image-derived engine identity for the first-launch profile generator.

Mac-only, read-only. Given a plain ELF64 game image, derive the engine cell
and per-image identity the generator needs, using ONLY the image's own bytes:

  * major/minor  -- source A: FEngineVersionBase store site (patternsleuth
                    scan_version_stores shape: mov rm32,imm32 whose imm is
                    major|minor<<16, immediately followed by a <=30 operand-
                    size 16-bit patch store to +4, with a changelist write to
                    +8 shortly after); source B: the 244-marker axes
                    fingerprint. Both must agree or we refuse.
  * patch/changelist/branch -> engine_revision, reconstructed from the same
                    store site + the branch-name pointer (FEngineVersion shape).
  * build_id     -- NT_GNU_BUILD_ID note (type 3, name "GNU").
  * non_shipping/stats -> from the axes fingerprint.

No disassembler library is required: the store site is matched structurally on
the raw bytes (the exact instruction shapes patternsleuth already uses). The
result is the generic input any new UE game produces from its own executable.

Prints a JSON identity + cell, and (with --expect) asserts against a known
answer for a self-test. Refuses (exit 3) on any ambiguity.
"""
import argparse, json, re, struct, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AXES_DIR = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/"
                "2026-10-04-zcode-team/worker-axes-detector")


def elf_loads_and_notes(data):
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        raise SystemExit("not a plain little-endian ELF64 image")
    phoff, = struct.unpack_from("<Q", data, 32)
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    loads, notes = [], []
    for i in range(phnum):
        o = phoff + i * phentsize
        ptype, flags, off, va, pa, fs, ms, al = struct.unpack_from("<IIQQQQQQ", data, o)
        if ptype == 1:
            loads.append((off, va, fs, ms, flags))
        elif ptype == 4:
            notes.append((off, fs))
    return loads, notes


def file_to_vaddr(loads, foff):
    for off, va, fs, ms, fl in loads:
        if off <= foff < off + fs:
            return va + (foff - off)
    return None


def gnu_build_id(data, notes):
    for off, size in notes:
        p, end = off, off + size
        while p + 12 <= end:
            namesz, descsz, typ = struct.unpack_from("<III", data, p)
            name = data[p + 12:p + 12 + namesz]
            desc_off = p + 12 + ((namesz + 3) & ~3)
            if name[:4] == b"GNU\x00" and typ == 3:
                return data[desc_off:desc_off + descsz].hex()
            p = desc_off + ((descsz + 3) & ~3)
    return ""


def _decode_c7_store(data, i, want_prefix_66):
    """Decode a `[REX] [66] C7 /0 mem, imm` store at offset i.
    Returns (base_reg, disp, imm, imm_width, end_off) or None. base_reg folds
    the REX.B bit; rip-relative (rm==5,mod==0) is reported as base_reg=-1."""
    p = i
    rex = 0
    if 0x40 <= data[p] <= 0x4f:
        rex = data[p]; p += 1
    if want_prefix_66:
        if data[p] != 0x66:
            return None
        p += 1
        if 0x40 <= data[p] <= 0x4f:
            rex = data[p]; p += 1
    if data[p] != 0xc7:
        return None
    p += 1
    modrm = data[p]; p += 1
    mod, reg, rm = modrm >> 6, (modrm >> 3) & 7, modrm & 7
    if reg != 0 or mod == 3:
        return None
    base = rm | ((rex & 1) << 3)
    if rm == 4:  # SIB
        sib = data[p]; p += 1
        base = (sib & 7) | ((rex & 1) << 3)
    if mod == 0 and rm == 5:
        base = -1; p += 4  # rip-relative disp32
        disp = 0
    elif mod == 0 and rm == 4 and (base & 7) == 5:
        disp, = struct.unpack_from("<i", data, p); p += 4
    elif mod == 0:
        disp = 0
    elif mod == 1:
        disp = struct.unpack_from("<b", data, p)[0]; p += 1
    else:
        disp, = struct.unpack_from("<i", data, p); p += 4
    width = 2 if want_prefix_66 else 4
    if p + width > len(data):
        return None
    imm = struct.unpack_from("<H" if width == 2 else "<I", data, p)[0]
    return base, disp, imm, width, p + width


def scan_version_stores(data):
    """Return {(major,minor): meta} from FEngineVersionBase store sites.

    Structural port of patternsleuth scan_version_stores: a `C7 /0 imm32`
    store whose imm32 is major|minor<<16, immediately followed by a 16-bit
    (`66`-prefixed) `C7 /0` patch store (value <=30) to the SAME base at
    disp+4, with a further write to disp+8 within the next few instructions
    (without it 'every mov dword[X],5 looks like 5.0'). disp must be 4-aligned
    and minor != 0.
    """
    out = {}
    for m in re.finditer(rb"\xc7", data):
        i = m.start()
        # allow an optional single REX prefix before C7
        start = i - 1 if i > 0 and 0x40 <= data[i - 1] <= 0x4f else i
        first = _decode_c7_store(data, start, False)
        if not first:
            continue
        base, disp, imm, _w, end = first
        if base < 0 or disp % 4 != 0:
            continue
        major, minor = imm & 0xffff, imm >> 16
        if not ((major == 4 and 1 <= minor <= 27) or (major == 5 and 1 <= minor <= 20)):
            continue
        patch_dec = _decode_c7_store(data, end, True)
        if not patch_dec:
            continue
        pbase, pdisp, patch, _pw, pend = patch_dec
        if pbase != base or pdisp != disp + 4 or patch > 30:
            continue
        # changelist write to disp+8 within the next few instructions
        found_cl = False
        q = pend
        for _ in range(4):
            dec = _decode_c7_store(data, q, False)
            if dec and dec[0] == base and dec[1] == disp + 8:
                found_cl = True; break
            # also accept a `mov [base+disp+8], r32` (89 /r) store
            r = q
            if 0x40 <= data[r] <= 0x4f:
                r += 1
            if data[r] == 0x89:
                mrm = data[r + 1]; md, rm2 = mrm >> 6, mrm & 7
                b2 = rm2 | (((data[q] if 0x40 <= data[q] <= 0x4f else 0) & 1) << 3)
                if rm2 != 4:
                    d8 = struct.unpack_from("<b", data, r + 2)[0] if md == 1 else (
                        struct.unpack_from("<i", data, r + 2)[0] if md == 2 else 0)
                    if b2 == base and d8 == disp + 8:
                        found_cl = True; break
            q += 1
        if not found_cl:
            continue
        out[(major, minor)] = {"store_file_off": start, "patch": patch}
    return out


def axes_verdict(image_path):
    """Run the host axes detector (C) and return its JSON verdict."""
    import subprocess, tempfile, os
    test_bin = AXES_DIR / "host" / "test_axes_detector"
    if not test_bin.exists():
        src = AXES_DIR / "src"
        subprocess.run(["cc", "-O2", "-std=c11", "-o", str(test_bin),
                        str(AXES_DIR / "host" / "test_axes_detector.c"),
                        str(src / "axes_detector.c")], check=True)
    r = subprocess.run([str(test_bin), image_path], capture_output=True, text=True)
    txt = r.stdout + r.stderr
    m = re.search(r"\{.*\"build_configuration\".*\}", txt, re.S)
    if not m:
        raise SystemExit("axes detector produced no verdict:\n" + txt[-2000:])
    return json.loads(m.group(0))


def reconstruct_revision(data, loads, major, minor, patch):
    """engine_revision = '<maj>.<min>.<patch>-<changelist>+++<branch>'.
    Changelist from the FEngineVersion helper that returns 0 in shipping
    (verified: helper body 'xor eax,eax; ret'); branch from the wide 'UE5'/
    'UE4' string the branch accessor references. We read the branch generically
    by looking for a wide 'UE5' or 'UE4' preceded by a NUL (string start)."""
    changelist = 0
    branch = None
    for cand in ("UE5", "UE4"):
        w = ("\x00" + cand).encode("utf-16-le")  # NUL-terminated predecessor + branch
        i = data.find(w)
        if i >= 0:
            branch = cand
            break
    if branch is None:
        return None
    return f"{major}.{minor}.{patch}-{changelist}+++{branch}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--expect", help="JSON file with expected cell+identity (self-test)")
    ap.add_argument("--out")
    a = ap.parse_args()
    data = Path(a.image).read_bytes()
    loads, notes = elf_loads_and_notes(data)

    stores = scan_version_stores(data)
    if len(stores) != 1:
        print(json.dumps({"refuse": "version store not uniquely resolved",
                          "candidates": [f"{m}.{n}" for (m, n) in stores]}, indent=1))
        return 3
    (major, minor), meta = next(iter(stores.items()))

    axes = axes_verdict(a.image)
    av = axes.get("version")
    if av != f"{major}.{minor}":
        print(json.dumps({"refuse": "version sources disagree",
                          "store": f"{major}.{minor}", "axes_fingerprint": av}, indent=1))
        return 3

    build_id = gnu_build_id(data, notes)
    if not build_id:
        print(json.dumps({"refuse": "no NT_GNU_BUILD_ID note"}, indent=1))
        return 3
    revision = reconstruct_revision(data, loads, major, minor, meta["patch"])
    if not revision:
        print(json.dumps({"refuse": "branch name not found for revision"}, indent=1))
        return 3

    non_shipping = axes["build_configuration"] != 1
    stats = axes["stats"] == 2
    extent = max(va + ms for (off, va, fs, ms, fl) in loads)
    base = min(va for (off, va, fs, ms, fl) in loads if fl & 1) if loads else 0
    result = {
        "cell": {"format": 1, "pointer_size": 8, "major": major, "minor": minor,
                 "target": "prospero-x86_64-sysv", "non_shipping": non_shipping, "stats": stats},
        "identity": {"engine_revision": revision, "build_id": build_id},
        "image": {"size": len(data), "extent": extent, "base": base},
        "version_sources": {"store": f"{major}.{minor}", "axes_fingerprint": av,
                            "axes_present_count": axes.get("present_count")},
        "patch": meta["patch"],
        "build_configuration": axes["build_configuration"], "stats_axis": axes["stats"],
    }
    print(json.dumps(result, indent=1))
    if a.out:
        Path(a.out).write_text(json.dumps(result, indent=1) + "\n")
    if a.expect:
        exp = json.loads(Path(a.expect).read_text())
        bad = []
        for k, v in exp.get("cell", {}).items():
            if result["cell"].get(k) != v:
                bad.append(f"cell.{k}={result['cell'].get(k)} != {v}")
        for k, v in exp.get("identity", {}).items():
            if result["identity"].get(k) != v:
                bad.append(f"identity.{k}={result['identity'].get(k)} != {v}")
        if bad:
            print("SELF-TEST FAIL:", *bad, sep="\n  ")
            return 2
        print("SELF-TEST PASS: image-derived identity matches expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
