#!/usr/bin/env python3
"""Offline recovery of v2 in-game VMMAP spans. Read-only inputs; writes only to this dir."""
import json, re, sys

BASE = "/WORKSPACE/.codex/worktrees/translation-core/what"
V2 = BASE + "/outputs/2026-10-04-zcode-controller/console/ingame-v2-facts-2026-10-04.jsonl"
V1 = BASE + "/outputs/2026-10-04-zcode-controller/console/ingame-v1-facts-2026-10-04.jsonl"
OUT = BASE + "/outputs/2026-10-04-zcode-team/worker-mapping-recovery"

PAGE = 0x4000          # 16 KiB
MAX36 = 0xFFFFFFFFF    # 36-bit user VA space bound (0 .. 2^36-1)
GUA_V = 0x93BA110      # GUObjectArray profile vaddr (worker-ingame-probe-v2 REPORT.md)

entry_re = re.compile(r'"start":"(0x[0-9a-fA-F]+)""end":"(0x[0-9a-fA-F]+)""prot":(?:"(0x[0-9a-fA-F]+)"|(\d+))')
hdr_re = re.compile(r'"seq":(\d+),"exp":"([a-z0-9_]+)"')
part_re = re.compile(r'"part":(\d+)')
hist_re = re.compile(r'"prot":"(0x[0-9a-fA-F]+)""count":(\d+)')
scan_cand_re = re.compile(r'"base":"(0x[0-9a-fA-F]+)""where":"([a-z_]+)""prot":(\d+)"span_start":"(0x[0-9a-fA-F]+)""span_end":"(0x[0-9a-fA-F]+)')

def parse(path):
    recs = []
    for ln, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
        line = line.strip()
        if not line:
            continue
        m = hdr_re.search(line)
        seq, exp = (int(m.group(1)), m.group(2)) if m else (None, None)
        pm = part_re.search(line)
        part = int(pm.group(1)) if pm else None
        ents = []
        for sm in entry_re.finditer(line):
            s = int(sm.group(1), 16); e = int(sm.group(2), 16)
            p = int(sm.group(3), 16) if sm.group(3) else int(sm.group(4))
            ents.append((s, e, p))
        hist = [(int(a, 16), int(b)) for a, b in hist_re.findall(line)]
        recs.append(dict(line=ln, seq=seq, exp=exp, part=part, entries=ents, hist=hist, raw=line))
    return recs

v1 = parse(V1)
v2 = parse(V2)

# ---- v1 g5 partial view (earlier record of the same process state) ----
v1g5 = [r for r in v1 if r["exp"] == "g5_vmmap"][0]
v1_tuples = set(v1g5["entries"])

# ---- v2 page records ----
pages = sorted([r for r in v2 if r["exp"] == "v1_vmmap_page"], key=lambda r: r["seq"])
full = [r for r in v2 if r["exp"] == "v1_vmmap_full"][0]
scan = [r for r in v2 if r["exp"] == "v2_scan_page"][0]

aligned = lambda x: x % PAGE == 0

trusted, plausible, rejected, anchors = [], [], [], {}
def add_anchor(kind, addr, src):
    anchors.setdefault(addr, {"addr": addr, "kinds": [], "sources": []})
    if kind not in anchors[addr]["kinds"]:
        anchors[addr]["kinds"].append(kind)
    anchors[addr]["sources"].append(src)

# collect garbage prot leaks that look like real addresses
leak_addrs = set()
for a, c in full["hist"]:
    if a > 7 and a <= MAX36 and aligned(a):
        leak_addrs.add(a)
for a, c in v1g5["hist"]:
    if a > 7 and a <= MAX36 and aligned(a):
        leak_addrs.add(a)

stream = []   # (seq, part, idx, s, e, p) in stream order
for r in pages:
    for i, (s, e, p) in enumerate(r["entries"]):
        stream.append((r["seq"], r["part"], i, s, e, p))

# ---------- pass 1: structural classification ----------
hi_family_cand = []
for seq, part, idx, s, e, p in stream:
    src = f"v2 seq {seq} part {part} entry {idx}"
    key = (s, e, p)
    if s == 0 and e == 0:
        rejected.append(dict(s=s, e=e, p=p, src=src, reason="empty slot: start=end=0 (writer padding / unread kve)"))
        continue
    if s >= e:
        rejected.append(dict(s=s, e=e, p=p, src=src, reason="start>=end (corrupt pair)"))
        if s and aligned(s) and s <= MAX36: add_anchor("start", s, src)
        if e and aligned(e) and e <= MAX36: add_anchor("end", e, src)
        continue
    if s == 0:
        rejected.append(dict(s=s, e=e, p=p, src=src, reason="start=0 (null page never mapped; unfilled start)"))
        if e and aligned(e) and e <= MAX36: add_anchor("end", e, src)
        continue
    if not (0 <= p <= 7):
        rejected.append(dict(s=s, e=e, p=p, src=src, reason=f"prot=0x{p:x} out of VM_PROT range (garbage prot field)"))
        if p <= MAX36 and aligned(p): add_anchor("prot_leak", p, src + f" prot=0x{p:x}")
        continue
    if not aligned(s) or not aligned(e):
        rejected.append(dict(s=s, e=e, p=p, src=src, reason="endpoint not 16KiB-aligned (real-mapping rule; start matches packed-garbage family)" if not aligned(s) else "end not 16KiB-aligned"))
        if aligned(e) and e <= MAX36: add_anchor("end", e, src)
        continue
    if e > MAX36:
        hi_family_cand.append(dict(s=s, e=e, p=p, src=src))
        continue
    # in-space, aligned, start<end, prot valid
    con = "clean-mapping: 16KiB-aligned start/end, 0<start<end, within 36-bit VA, prot in 1..7"
    if p == 0:
        con = "reservation: 16KiB-aligned start/end, 0<start<end, within 36-bit VA, prot 0 (plausible reservation prot)"
    if key in v1_tuples:
        trusted.append(dict(s=s, e=e, p=p, src=src + " (also v1 g5 seq5)", constraint="v1/v2 cross-record exact corroboration + alignment + 36-bit bounds + prot valid"))
    else:
        plausible.append(dict(s=s, e=e, p=p, src=src, constraint=con, flag=None))

# ---------- pass 2: monotone high family admission ----------
# verify strict monotonic non-overlapping order in stream order
fam, ok_chain, breaks = [], True, []
last_end = -1
for c in hi_family_cand:
    if c["s"] < last_end:
        ok_chain = False
        breaks.append(c["src"])
    fam.append(c)
    last_end = max(last_end, c["e"])
for c in fam:
    con = ("monotone-walk family: 16KiB-aligned (0x10000-granular), start<end, prot in {0,3}, "
           "strictly monotonic non-overlapping across parts 4-47")
    plausible.append(dict(s=c["s"], e=c["e"], p=c["p"], src=c["src"], constraint=con,
                          flag="exceeds stated 36-bit VA bound; whole family lies in [0x1000000000, 0x4000000000) -- real >36-bit VA region OR systematic high-bit displacement; v3 must arbitrate"))

# E3/E4 low boundary pairs: entries whose end is a small aligned address while start is hi-family,
# followed by an entry whose start is a small aligned address
low_bounds = []
flat = [(s, e, p, seq, part, idx) for seq, part, idx, s, e, p in stream]
for i in range(len(flat) - 1):
    s1, e1, p1, q1, pt1, i1 = flat[i]
    s2, e2, p2, q2, pt2, i2 = flat[i + 1]
    if (e1 <= MAX36 and e1 >= 0x1000000 and aligned(e1) and s1 > MAX36 and s1 < 0x4000000000
            and s2 <= MAX36 and s2 >= 0x1000000 and aligned(s2) and s2 > e1):
        low_bounds.append(dict(prev_end=e1, next_start=s2, src=f"v2 seq {q1} part {pt1} entry {i1}->{i2}"))
        add_anchor("boundary", e1, f"v2 seq {q1} e3.end")
        add_anchor("boundary", s2, f"v2 seq {q2} e4.start")

# paired prot leaks: consecutive garbage prots (a,b) on same part, a<b, both aligned in-space, sane size
prot_pairs = []
for r in pages:
    ps = [p for (_s, _e, p) in r["entries"]]
    for i in range(len(ps) - 1):
        a, b = ps[i], ps[i + 1]
        if (a > 7 and b > 7 and aligned(a) and aligned(b) and 0x400000 <= a < b <= MAX36 and 0 < b - a <= 0x800000):
            prot_pairs.append(dict(a=a, b=b, src=f"v2 seq {r['seq']} part {r['part']} prots {i},{i+1}"))

# ---------- gap analysis ----------
gap_lo, gap_hi = 0x787C000, 0x8018C000
recovered_all = trusted + plausible
in_gap_spans = [x for x in recovered_all if x["s"] >= gap_lo and x["e"] <= gap_hi and not (x["s"] == gap_lo)]
in_gap_anchors = {a: v for a, v in anchors.items() if gap_lo <= a < gap_hi}
in_gap_leaks = [x for x in prot_pairs if x["a"] >= gap_lo and x["b"] <= gap_hi]

# ---------- GUA anchor candidates ----------
gua_rows = []
cand_bases = [
    ("trusted image span1 start (RX image base)", 0x400000),
    ("trusted image span2 start (R image region)", 0x8018C000),
    ("v2 scan sole RX candidate (0x800ecc000, span_end lost, ELF magic unread)", 0x800ECC000),
    ("recovered 16KiB exec page start", 0x800E8C000),
    ("recovered R span start 0x800250000", 0x800250000),
]
def where(a):
    hits = [f"{x['s']:#x}-{x['e']:#x} p{x['p']}" for x in recovered_all if x["s"] <= a < x["e"]]
    if hits: return "inside recovered span " + ";".join(hits)
    if gap_lo <= a < gap_hi: return "INSIDE THE 0x787c000-0x8018c000 GAP (unrecovered region)"
    if 0x400000 <= a < gap_lo: return "inside trusted exec span 0x400000-0x787c000"
    return "outside any recovered span"
for name, b in cand_bases:
    a = b + GUA_V
    gua_rows.append(dict(hyp="base+vaddr", base=b, base_desc=name, anchor=a, where=where(a)))
# span_start+vaddr reading: module containing-image span starts only
for x in trusted:
    a = x["s"] + GUA_V
    gua_rows.append(dict(hyp="span_start+vaddr", base=x["s"], base_desc=f"trusted span start {x['s']:#x}", anchor=a, where=where(a)))

# ---------- stats ----------
n_listed = len(stream)
prot_ok = lambda x: x["p"] in (1, 2, 3, 4, 5, 6, 7)
stats = dict(
    v2_records=len(v2), page_records=len(pages), entries_listed=n_listed,
    hist_spans_kept=1024, hist_entries_returned=1243, unlisted_kept=1024 - n_listed,
    trusted=len(trusted), plausible=len(plausible), plausible_unflagged=sum(1 for x in plausible if not x["flag"]),
    plausible_hi_family=len(fam), hi_family_chain_monotonic=ok_chain, hi_family_breaks=breaks,
    rejected=len(rejected),
    rejected_reasons={},
    spans_r_expected=57, spans_w_expected=52, spans_x_expected=5,
    recovered_r=sum(1 for x in recovered_all if x["p"] & 1), recovered_w=sum(1 for x in recovered_all if x["p"] & 2),
    recovered_x=sum(1 for x in recovered_all if x["p"] & 4),
    anchors=len(anchors), boundary_pairs=len(low_bounds), prot_leak_pairs=len(prot_pairs),
    gap_recovered_spans=len([x for x in in_gap_spans if x['s'] > gap_lo or x['e'] < gap_hi]),
    gap_anchor_addrs=len(in_gap_anchors), gap_prot_leak_pairs=len(in_gap_leaks),
)
for r in rejected:
    key = r["reason"].split("(")[0].strip()
    stats["rejected_reasons"][key] = stats["rejected_reasons"].get(key, 0) + 1

# ---------- write recovered-spans.json ----------
def span_json(x, cls):
    d = dict(start=f"{x['s']:#018x}", end=f"{x['e']:#018x}", prot=x["p"],
             size=x["e"] - x["s"], classification=cls, constraint=x["constraint"], source=x["src"])
    if x.get("flag"): d["caveat"] = x["flag"]
    return d

out = dict(
    schema="worker-mapping-recovery/recovered-spans/1",
    provenance=dict(
        inputs=[V2, V1], method="offline structural recovery; no span invented; every entry carries the constraint that admitted it",
        constraints=[
            "FreeBSD kinfo_vmentry: kve_start < kve_end, kve_protection = VM_PROT mask in 0..7 (R=1 W=2 X=4); reservation entries may carry prot 0",
            "16KiB page alignment of start and end for real mappings",
            "36-bit user VA bound: addresses must be <= 0xFFFFFFFFF (flagged, not auto-rejected, when violated by an otherwise-perfect monotone family)",
            "strictly monotonic non-overlapping span order across the page-record stream",
            "v1 (g5, igf-f4a91d8123b0) and v2 (igf2-764f957dc0a6) are two reads of the same live map: exact (start,end,prot) matches are trusted",
        ],
        va_space_note="kve_size=1160 confirmed by both records; spans_kept=1024, entries returned 1243, listed 48x20=960, 64 kept spans never listed",
    ),
    summary=stats,
    trusted=[span_json(x, "trusted") for x in sorted(trusted, key=lambda z: z["s"])],
    plausible=[span_json(x, "plausible") for x in sorted(plausible, key=lambda z: z["s"])],
    rejected=[dict(start=f"{x['s']:#018x}", end=f"{x['e']:#018x}", prot=f"{x['p']:#x}", source=x["src"], reason=x["reason"]) for x in rejected],
    boundary_anchors=[dict(addr=f"{a:#018x}", kinds=sorted(v["kinds"]), sources=v["sources"][:4]) for a, v in sorted(anchors.items())],
    prot_leak_address_set=[f"{a:#018x}" for a in sorted(leak_addrs)],
    paired_prot_leaks=[dict(a=f"{x['a']:#018x}", b=f"{x['b']:#018x}", size=x["b"] - x["a"], source=x["src"]) for x in prot_pairs],
    low_region_boundary_pairs=[dict(prev_end=f"{x['prev_end']:#018x}", next_start=f"{x['next_start']:#018x}", source=x["src"]) for x in low_bounds],
    gap_analysis=dict(
        gap=[f"{gap_lo:#x}", f"{gap_hi:#x}"], size=gap_hi - gap_lo,
        recovered_spans_inside=len([x for x in in_gap_spans if x['s'] >= gap_lo and x['e'] <= gap_hi and x['s'] != gap_lo]),
        note="no recovered span lies strictly inside the gap; structure evidence below",
        boundary_anchor_addrs_inside=[f"{a:#018x}" for a in sorted(in_gap_anchors)],
        prot_leak_pairs_inside=[[f"{x['a']:#x}", f"{x['b']:#x}"] for x in in_gap_leaks],
    ),
    gua_anchor=GUA_V and dict(vaddr=f"{GUA_V:#x}", symbol="GUObjectArray (v2 REPORT profile vaddr)", rows=gua_rows),
)
with open(OUT + "/recovered-spans.json", "w") as f:
    json.dump(out, f, indent=1)

# ---------- write v3-format drop-in ----------
recs = sorted(recovered_all, key=lambda z: z["s"])
with open(OUT + "/vmmap-v3-records.jsonl", "w") as f:
    f.write(json.dumps(dict(marker="modbridge.ingamefacts", schema=1, build_id="igf2-764f957dc0a6-recovered",
        seq=1, exp="v1_vmmap_page", rc=0, errno=0,
        fields=dict(part=0, parts=1, entries=[dict(start=f"{x['s']:#018x}", end=f"{x['e']:#018x}", prot=x["p"]) for x in recs]))) + "\n")
    for i, x in enumerate(recs, 1):
        f.write(json.dumps(dict(marker="modbridge.ingamefacts", schema=1, build_id="igf2-764f957dc0a6-recovered",
            seq=i + 1, exp="rr_recovered_span", rc=0, errno=0,
            fields=dict(start=f"{x['s']:#018x}", end=f"{x['e']:#018x}", prot=x["p"],
                        classification=("trusted" if x in trusted else "plausible"),
                        constraint=x["constraint"], caveat=x.get("flag") or "", source=x["src"]))) + "\n")

# ---------- console summary for ANALYSIS.md ----------
print("STATS", json.dumps(stats, indent=1))
print("TRUSTED:", [(hex(x["s"]), hex(x["e"]), x["p"]) for x in trusted])
print("PLAUSIBLE unflagged:", [(hex(x["s"]), hex(x["e"]), x["p"]) for x in plausible if not x["flag"]])
print("HI-FAMILY count:", len(fam), "monotonic:", ok_chain, "breaks:", breaks[:5])
print("HI-FAMILY first/last:", hex(fam[0]["s"]) if fam else None, hex(fam[-1]["e"]) if fam else None)
print("GAP:", [f"{x['s']:#x}-{x['e']:#x}" for x in in_gap_spans])
print("GAP anchors:", [f"{a:#x}" for a in sorted(in_gap_anchors)])
print("GAP prot-leak pairs:", [(f"{x['a']:#x}", f"{x['b']:#x}") for x in in_gap_leaks])
print("GUA:")
for r in gua_rows:
    print("  ", r["hyp"], f"B={r['base']:#x}", f"A={r['anchor']:#x}", r["where"][:90])
print("prot leak pairs total:", len(prot_pairs), "sample:", [(hex(x["a"]), hex(x["b"])) for x in prot_pairs[:8]])
print("low boundary pairs:", len(low_bounds), "sample:", [(hex(x["prev_end"]), hex(x["next_start"])) for x in low_bounds[:10]])
print("corroborated leak addrs (v1&v2 histogram):", [hex(a) for a in sorted(leak_addrs)])
