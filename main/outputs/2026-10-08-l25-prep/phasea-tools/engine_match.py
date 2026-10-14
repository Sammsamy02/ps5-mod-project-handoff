#!/usr/bin/env python3
"""engine_match.py — PHASE A item #2: engine family/version matcher for a NEW
decrypted PS5 eboot (UE4 + UE5 generalization of the Townfall profile scanner).

STATIC ANALYSIS ONLY. Reads files read-only. No console contact. Nothing here
authorizes any console step: every runtime/overlay action still needs Claude
review + explicit user OK (project law).

What it does (the "markers-file diff" generalized):
  1. image identity   sha256/size/extent of the input ELF (profile stage 1;
                      a game-2 profile must bind image identity before any pin)
  2. quick triage     a small always-present marker subset scanned first;
                      a non-Unreal executable REFUSES here in seconds
                      (the Astro Bot refusal path, made a first-class tier)
  3. full axes scan   all markers from the pinned patternsleuth
                      engine_version.rs table (30 versions, UE 4.7..5.8,
                      244 markers) scanned utf-16le over PT_LOAD slices,
                      identical semantics to worker-axes-detector ref_axes.py
  4. cost model       candidate = version x build_configuration x stats;
                      same cost weights as ref_axes.py so Townfall results
                      reproduce exactly
  5. confidence tiers REFUSED / FAMILY_ONLY / VERSION_TENTATIVE /
                      VERSION_CONFIDENT, with an anchor-pack recommendation
                      per tier (UE4-era vs UE5-era FName/GUObjectArray shapes)
  6. --diff mode      presence diff of two ELFs (which markers a new game adds
                      or drops vs a known profiled one)

Exit codes: 0 = report produced (any tier); 2 = input error (not ELF, fake-SELF
container, unreadable). REFUSED is a SUCCESS exit: refusal is correct behavior.

Usage:
  python3 engine_match.py IMAGE.elf [--markers engine_version.rs] [--json]
  python3 engine_match.py --diff A.elf B.elf [--markers engine_version.rs] [--json]
"""

import hashlib
import json
import re
import struct
import sys
from pathlib import Path

DEFAULT_MARKERS = (
    "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/"
    "native-dependency-spike/ue4ss-native-patternsleuth-1/upstream/"
    "patternsleuth/src/resolvers/unreal/engine_version.rs"
)

# Tier thresholds (documented, tunable; defaults validated on Townfall + Astro)
TRIAGE_MIN_PRESENT = 3      # fewer triage hits => REFUSED without full scan
REFUSED_MAX_PRESENT = 8     # full scan: below this we do not claim Unreal at all
VERSION_MARGIN_CONFIDENT = 12  # == Townfall stats margin; smaller = softer
VERSION_MARGIN_TENTATIVE = 4   # == ref_axes.py resolution threshold
FAMILY_CONFIDENT_PRESENT = 25  # enough markers to trust the family axis
# [GW test-19 fix, 2026-10-10] ABSOLUTE-fit guard: a large relative margin can
# coexist with garbage evidence (a MODIFIED build: foreign markers present,
# native ones destroyed — cost 103 vs baseline 43 while the margin stayed 20).
# CONFIDENT therefore also requires the best candidate's total misfit
# (best_cost) to stay within 1.0 cost-units per PRESENT marker. Baseline
# Townfall: 43/54 = 0.796 PASSES; the test-19 tampered fixture 79/48 = 1.65
# is demoted to VERSION_TENTATIVE. Without this guard the tool pinned
# CONFIDENT offsets onto a modified build (Claude round-6 #2a finding).
VERSION_CONFIDENT_MAX_MISFIT = 1.0


# ---------------------------------------------------------------- markers ----

def load_markers(rs_path):
    """Parse VERSIONS + MARKERS out of engine_version.rs (same regexes as
    ref_axes.py; keep the two parsers in lockstep or agreement checks lie)."""
    src = open(rs_path, encoding="utf-8").read()
    vtab = re.search(r"const VERSIONS:\s*&\[EngineVersion\]\s*=\s*&\[(.*?)\];",
                     src, re.S).group(1)
    versions = [tuple(map(int, v)) for v in
                re.findall(r"v\((\d+),\s*(\d+)\)", vtab)]
    vindex = {v: i for i, v in enumerate(versions)}
    mtab = re.search(r"const MARKERS:\s*&\[Marker\]\s*=\s*&\[(.*?)\n\];",
                     src, re.S).group(1)
    markers = []
    for m in re.finditer(
            r'Marker\s*\{\s*string:\s*("(?:[^"\\]|\\.)*")\s*,'
            r'\s*gates:\s*&\[((?:[A-Za-z_][A-Za-z0-9_]*)?)\]\s*,'
            r'\s*first:\s*(None|Some\(\s*v\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\))\s*,'
            r'\s*last:\s*(None|Some\(\s*v\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\))\s*,?'
            r'\s*\}', mtab):
        s = json.loads(m.group(1))
        gate = m.group(2) or ""
        first = None if m.group(3) == "None" else (int(m.group(4)), int(m.group(5)))
        last = None if m.group(6) == "None" else (int(m.group(7)), int(m.group(8)))
        markers.append({"string": s, "gate": gate,
                        "first": first, "last": last,
                        "first_idx": vindex[first] if first else 0xFF,
                        "last_idx": vindex[last] if last else 0xFF})
    return versions, markers


def covers(marker, version_idx, cfg, stats_on):
    """ref_axes.py gate semantics, verbatim."""
    gate = marker["gate"]
    if gate == "NotShipping" and cfg == 2:
        return False
    if gate == "DevOnly" and cfg != 0:
        return False
    if gate == "TestOrShipping" and cfg == 0:
        return False
    if gate == "ShippingOnly" and cfg != 2:
        return False
    if gate == "WithStats" and not stats_on:
        return False
    first, last = marker["first_idx"], marker["last_idx"]
    if first == 0xFF and last == 0xFF:
        return True
    if version_idx < 0:
        return False
    if first != 0xFF and version_idx < first:
        return False
    if last != 0xFF and version_idx > last:
        return False
    return True


# ------------------------------------------------------------------- image ----

def parse_image(path):
    """Plain-ELF identity + PT_LOAD spans. Rejects fake-SELF containers with a
    pointer at the make_fself extraction path (never treat a download as the
    real file without decrypt-off — PS5 AGENTS law)."""
    p = Path(path)
    h = hashlib.sha256()
    size = 0
    with p.open("rb") as f:
        head = f.read(64)
        if len(head) < 64:
            raise ValueError("file too small to be an ELF")
        if head[:4] == b"\x7fELF":
            pass
        elif head[:4] in (bytes.fromhex("4f153d1d"), bytes.fromhex("5414f5ee")):
            raise ValueError(
                "SELF container magic %s: this is a fake/signed SELF, not the "
                "plain ELF. Reconstruct the inner ELF from the fake-SELF "
                "payload (see FZ-GAME2/astro_fakeself_dryrun.py) or download "
                "the decrypt-on view per POSTMORTEM rules; never re-upload it."
                % head[:4].hex())
        else:
            raise ValueError("not an ELF (magic %s)" % head[:4].hex())
        if struct.unpack_from("<H", head, 18)[0] != 0x3e:
            raise ValueError("not x86-64")
        f.seek(0)
        while True:
            chunk = f.read(1 << 20)
            if not chunk:
                break
            size += len(chunk)
            h.update(chunk)
    data_head = head
    phoff = struct.unpack_from("<Q", data_head, 32)[0]
    phentsize = struct.unpack_from("<H", data_head, 54)[0]
    phnum = struct.unpack_from("<H", data_head, 56)[0]
    loads = []
    extent = 0
    with p.open("rb") as f:
        f.seek(phoff)
        raw = f.read(phentsize * phnum)
    for i in range(phnum):
        e = raw[i * phentsize:(i + 1) * phentsize]
        ptype, = struct.unpack_from("<I", e, 0)
        if ptype != 1:
            continue
        off, va, _pa, filesz, memsz, _al = struct.unpack_from("<QQQQQQ", e, 8)
        loads.append({"off": off, "vaddr": va, "filesz": filesz, "memsz": memsz})
        extent = max(extent, va + memsz)
    return {"path": str(p), "sha256": h.hexdigest(), "size": size,
            "extent": extent, "loads": loads}


def scan_markers(image, markers, only_idx=None):
    """utf-16le presence scan over file-backed PT_LOAD slices (ref_axes.py
    semantics). only_idx restricts to a triage subset."""
    needles = [(m["string"].encode("utf-16-le") if isinstance(m["string"], str)
                else m["string"]) for m in markers]
    present = [False] * len(markers)
    with open(image["path"], "rb") as f:
        for L in image["loads"]:
            f.seek(L["off"])
            seg = f.read(L["filesz"])
            for i, nd in enumerate(needles):
                if only_idx is not None and i not in only_idx:
                    continue
                if not present[i] and seg.find(nd) != -1:
                    present[i] = True
    return present


# ------------------------------------------------------------- classify -----

def triage_indices(versions, markers):
    """Cheap pre-scan set: the UNGATED markers with the WIDEST version
    coverage (no marker in this table is truly version-unbounded, but the
    widest 16 cover 4.8..5.x and are present in every known UE shipping
    image; 14/16 present in Townfall). A non-Unreal executable misses nearly
    all of them, so we can REFUSE in seconds without the 244-marker scan."""
    ranked = sorted(
        ((m["last_idx"] - m["first_idx"], i) for i, m in enumerate(markers)
         if m["gate"] == ""),
        reverse=True)
    seen, out = set(), []
    for _cov, i in ranked:
        s = markers[i]["string"]
        if s not in seen:
            seen.add(s)
            out.append(i)
        if len(out) >= 16:
            break
    return out


def cost_solve(versions, markers, present):
    ncand = (len(versions) + 1) * 6
    costs = []
    for c in range(ncand):
        vi, cfg, on = c // 6 - 1, (c // 2) % 3, c % 2
        cost = 0
        for mi, marker in enumerate(markers):
            covered = covers(marker, vi, cfg, on)
            if not covered and present[mi]:
                cost += 5
            elif covered and not present[mi]:
                cost += 1
        costs.append(cost)

    def resolve(axis):
        best = min(range(ncand), key=lambda c: (costs[c], c))
        vi, cfg, on = best // 6 - 1, (best // 2) % 3, best % 2
        val = (vi, cfg, on)[axis]
        others = [costs[c] for c in range(ncand)
                  if (c // 6 - 1, (c // 2) % 3, c % 2)[axis] != val]
        runner = min(others) if others else costs[best]
        margin = runner - costs[best]
        return val, costs[best], margin

    vi, v_cost, v_margin = resolve(0)
    cfg_v, cfg_cost, cfg_margin = resolve(1)
    st_v, st_cost, st_margin = resolve(2)
    # family axis: does every near-best version candidate share a major?
    best_cost = min(costs)
    near_majors = sorted({versions[c // 6 - 1][0] for c in range(ncand)
                          if c // 6 - 1 >= 0 and costs[c] <= best_cost + VERSION_MARGIN_TENTATIVE})
    return {
        "version_idx": vi, "version": None if vi in (None, -1) else "%d.%d" % versions[vi],
        "version_cost": v_cost, "version_margin": v_margin,
        "build_configuration_code": 0 if cfg_v is None else {0: 2, 1: 3, 2: 1}[cfg_v],
        "build_configuration": None if cfg_v is None else {0: "debug", 1: "test", 2: "shipping"}[cfg_v],
        "config_margin": cfg_margin,
        "stats_code": 0 if st_v is None else (2 if st_v == 1 else 1),
        "stats": None if st_v is None else ("on" if st_v == 1 else "off"),
        "stats_margin": st_margin,
        "near_major_versions": near_majors,
        "best_cost": best_cost,
    }


def anchor_pack_recommendation(family, version):
    """Which anchor/static-analysis pack the runtime profile generator should
    try next. UE4-era engines have different FName/GUObjectArray shapes:
      - UE4 < 4.23: FUObjectArray pre-chunked layout; FName TNameEntryArray
        (per-thread 2^k slots, case-preserving compile flag variants)
      - UE4 >= 4.23: chunked FUObjectItem arrays; FNamePool introduced
      - UE5.x: FNamePool block/stride allocator (current Townfall pack),
        FField replaces UProperty (4.25+)
    These are RESEARCH labels for the profile generator; anchors still have to
    be multi-pattern verified per image with refusal on disagreement."""
    if family is None:
        return "none (refused)"
    if family == "UE4":
        if version and float(version) < 4.23:
            return "ue4-legacy (pre-4.23 FUObjectArray + TNameEntryArray FName; NO current pack exists - author first)"
        return "ue4-chunked (4.23+ FUObjectItem chunks + FNamePool-era; partial overlap with UE5 pack, verify each anchor)"
    if family == "UE5":
        return "ue5-fnamepool (current Townfall pack basis: GUObjectArray 9-pattern + GMalloc 5-pattern + FName ctor slot; verify per-image)"
    return "unknown"


def classify(image, versions, markers):
    tri = triage_indices(versions, markers)
    tri_present = scan_markers(image, markers, only_idx=set(tri))
    tri_hits = sum(1 for i in tri if tri_present[i])
    triage = {"set_size": len(tri), "present": tri_hits}
    if tri_hits < TRIAGE_MIN_PRESENT:
        present = [False] * len(markers)
        full_scan_done = False
    else:
        present = scan_markers(image, markers)
        full_scan_done = True
    present_count = sum(present)

    report = {
        "tool": "engine_match.py (phasea #2)",
        "image": {k: image[k] for k in ("path", "sha256", "size", "extent")},
        "marker_source_versions": len(versions),
        "markers_total": len(markers),
        "triage": triage,
        "full_scan_done": full_scan_done,
        "present_count": present_count,
    }

    if tri_hits < TRIAGE_MIN_PRESENT or present_count < REFUSED_MAX_PRESENT:
        report.update({
            "tier": "REFUSED",
            "tier_meaning": "not an Unreal Engine executable (or markers below evidence floor); profile generation MUST refuse - this is the Astro Bot path and it is correct behavior",
            "family": None, "version": None,
            "anchor_pack": "none",
            "next_step": "stop; no UE4SS profile can be generated for this image",
        })
        return report, present

    sol = cost_solve(versions, markers, present)
    family_majors = sol["near_major_versions"]
    family = ("UE4" if family_majors == [4] else
              "UE5" if family_majors == [5] else None)
    misfit_ratio = sol["best_cost"] / float(present_count)  # present_count >= 8 here
    report.update({
        "axes": sol,
        "family": family,
        "family_confident": (family is not None
                             and present_count >= FAMILY_CONFIDENT_PRESENT),
        "version": sol["version"] if sol["version_margin"] >= VERSION_MARGIN_TENTATIVE else None,
        "misfit_ratio": misfit_ratio,
    })

    if (report["version"] and sol["version_margin"] >= VERSION_MARGIN_CONFIDENT
            and misfit_ratio <= VERSION_CONFIDENT_MAX_MISFIT):
        tier = "VERSION_CONFIDENT"
        meaning = ("family + version + axes resolved with strong margins AND "
                   "absolute misfit within VERSION_CONFIDENT_MAX_MISFIT "
                   "(best_cost <= present_count); proceed to anchor-pin "
                   "candidate scan (multi-pattern agreement) then static-"
                   "layout windows; two-source agreement required, refusal "
                   "on mismatch")
    elif (report["version"] and sol["version_margin"] >= VERSION_MARGIN_CONFIDENT
            and misfit_ratio > VERSION_CONFIDENT_MAX_MISFIT):
        tier = "VERSION_TENTATIVE"
        meaning = ("relative margin is CONFIDENT-grade but the absolute "
                   "misfit ratio %.2f exceeds VERSION_CONFIDENT_MAX_MISFIT: "
                   "the marker evidence is internally inconsistent (modified/"
                   "unrecognized Unreal build shape). Demoted from CONFIDENT "
                   "by the test-19 guard; anchors may proceed as CANDIDATES "
                   "only; every pin needs the image identity bound" % misfit_ratio)
    elif report["version"]:
        tier = "VERSION_TENTATIVE"
        meaning = ("version resolved above the ref_axes threshold but below "
                   "the profile margin; anchors may proceed as CANDIDATES "
                   "only; every pin needs the image identity bound")
    elif family:
        tier = "FAMILY_ONLY"
        meaning = ("UE4 vs UE5 discriminated but the minor version is "
                   "ambiguous; run the anchor scan with the family pack and "
                   "let multi-pattern agreement + layout windows select exact "
                   "offsets; do NOT emit a versioned profile yet")
    else:
        tier = "REFUSED"
        meaning = ("markers present but no consistent engine candidate; "
                   "treat as unknown engine, refuse profile generation")
    report.update({
        "tier": tier,
        "tier_meaning": meaning,
        "anchor_pack": anchor_pack_recommendation(family, report["version"]),
        "next_step": {
            "REFUSED": "stop",
            "FAMILY_ONLY": "anchor scan with family pack; no versioned profile",
            "VERSION_TENTATIVE": "anchor scan; pins stay candidates until layout windows agree",
            "VERSION_CONFIDENT": "full stage 3-4 pipeline (anchors, member/vtable offsets, unit emission)",
        }[tier],
    })
    return report, present


# ------------------------------------------------------------------ diff ----

def diff_images(a_report, a_present, b_report, b_present, markers):
    rows = []
    for i, m in enumerate(markers):
        if a_present[i] != b_present[i]:
            rows.append({
                "string": m["string"][:80],
                "gate": m["gate"] or None,
                "first": "%s.%s" % m["first"] if m["first"] else None,
                "last": "%s.%s" % m["last"] if m["last"] else None,
                "in_a": a_present[i], "in_b": b_present[i],
            })
    return {
        "a": {"path": a_report["image"]["path"], "tier": a_report["tier"],
              "family": a_report["family"], "version": a_report["version"],
              "present_count": a_report["present_count"]},
        "b": {"path": b_report["image"]["path"], "tier": b_report["tier"],
              "family": b_report["family"], "version": b_report["version"],
              "present_count": b_report["present_count"]},
        "differing_markers": rows,
        "differing_count": len(rows),
    }


# ------------------------------------------------------------------ main ----

def main(argv):
    args = list(argv[1:])
    json_out = "--json" in args
    args = [a for a in args if a != "--json"]
    markers_path = DEFAULT_MARKERS
    if "--markers" in args:
        i = args.index("--markers")
        markers_path = args[i + 1]
        del args[i:i + 2]

    try:
        versions, markers = load_markers(markers_path)
    except OSError as e:
        print("markers file error: %s" % e, file=sys.stderr)
        return 2

    if not args:
        print(__doc__)
        return 2

    try:
        if args[0] == "--diff":
            if len(args) < 3:
                print("--diff needs two ELFs", file=sys.stderr)
                return 2
            ra, pa = classify(parse_image(args[1]), versions, markers)
            rb, pb = classify(parse_image(args[2]), versions, markers)
            out = diff_images(ra, pa, rb, pb, markers)
        else:
            out, _ = classify(parse_image(args[0]), versions, markers)
    except ValueError as e:
        print("input error: %s" % e, file=sys.stderr)
        return 2

    if json_out:
        print(json.dumps(out, indent=1))
    else:
        if "tier" in out:
            print("image: %s" % out["image"]["path"])
            print("sha256: %s  size=%d extent=0x%x" %
                  (out["image"]["sha256"][:16] + "...", out["image"]["size"],
                   out["image"]["extent"]))
            print("triage: %d/%d  full_scan=%s  present=%d/%d" %
                  (out["triage"]["present"], out["triage"]["set_size"],
                   out["full_scan_done"], out["present_count"], out["markers_total"]))
            print("TIER: %s" % out["tier"])
            if out.get("family"):
                print("family: %s (confident=%s)  version: %s" %
                      (out["family"], out["family_confident"], out["version"]))
            if out.get("axes"):
                a = out["axes"]
                print("axes: build=%s(margin %d) stats=%s(margin %d) "
                      "version_margin=%d near_majors=%s" %
                      (a["build_configuration"], a["config_margin"],
                       a["stats"], a["stats_margin"], a["version_margin"],
                       a["near_major_versions"]))
            print("anchor_pack: %s" % out["anchor_pack"])
            print("meaning: %s" % out["tier_meaning"])
        else:
            d = out
            print("A: %s tier=%s family=%s ver=%s present=%d" %
                  (d["a"]["path"], d["a"]["tier"], d["a"]["family"],
                   d["a"]["version"], d["a"]["present_count"]))
            print("B: %s tier=%s family=%s ver=%s present=%d" %
                  (d["b"]["path"], d["b"]["tier"], d["b"]["family"],
                   d["b"]["version"], d["b"]["present_count"]))
            print("differing markers: %d" % d["differing_count"])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
