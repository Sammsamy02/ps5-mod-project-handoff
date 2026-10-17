#!/usr/bin/env python3
"""rearm_ploader.py -- re-arm the ploader after its 120 s injector timeout expired, WITHOUT re-uploading anything.

Use when prep_modded_launch.py already ran (uploads verified) but the game was not started inside the ploader's
120 s window. Fresh uploads right before an arm caused two console drops on 2026-10-13, so this path re-checks the
console state read-only and dispatches the arm once:

  1. preconditions (prep step 1): daemons up, no eboot.bin, ploader not resident
  2. resident PRX decrypt-OFF readback: magic + FULL sha256 + size == --prx-path (no upload)
  3. UE4SS-settings.ini and ploader.ini readback: byte-identical to the pinned backups (no upload)
  4. loadlist row-check (prep step 5.5)
  5. klog tap: reuse the live tap for --out-dir, else start tap + eboot monitor (prep step 7)
  6. ONE loadpayload dispatch + fresh payload.elf pid (prep step 6)

Any failed check aborts before the arm. Writes REARM-<ts>.md (the ledger) into --out-dir.
"""
import argparse
import filecmp
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prep_modded_launch as P  # noqa: E402


def klog_tap_alive(out_dir):
    """True if a klog_capture.py process is already writing into out_dir."""
    r = subprocess.run(["pgrep", "-fl", "klog_capture.py"], capture_output=True, text=True)
    want = os.path.normpath(out_dir)
    for ln in (r.stdout or "").splitlines():
        parts = ln.split()
        if "--out-dir" in parts:
            i = parts.index("--out-dir")
            if i + 1 < len(parts) and os.path.normpath(parts[i + 1]) in (want, os.path.relpath(want, P._PREP_ROOT)):
                return parts[0]
    return None


def readback_ini(console_ip, out_dir, local, remote, label, ledger):
    dest = os.path.join(out_dir, os.path.basename(remote) + ".rearm-readback")
    r = P.run_cmd(P.ftp_get_cmd(console_ip, remote, dest, P.CURL_INI_MAX), 60)
    same = r["rc"] == 0 and os.path.isfile(dest) and filecmp.cmp(local, dest, shallow=False)
    ledger.add("3", "%s readback == pinned backup (no upload)" % label, "byte-identical",
               "identical" if same else "rc %s / DIFFERENT" % r["rc"], same)
    if not same:
        raise P.Abort("STEP 3 FAILED: console %s differs from the pinned backup. Run the full "
                      "prep_modded_launch.py instead (it restores and verifies it)." % label)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--prx-path", required=True, help="the PRX that prep uploaded (its sha is the expected resident)")
    ap.add_argument("--out-dir", required=True, help="the launch's evidence out-dir (same as the prep run)")
    ap.add_argument("--console-ip", default=P.DEFAULT_CONSOLE_IP)
    args = ap.parse_args(argv)

    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)
    ledger = P.Ledger()
    abort = None
    try:
        procs = P.step1_preconditions(args.console_ip, out_dir, ledger)
        prx_size, prx_sha, _ = P.step2_prx_gate(args.prx_path, ledger)

        rb = os.path.join(out_dir, "prx-readback-rearm.decrypt-off.prx")
        r = P.run_cmd(P.ftp_get_cmd(args.console_ip, P.PRX_REMOTE, rb, P.CURL_PRX_RB_MAX, self_mode=True,
                                    max_filesize=P.FTP_MAX_FILESIZE), 180)
        got = P.sha256_of(rb) if r["rc"] == 0 and os.path.isfile(rb) else None
        size = os.path.getsize(rb) if got else -1
        P.verify_readback_prx(rb, "resident PRX", ledger, "2")
        ok = got == prx_sha and size == prx_size and P.magic_of(rb) in (m.hex() for m in P.MAGIC_SIGNED)
        ledger.add("2", "resident PRX readback (decrypt-OFF, no upload)", "%s / %d bytes" % (prx_sha, prx_size),
                   "%s / %d bytes" % (got, size), ok)
        if not ok:
            raise P.Abort("STEP 2 FAILED: the resident PRX is not the expected build. Run the full prep instead.")

        readback_ini(args.console_ip, out_dir, P.SETTINGS_LOCAL, P.SETTINGS_REMOTE, "UE4SS-settings.ini", ledger)
        readback_ini(args.console_ip, out_dir, P.PLOADER_INI_LOCAL, P.PLOADER_INI_REMOTE, "ploader.ini", ledger)

        P.step5_5_rowcheck(args.console_ip, out_dir, ledger)

        tap = klog_tap_alive(out_dir)
        if tap:
            ledger.add("7", "klog tap already running for this out-dir", "alive", "pid %s (reused)" % tap, True)
        else:
            P.step7_background(args.console_ip, out_dir, "A", ledger, False)

        fresh_pid, _, _ = P.step6_arm_ploader(args.console_ip, out_dir, procs, ledger)
    except P.Abort as e:
        abort = str(e)
    except Exception as e:  # same contract as prep: unexpected = failed verification
        abort = "UNEXPECTED ERROR (%s: %s)" % (type(e).__name__, e)

    checked = len(ledger.rows)
    lines = ["# REARM %s" % P.now_utc(), "", "- PRX: %s" % args.prx_path, "- out-dir: %s" % out_dir,
             "- checks run: %d" % checked, "", "| step | check | expected | actual | status |", "|---|---|---|---|---|"]
    lines += ["| %s | %s | %s | %s | %s |" % (x["step"], x["check"], x["expected"], x["actual"], x["status"])
              for x in ledger.rows]
    lines += ["", "ABORT: %s" % abort if abort else
              "ARMED: ploader pid %s. Launch the game within %d s." % (fresh_pid, P.PLOADER_TIMEOUT_S)]
    path = os.path.join(out_dir, "REARM-%s.md" % P.ts_slug())
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines[-1:]))
    print("[rearm] %d checks; ledger %s" % (checked, path))
    if checked == 0:
        print("[rearm] FAIL: zero checks ran", file=sys.stderr)
        return 1
    return 1 if abort else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
