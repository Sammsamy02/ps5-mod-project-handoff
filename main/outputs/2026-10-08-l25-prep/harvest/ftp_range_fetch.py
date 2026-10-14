#!/usr/bin/env python3
"""ftp_range_fetch.py -- READ-ONLY byte-range fetch from the console over FTP (REST + RETR).

Usage: ftp_range_fetch.py --plan <fetch-plan.json> --host <LAN ip> --port 2121
                          --remote-dir <paks dir> --out <dir> [--max-priority N]

The plan is the #8 intake's fetch plan (chunks[] with ucas, physical_ranges[{offset,length}],
save_as) or a #12 plan (rows / fetch_plan / container_header_plan with remote_file, pieces, local_name). Each range is read on a fresh connection (REST offset, RETR, stop after
`length` bytes), so no write command is ever sent. Every chunk file must come out exactly
`length` bytes or the run fails. The intake re-checks each file against the base TOC IoHash.
Prints how many chunks/bytes it fetched and exits non-zero on zero chunks or any short read.
"""
import argparse
import ftplib
import ipaddress
import json
import os
import sys


def read_range(host, port, path, offset, length):
    ftp = ftplib.FTP()
    ftp.connect(host, port, timeout=60)
    try:
        ftp.login()
        ftp.set_pasv(True)
        ftp.voidcmd("TYPE I")
        conn = ftp.transfercmd("RETR " + path, rest=offset)
        buf = bytearray()
        try:
            while len(buf) < length:
                b = conn.recv(min(1 << 20, length - len(buf)))
                if not b:
                    break
                buf += b
        finally:
            conn.close()
        try:
            ftp.voidresp()
        except ftplib.all_errors:
            pass  # 426/451 after an early close is expected
        return bytes(buf)
    finally:
        try:
            ftp.close()
        except Exception:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--plan", required=True)
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=2121)
    ap.add_argument("--remote-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-priority", type=int, default=None)
    a = ap.parse_args()
    addr = ipaddress.IPv4Address(a.host)
    if not (addr.is_private or addr.is_loopback):
        sys.exit("refused: host must be a LAN address")
    plan = json.load(open(a.plan))
    if "chunks" in plan:
        chunks = plan["chunks"]
    else:  # #12 shape: rows / fetch_plan / container_header_plan with remote_file, pieces, local_name
        rows = []
        for k in ("rows", "fetch_plan", "container_header_plan"):
            rows += plan.get(k) or []
        chunks = [{"ucas": r["remote_file"], "save_as": r["local_name"], "priority": r.get("priority", 1),
                   "physical_ranges": [{"offset": o, "length": n} for o, n in r["pieces"]]}
                  for r in rows]
    if a.max_priority is not None:
        chunks = [c for c in chunks if c.get("priority", 1) <= a.max_priority]
    n = total = 0
    bad = []
    for c in chunks:
        dst = os.path.join(a.out, c["save_as"])
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        path = a.remote_dir.rstrip("/") + "/" + c["ucas"]
        data = b"".join(read_range(a.host, a.port, path, r["offset"], r["length"])
                        for r in c["physical_ranges"])
        want = sum(r["length"] for r in c["physical_ranges"])
        if len(data) != want:
            bad.append((c["save_as"], len(data), want))
            continue
        with open(dst, "wb") as f:
            f.write(data)
        n += 1
        total += len(data)
    print("FETCH: %d/%d chunks, %d bytes, %d short reads" % (n, len(chunks), total, len(bad)))
    for b in bad:
        print("SHORT %s got %d want %d" % b)
    return 1 if n == 0 or bad else 0


if __name__ == "__main__":
    sys.exit(main())
