#!/usr/bin/env python3
"""Complete IoStore .utoc header probe — worker-container-headers v2 (2026-10-04).

Corrects worker-6's magic-only dispatch: `-==--==--==--==' is the GENERIC IoStore
TOC magic (CUE4Parse FIoStoreTocHeader.TOC_MAGIC; retoc runner checks data[:16]).
This probe parses the COMPLETE 144-byte header + trailing array region of every
saved .utoc in the worker-6 corpus and emits a supported/unsupported verdict with
the exact field values that decide it. Read-only on corpus bytes.

Field layout citations (retained sources, see ../HEADER-SPEC.md):
  S1 CUE4Parse .../UE4/IO/Objects/FIoStoreTocHeader.cs   (header order, SIZE=144)
  S2 CUE4Parse .../UE4/IO/Objects/FIoStoreTocResource.cs (array order + gates)
  S3 CUE4Parse .../UE4/IO/Objects/{FIoChunkId,FIoOffsetAndLength,
     FIoStoreTocCompressedBlockEntry,FIoStoreTocEntryMeta,FIoDirectoryIndexEntry,
     FIoFileIndexEntry}.cs + IO/FIoStoreEncryptionIV.cs
  S4 retoc-v0.1.5-raw-roundtrip-1/repack_runner.py (version@16, container_id@56:64)
  S5 townfall-manifest-capture-2026-10-01/container-header-receipt.json (v8, 144)
  S6 townfall-original-zen-headers-1/profile.json (pakchunk0 cross-check offsets)

Usage: utoc_header_probe.py [--selftest] [files...]
"""
import datetime
import hashlib
import json
import os
import struct
import sys

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
OUT_DIR = os.path.join(WS, "outputs/2026-10-04-zcode-team/worker-container-headers")

MAGIC = b"-==--==--==--==-"          # S1 TOC_MAGIC (16 bytes)
HEADER_SIZE_EXPECTED = 144            # S1 SIZE; S5/S6 observed
BLOCK_ENTRY_SIZE_EXPECTED = 12        # S3 FIoStoreTocCompressedBlockEntry.SIZE
SHA1_SIZE = 20                        # S3 FShaHash.SIZE used in S2 signatures region
IV_SIZE = 12                          # S3 FIoStoreEncryptionIV.Size

VERSION_NAMES = {                     # S1 EIoStoreTocVersion (verbatim values)
    0: "Invalid", 1: "Initial", 2: "DirectoryIndex", 3: "PartitionSize",
    4: "PerfectHash", 5: "PerfectHashWithOverflow", 6: "OnDemandMetaData",
    7: "RemovedOnDemandMetaData", 8: "ReplaceIoChunkHashWithIoHash",
    9: "AddedSourceHashes", 10: "ContainerEncryptionMethod",
}
VERSION_LATEST = 10                   # S1 Latest
FLAG_NAMES = {1: "Compressed", 2: "Encrypted", 4: "Signed", 8: "Indexed", 16: "OnDemand"}

# Admission set (profile v2): version identity + structure actually proved on this
# corpus by receipts (retoc verify exit 0 / byte roundtrip) — NOT a magic claim.
ADMIT_VERSIONS = {8}
# Codecs our pinned stack can decode: zlib 1.3.2 proved (capability matrix §7);
# Oodle has no decoder anywhere in the workspace.
ADMIT_METHODS = {"None", "Zlib"}

CORPUS = [
    ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc",
    ".superpowers/native-observations/townfall-manifest-capture-2026-10-01/global.utoc",
    ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc",
    ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk1-ps5.utoc",
    ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk10-ps5.utoc",
    ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk11-ps5.utoc",
    ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk12-ps5.utoc",
    ".superpowers/native-dependency-spike/townfall-62-rar-intake-1/bundle-input/1_999_P.utoc",
]

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def parse_header(data):
    """Complete FIoStoreTocHeader per S1 (<UE6_0 reading; UE6 alternative reported)."""
    if len(data) < HEADER_SIZE_EXPECTED:
        return None, "file smaller than 144-byte header"
    h = {}
    h["toc_magic_hex"] = data[0:16].hex()
    h["magic_ok"] = data[0:16] == MAGIC
    h["version"] = data[16]                      # S1/S4/S5
    h["version_name"] = VERSION_NAMES.get(h["version"], "unmapped")
    h["reserved0"] = data[17]                    # S1 (unread)
    h["reserved1"] = struct.unpack_from("<H", data, 18)[0]
    (h["toc_header_size"], h["toc_entry_count"], h["toc_compressed_block_entry_count"],
     h["toc_compressed_block_entry_size"], h["compression_method_name_count"],
     h["compression_method_name_length"], h["compression_block_size"],
     h["directory_index_size"], h["partition_count"]) = struct.unpack_from("<9I", data, 20)
    h["container_id"] = struct.unpack_from("<Q", data, 56)[0]      # S4 56:64
    h["container_id_hex"] = data[56:64].hex()
    h["encryption_key_guid_hex"] = data[64:80].hex()               # 16B by closure (S1)
    h["container_flags_raw"] = data[80]                            # S1
    h["container_flags"] = [n for b, n in FLAG_NAMES.items() if data[80] & b]
    h["encryption_method_field"] = data[81]                        # v10+ serialized (S1)
    h["reserved4"] = struct.unpack_from("<H", data, 82)[0]
    h["toc_chunk_perfect_hash_seeds_count"] = struct.unpack_from("<I", data, 84)[0]
    h["partition_size"] = struct.unpack_from("<Q", data, 88)[0]
    h["toc_chunks_without_perfect_hash_count"] = struct.unpack_from("<I", data, 96)[0]
    # S2 lines 76-80: for version < 10 the method is derived, IV count is 0
    if h["version"] < 10:
        h["encryption_method_effective"] = "AES" if (data[80] & 2) else "None"
        h["encryption_iv_count_effective"] = 0
    else:
        h["encryption_method_effective"] = {0: "None", 1: "AES", 2: "AES_CTR"}.get(
            data[81], f"unknown({data[81]})")
        h["encryption_iv_count_effective"] = struct.unpack_from("<I", data, 104)[0]  # UE6 reading
    h["trailing_words_ue5_reading_u64"] = [struct.unpack_from("<Q", data, 104 + 8 * i)[0]
                                           for i in range(5)]     # S1 <UE6_0 CustomGameData
    h["reserved7"] = struct.unpack_from("<I", data, 100)[0]
    return h, None

def parse_region(data, h):
    """Trailing array region walk per S2; returns layout + compression census."""
    r = {}
    v, e = h["version"], h["toc_entry_count"]
    pos = h["toc_header_size"]
    r["chunk_ids_offset"] = pos
    pos += 12 * e                                                   # S3 FIoChunkId=12
    r["chunk_offset_lengths_offset"] = pos
    pos += 10 * e                                                   # S3 =10
    r["perfect_hash_seeds_count"] = 0
    r["chunks_without_perfect_hash_count"] = 0
    if v >= 5:                                                      # S2 PerfectHashWithOverflow
        r["perfect_hash_seeds_count"] = h["toc_chunk_perfect_hash_seeds_count"]
        r["chunks_without_perfect_hash_count"] = h["toc_chunks_without_perfect_hash_count"]
    elif v >= 4:                                                    # S2 PerfectHash
        r["perfect_hash_seeds_count"] = h["toc_chunk_perfect_hash_seeds_count"]
    pos += 4 * r["perfect_hash_seeds_count"] + 4 * r["chunks_without_perfect_hash_count"]
    r["compression_blocks_offset"] = pos
    blocks = h["toc_compressed_block_entry_count"]
    pos += 12 * blocks                                              # S3 =12
    r["encryption_ivs_offset"] = pos
    pos += IV_SIZE * h["encryption_iv_count_effective"]             # S3 IV=12
    r["compression_methods_offset"] = pos
    n, l = h["compression_method_name_count"], h["compression_method_name_length"]
    names_raw = data[pos:pos + n * l]
    pos += n * l
    r["compression_method_names"] = [names_raw[i * l:(i + 1) * l].split(b"\0")[0].decode("latin-1")
                                     for i in range(n)] if n and l else []
    r["signatures_offset"] = pos
    r["signature_hash_size"] = None
    if h["container_flags_raw"] & 4:                                # S2 Signed
        if pos + 4 > len(data):
            return r, "signed region out of bounds"
        r["signature_hash_size"] = struct.unpack_from("<i", data, pos)[0]
        pos += 4 + 2 * r["signature_hash_size"] + SHA1_SIZE * blocks
    r["directory_index_offset"] = pos
    indexed = bool(h["container_flags_raw"] & 8)
    r["directory_index_present"] = bool(v >= 2 and indexed and h["directory_index_size"] > 0)
    if r["directory_index_present"]:
        pos += h["directory_index_size"]
    r["meta_offset"] = pos
    meta_entry_size = 24 if v >= 8 else None   # S3: v>=8 SHA(20)+flags(1)+3; else unknown FIoChunkHash
    if meta_entry_size:
        r["meta_entry_size"] = meta_entry_size
        pos += meta_entry_size * e
    if v == 6 and (h["container_flags_raw"] & 16):                  # S2 OnDemand v6
        pos += SHA1_SIZE * e + SHA1_SIZE * blocks
    r["computed_end"] = pos
    r["file_size"] = len(data)
    r["structural_eof_match"] = pos == len(data)
    r["unaccounted_residual_bytes"] = len(data) - pos
    # compression-method census over blocks (S3: method index = high byte of last u32)
    r["referenced_method_indices"] = []
    if blocks and r["compression_blocks_offset"] + 12 * blocks <= len(data):
        idxs = set()
        for i in range(blocks):
            u32 = struct.unpack_from("<I", data, r["compression_blocks_offset"] + 12 * i + 8)[0]
            idxs.add(u32 >> 24)
        r["referenced_method_indices"] = sorted(idxs)
    table = ["None"] + r["compression_method_names"]                # S2: slot 0 is None
    r["referenced_methods_resolved"] = [table[i] if i < len(table) else f"index-{i}-beyond-table"
                                        for i in r["referenced_method_indices"]]
    return r, None

def verdict(h, r, ucas_present, wrapper_pak):
    """Verdict from COMPLETE parsed values, never magic alone."""
    reasons = []
    if not h["magic_ok"]:
        return "unsupported", [f"toc-magic:{h['toc_magic_hex']} (expected generic IoStore "
                               f"'-==-' x4, 0x2d3d3d2d x4)"], []
    if h["version"] == 0 or h["version"] > VERSION_LATEST:
        return "unsupported", [f"toc-version:{h['version']} outside enum 1..{VERSION_LATEST} (S1)"], []
    if h["toc_header_size"] != HEADER_SIZE_EXPECTED:
        reasons.append(f"toc-header-size:{h['toc_header_size']} (expected 144, S1/S5/S6)")
    if h["toc_compressed_block_entry_size"] != BLOCK_ENTRY_SIZE_EXPECTED:
        reasons.append(f"toc-block-entry-size:{h['toc_compressed_block_entry_size']} (expected 12, S3)")
    if h["version"] not in ADMIT_VERSIONS:
        reasons.append(f"toc-version:{h['version']} ({h['version_name']}) outside admission set "
                       f"{sorted(ADMIT_VERSIONS)} — no receipt proves this version's structure")
    if "Encrypted" in h["container_flags"]:
        reasons.append(f"encrypted-container (flags={h['container_flags']}, no key)")
    if "Signed" in h["container_flags"]:
        reasons.append("signed-container (no signature-verification capability, S2 skips verify)")
    if "OnDemand" in h["container_flags"] or h["version"] == 6:
        reasons.append("ondemand-metadata (version-6 semantics, no decoder)")
    bad = [m for m in r["referenced_methods_resolved"] if m not in ADMIT_METHODS]
    if bad:
        reasons.append(f"compression-referenced:{','.join(sorted(set(bad)))} "
                       f"(admission {sorted(ADMIT_METHODS)}; Oodle has no decoder)")
    if not r["structural_eof_match"]:
        reasons.append(f"structure-mismatch: header arrays account to byte {r['computed_end']} "
                       f"but file is {r['file_size']} (residual {r['unaccounted_residual_bytes']})")
    if reasons:
        return "unsupported", reasons, []
    warnings = []
    unresolved_table = [m for m in r["compression_method_names"] if m not in ADMIT_METHODS]
    if unresolved_table:
        warnings.append(f"name-table lists {unresolved_table} but no block references it")
    if not r["directory_index_present"]:
        warnings.append(f"no-directory-index (Indexed={'Indexed' in h['container_flags']}, "
                        f"dir_size={h['directory_index_size']}) — filename mapping unavailable")
    if h["partition_count"] != 1:
        warnings.append(f"partition-count:{h['partition_count']} (multi-partition unproved)")
    if ucas_present:
        return "supported", [], warnings
    return "supported-header-only", ["missing-ucas-sibling (chunk payloads not saved; retoc verify "
                                     "panics file-not-found per S7)"], warnings

def probe(rel):
    path = os.path.join(WS, rel)
    if not os.path.exists(path):
        return {"target": rel, "present": False}
    with open(path, "rb") as f:
        data = f.read()
    res = {"target": rel, "bytes": len(data), "file_sha256": sha256_file(path)}
    h, err = parse_header(data)
    if err:
        res["parse_error"] = err
        res["verdict"] = "unsupported"
        res["reasons"] = [err]
        return res
    res["header"] = h
    r, rerr = parse_region(data, h)
    if rerr:
        res["region_parse_error"] = rerr
        res["verdict"] = "unsupported"
        res["reasons"] = [rerr]
        return res
    res["region"] = r
    stem = path[:-5]
    res["siblings"] = {"ucas": os.path.exists(stem + ".ucas"),
                       "wrapper_pak": os.path.exists(stem + ".pak")}
    v, reasons, warnings = verdict(h, r, res["siblings"]["ucas"], res["siblings"]["wrapper_pak"])
    res["verdict"], res["reasons"], res["warnings"] = v, reasons, warnings
    res["version_identity"] = (f"magic={h['toc_magic_hex']} version={h['version']}"
                               f"({h['version_name']}) header_size={h['toc_header_size']}")
    if rel.endswith("pakchunk0-ps5.utoc"):                         # S6 cross-check
        res["s6_crosscheck"] = {
            "chunk_offset_lengths_offset": {"receipt": 713880, "parsed": r["chunk_offset_lengths_offset"],
                                            "match": r["chunk_offset_lengths_offset"] == 713880},
            "compression_blocks_offset": {"receipt": 1427616, "parsed": r["compression_blocks_offset"],
                                          "match": r["compression_blocks_offset"] == 1427616},
            "partition_count": {"receipt": 1, "parsed": h["partition_count"],
                                "match": h["partition_count"] == 1},
        }
    return res

def make_synthetic(version=8, magic=MAGIC, e=2, blocks=1, names=(b"Zlib",), flags=8,
                   dir_size=8, iv_count=0, ref_method_index=0):
    h = magic + bytes([version, 0]) + struct.pack("<H", 0)
    h += struct.pack("<9I", 144, e, blocks, 12, len(names), 32, 65536, dir_size, 1)
    h += struct.pack("<Q", 0x1122334455667788) + b"\x11" * 16
    h += bytes([flags, 0]) + struct.pack("<H", 0)
    h += struct.pack("<I", 0) + struct.pack("<Q", 0) + struct.pack("<I", 0)
    h += struct.pack("<I", 0) + b"\x00" * 40
    body = b"\x00" * (12 * e) + b"\x00" * (10 * e)
    # S2: seeds/without arrays are present only when their header counts are > 0
    # (synthetic header counts are 0, so no bytes are appended here)
    for i in range(blocks):
        body += struct.pack("<Q", 0) + struct.pack("<I", (ref_method_index + i) << 24 | 64)
    body += b"\x00" * (12 * iv_count)
    for n in names:
        body += n.ljust(32, b"\x00")
    body += b"\x00" * dir_size
    if version >= 8:
        body += b"\x00" * (24 * e)
    return h + body

def selftest():
    cases = []
    ok = make_synthetic()
    h, _ = parse_header(ok)
    r, _ = parse_region(ok, h)
    cases.append(("clean v8 synthetic", *verdict(h, r, True, True)[0:2],
                  r["structural_eof_match"] is True))
    bad = make_synthetic(magic=b"\xA9\xA5\xEC\xCA" * 4)
    h2, _ = parse_header(bad)
    r2, _ = parse_region(bad, h2)
    v2 = verdict(h2, r2, True, True)
    cases.append(("wrong magic -> exact reason", v2[0], v2[1], "toc-magic" in v2[1][0]))
    v11 = make_synthetic(version=11, e=0, blocks=0, dir_size=0)
    h3, _ = parse_header(v11)
    r3, _ = parse_region(v11, h3)
    v3 = verdict(h3, r3, True, True)
    cases.append(("version 11 -> version reason", v3[0], v3[1],
                  any("toc-version" in x for x in v3[1])))
    oodle = make_synthetic(names=(b"Oodle",), ref_method_index=1)  # slot 1 = first named method
    h4, _ = parse_header(oodle)
    r4, _ = parse_region(oodle, h4)
    v4 = verdict(h4, r4, True, True)
    cases.append(("Oodle referenced -> compression reason", v4[0], v4[1],
                  any("compression-referenced:Oodle" in x for x in v4[1])))
    trunc = ok[:len(ok) - 10]
    h5, _ = parse_header(trunc)
    r5, _ = parse_region(trunc, h5)
    v5 = verdict(h5, r5, True, True)
    cases.append(("truncated -> structure reason", v5[0], v5[1],
                  any("structure-mismatch" in x for x in v5[1])))
    return cases

def main():
    if "--selftest" in sys.argv:
        for name, v, reasons, ok in selftest():
            print(f"[selftest] {name}: verdict={v} ok={ok} reasons={reasons}")
        sys.exit(0)
    targets = [a for a in sys.argv[1:] if not a.startswith("-")] or CORPUS
    results = [probe(t) for t in targets]
    receipt = {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "scope": "read-only complete-header parse of saved worker-6 corpus utocs; "
                 "generic-magic correction applied; no game files touched",
        "admission": {"versions": sorted(ADMIT_VERSIONS), "methods": sorted(ADMIT_METHODS)},
        "results": results,
    }
    os.makedirs(os.path.join(OUT_DIR, "evidence"), exist_ok=True)
    ev = os.path.join(OUT_DIR, "evidence/utoc-header-probe.json")
    with open(ev, "w") as f:
        json.dump(receipt, f, indent=1)
    for res in results:
        print("=" * 100)
        print(res["target"], f"({res.get('bytes')} bytes)")
        if not res.get("present", True):
            print("  MISSING"); continue
        h, r = res.get("header", {}), res.get("region", {})
        if h:
            print(f"  version={h['version']} ({h['version_name']})  header_size={h['toc_header_size']}"
                  f"  entries={h['toc_entry_count']}  blocks={h['toc_compressed_block_entry_count']}"
                  f"  block_entry_size={h['toc_compressed_block_entry_size']}")
            print(f"  container_id=0x{h['container_id']:016x}  flags={h['container_flags']}"
                  f"  enc_key_guid={h['encryption_key_guid_hex'][:16]}...")
            print(f"  method_names={r.get('compression_method_names')}"
                  f"  referenced={r.get('referenced_methods_resolved')}")
            print(f"  partition_count={h['partition_count']}  partition_size={h['partition_size']}"
                  f"  dir_index_size={h['directory_index_size']}"
                  f"  compression_block_size={h['compression_block_size']}")
            print(f"  offsets: ids@{r.get('chunk_ids_offset')} offlen@{r.get('chunk_offset_lengths_offset')}"
                  f" seeds={r.get('perfect_hash_seeds_count')}+{r.get('chunks_without_perfect_hash_count')}"
                  f" blocks@{r.get('compression_blocks_offset')} methods@{r.get('compression_methods_offset')}"
                  f" diridx@{r.get('directory_index_offset')} meta@{r.get('meta_offset')}")
            print(f"  structural_eof_match={r.get('structural_eof_match')}"
                  f" (computed_end={r.get('computed_end')} file={r.get('file_size')}"
                  f" residual={r.get('unaccounted_residual_bytes')})")
        for key in ("siblings", "s6_crosscheck"):
            if key in res:
                print(f"  {key}: {json.dumps(res[key])}")
        print(f"  VERDICT: {res['verdict']}")
        for x in res.get("reasons", []):
            print(f"    reason: {x}")
        for x in res.get("warnings", []):
            print(f"    warning: {x}")
    print("=" * 100)
    print("evidence:", ev)

if __name__ == "__main__":
    main()
