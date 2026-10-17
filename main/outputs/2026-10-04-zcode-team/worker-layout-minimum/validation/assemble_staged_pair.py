#!/usr/bin/env python3
"""assemble_staged_pair.py — staged-layout pair assembler with a hard validation gate.

worker-layout-minimum / team 2026-10-04-zcode-team.

Emits MemberVariableLayout.ini + VTableLayout.ini in the EXACT grammar accepted
by RC::Platform::NativeLayouts::ParsePair (ue4ss-native-layout-parser-1) and
the binding invariants (ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp):

  [NativeLayout]
  Format=1 / Target=prospero-x86_64-sysv / PointerSize=8 /
  EngineMajor=5 / EngineMinor=6 / NonShipping=<0|1> / Stats=<0|1> /
  EngineRevision=<opaque printable nonempty> / BuildId=<opaque printable nonempty>

  [<Class>]           (sections accept C++ nested names; here plain identifiers)
  Member=0xHEX        (UEP_TotalSize must be positive; members must fit total)

GATE — an entry is emitted ONLY if:
  1. its class chain has a PASSED in-process predicate verdict (Verdict records
     in the exact worker-5 vocabulary), and the winning GUObjectArray
     hypothesis matches the requested scope;
  2. the member has a compiled accessor width (null-width members are refused
     for staging and recorded as skipped, mirroring the binding's
     "Native accessor width unavailable");
  3. identity is COMPLETE — EngineRevision/BuildId/axes must be supplied as
     real in-process scan facts by the controller, never defaults;
  4. offset+width fit inside UEP_TotalSize, UEP_TotalSize positive.

Anything else: REFUSAL (nonzero exit, no output files, reasons printed).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys

REFUSAL_EXIT = 2

# Which candidate classes are emittable after which predicates pass.
# Extending this table with new predicate names is how rung-3/5 classes become
# emittable later; until then they are REFUSED (no predicate exists anywhere).
PREDICATE_REQUIREMENTS = {
    # hypothesis-tagged classes (rung 2)
    "FUObjectArray": {"probes": ["guobject_array"], "hypothesis": "A"},
    "TUObjectArray": {"probes": ["guobject_array"], "hypothesis": None},  # either
    # object-chain-gated classes (rung 2 cross-check)
    "FUObjectItem": {"probes": ["guobject_array", "uobject_item_chain"]},
    "UObjectBase": {"probes": ["guobject_array", "uobject_item_chain"]},
    # rung 3/5 classes: NO PREDICATE EXISTS YET anywhere in the retained work
    # (worker-5 built the GUObjectArray chain only) — refused until commissioned.
    "FField": None,
    "FProperty": None,
    "UStruct": None,
    "UClass": None,
    "UFunction": None,
}


class Refusal(Exception):
    pass


def _sha(obj) -> str:
    return hashlib.sha256(json.dumps(obj, sort_keys=True).encode()).hexdigest()


def load_json(path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def build_identity(candidates, identity_facts):
    missing = [k for k in ("engine_revision", "build_id", "non_shipping", "stats")
               if identity_facts.get(k) in (None, "")]
    if missing:
        raise Refusal(
            "identity incomplete: missing " + ", ".join(missing) +
            " — these must come from the live in-process scan (controller), never defaults")
    ident = candidates["identity"]
    non_shipping = int(bool(identity_facts["non_shipping"]))
    stats = int(bool(identity_facts["stats"]))
    lines = [
        "[NativeLayout]",
        "Format=1",
        "Target=%s" % ident["target"],
        "PointerSize=%d" % ident["pointer_size"],
        "EngineMajor=%d" % ident["engine_major"],
        "EngineMinor=%d" % ident["engine_minor"],
        "NonShipping=%d" % non_shipping,
        "Stats=%d" % stats,
        "EngineRevision=%s" % identity_facts["engine_revision"],
        "BuildId=%s" % identity_facts["build_id"],
    ]
    for key, value in (("EngineRevision", identity_facts["engine_revision"]),
                       ("BuildId", identity_facts["build_id"])):
        if not value or any((not (0x20 <= ord(c) <= 0x7E)) for c in value):
            raise Refusal("identity %s must be nonempty printable ASCII" % key)
    return "\n".join(lines) + "\n"


def gate_verdicts(verdict_records, hypothesis):
    """Index verdict records by probe name; enforce the predicate gate."""
    records = {}
    for rec in verdict_records:
        probe = rec.get("probe")
        if probe in records:
            raise Refusal("duplicate verdict record for probe %r" % probe)
        records[probe] = rec
    gua = records.get("guobject_array")
    if gua is None:
        raise Refusal("no guobject_array verdict record: the GUObjectArray chain "
                      "predicate has not run — nothing may be emitted")
    if gua.get("verdict") != "Validated":
        raise Refusal("guobject_array verdict is %r (codes: %s): candidates stay "
                      "candidates" % (gua.get("verdict"), gua.get("codes")))
    matched = gua.get("layout_matched")
    if hypothesis is None:
        hypothesis = matched
    if hypothesis != matched:
        raise Refusal("scope hypothesis %r disagrees with validated layout %r"
                      % (hypothesis, matched))
    return records, hypothesis


def assemble(candidates, verdict_records, identity_facts, scope):
    identity_text = build_identity(candidates, identity_facts)
    records, hypothesis = gate_verdicts(verdict_records, scope.get("hypothesis"))

    requested = scope.get("classes")
    if not requested:
        requested = ["TUObjectArray", "FUObjectItem", "UObjectBase"]
        if hypothesis == "A":
            requested = ["FUObjectArray"] + requested

    member_sources = candidates["candidate_member_offsets"]
    sections = []
    manifest = {
        "identity": {"engine_major": candidates["identity"]["engine_major"],
                     "engine_minor": candidates["identity"]["engine_minor"],
                     "target": candidates["identity"]["target"],
                     "engine_revision_source": "controller-supplied in-process scan fact",
                     "build_id_source": "controller-supplied in-process scan fact"},
        "hypothesis": hypothesis,
        "emitted": [],
        "skipped_no_width": [],
        "refused": [],
        "verdict_digests": {name: _sha(rec) for name, rec in records.items()},
        "origin_note": "every emitted offset originates from candidates/"
                       "layout-candidates.profile.json (C-set-506 port template, "
                       "Windows-derived) and passed the in-process predicate gate",
    }

    for cls in requested:
        if cls not in member_sources:
            raise Refusal("unknown candidate class %r" % cls)
        req = PREDICATE_REQUIREMENTS.get(cls, False)
        if req is None:
            manifest["refused"].append({"class": cls,
                                        "reason": "no predicate exists for this class "
                                                  "chain yet; refusing (rung 3/5 "
                                                  "predicates not commissioned)"})
            continue
        if req is False:
            raise Refusal("class %r is not in the emittable requirements table" % cls)
        for probe in req["probes"]:
            rec = records.get(probe)
            if rec is None or rec.get("verdict") != "Validated":
                manifest["refused"].append(
                    {"class": cls, "reason": "predicate %r not Validated (%r)"
                     % (probe, None if rec is None else rec.get("verdict"))})
                break
        else:
            if req.get("hypothesis") and hypothesis != req["hypothesis"]:
                manifest["refused"].append(
                    {"class": cls, "reason": "requires hypothesis %s, validated %s"
                     % (req["hypothesis"], hypothesis)})
                continue
            total = member_sources[cls]["entries"].get("UEP_TotalSize", {}).get("offset")
            if not total or total <= 0:
                raise Refusal("%s lacks positive UEP_TotalSize" % cls)
            body = ["[%s]" % cls]
            emitted_keys = []
            for member, spec in member_sources[cls]["entries"].items():
                offset = spec["offset"]
                width = spec.get("width_bytes")
                if member != "UEP_TotalSize":
                    if width is None:
                        manifest["skipped_no_width"].append(
                            {"class": cls, "member": member,
                             "reason": spec.get("reason", "no compiled accessor width")})
                        continue
                    if offset < 0 or offset >= total or width > total - offset:
                        raise Refusal("%s::%s (offset 0x%X width %d) outside declared "
                                      "class size 0x%X" % (cls, member, offset, width, total))
                body.append("%s=0x%X" % (member, offset))
                emitted_keys.append(member)
            sections.append("\n".join(body))
            manifest["emitted"].append({"class": cls, "members": emitted_keys,
                                        "uep_total_size": "0x%X" % total})

    if not manifest["emitted"]:
        raise Refusal("no class passed the gate: " +
                      json.dumps(manifest["refused"]))
    members_text = identity_text + "\n" + "\n\n".join(sections) + "\n"
    vtables_text = identity_text  # identity-only by design (relocation-based hooks)
    return members_text, vtables_text, manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--verdicts", required=True,
                        help="JSON: list of probe verdict records (worker-5 vocabulary)")
    parser.add_argument("--identity", required=True,
                        help="JSON: engine_revision/build_id/non_shipping/stats "
                             "(in-process scan facts)")
    parser.add_argument("--scope", default=None,
                        help="JSON: {hypothesis: A|B, classes: [...]}")
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args(argv)

    candidates = load_json(args.candidates)
    verdicts = load_json(args.verdicts)
    identity_facts = load_json(args.identity)
    scope = load_json(args.scope) if args.scope else {}

    try:
        members_text, vtables_text, manifest = assemble(
            candidates, verdicts, identity_facts, scope)
    except Refusal as exc:
        print("REFUSED: %s" % exc, file=sys.stderr)
        return REFUSAL_EXIT

    os.makedirs(args.out_dir, exist_ok=True)
    for name, text in (("MemberVariableLayout.ini", members_text),
                       ("VTableLayout.ini", vtables_text)):
        with open(os.path.join(args.out_dir, name), "w", encoding="utf-8") as fh:
            fh.write(text)
    with open(os.path.join(args.out_dir, "assembly-manifest.json"), "w",
              encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2)
    print("emitted %d class section(s) to %s (hypothesis %s)"
          % (len(manifest["emitted"]), args.out_dir, manifest["hypothesis"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
