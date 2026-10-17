"""Local l19 destinations and a fail-closed hold before build execution."""
import hashlib
import json
import os
from pathlib import Path

OWN = Path(__file__).resolve().parent


def require_owned(path):
    result = Path(path).resolve()
    if not result.is_relative_to(OWN):
        raise ValueError("l19 output outside owned directory: " + str(result))
    return result


def local_environment():
    env = dict(os.environ)
    for key, relative in {
        "TMPDIR": "tmp", "TEMP": "tmp", "TMP": "tmp",
        "DOTNET_CLI_HOME": "tool-env/dotnet-home",
        "NUGET_PACKAGES": "tool-env/nuget",
        "XDG_CACHE_HOME": "tool-env/cache",
    }.items():
        destination = require_owned(OWN / relative)
        destination.mkdir(parents=True, exist_ok=True)
        env[key] = str(destination)
    env.update(PYTHONDONTWRITEBYTECODE="1", DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1",
               DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_NOLOGO="1",
               DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE="true", CCACHE_DISABLE="1")
    return env


def pinned_file(record):
    if not isinstance(record, dict) or not isinstance(record.get("path"), str):
        raise ValueError("HOLD: missing reviewed file pin")
    path = Path(record["path"])
    digest = record.get("sha256")
    if not isinstance(digest, str) or len(digest) != 64 or not path.is_file():
        raise ValueError("HOLD: incomplete reviewed file pin: " + str(path))
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise ValueError("HOLD: reviewed file pin drift: " + str(path))
    return path


def require_ready():
    prep = json.loads((OWN / "PREP-MANIFEST.json").read_text())
    if prep.get("readiness") != "READY":
        raise ValueError("HOLD: XOM audit, new source manifest and parent build go are pending")
    sources = pinned_file(prep.get("source_manifest"))
    audit = pinned_file(prep.get("xom_audit_receipt"))
    source_data = json.loads(sources.read_text())
    required = {"runtime_owner", "initializer", "bridge", "CopyinStartup.hpp",
                "CodeImageCache.hpp", "CodeImageSha256.hpp", "CodeSignatureProfile.hpp",
                "NativeLayoutBinding.cpp", "NativeLayoutBinding.hpp", "BuildFactsChecks.hpp", "RuntimeBuildProfile.hpp"}
    records = source_data.get("selected_sources")
    if not isinstance(records, dict) or not required <= records.keys():
        raise ValueError("HOLD: new source manifest lacks required l17/l19 units")
    for record in records.values():
        pinned_file(record)
    edition = json.loads((OWN / "EDITIONS-l19.json").read_text())
    scope = edition.get("l19_scope", {})
    if (scope.get("status") != "ADMITTED-FROZEN-SOURCE-MANIFEST"
            or scope.get("source_manifest_sha256") != prep["source_manifest"]["sha256"]
            or scope.get("selected_sources") != records):
        raise ValueError("HOLD: l19 source bindings/admissions have not been frozen")
    go = json.loads((OWN / "BUILD-GO.json").read_text())
    if (go.get("parent_reported_xom_audit") is not True
            or go.get("source_manifest_sha256") != prep["source_manifest"]["sha256"]
            or go.get("xom_audit_sha256") != prep["xom_audit_receipt"]["sha256"]):
        raise ValueError("HOLD: parent build go does not bind the reviewed source/XOM receipts")
    return prep, source_data
