"""Bounded desktop analysis diagnostics; never runtime or installation evidence."""
from copy import deepcopy
import re


REASON_CODES = frozenset({
    "unknown_target", "unbound_package_requirement", "inventory_unresolved", "inventory_incomplete",
    "inventory_issue", "input_provenance_unstable", "missing_package_identity", "missing_source_hash",
    "missing_requirements", "missing_backend", "ambiguous_backend", "version_conflict",
    "unsupported_dependency_constraint", "missing_dependency", "installation_conflict",
    "unknown_dependency_type", "ordering_cycle", "dependency_cycle", "unsupported_reference_cycle",
    "dependency_unresolved", "platform_acceptance_missing", "windows_runtime_unavailable",
    "managed_runtime_unavailable", "analysis_detail_unavailable",
})
_INSTALL = {"state": "unsupported", "reason": "install_operation_unavailable", "scope": []}
_FIELDS = {"schema_version", "operation", "execution_context", "operation_state",
    "compatibility_state", "reason_codes", "install_authority"}
_PREREQUISITE_LIMIT = 128
_PROJECTION_WORK_LIMIT = 10000
_SAFE_ID = re.compile(r"[a-z][a-z0-9_-]*(?:\.[a-z][a-z0-9_-]*)*")
_SAFE_VERSION = re.compile(r"(?:>=[0-9]+(?:\.[0-9]+)*|[A-Za-z0-9][A-Za-z0-9._+-]*)")


def _prerequisite_key(requirement):
    if not isinstance(requirement, dict):
        raise ValueError("Invalid prerequisite")
    identity, version = requirement.get("id"), requirement.get("version")
    if (not isinstance(identity, str) or len(identity) > 96 or not _SAFE_ID.fullmatch(identity)
            or (version is not None and (not isinstance(version, str) or len(version) > 64
                or not _SAFE_VERSION.fullmatch(version)))):
        raise ValueError("Invalid prerequisite")
    return identity, version


def _prerequisite_order(item):
    return item[0], item[1] is not None, item[1] or ""


def _validate_prerequisites(value):
    if (not isinstance(value, dict) or set(value) != {"complete", "items"}
            or type(value["complete"]) is not bool or not isinstance(value["items"], list)
            or len(value["items"]) > _PREREQUISITE_LIMIT
            or (not value["complete"] and value["items"])):
        raise ValueError("Invalid prerequisite projection")
    keys = []
    for item in value["items"]:
        if not isinstance(item, dict) or set(item) != {"id", "version"}:
            raise ValueError("Invalid prerequisite projection")
        keys.append(_prerequisite_key(item))
    if keys != sorted(set(keys), key=_prerequisite_order):
        raise ValueError("Invalid prerequisite projection")


def _blocked_prerequisites(plan):
    """Project blocked capability requirements, not the full dependency graph.

    Planner diagnostics alone identify blockers. Inventory declarations, paths,
    context and evidence confer no fetching, installation or runtime authority.
    Any malformed selected diagnostic erases the entire bounded projection.
    """
    keys = set()
    work = 0

    def visit():
        nonlocal work
        work += 1
        if work > _PROJECTION_WORK_LIMIT:
            raise ValueError("Prerequisite projection work limit")

    def requirement(value):
        visit()
        keys.add(_prerequisite_key(value))
        if len(keys) > _PREREQUISITE_LIMIT:
            raise ValueError("Prerequisite projection limit")

    def requirements(values, diagnostic=False):
        if not isinstance(values, list) or len(values) > _PROJECTION_WORK_LIMIT:
            raise ValueError("Invalid blocked requirements")
        for value in values:
            visit()
            if not isinstance(value, dict):
                raise ValueError("Invalid blocked requirement")
            if diagnostic and value.get("code") == "version_conflict":
                requirements(value.get("requirements"))
            elif "requirement" in value:
                requirement(value["requirement"])
            elif diagnostic:
                raise ValueError("Missing blocked requirement")
            else:
                requirement(value)

    try:
        for record in plan["unresolved"]:
            visit()
            if not isinstance(record, dict):
                raise ValueError("Invalid planner diagnostic")
            code = record.get("code")
            if code == "missing_requirements":
                requirements(record.get("requirements"), diagnostic=True)
            elif code == "unbound_package_requirement":
                requirement(record.get("requirement"))
            elif code == "missing_backend":
                candidates = record.get("candidates")
                if not isinstance(candidates, list) or len(candidates) > _PROJECTION_WORK_LIMIT:
                    raise ValueError("Invalid backend candidates")
                for candidate in candidates:
                    visit()
                    if not isinstance(candidate, dict):
                        raise ValueError("Invalid backend candidate")
                    if "requirements" in candidate:
                        requirements(candidate["requirements"], diagnostic=True)
        conflicts = plan.get("conflicts", [])
        if not isinstance(conflicts, list) or len(conflicts) > _PROJECTION_WORK_LIMIT:
            raise ValueError("Invalid planner conflicts")
        for conflict in conflicts:
            visit()
            if not isinstance(conflict, dict):
                raise ValueError("Invalid planner conflict")
            if conflict.get("code") == "version_conflict":
                requirements(conflict.get("requirements"))
    except ValueError:
        return {"complete": False, "items": []}
    return {"complete": True, "items": [
        {"id": identity, "version": version} for identity, version in sorted(keys, key=_prerequisite_order)]}


def validate_report(value):
    if (not isinstance(value, dict) or not _FIELDS <= set(value)
            or set(value) - _FIELDS - {"blocked_prerequisites"}):
        raise ValueError("Invalid analysis report")
    reasons = value["reason_codes"]
    if (type(value["schema_version"]) is not int or value["schema_version"] != 1
            or value["operation"] != "archive_inventory"
            or value["execution_context"] != "desktop_preparation"
            or value["operation_state"] not in ("supported", "unverified")
            or value["compatibility_state"] not in ("unverified", "unsupported")
            or value["install_authority"] != _INSTALL
            or not isinstance(reasons, list) or not 1 <= len(reasons) <= 32
            or any(not isinstance(code, str) or code not in REASON_CODES for code in reasons)
            or reasons != sorted(set(reasons))
            or "platform_acceptance_missing" not in reasons):
        raise ValueError("Invalid analysis report")
    unsupported = bool({"windows_runtime_unavailable", "managed_runtime_unavailable"} & set(reasons))
    if (value["compatibility_state"] == "unsupported") != unsupported:
        raise ValueError("Invalid compatibility projection")
    if "blocked_prerequisites" in value:
        _validate_prerequisites(value["blocked_prerequisites"])
        if not value["blocked_prerequisites"]["complete"] and "analysis_detail_unavailable" not in reasons:
            raise ValueError("Incomplete prerequisite projection lacks diagnostic")
    return deepcopy(value)


def analysis_report(inventory, plan):
    if not isinstance(inventory, dict) or not isinstance(plan, dict):
        raise ValueError("Invalid analysis input")
    entries, unresolved = inventory.get("entries"), plan.get("unresolved")
    if (not isinstance(entries, list) or len(entries) > 10000
            or not isinstance(unresolved, list) or len(unresolved) > 100000):
        raise ValueError("Invalid analysis input")
    reasons = {"platform_acceptance_missing"}
    prerequisites = _blocked_prerequisites(plan)
    if not prerequisites["complete"]:
        reasons.add("analysis_detail_unavailable")
    for record in unresolved:
        code = record.get("code") if isinstance(record, dict) else None
        reasons.add(code if isinstance(code, str) and code in REASON_CODES else "analysis_detail_unavailable")
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("Invalid analysis entry")
        facts, requirements = entry.get("facts", {}), entry.get("requirements", [])
        if not isinstance(facts, dict) or not isinstance(requirements, list) or len(requirements) > 10000:
            raise ValueError("Invalid analysis entry")
        # A filename or manifest hint cannot establish an executable ABI.
        if entry.get("kind") in ("native_pe", "managed_pe") and facts.get("classification_confidence") == "structural":
            for requirement in requirements:
                identity = requirement.get("id") if isinstance(requirement, dict) else None
                if identity == "runtime.windows":
                    reasons.add("windows_runtime_unavailable")
                elif identity == "runtime.clr":
                    reasons.add("managed_runtime_unavailable")
    complete = (inventory.get("status") == "analyzed" and inventory.get("inventory_complete") is True
        and isinstance(inventory.get("provenance"), dict)
        and inventory["provenance"].get("input_stable") is True)
    if inventory.get("inventory_complete") is not True:
        reasons.add("inventory_incomplete")
    if not isinstance(inventory.get("provenance"), dict) or inventory["provenance"].get("input_stable") is not True:
        reasons.add("input_provenance_unstable")
    unsupported = bool({"windows_runtime_unavailable", "managed_runtime_unavailable"} & reasons)
    return validate_report({"schema_version": 1, "operation": "archive_inventory",
        "execution_context": "desktop_preparation", "operation_state": "supported" if complete else "unverified",
        "compatibility_state": "unsupported" if unsupported else "unverified", "reason_codes": sorted(reasons),
        "install_authority": deepcopy(_INSTALL), "blocked_prerequisites": prerequisites})
