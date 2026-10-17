#!/usr/bin/env python3
"""deploy-precreate-writable.py -- DEPLOY-TIME pre-creation of the shim's
writable-area directories (agent IQ, Claude round-12 items 1-2, 2026-10-12).

THE LAW THIS IMPLEMENTS (IQ-SHIM-AUDIT.md section 4):
  The LuaPathShim write-area does NOTHING at module_start/mod-prepare and
  creates directories only lazily (one-shot, first write, error_code-checked,
  spill-backed). The RELIABLE way the physical dirs
  /data/modbridge/writable/<mod>/ exist is DEPLOY TIME: ftpsrv is a
  ROOT-LEVEL process — it is NOT subject to the game-process /data contract
  (modforge_activation.h:6-8: fresh opens fail ENOENT after module_start —
  that contract binds the GAME process, not ftpsrv). This snippet runs
  before every launch, exactly like every other deploy upload.

WHY THIS IS ADMISSION-SAFE (IQ-SHIM-AUDIT.md section 3):
  mfa_activate reads ONLY <root>/loadlist.txt and the files its rows list
  (claude-takeover-43a96969/source/runtime/modforge_activation.c:
  read_loadlist :211-224, read_exact :162-208, join_path :151 — there is no
  directory walk and no path outside the root is ever opened). The writable
  tree /data/modbridge/writable is a SIBLING of roots/, so it can never be
  admission-checked, needs no loadlist row, and cannot trip the size-exact
  all-or-nothing gate (a row naming a missing file — the 2026-10-11 -6
  incident class — is impossible: no row ever names it). The step 5.5
  rowcheck is UNAFFECTED (it only fetches declared rows under roots/).

DROP-IN WIRING for harvest/prep_modded_launch.py (step 5.6, right after
step5_5_rowcheck and before step6_arm_ploader):

    # 5.6 writable-area pre-creation (agent IQ, Claude round-12 items 1-2;
    # see round5-staging/shim-fix/deploy-precreate-writable.py)
    mods = step5_6_ensure_writable(console_ip, out_dir, ledger, args.dry_run)
    # -> returns the mod list; ledger rows land in the same LAUNCH-READY
    #    verification table; NEVER aborts the launch when only this step
    #    fails (the shim's in-memory spill keeps every mod functional --
    #    IB round-8; the failure is recorded as FAIL rows + a warning note).

This file is also runnable STANDALONE for a one-off pre-create:
    python3 deploy-precreate-writable.py --console ps5-host-not-configured.invalid --out-dir D
    python3 deploy-precreate-writable.py --dry-run   # zero network, exit 0

RULES OBEYED: MAC-ONLY, no game-process contact (run with the game CLOSED,
same law as step 5.5), never touches anything under /data/modbridge/roots/
(the MFA size-exact tree), marker upload is verified by readback (cmp
byte-exact), idempotent (existing dirs are left untouched), no process
kills, no abort-on-spill-covered-condition.
"""

import argparse
import datetime
import os
import subprocess
import sys
import tempfile

# --- constants mirrored from prep_modded_launch.py (single source: that file)
FTP_PORT = 2121                       # ftpsrv (LAUNCH-PLAN "Console endpoints")
LOADLIST_REMOTE = "/data/modbridge/roots/PPSA33286/loadlist.txt"  # step 5.5
WRITABLE_ROOT = "/data/modbridge/writable"  # IB round-8 writable root (sibling of roots/)
MARKER_NAME = "deploy-keep.txt"       # visible in LIST; documents the dir's origin
CURL_MAX = "60"


def run_cmd(argv, timeout):
    """prep_modded_launch.py run_cmd shape (subprocess + rc/out/err/argv)."""
    try:
        p = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
        return {"rc": p.returncode, "out": p.stdout, "err": p.stderr, "argv": argv}
    except subprocess.TimeoutExpired:
        return {"rc": -1, "out": "", "err": "local subprocess timeout", "argv": argv}
    except OSError as e:
        return {"rc": -1, "out": "", "err": "cannot execute: %s" % e, "argv": argv}


def ftp_get_cmd(console_ip, remote, dest, max_time=CURL_MAX):
    """prep_modded_launch.py ftp_get_cmd (no SITE SELF: plain files/dirs)."""
    return ["curl", "--silent", "--show-error", "--connect-timeout", "5",
            "--max-time", max_time, "-o", dest,
            "ftp://%s:%d%s" % (console_ip, FTP_PORT, remote)]


def ftp_put_create_dirs_cmd(console_ip, local, remote, max_time=CURL_MAX):
    """ftp_put_cmd shape + --ftp-create-dirs: curl issues the MKD/CWD chain
    for the parent dirs of `remote` before STOR (works even on ftpsrvs
    without a usable bare MKD quote path; STOR+CWD+MKD is the core set)."""
    return ["curl", "--silent", "--show-error", "--fail",
            "--connect-timeout", "5", "--max-time", max_time,
            "-Q", "SITE SELF", "--ftp-create-dirs", "--upload-file", local,
            "ftp://%s:%d%s" % (console_ip, FTP_PORT, remote)]


def ftp_mkd_cmd(console_ip, remote_dir, max_time=CURL_MAX):
    """MKD <absolute dir> as a pre-transfer quote, then LIST of '/'. Proven
    form on this ftpsrv (l16 record: 'MKD /data/modbridge/intake returns 226
    rc 0') once the parent exists. Replaces --ftp-create-dirs (2026-10-12):
    each level gets its own ledger row and its own verify."""
    return ["curl", "--silent", "--show-error", "--fail",
            "--connect-timeout", "5", "--max-time", max_time,
            "-Q", "MKD %s" % remote_dir, "-o", os.devnull,
            "ftp://%s:%d/" % (console_ip, FTP_PORT)]


def ftp_put_cmd(console_ip, local, remote, max_time=CURL_MAX):
    """Plain STOR (proven lua_fast_deploy.sh shape: curl -T; text file)."""
    return ["curl", "--silent", "--show-error", "--fail",
            "--connect-timeout", "5", "--max-time", max_time,
            "--upload-file", local,
            "ftp://%s:%d%s" % (console_ip, FTP_PORT, remote)]


def ensure_dir(console_ip, remote_dir, out_dir, label, ledger):
    """LIST -> (missing) MKD -> LIST verify. True when the dir exists after."""
    exists = dir_exists(console_ip, remote_dir, out_dir, label, False)
    if exists is True:
        ledger.add("5.6", "dir %s" % remote_dir, "present or created",
                   "already present (LIST ok, left untouched)", True)
        return True
    if exists is None:
        ledger.add("5.6", "dir %s" % remote_dir, "LIST verdict",
                   "indeterminate (network) -- spill covers writes", False,
                   "shim spill keeps mods functional regardless")
        return False
    mk = run_cmd(ftp_mkd_cmd(console_ip, remote_dir), 90)
    after = dir_exists(console_ip, remote_dir, out_dir, label + "-after", False)
    ok = after is True
    ledger.add("5.6", "dir %s" % remote_dir, "MKD then LIST ok",
               "MKD rc %s, LIST after: %s" % (mk["rc"], after), ok,
               "" if ok else "MKD refused or not visible; spill covers writes "
               "(err %s)" % mk["err"][:120])
    return ok


def parse_mod_names(loadlist_text):
    """loadlist 'file <role> <path> <size>' rows (TAB- or space-split, same
    tolerance as the shim's load_pinned_set) -> ordered unique mod names.

    A mod name is the first path component after 'ue4ss/Mods/' in any file
    row, EXCLUDING 'shared' (Mods/shared is a library folder, not a mod --
    the shim's own calling-mod detection excludes it too).
    """
    names = []
    seen = set()
    for raw in loadlist_text.splitlines():
        fields = raw.replace("\t", " ").split()
        if len(fields) < 4 or fields[0] != "file":
            continue
        path = fields[2]
        prefix = "ue4ss/Mods/"
        if not path.startswith(prefix):
            continue
        rest = path[len(prefix):]
        name = rest.split("/", 1)[0]
        if name and name != "shared" and name not in seen:
            seen.add(name)
            names.append(name)
    return names


def dir_exists(console_ip, remote_dir, out_dir, label, dry_run):
    """LIST the dir: True = exists, False = missing (curl rc 78 / FTP 550),
    None = indeterminate (network/other error). Writes the listing aside."""
    listing = os.path.join(out_dir, "writable-dir-%s.listing" % label)
    if dry_run:
        return False  # dry-run assumes missing so the plan prints every command
    r = run_cmd(ftp_get_cmd(console_ip, remote_dir + "/", listing), 90)
    if r["rc"] == 0 and os.path.isfile(listing):
        return True
    if r["rc"] in (9, 78):  # 9 = CWD refused (missing dir on this ftpsrv, seen
        return False        # live 2026-10-12); 78 = RETR/LIST 550 (lock-probe law)
    return None


def ensure_writable_dirs(console_ip, out_dir, ledger, dry_run=False):
    """STEP 5.6 body. Returns the mod list (possibly empty on fetch
    failure). NEVER raises for a spill-covered condition: every failure is
    a ledger row; only a loadlist FETCH failure with rc not in {0,78} is a
    hard Abort-worthy inconsistency (caller decides -- prep wiring treats
    it as abort: without the loadlist the rowcheck 5.5 would also fail)."""
    os.makedirs(out_dir, exist_ok=True)

    # 1. fetch the loadlist (rowcheck 5.5 already proved it readable with
    #    the game closed; we re-fetch the tiny file for the mod names).
    ll_local = os.path.join(out_dir, "loadlist-for-writable.txt")
    if dry_run:
        mods = ["<mod-from-loadlist>"]
        ledger.add("5.6", "writable-area mod list (parse loadlist.txt)",
                   ">=1 mod", "dry-run placeholder", True,
                   "commands printed below; zero network in --dry-run")
    else:
        r = run_cmd(ftp_get_cmd(console_ip, LOADLIST_REMOTE, ll_local), 90)
        if r["rc"] != 0 or not os.path.isfile(ll_local):
            ledger.add("5.6", "loadlist.txt fetch (for mod names)", "curl rc 0",
                       "rc %s err %s" % (r["rc"], r["err"][:120]), False,
                       "step 5.5 rowcheck fetches the same file -- reconcile")
            return []
        mods = parse_mod_names(open(ll_local, "r", encoding="utf-8",
                                    errors="replace").read())
        ledger.add("5.6", "writable-area mod list (parse loadlist.txt)",
                   ">=1 mod", "%d mods: %s" % (len(mods), ", ".join(mods)),
                   len(mods) >= 1)

    if dry_run:
        for name in mods:
            remote_dir = "%s/%s" % (WRITABLE_ROOT, name)
            marker_local = os.path.join(out_dir, MARKER_NAME)
            marker_remote = "%s/%s" % (remote_dir, MARKER_NAME)
            ledger.add("5.6", "pre-create %s (LIST)" % remote_dir,
                       "rc 0 exists / rc 78 missing", "dry-run (planned)", True)
            ledger.add("5.6", "pre-create %s (marker upload --ftp-create-dirs)"
                       % marker_remote, "curl rc 0", "dry-run (planned)", True)
            ledger.add("5.6", "verify %s (LIST + readback cmp)" % remote_dir,
                       "dir listed + marker byte-exact", "dry-run (planned)",
                       True)
        return mods

    # 2. per mod: LIST -> (if missing) marker upload with --ftp-create-dirs
    #    -> LIST + readback cmp verify. Idempotent: an EXISTING dir (from a
    #    previous run) is verified present and left untouched.
    marker_body = ("modbridge writable-area deploy marker\ncreated: %s\n"
                   "tool: round5-staging/shim-fix/deploy-precreate-writable.py\n"
                   % datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"))
    if not ensure_dir(console_ip, WRITABLE_ROOT, out_dir, "root", ledger):
        return mods
    for name in mods:
        remote_dir = "%s/%s" % (WRITABLE_ROOT, name)
        marker_remote = "%s/%s" % (remote_dir, MARKER_NAME)
        existed = dir_exists(console_ip, remote_dir, out_dir, name + "-before",
                             False) is True
        if not ensure_dir(console_ip, remote_dir, out_dir, name, ledger):
            continue
        if existed:
            continue  # idempotent: an existing dir (and its marker) is left as is

        marker_local = os.path.join(out_dir, "%s-%s" % (name, MARKER_NAME))
        with open(marker_local, "w", encoding="utf-8") as f:
            f.write(marker_body)
        up = run_cmd(ftp_put_cmd(console_ip, marker_local, marker_remote), 120)
        rb = os.path.join(out_dir, "%s-%s.readback" % (name, MARKER_NAME))
        rbr = run_cmd(ftp_get_cmd(console_ip, marker_remote, rb), 90)
        same = (up["rc"] == 0 and rbr["rc"] == 0 and os.path.isfile(rb) and
                open(marker_local, "rb").read() == open(rb, "rb").read())
        ledger.add("5.6", "marker %s (STOR + readback cmp)" % marker_remote,
                   "curl rc 0 + byte-exact",
                   "STOR rc %s, readback rc %s, cmp %s" % (
                       up["rc"], rbr["rc"], "identical" if same else "DIFFERENT"),
                   same, "" if same else "dir exists; marker is cosmetic")
    return mods


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--console", default="ps5-host-not-configured.invalid")
    ap.add_argument("--out-dir", default=os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
        "evidence", "writable-" + datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")))
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)

    class _Ledger:
        def __init__(self):
            self.rows = []

        def add(self, step, check, expected, actual, ok, note=""):
            self.rows.append({"step": step, "check": check, "expected": expected,
                              "actual": actual, "status": "PASS" if ok else "FAIL",
                              "note": note})
            print("[%s] %s: %s (expected %s) %s"
                  % ("PASS" if ok else "FAIL", check, actual, expected,
                     ("-- " + note) if note else ""))

    os.makedirs(args.out_dir, exist_ok=True)
    ledger = _Ledger()
    mods = ensure_writable_dirs(args.console, args.out_dir, ledger,
                                dry_run=args.dry_run)
    print("mods: %s" % (mods if mods else "(none parsed)"))
    # [gate law 2026-10-12] the exit code reflects the result: it used to be 0
    # even with FAIL rows or zero mods parsed.
    failed = [r for r in ledger.rows if r["status"] == "FAIL"]
    print("checked %d rows, %d failed" % (len(ledger.rows), len(failed)))
    if not mods:
        print("FAIL: zero mods parsed")
        return 1
    if failed:
        print("FAIL: %d step(s) failed" % len(failed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
