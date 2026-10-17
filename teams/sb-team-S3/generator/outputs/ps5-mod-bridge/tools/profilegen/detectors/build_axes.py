"""Build configuration (Shipping / Development / Test) and STATS, from marker strings.

Method (a port of patternsleuth's ``BuildConfiguration`` / ``Stats`` resolvers, the same ones
UE4SS-RE uses on PC): an Unreal binary contains a different set of UTF-16 CVar names and log
strings depending on the compile-time configuration and engine version -- ``#if !UE_BUILD_SHIPPING``
strings exist only in non-shipping builds, ``#if STATS`` strings only with stats, and so on.
``data/engine_markers.json`` lists 244 such strings, each with a *gate* and an engine-version
window.  For every candidate (version x configuration x stats) the cost is

    5 per marker present in the image but not covered by the candidate      (contradiction)
  + 1 per marker covered by the candidate but absent from the image          (omission)

An axis is only reported when the cheapest candidate beats every candidate that differs on that
axis by at least ``MIN_MARGIN`` and at least ``MIN_MARKERS_PRESENT`` markers were found at all.
Below that the axis is left undetected, never guessed.
"""
from __future__ import annotations

import json
import os
from typing import Dict, List, Optional, Tuple

from ..model import HIGH, MEDIUM, Context, Hex

CONTRADICTION = 5
OMISSION = 1
MIN_MARGIN = 4
MIN_MARKERS_PRESENT = 10

_DATA = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "engine_markers.json")
CONFIG_NAMES = {0: "development", 1: "test", 2: "shipping"}      # candidate index -> name


def load_table(path: str = _DATA) -> dict:
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def presence(elf, markers: List[dict], scan_exec: bool = False) -> List[bool]:
    """Which marker strings occur (as UTF-16LE, anywhere, any alignment) in the file-backed image."""
    needles = [m["s"].encode("utf-16-le") for m in markers]
    present = [False] * len(markers)
    segs = elf.data_segments if not scan_exec else list(elf.loads)
    for pass_no in range(2):
        for i, nd in enumerate(needles):
            if present[i]:
                continue
            for s in segs:
                if elf.data.find(nd, s.offset, s.offset + s.filesz) != -1:
                    present[i] = True
                    break
        if pass_no == 0 and sum(present) < MIN_MARKERS_PRESENT and not scan_exec:
            segs = elf.other_segments                              # fall back to code / unreadable segments
            continue
        break
    return present


def covers(marker: dict, version_idx: int, cfg: int, stats_on: bool) -> bool:
    gate, first, last = marker["gate"], marker["first"], marker["last"]
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
    if first is None and last is None:
        return True
    if version_idx < 0:
        return False
    if first is not None and version_idx < first:
        return False
    if last is not None and version_idx > last:
        return False
    return True


def costs(markers: List[dict], present: List[bool], n_versions: int) -> List[Tuple[int, int, int, int]]:
    """[(cost, version_idx, cfg, stats_on)] for every candidate; version_idx -1 = unknown version."""
    out = []
    for vi in range(-1, n_versions):
        for cfg in range(3):
            for st in (False, True):
                c = 0
                for m, here in zip(markers, present):
                    cov = covers(m, vi, cfg, st)
                    if here and not cov:
                        c += CONTRADICTION
                    elif cov and not here:
                        c += OMISSION
                out.append((c, vi, cfg, int(st)))
    return out


def resolve(cands: List[Tuple[int, int, int, int]], field: int) -> Tuple[Optional[int], int, int]:
    """(value, best_cost, runner_up_cost) for one axis.

    ``field`` indexes the candidate tuple: 1 = engine version, 2 = configuration, 3 = stats.
    The value is None when the cheapest candidate does not win by ``MIN_MARGIN``."""
    best = min(cands, key=lambda c: (c[0], c[1:]))
    val = best[field]
    runner = min(c[0] for c in cands if c[field] != val)
    return (None if runner - best[0] < MIN_MARGIN else val), best[0], runner


def detect(ctx: Context) -> None:
    table = ctx.options.get("marker_table") or load_table()
    markers, versions = table["markers"], table["versions"]
    present = presence(ctx.elf, markers)
    n_present = sum(present)
    ctx.facts["axes.markers_present"] = n_present
    if n_present < MIN_MARKERS_PRESENT:
        for p in ("build.configuration", "build.stats", "build.non_shipping"):
            ctx.fail(p, "only %d of %d fingerprint markers found (need %d); axis left undetected" % (
                n_present, len(markers), MIN_MARKERS_PRESENT))
        return
    cands = costs(markers, present, len(versions))
    cfg_v, cfg_best, cfg_runner = resolve(cands, 2)
    stats_v, st_best, st_runner = resolve(cands, 3)
    ver_idx, ver_best, ver_runner = resolve(cands, 1)
    ver_text = None if ver_idx in (None, -1) else "%d.%d" % tuple(versions[ver_idx])
    ctx.facts["axes.version"] = ver_text
    ctx.facts["axes.version_margin"] = ver_runner - ver_best

    shipping_markers = [m["s"] for m, h in zip(markers, present) if h and m["gate"] == "ShippingOnly"]
    nonship_present = [m["s"] for m, h in zip(markers, present) if h and m["gate"] in ("NotShipping", "DevOnly")]
    stats_present = [m["s"] for m, h in zip(markers, present) if h and m["gate"] == "WithStats"]
    n_by_gate: Dict[str, Tuple[int, int]] = {}
    for g in ("NotShipping", "DevOnly", "TestOrShipping", "ShippingOnly", "WithStats"):
        total = sum(1 for m in markers if m["gate"] == g)
        here = sum(1 for m, h in zip(markers, present) if h and m["gate"] == g)
        n_by_gate[g] = (here, total)
    gate_txt = "; ".join("%s %d/%d" % (g, a, b) for g, (a, b) in n_by_gate.items())

    ev_common = ["%d of %d fingerprint markers present" % (n_present, len(markers)),
                 "markers present per gate: " + gate_txt]
    if cfg_v is None:
        ctx.fail("build.configuration", "cost margin %d below the required %d" % (cfg_runner - cfg_best, MIN_MARGIN))
    else:
        name = CONFIG_NAMES[cfg_v]
        margin = cfg_runner - cfg_best
        # high when the independent gate evidence points the same way, medium otherwise
        gate_agrees = (cfg_v == 2 and not nonship_present and len(shipping_markers) > 0) or \
                      (cfg_v == 0 and bool(nonship_present)) or (cfg_v == 1)
        ctx.put("build.configuration", name, HIGH if gate_agrees else MEDIUM, "axes.marker_fingerprint",
                ev_common + ["best candidate cost %d, runner-up differing on this axis %d (margin %d, need %d)" % (
                    cfg_best, cfg_runner, margin, MIN_MARGIN)])
        ctx.put("build.non_shipping", cfg_v != 2, HIGH if gate_agrees else MEDIUM, "axes.marker_fingerprint",
                ["derived from build.configuration = %s" % name])
        ctx.put("build.configuration_margin", Hex(margin), HIGH, "axes.marker_fingerprint",
                ["cost %d vs %d" % (cfg_best, cfg_runner)])
    if stats_v is None:
        ctx.fail("build.stats", "cost margin %d below the required %d" % (st_runner - st_best, MIN_MARGIN))
    else:
        margin = st_runner - st_best
        ctx.put("build.stats", bool(stats_v), HIGH if (stats_v == 0 and not stats_present) or (stats_v and stats_present)
                else MEDIUM, "axes.marker_fingerprint",
                ev_common + ["best candidate cost %d, runner-up differing on this axis %d (margin %d, need %d)" % (
                    st_best, st_runner, margin, MIN_MARGIN)])
        ctx.put("build.stats_margin", Hex(margin), HIGH, "axes.marker_fingerprint", ["cost %d vs %d" % (st_best, st_runner)])
    ctx.put("build.axes_markers_present", Hex(n_present), HIGH, "axes.marker_fingerprint",
            ["marker strings found as UTF-16 in the image: %d (need %d)" % (n_present, MIN_MARKERS_PRESENT)])
    ctx.put("build.axes_markers_total", Hex(len(markers)), HIGH, "axes.marker_fingerprint",
            ["size of the marker table (data/engine_markers.json)"])
    ctx.facts["axes.cfg"] = cfg_v
    ctx.facts["axes.stats"] = stats_v
    ctx.evidence_files["axes"] = {
        "present_markers": [m["s"] for m, h in zip(markers, present) if h],
        "cost": {"config": [cfg_best, cfg_runner], "stats": [st_best, st_runner], "version": [ver_best, ver_runner]},
        "version": ver_text,
    }
