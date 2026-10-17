#!/usr/bin/env python3
"""make_gw19_fixture.py — build the test-19 MODIFIED-Unreal-build fixture.

Takes the ONLY real Unreal sample on this Mac (the Townfall decrypted eboot,
UE 5.6.1 shipping — the positive-control image for tests 3-7) and produces a
tampered copy simulating a MODIFIED Unreal build (licensee fork / patched
binary shape):

  * CORRUPT the first 12 present markers in marker-table order: one byte
    (offset +6 inside the utf-16le encoding) XOR 0x01 in EVERY PT_LOAD
    occurrence — engine strings renamed/changed by the modification.
  * SPLICE the first 6 old-version-only foreign markers (table order, absent
    in the base image, gate compatible with a shipping-nostats candidate,
    last version <= 4.20) into the largest all-zero run inside the LAST
    PT_LOAD file slice — code backported/merged from an old branch carrying
    old-version strings.

Everything is deterministic (no RNG): same input eboot => byte-identical
fixture. Output: townfall-modified-build.elf + tamper-manifest.json (+ .sha256
sidecars). Idempotent: re-running verifies the existing fixture instead of
rewriting it.

Mac-safe: local file copy + byte edits only. Read-only on the source eboot.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PHASEA = HERE.parent
WHAT = PHASEA.parent.parent.parent
sys.path.insert(0, str(PHASEA))
import engine_match as em  # noqa: E402

SOURCE = WHAT / ".superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
OUT_ELF = HERE / "townfall-modified-build.elf"
OUT_MANIFEST = HERE / "tamper-manifest.json"

CORRUPT_COUNT = 12
SPLICE_COUNT = 6
SPLICE_GATE_OK = ("", "TestOrShipping", "ShippingOnly")
FLIP_AT = 6          # byte offset inside the utf-16le needle
FLIP_XOR = 0x01
FOREIGN_LAST_MAJOR_MAX = (4, 20)  # last version index comparison bound


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    vers, marks = em.load_markers(em.DEFAULT_MARKERS)
    vindex = {v: i for i, v in enumerate(vers)}
    img = em.parse_image(SOURCE)

    needles = [m["string"].encode("utf-16-le") for m in marks]

    # ---- collect occurrences of every marker inside PT_LOAD file slices ----
    occ = [[] for _ in marks]  # list of file offsets
    zero_runs_best = (0, 0)   # (length, file offset) inside a NON-TEXT load
    with open(SOURCE, "rb") as f:
        for li, L in enumerate(img["loads"]):
            f.seek(L["off"])
            seg = f.read(L["filesz"])
            for i, nd in enumerate(needles):
                start = 0
                while True:
                    j = seg.find(nd, start)
                    if j == -1:
                        break
                    occ[i].append(L["off"] + j)
                    start = j + 1
            if li >= 1:  # skip LOAD0 (text): splice into data/rodata slack
                z = seg
                pos = 0
                while pos < len(z):
                    if z[pos] == 0:
                        run_begin = pos
                        while pos < len(z) and z[pos] == 0:
                            pos += 1
                        run_len = pos - run_begin
                        if run_len > zero_runs_best[0]:
                            zero_runs_best = (run_len, L["off"] + run_begin)
                    else:
                        pos += 1
    present_idx = [i for i, o in enumerate(occ) if o]

    # ---- deterministic picks ------------------------------------------------
    corrupt_idx = present_idx[:CORRUPT_COUNT]
    i420 = vindex[FOREIGN_LAST_MAJOR_MAX]
    foreign_idx = []
    for i in range(len(marks)):
        if len(foreign_idx) >= SPLICE_COUNT:
            break
        m = marks[i]
        if occ[i] or m["gate"] not in SPLICE_GATE_OK:
            continue
        if m["last_idx"] == 0xFF or m["last_idx"] > i420:
            continue
        if len(m["string"].encode("utf-16-le")) < 12:
            continue
        foreign_idx.append(i)
    if len(corrupt_idx) != CORRUPT_COUNT or len(foreign_idx) != SPLICE_COUNT:
        print("fixture pick failed: corrupt=%d foreign=%d" %
              (len(corrupt_idx), len(foreign_idx)), file=sys.stderr)
        return 2

    splice_len = sum(len(needles[i]) + 2 for i in foreign_idx)
    run_len, run_off = zero_runs_best
    if run_len < splice_len + 16:
        print("no zero run big enough: %d < %d" % (run_len, splice_len),
              file=sys.stderr)
        return 2

    # ---- idempotence: verify existing fixture instead of rewriting ---------
    if OUT_ELF.exists() and OUT_MANIFEST.exists():
        man = json.loads(OUT_MANIFEST.read_text())
        actual = sha256_file(OUT_ELF)
        if (man.get("fixture_sha256") == actual
                and man.get("source_sha256") == img["sha256"]):
            print("fixture already present and verified: %s" % actual)
            return 0
        print("stale fixture (sha mismatch) — rebuilding", file=sys.stderr)

    # ---- build --------------------------------------------------------------
    import shutil
    tmp = OUT_ELF.with_suffix(".elf.partial")
    shutil.copyfile(SOURCE, tmp)
    edits = {}  # file offset -> new byte
    corrupted = []
    for i in corrupt_idx:
        nd = needles[i]
        assert len(nd) > FLIP_AT + 1
        sites = []
        for off in occ[i]:
            edits[off + FLIP_AT] = None  # filled after reading current byte
            sites.append(off)
        corrupted.append({"marker_index": i,
                          "string": marks[i]["string"][:80],
                          "gate": marks[i]["gate"] or None,
                          "occurrences": sites})
    spliced = []
    with open(SOURCE, "rb") as fs:
        cur = run_off
        for i in foreign_idx:
            nd = needles[i]
            for k, b in enumerate(nd):
                edits[cur + k] = b
            edits[cur + len(nd)] = 0
            edits[cur + len(nd) + 1] = 0
            spliced.append({"marker_index": i,
                            "string": marks[i]["string"][:80],
                            "first": "%s.%s" % marks[i]["first"] if marks[i]["first"] else None,
                            "last": "%s.%s" % marks[i]["last"] if marks[i]["last"] else None,
                            "file_offset": cur})
            cur += len(nd) + 2
    with open(SOURCE, "rb") as fs, open(tmp, "r+b") as fd:
        for off in sorted(edits):
            if edits[off] is None:
                fs.seek(off)
                orig = fs.read(1)[0]
                fd.seek(off)
                fd.write(bytes([orig ^ FLIP_XOR]))
            else:
                fd.seek(off)
                fd.write(bytes([edits[off]]))
    tmp.rename(OUT_ELF)

    fixture_sha = sha256_file(OUT_ELF)
    manifest = {
        "fixture": "townfall-modified-build.elf",
        "purpose": "test 19: MODIFIED Unreal build must NOT classify VERSION_CONFIDENT (fails closed)",
        "source": str(SOURCE),
        "source_sha256": img["sha256"],
        "fixture_sha256": fixture_sha,
        "size": OUT_ELF.stat().st_size,
        "tamper": {
            "corrupt": {
                "rule": "first %d present markers in marker-table order; byte at needle+%d XOR 0x%02x in EVERY PT_LOAD occurrence"
                        % (CORRUPT_COUNT, FLIP_AT, FLIP_XOR),
                "count": len(corrupted),
                "markers": corrupted,
            },
            "splice": {
                "rule": "first %d absent markers in table order with last<=%s and gate in %s, written utf-16le+NUL into the largest all-zero run of any non-text PT_LOAD"
                        % (SPLICE_COUNT, "%s.%s" % FOREIGN_LAST_MAJOR_MAX, list(SPLICE_GATE_OK)),
                "count": len(spliced),
                "slack_run": {"file_offset": run_off, "length": run_len},
                "markers": spliced,
            },
        },
        "markers_source": em.DEFAULT_MARKERS,
    }
    OUT_MANIFEST.write_text(json.dumps(manifest, indent=1) + "\n")
    print("built %s sha=%s size=%d" % (OUT_ELF.name, fixture_sha, manifest["size"]))
    print("corrupted %d markers (%d byte sites); spliced %d foreign markers at 0x%x"
          % (len(corrupted), sum(len(c["occurrences"]) for c in corrupted),
             len(spliced), run_off))
    return 0


if __name__ == "__main__":
    sys.exit(main())
