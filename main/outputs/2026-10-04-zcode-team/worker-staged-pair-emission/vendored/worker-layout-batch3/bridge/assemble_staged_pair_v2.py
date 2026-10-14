#!/usr/bin/env python3
"""assemble_staged_pair_v2.py — batch-3 bridge over the layout-minimum
assembler.

worker-layout-batch3 / team 2026-10-04-zcode-team.

This module IMPORTS worker-layout-minimum's assemble_staged_pair.py read-only
and extends its hard gate with the batch2/batch3 predicate inventory and the
compiled width contract:

  NEW GATE A (predicate inventory): rung-3 classes become emittable exactly
  when their retained Validated-class predicates pass — ustruct_chain (batch2)
  gates UObjectBase/UStruct/UClass; ffield_variant (batch2) gates FField/
  FProperty AND must settle variant=UE5_FField (the candidate offsets are the
  SET-5_06 table; a validated UE4_UProperty variant REFUSES this table).
  UFunction stays REFUSED: only an Inconclusive-classifier exists (batch2
  discipline), so no Validated verdict can ever name it.

  NEW GATE B (width authority): staged members require a width in the
  compiled contract (widths-1 coverage.json + widths-2 delta), NOT the
  candidates profile transcription. Transcription disagreements are recorded
  in the manifest under width_contract_mismatches; SuperStruct — transcribed
  as width 8 in the candidates profile but null in the contract — is the
  critical case: v2 refuses to emit it.

Everything else (identity completeness, hypothesis matching, duplicate
verdict refusal, offset/width fit inside UEP_TotalSize, refusal-without-
output) is inherited verbatim from the original assembler.

Batch3 chain-walk verdicts (children_walk / childproperties_walk /
uclass_links) are recorded as strengthening evidence; they never widen the
gate and never manufacture widths.
"""
from __future__ import annotations

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "worker-layout-minimum", "validation"))

import assemble_staged_pair as base  # noqa: E402  (read-only import)
from width_contract import (  # noqa: E402
    PREDICATE_REQUIREMENTS, STRENGTHENING_PROBES, load_contract)

REFUSAL_EXIT = base.REFUSAL_EXIT
Refusal = base.Refusal
load_json = base.load_json


def gate_verdicts_v2(verdict_records, hypothesis):
    """Extend the base gate with per-probe validation and the variant rule."""
    records, hypothesis = base.gate_verdicts(verdict_records, hypothesis)

    # Defensive completeness: every probe named by a requirement we will use
    # must carry a worker-5 vocabulary verdict.
    for name, rec in records.items():
        if rec.get("verdict") not in ("Validated", "Inconclusive", "Refuted"):
            raise Refusal("verdict record %r has non-vocabulary verdict %r"
                          % (name, rec.get("verdict")))

    variant = records.get("ffield_variant")
    if variant is not None and variant.get("verdict") == "Validated":
        if variant.get("variant") not in ("UE5_FField", "UE4_UProperty"):
            raise Refusal("ffield_variant Validated without a named variant")
    return records, hypothesis


def _class_gate_ok(req, records, hypothesis):
    """Return (ok, reason). Mirrors the base assembler's per-class logic and
    adds the variant gate."""
    for probe in req["probes"]:
        rec = records.get(probe)
        if rec is None or rec.get("verdict") != "Validated":
            return False, ("predicate %r not Validated (%s)"
                           % (probe, "absent" if rec is None
                              else rec.get("verdict")))
    if req.get("hypothesis") and hypothesis != req["hypothesis"]:
        return False, ("requires hypothesis %s, validated %s"
                       % (req["hypothesis"], hypothesis))
    if req.get("variant"):
        rec = records.get("ffield_variant")
        if rec is None or rec.get("verdict") != "Validated":
            return False, ("variant discriminator not settled (%s)"
                           % ("absent" if rec is None
                              else rec.get("verdict")))
        if rec.get("variant") != req["variant"]:
            return False, ("settled variant %s refutes the SET-5_06 candidate "
                           "table (requires %s)"
                           % (rec.get("variant"), req["variant"]))
    return True, None


def assemble_v2(candidates, verdict_records, identity_facts, scope,
                contract, default_full_scope=True):
    identity_text = base.build_identity(candidates, identity_facts)
    records, hypothesis = gate_verdicts_v2(verdict_records,
                                           scope.get("hypothesis"))

    requested = scope.get("classes")
    if not requested:
        if default_full_scope:
            # Full promotable set in dependency order (hypothesis-filtered).
            requested = ["FUObjectArray", "TUObjectArray", "FUObjectItem",
                         "UObjectBase", "UStruct", "UClass", "FField",
                         "FProperty", "UFunction"]
            if hypothesis != "A":
                requested.remove("FUObjectArray")
        else:
            requested = ["TUObjectArray", "FUObjectItem", "UObjectBase"]
            if hypothesis == "A":
                requested = ["FUObjectArray"] + requested

    member_sources = candidates["candidate_member_offsets"]
    sections = []
    manifest = {
        "identity": {"engine_major": candidates["identity"]["engine_major"],
                     "engine_minor": candidates["identity"]["engine_minor"],
                     "target": candidates["identity"]["target"],
                     "engine_revision_source":
                         "controller-supplied in-process scan fact",
                     "build_id_source":
                         "controller-supplied in-process scan fact"},
        "assembler": "assemble_staged_pair_v2 (batch3 bridge)",
        "width_authority": "compiled contract: widths-1 coverage.json + "
                           "widths-2 coverage-delta.json",
        "hypothesis": hypothesis,
        "emitted": [],
        "skipped_no_width": [],
        "refused": [],
        "strengthening_verdicts": {},
        "width_contract_mismatches": [],
        "verdict_digests": {name: base._sha(rec)
                            for name, rec in records.items()},
        "origin_note": base.__doc__.splitlines()[0] if base.__doc__ else "",
    }

    # Record strengthening probes verbatim (never gates).
    for probe in ("children_walk", "childproperties_walk", "uclass_links"):
        rec = records.get(probe)
        if rec is not None:
            manifest["strengthening_verdicts"][probe] = {
                "verdict": rec.get("verdict"),
                "codes": rec.get("codes"),
            }

    # Contract-vs-transcription mismatches for transparency.
    for cls in requested:
        spec = member_sources.get(cls)
        if not spec:
            continue
        c_contract = contract.get(cls, {})
        for member, entry in spec["entries"].items():
            if member == "UEP_TotalSize":
                continue
            cw = entry.get("width_bytes")
            kw = c_contract.get(member)
            if (cw is not None and kw is None) or \
               (cw is not None and kw is not None and cw != kw) or \
               (cw is None and kw is not None):
                manifest["width_contract_mismatches"].append(
                    {"class": cls, "member": member,
                     "candidates_width": cw, "contract_width": kw})

    for cls in requested:
        if cls not in member_sources:
            raise Refusal("unknown candidate class %r" % cls)
        req = PREDICATE_REQUIREMENTS.get(cls, False)
        if req is None:
            manifest["refused"].append(
                {"class": cls,
                 "reason": "no Validated-class predicate exists (UFunction "
                           "has an Inconclusive-classifier only, batch2 "
                           "discipline); refusing"})
            continue
        if req is False:
            raise Refusal("class %r is not in the emittable requirements "
                          "table" % cls)
        ok, reason = _class_gate_ok(req, records, hypothesis)
        if not ok:
            manifest["refused"].append({"class": cls, "reason": reason})
            continue
        total = member_sources[cls]["entries"].get("UEP_TotalSize",
                                                   {}).get("offset")
        if not total or total <= 0:
            raise Refusal("%s lacks positive UEP_TotalSize" % cls)
        body = ["[%s]" % cls]
        emitted_keys = []
        c_contract = contract.get(cls, {})
        for member, spec in member_sources[cls]["entries"].items():
            offset = spec["offset"]
            width = c_contract.get(member)  # CONTRACT width, not transcription
            if member != "UEP_TotalSize":
                if width is None:
                    manifest["skipped_no_width"].append(
                        {"class": cls, "member": member,
                         "reason": "no compiled accessor width in the "
                                   "contract (widths-1 + widths-2)"})
                    continue
                if offset < 0 or offset >= total or width > total - offset:
                    raise Refusal("%s::%s (offset 0x%X width %d) outside "
                                  "declared class size 0x%X"
                                  % (cls, member, offset, width, total))
            body.append("%s=0x%X" % (member, offset))
            emitted_keys.append(member)
        sections.append("\n".join(body))
        manifest["emitted"].append({"class": cls, "members": emitted_keys,
                                    "uep_total_size": "0x%X" % total})

    if not manifest["emitted"]:
        raise Refusal("no class passed the gate: " +
                      json.dumps(manifest["refused"]))
    members_text = identity_text + "\n" + "\n\n".join(sections) + "\n"
    vtables_text = identity_text  # identity-only by design (relocation hooks)
    return members_text, vtables_text, manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--widths1", required=True)
    parser.add_argument("--delta", default=None)
    parser.add_argument("--verdicts", required=True)
    parser.add_argument("--identity", required=True,
                        help="JSON: engine_revision/build_id/non_shipping/"
                             "stats (in-process scan facts)")
    parser.add_argument("--scope", default=None)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args(argv)

    candidates = load_json(args.candidates)
    verdicts = load_json(args.verdicts)
    identity_facts = load_json(args.identity)
    scope = load_json(args.scope) if args.scope else {}

    import contextlib
    with open(args.widths1, "r", encoding="utf-8") as fh:
        widths1 = json.load(fh)
    delta = None
    if args.delta:
        with open(args.delta, "r", encoding="utf-8") as fh:
            delta = json.load(fh)
    contract = load_contract(widths1, delta)

    try:
        members_text, vtables_text, manifest = assemble_v2(
            candidates, verdicts, identity_facts, scope, contract)
    except Refusal as exc:
        print("REFUSED: %s" % exc, file=sys.stderr)
        return REFUSAL_EXIT

    os.makedirs(args.out_dir, exist_ok=True)
    for name, text in (("MemberVariableLayout.ini", members_text),
                       ("VTableLayout.ini", vtables_text)):
        with open(os.path.join(args.out_dir, name), "w",
                  encoding="utf-8") as fh:
            fh.write(text)
    with open(os.path.join(args.out_dir, "assembly-manifest.json"), "w",
              encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2)
    print("emitted %d class section(s) to %s (hypothesis %s)"
          % (len(manifest["emitted"]), args.out_dir, manifest["hypothesis"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
