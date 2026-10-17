#!/usr/bin/env python3
"""audit_probe_controls.py -- positive control for the dry-run guards (Mac-only).

Run by run_verify_and_dryrun.py as:
  sandbox-exec -p <no-network,no-write profile> python3 -B -I audit_dryrun_wrapper.py audit_probe_controls.py <tmp file>

It deliberately TRIES the two things the dry run must never do, so the runner
can prove both guards actually bite:
  1. open(<tmp file>, "w")             -> audit hook must record a write-open;
                                          the sandbox must refuse it (PermissionError)
  2. TCP connect to 127.0.0.1 port 9   -> audit hook must record socket events;
                                          the sandbox must refuse it (PermissionError).
     Loopback only: nothing can leave the Mac, and nothing goes to the console.
Prints one RESULT line per attempt. Always exits 0 (the wrapper decides).
"""
import socket
import sys


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else ""
    try:
        with open(target, "w") as f:
            f.write("probe\n")
        print("PROBE write: SUCCEEDED (sandbox did NOT block the write)")
    except Exception as e:  # noqa: BLE001 -- report the exact class
        print("PROBE write: %s %s" % (type(e).__name__, e))
    s = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect(("127.0.0.1", 9))
        print("PROBE connect: SUCCEEDED (sandbox did NOT block the connect)")
    except Exception as e:  # noqa: BLE001
        print("PROBE connect: %s %s" % (type(e).__name__, e))
    finally:
        if s is not None:
            s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
