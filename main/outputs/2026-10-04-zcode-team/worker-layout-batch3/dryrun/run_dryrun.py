#!/usr/bin/env python3
"""run_dryrun.py — end-to-end DRY RUN orchestrator (synthetic facts only).

worker-layout-batch3 / team 2026-10-04-zcode-team.

Chain executed, in order:
  1. dryrun_chain_fixture (C++, built by this script if absent) executes the
     REAL retained predicate chain — worker-5 probe_guobject_array (batch1),
     batch2 probe_ustruct_chain + probe_ffield_variant, batch3 chain walks —
     on a SYNTHETIC host image and writes verdict records in the assembler
     grammar.
  2. assemble_staged_pair_v2.py consumes the verdicts + identity facts + the
     candidates profile + the compiled width contract and emits the maximal
     staged pair it can (or refuses).
  3. parser_verify (C++, linked against the VENDORED layout parser) re-parses
     the EXACT emitted bytes; this script compares the parsed bundle against
     the assembly manifest entry by entry and against the identity facts.
  4. Negative control: the members text with its BuildId line removed must be
     REFUSED by the vendored parser.
  5. A receipt (dryrun-receipt.json) records every artifact hash and the
     byte-verify verdict.

Everything is a SYNTHETIC-HOST-FIXTURE: no target claim is made anywhere.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORKER = os.path.dirname(HERE)
TEAM = os.path.dirname(WORKER)
ROOT = os.path.abspath(os.path.join(TEAM, "..", ".."))
SPIKE = os.path.join(ROOT, ".superpowers", "native-dependency-spike")

CANDIDATES = os.path.join(TEAM, "worker-layout-minimum", "candidates",
                          "layout-candidates.profile.json")
W1 = os.path.join(SPIKE, "ue4ss-native-layout-widths-1", "coverage.json")
DELTA = os.path.join(SPIKE, "ue4ss-native-layout-widths-2",
                     "coverage-delta.json")
PARSER_DIR = os.path.join(SPIKE, "ue4ss-native-layout-parser-1")

IDENTITY = {
    "engine_revision": "SYNTHETIC-DRYRUN-REVISION",
    "build_id": "SYNTHETIC-DRYRUN-BUILDID",
    "non_shipping": 0,
    "stats": 0,
}


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        h.update(fh.read())
    return h.hexdigest()


def build_binaries():
    fixture_cpp = os.path.join(HERE, "dryrun_chain_fixture.cpp")
    fixture = os.path.join(HERE, "dryrun_chain_fixture")
    if not os.path.exists(fixture) or os.path.getmtime(fixture) < os.path.getmtime(fixture_cpp):
        r = sh(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I", os.path.join(WORKER, "src"), fixture_cpp,
                os.path.join(WORKER, "src", "chain_walk_predicates.cpp"),
                os.path.join(WORKER, "..", "worker-layout-batch2", "src",
                             "reflection_layout_predicates.cpp"),
                os.path.join(WORKER, "..", "worker-5-layout-validator", "src",
                             "townfall_layout_probes.cpp"),
                "-o", fixture])
        if r.returncode != 0:
            raise SystemExit("fixture build failed:\n" + r.stderr)
    verify_cpp = os.path.join(HERE, "parser_verify_main.cpp")
    verify = os.path.join(HERE, "parser_verify")
    if not os.path.exists(verify) or os.path.getmtime(verify) < os.path.getmtime(verify_cpp):
        r = sh(["c++", "-std=c++2b", "-Wall", "-Wextra",
                "-I", os.path.join(PARSER_DIR, "include"), verify_cpp,
                os.path.join(PARSER_DIR, "src", "NativeLayoutFiles.cpp"),
                "-o", verify])
        if r.returncode != 0:
            raise SystemExit("parser_verify build failed:\n" + r.stderr)


def main():
    build_binaries()
    receipt = {"scope": "SYNTHETIC-HOST-FIXTURE only; no target claims",
               "steps": []}

    # 1. predicate chain on the synthetic image
    verdicts_path = os.path.join(HERE, "synthetic-verdicts.json")
    r = sh([os.path.join(HERE, "dryrun_chain_fixture"), verdicts_path])
    receipt["steps"].append({"step": "predicate-chain-fixture",
                             "exit": r.returncode,
                             "tail": r.stdout.strip().splitlines()[-3:]})
    if r.returncode != 0:
        print("\n".join(r.stdout.splitlines()[-5:]), file=sys.stderr)
        raise SystemExit("predicate chain fixture failed")

    # 2. identity facts (synthetic, clearly labeled) + assembler v2
    identity_path = os.path.join(HERE, "synthetic-identity.json")
    with open(identity_path, "w", encoding="utf-8") as fh:
        json.dump(IDENTITY, fh, indent=2)
    emission = os.path.join(HERE, "emission")
    r = sh([sys.executable, os.path.join(WORKER, "bridge",
                                         "assemble_staged_pair_v2.py"),
            "--candidates", CANDIDATES,
            "--widths1", W1, "--delta", DELTA,
            "--verdicts", verdicts_path,
            "--identity", identity_path,
            "--out-dir", emission])
    receipt["steps"].append({"step": "assemble-v2", "exit": r.returncode,
                             "stdout": r.stdout.strip(),
                             "stderr": r.stderr.strip()})
    if r.returncode != 0:
        raise SystemExit("assembler refused (unexpected for the maximal dry "
                         "run): " + r.stderr)

    members_path = os.path.join(emission, "MemberVariableLayout.ini")
    vtables_path = os.path.join(emission, "VTableLayout.ini")
    manifest_path = os.path.join(emission, "assembly-manifest.json")
    with open(manifest_path, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    # 3. byte-verify the emitted pair through the vendored parser
    r = sh([os.path.join(HERE, "parser_verify"), members_path, vtables_path])
    try:
        parsed = json.loads(r.stdout)
    except json.JSONDecodeError:
        parsed = {"ok": False, "error": "no JSON", "raw": r.stdout}
    receipt["steps"].append({"step": "vendored-parser-parse", "exit": r.returncode,
                             "ok": parsed.get("ok")})

    checks = []
    ok = parsed.get("ok") is True and r.returncode == 0
    checks.append({"name": "parser-accepts-exact-bytes", "pass": ok})

    if ok:
        ident = parsed["identity"]
        ident_expect = {
            "format": 1, "pointer_size": 8, "engine_major": 5,
            "engine_minor": 6, "non_shipping": 0, "stats": 0,
            "target": "prospero-x86_64-sysv",
            "engine_revision": IDENTITY["engine_revision"],
            "build_id": IDENTITY["build_id"],
        }
        checks.append({"name": "identity-roundtrip", "pass": ident == ident_expect,
                       "expected": ident_expect, "parsed": ident})

        # entries vs manifest vs candidates offsets
        with open(CANDIDATES, "r", encoding="utf-8") as fh:
            cands = json.load(fh)["candidate_member_offsets"]
        want = []
        for emitted in manifest["emitted"]:
            cls = emitted["class"]
            for member in emitted["members"]:
                off = cands[cls]["entries"][member]["offset"]
                want.append({"section": cls, "key": member, "offset": off})
        got = parsed["members"]
        checks.append({"name": "entries-match-manifest",
                       "pass": got == want,
                       "entry_count": len(got), "expected_count": len(want)})
        if got != want:
            extra = [e for e in got if e not in want]
            missing = [e for e in want if e not in got]
            checks[-1]["extra"] = extra[:10]
            checks[-1]["missing"] = missing[:10]
        checks.append({"name": "vtables-identity-only",
                       "pass": parsed["vtables"] == []})

    # 4. negative control: identity line removed -> parser must refuse
    with open(members_path, "r", encoding="utf-8") as fh:
        members_text = fh.read()
    broken = os.path.join(HERE, "negative-control.ini")
    with open(broken, "w", encoding="utf-8") as fh:
        fh.write("".join(line + "\n" for line in members_text.splitlines()
                         if not line.startswith("BuildId=")))
    r2 = sh([os.path.join(HERE, "parser_verify"), broken, vtables_path])
    refused = (r2.returncode == 3 and '"ok":false' in r2.stdout)
    checks.append({"name": "negative-control-refused", "pass": refused,
                   "exit": r2.returncode, "stdout": r2.stdout.strip()[:200]})

    # 5. receipt
    receipt["byte_verify"] = {"pass": all(c["pass"] for c in checks),
                              "checks": checks}
    receipt["artifacts"] = {
        "synthetic_verdicts": sha256_file(verdicts_path),
        "synthetic_identity": sha256_file(identity_path),
        "MemberVariableLayout.ini": sha256_file(members_path),
        "VTableLayout.ini": sha256_file(vtables_path),
        "assembly-manifest.json": sha256_file(manifest_path),
        "promotable-set.json": sha256_file(os.path.join(HERE,
                                                        "promotable-set.json")),
        "fixture_binary": sha256_file(os.path.join(HERE,
                                                   "dryrun_chain_fixture")),
        "parser_verify_binary": sha256_file(os.path.join(HERE,
                                                         "parser_verify")),
        "emitted_members_bytes": os.path.getsize(members_path),
        "emitted_vtables_bytes": os.path.getsize(vtables_path),
    }
    out = os.path.join(HERE, "dryrun-receipt.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(receipt, fh, indent=2)
    print("DRY RUN byte_verify pass=%s (receipt: %s)"
          % (receipt["byte_verify"]["pass"], out))
    for c in receipt["byte_verify"]["checks"]:
        print("  %-28s %s" % (c["name"], "PASS" if c["pass"] else "FAIL"))
    return 0 if receipt["byte_verify"]["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
