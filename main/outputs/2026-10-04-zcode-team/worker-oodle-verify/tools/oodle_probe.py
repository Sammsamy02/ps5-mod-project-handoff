#!/usr/bin/env python3
"""worker-oodle-verify: definitive Oodle-necessity probe over retained game containers.

Read-only. Bounded: reads utoc TOC regions (not ucas payloads except none), pak footers,
one pak index blob. No writes outside worker-oodle-verify/.
"""
import json, struct, hashlib, os, sys

ROOT = "/WORKSPACE/.codex/worktrees/translation-core/what"
OUT = os.path.join(ROOT, "outputs/2026-10-04-zcode-team/worker-oodle-verify")
PAK_MAGIC = 0x5A6F12E1
TOC_MAGIC = b"-==-" * 4  # 2d 3d 3d 2d repeated x4

def rd(rel):
    with open(os.path.join(ROOT, rel), "rb") as f:
        return f.read()

def sha(b):
    return hashlib.sha256(b).hexdigest()

def utoc_probe(rel):
    data = rd(rel)
    r = {"target": rel, "bytes": len(data), "file_sha256": sha(data)}
    if data[:16] != TOC_MAGIC:
        r["magic_ok"] = False
        return r
    r["magic_ok"] = True
    (hdr_size, entry_count, block_count, be_size, name_count, name_len,
     blk_sz, dir_size, part_count) = struct.unpack_from("<9I", data, 20)
    seeds = struct.unpack_from("<I", data, 84)[0]
    without = struct.unpack_from("<I", data, 96)[0]
    flags = data[80]
    iv_count = struct.unpack_from("<I", data, 104)[0] if data[16] >= 10 else 0
    r["version"] = data[16]
    r["toc_entry_count"] = entry_count
    r["block_entry_count"] = block_count
    r["method_name_count"] = name_count
    r["compression_block_size"] = blk_sz
    r["partition_count"] = part_count
    # region walk per worker-container-headers S2/S3 (byte-exact EOF-validated)
    pos = hdr_size
    pos += 12 * entry_count      # FIoChunkId x12
    pos += 10 * entry_count      # offset/length x10
    pos += 4 * seeds + 4 * without
    blocks_off = pos
    r["compression_blocks_offset"] = blocks_off
    pos += 12 * block_count
    pos += 12 * iv_count
    methods_off = pos
    names = []
    if name_count and name_len:
        raw = data[methods_off:methods_off + name_count * name_len]
        names = [raw[i*name_len:(i+1)*name_len].split(b"\0")[0].decode("latin-1") for i in range(name_count)]
    r["compression_method_names"] = names
    pos += name_count * name_len
    if flags & 4:
        hs = struct.unpack_from("<i", data, pos)[0]
        pos += 4 + 2 * hs + 20 * block_count
    if (data[16] >= 2) and (flags & 8) and dir_size > 0:
        pos += dir_size
    pos += 24 * entry_count if data[16] >= 8 else 0
    r["structural_eof_match"] = (pos == len(data))
    r["unaccounted_residual_bytes"] = len(data) - pos
    table = ["None"] + names
    meth_set, sizes_sum, bad = set(), 0, 0
    offs, uszs, spans = [], [], []
    for i in range(block_count):
        o = blocks_off + 12 * i
        a, b, c = struct.unpack_from("<III", data, o)   # layout: offset | (disk_size<<8|method) | uncompressed
        meth_set.add(b & 0xFF)                          # TRUE method byte
        sizes_sum += c
        offs.append(a); uszs.append(c); spans.append(b >> 8)
    r["layout"] = "[u32 block_offset][u32 disk_size<<8|method_byte][u32 uncompressed_size]"
    r["referenced_method_indices"] = sorted(meth_set)
    r["referenced_methods_resolved"] = [table[m] if m < len(table) else f"index-{m}-beyond-table" for m in sorted(meth_set)]
    r["sum_uncompressed_sizes"] = sizes_sum
    r["sum_disk_block_sizes"] = sum(spans)
    r["block_offsets_monotonic_nondecreasing"] = all(offs[i] <= offs[i+1] for i in range(len(offs) - 1))
    r["blocks_disk_ne_uncompressed"] = sum(1 for s, u in zip(spans, uszs) if s != u)
    r["blocks_disk_lt_uncompressed"] = sum(1 for s, u in zip(spans, uszs) if s < u)
    return r

def ucas_check(rel_utoc, rel_ucas):
    """True pairing check: block offsets/spans vs actual ucas size."""
    u = utoc_probe(rel_utoc)
    ucas_size = os.path.getsize(os.path.join(ROOT, rel_ucas))
    data = rd(rel_utoc)
    blocks_off = u["compression_blocks_offset"]
    offs, uszs, disks, meths = [], [], [], []
    for i in range(u["block_entry_count"]):
        a, b, c = struct.unpack_from("<III", data, blocks_off + 12 * i)
        offs.append(a); uszs.append(c); disks.append(b >> 8); meths.append(b & 0xFF)
    return {
        "utoc": rel_utoc, "ucas": rel_ucas, "ucas_bytes": ucas_size,
        "blocks": len(offs), "methods_present": sorted(set(meths)),
        "first_block_offset": offs[0] if offs else None,
        "sum_disk_block_sizes": sum(disks),
        "sum_uncompressed_sizes": sum(uszs),
        "end_of_last_block": offs[-1] + disks[-1] if offs else None,
        "blocks_fit_ucas": (offs[-1] + disks[-1] == ucas_size) if offs else False,
        "sum_disk_eq_ucas": sum(disks) == ucas_size,
        "sum_uncompressed_eq_ucas": sum(uszs) == ucas_size,
        "blocks_disk_lt_uncompressed": sum(1 for d, s in zip(disks, uszs) if d < s),
        "blocks_disk_ne_uncompressed": sum(1 for d, s in zip(disks, uszs) if d != s),
    }

def pak_footer(rel):
    p = os.path.join(ROOT, rel)
    size = os.path.getsize(p)
    with open(p, "rb") as f:
        tail_start = max(0, size - 512)
        f.seek(tail_start)
        tail = f.read()
    pos = tail.find(struct.pack("<I", PAK_MAGIC))
    r = {"target": rel, "bytes": size}
    if pos < 0:
        r["pak_magic_found"] = False
        return r
    magic_off = tail_start + pos
    r["pak_magic_found"] = True
    r["footer_offset"] = magic_off
    ver = struct.unpack_from("<I", tail, pos + 4)[0]
    r["version"] = ver
    idx_off, idx_size = struct.unpack_from("<QQ", tail, pos + 8)
    idx_sha = tail[pos + 24:pos + 44].hex()
    slots_raw = tail[pos + 44:pos + 44 + 160]
    slots = [slots_raw[i*32:(i+1)*32].split(b"\0")[0].decode("latin-1") for i in range(5)]
    r["index_offset"] = idx_off
    r["index_size"] = idx_size
    r["index_hash_sha1"] = idx_sha
    r["method_slots"] = slots
    r["method_slots_all_zero"] = all(s == "" for s in slots)
    return r

def pak_index_sum(rel):
    """Sum 12B entry size fields from the saved main pak index; compare vs payload span."""
    fp = pak_footer(rel)
    if not fp.get("pak_magic_found"):
        return {"target": rel, "error": "no footer"}
    with open(os.path.join(ROOT, rel), "rb") as f:
        f.seek(fp["index_offset"])
        idx = f.read(fp["index_size"])
    mount_len = struct.unpack_from("<I", idx, 0)[0] + 1
    entry_count = struct.unpack_from("<I", idx, 14)[0]  # byte-proved at byte 14 per worker-pak-index
    # layout per worker-pak-index (byte-proved): mount FString [0,14), entry_count@14, region_table [18,114), entries_blob [114, index_size)
    entries_off = 114
    sizes, offs, flags = [], [], set()
    for i in range(entry_count):
        o, s, fl = struct.unpack_from("<III", idx, entries_off + 12 * i)
        offs.append(o); sizes.append(s); flags.add(fl)
    span_end = max(o + s for o, s in zip(offs, sizes))
    span_start = min(offs)
    return {
        "target": rel, "entry_count": entry_count,
        "distinct_flags": sorted(hex(v) for v in flags),
        "sum_uncompressed_sizes": sum(sizes),
        "payload_span_start": span_start, "payload_span_end": span_end,
        "payload_span_bytes": span_end - span_start,
        "index_start": fp["index_offset"],
        "note": "if sum(sizes) ~= span, payload stored uncompressed",
    }

def main():
    ev = {"utc_probe_basis": "layout derived from worker-container-headers tool + worker-pak-index decode; validated by structural coherence below"}
    utocs = [
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk1-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk10-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk11-ps5.utoc",
        ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk12-ps5.utoc",
        ".superpowers/native-observations/townfall-manifest-capture-2026-10-01/global.utoc",
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc",
        ".superpowers/native-dependency-spike/leon-iostore-adapter-4k-trial-1/source/zzz_Leon_Townfall_4K_P.utoc",
        ".superpowers/native-dependency-spike/townfall-62-rar-intake-1/bundle-input/1_999_P.utoc",
        ".superpowers/native-dependency-spike/controller-bundle-adapter-1/trial-1/bundle/1_999_P.utoc",
    ]
    ev["utocs"] = [utoc_probe(u) for u in utocs]
    ev["ucas_checks"] = [
        ucas_check(utocs[5], ".superpowers/native-observations/townfall-global-scriptobjects-1/global.ucas"),
        ucas_check(utocs[6], ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas"),
        ucas_check(utocs[7], ".superpowers/native-dependency-spike/leon-iostore-adapter-4k-trial-1/source/zzz_Leon_Townfall_4K_P.ucas"),
        ucas_check(utocs[8], ".superpowers/native-dependency-spike/townfall-62-rar-intake-1/bundle-input/1_999_P.ucas"),
        ucas_check(utocs[9], ".superpowers/native-dependency-spike/controller-bundle-adapter-1/trial-1/bundle/1_999_P.ucas"),
    ]
    paks = [
        ".superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak",
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.pak",
        ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/base-pak-metadata/townfall-pakchunk0-metadata-only.pak",
        ".superpowers/native-dependency-spike/leon-iostore-adapter-4k-trial-1/source/zzz_Leon_Townfall_4K_P.pak",
        ".superpowers/native-dependency-spike/townfall-62-rar-intake-1/bundle-input/1_999_P.pak",
        ".superpowers/native-dependency-spike/controller-bundle-adapter-1/trial-1/bundle/1_999_P.pak",
        ".superpowers/native-observations/townfall-extra-pak-test-2026-10-01/pakchunk9999-ModBridgeOriginals_6ec975d1-61fc-43dc-9ca9-3475ed94d6d8_P.pak",
        ".superpowers/native-observations/townfall-extra-pak-test-2026-10-01/diagnostic-render-control/pakchunk9999-ModBridgeRenderDiag_a6d6f620-2878-494b-a0cb-45e4486fdb44_P.pak",
        # positive control: repak fixture known to use Zlib compression
        ".superpowers/native-dependency-spike/repak-independent-fixture-research-1/source/repak/tests/packs/pack_v11_compress.pak",
        # negative control: repak fixture known uncompressed
        ".superpowers/native-dependency-spike/repak-independent-fixture-research-1/source/repak/tests/packs/pack_v11.pak",
    ]
    ev["pak_footers"] = [pak_footer(p) for p in paks]
    ev["pak_index_sum_main"] = pak_index_sum(paks[0])
    os.makedirs(os.path.join(OUT, "evidence"), exist_ok=True)
    with open(os.path.join(OUT, "evidence", "oodle-probe.json"), "w") as f:
        json.dump(ev, f, indent=1)
    # console summary
    for u in ev["utocs"]:
        print("UTOC", u["target"].split("/")[-1], "| methods:", u.get("referenced_methods_resolved"),
              "| names:", u.get("compression_method_names"),
              "| eof_match:", u.get("structural_eof_match"), "residual:", u.get("unaccounted_residual_bytes"),
              "| mono:", u.get("block_offsets_monotonic_nondecreasing"),
              "| sum_disk:", u.get("sum_disk_block_sizes"), "sum_usz:", u.get("sum_uncompressed_sizes"),
              "| disk<usz:", u.get("blocks_disk_lt_uncompressed"), "disk!=usz:", u.get("blocks_disk_ne_uncompressed"))
    for c in ev["ucas_checks"]:
        print("UCAS", c["ucas"].split("/")[-1], "methods:", c["methods_present"],
              "ucas_bytes:", c["ucas_bytes"], "end_last:", c["end_of_last_block"],
              "fits:", c["blocks_fit_ucas"], "sum_disk==ucas:", c["sum_disk_eq_ucas"],
              "sum_usz==ucas:", c["sum_uncompressed_eq_ucas"],
              "disk<usz blocks:", c["blocks_disk_lt_uncompressed"],
              "disk!=usz blocks:", c["blocks_disk_ne_uncompressed"])
    for p in ev["pak_footers"]:
        print("PAK", p["target"].split("/")[-1], "ver:", p.get("version"), "slots:", p.get("method_slots"), "all_zero:", p.get("method_slots_all_zero"))
    print("INDEX_SUM", json.dumps(ev["pak_index_sum_main"]))

if __name__ == "__main__":
    main()
