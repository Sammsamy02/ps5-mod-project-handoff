"""Bounded desktop analysis diagnostics; never runtime or installation evidence."""
from copy import deepcopy


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


def validate_report(value):
    if not isinstance(value, dict) or set(value) != _FIELDS:
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
    return deepcopy(value)


def analysis_report(inventory, plan):
    if not isinstance(inventory, dict) or not isinstance(plan, dict):
        raise ValueError("Invalid analysis input")
    entries, unresolved = inventory.get("entries"), plan.get("unresolved")
    if (not isinstance(entries, list) or len(entries) > 10000
            or not isinstance(unresolved, list) or len(unresolved) > 100000):
        raise ValueError("Invalid analysis input")
    reasons = {"platform_acceptance_missing"}
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
        "install_authority": deepcopy(_INSTALL)})
