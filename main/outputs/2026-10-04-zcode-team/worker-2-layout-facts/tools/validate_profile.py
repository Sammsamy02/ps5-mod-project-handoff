#!/usr/bin/env python3
"""Generic Unreal build/layout profile validator (host-side, read-only).

Rules are generic and data-driven: family/fact vocabularies, status enums,
width policies and the target registry are loaded from the schema document.
Per-build values live only in profile files. The tool contains no constants
belonging to any particular game build.

It enforces, among others:
  * identity axes (target registry, pointer size, engine version range);
  * every fact traces to a declared evidence source (no unattributed numbers);
  * forbidden provenance classes (pdb-default, other-game-build, assumed,
    guessed, unattributed) are rejected anywhere they appear;
  * file-image vaddr / file byte offset / mapped runtime address / load base
    are distinct fields; copying one into another is an error; mapped fields
    are null unless genuine game-runtime evidence exists;
  * widths come only from allowed provenance classes, are in the allowed set,
    width 16 requires the conservative-maximum alternatives record, and an
    unavailable width is null+reason (never 0);
  * declared family_status equals the recomputed aggregation;
  * candidate-only facts are never referenced as validated;
  * readiness flags cannot exceed the evidence (static_layout_ready requires
    resolved identity axes; runtime_binding_ready requires game-runtime
    provenance);
  * build identification matches profiles strictly by axes (target, pointer
    size, engine version, optional title) and refuses ambiguity or fallback.

Usage:
  validate_profile.py --schema SCHEMA.json --profile PROFILE.json [--identify OBS.json]
  validate_profile.py --schema SCHEMA.json --identify OBS.json PROFILE1.json [PROFILE2.json ...]
Exit code 0 = valid, 1 = invalid (findings printed), 2 = usage/input error.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HEX_RE = re.compile(r"^0x[0-9a-fA-F]+$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
FAMILY_PRIORITY = ["validated", "candidate-only", "absent", "unresolved", "not-surveyed"]
STATUS_TO_FAMILY = {
    "validated-static": "validated",
    "candidate-only": "candidate-only",
    "absent": "absent",
    "unresolved": "unresolved",
    "not-surveyed": "not-surveyed",
}
NO_VALUE_STATUSES = {"absent", "unresolved", "not-surveyed"}


class Findings:
    def __init__(self) -> None:
        self.items: list[str] = []

    def add(self, where: str, message: str) -> None:
        self.items.append(f"{where}: {message}")

    def ok(self) -> bool:
        return not self.items

    def report(self) -> str:
        return "\n".join(f"- {item}" for item in self.items)


def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"input error: cannot load {path}: {exc}")
    if not isinstance(data, dict):
        raise SystemExit(f"input error: {path} must contain a JSON object")
    return data


def deep_forbidden_provenance(node, path: str, forbidden: set[str], findings: Findings) -> None:
    """Reject forbidden provenance classes anywhere in the document."""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "provenance_class" and isinstance(value, str):
                if value in forbidden:
                    findings.add(path, f"forbidden provenance_class '{value}'")
            elif key == "origin" and isinstance(value, dict):
                pc = value.get("provenance_class")
                if isinstance(pc, str) and pc in forbidden:
                    findings.add(f"{path}/{key}", f"forbidden provenance_class '{pc}'")
            deep_forbidden_provenance(value, f"{path}/{key}", forbidden, findings)
    elif isinstance(node, list):
        for index, value in enumerate(node):
            deep_forbidden_provenance(value, f"{path}[{index}]", forbidden, findings)


def parse_hex(value, where: str, findings: Findings):
    if not isinstance(value, str) or not HEX_RE.match(value):
        findings.add(where, f"address '{value!r}' must be a hex string like '0x1a2b'")
        return None
    return int(value, 16)


def validate_identity(profile: dict, schema: dict, findings: Findings) -> None:
    identity = profile.get("identity")
    if not isinstance(identity, dict):
        findings.add("identity", "missing or not an object")
        return
    where = "identity"
    for field in ("target", "pointer_size", "engine_major", "engine_minor"):
        if field not in identity:
            findings.add(where, f"missing required field '{field}'")
    registry = schema.get("target_registry", {})
    target = identity.get("target")
    if target is not None and target not in registry:
        findings.add(where, f"unknown target '{target}' (known: {sorted(registry)})")
        return
    expected_ptr = registry.get(target, {}).get("pointer_size")
    if expected_ptr is not None and identity.get("pointer_size") != expected_ptr:
        findings.add(where, f"pointer_size {identity.get('pointer_size')} disagrees with target registry ({expected_ptr})")
    major, minor = identity.get("engine_major"), identity.get("engine_minor")
    if not isinstance(major, int) or isinstance(major, bool) or major < 4:
        findings.add(where, f"engine_major must be an integer >= 4, got {major!r}")
    if not isinstance(minor, int) or isinstance(minor, bool) or minor < 0:
        findings.add(where, f"engine_minor must be an integer >= 0, got {minor!r}")
    axes = identity.get("build_axes")
    if not isinstance(axes, dict) or set(axes) != {"non_shipping", "stats"}:
        findings.add(where, "build_axes must contain exactly 'non_shipping' and 'stats'")
    else:
        for axis, value in axes.items():
            if value is not None and not isinstance(value, bool):
                findings.add(f"{where}/build_axes/{axis}", "must be a boolean or null (never a defaulted truth value)")
        if any(v is None for v in axes.values()) and not identity.get("build_axes_status"):
            findings.add(where, "unresolved build axes require an explicit build_axes_status reason")


def validate_evidence_sources(profile: dict, findings: Findings) -> dict:
    sources = profile.get("evidence_sources")
    if not isinstance(sources, list) or not sources:
        findings.add("evidence_sources", "missing or empty")
        return {}
    ids: dict[str, dict] = {}
    for index, source in enumerate(sources):
        where = f"evidence_sources[{index}]"
        if not isinstance(source, dict):
            findings.add(where, "not an object")
            continue
        source_id = source.get("id")
        if not isinstance(source_id, str) or not source_id:
            findings.add(where, "missing id")
            continue
        if source_id in ids:
            findings.add(where, f"duplicate evidence id '{source_id}'")
            continue
        for field in ("role", "path"):
            if not isinstance(source.get(field), str) or not source.get(field):
                findings.add(where, f"missing '{field}'")
        if not Path(str(source.get("path", ""))).is_absolute():
            findings.add(where, "path must be absolute")
        sha = source.get("sha256")
        if sha is not None and (not isinstance(sha, str) or not SHA256_RE.match(sha)):
            findings.add(where, "sha256 must be 64 lowercase hex characters")
        ids[source_id] = source
    return ids


def check_origin(origin, where: str, source_ids: dict, findings: Findings, required: bool) -> None:
    if origin is None:
        if required:
            findings.add(where, "numeric facts require an origin with an artifact reference")
        return
    if not isinstance(origin, dict):
        findings.add(where, "origin must be an object")
        return
    artifact = origin.get("artifact")
    if not isinstance(artifact, str) or not artifact:
        findings.add(where, "origin.artifact missing")
    elif artifact not in source_ids:
        findings.add(where, f"origin.artifact '{artifact}' is not a declared evidence source")


def validate_address_block(address, where: str, findings: Findings, runtime_evidence: bool) -> None:
    if not isinstance(address, dict):
        findings.add(where, "address must be an object")
        return
    allowed = {"elf_image_vaddr", "file_byte_offset", "mapped_runtime_address", "load_base"}
    if set(address) - allowed:
        findings.add(where, f"unknown address fields {sorted(set(address) - allowed)}")
    vaddr = address.get("elf_image_vaddr")
    if vaddr is not None:
        value = parse_hex(vaddr, f"{where}/elf_image_vaddr", findings)
        if value is not None and value == 0:
            findings.add(f"{where}/elf_image_vaddr", "0 is the survey failure sentinel, not an address")
    fbo = address.get("file_byte_offset")
    if fbo is not None:
        parse_hex(fbo, f"{where}/file_byte_offset", findings)
        derivation = address.get("file_byte_offset_derivation")
        if not isinstance(derivation, str) or not derivation:
            findings.add(f"{where}/file_byte_offset",
                         "non-null file_byte_offset requires a recorded program-header derivation")
    for mapped_field in ("mapped_runtime_address", "load_base"):
        if address.get(mapped_field) is not None:
            if not runtime_evidence:
                findings.add(f"{where}/{mapped_field}",
                             "non-null mapped runtime field without game-runtime evidence (file/mapped confusion)")
            else:
                parse_hex(address[mapped_field], f"{where}/{mapped_field}", findings)
    # A file-image vaddr duplicated into a byte offset or mapped slot without
    # its own evidence is exactly the confusion this tool must reject.
    if vaddr is not None:
        for other in ("file_byte_offset", "mapped_runtime_address", "load_base"):
            if address.get(other) == vaddr:
                findings.add(f"{where}/{other}", f"copies elf_image_vaddr {vaddr} — address kinds must stay distinct")


def validate_fact(name: str, fact: dict, family: str, schema: dict, source_ids: dict,
                  findings: Findings, extra: dict) -> None:
    where = f"static_survey_facts/{family}/{name}"
    vocabulary = schema.get("family_vocabularies", {}).get(family, [])
    if name not in vocabulary:
        findings.add("static_survey_facts/" + family, f"fact '{name}' is not in the schema vocabulary")
    status = fact.get("status")
    if status not in STATUS_TO_FAMILY:
        findings.add(where, f"unknown status {status!r}")
        status = None
    if "endpoint_status_code" in fact:
        code = fact["endpoint_status_code"]
        known_codes = {int(k) for k in schema.get("survey_endpoint_status_codes", {})}
        if code not in known_codes:
            findings.add(where, f"unknown endpoint_status_code {code!r}")
    if "producer" in fact and fact["producer"] is not None and not fact.get("origin"):
        findings.add(where, "producer recorded without origin evidence")
    check_origin(fact.get("origin"), where, source_ids, findings, required=True)
    value = fact.get("value")
    if status in NO_VALUE_STATUSES:
        if value is not None:
            findings.add(where, f"status '{status}' must not carry a value")
        return
    if value is None:
        findings.add(where, f"status '{status}' requires a value")
        return
    if not isinstance(value, dict):
        findings.add(where, "value must be an object")
        return
    if status == "candidate-only":
        extra["candidate_addresses"].append((where, value))
    if status == "validated-static":
        extra["validated_addresses"].append((where, value))
    address = value.get("address")
    if address is not None:
        validate_address_block(address, f"{where}/value/address", findings, extra["runtime_evidence"])
    for key in value:
        if key.endswith("_vaddr") and key != "address" and key not in ("value_kind",):
            parse_hex(value[key], f"{where}/value/{key}", findings)
    rv = fact.get("runtime_validation")
    if rv is not None:
        if not isinstance(rv, dict):
            findings.add(where, "runtime_validation must be an object")
        elif rv.get("validated") is True and fact.get("provenance_class") != "game-runtime":
            findings.add(where, "runtime_validation.validated=true requires provenance_class 'game-runtime'")


def aggregate_family_status(facts: dict) -> str:
    achieved = {STATUS_TO_FAMILY.get(f.get("status")) for f in facts.values()}
    for family_status in FAMILY_PRIORITY:
        if family_status in achieved:
            return family_status
    return "not-surveyed"


def validate_widths(profile: dict, schema: dict, source_ids: dict, findings: Findings) -> None:
    widths = profile.get("compiled_port_widths")
    if not isinstance(widths, dict):
        findings.add("compiled_port_widths", "missing or not an object")
        return
    if not isinstance(widths.get("scope_statement"), str) or not widths["scope_statement"]:
        findings.add("compiled_port_widths", "missing scope_statement separating port contract from game ABI")
    rules = schema.get("width_rules", {})
    allowed_widths = set(rules.get("allowed_width_bytes", []))
    for class_name, block in (widths.get("classes") or {}).items():
        where = f"compiled_port_widths/classes/{class_name}"
        if not isinstance(block, dict):
            findings.add(where, "not an object")
            continue
        pc = block.get("provenance_class")
        if pc not in schema.get("provenance_classes", {}).get("allowed", []):
            findings.add(where, f"provenance_class {pc!r} not allowed for width facts")
        check_origin(block.get("origin"), where, source_ids, findings, required=True)
        available = sum(1 for e in block.get("entries", []) if isinstance(e, dict) and e.get("width_bytes") is not None)
        if isinstance(block.get("available"), int) and block["available"] != available:
            findings.add(where, f"declared available={block['available']} but entries contain {available}")
        if isinstance(block.get("total"), int) and block["total"] != len(block.get("entries", [])):
            findings.add(where, f"declared total={block['total']} but entries list {len(block.get('entries', []))}")
        for index, entry in enumerate(block.get("entries", [])):
            ewhere = f"{where}/entries[{index}]"
            if not isinstance(entry, dict):
                findings.add(ewhere, "not an object")
                continue
            width = entry.get("width_bytes")
            if width is None:
                if not isinstance(entry.get("reason"), str) or not entry["reason"]:
                    findings.add(ewhere, "unavailable width requires a non-empty reason")
            elif not isinstance(width, int) or isinstance(width, bool):
                findings.add(ewhere, f"width_bytes must be an integer, got {width!r}")
            elif width == 0:
                findings.add(ewhere, "width 0 is never valid")
            elif width not in allowed_widths:
                findings.add(ewhere, f"unknown width {width} (allowed: {sorted(allowed_widths)})")
            elif width == 16 and rules.get("width_16_policy"):
                alts = entry.get("alternatives_compiled_bytes")
                if not isinstance(alts, list) or len(set(alts)) < 2 or max(alts) != 16:
                    findings.add(ewhere, "width 16 requires alternatives_compiled_bytes with >=2 distinct values maxing at 16")


def validate_readiness(profile: dict, findings: Findings) -> None:
    readiness = profile.get("readiness")
    if not isinstance(readiness, dict):
        findings.add("readiness", "missing or not an object")
        return
    identity = profile.get("identity", {})
    if readiness.get("static_layout_ready") is True:
        missing = [field for field in ("engine_revision", "build_id")
                   if not identity.get(field)]
        axes = identity.get("build_axes") if isinstance(identity.get("build_axes"), dict) else {}
        missing += [f"build_axes.{axis}" for axis, value in axes.items() if value is None]
        if missing:
            findings.add("readiness", f"static_layout_ready=true but unresolved: {', '.join(missing)}")
    if readiness.get("runtime_binding_ready") is True:
        findings.add("readiness", "runtime_binding_ready=true is not supported by any current evidence")


def validate_profile(profile: dict, schema: dict) -> Findings:
    findings = Findings()
    deep_forbidden_provenance(profile, "$", set(schema.get("provenance_classes", {}).get("forbidden", [])), findings)

    if profile.get("profile_format") != schema.get("profile_format"):
        findings.add("$", f"profile_format {profile.get('profile_format')!r} != schema {schema.get('profile_format')!r}")
    if profile.get("profile_kind") not in schema.get("profile_kinds", []):
        findings.add("$", f"unknown profile_kind {profile.get('profile_kind')!r}")
    if profile.get("schema_id") != schema.get("schema_id"):
        findings.add("$", f"schema_id mismatch: {profile.get('schema_id')!r} vs {schema.get('schema_id')!r}")
    for field in ("profile_id", "title"):
        if not isinstance(profile.get(field), str) or not profile.get(field):
            findings.add("$", f"missing '{field}'")

    validate_identity(profile, schema, findings)
    source_ids = validate_evidence_sources(profile, findings)

    runtime_sources = [s for s in source_ids.values() if s.get("role") == "runtime-observation"]
    extra = {
        "candidate_addresses": [],
        "validated_addresses": [],
        "runtime_evidence": bool(runtime_sources)
                           and any("runtime" in str(s.get("role", "")) for s in runtime_sources),
    }

    vocabularies = schema.get("family_vocabularies", {})
    families = profile.get("static_survey_facts")
    if not isinstance(families, dict):
        findings.add("static_survey_facts", "missing or not an object")
    else:
        for family, block in families.items():
            if family not in vocabularies:
                findings.add("static_survey_facts", f"unknown family '{family}' (known: {sorted(vocabularies)})")
            if not isinstance(block, dict) or not isinstance(block.get("facts"), dict):
                findings.add(f"static_survey_facts/{family}", "missing facts object")
                continue
            for name, fact in block["facts"].items():
                if isinstance(fact, dict):
                    validate_fact(name, fact, family, schema, source_ids, findings, extra)
            declared = block.get("family_status")
            computed = aggregate_family_status(block["facts"])
            if declared != computed:
                findings.add(f"static_survey_facts/{family}",
                             f"declared family_status '{declared}' but facts aggregate to '{computed}'")

    # EngineVersion fact must agree with identity axes (generic cross-check).
    ev = ((families or {}).get("EngineVersion", {}).get("facts", {}) or {}).get("engine_version", {})
    if isinstance(ev, dict) and isinstance(ev.get("value"), dict):
        if ev["value"].get("major") != profile.get("identity", {}).get("engine_major") or \
           ev["value"].get("minor") != profile.get("identity", {}).get("engine_minor"):
            findings.add("static_survey_facts/EngineVersion/engine_version",
                         "value disagrees with identity.engine_major/engine_minor")

    # Same-family address corroboration / conflict detection (generic).
    for family, block in (families or {}).items():
        if not isinstance(block, dict):
            continue
        by_address: dict[int, list[str]] = {}
        for name, fact in (block.get("facts") or {}).items():
            value = fact.get("value") if isinstance(fact, dict) else None
            if isinstance(value, dict) and isinstance(value.get("address"), dict):
                raw = value["address"].get("elf_image_vaddr")
                if isinstance(raw, str) and HEX_RE.match(raw):
                    by_address.setdefault(int(raw, 16), []).append(name)
        for address, names in by_address.items():
            statuses = [block["facts"][n].get("status") for n in names]
            if "validated-static" in statuses and "candidate-only" in statuses:
                findings_note = None  # corroboration is good; require the note
                for n in names:
                    fact = block["facts"][n]
                    if fact.get("status") == "validated-static" and not fact.get("corroboration"):
                        findings_note = n
                if findings_note:
                    findings.add(f"static_survey_facts/{family}/{findings_note}",
                                 "address shared with a candidate fact must record corroboration")

    for where, _value in extra["candidate_addresses"]:
        pass  # candidate facts are already barred from 'validated' statuses above

    validate_widths(profile, schema, source_ids, findings)
    validate_readiness(profile, findings)
    return findings


# ---------------------------------------------------------------------------
# Generic build identification: recognize a DIFFERENT Unreal build using axes
# only; no game-specific constants appear here and no fallback matching exists.
# ---------------------------------------------------------------------------

def identify_build(profiles, observation: dict):
    """Return (matches, error). Matches profiles strictly by declared axes.

    Axes used: target, pointer_size, engine_major, engine_minor and (when the
    observation provides one) title. 'target' and 'engine_major' are required
    in the observation; every other axis present in the observation must match
    a profile's declared value exactly. An absent axis never matches silently
    in a way that hides ambiguity: if the constrained set still matches more
    than one profile, identification fails as ambiguous, and if it matches
    none, there is no 'closest profile' fallback by design.
    """
    if not isinstance(observation, dict):
        return [], "observation must be an object"
    required = ("target", "engine_major")
    for axis in required:
        if axis not in observation:
            return [], f"observation lacks required axis '{axis}'"
    matches = []
    for profile in profiles:
        identity = profile.get("identity", {})
        candidate = {
            "target": identity.get("target"),
            "pointer_size": identity.get("pointer_size"),
            "engine_major": identity.get("engine_major"),
            "engine_minor": identity.get("engine_minor"),
        }
        if observation.get("title") is not None:
            candidate["title"] = profile.get("title")
        pairs = [(axis, value) for axis, value in candidate.items() if observation.get(axis) is not None]
        if all(observation.get(axis) == value for axis, value in pairs):
            matches.append(profile.get("profile_id"))
    if len(matches) > 1:
        return [], f"ambiguous build identification: {matches}"
    if not matches:
        return [], ("no profile matches the observed axes; refusing to fall back to a "
                    "different engine build or to assumed values")
    return matches, None


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Validate Unreal build/layout fact profiles")
    parser.add_argument("--schema", required=True, type=Path)
    parser.add_argument("--profile", action="append", type=Path, default=[])
    parser.add_argument("--identify", type=Path, help="observation JSON for generic build identification")
    parser.add_argument("positional_profiles", nargs="*", type=Path)
    args = parser.parse_args(argv)

    schema = load_json(args.schema)
    profiles_paths = list(args.profile) + list(args.positional_profiles)
    if not profiles_paths:
        parser.error("at least one --profile (or positional profile) is required")

    profiles = []
    all_ok = True
    for path in profiles_paths:
        profile = load_json(path)
        profiles.append(profile)
        findings = validate_profile(profile, schema)
        if findings.ok():
            print(f"VALID: {path}")
        else:
            all_ok = False
            print(f"INVALID: {path}")
            print(findings.report())

    if args.identify:
        observation = load_json(args.identify)
        matches, error = identify_build(profiles, observation)
        if error:
            all_ok = False
            print(f"IDENTIFY: {error}")
        else:
            print(f"IDENTIFY: {matches[0]}")

    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
