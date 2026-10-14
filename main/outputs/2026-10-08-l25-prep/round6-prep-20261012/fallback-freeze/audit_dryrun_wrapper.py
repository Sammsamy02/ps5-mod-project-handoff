#!/usr/bin/env python3
"""audit_dryrun_wrapper.py -- run a Python script (here: harvest/prep_modded_launch.py
--dry-run) under a CPython audit hook and report every attempt it makes to
write a file, change the filesystem, open a network socket or start a process.

Usage: python3 -B -I audit_dryrun_wrapper.py <script.py> [script args...]

Mac-only. The target script is executed unchanged (runpy.run_path, run_name
"__main__"); it is never modified. The wrapper itself writes nothing: the audit
report goes to stderr. Exit code: the script's own exit code when zero
violations were seen; 3 when any violation was seen; 4 when zero audit events
were observed at all (a hook that saw nothing is a failed check, not a pass).

What counts as a violation (events recorded BEFORE the operation runs, so an
attempt is seen even if a sandbox would then refuse it):
  - open() / os.open with a write, append, create, truncate or exclusive mode
  - socket.* (connect, bind, getaddrinfo, sendto, ...), http/ftp/urllib/smtp
  - subprocess.Popen, os.system, os.exec*, os.spawn*, os.posix_spawn*, os.fork
  - os.remove/unlink/rename/replace/mkdir/makedirs/rmdir/chmod/chown/truncate/
    link/symlink/utime, shutil.* copy/move/rmtree
Limits: C extensions that bypass the audit API are not seen; the sandbox-exec
wrapper used by the runner (network + file writes denied) covers that layer.
"""
import os
import runpy
import sys

WRITE_FLAGS = os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_APPEND | os.O_TRUNC | getattr(os, "O_EXCL", 0)
NET_PREFIXES = ("socket.", "http.client.", "ftplib.", "smtplib.", "urllib.", "poplib.", "imaplib.", "nntplib.", "telnetlib.")
PROC_EVENTS = ("subprocess.Popen", "os.system", "os.exec", "os.spawn", "os.posix_spawn", "os.fork", "os.forkpty",
               "os.kill", "os.killpg", "pty.spawn")
FS_EVENTS = ("os.remove", "os.unlink", "os.rename", "os.replace", "os.mkdir", "os.rmdir", "os.chmod", "os.chown",
             "os.chflags", "os.lchflags", "os.truncate", "os.link", "os.symlink", "os.utime", "os.removexattr",
             "os.setxattr", "shutil.copyfile", "shutil.copymode", "shutil.copystat", "shutil.copytree",
             "shutil.move", "shutil.rmtree", "shutil.make_archive", "shutil.unpack_archive")

seen = {"events": 0, "opens_read": 0}
violations = []


def is_write_open(args):
    mode = args[1] if len(args) > 1 else None
    flags = args[2] if len(args) > 2 else 0
    if isinstance(mode, str) and any(c in mode for c in "wax+"):
        return True
    if isinstance(flags, int) and flags & WRITE_FLAGS:
        return True
    return False


def hook(event, args):
    seen["events"] += 1
    if event == "open":
        if is_write_open(args):
            violations.append(("write-open", repr(args[:3])))
        else:
            seen["opens_read"] += 1
        return
    if event.startswith(NET_PREFIXES):
        violations.append(("network", event))
    elif event.startswith(PROC_EVENTS):
        violations.append(("process", event))
    elif event.startswith(FS_EVENTS):
        violations.append(("fs-change", "%s %r" % (event, args[:2])))


def main():
    if len(sys.argv) < 2:
        print("usage: audit_dryrun_wrapper.py <script.py> [args...]", file=sys.stderr)
        return 2
    target = sys.argv[1]
    sys.argv = [target] + sys.argv[2:]
    sys.addaudithook(hook)
    rc = 0
    try:
        runpy.run_path(target, run_name="__main__")
    except SystemExit as e:
        rc = e.code if isinstance(e.code, int) else (0 if e.code is None else 1)
    sys.stdout.flush()
    print("# --- audit (Mac-only, CPython audit hook) ---", file=sys.stderr)
    print("AUDIT events_observed=%d read_opens=%d violations=%d script_exit=%d"
          % (seen["events"], seen["opens_read"], len(violations), rc), file=sys.stderr)
    for kind, what in violations[:50]:
        print("AUDIT VIOLATION %s: %s" % (kind, what), file=sys.stderr)
    if seen["events"] == 0:
        print("AUDIT FAIL: zero audit events observed", file=sys.stderr)
        return 4
    if violations:
        return 3
    print("AUDIT RESULT: no write-open, network, process or fs-change attempt", file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())
