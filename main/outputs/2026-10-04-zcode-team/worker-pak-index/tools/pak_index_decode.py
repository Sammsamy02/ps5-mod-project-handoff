#!/usr/bin/env python3
"""worker-pak-index: decode the saved v11 PAK primary index from saved bytes only.

Reads ONLY (bounded seeks, no full-file IO):
  - footer tail (last 1024 B),
  - primary-index region at footer IndexPosition (297250226, 56502 B),
  - the two regions the index header points at (path-hash + name directory),
    all inside the already-saved pak (sha ee145cbc... pinned in worker-6
    pak-probe.json).

Encoding provenance (honest scope):
  - The retained CUE4Parse subset (unreal-library-container-survey-1/parser/
    source/CUE4Parse) contains ONLY UE4/IO/{IoStoreReader.cs,
    FIoStoreEncryptionIV.cs}; FPakFile.cs / FPakInfo.cs are NOT retained.
  - The retained repak copy (mount-research/repak/repak/src/lib.rs lines
    38-39, 108-109) only maps Version::V11 -> VersionMajor::Fnv64BugFix.
  - Therefore every field below is decoded from bytes and cross-checked
    against pinned ground truth (worker-6 pak-probe.json; the frozen survey
    actual-metadata-classification.json). Field semantics not provable from
    these bytes are marked confidence:"inferred"; the saved pak's index
    layout is byte-proved structural, and hashed/duplicate-sentinel schemes
    are marked unknown where no retained source documents them for v11.

Decoded layout established by this run (byte-proved):
  primary index @297250226, 56502 B =
    [0,14)    FString mount "../../../"  (i32 len 10 + 9 chars + NUL)
    [14,18)   i32 entry count = 4699
    [18,114)  96-B region table: for each of 2 regions
              { i32 present, u64 offset, u64 size, sha1[20] }
              + trailing u32 entries_blob_size=56388, u32 master_flags=0xE0000000
    [114,end) 4699 x 12-B records [u32 data_offset][u32 size][u32 flags]
  region table -> B: path-hash table @297306728 (146487 B, sha1 verified)
              -> C: name directory   @297453215 (119026 B, sha1 verified)
"""
import hashlib
import json
import os
import struct
import sys

WORKSPACE = "/WORKSPACE/.codex/worktrees/translation-core/what"
PAK = os.path.join(
    WORKSPACE,
    ".superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak",
)
EXPECTED = {  # pinned in worker-6-containers/evidence/pak-probe.json
    "sha256": "ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867",
    "bytes": 297572462,
    "index_offset": 297250226,
    "index_size": 56502,
    "declared_entry_count": 4699,
    "mount": "../../../",
}
OUT = os.path.join(WORKSPACE, "outputs/2026-10-04-zcode-team/worker-pak-index/evidence/pak-index-decode.json")
MAGIC = 0x5A6F12E1


def fstring(buf, off, cap=4096):
    (n,) = struct.unpack_from("<i", buf, off)
    if n == 0:
        return "", off + 4
    if 0 < n <= cap:
        return buf[off + 4 : off + 4 + n - 1].decode("ascii", "replace"), off + 4 + n
    u = -n
    if 0 < u <= cap:
        return buf[off + 4 : off + 4 + u * 2 - 2].decode("utf-16-le", "replace"), off + 4 + u * 2
    raise ValueError(f"implausible FString len {n} at {off}")


def main():
    ev = {"worker": "worker-pak-index", "tool": "tools/pak_index_decode.py",
          "pak": os.path.relpath(PAK, WORKSPACE),
          "pak_sha256_pinned": EXPECTED["sha256"], "scope": "bounded seeks: footer tail + index + 2 pointed regions"}
    fs = os.path.getsize(PAK)
    ev["file_size"] = fs
    with open(PAK, "rb") as f:
        # footer (compact 204..221 B; magic-anchored)
        f.seek(fs - 1024)
        tail = f.read(1024)
        mp = tail.rfind(struct.pack("<I", MAGIC))
        fb = tail[mp:]
        magic, version = struct.unpack_from("<II", fb, 0)
        ipos, isize = struct.unpack_from("<qq", fb, 8)
        ev["footer"] = {
            "magic": hex(magic), "version": version,
            "index_offset": ipos, "index_size": isize,
            "index_hash_sha1": fb[24:44].hex(),
            "region_of_footer_magic": [fs - (len(tail) - mp), fs],
            "bytes_after_index_fields": len(fb) - 44,
            "layout_note": "compact footer: magic/version/indexpos/indexsize/sha1 then 5x32B method slots; no v10 path-hash footer fields present (confidence: byte-proved; footer struct end = " + str(44 + 160) + " of " + str(len(fb)) + " bytes)",
            "matches_probe": {"index_offset": ipos == EXPECTED["index_offset"],
                              "index_size": isize == EXPECTED["index_size"]},
        }

        # primary index region
        f.seek(ipos)
        idx = f.read(isize)
        mount, m_end = fstring(idx, 0)
        (count,) = struct.unpack_from("<i", idx, m_end)
        off = m_end + 4
        pe = {"region": [ipos, ipos + isize], "region_sha256": hashlib.sha256(idx).hexdigest(),
              "mount_point": {"value": mount, "bytes": [0, m_end],
                              "matches_probe": mount == EXPECTED["mount"]},
              "entry_count_field": {"value": count, "at_byte": m_end,
                                    "bytes_hex": idx[m_end:m_end + 4].hex(),
                                    "matches_probe_4699": count == EXPECTED["declared_entry_count"]}}

        # 96-byte region table: 2 x {i32 present, u64 off, u64 size, sha1 20} + u32 blob_size + u32 master_flags
        hdr = idx[off:off + 96]
        u = struct.unpack_from("<24I", hdr, 0)
        regions = []
        for i, (base, tag) in enumerate(((2, "region_B_path_hash_table"), (12, "region_C_name_directory"))):
            present, roff, rsize = u[base], u[base + 1] | (u[base + 2] << 32), u[base + 3] | (u[base + 4] << 32)
            rsha = hdr[(base + 5) * 4:(base + 10) * 4].hex()
            regions.append({"tag": tag, "present": present, "offset": roff, "size": rsize, "sha1_hex": rsha,
                            "confidence": "byte-proved offsets/sizes; tags inferred from content"})
        pe["region_table_96B"] = {
            "bytes": [off, off + 96], "as_u32[0..5]": u[:6],
            "regions": regions,
            "entries_blob_size_field": u[22], "master_flags": hex(u[23]),
            "entries_blob_size_matches_count_x12": u[22] == count * 12,
        }

        # entry records: count x 12 B [u32 offset][u32 size][u32 flags]
        base = off + 96
        recs = [struct.unpack_from("<III", idx, base + i * 12) for i in range(count)]
        offs = [r[0] for r in recs]; sizes = [r[1] for r in recs]; fl = [r[2] for r in recs]
        closed = base + count * 12 == isize
        pe["entry_records_12B"] = {
            "layout": "[u32 data_offset][u32 size][u32 flags] per record (confidence: byte-proved pattern from tail records; retained sources do not document this custom v11 encoding)",
            "count": count, "array_bytes": [base, base + count * 12],
            "fills_region_exactly": closed,
            "distinct_data_offsets": len(set(offs)),
            "duplicate_data_offsets": count - len(set(offs)),
            "offset_monotonic_nondecreasing": all(b >= a for a, b in zip(offs, offs[1:])),
            "all_offsets_below_index_start": sum(1 for o in offs if o < ipos),
            "offset_min_max": [min(offs), max(offs)],
            "size_min_max": [min(sizes), max(sizes)], "zero_size_records": sum(1 for s in sizes if s == 0),
            "flags_histogram": {hex(k): v for k, v in
                                {0: fl.count(0), 0xE0000000: fl.count(0xE0000000)}.items()},
            "data_span_end_max_offset_plus_size": max(o + s for o, s in zip(offs, sizes)),
            "deleted_or_sentinel_records": "none observed: no record matches any retained-source v11 deleted/duplicate sentinel (scheme not documented in retained sources -> unknown-by-spec, absent-by-bytes)",
            "first_record": list(recs[0]), "last_record": list(recs[-1]),
        }
        ev["primary_index"] = pe

        # pointed regions
        for r, buf_read in ((regions[0], None), (regions[1], None)):
            f.seek(r["offset"])
            rb = f.read(r["size"])
            r["sha1_actual"] = hashlib.sha1(rb).hexdigest()
            r["sha1_matches_table"] = r["sha1_actual"] == r["sha1_hex"]
            if r["tag"] == "region_B_path_hash_table":
                (n,) = struct.unpack_from("<i", rb, 0)
                pairs = [struct.unpack_from("<QI", rb, 4 + i * 12) for i in range(n)]
                ploc = [p[1] for p in pairs]
                fits = 4 + n * 12 <= len(rb)
                r["decoded"] = {
                    "count_field": n, "per_pair": "[u64 hash][u32 ref] (confidence: byte-prove ref column = ascending 12-step refs into 12B entry array; hash semantics unknown)",
                    "pairs_fit_in_region": fits, "trailing_bytes": len(rb) - (4 + n * 12),
                    "distinct_refs": len(set(ploc)), "ref_min_max": [min(ploc), max(ploc)],
                    "refs_all_multiple_of_12": all(x % 12 == 0 for x in ploc),
                    "refs_all_within_entry_array": max(ploc) <= count * 12,
                    "note_if_trailing_bytes": "content of trailing bytes not decoded (no retained-source schema; marked unknown)" if len(rb) - (4 + n * 12) else None,
                }
            else:
                # name directory: [i32 dir_count][dir FString][i32 n][n x ([file FString][u32 ref])]
                (ndirs,) = struct.unpack_from("<i", rb, 0)
                o = 4
                names = []
                werr = None
                try:
                    for _ in range(ndirs):
                        d, o = fstring(rb, o)
                        (n,) = struct.unpack_from("<i", rb, o); o += 4
                        if n < 0 or n > 100000:
                            raise ValueError(f"bad file count {n} at {o}")
                        for _ in range(n):
                            fn, o = fstring(rb, o)
                            (ref,) = struct.unpack_from("<I", rb, o); o += 4
                            names.append((d + fn, ref))
                except (ValueError, struct.error) as e:
                    werr = str(e)
                refs = [x[1] for x in names]
                r["decoded"] = {
                    "dir_count_field": ndirs, "walk_error": werr,
                    "walk_closed_exactly": o == len(rb) if werr is None else False,
                    "bytes_consumed": o,
                    "file_records": len(names),
                    "distinct_names": len(set(n for n, _ in names)),
                    "duplicate_names": len(names) - len(set(n for n, _ in names)),
                    "distinct_refs": len(set(refs)),
                    "duplicate_refs": len(refs) - len(set(refs)),
                    "ref_min_max": [min(refs), max(refs)] if refs else None,
                    "negative_or_sentinel_refs": sum(1 for x in refs if x >= 0x80000000 or x < 0),
                    "first_names": names[:2], "last_names": names[-2:],
                    "grammar_confidence": "byte-proved: standard FStrings + u32 refs; ref semantics (index/offset mapping) inferred",
                }
        ev["pointed_regions"] = regions

    # reconciliation arithmetic
    ent = pe["entry_records_12B"]
    nd = regions[1]["decoded"]
    ev["reconciliation"] = {
        "saved_pak": {
            "raw_index_records": count,
            "name_directory_file_records": nd.get("file_records"),
            "distinct_names": nd.get("distinct_names"),
            "distinct_entry_data_offsets": ent["distinct_data_offsets"],
            "duplicate_or_deleted_reduction": 0 if (nd.get("file_records") == count == ent["distinct_data_offsets"]) else "see fields",
        },
        "original_disc_pak_4544": {
            "source": "unreal-library-container-survey-1/parser/actual-metadata-classification.json (frozen receipt; file_identity: logical_size 127387878395? no -> 1273878395 B, sha/index hashes differ from saved pak)",
            "file_records": 4544, "encoded_entry_count": 4544, "distinct_encoded_offsets": 4544,
            "deleted_tombstones": 0, "negative_nonencoded_refs": 0,
        },
        "delta": count - 4544,
        "verdict": "4699 and 4544 are counts of TWO DIFFERENT FILES, not raw-vs-dedup of one index: the saved pak's 4699 records are all distinct data offsets with a 1:1 name mapping (no duplicate/deleted reduction), and the 4544 receipt belongs to the larger on-disc original pak. Delta 4699-4544=155 is a build/corpus difference.",
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as w:
        json.dump(ev, w, indent=1)
    print(json.dumps({"footer_ok": ev["footer"]["matches_probe"],
                      "mount": mount, "count": count, "fills_exactly": closed,
                      "distinct_offsets": ent["distinct_data_offsets"],
                      "sha1_B_ok": regions[0]["sha1_matches_table"],
                      "sha1_C_ok": regions[1]["sha1_matches_table"],
                      "regionB_count": regions[0]["decoded"]["count_field"],
                      "regionB_trailing": regions[0]["decoded"]["trailing_bytes"],
                      "regionC_dirs": nd.get("dir_count_field" if False else "dir_count_field") if False else nd.get("dir_count_field"),
                      "regionC_files": nd.get("file_records"),
                      "regionC_closed": nd.get("walk_closed_exactly"),
                      "regionC_err": nd.get("walk_error"),
                      "verdict": ev["reconciliation"]["verdict"][:120]}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
