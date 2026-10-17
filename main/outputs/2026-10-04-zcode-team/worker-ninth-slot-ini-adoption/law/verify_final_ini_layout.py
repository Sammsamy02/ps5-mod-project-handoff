#!/usr/bin/env python3
"""verify_final_ini_layout.py — FINAL-LAYOUT VERIFIER for the definitive
return-session ploader.ini (worker-final-ini-layout, 2026-10-04; NINTH-SLOT
revision by worker-ninth-slot-ini-adoption, 2026-10-04).

NINTH-SLOT REVISION: pins the 9-entry law of
worker-carrier-probe-build/STAGE-PLAN.md — the cc1 carrier probe appended
AFTER the igf `= 0` line (next free index; no existing line edited).
Supersedes the 8-entry law (400 B, sha bbc34c16…, 7 registered), which this
verifier pinned until this revision — that verifier CORRECTLY printed
FINAL-INI-FAIL on 9-entry bytes before adoption (the supersession decision
the constants brief deferred to the layout owner chain). The 8-entry law is
preserved as: law/ploader.ini.canonical.asfound + law/*.asfound snapshots in
worker-ninth-slot-ini-adoption/ and this worker's prior receipts.

Usage:
    python3 verify_final_ini_layout.py <ini-file>          # verify a file
    python3 verify_final_ini_layout.py --selftest          # built-in cases

Order-sensitive by design: the pinned ploader parser ACCEPTS many of the
sequences this verifier rejects (probe-after-composition parses fine; its
consequence is a degraded initializer verdict, not a parse failure), so the
byte-exact canonical gate lives HERE, not in the parser.

Checks, in order:
  1. byte-exact equality with ploader.ini.canonical (sha256 printed either way)
  2. on byte-mismatch: structured diagnosis — header count, per-line set and
     order vs canonical, duplicates, unknown lines, mode flips
  3. parser-law re-check with the ridealongs law (registered == 8, igf never
     registered, MAX_PLUGINS headroom)

Exit 0 only for FINAL-INI-OK. Never edits anything; host-side verifier."""
import hashlib
import sys

HERE = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-final-ini-layout"

HEADER = "[PPSA33286]"
CANONICAL_LINES = [
    "/data/plugins/modbridge_exec_write_probe_armed.prx = 2",
    "/data/plugins/modbridge_fc1pool.prx = 2",
    "/data/plugins/modbridge_supplier_composition.prx = 2",
    "/data/plugins/modbridge_qh_driver.prx = 2",
    "/data/plugins/modbridge_runtime_aligned_observer.prx = 2",
    "/data/plugins/modbridge_runtime_aligned_entry.prx = 2",
    "/data/plugins/modbridge_path_test.prx = 2",
    "/data/plugins/modbridge_ingame_facts.prx = 0",
    "/data/plugins/modbridge_carrier.prx = 2",
]
CANONICAL_BYTES = (HEADER + "\n" + "".join(l + "\n" for l in CANONICAL_LINES)).encode()
EXPECTED_SHA = hashlib.sha256(CANONICAL_BYTES).hexdigest()
KNOWN_PATHS = {l.rsplit(" = ", 1)[0] for l in CANONICAL_LINES}
EXPECTED_REGISTERED = 8
MAX_PLUGINS = 10


def diagnose(data: bytes) -> list:
    problems = []
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError:
        return ["file is not ASCII — ini bytes must be ASCII"]
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    if not lines:
        problems.append("file is empty — no header, no entries")
        return problems
    if lines[0].strip() != HEADER:
        problems.append(f"first line is {lines[0]!r}, expected header {HEADER!r}")
        return problems
    header_count = sum(1 for l in lines if l.strip() == HEADER)
    if header_count != 1:
        problems.append(f"header appears {header_count}x, expected exactly 1")
    body = [l for l in lines[1:] if l.strip() not in ("",) and not l.strip().startswith((";", "#"))]
    seen = {}
    for i, l in enumerate(body):
        path = l.rsplit(" = ", 1)[0].strip() if " = " in l else l.strip()
        seen.setdefault(path, []).append(i + 2)  # 1-based, +header
    for path, at in seen.items():
        if len(at) > 1:
            problems.append(f"DUPLICATE line {path} at lines {at} — ploader "
                            f"does NOT dedupe paths (registers/injects twice)")
    for path in KNOWN_PATHS:
        if path not in seen:
            problems.append(f"MISSING expected line {path}")
    for path in seen:
        if path not in KNOWN_PATHS and path != HEADER:
            problems.append(f"UNKNOWN line {path} — the parser would REGISTER "
                            f"it (no dedupe, slot burn); not in the contract")
    for path in KNOWN_PATHS:
        want = next(l for l in CANONICAL_LINES if l.startswith(path))
        if path in seen:
            got = body[seen[path][0] - 2].strip()
            if got != want:
                problems.append(f"MODE/PIN mismatch {path}: got {got!r}, "
                                f"pinned {want!r} — never rewrite pinned modes")
    actual_active = [l for l in body if l.strip() in
                     [c for c in CANONICAL_LINES if not c.endswith("= 0")]]
    if [l.strip() for l in body] != CANONICAL_LINES:
        problems.append("ORDER mismatch vs canonical (injection order == file "
                        "order; probe/fc1 MUST precede composition):")
        for i, (got, want) in enumerate(zip([l.strip() for l in body] + [None] * 8,
                                            CANONICAL_LINES)):
            if got != want:
                problems.append(f"  line {i + 2}: got {got!r}, want {want!r}")
                break
    if len(actual_active) > MAX_PLUGINS:
        problems.append(f"{len(actual_active)} active lines exceed "
                        f"MAX_PLUGINS={MAX_PLUGINS}")
    return problems


def parser_law(data: bytes) -> list:
    """Re-check with the ridealongs pinned-parser law (import if available)."""
    sys.path.insert(0, HERE)
    import simulate_ploader_ini_final as sim
    text = data.decode("ascii", errors="replace")
    reg, _dropped = sim.registered(text)
    problems = []
    if len(reg) != EXPECTED_REGISTERED:
        problems.append(f"parser registered {len(reg)} plugins, expected "
                        f"exactly {EXPECTED_REGISTERED} (mode-0 lines consume no slot)")
    paths = [p for p, _ in reg]
    expect = [l.rsplit(" = ", 1)[0] for l in CANONICAL_LINES if not l.endswith("= 0")]
    if paths != expect:
        problems.append(f"parser registration order {paths} != canonical "
                        f"active order {expect}")
    if any(p.endswith("modbridge_ingame_facts.prx") for p in paths):
        problems.append("igf line registered despite mode 0 (parser violation)")
    if len(reg) > MAX_PLUGINS:
        problems.append("MAX_PLUGINS exceeded")
    return problems


def verify(path: str) -> int:
    data = open(path, "rb").read()
    sha = hashlib.sha256(data).hexdigest()
    print(f"file={path} bytes={len(data)} sha256={sha}")
    if data == CANONICAL_BYTES:
        reg_problems = parser_law(data)
        if reg_problems:
            print("FINAL-INI-FAIL (parser law):")
            for p in reg_problems:
                print(" -", p)
            return 1
        print("FINAL-INI-OK: byte-exact canonical 9-entry layout; 8 active "
              "entries in definitive order (carrier last, after igf=0); "
              "igf=0 skipped; 2/10 slot headroom.")
        return 0
    print(f"FINAL-INI-FAIL: bytes differ from canonical (expected sha256 "
          f"{EXPECTED_SHA})")
    for p in diagnose(data):
        print(" -", p)
    return 1


def selftest() -> int:
    import tempfile, os
    ok = 0
    cases_fail = {
        "canonical_plus_extra": CANONICAL_BYTES + b"/data/plugins/x.prx = 2\n",
        "probe_after_composition": (HEADER + "\n" + "".join(
            l + "\n" for l in [
                CANONICAL_LINES[1], CANONICAL_LINES[2], CANONICAL_LINES[0],
                *CANONICAL_LINES[3:]])).encode(),
        "fc1_mode1": CANONICAL_BYTES.replace(b"modbridge_fc1pool.prx = 2",
                                             b"modbridge_fc1pool.prx = 1"),
        "igf_mode2": CANONICAL_BYTES.replace(b"modbridge_ingame_facts.prx = 0",
                                             b"modbridge_ingame_facts.prx = 2"),
        "duplicate_fc1": CANONICAL_BYTES + CANONICAL_LINES[1].encode() + b"\n",
        "no_trailing_newline": CANONICAL_BYTES.rstrip(b"\n"),
        "empty": b"",
        "header_only": (HEADER + "\n").encode(),
        # ninth-slot revision cases: the carrier line must sit EXACTLY after
        # the igf =0 line (next free index), at mode 2, exactly once
        "carrier_before_igf": (HEADER + "\n" + "".join(
            l + "\n" for l in [
                *CANONICAL_LINES[:7], CANONICAL_LINES[8],
                CANONICAL_LINES[7]])).encode(),
        "carrier_mode1": CANONICAL_BYTES.replace(
            b"modbridge_carrier.prx = 2", b"modbridge_carrier.prx = 1"),
        "duplicate_carrier": CANONICAL_BYTES
                             + CANONICAL_LINES[8].encode() + b"\n",
        "carrier_only_8entry": (HEADER + "\n" + "".join(
            l + "\n" for l in CANONICAL_LINES[:8])).encode(),  # old 8-entry law
    }
    fd, tmp = tempfile.mkstemp()
    os.close(fd)
    try:
        with open(tmp, "wb") as f:
            f.write(CANONICAL_BYTES)
        ok += verify(tmp) == 0
        for name, blob in cases_fail.items():
            with open(tmp, "wb") as f:
                f.write(blob)
            rc = verify(tmp)
            print(f"   case={name} -> {'REJECTED' if rc != 0 else 'ACCEPTED (BUG)'}")
            ok += rc != 0
    finally:
        os.unlink(tmp)
    total = 1 + len(cases_fail)
    print(f"SELFTEST: {ok}/{total} expectations met")
    return 0 if ok == total else 1


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(verify(sys.argv[1]))
