"""A stand-in for tools/profilegen, for the host's tests: same command line, a contract-shaped profile, no analysis.

    python3 tests/fake_profilegen.py [--mode MODE] [--release FILE] --eboot PATH --param PATH --out DIR

It reads the title id and content version from param.json and the sha256 of the executable it is given, and writes
<out>/runtime-profile.json in the shape of docs/profiles/PROFILE-CONTRACT-PROPOSAL.md section 3 (`build_profile`), so the host's reading,
validation and status rules run on a realistic file. Modes: ok (default); hold (wait for --release, bounded, then ok); fail (exit 2
after printing a path to stderr, which the host must never forward); garbage (exit 0 after writing text that is not a profile);
sleep (never finishes in time); silent (exit 0 and write nothing); verified (ok, but the file already carries a console verification,
which a generator must never write: the host must not hide it either).

Like the real generator it refuses (exit 3, nothing written) when the file already in --out carries a verification, unless
--force is given: the host passes --force only for a verification that no longer applies (stale or not a pass).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time

SCHEMA = "modbridge.runtime-profile/1"
SECTIONS = ("binding", "version_map", "elf", "load", "build", "symbols", "structs", "vtables", "ue4ss")


def tiny_elf(marker=b""):
    """The smallest thing that passes the host's executable check: the ELF magic, then padding and a marker."""
    return b"\x7fELF" + bytes(60) + marker


def envelope(value, confidence="high", method="fake.method"):
    return {"value": value, "confidence": confidence, "method": method,
            "evidence": [{"class": "host-derived", "ref": "fake", "sha256": "0" * 64, "note": "fixture"}]}


def digest_of(document):
    sections = {name: document[name] for name in SECTIONS if name in document}
    return hashlib.sha256(json.dumps(sections, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def build_profile(title_id, version, exe_sha256, *, exe_bytes=64, param_sha256="1" * 64, utc="2026-10-07T12:00:00Z",
                  high=10, medium=3, low=2, unresolved=2, plain_binding=False):
    """A profile document in the contract's shape with exactly these counts of resolved values per confidence.

    `binding` itself holds high envelopes: 8 as tools/profilegen writes it (every leaf an envelope, the executable's sha256
    included), 2 with `plain_binding=True` (hashes and sizes plain, as the contract's example has them); `high` counts them, so it
    may not be smaller. The document also holds a null-valued envelope and an `alternatives` list, which the host must not count,
    and `unresolved` rows.
    """
    base = 2 if plain_binding else 8
    if high < base:
        raise ValueError("high must be at least %d" % base)
    symbols = {}
    for level, total in (("high", high - base), ("medium", medium), ("low", low)):
        for index in range(total):
            symbols["%s_%d" % (level, index)] = {"kind": "data", "label": envelope("0x%x" % (0x1000 + index), level),
                                                 "runtime": "0x%x" % (0x401000 + index), "binding": "seed"}
    symbols["gap"] = {"kind": "code", "label": envelope(None, "low", "fake.missing"),
                      "alternatives": [envelope("0x10", "low"), envelope("0x20", "medium")]}
    if plain_binding:
        binding = {"title_id": envelope(title_id), "content_version": envelope(version),
                   "param_json": {"sha256": param_sha256, "bytes": 4096},
                   "executable": {"name": "eboot.bin", "form": "decrypted", "bytes": exe_bytes, "sha256": exe_sha256}}
    else:
        binding = {"title_id": envelope(title_id), "content_version": envelope(version),
                   "param_json": {"sha256": envelope(param_sha256, "high", "fake.sha256"), "bytes": envelope(4096, "high", "fake.size")},
                   "executable": {"name": envelope("eboot.bin", "high", "fake.name"), "form": envelope("decrypted", "high", "fake.form"),
                                  "bytes": envelope(exe_bytes, "high", "fake.size"), "sha256": envelope(exe_sha256, "high", "fake.sha256")}}
    document = {
        "schema": SCHEMA,
        "profile_id": "%s/%s/%s" % (title_id, version, exe_sha256[:8]),
        "status": "generated",
        "profile_digest": "",
        "verification": None,
        "generated": {"utc": utc, "generator": {"name": "fake_profilegen", "version": "0", "commit": "none"},
                      "inputs": [{"role": "executable", "sha256": exe_sha256, "bytes": exe_bytes},
                                 {"role": "param.json", "sha256": param_sha256}]},
        "binding": binding,
        "version_map": {"ref": "modbridge.game-profile/1", "path": None, "console_content_version": version, "pc_build": None},
        "elf": {"segments": [{"i": 0, "type": "LOAD", "flags": "--X", "p_offset": "0x4000", "p_vaddr": "0x0"}]},
        "load": {"base_policy": "confirm-at-runtime", "file_offset_rule": "file = p_offset + (vaddr - p_vaddr) per PT_LOAD"},
        "build": {},
        "symbols": symbols,
        "structs": {},
        "vtables": {},
        "ue4ss": {"settings": {}, "emit_signature_scripts": False},
        "emit": [],
        "unresolved": [{"id": "unresolved-%d" % index, "why": "fixture", "needs": "more-analysis"} for index in range(unresolved)],
    }
    document["profile_digest"] = digest_of(document)
    return document


def with_verification(document, *, digest=None, result="pass", status="console-verified"):
    """What the console test's receipt ingester would add (the host's tests plant it; no generator writes it)."""
    verified = dict(document)
    verified["status"] = status
    verified["verification"] = {"receipt_id": "fixture-receipt-1", "date_utc": "2026-10-07T13:00:00Z",
                                "profile_digest": document["profile_digest"] if digest is None else digest,
                                "runtime_build": {"build_id": "fixture", "sha256": document["binding"]["executable"]["sha256"]},
                                "verified": ["symbols.high_0"], "result": result}
    return verified


def write_profile(path, document):
    """The profile file the way a generator writes it: 2-space indent, LF, atomic."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", default="ok")
    parser.add_argument("--release")
    parser.add_argument("--eboot", required=True)
    parser.add_argument("--param", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)
    if args.mode == "fail":
        print("fixture generator failure reading " + args.eboot, file=sys.stderr)
        return 2
    if args.mode == "sleep":
        time.sleep(60)
        return 0
    if args.mode == "silent":
        return 0
    out = Path(args.out) / "runtime-profile.json"
    if args.mode == "garbage":
        out.write_text("this is not a profile\n")
        return 0
    if args.mode == "hold" and args.release:
        deadline = time.monotonic() + 20
        while not os.path.exists(args.release) and time.monotonic() < deadline:
            time.sleep(0.05)
    param = json.loads(Path(args.param).read_text(encoding="utf-8"))
    sha = hashlib.sha256()
    with open(args.eboot, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            sha.update(chunk)
    document = build_profile(param["titleId"], param["contentVersion"], sha.hexdigest(), exe_bytes=os.path.getsize(args.eboot),
                             param_sha256=hashlib.sha256(Path(args.param).read_bytes()).hexdigest())
    if out.exists():
        try:
            earlier = json.loads(out.read_text(encoding="utf-8"))
        except ValueError:
            earlier = None
        if isinstance(earlier, dict) and earlier.get("verification") is not None and not args.force:
            print("refusing to overwrite a verified profile", file=sys.stderr)
            return 3
    if args.mode == "verified":
        document = with_verification(document)
    write_profile(out, document)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
