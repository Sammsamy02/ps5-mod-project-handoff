#!/usr/bin/env python3
"""check_host_output.py — HOST TEST validator (worker-c2-probe). Parses the
three host-run JSONL files and asserts the probe logic end to end:
record shape/validity, mapscan geometry cross-checks, persisted readable
span list, gating outcomes per mode, live_hex values equal the OFFLINE
expected values (the offline comparison REHEARSAL), refusal reasons, and
the absence of any expected-bytes/byte_match keys in the emitted records."""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "out"
EV = json.loads((HERE.parent / "EXPECTED-VALUES-OFFLINE.json").read_text())
EXPECT = {c["id"]: c for c in EV["candidates"]}
ORDER = ["B", "C", "D", "E", "A"]

fail = []


def check(cond, msg):
    if not cond:
        fail.append(msg)


def load(name):
    recs = []
    for ln in (OUT / name).read_text().splitlines():
        if not ln.strip():
            continue
        recs.append(json.loads(ln))  # raises on invalid JSON = hard fail
    return recs


def common(recs, name):
    for i, r in enumerate(recs):
        check(r.get("marker") == "modbridge.ingamefacts", f"{name}[{i}] marker")
        check(r.get("schema") == 1, f"{name}[{i}] schema")
        check(r.get("build_id") == "igf7-HOSTTEST", f"{name}[{i}] build_id")
        check("fields" in r and isinstance(r["fields"], dict), f"{name}[{i}] fields")
        flat = json.dumps(r)
        # Forbidden: any key/value from the self-referential igf4/igf6 design.
        for bad_key in ("byte_match", "diff_mask", "expected_bytes_hex",
                        "expected_src", "expected16_hex"):
            check(f'"{bad_key}"' not in flat, f"{name}[{i}] {bad_key} present")
        check("__trunc" not in flat, f"{name}[{i}] truncated record")


def cand_records(recs):
    return [r for r in recs if r.get("exp") in ("cand_read", "cand_refusal")]


def check_read(r, name):
    f = r["fields"]
    cid = f["cand"]
    e = EXPECT[cid]
    check(r["exp"] == "cand_read", f"{name} seq{r['seq']} expected cand_read")
    check(f["live_hex"] == e["expected_hex"],
          f"{name} {cid} live_hex {f['live_hex']} != offline expected")
    check(f["len"] == e["len"], f"{name} {cid} len")
    check(int(f["rt_addr"], 16) == int(e["rt_base_0x400000"], 16),
          f"{name} {cid} rt_addr")
    check(int(f["vaddr"], 16) == int(e["vaddr"], 16), f"{name} {cid} vaddr")
    check(int(f["image_file_off"], 16) == int(e["derived_file_offset"], 16),
          f"{name} {cid} file_off")
    check(int(f["rt_addr"], 16) - int(f["vaddr"], 16) == 0x400000,
          f"{name} {cid} rt-vaddr != base 0x400000")
    check(int(f["span_prot"]) & 1, f"{name} {cid} span_prot not readable")
    check(int(f["span_start"], 16) <= int(f["rt_addr"], 16) and
          int(f["rt_addr"], 16) + f["len"] <= int(f["span_end"], 16),
          f"{name} {cid} span does not cover window")


def check_refusal(r, name, reason):
    f = r["fields"]
    check(r["exp"] == "cand_refusal", f"{name} seq{r['seq']} expected refusal")
    check(f["refusal"] == reason, f"{name} {f.get('cand')} refusal {f['refusal']}")
    check("live_hex" not in f, f"{name} {f.get('cand')} refusal carries bytes")


def mapspan_check(recs, name, expected_readable):
    pages = [r for r in recs if r.get("exp") == "mapspans"]
    if expected_readable == 0:
        # Untrusted map: the empty state record is honest and allowed.
        check(len(pages) <= 1, f"{name} unexpected mapspans pages")
        check(all(p["fields"]["spans_in_page"] == 0 for p in pages),
              f"{name} mapspans emitted with untrusted map")
        check(all(p["fields"]["map_trusted"] == 0 for p in pages),
              f"{name} mapspans trusted flag")
        return
    total = sum(p["fields"]["spans_in_page"] for p in pages)
    check(total == expected_readable,
          f"{name} mapspans total {total} != readable {expected_readable}")
    check(sum(1 for p in pages if p["fields"]["page"] == 0) == 1,
          f"{name} mapspans page 0 missing/dup")
    n = 0
    for p in pages:
        for k, v in sorted(p["fields"].items()):
            if not (k.startswith("s") and k[1:].isdigit()):
                continue
            sp, prot = v.rsplit(":", 1)
            s0, s1 = sp.split("-")
            check(int(s1, 16) > int(s0, 16), f"{name} mapspan start>=end")
            check(int(prot) & 1, f"{name} mapspan not readable")
            n += 1
    check(n == expected_readable, f"{name} parsed {n} mapspans != {expected_readable}")


def summary(recs, name):
    s = [r for r in recs if r.get("exp") == "summary"]
    check(len(s) == 1, f"{name} summary count")
    return s[0]["fields"] if s else {}


# ---- run 1: trusted ----------------------------------------------------
recs = load("trusted.jsonl")
common(recs, "trusted")
check(recs[0]["seq"] == 1 and recs[0]["exp"] == "entry", "trusted seq1")
check(recs[1]["seq"] == 2 and recs[1]["exp"] == "startup", "trusted seq2")
ms = recs[2]
check(ms["seq"] == 3 and ms["exp"] == "mapscan", "trusted seq3 mapscan")
check(ms["fields"]["trusted"] == 1, "trusted mapscan trusted!=1")
check(ms["fields"]["base_matches_expected"] == 1, "trusted base flag")
check(int(ms["fields"]["derived_base"], 16) == 0x400000, "trusted derived_base")
check(ms["fields"]["exec_extent_matches_expected"] == 1, "trusted exec extent flag")
check(int(ms["fields"]["exec_span_bytes"], 16) == 0x747c000, "trusted exec bytes")
check(ms["fields"]["ksz_144"] == 8, "trusted ksz_144 (stride fix) count")
mapspan_check(recs, "trusted", 7)
cands = cand_records(recs)
check([c["fields"]["cand"] for c in cands] == ORDER,
      f"trusted order {[c['fields']['cand'] for c in cands]}")
for i, cid in enumerate(ORDER):
    check(cands[i]["seq"] == 4 + i, f"trusted {cid} seq")
    check_read(cands[i], "trusted")
sf = summary(recs, "trusted")
check(sf.get("reads_performed") == 5 and sf.get("refusals") == 0, "trusted summary counts")
check(sf.get("exec_reads_attempted") == 0, "trusted exec_reads_attempted")
check(sf.get("comparison_on_console") == 0, "trusted comparison_on_console")
for cid in ORDER:
    check(sf.get(f"{cid}_read") == 1 and sf.get(f"{cid}_refusal") == 0,
          f"trusted summary {cid}")
    check(sf.get(f"{cid}_covered") == 1, f"trusted summary {cid} covered")

# ---- run 2: untrusted --------------------------------------------------
recs = load("untrusted.jsonl")
common(recs, "untrusted")
ms = [r for r in recs if r.get("exp") == "mapscan"][0]
check(ms["fields"]["trusted"] == 0, "untrusted mapscan trusted")
mapspan_check(recs, "untrusted", 0)
cands = cand_records(recs)
check(len(cands) == 5, "untrusted candidate record count")
for r in cands:
    check_refusal(r, "untrusted", "map_untrusted")
sf = summary(recs, "untrusted")
check(sf.get("reads_performed") == 0 and sf.get("refusals") == 5,
      "untrusted summary counts")

# ---- run 3: covered_not_readable ---------------------------------------
recs = load("nonreadable.jsonl")
common(recs, "nonreadable")
cands = cand_records(recs)
check([c["fields"]["cand"] for c in cands] == ORDER,
      "nonreadable candidate order")
for i, cid in enumerate(ORDER):
    if cid == "A":
        check_read(cands[i], "nonreadable")
        check(cands[i]["seq"] == 8, "nonreadable A seq")
    else:
        check_refusal(cands[i], "nonreadable", "covered_not_readable")
        check(cands[i]["fields"].get("covered") == 1,
              f"nonreadable {cid} covered flag")
sf = summary(recs, "nonreadable")
check(sf.get("reads_performed") == 1 and sf.get("refusals") == 4,
      "nonreadable summary counts")
for cid in ("B", "C", "D", "E"):
    check(sf.get(f"{cid}_refusal") == 3, f"nonreadable {cid} refusal code")

if fail:
    print("HOST CHECK FAILURES:")
    for f in fail:
        print(" -", f)
    sys.exit(1)
print("HOST CHECKS PASSED: 3 runs, 5 candidates, order B,C,D,E,A; "
      "live_hex == offline expected for every read; no expected bytes or "
      "byte_match keys in any emitted record.")
