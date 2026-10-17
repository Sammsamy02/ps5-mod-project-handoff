#!/usr/bin/env python3
"""derive2 — capstone-faithful replication of the shipped ps_scan routes:
(a) slot 4: static_construct_string_elf (decode 130B from each sco-string xref,
    near-call targets, consecutive pairs, intersection, ensure_one, take .1)
    + population statistics over ALL 1918 xref windows.
(b) slot 3: gmalloc_string_pe find_global (root function of deletefile xref,
    mov rcx,[rip+X]; test rcx,rcx + cmp qword [rip+X],0 candidates; recurse
    into callees depth 3; try_ensure_one).
Merges results into derivation.json. Offline read-only."""
import json, mmap, re, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

EB = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-seed-table-derivation/derivation.json"
BASE = 0x400000
DELTA = 0x4000  # LOAD0/LOAD1: file_off = vaddr + 0x4000

md = Cs(CS_ARCH_X86, CS_MODE_64)

f = open(EB, "rb")
mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)

def code(v, n):
    return mm[v + DELTA: v + DELTA + n]

def decode_calls(v, n):
    """near-call (e8 rel32) targets decoded with real instruction boundaries."""
    out = []
    for ins in md.disasm(code(v, n), v):
        if ins.mnemonic == "call" and ins.op_str.startswith("0x"):
            out.append(int(ins.op_str, 16))
    return out

R = json.load(open(OUT))

# ---- (a) SCO ----------------------------------------------------------------
sco_str = int(R["sco_string_hits"][0], 16)
sites = sorted(int(k, 16) for k in R["sco_call_targets_per_xref"])

def window_pairs(site):
    ts = decode_calls(site, 130)
    return ts, list(zip(ts, ts[1:]))

first6 = sites[:6]
per6 = [window_pairs(s)[1] for s in first6]
inter6 = set.intersection(*[set(p) for p in per6]) if per6 else set()
R["sco_capstone_pair_intersection_first6"] = [[hex(a), hex(b)] for a, b in sorted(inter6)]
# population statistics over all xref windows
from collections import Counter
pair_freq = Counter()
term_freq = Counter()
for s in sites:
    ts = decode_calls(s, 130)
    if len(ts) >= 2:
        pair_freq[(ts[-2], ts[-1])] += 1
        term_freq[ts[-1]] += 1
R["sco_terminal_call_top"] = [[hex(a), b] for a, b in term_freq.most_common(6)]
R["sco_final_pair_top"] = [[hex(a), hex(b), c] for (a, b), c in pair_freq.most_common(6)]
sco = None
if len(inter6) == 1:
    sco = next(iter(inter6))[1]
    R["sco_method"] = "first6 pair intersection (shipped semantics)"
else:
    # widest honest statistic: dominant terminal call across all windows
    if term_freq:
        top, n = term_freq.most_common(1)[0]
        R["sco_method"] = (f"dominant terminal near-call across {len(sites)} xref windows "
                           f"({n} windows); canonical triple [init, params-ctor, SCO]")
        sco = top
R["sco_linked"] = hex(sco) if sco else None
R["sco_runtime"] = hex(sco + BASE) if sco else None
if sco:
    R["sco_first_bytes"] = code(sco, 16).hex()
    R["sco_expected_prefix"] = "554889e54157415641554154534881ec"
    R["sco_prefix_match"] = R["sco_first_bytes"] == R["sco_expected_prefix"]
    o = sco + DELTA
    R["sco_file_off"] = hex(o)

# ---- (b) GMalloc find_global -------------------------------------------------
del_sites = [int(s, 16) for s in R["xref_sites"].get("deletefile-string", [])]

def root_approx(site):
    off = site + DELTA
    i = mm.rfind(b"\xcc\xcc\xcc\xcc", max(0, off - 0x2000), off)
    return (i + 4 - DELTA) if i != -1 else None

def rip_target(ins):
    """effective rip-relative target from op_str '[rip + 0x..]' / '[rip - 0x..]'"""
    m = re.search(r"\[rip ([+-]) (0x[0-9a-f]+)\]", ins.op_str)
    if not m:
        return None
    d = int(m.group(2), 16)
    return ins.address + ins.size + (d if m.group(1) == "+" else -d)

def find_global(fn, depth, searched):
    """Rust find_global: scan fn..fn+1000; mov rcx,[rip+X] followed by
    test rcx,rcx -> candidate; cmp qword [rip+X],0 -> candidate; collect callees."""
    searched.add(fn)
    mov_rcx = None
    possible = []
    calls = []
    for ins in md.disasm(code(fn, 1000), fn):
        if ins.address >= fn + 1000:
            break
        m = ins.mnemonic; o = ins.op_str
        # cmp qword ptr [rip+X], 0
        if m == "cmp" and o.startswith("qword ptr [rip") and o.endswith(", 0"):
            t = rip_target(ins)
            if t is not None:
                possible.append(t)
        # test rcx, rcx after mov rcx,[rip+X]
        if m == "test" and o == "rcx, rcx" and mov_rcx is not None:
            possible.append(mov_rcx)
        if m == "mov" and o.startswith("rcx, qword ptr [rip"):
            mov_rcx = rip_target(ins)
        else:
            mov_rcx = None
        if m == "call" and o.startswith("0x"):
            calls.append(int(o, 16))
    if len(possible) == 1:
        return [possible[0]]
    if depth > 0:
        res = []
        for c in reversed(calls):
            if c in searched:
                continue
            res += find_global(c, depth - 1, searched)
            if res:
                return res
    return []

gm_results = []
for s in del_sites:
    fn = root_approx(s)
    R.setdefault("gm_deletefile_roots", []).append({"xref": hex(s), "root": hex(fn) if fn else None})
    if fn is None:
        continue
    found = find_global(fn, 3, set())
    R["gm_deletefile_roots"][-1]["candidates"] = [hex(x) for x in found]
    gm_results += found
R["gmalloc_capstone_candidates"] = [hex(x) for x in gm_results]
R["gmalloc_capstone_unique"] = len(set(gm_results)) == 1 and len(gm_results) == 1
if gm_results:
    g = gm_results[0]
    R["gmalloc_top_aligned8"] = g % 8 == 0
    # segment lookup (reuse from R.loads info)
    loads = []
    for L in R["loads"]:
        pass
    R["gmalloc_candidate_linked"] = hex(g)
    R["gmalloc_candidate_runtime"] = hex(g + BASE)
    off = g + DELTA
    R["gmalloc_candidate_file_off"] = hex(off) if 0 <= off < len(mm) else None

json.dump(R, open(OUT, "w"), indent=1)
for k in ["sco_capstone_pair_intersection_first6", "sco_terminal_call_top",
          "sco_final_pair_top", "sco_method", "sco_linked", "sco_runtime",
          "sco_first_bytes", "sco_prefix_match", "sco_file_off",
          "gm_deletefile_roots", "gmalloc_capstone_candidates",
          "gmalloc_capstone_unique", "gmalloc_top_aligned8",
          "gmalloc_candidate_linked", "gmalloc_candidate_runtime"]:
    print(f"== {k}: {json.dumps(R.get(k))[:400]}")
