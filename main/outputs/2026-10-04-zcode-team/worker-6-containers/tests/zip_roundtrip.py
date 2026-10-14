#!/usr/bin/env python3
"""Bounded ZIP roundtrip verification against saved ORIGINAL Nexus mod archives.

Read-only on the originals. Extract -> repack (store or deflate per original
member method) -> per-member sha256/size/CRC compare. Host-side integrity
evidence only: it does not re-run the native C zip intake, and container
readability is NOT mod execution.

Writes one JSON receipt to evidence/. Scratch tree stays under this worker dir.
"""
import hashlib, json, os, sys, tempfile, zipfile, datetime

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
OUT = os.path.join(WS, "outputs/2026-10-04-zcode-team/worker-6-containers")
SCRATCH = os.path.join(OUT, "tests/scratch-zip")
TARGETS = [
    "nexus-real-mods-2026-10-01/infinite-stamina-v1.zip",
    "nexus-real-mods-2026-10-01/mod-menu-v6.zip",
    "nexus-real-mods-2026-10-01/minimap-v3.1.zip",
    "nexus-real-mods-2026-10-01/ue4ss-v1.zip",
    "nexus-real-mods-2026-10-01/leon-2k-v1.1.zip",
    "item-spawner-real-mod-intake-1/original.zip",
    "movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip",
    "performance-pak-real-mod-intake-1/original.zip",
]

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def main():
    results = []
    for rel in TARGETS:
        src = os.path.join(WS, ".superpowers/native-observations", rel)
        r = {"target": rel, "present": os.path.exists(src)}
        if not r["present"]:
            results.append(r); continue
        r["bytes"] = os.path.getsize(src)
        r["sha256"] = sha256(src)
        try:
            with zipfile.ZipFile(src) as z:
                bad = z.testzip()  # CRC verify every member
                r["crc_testzip_bad_member"] = bad
                infos = z.infolist()
                r["members"] = len(infos)
                r["methods"] = sorted({i.compress_type for i in infos})
                flags = set()
                for i in infos:
                    if i.flag_bits & 0x1: flags.add("encrypted")
                    if i.flag_bits & 0x8: flags.add("data_descriptor")
                    if i.flag_bits & 0x800: flags.add("utf8_names")
                    if i.file_size > 0xFFFFFFFF or i.compress_size > 0xFFFFFFFF: flags.add("zip64")
                r["flags_detected"] = sorted(flags)
                names = [i.filename for i in infos]
                r["unsafe_names"] = [n for n in names if n.startswith("/") or ".." in n.split("/") or "\\" in n or ":" in n]
                # extract to scratch
                ext = os.path.join(SCRATCH, hashlib.sha256(rel.encode()).hexdigest()[:12])
                with tempfile.TemporaryDirectory(dir=SCRATCH) as td:
                    z.extractall(td)
                    # repack per-member with the original method
                    rp = os.path.join(td, "repacked.zip")
                    with zipfile.ZipFile(rp, "w") as w:
                        for i in infos:
                            nf = zipfile.ZipInfo(i.filename, date_time=i.date_time)
                            nf.compress_type = i.compress_type
                            nf.external_attr = i.external_attr
                            if i.filename.endswith("/"):
                                w.writestr(nf, b"")  # directory entry
                                continue
                            with open(os.path.join(td, i.filename), "rb") as fh:
                                data = fh.read()
                            w.writestr(nf, data)
                    # per-member roundtrip compare
                    mismatches = []
                    with zipfile.ZipFile(rp) as w:
                        if w.testzip() is not None:
                            mismatches.append("repacked-crc-failure")
                        for i in infos:
                            try:
                                a = z.read(i.filename); b = w.read(i.filename)
                            except Exception as e:
                                mismatches.append(f"{i.filename}: {e}"); continue
                            if hashlib.sha256(a).hexdigest() != hashlib.sha256(b).hexdigest() or len(a) != len(b):
                                mismatches.append(i.filename)
                    r["member_roundtrip_mismatches"] = mismatches
                    r["roundtrip_pass"] = (not mismatches) and bad is None and not r["unsafe_names"]
        except Exception as e:
            r["error"] = f"{type(e).__name__}: {e}"
            r["roundtrip_pass"] = False
        results.append(r)
    receipt = {
        "utc": datetime.datetime.utcnow().isoformat() + "Z",
        "scope": "host-only zip integrity + member-level roundtrip on saved original archives; originals read-only; NOT native intake evidence, NOT mod execution",
        "results": results,
        "summary": {
            "targets": len(results),
            "present": sum(1 for x in results if x["present"]),
            "roundtrip_pass": sum(1 for x in results if x.get("roundtrip_pass")),
        },
    }
    with open(os.path.join(OUT, "evidence/zip-roundtrip.json"), "w") as f:
        json.dump(receipt, f, indent=1)
    print(json.dumps(receipt["summary"], indent=1))
    for x in results:
        print(("PASS " if x.get("roundtrip_pass") else "FAIL "), x["target"],
              x.get("members", ""), "members methods=", x.get("methods", ""),
              x.get("error", ""))

if __name__ == "__main__":
    os.makedirs(SCRATCH, exist_ok=True)
    main()
