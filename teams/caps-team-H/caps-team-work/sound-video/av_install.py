#!/usr/bin/env python3
"""av_install.py -- sound / video mod installer prototype (Team H, Mac side, DRY RUN by default).

Mirrors capability #12's shape: detect by files, derive at install time, fail
closed, byte-exact undo.  Generic per engine: every rule comes from the base
profile (the game's own config, asset registry, executable and audio runtime,
see av_profile.py) and the base media facts (the game's own bank / movie
headers, read-only console fetch, see av_fetch.py).  No game or mod names.

  plan    --profile P --listing L [--base-media M] --mod DIR [--decision D ...]
          --out plan.json            -> dry run: classification, verdict per
                                        file, the derived files, every planned
                                        console operation and the undo steps.
  apply   --plan plan.json --confirm PLAN_SHA --target-dir DIR --journal J
          LOCAL FOLDER ONLY (host tests / a Mac mirror).  There is deliberately
          no console backend in this prototype; the integrator wires one later.
  restore --plan plan.json --target-dir DIR --journal J
          identity-driven, idempotent: puts every original back by rename and
          checks its sha256; our file is moved aside, never deleted.

Derivation today is byte-identical (the formats the PC mods ship are formats
the platform build already decodes: see DESIGN.md); the plan still treats the
output as a derived copy with its own path, sha and readback so a real
converter can slot in per kind later.  Mod files are only ever read.

Exit codes: 0 ok (plan executable / applied / restored); 2 refused (nothing
changed); 3 stopped mid-way (run restore); 4 dry-run only (plan is valid but
must not be executed: provisional listing or pending base facts).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import secrets
import shutil
import sys
from typing import Dict, List, Optional, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import av_formats as F  # noqa: E402

SCHEMA = "av-install-plan/1"
AUDIO_EXT = {".bank"}
VIDEO_EXT = {".bk2", ".bik", ".mp4", ".m4v"}
WWISE_EXT = {".bnk", ".wem"}
RAW_AUDIO_EXT = {".wav", ".ogg", ".mp3", ".flac", ".fsb", ".opus", ".aac"}
CONTAINER_EXT = {".pak", ".utoc", ".ucas", ".sig"}
SIDE_EXT = {".txt", ".md", ".pdf", ".png", ".jpg", ".jpeg", ".url", ".ini", ".lua", ".json", ".gif", ".webp"}
DECISIONS = {
    "accept-older-bank-format": "a mod bank built by an OLDER bank format than the game's own banks",
    "accept-unproven-codec": "a codec the platform runtime contains but none of the game's own banks/movies use",
    "accept-unproven-bink-revision": "a Bink 2 revision the executable accepts but none of the game's movies use",
    "allow-master-bank": "replace the mixer bank (platform-specific routing; v1 refuses it by default)",
    "use-non-default-pc-variant": "use a PC bank variant other than the default PC folder",
}


class Refused(Exception):
    def __init__(self, code: str, detail: str = ""):
        super().__init__("%s: %s" % (code, detail))
        self.code, self.detail = code, detail


def require(ok, code: str, detail: str = "") -> None:
    if not ok:
        raise Refused(code, detail)


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def canonical(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":")).encode()


def jload(p: str):
    with open(p) as f:
        return json.load(f)


def jdump(obj, p: str) -> None:
    tmp = p + ".tmp"
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=1, sort_keys=True)
    os.replace(tmp, p)


# ---------------------------------------------------------------------------
# provisional listing (dry runs before the console fetch)
# ---------------------------------------------------------------------------

def provisional_listing(profile: dict) -> dict:
    """Expected loose file names derived from the game's own registry/config.

    Lower-case, like the platform's staged folders already seen on the console
    (paks dir).  Sizes unknown (-1).  A plan made from it is DRY-RUN ONLY."""
    entries = {}
    fm = profile["audio"]["fmod"] or {}
    for d in fm.get("platform_bank_dirs", []):
        for b in fm.get("banks", []):
            entries[(d + "/" + b["name"] + ".bank").lower()] = ["-", -1]
        if fm.get("master_bank"):
            entries[(d + "/" + fm["master_bank"] + ".strings.bank").lower()] = ["-", -1]
    for r in profile["video"]["electra"]["file_media_sources"]:
        if r.get("file_path"):
            entries[r["file_path"].lstrip("./").lower()] = ["-", -1]
    for pkg in profile["video"]["bink"]["players"]:
        rel = pkg.replace("/Game/", "", 1)
        entries[(rel + ".bk2").lower()] = ["-", -1]
    return {"schema": "av-listing/1", "root": "<content dir: provisional>", "provisional": True,
            "entries": entries, "files": len(entries)}


# ---------------------------------------------------------------------------
# inventory + classification
# ---------------------------------------------------------------------------

def inventory(mod_dir: str) -> List[dict]:
    require(os.path.isdir(mod_dir), "mod_dir_missing", mod_dir)
    rows = []
    for root, dirs, files in os.walk(mod_dir, followlinks=False):
        dirs.sort()
        for d in dirs:
            require(not os.path.islink(os.path.join(root, d)), "mod_contains_link", d)
        for f in sorted(files):
            p = os.path.join(root, f)
            require(not os.path.islink(p), "mod_contains_link", f)
            rel = os.path.relpath(p, mod_dir).replace(os.sep, "/")
            with open(p, "rb") as fh:
                head = fh.read(16)
            rows.append({"rel": rel, "path": p, "bytes": os.path.getsize(p), "sha256": sha256_file(p),
                         "ext": os.path.splitext(f)[1].lower(), "sniff": F.sniff(head)})
    require(rows, "mod_empty")
    return rows


def classify(row: dict, profile: dict) -> str:
    e, s = row["ext"], row["sniff"]
    if e == ".bank" or s == "fmod_bank":
        return "fmod_bank"
    if e == ".bk2" or s == "bink2":
        return "bink2"
    if e == ".bik" or s == "bink1":
        return "bink1"
    if e in (".mp4", ".m4v") or s == "mp4":
        return "mp4"
    if e in WWISE_EXT or s == "wwise_bank":
        return "wwise"
    if e in RAW_AUDIO_EXT or s in ("ogg", "fsb5") or (s == "wav_or_wem" and e not in WWISE_EXT):
        return "raw_audio"
    if e in CONTAINER_EXT or s == "utoc":
        return "container"
    if e in SIDE_EXT:
        return "side"
    return "unknown"


def _parts_lower(p: str) -> List[str]:
    return [x.lower() for x in p.replace("\\", "/").split("/") if x]


def _find_seq(parts: List[str], seq: List[str]) -> Optional[int]:
    for i in range(len(parts) - len(seq) + 1):
        if parts[i:i + len(seq)] == seq:
            return i
    return None


def listing_index(listing: dict) -> Dict[str, List[str]]:
    idx: Dict[str, List[str]] = {}
    for rel, (kind, _size) in listing["entries"].items():
        if kind == "-":
            idx.setdefault(rel.lower(), []).append(rel)
    return idx


def resolve_target(idx: Dict[str, List[str]], want: str) -> str:
    hits = idx.get(want.lower(), [])
    require(hits, "target_not_in_game", "%s (replacement only: the game has no such file)" % want)
    require(len(hits) == 1, "target_ambiguous_case", want)
    return hits[0]


def map_bank(row: dict, profile: dict, idx, decisions) -> Tuple[str, dict]:
    fm = profile["audio"]["fmod"]
    require(fm, "no_fmod_in_game", "the game does not use FMOD")
    plats = fm["platform_bank_dirs"]
    require(len(plats) == 1, "platform_bank_dir_not_unique", repr(plats))
    parts = _parts_lower(row["rel"])
    hit = None
    for pc in fm["pc_bank_dirs"]:
        k = _find_seq(parts, _parts_lower(pc))
        if k is not None:
            hit = (pc, k + len(_parts_lower(pc)))
            break
    require(hit is not None, "bank_outside_pc_bank_folder",
            "%s: expected under one of %s" % (row["rel"], fm["pc_bank_dirs"]))
    pc, k = hit
    orig = [x for x in row["rel"].replace("\\", "/").split("/") if x]
    require(k < len(orig), "bank_folder_without_file", row["rel"])
    rest = "/".join(orig[k:])
    stem = rest.rsplit("/", 1)[-1][:-len(".bank")] if rest.lower().endswith(".bank") else rest
    target = resolve_target(idx, plats[0] + "/" + rest)
    meta = {"pc_variant": pc, "is_default_variant": pc == fm.get("default_pc_bank_dir"), "bank_stem": stem}
    master = (fm.get("master_bank") or "").lower()
    require(master, "master_bank_unknown",
            "the game's config names no master bank: cannot tell the mixer bank apart, refusing every bank")
    if stem.lower().split(".")[0] == master:
        require("allow-master-bank" in decisions, "mixer_bank_refused",
                "%s holds the platform mixer (v1 refuses it; decision allow-master-bank)" % rest)
    return target, meta


def map_movie(row: dict, profile: dict, idx) -> Tuple[str, dict]:
    parts = row["rel"].replace("\\", "/").split("/")
    low = [p.lower() for p in parts]
    dirs = {d.lower() for d in profile["video"].get("media_dirs") or []} | {"movies"}
    k = next((i for i, p in enumerate(low[:-1]) if p in dirs), None)
    if k is not None:
        return resolve_target(idx, "/".join(parts[k:])), {"matched_by": "path"}
    name = low[-1]
    hits = [r for key, rels in idx.items() if key.rsplit("/", 1)[-1] == name for r in rels]
    require(hits, "target_not_in_game", "%s (no movie of that name)" % row["rel"])
    require(len(hits) == 1, "movie_name_ambiguous", "%s matches %d game files" % (row["rel"], len(hits)))
    return hits[0], {"matched_by": "unique file name"}


# ---------------------------------------------------------------------------
# gates
# ---------------------------------------------------------------------------

def gate_bank(row, data: bytes, base: Optional[dict], media: Optional[dict], profile: dict, decisions) -> dict:
    fm = profile["audio"]["fmod"]
    b = F.parse_fmod_bank(data, file_size=len(data), complete=True)
    fsbs = [s["fsb"] for s in b["snd"] if s["fsb"]]
    require(fsbs, "bank_has_no_samples_or_encrypted", row["rel"])
    codecs = sorted({f["codec"] for f in fsbs})
    for c in codecs:
        require(c in fm["fsb_codecs_decodable"], "codec_not_in_runtime",
                "%s uses %s; the platform audio runtime has no decoder for it" % (row["rel"], c))
    facts = {"fmt_version": b["fmt_version"], "fmt_subversion": b["fmt_subversion"], "codecs": codecs,
             "samples": sum(f["samples"] for f in fsbs), "vorbis_setup_crc32": sorted({x for f in fsbs
                                                                                     for x in f["vorbis_setup_crc32"]}),
             "padded_chunks": b["padded_chunks"], "pending": [], "notes": []}
    if "VORBIS" in codecs:
        # second source for Vorbis: every setup id the mod's streams name must be
        # in the platform runtime's own table of known setups (av_profile)
        require(fm.get("vorbis_table_validated"), "vorbis_table_unvalidated",
                "the runtime's Vorbis setup table was not found/validated in the game's audio library")
        known = set(fm.get("vorbis_setup_crcs") or [])
        unknown = [c for c in facts["vorbis_setup_crc32"] if c not in known]
        require(not unknown, "vorbis_setup_unknown_to_runtime",
                "%s: Vorbis setup id(s) %s are not in the platform runtime's table" %
                (row["rel"], ["%08x" % c for c in unknown]))
    if base is None:
        facts["pending"].append("base bank facts (format version, codecs, GUID) not fetched yet")
        return facts
    require(base.get("parsed"), "base_bank_unparsed", row["rel"])
    bv = base["fmt_version"]
    require(b["fmt_version"] <= bv, "bank_format_newer_than_game",
            "mod %d > game %d (built by a newer FMOD Studio)" % (b["fmt_version"], bv))
    if b["fmt_version"] < bv:
        require("accept-older-bank-format" in decisions, "bank_format_older_than_game",
                "mod %d < game %d (decision accept-older-bank-format)" % (b["fmt_version"], bv))
        facts["notes"].append("older bank format accepted by decision")
    proven = set((media or {}).get("selfcheck", {}).get("bank_codecs", []))
    unproven = [c for c in codecs if c not in proven]
    if unproven:
        require("accept-unproven-codec" in decisions, "codec_unproven_on_platform",
                "%s: %s decodable by the runtime library but not used by any game bank (decision accept-unproven-codec)"
                % (row["rel"], unproven))
        facts["notes"].append("codec(s) %s: library-only evidence; needs the console proof" % unproven)
    sc = (media or {}).get("selfcheck", {})
    enc, anchor = sc.get("bank_guid_encoding"), sc.get("bank_guid_anchor")
    require(enc and anchor, "bank_guid_method_unproven",
            "the game's own banks did not show one consistent GUID layout and position")
    asset = next((x for x in fm["banks"] if x["name"].lower() == facts_stem(row["target"]).lower()), None)
    require(asset and asset.get("guid"), "bank_asset_missing", "no bank asset for %s" % row["target"])
    want = F.guid_encodings(asset["guid"])[enc]
    meta = data[:b["metadata_end"]]
    tag = anchor.encode("ascii")
    spots = [k for k in range(len(meta)) if meta.startswith(tag, k)]
    require(any(meta[k + 8:k + 24] == want for k in spots), "bank_guid_mismatch",
            "%s does not carry the game's bank id %s right after its '%s' chunk header (the game would not find it)"
            % (row["rel"], asset["guid"], anchor))
    facts["guid"] = asset["guid"]
    if base.get("file_bytes", 0) > 0 and len(data) > 2 * base["file_bytes"]:
        facts["notes"].append("mod bank is %.1fx the game's (memory: all sample data is loaded)"
                              % (len(data) / base["file_bytes"]))
    return facts


def facts_stem(target: str) -> str:
    n = target.rsplit("/", 1)[-1]
    return n[:-5] if n.lower().endswith(".bank") else n


def gate_bink(row, data: bytes, base: Optional[dict], media: Optional[dict], profile: dict, decisions) -> dict:
    h = F.parse_bink(data, file_size=len(data))
    require(h["kind"] == "bink2", "bink1_not_supported", "the executable says 'No Bink 1 support.'")
    exe = (profile.get("exe") or {}).get("bink_header_checks")
    require(exe, "no_executable_facts", "profile has no Bink header check facts from the executable")
    require(h["magic"] in exe["bink2_accepted"], "bink_revision_not_accepted",
            "%s not in the executable's header check %s" % (h["magic"], exe["bink2_accepted"]))
    facts = {k: h[k] for k in ("magic", "width", "height", "fps", "frames", "audio_tracks")}
    facts.update(pending=[], notes=[])
    if base is None:
        facts["pending"].append("base movie facts (revision, size, audio tracks) not fetched yet")
        return facts
    require(base.get("parsed") and base.get("kind") == "bink2", "base_movie_not_bink2", row["target"])
    proven = set((media or {}).get("selfcheck", {}).get("bink_magics", []))
    if h["magic"] not in proven:
        require("accept-unproven-bink-revision" in decisions, "bink_revision_unproven",
                "%s accepted by the executable but no game movie uses it" % h["magic"])
        facts["notes"].append("Bink revision %s: executable-only evidence; needs the console proof" % h["magic"])
    if (h["width"], h["height"]) != (base["width"], base["height"]):
        facts["notes"].append("size %dx%d differs from the game's %dx%d (the player scales to its texture)"
                              % (h["width"], h["height"], base["width"], base["height"]))
    if h["audio_tracks"] and not base["audio_tracks"]:
        facts["notes"].append("the mod movie carries audio but the game's does not (the game plays movie sound "
                              "through its own sound system; the embedded track may stay silent)")
    if abs(h["seconds"] - base["frames"] * base["fps_den"] / base["fps_num"]) > 0.5:
        facts["notes"].append("length differs from the game's by more than 0.5 s")
    return facts


def gate_mp4(row, data: bytes, base: Optional[dict], media: Optional[dict], profile: dict, decisions) -> dict:
    m = F.parse_mp4(data, file_size=len(data), complete=True)
    tracks = [(t["handler"], e) for t in m["tracks"] for e in t["entries"]]
    facts = {"codecs": sorted({e["fourcc"] for _, e in tracks}), "handlers": sorted({h for h, _ in tracks}),
             "faststart": m["faststart"], "pending": [], "notes": []}
    require(tracks, "mp4_no_tracks", row["rel"])
    if base is None:
        facts["pending"].append("base mp4 facts (tracks, codecs, levels) not fetched yet")
        return facts
    require(base.get("parsed") and base.get("kind") == "mp4" and base.get("tracks"), "base_movie_not_mp4", row["target"])
    btracks = [(t["handler"], e) for t in base["tracks"] for e in t["entries"]]
    require(sorted(h for h, _ in tracks) == sorted(h for h, _ in btracks), "mp4_tracks_differ",
            "mod has %s tracks, the game's file %s" % (sorted(h for h, _ in tracks), sorted(h for h, _ in btracks)))
    proven = set((media or {}).get("selfcheck", {}).get("mp4_codecs", []))
    for h, e in tracks:
        require(e["fourcc"] in proven, "mp4_codec_unproven", "%s not used by any game mp4 (%s)" % (e["fourcc"], sorted(proven)))
        same = [be for bh, be in btracks if bh == h and be["fourcc"] == e["fourcc"]]
        require(same, "mp4_codec_differs_from_target", "%s %s: the game's file uses %s" %
                (h, e["fourcc"], sorted(be["fourcc"] for bh, be in btracks if bh == h)))
        be = same[0]
        if h == "vide":
            for k in ("profile_idc", "level_idc"):
                require(k in e and k in be, "mp4_video_facts_missing", "%s missing on the %s side" %
                        (k, "mod" if k not in e else "game"))
            require(e["level_idc"] <= be["level_idc"] and e["profile_idc"] <= be["profile_idc"],
                    "mp4_profile_level_above_game", "%s %s > game %s" % (e["fourcc"], (e["profile_idc"], e["level_idc"]),
                                                                        (be["profile_idc"], be["level_idc"])))
        elif h == "soun" and "aac_aot" in be:
            require(e.get("aac_aot") == be["aac_aot"], "mp4_audio_profile_differs",
                    "AAC object type %s vs game %s" % (e.get("aac_aot"), be["aac_aot"]))
    return facts


GATES = {"fmod_bank": gate_bank, "bink2": gate_bink, "bink1": gate_bink, "mp4": gate_mp4}


# ---------------------------------------------------------------------------
# plan
# ---------------------------------------------------------------------------

def make_plan(profile: dict, listing: dict, media: Optional[dict], mod_dir: str, decisions: List[str],
              tool_sha: str) -> dict:
    for d in decisions:
        require(d in DECISIONS, "unknown_decision", d)
    if media is not None:
        require(media.get("root") == listing.get("root"), "base_media_from_other_listing")
        require(not media.get("failures"), "base_media_has_failures", "%d" % len(media["failures"]))
    rows = inventory(mod_dir)
    idx = listing_index(listing)
    files, delegated, ignored, refused = [], [], [], []
    for r in rows:
        kind = classify(r, profile)
        r = dict(r, kind=kind)
        if kind == "side":
            if r["ext"] in (".lua", ".ini", ".json"):
                delegated.append({"rel": r["rel"], "to": "mod manager (UE4SS script / config, not sound or video)"})
            else:
                ignored.append({"rel": r["rel"], "why": "readme / image: not a sound or video file"})
            continue
        if kind == "container":
            delegated.append({"rel": r["rel"], "to": "capabilities #7/#8 (paktx pak/IoStore pipeline)"})
            continue
        try:
            if kind == "wwise":
                raise Refused("middleware_mismatch" if (profile["audio"]["fmod"] and not profile["audio"]["wwise"]["present"])
                              else "wwise_not_profiled", "%s is a Wwise file" % r["rel"])
            if kind == "raw_audio":
                raise Refused("raw_audio_needs_authoring", "%s: loose audio must be built into the game's sound "
                                                          "bank format by an authoring tool first" % r["rel"])
            if kind == "unknown":
                raise Refused("unknown_file_kind", r["rel"])
            if kind == "fmod_bank":
                target, meta = map_bank(r, profile, idx, decisions)
            else:
                target, meta = map_movie(r, profile, idx)
            r.update(target=target, **meta)
            data = open(r["path"], "rb").read()
            base = (media or {}).get("files", {}).get(target)
            if media is not None:
                require(base is not None, "base_facts_missing_for_target", target)
            r["facts"] = GATES[kind](r, data, base, media, profile, decisions)
            r["base"] = {"file_bytes": listing["entries"][target][1],
                         "head_sha256": base.get("head_sha256") if base else None,
                         "head_bytes": base.get("head_bytes") if base else None}
            files.append(r)
        except Refused as e:
            refused.append({"rel": r["rel"], "kind": kind, "code": e.code, "detail": e.detail})
        except F.FormatError as e:
            refused.append({"rel": r["rel"], "kind": kind, "code": "mod_file_" + e.code, "detail": e.detail})
        except Exception as e:  # any parser crash refuses the file (fail closed)
            refused.append({"rel": r["rel"], "kind": kind, "code": "mod_file_unparsable",
                            "detail": "%s: %s" % (type(e).__name__, e)})
    # variants: one file per target; prefer the default PC variant
    by_target: Dict[str, List[dict]] = {}
    for r in files:
        by_target.setdefault(r["target"], []).append(r)
    chosen = []
    for target, rs in sorted(by_target.items()):
        if len({x["sha256"] for x in rs}) == 1:
            pick = next((x for x in rs if x.get("is_default_variant")), rs[0])
            chosen.append(pick)
            ignored += [{"rel": x["rel"], "why": "identical copy for another PC variant"} for x in rs if x is not pick]
            continue
        dflt = [x for x in rs if x.get("is_default_variant")]
        if len(dflt) == 1:
            chosen.append(dflt[0])
            ignored += [{"rel": x["rel"], "why": "PC variant not used on this platform"} for x in rs if x is not dflt[0]]
            continue
        refused.append({"rel": ", ".join(x["rel"] for x in rs), "kind": rs[0]["kind"],
                        "code": "conflicting_files_for_one_target", "detail": target})
    for r in chosen:
        if r["kind"] == "fmod_bank" and not r.get("is_default_variant"):
            if "use-non-default-pc-variant" not in decisions:
                refused.append({"rel": r["rel"], "kind": r["kind"], "code": "non_default_pc_variant",
                                "detail": "only %s shipped (decision use-non-default-pc-variant)" % r["pc_variant"]})
    refused_rels = {x["rel"] for x in refused}
    chosen = [r for r in chosen if r["rel"] not in refused_rels]
    pending = sorted({p for r in chosen for p in r["facts"]["pending"]})
    nonce = hashlib.sha256(canonical([tool_sha, sorted((r["rel"], r["sha256"]) for r in chosen)])).hexdigest()[:12]
    ops, undo = [], []
    for r in chosen:
        t = r["target"]
        # final names get "-<apply id>" (random per apply, recorded in the journal)
        bak = "%s.original-%s" % (t, nonce)
        aside = "%s.mod-%s" % (t, nonce)
        ops += [{"op": "verify_base", "path": t, "bytes": r["base"]["file_bytes"],
                 "head_sha256": r["base"]["head_sha256"], "head_bytes": r["base"]["head_bytes"]},
                {"op": "record_base_sha", "path": t},
                {"op": "rename", "src": t, "dst_stem": bak, "suffix": ".bak"},
                {"op": "upload", "local": r["path"], "local_sha256": r["sha256"], "dst": t, "bytes": r["bytes"]},
                {"op": "verify", "path": t, "sha256": r["sha256"], "bytes": r["bytes"]}]
        undo += [{"op": "restore", "path": t, "backup_stem": bak, "aside_stem": aside, "ours_sha256": r["sha256"]}]
    plan = {
        "schema": SCHEMA, "label": "Mac-only dry run", "tool_sha256": tool_sha,
        "profile": {"project": profile["project"], "platform": profile["platform"], "sha256": hashlib.sha256(canonical(profile)).hexdigest()},
        "listing": {"root": listing["root"], "provisional": bool(listing.get("provisional")),
                    "sha256": hashlib.sha256(canonical(listing)).hexdigest()},
        "base_media_sha256": hashlib.sha256(canonical(media)).hexdigest() if media is not None else None,
        "mod_dir": os.path.abspath(mod_dir),
        "mod_tree_sha256": hashlib.sha256(canonical(sorted((x["rel"], x["sha256"]) for x in rows))).hexdigest(),
        "decisions": sorted(decisions), "nonce": nonce,
        "files": [{k: r[k] for k in ("rel", "kind", "target", "sha256", "bytes", "facts", "base")
                   if k in r} | {k: r[k] for k in ("pc_variant", "matched_by") if k in r} for r in chosen],
        "delegated": delegated, "ignored": ignored, "refused": refused, "pending_base_facts": pending,
        "operations": ops, "undo": undo,
    }
    plan["executable"] = bool(chosen) and not refused and not pending and not plan["listing"]["provisional"]
    plan["verdict"] = ("REFUSED" if refused or not chosen else
                       "DRY-RUN ONLY" if not plan["executable"] else "READY")
    plan["plan_sha256"] = plan_sha(plan)
    return plan


# ---------------------------------------------------------------------------
# local executor (host tests / Mac mirror only) + restore
# ---------------------------------------------------------------------------

class LocalBackend:
    """Local folder with the same semantics as an FTP target: rename refuses an
    existing destination; upload writes straight to the final name (a failed
    upload can leave a partial file there, like STOR)."""

    def __init__(self, root: str, fail_after: Optional[int] = None, partial_on_fail: bool = False):
        require(os.path.isdir(root), "target_dir_missing", root)
        self.root = os.path.realpath(root)
        self.fail_after = fail_after
        self.partial_on_fail = partial_on_fail
        self.mutations = 0

    def p(self, rel: str) -> str:
        parts = rel.split("/")
        require(rel and not rel.startswith("/") and all(x not in ("", ".", "..") for x in parts), "bad_rel", rel)
        full = os.path.realpath(os.path.join(self.root, *parts))
        require(full.startswith(self.root + os.sep), "path_escapes_target", rel)
        return full

    def _tick(self):
        if self.fail_after is not None and self.mutations >= self.fail_after:
            raise RuntimeError("injected failure after %d mutations" % self.mutations)
        self.mutations += 1

    def exists(self, rel): return os.path.lexists(self.p(rel))
    def size(self, rel): return os.path.getsize(self.p(rel))
    def sha256(self, rel): return sha256_file(self.p(rel))

    def head_sha(self, rel, n):
        with open(self.p(rel), "rb") as f:
            return hashlib.sha256(f.read(n)).hexdigest()

    def rename(self, src, dst):
        require(not self.exists(dst), "rename_target_exists", dst)
        self._tick()
        os.rename(self.p(src), self.p(dst))

    def upload(self, local, dst):
        require(not self.exists(dst), "upload_target_exists", dst)
        try:
            self._tick()
        except RuntimeError:
            if self.partial_on_fail:
                with open(local, "rb") as f, open(self.p(dst), "wb") as g:
                    g.write(f.read(max(1, os.path.getsize(local) // 2)))
            raise
        shutil.copyfile(local, self.p(dst))


def plan_sha(plan: dict) -> str:
    return hashlib.sha256(canonical({k: v for k, v in plan.items() if k != "plan_sha256"})).hexdigest()


def _names(row: dict, apply_id: str) -> dict:
    return {"backup": "%s-%s.bak" % (row["backup_stem"], apply_id),
            "aside": "%s-%s.aside" % (row["aside_stem"], apply_id),
            "unknown_aside": "%s-%s.unknown.aside" % (row["aside_stem"], apply_id)}


def apply_plan(plan: dict, confirm: str, be: LocalBackend, journal_path: str, tool_sha: Optional[str] = None,
               apply_id: Optional[str] = None) -> dict:
    require(plan.get("schema") == SCHEMA, "plan_schema")
    require(plan["plan_sha256"] == plan_sha(plan), "plan_tampered")
    require(confirm == plan["plan_sha256"], "confirm_mismatch", "pass --confirm %s" % plan["plan_sha256"])
    require(plan["executable"], "plan_not_executable", plan["verdict"])
    if tool_sha is not None:
        require(plan["tool_sha256"] == tool_sha, "tool_changed_since_plan", "re-run plan with this tool")
    require(not os.path.exists(journal_path), "journal_exists", "run restore first: " + journal_path)
    apply_id = apply_id or secrets.token_hex(4)
    targets = {u["path"]: dict(_names(u, apply_id), ours_sha256=u["ours_sha256"], base_sha256=None)
               for u in plan["undo"]}
    # gate: every target is the file the plan saw, every backup/aside name is free
    for op in plan["operations"]:
        if op["op"] == "verify_base":
            require(be.exists(op["path"]), "base_missing", op["path"])
            if op["bytes"] >= 0:
                require(be.size(op["path"]) == op["bytes"], "base_size_changed", op["path"])
            if op["head_sha256"]:
                require(be.head_sha(op["path"], op["head_bytes"]) == op["head_sha256"], "base_head_changed", op["path"])
    for t, n in targets.items():
        for k in ("backup", "aside", "unknown_aside"):
            require(not be.exists(n[k]), "backup_name_taken", n[k])
    journal = {"plan_sha256": plan["plan_sha256"], "apply_id": apply_id, "targets": {}, "done": []}
    jdump(journal, journal_path)
    started = False
    try:
        for i, op in enumerate(plan["operations"]):
            if op["op"] == "record_base_sha":
                journal["targets"][op["path"]] = dict(targets[op["path"]], base_sha256=be.sha256(op["path"]))
                jdump(journal, journal_path)  # written BEFORE the first change to this target
            elif op["op"] == "rename":
                require(journal["targets"].get(op["src"], {}).get("base_sha256"), "base_sha_not_recorded", op["src"])
                started = True
                be.rename(op["src"], journal["targets"][op["src"]]["backup"])
            elif op["op"] == "upload":
                require(sha256_file(op["local"]) == op["local_sha256"], "mod_file_changed", op["local"])
                be.upload(op["local"], op["dst"])
            elif op["op"] == "verify":
                require(be.size(op["path"]) == op["bytes"] and be.sha256(op["path"]) == op["sha256"],
                        "readback_mismatch", op["path"])
            journal["done"].append(i)
            jdump(journal, journal_path)
    except Exception as e:
        jdump(journal, journal_path)
        if started:
            raise Stopped(str(e))
        raise
    return journal


class Stopped(Exception):
    pass


def restore(plan: dict, be: LocalBackend, journal_path: str) -> dict:
    """Idempotent undo, driven by the journal the apply wrote.

    The plan must be the untampered plan the journal belongs to.  Only targets
    the apply recorded (base sha256 taken before any change) are touched.  If
    our backup exists, whatever sits at the target (our copy, or a partial
    upload) is moved aside -- never deleted -- and the backup is renamed back.
    The target's sha256 must then equal the one recorded before the change."""
    require(plan.get("plan_sha256") == plan_sha(plan), "plan_tampered")
    require(os.path.exists(journal_path), "journal_missing", journal_path)
    journal = jload(journal_path)
    require(journal["plan_sha256"] == plan["plan_sha256"], "journal_other_plan")
    undo = {u["path"]: u for u in plan["undo"]}
    report = []
    for t, n in sorted(journal["targets"].items()):
        u = undo.get(t)
        require(u is not None, "journal_target_not_in_plan", t)
        want = _names(u, journal["apply_id"])
        require(all(n[k] == want[k] for k in want), "journal_names_not_from_plan", t)
        require(n.get("base_sha256"), "journal_without_base_sha", t)
        state = "original in place"
        if be.exists(n["backup"]):
            if be.exists(t):
                ours = be.sha256(t) == u["ours_sha256"]
                dst = n["aside"] if ours else n["unknown_aside"]
                require(not be.exists(dst), "aside_name_taken", dst)
                be.rename(t, dst)
                state = "restored (our copy moved aside)" if ours else \
                    "restored (unexpected/partial file moved aside: %s)" % dst
            else:
                state = "restored (no copy had been written)"
            be.rename(n["backup"], t)
        else:
            require(be.exists(t), "original_and_backup_missing", t)
        require(be.sha256(t) == n["base_sha256"], "restored_sha_mismatch", t)
        report.append({"path": t, "state": state, "sha256_verified": True})
    untouched = sorted(set(undo) - set(journal["targets"]))
    return {"restored": report, "untouched": untouched}


# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("plan")
    a.add_argument("--profile", required=True)
    g = a.add_mutually_exclusive_group(required=True)
    g.add_argument("--listing")
    g.add_argument("--provisional-listing", action="store_true")
    a.add_argument("--base-media")
    a.add_argument("--mod", required=True)
    a.add_argument("--decision", action="append", default=[])
    a.add_argument("--out", required=True)
    for name in ("apply", "restore"):
        a = sub.add_parser(name)
        a.add_argument("--plan", required=True)
        a.add_argument("--target-dir", required=True)
        a.add_argument("--journal", required=True)
        if name == "apply":
            a.add_argument("--confirm", required=True)
    a = ap.parse_args(argv)
    tool_sha = sha256_file(os.path.abspath(__file__))
    try:
        if a.cmd == "plan":
            profile = jload(a.profile)
            listing = provisional_listing(profile) if a.provisional_listing else jload(a.listing)
            media = jload(a.base_media) if a.base_media else None
            plan = make_plan(profile, listing, media, a.mod, a.decision, tool_sha)
            jdump(plan, a.out)
            print("av_install plan: %s -- %d file(s) to install, %d refused, %d delegated, %d ignored, "
                  "%d operations, %d pending base facts; plan sha %s" %
                  (plan["verdict"], len(plan["files"]), len(plan["refused"]), len(plan["delegated"]),
                   len(plan["ignored"]), len(plan["operations"]), len(plan["pending_base_facts"]),
                   plan["plan_sha256"][:16]))
            for f in plan["files"]:
                print("  OK      %s -> %s  %s" % (f["rel"], f["target"], json.dumps({k: v for k, v in f["facts"].items()
                                                                                   if k not in ("pending",)})[:200]))
            for f in plan["refused"]:
                print("  REFUSED %s [%s] %s" % (f["rel"], f["code"], f["detail"]))
            for p in plan["pending_base_facts"]:
                print("  PENDING %s" % p)
            if plan["verdict"] == "REFUSED":
                return 2
            return 0 if plan["executable"] else 4
        plan = jload(a.plan)
        be = LocalBackend(a.target_dir)
        if a.cmd == "apply":
            j = apply_plan(plan, a.confirm, be, a.journal, tool_sha=tool_sha)
            print("av_install apply: %d operations done (local folder)" % len(j["done"]))
            return 0
        r = restore(plan, be, a.journal)
        print("av_install restore: %s" % json.dumps(r))
        return 0
    except Stopped as e:
        print("av_install %s: STOPPED after a change was started: %s -- run restore" % (a.cmd, e))
        return 3
    except Refused as e:
        print("av_install %s: REFUSED [%s] %s (nothing changed)" % (a.cmd, e.code, e.detail))
        return 2


if __name__ == "__main__":
    sys.exit(main())
