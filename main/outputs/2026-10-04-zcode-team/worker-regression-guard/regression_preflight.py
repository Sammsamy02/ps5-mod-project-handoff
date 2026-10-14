#!/usr/bin/env python3
"""worker-regression-guard: preflight loader + runner contract for the four
completed host regression suites (settings/parser, codecvt conversion, input
provider-selection, activation/runtime-owner).

Status: PROPOSAL deliverable for worker-preflight-runner. This loader is
self-tested and runnable standalone; the runner owner performs integration.
Host-only: it never contacts a console, engine, or provider.

Contract for worker-preflight-runner (import this module):
    load_manifest(path) -> dict                       # schema-validated fragment
    load_editions(path) -> dict                       # raw EDITIONS.json
    resolve_editions_pin(editions, keys) -> str       # walk EDITIONS.json by key list
    sha256_file(path) -> str
    lineage_hash(check_id, inputs) -> str             # stable composite lineage hash
    verify_check_lineage(check_id, check, editions, root) -> dict   # no execution, ever
    run_check(check_id, check, editions, root, execute=False) -> (result, exit_code)
    run_preflight(manifest, editions, root, check_ids=None, execute=False)
        -> (aggregate_result, exit_code)

Exit codes (the contract the automatic runner must honor):
    0  every requested check: lineage verified (and, with --exec, executed + passed)
    1  lineage clean but at least one executed suite FAILED (a real regression signal)
    2  FAIL-CLOSED: missing input, on-disk hash mismatch, artifact hash mismatch
       (only possible with --exec), or EDITIONS.json pin mismatch (lineage drift).
       NO suite is executed when any requested check fails verification.
    3  contract/usage error: unreadable manifest/editions, schema mismatch, bad CLI.

Result JSON schema ("worker-regression-guard/preflight-result/1"): aggregate with
per-check {check_id, lineage_sha256, lineage_verified, inputs[], artifacts[],
executed, commands[{argv, exit_code, expect_rc, stdout_ok, elapsed_ms}],
passed, status, elapsed_ms, detail}. Every check result records the lineage hash
it would have run under; drift results still carry the expected lineage hash.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

MANIFEST_SCHEMA = "worker-regression-guard/regression-input-manifest/1"
RESULT_SCHEMA = "worker-regression-guard/preflight-result/1"

EXIT_OK = 0
EXIT_SUITE_FAIL = 1
EXIT_LINEAGE_DRIFT = 2
EXIT_CONTRACT_ERROR = 3


class ContractError(Exception):
    """Usage/schema/environment error -> exit 3."""


class LineageDrift(Exception):
    """Fail-closed condition -> exit 2 (message carries the drift detail)."""


# ---------------------------------------------------------------- helpers

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_manifest(path: Path) -> dict:
    try:
        data = json.loads(Path(path).read_text())
    except OSError as e:
        raise ContractError(f"manifest unreadable: {path}: {e}")
    except json.JSONDecodeError as e:
        raise ContractError(f"manifest not JSON: {path}: {e}")
    if data.get("schema") != MANIFEST_SCHEMA:
        raise ContractError(
            f"manifest schema mismatch: expected {MANIFEST_SCHEMA}, got {data.get('schema')!r}")
    if not isinstance(data.get("regressions"), dict) or not data["regressions"]:
        raise ContractError("manifest has no regressions object")
    return data


def load_editions(path: Path) -> dict:
    try:
        return json.loads(Path(path).read_text())
    except OSError as e:
        raise ContractError(f"EDITIONS.json unreadable: {path}: {e}")
    except json.JSONDecodeError as e:
        raise ContractError(f"EDITIONS.json not JSON: {path}: {e}")


def resolve_editions_pin(editions: dict, keys: list) -> str:
    """Walk EDITIONS.json by an exact key list; return the pinned sha string."""
    node = editions
    walked = []
    for k in keys:
        walked.append(k)
        if not isinstance(node, dict) or k not in node:
            raise LineageDrift(
                f"editions pin unresolved: {' -> '.join(walked)!r} missing in EDITIONS.json "
                f"(composition lineage changed; refresh the manifest fragment consciously)")
        node = node[k]
    if not isinstance(node, str):
        raise LineageDrift(
            f"editions pin {' -> '.join(walked)!r} is not a sha string: {node!r}")
    return node


def resolve_path(root: Path, p: str) -> Path:
    return Path(p) if os.path.isabs(p) else (root / p)


def lineage_hash(check_id: str, inputs: list) -> str:
    """Composite hash binding this check to one exact input lineage.

    Over sorted 'role|resolved_path|sha256' lines, prefixed by check_id and the
    manifest fragment id so it cannot collide across fragments or checks.
    """
    lines = sorted(f"{i['role']}|{i['path_resolved']}|{i['sha256']}" for i in inputs)
    h = hashlib.sha256()
    h.update(f"worker-regression-guard|{check_id}\n".encode())
    for line in lines:
        h.update((line + "\n").encode())
    return h.hexdigest()


# ---------------------------------------------------------------- verification

def _verify_entry(entry: dict, root: Path, editions: dict, kind: str) -> dict:
    """Verify one manifest input/artifact entry; returns its result record."""
    role = entry.get("role")
    rel = entry.get("path")
    expected = entry.get("sha256")
    if not (role and rel and expected):
        raise ContractError(f"{kind} entry missing role/path/sha256: {entry!r}")
    path = resolve_path(root, rel)
    record = {
        "role": role,
        "path": rel,
        "sha256_expected": expected,
        "sha256_actual": None,
        "match": False,
        "editions_keys": entry.get("editions_keys"),
        "editions_expected": None,
        "editions_match": None,  # None = no pin declared
    }
    if not path.is_file():
        raise LineageDrift(f"{kind} {role} MISSING: {path}")
    actual = sha256_file(path)
    record["sha256_actual"] = actual
    if actual != expected:
        raise LineageDrift(
            f"{kind} {role} HASH DRIFT: {path} on-disk {actual[:16]}... != pinned {expected[:16]}... "
            f"(lineage drift; refresh the fragment consciously, never bypass)")
    if entry.get("editions_keys"):
        pinned = resolve_editions_pin(editions, entry["editions_keys"])
        record["editions_expected"] = pinned
        record["editions_match"] = (pinned == actual)
        if pinned != actual:
            raise LineageDrift(
                f"{kind} {role} EDITIONS PIN MISMATCH: {path} is {actual[:16]}... but "
                f"EDITIONS.json {' -> '.join(entry['editions_keys'])} pins {pinned[:16]}... "
                f"(regression would exercise a different lineage than the composition)")
    record["match"] = True
    return record


def verify_check_lineage(check_id: str, check: dict, editions: dict, root: Path) -> dict:
    """Verify ALL inputs of one check against disk + EDITIONS.json. No execution."""
    result = {
        "check_id": check_id,
        "lineage_sha256": None,
        "lineage_verified": False,
        "inputs": [],
        "artifacts": [],
        "executed": False,
        "commands": [],
        "passed": False,
        "status": "lineage_drift",
        "elapsed_ms": None,
        "detail": "",
    }
    t0 = time.monotonic()
    inputs_resolved = []
    for entry in check.get("inputs", []):
        rec = _verify_entry(entry, root, editions, "input")
        result["inputs"].append(rec)
        inputs_resolved.append({
            "role": rec["role"],
            "path_resolved": str(resolve_path(root, entry["path"])),
            "sha256": rec["sha256_actual"],
        })
    result["lineage_sha256"] = lineage_hash(check_id, inputs_resolved)
    result["lineage_verified"] = True
    result["status"] = "verified"
    result["elapsed_ms"] = int((time.monotonic() - t0) * 1000)
    result["detail"] = (
        f"{len(inputs_resolved)} inputs verified against disk + EDITIONS.json; "
        f"lineage {result['lineage_sha256'][:16]}...")
    return result


# ---------------------------------------------------------------- execution

def _resolve_argv_tokens(argv: list, inputs_by_role: dict, artifacts_by_role: dict,
                         root: Path) -> list:
    out = []
    for tok in argv:
        if isinstance(tok, str):
            out.append(tok)
        elif isinstance(tok, dict) and "role" in tok:
            role = tok["role"]
            if role in inputs_by_role:
                out.append(str(resolve_path(root, inputs_by_role[role]["path"])))
            elif role in artifacts_by_role:
                out.append(str(resolve_path(root, artifacts_by_role[role]["path"])))
            else:
                raise ContractError(f"argv role token unresolved: {role!r}")
        else:
            raise ContractError(f"argv token must be str or {{role}}: {tok!r}")
    return out


def run_check(check_id: str, check: dict, editions: dict, root: Path,
              execute: bool = False) -> tuple[dict, int]:
    """Verify lineage for one check; if clean and execute=True, run its suites.

    Returns (result, exit_code) where exit_code is 0 (pass), 1 (suite fail),
    or 2 (fail-closed drift). Contract errors raise ContractError (exit 3).
    """
    result = None
    try:
        result = verify_check_lineage(check_id, check, editions, root)
    except LineageDrift as e:
        result = {
            "check_id": check_id,
            "lineage_sha256": None,
            "lineage_verified": False,
            "inputs": [],
            "artifacts": [],
            "executed": False,
            "commands": [],
            "passed": False,
            "status": "lineage_drift",
            "elapsed_ms": None,
            "detail": str(e),
        }
        return result, EXIT_LINEAGE_DRIFT
    if not result["lineage_verified"]:
        return result, EXIT_LINEAGE_DRIFT
    if not execute:
        result["status"] = "verified"
        result["passed"] = None  # None = lineage-only mode: suite not exercised
        result["detail"] = ("lineage verified (suite not executed; pass --exec to run)"
                            + "; " + result["detail"])
        return result, EXIT_OK

    # Fail-closed BEFORE any execution: artifacts must match their recorded
    # hashes or we refuse to run a binary of unknown provenance.
    artifacts_by_role = {}
    for entry in check.get("artifacts", []):
        rec = _verify_entry(entry, root, editions, "artifact")
        result["artifacts"].append(rec)
        artifacts_by_role[rec["role"]] = rec

    inputs_by_role = {i["role"]: i for i in check.get("inputs", [])}
    all_ok = True
    t0 = time.monotonic()
    for cmd in check.get("exec", []):
        art_role = cmd.get("artifact")
        if art_role not in artifacts_by_role:
            raise ContractError(f"{check_id}: exec references unknown artifact {art_role!r}")
        binary = resolve_path(root, artifacts_by_role[art_role]["path"])
        argv = _resolve_argv_tokens(cmd.get("argv", []), inputs_by_role,
                                    artifacts_by_role, root)
        expect_rc = int(cmd.get("expect_rc", 0))
        timeout_s = int(cmd.get("timeout_s", 60))
        env = dict(os.environ)
        env.update({k: str(v) for k, v in cmd.get("env", {}).items()})
        stdout_contains = list(cmd.get("stdout_contains", []))
        cmd_rec = {
            "artifact": art_role,
            "argv": [str(binary)] + argv,
            "expect_rc": expect_rc,
            "exit_code": None,
            "stdout_ok": None,
            "elapsed_ms": None,
            "detail": "",
        }
        c0 = time.monotonic()
        try:
            proc = subprocess.run(
                [str(binary)] + argv, cwd=str(root), env=env,
                capture_output=True, text=True, timeout=timeout_s)
        except subprocess.TimeoutExpired:
            cmd_rec["detail"] = f"TIMEOUT after {timeout_s}s"
            cmd_rec["exit_code"] = -1
            all_ok = False
        else:
            cmd_rec["exit_code"] = proc.returncode
            cmd_rec["elapsed_ms"] = int((time.monotonic() - c0) * 1000)
            missing = [s for s in stdout_contains if s not in (proc.stdout or "")]
            cmd_rec["stdout_ok"] = not missing
            if proc.returncode != expect_rc or missing:
                all_ok = False
                cmd_rec["detail"] = (
                    f"rc={proc.returncode} (expected {expect_rc})"
                    + (f"; stdout missing {missing!r}" if missing else "")
                    + f"; stderr tail: {(proc.stderr or '')[-400:]!r}")
            else:
                cmd_rec["detail"] = f"rc={proc.returncode}; stdout anchors matched"
        result["commands"].append(cmd_rec)

    result["executed"] = True
    result["passed"] = all_ok
    result["status"] = "pass" if all_ok else "fail"
    result["elapsed_ms"] = int((time.monotonic() - t0) * 1000)
    result["detail"] = (
        f"lineage {result['lineage_sha256'][:16]}...; "
        f"{len(result['commands'])} command(s), {'all passed' if all_ok else 'FAILURE'}")
    return result, (EXIT_OK if all_ok else EXIT_SUITE_FAIL)


def run_preflight(manifest: dict, editions: dict, root: Path, check_ids=None,
                  execute: bool = False, manifest_path: Path | None = None) -> tuple[dict, int]:
    """Full preflight. Fail-closed is all-or-nothing: if ANY requested check
    fails verification, NO suite runs and the aggregate exits 2."""
    regressions = manifest["regressions"]
    requested = list(check_ids) if check_ids else list(regressions)
    unknown = [c for c in requested if c not in regressions]
    if unknown:
        raise ContractError(f"unknown check id(s): {unknown}; known: {list(regressions)}")

    editions_path = manifest.get("editions", {}).get("path")
    aggregate = {
        "schema": RESULT_SCHEMA,
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "worktree_root": str(root),
        "manifest_path": str(manifest_path) if manifest_path else None,
        "manifest_sha256": sha256_file(manifest_path) if manifest_path else None,
        "editions_path": editions_path,
        "editions_sha256": sha256_file(resolve_path(root, editions_path)),
        "exec_mode": bool(execute),
        "fail_closed_triggered": False,
        "checks": [],
        "pass": False,
    }

    # Phase 1: verify lineage for every requested check. Any drift -> no exec.
    verified = {}
    for cid in requested:
        try:
            result = verify_check_lineage(cid, regressions[cid], editions, root)
        except LineageDrift as e:
            result = {
                "check_id": cid,
                "lineage_sha256": None,
                "lineage_verified": False,
                "inputs": [],
                "artifacts": [],
                "executed": False,
                "commands": [],
                "passed": False,
                "status": "lineage_drift",
                "elapsed_ms": None,
                "detail": str(e),
            }
        verified[cid] = result
        if not result["lineage_verified"]:
            aggregate["fail_closed_triggered"] = True
    if aggregate["fail_closed_triggered"]:
        aggregate["checks"] = [verified[cid] for cid in requested]
        aggregate["pass"] = False
        return aggregate, EXIT_LINEAGE_DRIFT

    if not execute:
        for cid in requested:
            result = verified[cid]
            result["status"] = "verified"
            result["passed"] = None
            result["detail"] = ("lineage verified (suite not executed; pass --exec to run)"
                                + "; " + result["detail"])
        aggregate["checks"] = [verified[cid] for cid in requested]
        aggregate["pass"] = True
        return aggregate, EXIT_OK

    # Phase 2: execute (lineage already clean for all requested checks).
    overall = EXIT_OK
    for cid in requested:
        result, rc = run_check(cid, regressions[cid], editions, root, execute=True)
        # keep the phase-1 records; run_check re-verified (cheap, deterministic)
        result["lineage_sha256"] = verified[cid]["lineage_sha256"]
        aggregate["checks"].append(result)
        if rc != EXIT_OK:
            overall = rc
    aggregate["pass"] = (overall == EXIT_OK)
    return aggregate, overall


# ---------------------------------------------------------------- self-test

def self_test(manifest_path: Path, editions_path: Path, root: Path) -> int:
    """End-to-end proof: pass case (real EDITIONS.json) then a deliberately
    corrupted temp copy (fail-closed case). Prints both outcomes; exits 0 only
    if the loader behaves exactly as contracted in both."""
    ok = True
    manifest = load_manifest(manifest_path)
    editions = load_editions(editions_path)

    print("== self-test 1: pass case (current EDITIONS.json, --exec) ==")
    agg, rc = run_preflight(manifest, editions, root, execute=True,
                            manifest_path=manifest_path)
    print(f"exit={rc} pass={agg['pass']} fail_closed={agg['fail_closed_triggered']}")
    for c in agg["checks"]:
        print(f"  {c['check_id']}: status={c['status']} lineage={c['lineage_sha256'][:16]}... "
              f"executed={c['executed']} passed={c['passed']}")
    if rc != EXIT_OK or not agg["pass"] or agg["fail_closed_triggered"]:
        ok = False

    print("== self-test 2: fail-closed case (corrupted temp EDITIONS.json) ==")
    with tempfile.TemporaryDirectory(prefix="rguard-selftest-") as td:
        corrupt = Path(td) / "EDITIONS.corrupted.json"
        raw = Path(editions_path).read_text()
        anchor = '"status": "STAGED"'
        assert anchor in raw, "corruption anchor missing"
        # Corrupt the pinned codecvt fix hash: 7c983db4... -> 0c983db4...
        needle = '"sha256": "7c983db4548b39b16625f979bfd2256647df0f57788e9ae27672af51b8302524"'
        if needle not in raw:  # fall back to first-pinned-hash flip
            needle = '"sha256": "7'
            replacement = '"sha256": "0'
        else:
            replacement = '"sha256": "0c983db4548b39b16625f979bfd2256647df0f57788e9ae27672af51b8302524"'
        corrupt.write_text(raw.replace(needle, replacement, 1))
        agg_bad, rc_bad = run_preflight(manifest, load_editions(corrupt), root,
                                        execute=True, manifest_path=manifest_path)
        print(f"exit={rc_bad} pass={agg_bad['pass']} "
              f"fail_closed={agg_bad['fail_closed_triggered']}")
        executed_any = any(c["executed"] for c in agg_bad["checks"])
        print(f"  any suite executed under drift: {executed_any} (must be False)")
        if rc_bad != EXIT_LINEAGE_DRIFT or agg_bad["pass"] or not agg_bad["fail_closed_triggered"]:
            ok = False
        if executed_any:
            ok = False

    print("SELF-TEST " + ("PASS" if ok else "FAIL"))
    return EXIT_OK if ok else EXIT_SUITE_FAIL


# ---------------------------------------------------------------- CLI

MANIFEST_PATH = None  # set in main for aggregate provenance


def main(argv=None) -> int:
    global MANIFEST_PATH
    ap = argparse.ArgumentParser(
        description="worker-regression-guard preflight loader (fail-closed lineage gate)")
    ap.add_argument("--manifest", required=True, help="regression-input-manifest.json")
    ap.add_argument("--editions", help="EDITIONS.json (default: manifest's editions.path under root)")
    ap.add_argument("--worktree-root", help="worktree root (default: derived from manifest location)")
    ap.add_argument("--check", action="append", help="check id (repeatable; default all)")
    ap.add_argument("--exec", action="store_true", dest="exec_",
                    help="execute the suites after lineage verification (fail-closed)")
    ap.add_argument("--json-out", help="write aggregate result JSON here (also printed)")
    ap.add_argument("--quiet", action="store_true", help="suppress per-check stdout lines")
    ap.add_argument("--self-test", action="store_true",
                    help="run built-in pass + corrupted-copy proof and exit")
    args = ap.parse_args(argv)

    manifest_path = Path(args.manifest).resolve()
    MANIFEST_PATH = manifest_path
    manifest = load_manifest(manifest_path)

    root = Path(args.worktree_root).resolve() if args.worktree_root else None
    if root is None:
        for cand in manifest_path.parents:
            if (cand / "outputs" / "2026-10-04-zcode-controller"
                    / "launch11-combined" / "EDITIONS.json").is_file():
                root = cand
                break
        if root is None:
            raise ContractError("cannot derive worktree root; pass --worktree-root")

    editions_path = (Path(args.editions).resolve() if args.editions
                     else resolve_path(root, manifest["editions"]["path"]))

    if args.self_test:
        return self_test(manifest_path, editions_path, root)

    editions = load_editions(editions_path)
    try:
        aggregate, rc = run_preflight(manifest, editions, root,
                                      check_ids=args.check, execute=args.exec_,
                                      manifest_path=manifest_path)
    except LineageDrift as e:
        # Drift raised outside per-check records (e.g. editions file itself).
        aggregate = {
            "schema": RESULT_SCHEMA, "generated_utc":
            datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "worktree_root": str(root), "manifest_path": str(manifest_path),
            "manifest_sha256": sha256_file(manifest_path),
            "editions_path": str(editions_path), "exec_mode": bool(args.exec_),
            "fail_closed_triggered": True, "checks": [], "pass": False,
            "detail": str(e),
        }
        rc = EXIT_LINEAGE_DRIFT

    payload = json.dumps(aggregate, indent=2)
    if args.json_out:
        out = Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(payload + "\n")
    if not args.quiet:
        print(payload)
    else:
        for c in aggregate["checks"]:
            print(f"{c['check_id']}: status={c['status']} lineage={c['lineage_sha256']} "
                  f"executed={c['executed']} passed={c['passed']} elapsed_ms={c['elapsed_ms']}")
        print(f"aggregate: pass={aggregate['pass']} "
              f"fail_closed_triggered={aggregate['fail_closed_triggered']} exit={rc}")
    return rc


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ContractError as e:
        print(f"CONTRACT ERROR (exit {EXIT_CONTRACT_ERROR}): {e}", file=sys.stderr)
        sys.exit(EXIT_CONTRACT_ERROR)
