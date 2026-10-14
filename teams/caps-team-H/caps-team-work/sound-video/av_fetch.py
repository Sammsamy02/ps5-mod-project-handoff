#!/usr/bin/env python3
"""av_fetch.py -- READ-ONLY fetch plans for the game's own sound/video files (Mac side).

The integrator runs the two console steps; this tool only writes plans and
checks what came back.  Nothing here can write to the console: the lister
sends USER/PASS (anonymous), TYPE I, PASV, LIST and CWD only, and every byte read
goes through the integrator's harvest/ftp_range_fetch.py (REST + RETR, fresh
connection per range).  Run all console steps with the GAME CLOSED (FTP reads
under game load come back incomplete).

  packages --profile P --mirror DIR --out plan.json
        fetch plan (ftp_range_fetch "chunks" shape, remote dir = the paks dir)
        for the small media/bank packages of the game: whole export chunks
        (+ two native SoundWave bulk chunks) with the TOC IoHash for each.
  list  --host IP --port 2121 --root CONTENT_DIR --subdirs fmod movies --out listing.json
        [integrator, console] recursive read-only LIST below the content dir.
  heads --listing L --profile P --out plan.json
        fetch plan (remote dir = the content dir) for the first bytes of every
        audio bank / movie file in the listing.
  hops  --plans heads-plan.json [hops1.json ...] --dir FETCHED --out hopsN.json
        next plan: the bank chunk headers up to the sample-data chunk / the mp4
        'moov' box when they lie beyond what was fetched (repeat until empty;
        normally one round).
  check --listing L --dir FETCHED --plans P1 [P2 ...] --profile P --out base-media.json
        parse every fetched piece, verify packages by IoHash (BLAKE3-20, second
        source = the game's TOC), and write the base media profile the
        installer gates on.  Fails (exit 1) when zero items were checked.
"""
from __future__ import annotations

import argparse
import ftplib
import hashlib
import ipaddress
import json
import os
import posixpath
import re
import sys
from typing import Callable, Dict, List, Optional, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import av_formats as F  # noqa: E402
import av_profile  # noqa: E402

HEAD_BYTES = {".bank": 65536, ".bk2": 4096, ".bik": 4096, ".mp4": 65536, ".m4v": 65536, ".bnk": 4096, ".wem": 4096}
HOP_BYTES = 65536
MAX_DEPTH = 10
MAX_ENTRIES = 50000
LIST_RE = re.compile(r"([-dl][rwxstST-]{9})\s+\d+\s+\S+\s+\S+\s+(\d+)\s+\S+\s+\d+\s+\S+\s+(.+)")


class FetchError(RuntimeError):
    pass


def need(ok, why):
    if not ok:
        raise FetchError(why)


def jdump(obj, path):
    with open(path, "w") as f:
        json.dump(obj, f, indent=1, sort_keys=True)


def jload(path):
    with open(path) as f:
        return json.load(f)


def safe_rel(rel: str) -> str:
    parts = rel.split("/")
    need(rel and not rel.startswith("/") and all(p not in ("", ".", "..") for p in parts), "bad relative path %r" % rel)
    return rel


# ---------------------------------------------------------------------------
# packages plan (paks dir) -- fully Mac-side, from the game's own TOC
# ---------------------------------------------------------------------------

MEDIA_CLASSES = ("BinkMediaPlayer.BinkMediaPlayer", "MediaAssets.FileMediaSource", "MediaAssets.MediaPlayer",
                 "FMODStudio.FMODBank", "FMODStudio.FMODBankLookup")


def packages_plan(profile: dict, mirror: str, max_chunk: int = 65536) -> dict:
    sys.path.insert(0, av_profile.MF)
    import utoc  # noqa: E402
    project = profile["project"]
    wanted = []
    fm = profile["audio"]["fmod"] or {}
    wanted += [b["package"] for b in fm.get("banks", [])]
    wanted += fm.get("bank_lookup_assets", [])
    wanted += profile["video"]["bink"]["players"]
    wanted += [r["package"] for r in profile["video"]["electra"]["file_media_sources"]]
    index = {}
    for f in sorted(os.listdir(mirror)):
        if f.endswith(".utoc"):
            t = utoc.load(os.path.join(mirror, f))
            for row, name in t.files.items():
                index[name.replace("../../../", "")] = (f, t, row)
    chunks, missing, too_big = [], [], []

    def add(rel: str, why: str, prio: int):
        hit = index.get(rel)
        if not hit:
            missing.append(rel)
            return
        f, t, row = hit
        off, length = t.offlen[row]
        if length > max_chunk:
            too_big.append(rel)
            return
        chunks.append({"ucas": f[:-5] + ".ucas", "priority": prio, "why": why,
                       "save_as": "packages/" + rel.replace("/", "__"), "toc": f, "toc_row": row,
                       "chunk_id": t.chunk_ids[row].hex(), "length": length,
                       "io_hash20": t.metas[row][0], "package_path": rel,
                       "physical_ranges": [{"offset": o, "length": n} for o, n in t.physical_ranges(row)]})
    for pkg in sorted(set(wanted)):
        need(pkg.startswith("/Game/"), "unexpected package root %s" % pkg)
        add(pkg.replace("/Game/", project + "/Content/", 1) + ".uasset", "media/bank package: player URL, bank lookup", 2)
    # two native SoundWaves (export + bulk): which audio format the engine cooked for this platform
    sw = sorted(p for p in index if p.endswith(".ubulk") and "/Sound" in p)[:2]
    for b in sw:
        add(b[:-6] + ".uasset", "native SoundWave export: cooked format container", 3)
        add(b, "native SoundWave bulk: cooked audio bytes header", 3)
    return {"tool": "av_fetch packages/1", "read_only": True, "remote_dir": "<paks dir>",
            "chunks": chunks, "missing": missing, "skipped_too_big": too_big,
            "bytes_total": sum(c["length"] for c in chunks)}


# ---------------------------------------------------------------------------
# read-only lister (integrator runs this one against the console)
# ---------------------------------------------------------------------------

def parse_listing(text: str) -> Dict[str, Tuple[str, int]]:
    out = {}
    for line in text.splitlines():
        line = line.rstrip("\r")
        if not line.strip() or line.startswith("total "):
            continue
        m = LIST_RE.fullmatch(line)
        need(m is not None, "unparsed LIST line: %r" % line[:120])
        name = m[3].split(" -> ", 1)[0] if m[1][0] == "l" else m[3]
        if name in (".", ".."):
            continue
        need("/" not in name and name not in out, "ambiguous LIST name %r" % name)
        out[name] = (m[1][0], int(m[2]))
    return out


def walk(list_dir: Callable[[str], str], root: str, subdirs: List[str]) -> dict:
    """list_dir(abs_path) -> raw LIST text. Root itself: one level; then the
    named subdirs (matched case-insensitively) recursively."""
    need(root.startswith("/") and root.strip("/"), "root must be an absolute folder below /: %r" % root)
    top = parse_listing(list_dir(root))
    entries = {name: list(v) for name, v in top.items()}
    want = {s.lower() for s in subdirs}
    todo = [(name, 1) for name, (k, _) in top.items() if k == "d" and name.lower() in want]
    found = {n.lower() for n, _ in todo}
    while todo:
        rel, depth = todo.pop()
        rows = parse_listing(list_dir(posixpath.join(root, rel)))
        for name, (kind, size) in sorted(rows.items()):
            path = rel + "/" + name
            need(kind != "l", "link below a listed folder: %s" % path)
            entries[path] = [kind, size]
            if kind == "d":
                need(depth + 1 <= MAX_DEPTH, "tree too deep at %s" % path)
                todo.append((path, depth + 1))
        need(len(entries) <= MAX_ENTRIES, "tree too large")
    return {"schema": "av-listing/1", "root": root, "subdirs_requested": sorted(want),
            "subdirs_found": sorted(found), "entries": entries,
            "files": sum(1 for v in entries.values() if v[0] == "-")}


def ftp_lister(host: str, port: int) -> Tuple[Callable[[str], str], Callable[[], None]]:
    addr = ipaddress.IPv4Address(host)
    need(addr.is_private or addr.is_loopback, "host must be a LAN address")
    ftp = ftplib.FTP()
    ftp.encoding = "utf-8"
    ftp.connect(host, port, timeout=30)
    ftp.login()
    ftp.set_pasv(True)
    ftp.voidcmd("TYPE I")

    def list_dir(path: str) -> str:
        # Read-only command set: LIST, plus CWD as a fallback for servers that
        # reject a path argument with spaces.  Never RETR/STOR/DELE/RNFR/SITE.
        buf = bytearray()
        try:
            ftp.retrbinary("LIST " + path, buf.extend)
        except ftplib.error_perm:
            buf = bytearray()
            ftp.cwd(path)
            ftp.retrbinary("LIST", buf.extend)
        need(len(buf) <= 1 << 22, "listing too large")
        return bytes(buf).decode("utf-8", "strict")

    def close():
        try:
            ftp.quit()
        except Exception:
            ftp.close()
    return list_dir, close


# ---------------------------------------------------------------------------
# heads / hops plans (content dir)
# ---------------------------------------------------------------------------

def heads_plan(listing: dict) -> dict:
    chunks = []
    for rel, (kind, size) in sorted(listing["entries"].items()):
        ext = os.path.splitext(rel)[1].lower()
        if kind != "-" or ext not in HEAD_BYTES or size == 0:
            continue
        n = min(size, HEAD_BYTES[ext])
        chunks.append({"ucas": safe_rel(rel), "priority": 1, "why": "head of %s" % ext,
                       "save_as": "heads/" + rel, "length": n, "file_bytes": size,
                       "physical_ranges": [{"offset": 0, "length": n}]})
    return {"tool": "av_fetch heads/1", "read_only": True, "remote_dir": listing["root"],
            "chunks": chunks, "bytes_total": sum(c["length"] for c in chunks)}


MAX_MOOV = 8 << 20


def views(plans: List[dict], fetched: str) -> Tuple[Dict[str, dict], List[Tuple[dict, str]]]:
    """Content-file pieces -> sparse views {rel: {file_bytes, view, ranges, missing}};
    package rows (they carry io_hash20) are returned separately."""
    by_file: Dict[str, dict] = {}
    pk_rows = []
    for plan in plans:
        for c in plan["chunks"]:
            p = os.path.join(fetched, c["save_as"])
            if "io_hash20" in c:
                pk_rows.append((c, p))
                continue
            rel = safe_rel(c["ucas"])
            need(len(c["physical_ranges"]) == 1, "content pieces must be one range each: %s" % c["save_as"])
            rec = by_file.setdefault(rel, {"file_bytes": c["file_bytes"], "pieces": {}, "missing": []})
            need(rec["file_bytes"] == c["file_bytes"], "plans disagree on the size of %s" % rel)
            at = c["physical_ranges"][0]["offset"]
            if not os.path.isfile(p):
                rec["missing"].append(at)
                continue
            data = open(p, "rb").read()
            need(len(data) == c["length"], "short piece %s" % c["save_as"])
            if at in rec["pieces"]:
                need(rec["pieces"][at] == data, "two pieces at %s@%d disagree" % (rel, at))
            rec["pieces"][at] = data
    for rel, rec in by_file.items():
        pieces = sorted(rec["pieces"].items())
        ranges = []
        view = bytearray()
        for at, data in pieces:
            if at < len(view):  # overlapping pieces must agree byte for byte
                k = min(len(data), len(view) - at)
                need(bytes(view[at:at + k]) == data[:k], "overlapping pieces disagree: %s@%d" % (rel, at))
                view[at:at + len(data)] = data
            else:
                view += bytes(at - len(view)) + data
            ranges.append((at, at + len(data)))
        rec["view"] = bytes(view)
        rec["ranges"] = ranges
        rec["head"] = rec["pieces"].get(0)
    return by_file, pk_rows


def have_fn(ranges):
    return lambda o, n: any(a <= o and o + n <= b for a, b in ranges)


def next_reads(rel: str, rec: dict) -> List[Tuple[int, int]]:
    """(offset, length) still needed to reach the facts the installer gates on."""
    ext = os.path.splitext(rel)[1].lower()
    size = rec["file_bytes"]
    have = have_fn(rec["ranges"])
    out = []
    if ext == ".bank":
        b = F.parse_fmod_bank(rec["view"], file_size=size, complete=False, have=have)
        for at in b.get("snd_payloads_missing", []):
            out.append((at, min(HOP_BYTES, size - at)))
        if not any(x["fsb"] for x in b["snd"]):
            for at in b.get("walk_stop_candidates", []):
                out.append((at, min(HOP_BYTES, size - at)))
    elif ext in (".mp4", ".m4v"):
        top = F.mp4_top_level(rec["view"], size, have)
        moov = [(p, n) for t, p, n in top["boxes"] if t == "moov"]
        if moov:
            p, n = moov[0]
            need(n <= MAX_MOOV, "%s: moov box of %d bytes is over the fetch cap" % (rel, n))
            out.append((p, n))
        elif top["stopped_at"] is not None:
            at = top["stopped_at"]
            out.append((at, min(1 << 20, size - at)))
    return [(a, n) for a, n in out if 0 <= a < size and n > 0 and not have(a, n)]


def hops_plan(plans: List[dict], fetched: str) -> dict:
    files, _ = views(plans, fetched)
    chunks = []
    remote = plans[0]["remote_dir"]
    for rel, rec in sorted(files.items()):
        if rec["head"] is None:
            continue
        try:
            reads = next_reads(rel, rec)
        except (F.FormatError, FetchError):
            continue  # 'check' reports it per file; no hop for this file
        for at, n in sorted(set(reads)):
            chunks.append({"ucas": rel, "priority": 1, "why": "hop at %d" % at,
                           "save_as": "hops/%s@%d+%d" % (rel, at, n), "length": n,
                           "file_bytes": rec["file_bytes"], "hop_offset": at,
                           "physical_ranges": [{"offset": at, "length": n}]})
    return {"tool": "av_fetch hops/2", "read_only": True, "remote_dir": remote,
            "chunks": chunks, "bytes_total": sum(c["length"] for c in chunks)}


# ---------------------------------------------------------------------------
# check -> base media profile
# ---------------------------------------------------------------------------

def _file_facts(rel: str, rec: dict, profile: dict) -> dict:
    view, have = rec["view"], have_fn(rec["ranges"])
    size = rec["file_bytes"]
    ext = os.path.splitext(rel)[1].lower()
    r = {"file_bytes": size, "head_sha256": hashlib.sha256(rec["head"]).hexdigest(),
         "head_bytes": len(rec["head"]), "sniff": F.sniff(rec["head"][:16])}
    if ext == ".bank":
        b = F.parse_fmod_bank(view, file_size=size, complete=False, have=have)
        walk_done = b["walk_stopped_at"] is None and not b.get("walk_stop_candidates")
        # a bank without samples (e.g. the master strings bank) is fine when the
        # chunk walk reached the RIFF end; otherwise every SND must be read
        need(all(x["fsb"] for x in b["snd"]) and (b["snd"] or walk_done),
             "sample-data header not reached (fetch another hop round)")
        r.update(kind="fmod_bank", fmt_version=b["fmt_version"], fmt_subversion=b["fmt_subversion"],
                 chunks=b["chunks"][:8], metadata_end=b["metadata_end"],
                 fsb=[{k: x["fsb"][k] for k in ("version", "codec", "codec_id", "samples", "rates",
                                                "channels", "vorbis_setup_crc32", "sample_rows_parsed")}
                      for x in b["snd"]])
        # the GUID check needs only the part of the metadata that holds the id:
        # search the whole metadata when fetched, else the contiguous head
        avail = b["metadata_end"] if have(0, b["metadata_end"]) else min(len(rec["head"]), b["metadata_end"])
        r["guid_hits"] = _guid_hits(view[:avail], profile, rel)
        r["guid_search_bytes"] = avail
        need(not r["guid_hits"]["bank_asset"] or r["guid_hits"]["encodings"],
             "the bank's registry id was not found in its first %d metadata bytes" % avail)
    elif ext in (".bk2", ".bik"):
        r.update(F.parse_bink(rec["head"], file_size=size))
    elif ext in (".mp4", ".m4v"):
        top = F.mp4_top_level(view, size, have)
        moov = [(p, n) for t, p, n in top["boxes"] if t == "moov"]
        need(moov and have(moov[0][0], moov[0][1]), "mp4 'moov' not fetched (fetch another hop round)")
        p, n = moov[0]
        order = [t for t, _, _ in top["boxes"]]
        r.update(kind="mp4", brand=view[8:12].decode("ascii", "replace") if order[:1] == ["ftyp"] else None,
                 tracks=F.parse_moov(view[p:p + n]), moov_offset=p,
                 faststart="mdat" not in order or order.index("moov") < order.index("mdat"))
    elif ext in (".bnk", ".wem"):
        r.update(F.parse_wwise(rec["head"]))
    r["parsed"] = True
    return r


def check(listing: dict, fetched: str, plans: List[dict], profile: dict) -> dict:
    sys.path.insert(0, av_profile.MF)
    import blake3  # noqa: E402
    files, pk_rows = views(plans, fetched)
    out_files = {}
    fails = []
    for rel, rec in sorted(files.items()):
        if rec["head"] is None:
            fails.append("%s: head not fetched" % rel)
            continue
        if rec["missing"]:
            fails.append("%s: planned piece(s) at %s not fetched" % (rel, rec["missing"]))
            continue
        listed = listing["entries"].get(rel)
        if not listed or listed[1] != rec["file_bytes"]:
            fails.append("%s: size differs from the listing" % rel)
            continue
        try:
            out_files[rel] = _file_facts(rel, rec, profile)
        except (F.FormatError, FetchError) as e:
            out_files[rel] = {"parsed": False, "error": getattr(e, "code", "incomplete"), "detail": str(e)}
            fails.append("%s: %s" % (rel, e))
        except Exception as e:  # any parser crash is a failure, never a pass
            out_files[rel] = {"parsed": False, "error": type(e).__name__, "detail": str(e)}
            fails.append("%s: parser crash %s: %s" % (rel, type(e).__name__, e))
    pk = {}
    for c, p in pk_rows:
        if not os.path.isfile(p):
            fails.append("%s: package chunk not fetched" % c["package_path"])
            continue
        data = open(p, "rb").read()
        if len(data) != c["length"] or blake3.iohash20(data) != c["io_hash20"]:
            fails.append("%s: IoHash mismatch" % c["package_path"])
            continue
        strings = sorted({m.group().decode("ascii", "replace") for m in
                          re.finditer(rb"[\x20-\x7e]{4,}\.(?:bk2|bik|mp4|bank)", data, re.I)})
        pk[c["package_path"]] = {"verified": "iohash", "media_paths": strings}
    # self-checks the installer relies on (each must hold on the game's OWN files)
    banks = {k: v for k, v in out_files.items() if v.get("kind") == "fmod_bank" and v.get("parsed")}
    codecs = sorted({f["codec"] for v in banks.values() for f in v["fsb"]})
    with_guid = [v for v in banks.values() if v["guid_hits"]["bank_asset"]]
    enc_sets = [set(v["guid_hits"]["encodings"]) for v in with_guid]
    common = set.intersection(*enc_sets) if enc_sets else set()
    enc = sorted(common)[0] if len(common) == 1 else None
    anchors = {v["guid_hits"]["anchors"].get(enc) for v in with_guid} if enc else set()
    bink = {k: v for k, v in out_files.items() if v.get("kind") in ("bink2", "bink1")}
    exe = profile.get("exe", {}).get("bink_header_checks", {})
    fm = profile["audio"]["fmod"] or {}
    selfcheck = {
        "bank_fmt_versions": sorted({v["fmt_version"] for v in banks.values()}),
        "bank_codecs": codecs,
        "bank_codecs_decodable_by_runtime": bool(codecs) and all(c in fm.get("fsb_codecs_decodable", []) for c in codecs),
        "bank_guid_encoding": enc,
        "bank_guid_encoding_candidates": sorted(common),
        # the GUID must sit at the same place in every game bank: right after the
        # same 4-byte chunk id + u32 size (anchor), or the gate stays closed
        "bank_guid_anchor": sorted(anchors)[0] if enc and len(anchors) == 1 and None not in anchors else None,
        "banks_matched_to_assets": len(with_guid),
        "banks_parsed": len(banks),
        "base_vorbis_setups": sorted({c for v in banks.values() for f in v["fsb"]
                                      for c in f.get("vorbis_setup_crc32", [])}),
        "bink_magics": sorted({v["magic"] for v in bink.values()}),
        "bink_magics_accepted_by_exe": bool(bink) and all(v["magic"] in exe.get("bink2_accepted", []) for v in bink.values()),
        "mp4_codecs": sorted({e.get("fourcc") for v in out_files.values() if v.get("kind") == "mp4"
                              for t in v.get("tracks", []) for e in t["entries"]}),
    }
    selfcheck["banks_without_asset"] = sorted(k for k, v in banks.items() if not v["guid_hits"]["bank_asset"])
    known = set(fm.get("vorbis_setup_crcs") or [])
    selfcheck["base_vorbis_setups_in_runtime_table"] = (all(c in known for c in selfcheck["base_vorbis_setups"])
                                                        if selfcheck["base_vorbis_setups"] else None)
    if selfcheck["base_vorbis_setups_in_runtime_table"] is False:
        fails.append("a game bank names a Vorbis setup id missing from the runtime table: the table reading is wrong")
    n = len(out_files) + len(pk)
    return {"schema": "av-base-media/2", "label": "Mac-only, from console read-only fetch",
            "root": listing["root"], "files": out_files, "packages": pk, "selfcheck": selfcheck,
            "failures": fails, "checked": n}


def _guid_hits(meta: bytes, profile: dict, rel: str) -> dict:
    """Which GUID byte layouts of the bank's registry id occur in its metadata,
    and for each, the chunk id + size word right before it (the anchor)."""
    fm = profile["audio"]["fmod"] or {}
    stem = os.path.splitext(os.path.basename(rel))[0].lower()
    bank = next((b for b in fm.get("banks", []) if b["name"].lower() == stem), None)
    if not bank or not bank.get("guid"):
        return {"bank_asset": None, "encodings": [], "anchors": {}}
    hits, anchors = [], {}
    for k, v in F.guid_encodings(bank["guid"]).items():
        at = meta.find(v)
        if at < 0:
            continue
        hits.append(k)
        if at >= 8 and meta.find(v, at + 1) < 0:
            cid = meta[at - 8:at - 4]
            if all(32 <= c < 127 for c in cid):
                anchors[k] = cid.decode("ascii")
    return {"bank_asset": bank["package"], "encodings": hits, "anchors": anchors}


# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("packages"); a.add_argument("--profile", required=True); a.add_argument("--mirror", required=True)
    a.add_argument("--out", required=True)
    a = sub.add_parser("list"); a.add_argument("--host", required=True); a.add_argument("--port", type=int, default=2121)
    a.add_argument("--root", required=True); a.add_argument("--subdirs", nargs="+", required=True)
    a.add_argument("--out", required=True)
    a = sub.add_parser("heads"); a.add_argument("--listing", required=True); a.add_argument("--out", required=True)
    a = sub.add_parser("hops"); a.add_argument("--plans", nargs="+", required=True); a.add_argument("--dir", required=True)
    a.add_argument("--out", required=True)
    a = sub.add_parser("check"); a.add_argument("--listing", required=True); a.add_argument("--dir", required=True)
    a.add_argument("--plans", nargs="+", required=True); a.add_argument("--profile", required=True)
    a.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    try:
        if a.cmd == "packages":
            plan = packages_plan(jload(a.profile), a.mirror)
            jdump(plan, a.out)
            print("av_fetch packages: %d chunks, %d bytes, %d missing, %d skipped (too big)" %
                  (len(plan["chunks"]), plan["bytes_total"], len(plan["missing"]), len(plan["skipped_too_big"])))
            return 0 if plan["chunks"] and not plan["missing"] else 1
        if a.cmd == "list":
            list_dir, close = ftp_lister(a.host, a.port)
            try:
                res = walk(list_dir, a.root.rstrip("/"), a.subdirs)
            finally:
                close()
            jdump(res, a.out)
            print("av_fetch list: %d entries (%d files), subdirs found %s" %
                  (len(res["entries"]), res["files"], res["subdirs_found"]))
            return 0 if res["files"] else 1
        if a.cmd == "heads":
            plan = heads_plan(jload(a.listing))
            jdump(plan, a.out)
            print("av_fetch heads: %d chunks, %d bytes" % (len(plan["chunks"]), plan["bytes_total"]))
            return 0 if plan["chunks"] else 1
        if a.cmd == "hops":
            plan = hops_plan([jload(p) for p in a.plans], a.dir)
            jdump(plan, a.out)
            print("av_fetch hops: %d chunks, %d bytes" % (len(plan["chunks"]), plan["bytes_total"]))
            return 0
        if a.cmd == "check":
            res = check(jload(a.listing), a.dir, [jload(p) for p in a.plans], jload(a.profile))
            jdump(res, a.out)
            print("av_fetch check: %d items checked, %d failures; selfcheck %s" %
                  (res["checked"], len(res["failures"]), json.dumps(res["selfcheck"], sort_keys=True)))
            for f in res["failures"][:40]:
                print("  FAIL", f)
            return 0 if res["checked"] and not res["failures"] else 1
    except (FetchError, F.FormatError, av_profile.ProfileError, OSError, ftplib.Error) as e:
        print("av_fetch %s: REFUSED: %s" % (a.cmd, e))
        return 2
    return 2


if __name__ == "__main__":
    sys.exit(main())
