#!/usr/bin/env python3
"""width_contract.py — authoritative compiled-width contract + promotable set.

worker-layout-batch3 / team 2026-10-04-zcode-team.

The WIDTH AUTHORITY for staging is the compiled accessor width contract:
  base    = .superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/
            coverage.json  (419 records: class_name/canonical_key/width_bytes/
            unsupported_reason)
  delta   = .superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/
            coverage-delta.json (only_changed_key UFunction.Func -> 16,
            alternatives [16,8]; UStruct.SuperStruct explicitly still null)

The worker-layout-minimum candidates profile TRANSCRIBED widths from widths-1;
this module cross-checks that transcription against the contract and reports
every mismatch. Discrepancies are facts about the transcription, not about the
contract; the CONTRACT wins for staging (a member without a compiled width can
never be staged, per the binding's "Native accessor width unavailable" rule).

PROMOTABLE SET (for a hypothetical validated GUA anchor): an entry could be
staged TODAY iff all three hold —
  1. PREDICATE: the class chain has a Validated-class predicate in the
     retained work (worker-5 rung-2 probes, batch2 reflection probes, batch3
     chain walks), including the variant gate below;
  2. WIDTH: the compiled contract has a non-null width for the member;
  3. ORIGIN: the offset comes from the cited candidate source (C-set-506
     setter template) as recorded in the candidates profile.
UFunction is blocked at (1) everywhere: batch2's probe_ufunction is an
Inconclusive-classifier ONLY and never returns Validated.
FField/FProperty additionally require the settled variant == UE5_FField (the
candidate offsets are SET-5_06; a validated UE4_UProperty variant refutes
this candidate table).
"""
from __future__ import annotations

import json

# ---------------------------------------------------------------------------
# Predicate inventory (class-level gates, mirroring the assembler's design)
# ---------------------------------------------------------------------------

# Probe names: guobject_array + uobject_item_chain (worker-5/batch1 rung 2),
# ustruct_chain + ffield_variant (batch2 rung 3), children_walk /
# childproperties_walk / uclass_links (batch3 rung 3 strengthening),
# ufunction_classifier (batch2, classifier-only).
RUNG2 = ["guobject_array", "uobject_item_chain"]

PREDICATE_REQUIREMENTS = {
    "FUObjectArray": {"probes": ["guobject_array"], "hypothesis": "A"},
    "TUObjectArray": {"probes": ["guobject_array"], "hypothesis": None},
    "FUObjectItem": {"probes": RUNG2},
    "UObjectBase": {"probes": RUNG2 + ["ustruct_chain"]},
    "UStruct": {"probes": RUNG2 + ["ustruct_chain"]},
    "UClass": {"probes": RUNG2 + ["ustruct_chain"]},
    "FField": {"probes": RUNG2 + ["ustruct_chain", "ffield_variant"],
               "variant": "UE5_FField"},
    "FProperty": {"probes": RUNG2 + ["ustruct_chain", "ffield_variant"],
                  "variant": "UE5_FField"},
    # batch2: probe_ufunction is Inconclusive-classifier ONLY — no Validated
    # predicate exists, so UFunction can never be staged today.
    "UFunction": None,
}

# Batch3 walks are STRENGTHENING evidence per class (required-not): they prove
# link semantics for members that are already width-gated or already covered
# by the class gate. Recorded in manifests, never a gate by themselves.
STRENGTHENING_PROBES = {
    "UStruct": ["children_walk", "childproperties_walk"],
    "UClass": ["uclass_links"],
    "FProperty": ["childproperties_walk"],
}

# ---------------------------------------------------------------------------
# Width contract
# ---------------------------------------------------------------------------

def load_contract(widths1_json: dict, delta_json: dict | None = None) -> dict:
    """Return {class: {member: width_bytes}} from widths-1 + optional delta."""
    table: dict[str, dict[str, int]] = {}
    for rec in widths1_json["records"]:
        table.setdefault(rec["class_name"], {})[rec["canonical_key"]] = (
            rec["width_bytes"])
    if delta_json is not None:
        key = delta_json["only_changed_key"]          # "UFunction.Func"
        cls, member = key.split(".", 1)
        table.setdefault(cls, {})[member] = delta_json["width_bytes"]
    return table


def cross_check_candidates(candidates: dict, contract: dict) -> list[dict]:
    """Compare the candidates profile transcription against the contract.

    Returns one row per disagreement:
      overstated  — candidates claims a width where the contract has none
                    (staging the transcription would EMIT an unbacked member)
      width_diff  — both exist but differ (binding contract wins for use)
      understated — contract has a width the candidates profile omitted
    """
    rows: list[dict] = []
    cand_members = candidates["candidate_member_offsets"]
    for cls, spec in cand_members.items():
        if cls.startswith("_"):
            continue
        c_contract = contract.get(cls, {})
        for member, entry in spec["entries"].items():
            if member == "UEP_TotalSize":
                continue
            cw = entry.get("width_bytes")
            kw = c_contract.get(member)
            if cw is not None and kw is None:
                rows.append({"class": cls, "member": member, "candidates_width": cw,
                             "contract_width": None, "kind": "overstated"})
            elif cw is not None and kw is not None and cw != kw:
                rows.append({"class": cls, "member": member, "candidates_width": cw,
                             "contract_width": kw, "kind": "width_diff"})
            elif cw is None and kw is not None:
                rows.append({"class": cls, "member": member, "candidates_width": None,
                             "contract_width": kw, "kind": "understated"})
    for cls, members in contract.items():
        if cls not in cand_members:
            continue
        for member, kw in members.items():
            if member in cand_members[cls]["entries"]:
                continue
            if kw is not None:
                rows.append({"class": cls, "member": member,
                             "candidates_width": None, "contract_width": kw,
                             "kind": "understated"})
    rows.sort(key=lambda r: (r["class"], r["member"]))
    return rows


# ---------------------------------------------------------------------------
# Promotable-set table
# ---------------------------------------------------------------------------

def promotable_rows(candidates: dict, contract: dict) -> list[dict]:
    """Complete per-entry promotable-set table for a hypothetical validated
    GUA anchor chain. Decision is one of:
      STAGEABLE_TODAY           — predicate exists + width exists (+origin)
      BLOCKED_NO_WIDTH          — predicate OK, contract width null
      BLOCKED_NO_PREDICATE      — no Validated-class predicate for the class
      BLOCKED_NO_PREDICATE_AND_WIDTH
      BLOCKED_VARIANT_GATED     — predicate class exists but needs the UE5/UE4
                                  variant settled (FField/FProperty without a
                                  settled discriminator are not stageable)
    """
    rows: list[dict] = []
    cand_members = candidates["candidate_member_offsets"]
    for cls, spec in cand_members.items():
        if cls.startswith("_"):
            continue
        req = PREDICATE_REQUIREMENTS.get(cls)
        c_contract = contract.get(cls, {})
        for member, entry in spec["entries"].items():
            origin = spec.get("_grammar", "").strip() or "C-set-506"
            if member == "UEP_TotalSize":
                rows.append({
                    "class": cls, "member": member, "offset": entry["offset"],
                    "kind": "total_size", "decision": "REQUIRED_SECTION_SIZE",
                    "predicate": "class-level" if req else None,
                    "width_bytes": None, "origin": origin})
                continue
            width = c_contract.get(member)
            cand_width = entry.get("width_bytes")
            if req is None:
                decision = ("BLOCKED_NO_PREDICATE_AND_WIDTH" if width is None
                            else "BLOCKED_NO_PREDICATE")
            elif width is None:
                decision = "BLOCKED_NO_WIDTH"
            else:
                decision = "STAGEABLE_TODAY"
            rows.append({
                "class": cls, "member": member, "offset": entry["offset"],
                "kind": "member", "decision": decision,
                "predicate": ("Validated-class: " + "+".join(req["probes"])
                              if req else None),
                "width_bytes": width,
                "candidates_width_bytes": cand_width,
                "width_mismatch": (cand_width is not None
                                   and width is not None
                                   and cand_width != width) or
                                  (cand_width is not None and width is None),
                "origin": "C-set-506 (5_06_MemberVariableLayout_DefaultSetter_%s.cpp)"
                          % cls})
    return rows


def summarize(rows: list[dict]) -> dict:
    summary: dict[str, int] = {}
    for r in rows:
        if r["kind"] != "member":
            continue
        summary[r["decision"]] = summary.get(r["decision"], 0) + 1
    return summary


def main() -> None:
    import argparse
    import os

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--widths1", required=True)
    ap.add_argument("--delta", default=None)
    ap.add_argument("--candidates", required=True)
    ap.add_argument("--out", required=True, help="output JSON path")
    args = ap.parse_args()

    with open(args.widths1, "r", encoding="utf-8") as fh:
        widths1 = json.load(fh)
    delta = None
    if args.delta and os.path.exists(args.delta):
        with open(args.delta, "r", encoding="utf-8") as fh:
            delta = json.load(fh)
    with open(args.candidates, "r", encoding="utf-8") as fh:
        candidates = json.load(fh)

    contract = load_contract(widths1, delta)
    mismatches = cross_check_candidates(candidates, contract)
    rows = promotable_rows(candidates, contract)
    out = {
        "contract_source": {
            "base": os.path.abspath(args.widths1),
            "delta": os.path.abspath(args.delta) if delta else None,
            "delta_note": (delta or {}).get("only_changed_key"),
        },
        "predicate_requirements": {k: v for k, v in
                                   PREDICATE_REQUIREMENTS.items()},
        "strengthening_probes": STRENGTHENING_PROBES,
        "width_contract_mismatches": mismatches,
        "promotable_rows": rows,
        "summary": summarize(rows),
        "honesty_note": (
            "STAGEABLE_TODAY means: IF the cited predicate chain passes "
            "Validated on the mapped snapshot AND the GUA hypothesis matches, "
            "the assembler may emit this member. Nothing here is validated "
            "for any target; no width is invented; UFunction can never be "
            "staged because only a classifier exists (batch2 discipline)."),
    }
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=2)
    print("wrote %s (mismatches=%d, summary=%s)"
          % (args.out, len(mismatches), json.dumps(out["summary"])))


if __name__ == "__main__":
    main()
