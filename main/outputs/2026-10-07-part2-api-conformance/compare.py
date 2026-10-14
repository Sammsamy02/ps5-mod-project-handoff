#!/usr/bin/env python3
"""Local, fail-closed comparison of future controlled UE4SS PC/PS5 records.

No network or engine access. 0=complete matching evidence, 1=substantive
difference/failure, 2=invalid/incomplete/reference pending. A match covers this
case suite only and is never a claim that the unmodified mod worked end to end.
"""
import argparse
import csv
import json
from pathlib import Path
import re
import sys

SCHEMA = "modbridge.ue4ss-conformance/1"
PLAN_SCHEMA = "modbridge.ue4ss-conformance-plan/1"
STATUSES = {"unavailable", "skipped", "failed", "observed", "proved", "fixture-pass"}
VOLATILE = {"identity", "object_address", "thread_id", "known_game_thread_id",
            "proof_id", "duration_ms", "timestamp_utc"}
COMMON_METADATA = ("suite_id", "mod_release_sha256", "helper_sha256", "script_sha256",
                   "game_id", "content_build", "scenario_id", "context_id", "selected_case_ids")
PLATFORM_METADATA = {"run_id", "side", "runtime_build", "runtime_sha256", "game_binary_sha256"}
ATTESTATIONS = ("real_engine", "release_immutable", "independent_thread_witness", "controlled_object_context")


class InvalidRecords(ValueError):
    pass


def object_without_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise InvalidRecords("duplicate JSON key: " + key)
        result[key] = value
    return result


def decode(text):
    def nonfinite(value):
        raise InvalidRecords("nonfinite JSON number: " + value)
    return json.loads(text, object_pairs_hook=object_without_duplicates, parse_constant=nonfinite)


def same_json(a, b):
    # Preserve JSON boolean/number/string types; Python's True == 1 is too loose.
    return json.dumps(a, sort_keys=True, ensure_ascii=False, allow_nan=False) == json.dumps(
        b, sort_keys=True, ensure_ascii=False, allow_nan=False)


def load_plan(path):
    plan = decode(path.read_text(encoding="utf-8"))
    if not isinstance(plan, dict) or plan.get("schema") != PLAN_SCHEMA or not isinstance(plan.get("cases"), list) or not plan["cases"]:
        raise InvalidRecords("invalid case manifest")
    if any(not isinstance(case, dict) for case in plan["cases"]):
        raise InvalidRecords("manifest cases must be objects")
    ids = [case.get("id") for case in plan["cases"]]
    orders = [case.get("order") for case in plan["cases"]]
    if any(not isinstance(x, str) or not x for x in ids) or len(set(ids)) != len(ids):
        raise InvalidRecords("manifest case IDs must be unique strings")
    if any(type(order) is not int for order in orders) or orders != list(range(1, len(ids) + 1)):
        raise InvalidRecords("manifest case order must be contiguous and explicit")
    if any(not isinstance(case.get("api"), str) for case in plan["cases"]):
        raise InvalidRecords("manifest API missing")
    return plan


def read_records(path):
    if path.suffix.lower() == ".tsv":
        with path.open(encoding="utf-8", newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            if reader.fieldnames != ["record_json"]:
                raise InvalidRecords("TSV input must have one lossless record_json column")
            records = []
            for row in reader:
                if None in row or not row.get("record_json"):
                    raise InvalidRecords("malformed TSV record")
                records.append(decode(row["record_json"]))
    else:
        records = [decode(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not records:
        raise InvalidRecords("empty record stream")
    return records


def normalized(record, removed):
    """Do not regex-rewrite addresses, IDs, text, numbers, or errors in a payload."""
    result = dict(record)
    if "volatile" in result:
        volatile = result["volatile"]
        if not isinstance(volatile, dict) or set(volatile) - VOLATILE:
            raise InvalidRecords("unknown volatile field: " + repr(volatile))
        if any(isinstance(v, (dict, list)) for v in volatile.values()):
            raise InvalidRecords("volatile values must be scalar metadata")
        removed.update("volatile." + key for key in volatile)
        del result["volatile"]
    return result


def validate(records, plan, side, removed):
    for record in records:
        if not isinstance(record, dict) or record.get("schema") != SCHEMA:
            raise InvalidRecords(side + ": unknown record schema")
        if record.get("kind") not in {"run", "binding", "case", "end"}:
            raise InvalidRecords(side + ": unknown record kind")
        normalized(record, removed)
    if records[0]["kind"] != "run" or records[-1]["kind"] != "end":
        raise InvalidRecords(side + ": missing run header or final end record")
    if sum(r["kind"] == "run" for r in records) != 1 or sum(r["kind"] == "end" for r in records) != 1:
        raise InvalidRecords(side + ": duplicate run/end records")
    header = records[0]
    for key in COMMON_METADATA + tuple(PLATFORM_METADATA) + ("evidence", "attestations"):
        if key not in header or header[key] in (None, ""):
            raise InvalidRecords(side + ": required run metadata missing: " + key)
    for key in ("mod_release_sha256", "helper_sha256", "script_sha256", "runtime_sha256", "game_binary_sha256"):
        if not isinstance(header[key], str) or re.fullmatch(r"[0-9a-f]{64}", header[key]) is None:
            raise InvalidRecords(side + ": invalid SHA-256 metadata: " + key)
    if header["evidence"] not in {"engine", "host-fixture"}:
        raise InvalidRecords(side + ": unknown evidence basis")
    if header["side"] not in {"pc", "ps5", "host"}:
        raise InvalidRecords(side + ": unknown platform side")
    if not isinstance(header["attestations"], dict):
        raise InvalidRecords(side + ": attestations must be explicit")
    expected = [c["id"] for c in plan["cases"]]
    selected = header["selected_case_ids"]
    if not isinstance(selected, list) or any(not isinstance(x, str) or x not in expected for x in selected) or len(set(selected)) != len(selected):
        raise InvalidRecords(side + ": unknown or duplicate selected case")
    cases = [r for r in records if r["kind"] == "case"]
    if [r.get("id") for r in cases] != expected:
        raise InvalidRecords(side + ": missing/extra/duplicate/out-of-order cases")
    bindings = [r for r in records if r["kind"] == "binding"]
    if any(r["kind"] == "binding" for r in records[next((i for i, r in enumerate(records) if r["kind"] == "case"), len(records)):]):
        raise InvalidRecords(side + ": binding inventory must precede case records")
    if "bindings" in plan and [r.get("name") for r in bindings] != plan["bindings"]:
        raise InvalidRecords(side + ": missing/unknown binding inventory")
    for case, spec in zip(cases, plan["cases"]):
        if case.get("api") != spec["api"] or type(case.get("order")) is not int or case.get("order") != spec["order"]:
            raise InvalidRecords(side + ": case API/order differs from manifest: " + spec["id"])
        if case.get("status") not in STATUSES or case.get("evidence") not in {"engine", "host-fixture"}:
            raise InvalidRecords(side + ": unknown parity/evidence status: " + spec["id"])
        if "value" not in case or "error" not in case or "reason" not in case:
            raise InvalidRecords(side + ": case terminal fields missing: " + spec["id"])
        if case["status"] == "proved" and (case["error"] is not None or case["reason"] is not None):
            raise InvalidRecords(side + ": proved case carries an error/skip reason: " + spec["id"])
        if "guard" in case and not isinstance(case["guard"], dict):
            raise InvalidRecords(side + ": guard must be an object: " + spec["id"])
    if records[-1].get("complete") is not True or type(records[-1].get("case_count")) is not int or records[-1].get("case_count") != len(cases):
        raise InvalidRecords(side + ": incomplete or inconsistent end record")
    return header, bindings, cases


def metadata_view(header, removed):
    # Runtime build and platform executable hashes differ by platform; preserve
    # them in the report as provenance, not by deleting them from the raw input.
    return {k: v for k, v in normalized(header, removed).items() if k not in PLATFORM_METADATA}


def compare(plan, reference, candidate, removed):
    pc, pc_bindings, pc_cases = validate(reference, plan, "reference", removed)
    ps5, ps5_bindings, ps5_cases = validate(candidate, plan, "candidate", removed)
    issues, incomplete, different = [], [], []
    if pc["side"] != "pc" or ps5["side"] != "ps5":
        incomplete.append("reference must be PC and candidate must be PS5")
    for label, header in (("reference", pc), ("candidate", ps5)):
        if header["evidence"] != "engine":
            incomplete.append(label + " is fixture-only")
        for flag in ATTESTATIONS:
            if header["attestations"].get(flag) is not True:
                incomplete.append(label + " lacks attestation: " + flag)
        if header["selected_case_ids"] != [c["id"] for c in plan["cases"]]:
            incomplete.append(label + " did not select the full suite")
        for field, wanted in (("suite_id", plan["suite_id"]), ("mod_release_sha256", plan["release_sha256"]),
                              ("helper_sha256", plan["helper_sha256"]), ("script_sha256", plan["script_sha256"])):
            if header[field] != wanted:
                different.append(label + " manifest mismatch: " + field)
        pins = plan.get("platform_pins", {}).get(header["side"], {})
        for field, wanted in pins.items():
            if header.get(field) != wanted:
                different.append(label + " platform provenance mismatch: " + field)
    pc_meta, ps5_meta = metadata_view(pc, removed), metadata_view(ps5, removed)
    if not same_json(pc_meta, ps5_meta):
        fields = sorted(k for k in set(pc_meta) | set(ps5_meta) if not same_json(pc_meta.get(k), ps5_meta.get(k)))
        # Evidence/attestations are an incomplete-evidence decision, not an
        # invented behavioral mismatch between synthetic and engine data.
        behavioral_fields = [k for k in fields if k not in {"evidence", "attestations"}]
        if behavioral_fields:
            different.append("run metadata differs: " + ", ".join(behavioral_fields))
        issues.append({"run_metadata_fields": fields, "reference": pc_meta, "candidate": ps5_meta})
    if not same_json([normalized(r, removed) for r in pc_bindings], [normalized(r, removed) for r in ps5_bindings]):
        different.append("binding availability inventory differs (presence is not behavioral proof)")
        issues.append({"reference_bindings": pc_bindings, "candidate_bindings": ps5_bindings})
    rows = []
    for spec, a, b in zip(plan["cases"], pc_cases, ps5_cases):
        a_norm, b_norm = normalized(a, removed), normalized(b, removed)
        row = {"id": spec["id"], "api": spec["api"], "reference_status": a["status"],
               "candidate_status": b["status"], "verdict": "match", "details": []}
        for label, record in (("reference", a), ("candidate", b)):
            if record["status"] == "failed":
                row["verdict"] = "different"
                row["details"].append(label + " failed: " + str(record["error"]))
            elif record["status"] != "proved" or record["evidence"] != "engine":
                if row["verdict"] != "different":
                    row["verdict"] = "incomplete"
                row["details"].append(label + " lacks proved engine behavior")
            if spec.get("requires_thread", True) and record["status"] == "proved":
                guard = record.get("guard", {})
                if guard.get("verified") is not True or guard.get("in_game_thread") is not True:
                    if row["verdict"] != "different":
                        row["verdict"] = "incomplete"
                    row["details"].append(label + " lacks independent game-thread proof")
        # Status/evidence differences alone signal pending proof; compare all
        # remaining fields, including errors, reasons, arguments and readbacks.
        a_behavior = {k: v for k, v in a_norm.items() if k not in {"status", "evidence"}}
        b_behavior = {k: v for k, v in b_norm.items() if k not in {"status", "evidence"}}
        if not same_json(a_behavior, b_behavior):
            row["verdict"] = "different"
            row["details"].append({"reference": a_behavior, "candidate": b_behavior})
        rows.append(row)
        if row["verdict"] == "different":
            different.append(spec["id"])
        elif row["verdict"] == "incomplete":
            incomplete.append(spec["id"])
    verdict = "different" if different else "incomplete" if incomplete else "match"
    return {"verdict": verdict, "scope": "this case suite only; original mod/menu/spawn acceptance remains separate",
            "different": different, "incomplete": incomplete, "issues": issues, "cases": rows,
            "reference_provenance": pc, "candidate_provenance": ps5}


def write_reports(report, json_path, tsv_path):
    body = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if json_path:
        json_path.write_text(body, encoding="utf-8")
    else:
        print(body, end="")
    if tsv_path:
        with tsv_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream, delimiter="\t")
            writer.writerow(["id", "api", "verdict", "reference_status", "candidate_status", "details_json"])
            for case in report.get("cases", []):
                writer.writerow([case.get(k, "") for k in ("id", "api", "verdict", "reference_status", "candidate_status")]
                                + [json.dumps(case.get("details", []), ensure_ascii=False, allow_nan=False)])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("manifest.json"))
    parser.add_argument("--reference", type=Path, help="future same-game controlled PC JSONL or TSV capture")
    parser.add_argument("--candidate", type=Path, required=True, help="PS5 JSONL or TSV capture")
    parser.add_argument("--json", type=Path, help="verdict JSON output")
    parser.add_argument("--tsv", type=Path, help="per-case TSV output")
    args = parser.parse_args(argv)
    removed = set()
    try:
        plan = load_plan(args.manifest)
        candidate = read_records(args.candidate)
        validate(candidate, plan, "candidate", removed)
        if args.reference is None:
            report = {"verdict": "reference_pending", "reason": "same-game PC reference unavailable; no parity decision",
                      "cases": [], "candidate_provenance": candidate[0]}
        else:
            report = compare(plan, read_records(args.reference), candidate, removed)
    except (OSError, ValueError, TypeError, KeyError) as error:
        report = {"verdict": "invalid_records", "error": str(error), "cases": []}
    report["schema"] = "modbridge.ue4ss-conformance-comparison/1"
    report["authenticity"] = "record labels are declarations, not authentication; independent harvested receipts are required"
    report["normalized_fields"] = sorted(removed)
    report["inputs"] = {"manifest": str(args.manifest), "reference": str(args.reference) if args.reference else None,
                        "candidate": str(args.candidate)}
    write_reports(report, args.json, args.tsv)
    return 0 if report["verdict"] == "match" else 1 if report["verdict"] == "different" else 2


if __name__ == "__main__":
    sys.exit(main())
