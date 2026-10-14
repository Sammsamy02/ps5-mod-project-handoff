#!/usr/bin/env python3
"""prelaunch_rowcheck.py -- pre-launch loadlist ROW deploy gate.

WHY (Claude round-8 item 6; the 2026-10-11 -6 incident,
CLAUDE-ROUND4-SPAWN-PROVEN.md "DEPLOY NOTES"): the loadlist declared
ueselftest/enabled.txt which was never uploaded -> MFA refused the WHOLE
module at boot (mfa_activate_failed -6) and the launch was wasted. The
all-or-nothing admission worked exactly as designed; what was missing was
a pre-launch check of every row. This tool is that check.

WHAT IT DOES (read-only; NEVER writes to the console):
  1. fetch the console loadlist.txt (default
     /data/modbridge/roots/PPSA33286/loadlist.txt, FTP :2121);
  2. parse EVERY 'file\\t<role>\\t<path>\\t<size>' row (header rows
     modforge-loadlist / title / engine / framework / mod are skipped);
  3. fetch each row's file and verify it EXISTS at its EXACT declared
     byte size (SIZE-EXACT MFA law, mod-manager/DESIGN.md GG: size
     mismatch = mfa_activate_failed code=-8; a declared-but-absent file
     = the -6 whole-module refusal);
  4. optionally (--expect pins.json, {"<path>": {"size": N, "sha256":
     "..."} | "<sha256>"}), also compare sha256 / a pinned size;
  5. report EVERY mismatch with the EXACT row text; exit 0 only when
     every row passes. --json prints one machine-readable document
     (consumed by prep_modded_launch.py STEP 5.5).

GAME-CLOSED LAW (round-4 live lesson, MANIFEST-LAUNCH-1.md): FTP file
reads return EMPTY under game load -- this check is only valid with the
game CLOSED. This tool does not enforce that itself; its callers do
(prep_modded_launch.py STEP 1 refuses while eboot.bin is running).

FIXTURE MODE (--from-dir DIR): read DIR as the console root (DIR must
contain loadlist.txt and the ue4ss/... tree). ZERO network calls -- the
offline tests exercise every path this way. No console contact is ever
made in fixture mode.

USAGE:
    python3 prelaunch_rowcheck.py                        # console (FTP)
    python3 prelaunch_rowcheck.py --json --out-dir D     # tooling mode
    python3 prelaunch_rowcheck.py --from-dir fixture/    # offline test
    python3 prelaunch_rowcheck.py --expect pins.json
    python3 prelaunch_rowcheck.py --help

EXIT CODES: 0 = every file row verified present at its exact size;
1 = gate FAILED (missing / size-mismatch / sha-mismatch / malformed row,
or a refused loadlist); 2 = usage error. A receipt JSON is always
written under --out-dir (default: the durable evidence tree
outputs/2026-10-08-l25-prep/evidence/rowcheck/<ts>/ -- evidence never
lives only in /tmp).

Written 2026-10-11 by agent ID (rowcheck task). Mac-only, battery-safe:
python3 + curl only; no builds; console writes never happen.
"""

import argparse
import datetime
import hashlib
import json
import os
import subprocess
import sys

VERSION = ("prelaunch_rowcheck.py 1.0 (agent ID rowcheck, "
           "2026-10-08-l25-prep/harvest)")

DEFAULT_CONSOLE_IP = "ps5-host-not-configured.invalid"          # WAKE-RUNBOOK constants block
FTP_PORT = 2121                              # ftpsrv (harvest.py FTP_PORT)
REMOTE_ROOT = "/data/modbridge/roots/PPSA33286"  # mod root (runbook header)
LOADLIST_REL = "loadlist.txt"

CURL_CONNECT_TIMEOUT = "5"
CURL_MAX_TIME = "20"        # per-file text fetch; rows are text-class
FETCH_MAX_FILESIZE = 8388608  # 8 MiB bound; largest known row is ~197 KB
CURL_SUBPROCESS_TIMEOUT = 45

# AGENTS.md PS5 rule 1: ftpsrv DECRYPTS SELF/SPRX on download and fakes
# LIST/SIZE to the decrypted size. Loadlist rows are text-class by law
# (MANIFEST-LAUNCH-1 §4); an executable-class row is at minimum suspicious.
EXEC_EXTS = (".prx", ".self", ".sprx", ".elf")

_PREP_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_EVIDENCE_ROOT = os.path.join(_PREP_ROOT, "evidence", "rowcheck")

FIX_INSTRUCTIONS = (
    "upload the missing rows or fix the loadlist: re-pin each row "
    "size-exactly to the actual bytes (wc -c law; never hand-trim or pad "
    "a file to force a pass), then re-run this check")


class Usage(Exception):
    """Bad invocation (exit 2)."""


def now_utc():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def ts_slug():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y%m%dT%H%M%SZ")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def run_cmd(argv, timeout):
    try:
        p = subprocess.run(argv, capture_output=True, text=True,
                           timeout=timeout)
        return {"rc": p.returncode, "out": p.stdout or "",
                "err": p.stderr or "", "argv": argv}
    except subprocess.TimeoutExpired:
        return {"rc": -1, "out": "", "err": "local subprocess timeout",
                "argv": argv}
    except OSError as e:
        return {"rc": -1, "out": "", "err": "cannot execute: %s" % e,
                "argv": argv}


# ---------------------------------------------------------------------------
# Byte sources: the real console (FTP, read-only) or a local fixture dir.
# Both return {"status": "ok"|"missing"|"error", "data": bytes|None,
#             "detail": str}.
# ---------------------------------------------------------------------------

def _spool_name(rel):
    return rel.replace("/", "__")


class FtpSource(object):
    """curl FTP GET per fetch (harvest.py / prep_modded_launch.py shape).
    Text-class files only -- no SITE SELF here, matching every other
    loadlist-surface tool (lua_fast_deploy.sh, modmanager ftp_get)."""

    def __init__(self, console_ip, ftp_port, root, spool_dir):
        self.console_ip = console_ip
        self.ftp_port = int(ftp_port)
        self.root = root
        self.spool_dir = spool_dir

    def describe(self):
        return {"console_ip": self.console_ip, "ftp_port": self.ftp_port,
                "root": self.root}

    def fetch(self, rel, tag=None):
        dest = os.path.join(self.spool_dir,
                            (tag + "-" if tag else "") + _spool_name(rel))
        url = "ftp://%s:%d%s/%s" % (self.console_ip, self.ftp_port,
                                    self.root, rel)
        argv = ["curl", "--silent", "--show-error", "--fail",
                "--connect-timeout", CURL_CONNECT_TIMEOUT,
                "--max-time", CURL_MAX_TIME,
                "--max-filesize", str(FETCH_MAX_FILESIZE),
                "-o", dest, url]
        r = run_cmd(argv, CURL_SUBPROCESS_TIMEOUT)
        if r["rc"] == 0 and os.path.isfile(dest):
            try:
                with open(dest, "rb") as f:
                    return {"status": "ok", "data": f.read(),
                            "detail": "readback: %s" % dest}
            except OSError as e:
                return {"status": "error", "data": None,
                        "detail": "readback unreadable: %s" % e}
        if r["rc"] == 78 or "550" in (r["err"] or ""):
            # prep_modded_launch STEP 1 precedent: rc 78 / RETR 550 = absent
            return {"status": "missing", "data": None,
                    "detail": "absent on console (rc 78 / RETR 550)"}
        return {"status": "error", "data": None,
                "detail": "curl rc=%s err=%s" % (
                    r["rc"], (r["err"] or "").strip()[:160])}


class FixtureSource(object):
    """--from-dir: a local directory standing in for the console root.
    Zero network. Read-only."""

    def __init__(self, root_dir):
        self.root_dir = os.path.abspath(root_dir)

    def describe(self):
        return {"from_dir": self.root_dir}

    def fetch(self, rel, tag=None):
        path = os.path.join(self.root_dir, *rel.split("/"))
        if not os.path.isfile(path):
            return {"status": "missing", "data": None,
                    "detail": "absent in fixture root (no such file)"}
        try:
            with open(path, "rb") as f:
                return {"status": "ok", "data": f.read(),
                        "detail": "fixture read: %s" % path}
        except OSError as e:
            return {"status": "error", "data": None,
                    "detail": "unreadable: %s" % e}


# ---------------------------------------------------------------------------
# Loadlist parsing (row shape mirrors modmanager.parse_loadlist: a file
# row is exactly 4 tab-separated fields, fields[0] == 'file').
# ---------------------------------------------------------------------------

def parse_loadlist(data):
    """bytes -> (rows, malformed). rows: dicts with lineno/raw/role/path/
    size; malformed: dicts with lineno/raw/detail. Path-safety guard
    included (no absolute paths, no '..')."""
    text = data.decode("utf-8", errors="replace")
    rows, malformed = [], []
    for lineno, raw in enumerate(text.splitlines(), 1):
        if not raw.strip():
            continue
        fields = raw.split("\t")
        if fields[0] != "file":
            continue  # header rows: modforge-loadlist/title/engine/...
        if len(fields) != 4:
            malformed.append({"lineno": lineno, "raw": raw,
                              "detail": "expected 4 tab-separated fields "
                                        "(file\\t<role>\\t<path>\\t<size>), "
                                        "got %d" % len(fields)})
            continue
        role, path, size_s = fields[1], fields[2], fields[3]
        if not size_s.isdigit():
            malformed.append({"lineno": lineno, "raw": raw,
                              "detail": "size field %r is not a non-negative "
                                        "integer" % size_s})
            continue
        if not path or path.startswith("/") or ".." in path.split("/"):
            malformed.append({"lineno": lineno, "raw": raw,
                              "detail": "unsafe or empty path %r" % path})
            continue
        rows.append({"lineno": lineno, "raw": raw, "role": role,
                     "path": path, "size": int(size_s)})
    return rows, malformed


def load_expect(path):
    """--expect manifest -> {rel: {"size": int?, "sha256": str?}}.
    Accepts values as {"size":..,"sha256":..} dicts or bare sha256
    strings. Raises Usage on anything unparsable."""
    try:
        with open(path, "r", encoding="utf-8") as f:
            raw = json.load(f)
    except OSError as e:
        raise Usage("--expect %s unreadable: %s" % (path, e))
    except ValueError as e:
        raise Usage("--expect %s is not valid JSON: %s" % (path, e))
    if not isinstance(raw, dict):
        raise Usage("--expect %s must be a JSON object keyed by loadlist "
                    "row path" % path)
    out = {}
    for key, val in raw.items():
        if isinstance(val, str):
            entry = {"sha256": val.lower()}
        elif isinstance(val, dict):
            entry = {}
            if "sha256" in val:
                if not isinstance(val["sha256"], str):
                    raise Usage("--expect[%r]: sha256 must be a string" % key)
                entry["sha256"] = val["sha256"].lower()
            if "size" in val:
                try:
                    entry["size"] = int(val["size"])
                except (TypeError, ValueError):
                    raise Usage("--expect[%r]: size must be an integer" % key)
            if not entry:
                raise Usage("--expect[%r]: empty pin (need size and/or "
                            "sha256)" % key)
        else:
            raise Usage("--expect[%r]: value must be a sha256 string or an "
                        "object with size/sha256" % key)
        out[key] = entry
    return out


# ---------------------------------------------------------------------------
# The check itself
# ---------------------------------------------------------------------------

def run_check(args):
    out_dir = args.out_dir or os.path.join(
        DEFAULT_EVIDENCE_ROOT, "rowcheck-%s-%d" % (ts_slug(), os.getpid()))
    os.makedirs(out_dir, exist_ok=True)
    spool = os.path.join(out_dir, "readback")
    os.makedirs(spool, exist_ok=True)

    if args.from_dir:
        if not os.path.isdir(args.from_dir):
            raise Usage("--from-dir %s is not a directory" % args.from_dir)
        src = FixtureSource(args.from_dir)
        mode = "fixture"
    else:
        src = FtpSource(args.console_ip, args.ftp_port, args.root, spool)
        mode = "ftp"

    doc = {
        "tool": VERSION,
        "utc": now_utc(),
        "mode": mode,
        "source": src.describe(),
        "loadlist": None,
        "counts": {"rows": 0, "pass": 0, "missing": 0, "size_mismatch": 0,
                   "sha_mismatch": 0, "malformed": 0, "fetch_error": 0},
        "warnings": [],
        "refusal": None,
        "ok": False,
        "rows": [],
        "fix": None,
    }

    def finish(rc):
        doc["ok"] = (rc == 0)
        doc["fix"] = None if rc == 0 else FIX_INSTRUCTIONS
        receipt = os.path.join(out_dir,
                               "rowcheck-receipt-%s-%d.json"
                               % (ts_slug(), os.getpid()))
        doc["receipt"] = receipt
        doc["out_dir"] = out_dir
        with open(receipt, "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=2, sort_keys=True)
            f.write("\n")
        if args.json:
            print(json.dumps(doc, indent=2, sort_keys=True))
        else:
            print_human(doc)
        return rc

    # 1. the loadlist itself
    expect = load_expect(args.expect) if args.expect else {}
    ll = src.fetch(LOADLIST_REL, tag="loadlist")
    if ll["status"] == "missing":
        doc["refusal"] = ("REFUSED: loadlist.txt absent at %s%s -- there is "
                          "nothing to verify; a launch would have NO "
                          "admission manifest. %s"
                          % (REMOTE_ROOT if mode == "ftp" else
                             args.from_dir,
                             "" if mode == "fixture" else " (fetch rc 78 / "
                             "RETR 550)", FIX_INSTRUCTIONS))
        return finish(1)
    if ll["status"] == "error":
        doc["refusal"] = ("REFUSED: cannot fetch loadlist.txt (%s). Never "
                          "verify -- or launch -- on an unknown console "
                          "state." % ll["detail"])
        return finish(1)
    doc["loadlist"] = {"rel": LOADLIST_REL, "bytes": len(ll["data"]),
                       "sha256": sha256_bytes(ll["data"])}

    # 2. parse the rows
    rows, malformed = parse_loadlist(ll["data"])
    doc["counts"]["rows"] = len(rows)
    for m in malformed:
        doc["rows"].append({"lineno": m["lineno"], "raw": m["raw"],
                            "status": "MALFORMED", "detail": m["detail"]})
    doc["counts"]["malformed"] = len(malformed)
    if not rows and not malformed:
        doc["refusal"] = ("REFUSED: loadlist.txt carries ZERO 'file' rows "
                          "(%d bytes, sha %s) -- the documented base "
                          "composition alone has 7; an empty manifest means "
                          "either a wrong fetch or a broken upload. %s"
                          % (doc["loadlist"]["bytes"],
                             doc["loadlist"]["sha256"][:16] + "...",
                             FIX_INSTRUCTIONS))
        return finish(1)

    # 3. verify every row
    seen_sizes = {}
    for row in rows:
        entry = {"lineno": row["lineno"], "raw": row["raw"],
                 "role": row["role"], "path": row["path"],
                 "declared_size": row["size"], "actual_size": None,
                 "sha256": None, "status": None, "detail": None}
        doc["rows"].append(entry)
        if row["path"].lower().endswith(EXEC_EXTS):
            doc["warnings"].append(
                "row %s: executable-class extension -- ftpsrv decrypts "
                "SELF/SPRX on download and FAKES SIZE (AGENTS.md PS5 rule "
                "1); loadlist rows are text-class by law, treat any pass "
                "here with suspicion" % row["path"])
        prev = seen_sizes.get(row["path"])
        if prev is not None and prev != row["size"]:
            doc["warnings"].append(
                "duplicate row %s pins two different sizes (%d and %d)"
                % (row["path"], prev, row["size"]))
        seen_sizes[row["path"]] = row["size"]

        fr = src.fetch(row["path"])
        if fr["status"] == "missing":
            entry["status"] = "MISSING"
            entry["detail"] = ("declared but ABSENT on the console (%s) -- "
                               "MFA refuses the WHOLE module at boot (the "
                               "-6 incident)" % fr["detail"])
            doc["counts"]["missing"] += 1
            continue
        if fr["status"] == "error":
            entry["status"] = "FETCH-ERROR"
            entry["detail"] = fr["detail"]
            doc["counts"]["fetch_error"] += 1
            continue

        data = fr["data"]
        entry["actual_size"] = len(data)
        entry["sha256"] = sha256_bytes(data)
        pin = expect.get(row["path"])
        if len(data) != row["size"]:
            entry["status"] = "SIZE-MISMATCH"
            entry["detail"] = ("row declares %d bytes but the console file "
                               "is %d bytes (MFA would fail code=-8; "
                               "re-pin the row to the actual bytes)"
                               % (row["size"], len(data)))
            doc["counts"]["size_mismatch"] += 1
        elif pin and "size" in pin and pin["size"] != len(data):
            entry["status"] = "SIZE-MISMATCH"
            entry["detail"] = ("--expect pins %d bytes but the console file "
                               "is %d bytes (reconcile the pin)"
                               % (pin["size"], len(data)))
            doc["counts"]["size_mismatch"] += 1
        elif pin and "sha256" in pin and pin["sha256"] != entry["sha256"]:
            entry["status"] = "SHA-MISMATCH"
            entry["detail"] = ("--expect pins sha256 %s... but the console "
                               "file hashes %s... (content drift)"
                               % (pin["sha256"][:16],
                                  entry["sha256"][:16]))
            doc["counts"]["sha_mismatch"] += 1
        else:
            entry["status"] = "PASS"
            bits = ["present, %d bytes == declared" % len(data)]
            if pin and "sha256" in pin:
                bits.append("sha256 matches --expect pin")
            entry["detail"] = "; ".join(bits)
            doc["counts"]["pass"] += 1

    # 4. expect keys that never matched a row: warning, not failure (the
    # loadlist is the admission authority; an extra pin cannot break MFA).
    row_paths = set(r["path"] for r in rows)
    for key in sorted(set(expect) - row_paths):
        doc["warnings"].append("--expect pins %s, which is not a loadlist "
                               "row (ignored by the size gate)" % key)

    bad = (doc["counts"]["missing"] + doc["counts"]["size_mismatch"]
           + doc["counts"]["sha_mismatch"] + doc["counts"]["malformed"]
           + doc["counts"]["fetch_error"])
    return finish(0 if bad == 0 else 1)


def print_human(doc):
    c = doc["counts"]
    src = doc["source"]
    where = src.get("from_dir") or "ftp://%s:%d%s" % (
        src.get("console_ip"), src.get("ftp_port", FTP_PORT),
        src.get("root", REMOTE_ROOT))
    print("[rowcheck] tool   : %s" % doc["tool"])
    print("[rowcheck] source : %s (%s mode)" % (where, doc["mode"]))
    if doc["loadlist"]:
        print("[rowcheck] list   : %s (%d bytes, sha256 %s)"
              % (doc["loadlist"]["rel"], doc["loadlist"]["bytes"],
                 doc["loadlist"]["sha256"]))
    print("[rowcheck] rows   : %d file row(s) -- every row must exist at "
          "its EXACT declared size (game-closed law)" % c["rows"])
    for w in doc["warnings"]:
        print("[rowcheck] WARN   : %s" % w)
    for r in doc["rows"]:
        if r["status"] == "PASS":
            print("  PASS        L%-4d %-64s %s"
                  % (r["lineno"], r["path"], r["detail"]))
        else:
            print("  %-12s L%-4d %-64s declared=%s actual=%s"
                  % (r["status"], r["lineno"], r["path"],
                     r.get("declared_size", "-"),
                     r.get("actual_size", "-")
                     if r.get("actual_size") is not None else "(absent)"))
            print("        detail: %s" % r["detail"])
            print("        row  : %r" % r["raw"])
    if doc["refusal"]:
        print("[rowcheck] %s" % doc["refusal"])
    verdict = "PASS" if doc["ok"] else "FAIL"
    print("[rowcheck] VERDICT: %s -- %d rows: %d pass, %d missing, "
          "%d size-mismatch, %d sha-mismatch, %d malformed, %d fetch-error"
          % (verdict, c["rows"], c["pass"], c["missing"],
             c["size_mismatch"], c["sha_mismatch"], c["malformed"],
             c["fetch_error"]))
    if not doc["ok"]:
        print("[rowcheck] FIX: %s" % doc["fix"])
    print("[rowcheck] receipt: %s" % doc.get("receipt"))


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="prelaunch_rowcheck.py",
        description="Pre-launch loadlist ROW deploy gate: fetch the console "
                    "loadlist.txt, parse every 'file\\t<role>\\t<path>\\t"
                    "<size>' row, fetch each file, and verify it exists at "
                    "its EXACT declared size (the -6 incident law). "
                    "Read-only; never writes to the console.")
    p.add_argument("--console-ip", default=DEFAULT_CONSOLE_IP,
                   help="console host (default %(default)s); ignored in "
                        "--from-dir fixture mode")
    p.add_argument("--ftp-port", type=int, default=FTP_PORT,
                   help="ftpsrv port (default %(default)s)")
    p.add_argument("--root", default=REMOTE_ROOT,
                   help="console mod root that holds loadlist.txt "
                        "(default %(default)s)")
    p.add_argument("--from-dir", default=None,
                   help="FIXTURE MODE: read this local directory as the "
                        "console root (must contain loadlist.txt); ZERO "
                        "network calls -- offline testing only")
    p.add_argument("--expect", default=None,
                   help="optional JSON manifest of sha256/size pins, "
                        "{\"<row path>\": {\"size\": N, \"sha256\": \"..\"}"
                        " | \"<sha256>\"}")
    p.add_argument("--out-dir", default=None,
                   help="where the readback spool + receipt land (default "
                        "the durable evidence tree outputs/2026-10-08-"
                        "l25-prep/evidence/rowcheck/<ts>/)")
    p.add_argument("--json", action="store_true",
                   help="print ONE machine-readable JSON document on "
                        "stdout (for prep_modded_launch.py STEP 5.5)")
    return p.parse_args(argv)


def main(argv):
    try:
        args = parse_args(argv)
        return run_check(args)
    except Usage as e:
        print("prelaunch_rowcheck.py: error: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
