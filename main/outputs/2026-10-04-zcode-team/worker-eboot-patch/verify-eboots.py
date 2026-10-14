#!/usr/bin/env python3
"""worker-eboot-patch verify-eboots.py — independent verification of the
worker-early-load patched eboots (A1/A2) BEFORE any deployment work.

Checks, per EARLY-LOAD-DESIGN.md §4 (all expected values re-derived here from
the bytes, NOT copied from the patch transcript):
  1. ELF header validity (magic, ELF64 LE x86-64, OSABI 9, e_type 0xFE10).
  2. Dynamic-table integrity: 261 entries, exactly one DT_NULL at [260],
     DT_NEEDED count, the new DT_NEEDED "mb.prx" at slot 236 with value
     0x8741, DT_STRSZ 0x8748, strtab actually contains "mb.prx\\0" at 0x8741.
  3. Variant-specific Sony metadata triple for A2 (0x61000045/0x61000049/
     0x61000019 at slots 240/257/258).
  4. Byte-level diff vs BOTH local baselines:
       - console-toplevel-eboot.bin (afe0a2a5..., the actual patch base)
       - console-decrypted-eboot.bin (3b6cfad4..., observations copy)
     expecting exactly 10 (A1) / 31 (A2) changed bytes vs the patch base, and
     the same 10/31 plus the known certificate-TLV zeroing vs the decrypted
     copy (5,386 bytes at file offset >= 0xb93c32e, per the design §0).
  5. sha256 + sha1 of both variants.

No console contact. Writes verify-eboots.json + verify-eboots.txt receipts.
"""

import hashlib
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEAM = HERE.parent

PATHS = {
    "base_toplevel": TEAM / "worker-game-version/console-toplevel-eboot.bin",
    "base_decrypted": TEAM / "worker-game-version/console-decrypted-eboot.bin",
    "A1": TEAM / "worker-early-load/eboot-earlyload-A1.bin",
    "A2": TEAM / "worker-early-load/eboot-earlyload-A2.bin",
}

EXPECTED_BASE_SHA = {
    "base_toplevel": "afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e",
    "base_decrypted": "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc",
}

DYN_BASE = 0x0B93B278      # PT_DYNAMIC file offset (261 x 16B entries)
DYN_COUNT = 261
STR_FILE_OFF = 0x930F420   # strtab segment file offset (DT_STRTAB vaddr mirror)
NAME_STROFF = 0x8741       # "mb.prx" offset inside strtab

CERT_REGION_START = 0xB93C32E  # game-root file has this trailing region zeroed

EXPECTED_VARIANT_BYTES = {"A1": 10, "A2": 31}

# slots both variants rewrite
EXPECTED_DYN_COMMON = {
    236: (0x00000001, 0x0000000000008741),   # DT_NEEDED "mb.prx"
    248: (0x0000000A, 0x0000000000008748),   # DT_STRSZ bumped
}
# slots only A2 rewrites (Sony metadata triple)
EXPECTED_DYN_A2 = {
    240: (0x61000045, 0x003A0101_00008741),  # module metadata
    257: (0x61000049, 0x003C0001_00008741),  # lib metadata
    258: (0x61000019, 0x0000000000000001),   # app-lib flag
}
# original (base) values A1 must RETAIN in those slots
ORIGINAL_DYN_A2_SLOTS = {
    240: (0x6FFFFFF9, 0x0000000000192B94),   # DT_RELACOUNT
    257: (0x0000000D, 0x0000000007479AD0),   # DT_FINI
    258: (0x6100003F, 0x000000000000BD90),   # Sony symtab byte-size hint
}


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def parse_elf_header(data):
    # parse field-by-field to avoid sub-slice mistakes
    magic = data[0:4]
    cls = data[4]
    data_enc = data[5]
    version = data[6]
    osabi = data[7]
    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    e_entry, e_phoff = struct.unpack_from("<QQ", data, 24)
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 54)
    return {
        "magic": magic.hex(), "class": cls, "data_encoding": data_enc,
        "version": version, "osabi": osabi, "e_type": e_type,
        "e_machine": e_machine, "e_entry": e_entry, "e_phoff": e_phoff,
        "e_phentsize": e_phentsize, "e_phnum": e_phnum,
    }


def find_dynamic(data):
    """Locate PT_DYNAMIC (p_type 2) from the program headers; return
    (file_offset, size) or fail."""
    h = parse_elf_header(data)
    phoff, phentsize, phnum = h["e_phoff"], h["e_phentsize"], h["e_phnum"]
    for i in range(phnum):
        p_type, p_flags = struct.unpack_from("<II", data, phoff + i * phentsize)
        if p_type == 2:
            p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from(
                "<QQQQ", data, phoff + i * phentsize + 8)
            return p_offset, p_filesz
    return None, None


def dyn_entries(data, dyn_off, count):
    out = []
    for i in range(count):
        tag, val = struct.unpack_from("<qQ", data, dyn_off + 16 * i)
        out.append((tag, val))
    return out


def strtab_read(data, stroff):
    base = STR_FILE_OFF + stroff
    end = data.index(b"\x00", base)
    return data[base:end].decode("ascii", "replace")


def diff_runs(a, b):
    """Return (n_changed_bytes, [(offset, a_byte, b_byte), ...], min_off)."""
    assert len(a) == len(b)
    diffs = [(i, a[i], b[i]) for i in range(len(a)) if a[i] != b[i]]
    return len(diffs), diffs, (diffs[0][0] if diffs else -1)


def verify_variant(name, data, bases):
    r = {"name": name, "bytes": len(data), "sha256": sha256(data),
         "sha1": sha1(data)}
    # 1. ELF header
    h = parse_elf_header(data)
    r["elf"] = h
    r["checks"] = {}
    r["checks"]["elf_magic_ok"] = h["magic"] == "7f454c46"
    r["checks"]["elf64_le_x86_64"] = (h["class"] == 2 and h["data_encoding"] == 1
                                      and h["e_machine"] == 0x3E)
    r["checks"]["osabi_freebsd_sony"] = h["osabi"] == 9
    r["checks"]["etype_sce_dynamic_exec"] = h["e_type"] == 0xFE10

    # 2. dynamic table via program headers (independent of constants above)
    dyn_off, dyn_sz = find_dynamic(data)
    r["pt_dynamic"] = {"file_offset": hex(dyn_off) if dyn_off else None,
                       "size": dyn_sz}
    r["checks"]["pt_dynamic_offset_matches_design"] = dyn_off == DYN_BASE
    r["checks"]["pt_dynamic_size_261_entries"] = dyn_sz == DYN_COUNT * 16
    ents = dyn_entries(data, dyn_off, DYN_COUNT)

    nulls = [i for i, (t, _) in enumerate(ents) if t == 0]
    needed = [i for i, (t, _) in enumerate(ents) if t == 1]
    r["dyn"] = {"dt_needed_count": len(needed), "dt_null_slots": nulls}
    r["checks"]["single_dt_null_last"] = nulls == [DYN_COUNT - 1]
    r["checks"]["dt_needed_count_58"] = len(needed) == 58
    r["checks"]["needed_last_slot_236"] = max(needed) == 236

    for slot, (want_tag, want_val) in EXPECTED_DYN_COMMON.items():
        got_tag, got_val = ents[slot]
        key = f"dyn{slot}_tag_0x{want_tag:x}"
        r["checks"][key] = (got_tag == want_tag and got_val == want_val)

    if name == "A2":
        for slot, (want_tag, want_val) in EXPECTED_DYN_A2.items():
            got_tag, got_val = ents[slot]
            key = f"dyn{slot}_tag_0x{want_tag:x}"
            r["checks"][key] = (got_tag == want_tag and got_val == want_val)
    else:  # A1 must leave the A2-only slots at their ORIGINAL values
        for slot, (want_tag, want_val) in ORIGINAL_DYN_A2_SLOTS.items():
            got_tag, got_val = ents[slot]
            key = f"dyn{slot}_original_retained_0x{want_tag:x}"
            r["checks"][key] = (got_tag == want_tag and got_val == want_val)

    # strtab name + DT_STRSZ
    strtab_name = strtab_read(data, NAME_STROFF)
    r["strtab@0x8741"] = strtab_name
    r["checks"]["strtab_name_is_mb.prx"] = strtab_name == "mb.prx"
    strsz = [v for t, v in ents if t == 0x0A][0]
    r["dt_strsz"] = hex(strsz)
    r["checks"]["dt_strsz_0x8748"] = strsz == 0x8748
    # the 7 bytes after the name are still inside strtab and the following
    # symtab must be untouched: strtab segment bytes 0x8741..0x8748 == mb.prx\0
    seg = data[STR_FILE_OFF + NAME_STROFF: STR_FILE_OFF + NAME_STROFF + 7]
    r["checks"]["name_bytes_exact"] = seg == b"mb.prx\x00"

    # 3. byte diffs vs both bases
    for bname, bdata in bases.items():
        n, diffs, minoff = diff_runs(bdata, data)
        entry = {"changed_bytes": n, "min_offset": hex(minoff) if diffs else None}
        if bname == "base_toplevel":
            entry["expected"] = EXPECTED_VARIANT_BYTES[name]
            entry["match"] = (n == EXPECTED_VARIANT_BYTES[name])
        else:  # decrypted copy: variant delta + cert-TLV zeroing
            variant_n = EXPECTED_VARIANT_BYTES[name]
            cert_only = [d for d in diffs if d[0] < CERT_REGION_START]
            entry["expected"] = (
                f"{variant_n} patch bytes + cert-TLV zeroed bytes "
                f"(design: 5,386 at >= 0x{CERT_REGION_START:x})")
            entry["pre_cert_region_changed_bytes"] = len(cert_only)
            entry["match"] = (len(cert_only) == variant_n
                              and minoff < CERT_REGION_START)
        r[f"diff_vs_{bname}"] = entry

    # enumerate the changed offsets vs the patch base (human-readable receipt)
    _, diffs, _ = diff_runs(bases["base_toplevel"], data)
    r["changed_vs_base"] = [
        {"file_offset": hex(o), "base": hex(a), "patched": hex(b)}
        for o, a, b in diffs
    ]
    ok = all(r["checks"].values()) and \
        r["diff_vs_base_toplevel"]["match"] and \
        r["diff_vs_base_decrypted"]["match"]
    r["result"] = "PASS" if ok else "FAIL"
    return r


def main():
    out = {"tool": "verify-eboots.py", "console_contact": False}
    bases = {}
    for bname, bpath in (("base_toplevel", PATHS["base_toplevel"]),
                         ("base_decrypted", PATHS["base_decrypted"])):
        data = bpath.read_bytes()
        got = sha256(data)
        assert got == EXPECTED_BASE_SHA[bname], f"{bname} sha mismatch {got}"
        bases[bname] = data
    out["base_sha256_confirmed"] = EXPECTED_BASE_SHA

    results = []
    for variant in ("A1", "A2"):
        results.append(verify_variant(variant, PATHS[variant].read_bytes(),
                                      bases))
    out["variants"] = results

    (HERE / "verify-eboots.json").write_text(json.dumps(out, indent=1))

    lines = [f"worker-eboot-patch eboot verification — console contact: false"]
    for bname, sha in EXPECTED_BASE_SHA.items():
        lines.append(f"base {bname}: sha256 {sha} CONFIRMED")
    for r in results:
        lines.append("=" * 72)
        lines.append(f"{r['name']}: {r['bytes']} bytes  "
                     f"sha256 {r['sha256']}  sha1 {r['sha1']}")
        lines.append(f"  result: {r['result']}")
        lines.append(f"  elf: magic={r['elf']['magic']} class={r['elf']['class']}"
                     f" enc={r['elf']['data_encoding']} osabi={r['elf']['osabi']}"
                     f" e_type=0x{r['elf']['e_type']:x}"
                     f" machine=0x{r['elf']['e_machine']:x}"
                     f" phnum={r['elf']['e_phnum']}")
        lines.append(f"  PT_DYNAMIC @0x{r['pt_dynamic']['file_offset']}"
                     f" size={r['pt_dynamic']['size']}"
                     f" ({r['dyn']['dt_needed_count']} DT_NEEDED,"
                     f" DT_NULL slots {r['dyn']['dt_null_slots']})")
        lines.append(f"  strtab@0x8741 = {r['strtab@0x8741']!r}"
                     f"  DT_STRSZ={r['dt_strsz']}")
        for k, v in r["checks"].items():
            if not v:
                lines.append(f"  FAILED CHECK: {k}")
        for bname in ("base_toplevel", "base_decrypted"):
            d = r[f"diff_vs_{bname}"]
            lines.append(f"  diff vs {bname}: {d['changed_bytes']} bytes"
                         f" (min offset {d['min_offset']}) — expected"
                         f" {d['expected']} — match={d['match']}")
        lines.append("  changed bytes vs patch base:")
        for c in r["changed_vs_base"]:
            lines.append(f"    {c['file_offset']}: {c['base']} -> {c['patched']}")
    (HERE / "verify-eboots.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
    overall = all(r["result"] == "PASS" for r in results)
    print("OVERALL:", "PASS" if overall else "FAIL")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
