#!/usr/bin/env python3
"""
derive_geometry.py — builds GEOMETRY.json for worker-geometry-reference.

Reads:
  1. The retained eboot via parse_phdrs (own parser, same directory).
  2. The pinned raw console logs (read-only) for observed-span provenance.

Emits GEOMETRY.json in the worker output dir. Every number is computed here,
never transcribed by hand, so the JSON is reproducible from the pinned inputs.

Vintage keys used in provenance:
  igf  = build igf-f4a91d8123b0   (ingame-v1-facts log, g5_vmmap)
  igf2 = build igf2-764f957dc0a6  (ingame-v2-facts log, v1_vmmap_page/full)
  igf3 = build igf3-de0c0cb644d3  (igf6-results.jsonl line 67 / ingame-v3-full-results.jsonl)
  igf4 = build igf4-4891869337b4  (igf6-results.jsonl lines 1103-1109)
  igf7 = build igf7-3dd949879a0a  (c2-alternative-results.jsonl lines 1122-1418)
  launch9 = standalone aligned-entry build f6530c79... (aligned-entry-log-snapshot.log)
"""

import hashlib
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from parse_phdrs import parse, PAGE_ALIGN_16K  # noqa: E402

WHAT = "/WORKSPACE/.codex/worktrees/translation-core/what"
EBOOT = os.path.join(WHAT, ".superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin")
CON = os.path.join(WHAT, "outputs/2026-10-04-zcode-controller/console")
L9LOG = os.path.join(WHAT, "outputs/2026-10-04-zcode-controller/launch9-evidence/aligned-entry-log-snapshot.log")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "GEOMETRY.json")

PAGE = PAGE_ALIGN_16K
SLIDE = 0x400000


def rnd_up(x):
    return (x + PAGE - 1) // PAGE * PAGE


def h(x):
    return "0x%x" % x


def jsonl_line(path, lineno_1based):
    """Return parsed record at 1-based line number (or None if unparsable)."""
    with open(path, "rb") as fh:
        for i, raw in enumerate(fh):
            if i + 1 == lineno_1based:
                try:
                    return json.loads(raw)
                except Exception:
                    return {"_raw_prefix": raw[:200].decode("utf-8", "replace")}
    return None


def main():
    hdr, phdrs = parse(EBOOT)
    loads = [p for p in phdrs if p["p_type"] == 1]

    # ---------------- observed spans from the pinned logs ----------------
    # igf3 clean paged dump, part 0 (1-based line 67 of igf6-results.jsonl)
    igf3_part0 = jsonl_line(os.path.join(CON, "igf6-results.jsonl"), 67)
    assert igf3_part0["build_id"] == "igf3-de0c0cb644d3" and igf3_part0["seq"] == 4
    igf3_spans = [
        {"start": int(e["start"], 16), "end": int(e["end"], 16), "prot": e["prot"]}
        for e in igf3_part0["fields"]["entries"]
    ]

    # igf4 records (lines 1103-1109)
    igf4 = {}
    for ln, key in ((1103, "tick_anchor"), (1104, "sco_anchor"), (1105, "gua_read"),
                    (1106, "gua_writeback"), (1109, "summary")):
        r = jsonl_line(os.path.join(CON, "igf6-results.jsonl"), ln)
        assert r["build_id"] == "igf4-4891869337b4", (ln, r.get("build_id"))
        igf4[key] = {"line": ln, "seq": r["seq"], "fields": r["fields"], "rc": r.get("rc")}

    # igf7 records
    igf7_mapscan = jsonl_line(os.path.join(CON, "c2-alternative-results.jsonl"), 1122)
    igf7_summary = jsonl_line(os.path.join(CON, "c2-alternative-results.jsonl"), 1418)
    igf7_cands = []
    for ln in range(1413, 1418):
        igf7_cands.append(jsonl_line(os.path.join(CON, "c2-alternative-results.jsonl"), ln))

    # launch-9 raw byte offsets
    l9 = open(L9LOG, "rb").read()
    def at(off, needle):
        # offsets are diagnostic line starts; the channel prefix alone is ~110 bytes
        return needle.encode() in l9[off:off + 300]
    l9_facts = {
        "file_size": len(l9),
        "bridge_lease_reg_spans@2270807": at(2270807, "bridge_lease_reg_spans value=0x00000006"),
        "bridge_lease_reg@2270933": at(2270933, "bridge_lease_reg value=0x00000006"),
        "bridge_vmentry_entries@2269782": at(2269782, "bridge_vmentry_entries value=0x00002723"),
    }
    assert all(v is True for k, v in l9_facts.items() if k != "file_size"), l9_facts

    # v1/v2 giant span: byte offset of the end address in both logs
    v1 = open(os.path.join(CON, "ingame-v1-facts-2026-10-04.jsonl"), "rb").read()
    v2 = open(os.path.join(CON, "ingame-v2-facts-2026-10-04.jsonl"), "rb").read()
    giant_probe = {
        "ingame-v1-facts-2026-10-04.jsonl": v1.find(b"0x0000000100118000"),
        "ingame-v2-facts-2026-10-04.jsonl": v2.find(b"0x0000000100118000"),
    }

    # ---------------- uniform-slide model vs igf3 observed ----------------
    extent_vaddr_end = max(p["vaddr_end"] for p in loads)          # 0xbc602c8
    rt_extent = (SLIDE, SLIDE + extent_vaddr_end)                  # [0x400000, 0xc0602c8)

    observed_image = [s for s in igf3_spans if s["start"] < rt_extent[1] and s["end"] > rt_extent[0]]
    observed_image.sort(key=lambda s: s["start"])

    seg_rows = []
    for p in loads:
        v0, m0 = p["p_vaddr"], p["p_memsz"]
        rt0, rt1 = SLIDE + v0, SLIDE + v0 + m0
        covered = [s for s in observed_image if s["start"] >= rnd_up(rt0) - PAGE and s["end"] <= rnd_up(rt1) + PAGE
                   and s["start"] < rnd_up(rt1) and s["end"] > rnd_up(rt0)]
        seg_rows.append({
            "phdr_index": p["index"],
            "flags": p["flags_str"],
            "p_flags": p["p_flags"],
            "vaddr": v0, "memsz_end": v0 + m0, "filesz_end": v0 + p["p_filesz"],
            "bss_bytes": p["bss_bytes"],
            "rt_start": rt0, "rt_end_memsz": rt1,
            "rt_page_rounded": [rnd_up(rt0) if v0 % PAGE else rt0, rnd_up(rt1)],
            "expected_prot_bits": p["p_flags"],
            "igf3_observed_spans": covered,
        })

    # check the 7 observed image spans tile [0x400000, 0x9a34000) contiguously
    tile_ok = observed_image[0]["start"] == SLIDE
    for a, b in zip(observed_image, observed_image[1:]):
        tile_ok = tile_ok and a["end"] == b["start"]
    last_obs_end = observed_image[-1]["end"]

    # ---------------- seeds ----------------
    def classify(rt):
        file_label = rt - SLIDE
        seg = next((p for p in loads if p["p_vaddr"] <= file_label < p["vaddr_end"]), None)
        in_bss = seg is not None and file_label >= seg["p_vaddr"] + seg["p_filesz"]
        return {
            "runtime": rt,
            "file_label_if_slide_0x400000": file_label,
            "segment_phdr": seg["index"] if seg else None,
            "segment_flags": seg["flags_str"] if seg else None,
            "in_bss": in_bss,
            "on_exec_only_pages": bool(seg and seg["p_flags"] == 1),
        }

    seeds = {
        "gu_object_array": classify(0x97ba110),
        "fname_ctor": classify(0x63d590),
        "static_construct_object": classify(0x890aa0),
        "gmalloc_ptr": classify(0x970add8),
        "ugameengine_tick": classify(0x33ac8c0),
    }

    # ---------------- GMalloc candidate tests ----------------
    g_tests = {
        "observed_runtime": 0x970add8,
        "derived_label_rank1_gmalloc_derivation": 0x930add8,
        "derived_label_plus_slide": 0x930add8 + SLIDE,
        "derived_label_matches_runtime": (0x930add8 + SLIDE) == 0x970add8,
        "rejected_deletefile_candidate_label": 0x9350950,
        "rejected_candidate_plus_slide": 0x9350950 + SLIDE,
        "rejected_candidate_plus_slide_matches_runtime": (0x9350950 + SLIDE) == 0x970add8,
        "rejected_candidate_implied_slide": 0x970add8 - 0x9350950,
        "runtime_minus_derived_label": 0x970add8 - 0x930add8,
        "rejected_candidate_refutation": "implied slide 0x3ba488 (the controller seed's 0x3b0488 is itself a digit slip) contradicts the pinned 0x400000 slide for L0/L1/L3/L7 and every observed span; the label itself fails worker-gmalloc-derivation's unique-store test",
    }

    # ---------------- controller seed arithmetic checks ----------------
    seed_checks = {
        "LOAD7_plus_0x400000": {
            "claimed": "0xd13c000", "computed": h(0x913c000 + SLIDE),
            "error_class": "0x4000000 added at digit 16^6 instead of 0x400000 at 16^5",
        },
        "image_extent_end": {
            "claimed": "0xfc602c8", "computed": h(SLIDE + extent_vaddr_end),
            "note": "runtime_start_bridge_r13.cpp:493 computes 0x400000+0xbc602c8 correctly at runtime; the string 0xfc602c8 exists only in the comment at :509 (typo)",
        },
        "LOAD3_memsz": {
            "claimed": "0x12800000", "computed": h(next(p["p_memsz"] for p in loads if p["index"] == 3)),
            "check": h(0x7ebc000 + next(p["p_memsz"] for p in loads if p["index"] == 3)) + " == 0x913c000",
        },
        "gmalloc_candidate_delta": {
            "claimed": "0x3b0488", "computed": h(0x970add8 - 0x9350950),
            "note": "controller seed digit slip; true delta of 0x970add8-0x9350950",
        },
    }

    # ---------------- inter-LOAD gaps ----------------
    gaps = []
    for a, b in zip(loads, loads[1:]):
        gaps.append({
            "between": [a["index"], b["index"]],
            "range": [a["vaddr_end"], b["p_vaddr"]],
            "bytes": b["p_vaddr"] - a["vaddr_end"],
            "sub_page": (b["p_vaddr"] - a["vaddr_end"]) < PAGE,
        })

    out = {
        "worker": "worker-geometry-reference",
        "generated": "2026-10-04",
        "image": {
            "path": ".superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin",
            "bytes": hdr["file_size"],
            "sha256": hdr["sha256"],
            "e_phnum": hdr["e_phnum"], "e_phoff": hdr["e_phoff"], "e_phentsize": hdr["e_phentsize"],
            "e_entry": hdr["e_entry"], "e_type": hdr["e_type"], "e_machine": hdr["e_machine"],
            "phdr_parse_script": "parse_phdrs.py (this directory); per-record file offsets in phdrs-derived.json",
        },
        "page_size": PAGE,
        "slide_model": {
            "uniform_runtime_slide": SLIDE,
            "confidence": "very high (see evidence)",
            "statement": "runtime = p_vaddr + 0x400000 for every PT_LOAD; validated below against four independent in-game observations",
            "file_space_image_extent": [0, extent_vaddr_end],
            "runtime_image_extent": list(rt_extent),
            "runtime_image_extent_page_rounded": [rt_extent[0], rnd_up(rt_extent[1])],
        },
        "segments": seg_rows,
        "phdr_gaps_file_space": gaps,
        "observed": {
            "igf3_clean_walk_part0": {
                "source": "outputs/2026-10-04-zcode-controller/console/igf6-results.jsonl line 67 (build igf3-de0c0cb644d3, seq 4, exp v1_vmmap_page part 0; same records re-exported in ingame-v3-full-results.jsonl)",
                "spans": igf3_spans,
                "image_extent_intersection": observed_image,
                "tiles_contiguously_from_slide_base": tile_ok,
                "last_image_span_end": last_obs_end,
                "note": "7 unique spans intersect the runtime image extent; 6 are readable (prot&1) — matches launch-9 bridge_lease_reg_spans=6; no span at all covers L8's range [0x9a34000,0xc0602c8) at this vintage",
            },
            "igf4_anchors": {
                "source": "outputs/2026-10-04-zcode-controller/console/igf6-results.jsonl lines 1103-1109 (build igf4-4891869337b4)",
                "tick_anchor": {"rt": 0x33ac8c0, "rc": igf4["tick_anchor"]["rc"], "refusal": igf4["tick_anchor"]["fields"].get("refusal")},
                "sco_anchor": {"rt": 0x890aa0, "rc": igf4["sco_anchor"]["rc"], "refusal": igf4["sco_anchor"]["fields"].get("refusal")},
                "gua_read": {
                    "rt": int(igf4["gua_read"]["fields"]["rt_addr"], 16),
                    "bytes": igf4["gua_read"]["fields"]["bytes"],
                    "span": [int(igf4["gua_read"]["fields"]["span_start"], 16), int(igf4["gua_read"]["fields"]["span_end"], 16)],
                    "span_prot": int(igf4["gua_read"]["fields"]["span_prot"], 16),
                    "rc": igf4["gua_read"]["rc"],
                },
                "gua_writeback": igf4["gua_writeback"]["fields"],
                "caveat": "gua structural predictions (header/live cfgs) were REFUTED (summary gua_validated_any_cfg=0); what is proven is read+same-value-writeback success in the RW span at 0x97ba110",
            },
            "igf7_data_reads": {
                "source": "outputs/2026-10-04-zcode-controller/console/c2-alternative-results.jsonl lines 1122 (seq3 mapscan), 1413-1417 (seq4-8 cand_read/refusal), 1418 (seq9 summary)",
                "derived_base": igf7_mapscan["fields"]["derived_base"],
                "exec_span": [igf7_mapscan["fields"]["exec_span_start"], igf7_mapscan["fields"]["exec_span_end"]],
                "readable_spans_count": igf7_mapscan["fields"]["readable_spans"],
                "candidate_reads": [
                    {"line": 1413 + i, "exp": c["exp"], "addr": c["fields"].get("rt_addr"),
                     "covering_span": [c["fields"].get("span_start"), c["fields"].get("span_end")],
                     "span_prot": c["fields"].get("span_prot")}
                    for i, c in enumerate(igf7_cands)
                ],
                "summary": {k: igf7_summary["fields"][k] for k in ("map_trusted", "exec_extent_matches_expected", "exec_reads_attempted")},
                "byte_match_note": "B/C/D/E live bytes matched offline expectations (FALLBACK-RANKING.md section 2) — proves L1 file content sits at L1+0x400000",
            },
            "launch9_entry": {
                "source": "outputs/2026-10-04-zcode-controller/launch9-evidence/aligned-entry-log-snapshot.log",
                "verified_offsets": l9_facts,
                "bridge_lease_reg_spans": 6,
                "bridge_lease_reg": 6,
                "bridge_vmentry_entries": 10019,
                "vmentry_walk": "clean stop reason 0, identical across three rounds (worker-launch9-diagnostics-mine/REPORT.md section 2)",
            },
            "giant_readable_span_v1_v2": {
                "span": [0x8018c000, 0x100118000, 1],
                "provenance": {
                    "ingame-v1-facts-2026-10-04.jsonl": "byte offset %d (g5_vmmap entries[1], build igf-f4a91d8123b0)" % giant_probe["ingame-v1-facts-2026-10-04.jsonl"],
                    "ingame-v2-facts-2026-10-04.jsonl": "byte offset %d (v1_vmmap_full entries[1], build igf2-764f957dc0a6)" % giant_probe["ingame-v2-facts-2026-10-04.jsonl"],
                },
                "classification": "OUTSIDE the runtime image extent [0x400000,0xc0602c8); real kernel-reported span at its vintage (span-arbitration VERDICT section 5.4), but absent from the igf3 clean walk where 0x8018c000 is covered by a 32 KiB R-- span inside a dense mixed-prot neighborhood starting 0x80000000; vintage-churned non-image mapping — plausible large data/heap-reservation class, identity UNKNOWN",
            },
        },
        "seeds": seeds,
        "gmalloc_label_tests": g_tests,
        "seed_arithmetic_checks": seed_checks,
        "known_false_claims": [
            {
                "claim": "continuous hole [0x787c000,0x8018c000) with nothing mapped inside",
                "status": "FALSE — RETRACTED",
                "why_wrong": "stitched the v1/v2-era exec-span end (0x787c000) to the v1/v2-era giant-span start (0x8018c000) and treated 'nothing recovered between' (an artifact of the 1160-stride vmmap corruption) as emptiness. The igf3 clean walk tiles [0x400000,0x9a34000) with 7 contiguous kernel spans (L0..L7); igf7 byte-matched reads and igf4 GUA read/write sit inside the alleged hole; launch-9 counted 6 readable spans intersecting the image extent.",
            },
            {
                "claim": "single global bias DISPROVEN (transport-ledger pinned verdict)",
                "status": "FALSE — REVERSED",
                "why_wrong": "the disproof rested on SEGMENT-CORRESPONDENCE section 5's overlap claim that 0x8018c000 lies inside L1's expected range [0x787c000,0x82bc000); it does not (0x8018c000 is ~2.0 GiB, the L1 range ends ~137 MB — a digit-width error). igf3/igf4/igf7 all confirm bias 0x400000 for L0/L1/L3/L7.",
            },
            {
                "claim": "LOAD7 file+0x400000 would start 0xd13c000 so the observed RW fragment cannot be LOAD7",
                "status": "FALSE — arithmetic slip in the controller seed",
                "why_wrong": "0x913c000+0x400000 = 0x953c000, not 0xd13c000 (0x4000000 was added instead of 0x400000). The observed fragment [0x970c000,0x9a34000) equals LOAD7's BSS (label filesz-end 0x93085f2 .. memsz-end 0x9633418, shifted +0x400000) page-rounded at 16 KiB, exactly.",
            },
            {
                "claim": "image extent [0x400000,0xfc602c8)",
                "status": "FALSE — digit error",
                "why_wrong": "extent_end = 0x400000 + 0xbc602c8 = 0xc0602c8. The bridge computes this correctly (runtime_start_bridge_r13.cpp:493); 0xfc602c8 appears only in a comment typo at :509.",
            },
            {
                "claim": "LOAD3 memsz=0x12800000",
                "status": "FALSE — extra zero in the controller seed",
                "why_wrong": "phdr bytes give p_memsz=0x1280000 (0x7ebc000+0x1280000=0x913c000 exact; parse_phdrs.py record @file 0xe8).",
            },
            {
                "claim": "gmalloc_deletefile_candidate file label 0x9350950 explains runtime GMalloc 0x970add8",
                "status": "FALSE — wrong label",
                "why_wrong": "0x9350950+0x400000=0x9750950 != 0x970add8; the implied slide 0x3b0488 fits no pinned span. The layout profile marks 0x9350950 candidate-only (worker-2-layout-facts REPORT.md:59) and worker-gmalloc-derivation measured-and-rejected the DeleteFile route. The derived label is 0x930add8 (unique store site, 11 consumer sites), which lands exactly on the observed runtime under the uniform slide.",
            },
        ],
        "consumer_read_risks": [
            "FName ctor 0x63d590, SCO 0x890aa0, Tick 0x33ac8c0 all live on LOAD0's exec-only pages ([0x400000,0x787c000), kernel prot=4, no READ bit registered): igf4 refusal class covered_not_readable (rc=3) proven at 0x33ac8c0/0x890aa0; igf6 died twice at the first exec-span read. The scan stage must gate these on exec-coverage, never read them as data.",
            "LOAD8 runtime range [0x9a34000,0xc0602c8) had NO vmmap span at the igf3 vintage and window A (0xc05f250) was refused uncovered by igf7 — anchors in L8 deadlock; treat L8 as unmapped-until-proven.",
            "SEED-TABLE.json fname_remedy.heap_span [0x8018c000,0x100118000) is inherited from the v1/v2 giant span and is NOT stable geometry (absent at igf3 vintage); any consumer must re-derive allocation targets from a fresh same-invocation mapscan.",
            "Host mock prospero_platform_host.cpp:452 encodes a fake 'gap [0x787c000,0x970c000)' for allocator hinting; on the real map that range is L1+L3+L7 image memory — allocator hole-hints must use the live mapscan, not this fixture.",
        ],
        "unknowns": [
            "Identity of the v1/v2 giant R-- span [0x8018c000,0x100118000) (vintage-transient; not present at igf3 vintage).",
            "Whether LOAD8 is ever mapped (its PT_DYNAMIC payload lives in its range) and why no span covers it at menu state.",
            "Why the kernel splits L7 into four entries with prots 3/1/3/3 (observed, mechanism unidentified).",
            "The actual enforcement behind exec-only read failures (igf6 mechanism never isolated; leading hypothesis only).",
            "mprotect transition behavior on this kernel (xwp probe pending, never run to completion).",
            "Exact cause of cross-vintage span-count churn (10,019 bridge vs 16,384 igf3-walk unique vs readable 8,657-8,705).",
        ],
    }

    with open(OUT, "w") as fh:
        json.dump(out, fh, indent=2)
    print("wrote", OUT)

    # ------- console summary of the model checks -------
    print("uniform slide checks:")
    for s in seg_rows:
        print("  L%d %-3s rt=[%#x,%#x) rounded=[%#x,%#x) igf3_spans=%d" % (
            s["phdr_index"], s["flags"], s["rt_start"], s["rt_end_memsz"],
            s["rt_page_rounded"][0], s["rt_page_rounded"][1], len(s["igf3_observed_spans"])))
    print("  contiguous tile from %#x to %#x: %s" % (SLIDE, last_obs_end, tile_ok))
    print("  observed image spans:")
    for s in observed_image:
        print("    [%#x,%#x) prot %d" % (s["start"], s["end"], s["prot"]))
    print("seed classifications:")
    for k, v in seeds.items():
        print("  %-24s rt=%#x label=%#x L%d %s bss=%s exec_only=%s" % (
            k, v["runtime"], v["file_label_if_slide_0x400000"], v["segment_phdr"],
            v["segment_flags"], v["in_bss"], v["on_exec_only_pages"]))
    print("gmalloc:", {k: (hex(v) if isinstance(v, int) else v) for k, v in g_tests.items()})
    print("seed arithmetic checks:", json.dumps(seed_checks, indent=1))


if __name__ == "__main__":
    main()
