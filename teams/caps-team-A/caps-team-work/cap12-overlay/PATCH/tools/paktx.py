#!/usr/bin/env python3
"""paktx.py -- capability #7 step 1: the proven "Leon" pak method, automated.

Method (console-proven ONCE, 2026-10-01/02, one mod, one container:
.superpowers/native-observations/leon-index-overlay-trial-2026-10-01/result.json
"phase": "in_game_leon_appearance_confirmed_by_user"): keep the game's huge
base .ucas untouched, put the mod's bytes in a NEW partition file
<stem>_s1.ucas, rename the game's original <stem>.utoc to a nonce backup and
publish a DERIVED <stem>.utoc that indexes base rows verbatim plus the mod
rows in partition 1.  Restore = put the original index back (by rename, so it
is byte-exact by construction) and verify its sha256.

What this tool adds (generic, no game or mod names in logic):
  * a recorded game build (`record`): every file of the game's paks folder
    with bytes + sha256 (sha for .utoc/.pak/other small files, size for the
    .ucas giants), per-container package ownership, and the ContainerHeader
    chunk bytes tied to the TOC by its IoHash.
  * multi-container ownership: each mod package goes into the base container
    that ALREADY owns it (no duplicate package ids are ever created); new
    packages go into the container that owns most of the mod's replaced
    packages (or an explicit --new-packages-into); a mod split across N
    containers is projected per owner, then each owner gets its own derived
    index + sidecar through the pinned overlay_builder.build_overlay.
  * classic .pak mods: an empty companion .pak is recognised and skipped; a
    .pak whose loose files override entries of a base .pak triggers a base
    .pak rebuild through the pinned repak (the 2026-10-01 config-trial
    command pattern), verified entry-by-entry; cooked assets inside a classic
    .pak are refused (conversion is not step 1).
  * a hash gate (fail closed): before any change, every recorded file is
    re-read and must match the record (sha or size), and no unrecorded
    mountable container may sit anywhere in the paks folder tree (the
    engine also mounts pak files in subfolders such as ~mods/); `check` runs
    the same gate read-only and prints the recursive listing.
  * a deterministic plan (same inputs -> same plan sha), a dry run that
    prints every planned change, and `--confirm <plan sha>` binding.
  * a journaled executor over a backend (local folder for host tests and Mac
    mirrors, or plain FTP for the console -- the integrator runs that one).
  * `restore`: identity-driven, idempotent, works after any interruption;
    original bytes are never rewritten (rename), then sha-verified.
  * `repair`: brings an install that still carries older experiments back to
    the recorded build using our own `.original-*.bak` backups (sha-checked)
    and moves unrecorded container files (at any depth) to a quarantine
    folder OUTSIDE the paks folder (nothing is deleted).  `--only` splits it
    into separate steps, one console OK each.  Refuses on a PARTIAL record.
  * `install --only-into STEM`: diagnostic fallback, one container only
    (packages owned elsewhere are listed and not installed).
  * `install --substitute MANIFEST --accept-decision D ...` (capability #12):
    ship DERIVED package bytes (derive_substitutes.py + mic_reparent.py: a
    mod material instance re-parented to a base-game material) in place of
    the mod's export chunk, with the store-entry import repointed.  Every
    hash has two sources (recomputed here with two independent readers +
    the game's / mod's own TOC, store and header data); the user decisions
    the manifest needs must be restated; the derived index is re-read after
    the build (payload, IoHash, merged store, every import resolves).  Mod
    files stay read-only; restore is unchanged (byte-exact by rename).

LAWS: mod files are read-only inputs. Base .ucas files are never read
beyond the bytes of a mirror, never written, renamed or deleted (FTP backend
refuses every mutation on a recorded .ucas name).  No deletions without
`cleanup --allow-delete`, and only of files this plan owns whose sha matches.

Exit codes: 0 ok; 2 refused = NOTHING on the target changed (no change was
started; JSON carries code/detail and "console_changed": false); 3 stopped =
a change was started, so the target MAY have changed (any error after the
first started change lands here): run the recovery the JSON names
(`restore` for install, `repair --dry-run` for repair).
"""
from __future__ import annotations

import argparse
import ftplib
import hashlib
import ipaddress
import json
import mmap
import os
import posixpath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import types
from typing import Dict, List, Optional, Sequence, Tuple

sys.dont_write_bytecode = True

TOOL = "paktx/1"
PLAN_SCHEMA = "paktx-plan/1"
RECORD_SCHEMA = "paktx-build-record/1"
JOURNAL_SCHEMA = "paktx-journal/1"
# Pinned overlay builder (outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py).
BUILDER_SHA256 = "a72636fbdec542bb6ce9521d6ca5709b61b09739938544671212ec93b0c958e6"
# Pinned repak build (the one the 2026-10-01 config-pak trial used).
REPAK_SHA256 = "57b321cca5714997e6aad7300e713c5137a5ea7fa60b3281bd8eb322c4e36960"

HEADER_CHUNK_TYPE = 6
ALLOWED_MOD_CHUNK_TYPES = {1: "ExportBundleData", 2: "BulkData",
                           3: "OptionalBulkData"}
EXPORT_CHUNK_TYPE = 1
MOUNTABLE_EXT = (".utoc", ".ucas", ".pak", ".sig")
INERT_SUFFIX_RE = re.compile(r"\.(original-[0-9a-f-]+\.bak|derived-[0-9a-f-]+\.retired|"
                             r"foreign-[0-9a-f-]+\.retired|paktx-[0-9a-f]+\.tmp|"
                             r"overlay-[0-9a-f-]+\.tmp|restore-[0-9a-f-]+\.tmp|"
                             r"[0-9a-f-]{36}\.tmp)$")
COOKED_EXT = (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl")
MAX_LOCAL_FILE = 2 * 1024 * 1024 * 1024
BLOCK = 1024 * 1024
# The engine finds pak files anywhere under Paks/ (subfolders such as ~mods/
# included), so the gate and repair walk the whole tree.  Fail closed beyond:
MAX_WALK_DEPTH = 8
MAX_WALK_ENTRIES = 200000


class Refused(Exception):
    def __init__(self, code: str, detail: str = ""):
        super().__init__(code)
        self.code = code
        self.detail = detail


def require(cond, code: str, detail: str = "") -> None:
    if not cond:
        raise Refused(code, detail)


class Stopped(Exception):
    """Executor stopped after (possibly) changing the target."""


# ---------------------------------------------------------------------------
# small helpers
# ---------------------------------------------------------------------------

def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(BLOCK), b""):
            h.update(blk)
    return h.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True).encode("ascii")


def write_json(path: str, obj) -> None:
    tmp = path + ".writing"
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=1, sort_keys=True)
        f.write("\n")
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def read_json(path: str):
    with open(path) as f:
        return json.load(f)


def load_builder(path: str):
    """Exec the pinned overlay builder from verified bytes (no __pycache__)."""
    require(os.path.isfile(path), "builder_missing", path)
    data = open(path, "rb").read()
    got = sha256_bytes(data)
    require(got == BUILDER_SHA256, "builder_sha_mismatch",
            "%s is %s, pinned %s" % (path, got, BUILDER_SHA256))
    mod = types.ModuleType("overlay_builder_pinned")
    mod.__file__ = path
    exec(compile(data, path, "exec"), mod.__dict__)
    mod.PAKTX_SHA256 = got
    return mod


def default_builder_path() -> str:
    env = os.environ.get("PAKTX_BUILDER")
    if env:
        return env
    return os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "overlay_builder.py")


def check_repak(path: Optional[str]) -> str:
    require(path is not None and os.path.isfile(path), "repak_unavailable",
            "pass --repak PATH (pinned sha %s)" % REPAK_SHA256)
    got = sha256_file(path)
    require(got == REPAK_SHA256, "repak_sha_mismatch", got)
    return path


def plan_sha(plan: dict) -> str:
    body = dict(plan)
    body.pop("plan_sha256", None)
    return sha256_bytes(canonical(body))


def nonce_from(*parts: str) -> str:
    return sha256_bytes("|".join(parts).encode())[:16]


# ---------------------------------------------------------------------------
# build record (the game's own files -> data)
# ---------------------------------------------------------------------------

def _export_pids(B, toc) -> List[int]:
    return sorted({B.package_id_of_chunk(c) for c in toc.ids
                   if c[11] == EXPORT_CHUNK_TYPE})


def make_record(B, mirror: str, title_id: str, version: str, paks_dir: str,
                sizes: Optional[Dict[str, int]] = None,
                headers_dir: Optional[str] = None,
                provenance: Optional[dict] = None,
                relative_to: Optional[str] = None) -> dict:
    """Record every file of a paks folder.  `mirror` holds exact copies of the
    small files; `sizes` gives sizes of files deliberately not mirrored (the
    multi-GB .ucas).  A .utoc whose ContainerHeader chunk cannot be read is
    recorded with header=None (planning into it refuses later)."""
    sizes = dict(sizes or {})
    require(paks_dir.startswith("/") and not paks_dir.endswith("/"),
            "paks_dir_must_be_absolute_without_trailing_slash")
    files: Dict[str, dict] = {}
    names = sorted(n for n in os.listdir(mirror)
                   if os.path.isfile(os.path.join(mirror, n)) and not n.startswith("."))
    for name in names:
        p = os.path.join(mirror, name)
        size = os.path.getsize(p)
        if name.lower().endswith(".ucas"):
            # base .ucas files are size-gated only (multi-GB; never read on the
            # console); a mirrored copy is used for its header chunk only
            if name in sizes:
                require(sizes[name] == size, "mirror_size_disagrees_with_sizes", name)
            files[name] = {"bytes": size, "sha256": None}
            continue
        files[name] = {"bytes": size, "sha256": sha256_file(p)}
    for name, size in sorted(sizes.items()):
        if name in files:
            continue
        require(name.lower().endswith(".ucas"), "only_ucas_may_be_size_only", name)
        require(type(size) is int and size > 0, "invalid_size", name)
        files[name] = {"bytes": size, "sha256": None}
    for name in files:
        require(not INERT_SUFFIX_RE.search(name), "record_contains_tool_leftover", name)
    require(files, "record_scanned_zero_files")
    containers: Dict[str, dict] = {}
    for name in sorted(files):
        if not name.lower().endswith(".utoc"):
            continue
        stem = name[:-5]
        ucas = stem + ".ucas"
        require(ucas in files, "ucas_missing_for_utoc", name)
        data = open(os.path.join(mirror, name), "rb").read()
        entry = {"utoc": name, "ucas": ucas, "ucas_bytes": files[ucas]["bytes"],
                 "pak": (stem + ".pak") if (stem + ".pak") in files else None}
        try:
            toc = B.parse_toc(data)
        except B.Refused as e:
            entry.update({"admitted": False, "refusal": e.code})
            containers[stem] = entry
            continue
        pids = _export_pids(B, toc)
        entry.update({"admitted": True, "container_id": toc.container_id.hex(),
                      "block_size": toc.block_size, "chunk_count": toc.chunk_count,
                      "export_package_count": len(pids),
                      "export_package_ids_sha256": sha256_bytes(
                          b"".join(struct.pack("<Q", p) for p in pids)),
                      "header": None})
        hid = toc.container_id + bytes.fromhex("00000006")
        header_bytes = None
        source = None
        if hid in toc.ids:
            local_ucas = os.path.join(mirror, ucas)
            pre = os.path.join(headers_dir, stem + ".container-header.bin") if headers_dir else None
            if os.path.isfile(local_ucas):
                header_bytes = B.read_base_header_chunk(toc, local_ucas, None)
                source = "mirror_ucas"
                entry["ucas_mirror_sha256"] = sha256_file(local_ucas)
            elif pre and os.path.isfile(pre):
                header_bytes = open(pre, "rb").read()
                source = os.path.abspath(pre)
                if relative_to:
                    # always relative to the record's folder (also with ".."), so the
                    # record sha does not depend on where the folders sit
                    source = os.path.relpath(source, os.path.abspath(relative_to))
        if header_bytes is not None:
            row = toc.ids.index(hid)
            require(toc.offset_length(row)[1] == len(header_bytes),
                    "header_length_disagrees_with_toc", stem)
            require(toc.meta(row)[:20] == B.blake3_20(header_bytes),
                    "header_iohash_disagrees_with_toc", stem)
            hdr = B.parse_container_header(header_bytes)
            require(set(hdr.packages) == set(pids),
                    "header_store_disagrees_with_export_chunks", stem)
            entry["header"] = {"bytes": len(header_bytes),
                               "sha256": sha256_bytes(header_bytes),
                               "source": source,
                               "iohash_matches_toc": True}
        containers[stem] = entry
    rec = {"schema": RECORD_SCHEMA, "title_id": title_id, "version": version,
           "paks_dir": paks_dir, "files": files, "containers": containers,
           "provenance": provenance or {}, "builder_sha256": B.PAKTX_SHA256}
    rec["record_sha256"] = sha256_bytes(canonical({k: v for k, v in rec.items()
                                                    if k != "record_sha256"}))
    return rec


def load_record(path: str) -> dict:
    rec = read_json(path)
    require(rec.get("schema") == RECORD_SCHEMA, "invalid_record_schema")
    body = {k: v for k, v in rec.items() if k != "record_sha256"}
    require(sha256_bytes(canonical(body)) == rec.get("record_sha256"),
            "record_self_hash_mismatch")
    require(rec["files"], "record_has_zero_files")
    rec["_dir"] = os.path.dirname(os.path.abspath(path))
    return rec


def record_header_bytes(rec: dict, stem: str, mirror: Optional[str], B) -> bytes:
    c = rec["containers"][stem]
    h = c.get("header")
    require(h is not None, "base_container_header_unavailable",
            "%s: the record has no ContainerHeader for this container; fetch "
            "%s (or its header chunk) and re-run record" % (stem, c["ucas"]))
    if h["source"] == "mirror_ucas":
        require(mirror is not None, "mirror_required_for_header", stem)
        toc = B.parse_toc(open(os.path.join(mirror, c["utoc"]), "rb").read())
        data = B.read_base_header_chunk(toc, os.path.join(mirror, c["ucas"]), None)
    else:
        src = h["source"]
        if not os.path.isabs(src):
            src = os.path.join(rec.get("_dir", "."), src)
        data = open(src, "rb").read()
    require(len(data) == h["bytes"] and sha256_bytes(data) == h["sha256"],
            "base_header_identity_changed", stem)
    return data


# ---------------------------------------------------------------------------
# mod inventory + ownership
# ---------------------------------------------------------------------------

def inventory_mod(B, mod_dir: str) -> dict:
    found: Dict[str, str] = {}
    for root, dirs, fnames in os.walk(mod_dir):
        dirs.sort()
        for fn in sorted(fnames):
            if fn.startswith("."):
                continue
            require(fn not in found, "mod_duplicate_basename", fn)
            found[fn] = os.path.join(root, fn)
    require(found, "mod_scanned_zero_files")
    iostore, paks, ignored, files = [], [], [], []
    by_stem: Dict[str, Dict[str, str]] = {}
    for fn, p in found.items():
        stem, ext = os.path.splitext(fn)
        by_stem.setdefault(stem, {})[ext.lower()] = p
        files.append({"name": fn, "bytes": os.path.getsize(p), "sha256": sha256_file(p)})
    for stem in sorted(by_stem):
        exts = by_stem[stem]
        for ext, p in sorted(exts.items()):
            if ext not in (".utoc", ".ucas", ".pak"):
                ignored.append({"name": os.path.basename(p), "reason": "not_a_container"})
        if ".utoc" in exts or ".ucas" in exts:
            require(".utoc" in exts and ".ucas" in exts, "mod_iostore_pair_incomplete", stem)
            mod_id = re.sub(r"[^A-Za-z0-9_.-]", "_", stem)
            iostore.append(B.ModSpec(mod_id, exts[".utoc"], exts[".ucas"]))
        if ".pak" in exts:
            data = open(exts[".pak"], "rb").read()
            try:
                pak = B.parse_pak(data)
            except B.Refused as e:
                if e.code == "classic_pak_empty":
                    ignored.append({"name": stem + ".pak",
                                    "reason": "zero_entry_companion_pak"})
                    continue
                raise Refused("mod_pak_refused", "%s: %s" % (stem, e))
            paks.append({"name": stem + ".pak", "path": exts[".pak"], "pak": pak})
    require(iostore or paks, "mod_has_no_usable_container")
    return {"iostore": iostore, "paks": paks, "ignored": ignored, "files": files}


def ownership(B, rec: dict, mods, new_into: Optional[str]) -> dict:
    """Assign every mod package to exactly one base container."""
    bases = {s: c for s, c in rec["containers"].items() if c.get("admitted")}
    require(bases, "record_has_zero_admitted_containers")
    # export ownership sets come from the mirror TOCs at plan time (re-parsed)
    owner_of: Dict[int, List[str]] = {}
    chunk_owner: Dict[bytes, str] = {}
    for stem, toc in rec["_tocs"].items():
        for cid in toc.ids:
            if cid[11] == EXPORT_CHUNK_TYPE:
                owner_of.setdefault(B.package_id_of_chunk(cid), []).append(stem)
            chunk_owner[cid] = stem
    per_mod = {}
    replaced_count: Dict[str, int] = {}
    for mod in mods:
        header = B.parse_container_header(B.read_mod_header_bytes(mod))
        pids = set(header.packages)
        require(pids, "mod_store_has_zero_packages", mod.mod_id)
        require(mod.toc.block_size in {c["block_size"] for c in bases.values()},
                "mod_block_size_unsupported", mod.mod_id)
        chunk_pids = set()
        for cid in mod.toc.ids:
            if cid == mod.header_chunk_id:
                continue
            require(cid[11] in ALLOWED_MOD_CHUNK_TYPES, "mod_chunk_type_unsupported",
                    "%s chunk %s type %d" % (mod.mod_id, cid.hex(), cid[11]))
            pid = B.package_id_of_chunk(cid)
            require(pid in pids, "mod_chunk_without_store_entry",
                    "%s chunk %s" % (mod.mod_id, cid.hex()))
            chunk_pids.add(pid)
        require(chunk_pids == pids, "mod_store_entry_without_chunks", mod.mod_id)
        assign = {}
        for pid in sorted(pids):
            owners = sorted(set(owner_of.get(pid, [])))
            require(len(owners) <= 1, "package_owned_by_multiple_base_containers",
                    "0x%016x: %s" % (pid, owners))
            assign[pid] = owners[0] if owners else None
            if owners:
                replaced_count[owners[0]] = replaced_count.get(owners[0], 0) + 1
        per_mod[mod.mod_id] = assign
    if new_into is not None:
        require(new_into in bases, "new_packages_target_unknown", new_into)
        primary, rule = new_into, "explicit --new-packages-into"
    else:
        require(replaced_count, "mod_replaces_no_base_package_no_primary",
                "pass --new-packages-into STEM")
        best = max(replaced_count.values())
        top = sorted(s for s, n in replaced_count.items() if n == best)
        require(len(top) == 1, "ambiguous_primary_container", str(top))
        primary, rule = top[0], "container owning most replaced packages"
    groups: Dict[str, Dict[str, set]] = {}
    for mod in mods:
        for pid, owner in per_mod[mod.mod_id].items():
            dest = owner if owner is not None else primary
            groups.setdefault(dest, {}).setdefault(mod.mod_id, set()).add(pid)
    # no mod chunk id may already exist in a container other than its destination
    for mod in mods:
        for cid in mod.toc.ids:
            if cid == mod.header_chunk_id:
                continue
            dest = per_mod[mod.mod_id][B.package_id_of_chunk(cid)] or primary
            other = chunk_owner.get(cid)
            require(other is None or other == dest,
                    "chunk_id_present_in_other_container",
                    "%s chunk %s in %s, destination %s" % (mod.mod_id, cid.hex(), other, dest))
    summary = {}
    for dest, mm in sorted(groups.items()):
        summary[dest] = {mid: {"replaced": sorted("%016x" % p for p in ps
                                                  if per_mod[mid][p] == dest),
                               "added": sorted("%016x" % p for p in ps
                                               if per_mod[mid][p] is None)}
                         for mid, ps in sorted(mm.items())}
    return {"groups": groups, "primary": primary, "primary_rule": rule,
            "summary": summary}


# ---------------------------------------------------------------------------
# projection: a subset of one mod's packages as a self-contained trio
# ---------------------------------------------------------------------------

def _align16(n: int) -> int:
    return (n + 15) & ~15


def project_mod(B, mod, pids: set, out_stem: str, target_dir=None,
                substitute: Optional[Dict[bytes, bytes]] = None,
                store_imports: Optional[Dict[int, Tuple[int, ...]]] = None):
    """Write <out_stem>.utoc/.ucas holding only `pids` (intermediate input for
    build_overlay; never shipped).  Payload bytes are copied block-for-block
    and re-verified chunk by chunk.  `target_dir` (the destination container's
    parsed directory index) re-roots the directory index onto a deeper
    destination mount point: every virtual path stays byte-identical, only the
    split between mount point and directories moves (fail-closed otherwise).

    capability #12 (`install --substitute`): `substitute` maps a chunk id to
    DERIVED payload bytes that replace that chunk in this intermediate trio
    (new stored blocks, new length, IoHash = blake3_20 of the derived bytes);
    `store_imports` maps a package id to its derived store-entry imports.
    Both are verified by verify_substitutions before they get here; the mod
    files themselves are read-only (only the intermediate trio differs)."""
    substitute = substitute or {}
    store_imports = store_imports or {}
    toc = mod.toc
    bs = toc.block_size
    header = B.parse_container_header(B.read_mod_header_bytes(mod))
    ranges = {}
    for row, cid in enumerate(toc.ids):
        off, ln = toc.offset_length(row)
        require(off % bs == 0, "chunk_not_block_aligned", cid.hex())
        ranges[row] = (off // bs, (off + ln - 1) // bs)
    spans = sorted(ranges.values())
    for i in range(1, len(spans)):
        require(spans[i][0] > spans[i - 1][1], "chunk_blocks_shared")
    keep = [r for r, cid in enumerate(toc.ids)
            if cid != mod.header_chunk_id and B.package_id_of_chunk(cid) in pids]
    require(keep, "projection_empty", mod.mod_id)
    require(set(substitute) <= {toc.ids[r] for r in keep}, "substitute_chunk_not_projected",
            mod.mod_id)
    require(set(store_imports) <= set(pids) and set(store_imports) <= set(header.packages),
            "substitute_store_entry_not_projected", mod.mod_id)
    ids, offsets, metas, blocks, ucas = [], bytearray(), bytearray(), bytearray(), bytearray()
    with open(mod.ucas_path, "rb") as fh:
        store = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
        try:
            for row in keep:
                first, last = ranges[row]
                new_first = len(blocks) // 12
                derived = substitute.get(toc.ids[row])
                if derived is not None:
                    # derived payload: stored blocks (the only pinned profile),
                    # new length, IoHash recomputed; meta flags/padding kept
                    require(0 < len(derived) and toc.meta(row)[20:] == bytes(4),
                            "substitute_unsupported_chunk_meta", toc.ids[row].hex())
                    for i in range(0, len(derived), bs):
                        part = derived[i:i + bs]
                        np_ = len(ucas)
                        ucas += part
                        ucas += bytes(_align16(len(ucas)) - len(ucas))
                        blocks += (np_.to_bytes(5, "little") + len(part).to_bytes(3, "little") +
                                   len(part).to_bytes(3, "little") + b"\x00")
                    ids.append(toc.ids[row])
                    offsets += (new_first * bs).to_bytes(5, "big") + len(derived).to_bytes(5, "big")
                    metas += B.blake3_20(derived) + toc.meta(row)[20:]
                    continue
                for b in range(first, last + 1):
                    phys, stored, decoded, method = toc.block(b)
                    np_ = len(ucas)
                    ucas += store[phys:phys + stored]
                    ucas += bytes(_align16(len(ucas)) - len(ucas))
                    blocks += (np_.to_bytes(5, "little") + stored.to_bytes(3, "little") +
                               decoded.to_bytes(3, "little") + bytes([method]))
                _, ln = toc.offset_length(row)
                ids.append(toc.ids[row])
                offsets += (new_first * bs).to_bytes(5, "big") + ln.to_bytes(5, "big")
                metas += toc.meta(row)
        finally:
            store.close()
    entries = {}
    for p in sorted(pids):
        e = header.packages[p]
        if p in store_imports:
            e = B.StoreEntry(tuple(store_imports[p]), e.shader_map_hashes)
        entries[p] = e
    sub = B.ContainerHeader(header.container_id, entries,
                            header.soft_serial_region_absent, header.opaque_zero_suffix)
    hb = B.serialize_container_header(sub)
    first = len(blocks) // 12
    for i in range(0, len(hb), bs):
        part = hb[i:i + bs]
        np_ = len(ucas)
        ucas += part
        ucas += bytes(_align16(len(ucas)) - len(ucas))
        blocks += (np_.to_bytes(5, "little") + len(part).to_bytes(3, "little") +
                   len(part).to_bytes(3, "little") + b"\x00")
    ids.append(mod.header_chunk_id)
    offsets += (first * bs).to_bytes(5, "big") + len(hb).to_bytes(5, "big")
    metas += B.blake3_20(hb) + b"\x00" * 4
    keep_ids = set(ids)
    row_by_id = {cid: i for i, cid in enumerate(ids)}
    dfiles = [f for f in mod.directory["files"] if f.chunk_id in keep_ids]
    mount, root_name = mod.directory["mount_point"], mod.directory["root_name"]
    if target_dir is not None and target_dir["mount_point"] != mount:
        tmount = target_dir["mount_point"]
        require(not root_name and not target_dir["root_name"], "reroot_needs_empty_root_name",
                mod.mod_id)
        require(tmount.startswith(mount) and tmount.endswith("/"), "reroot_target_not_below_mod_mount",
                "%s vs %s" % (tmount, mount))
        extra = tuple(x for x in tmount[len(mount):].split("/") if x)
        require(extra, "reroot_empty_extra")
        moved = []
        for f in dfiles:
            require(f.directories[:len(extra)] == extra, "reroot_file_outside_target_mount", f.path)
            g = B.DirFile(f.path, f.directories[len(extra):], f.name, f.chunk_id)
            require(tmount + "/".join(g.directories + (g.name,)) == f.path, "reroot_path_changed", f.path)
            moved.append(g)
        dfiles, mount = moved, tmount
    directory, _ = B.emit_directory_index(mount, root_name,
                                          frozenset({()}), dfiles, row_by_id)
    head = bytearray(toc.header)
    for off, val in ((24, len(ids)), (28, len(blocks) // 12), (48, len(directory)),
                     (52, 1), (84, 0), (96, 0)):
        struct.pack_into("<I", head, off, val)
    struct.pack_into("<Q", head, 88, (1 << 64) - 1)
    utoc = (bytes(head) + b"".join(ids) + bytes(offsets) + bytes(blocks) +
            directory + bytes(metas))
    with open(out_stem + ".utoc", "wb") as f:
        f.write(utoc)
    with open(out_stem + ".ucas", "wb") as f:
        f.write(ucas)
    spec = B.ModSpec(os.path.basename(out_stem), out_stem + ".utoc", out_stem + ".ucas")
    # re-verify every kept chunk payload against the original mod
    with open(mod.ucas_path, "rb") as a, open(out_stem + ".ucas", "rb") as b2:
        sa = mmap.mmap(a.fileno(), 0, access=mmap.ACCESS_READ)
        sb = mmap.mmap(b2.fileno(), 0, access=mmap.ACCESS_READ)
        try:
            for row in keep:
                cid = toc.ids[row]
                want = substitute.get(cid)
                if want is None:
                    want = B.extract_chunk_bytes(toc, row, sa)
                require(want == B.extract_chunk_bytes(spec.toc, spec.toc.ids.index(cid), sb),
                        "projection_payload_mismatch", cid.hex())
        finally:
            sa.close()
            sb.close()
    return spec


# ---------------------------------------------------------------------------
# capability #12: ship DERIVED package bytes (`install --substitute`)
# ---------------------------------------------------------------------------
# A derived-packages manifest (written by derive_substitutes.py from a
# reparent plan + mic_reparent.py outputs) lists, per mod package id, the
# derived package bytes that replace the mod's export chunk in the DERIVED
# overlay, plus the store-entry import repoint.  Every value is checked here
# again, independently of the manifest writer, and every hash has two
# sources (recomputed here + the game's / mod's own data); anything else
# refuses before a single byte is planned.  Mod files stay read-only inputs:
# only the intermediate projected trio (deleted after the build) and the
# derived index/sidecar carry the derived bytes.  Restore is unchanged
# (rename of the original index back), so it stays byte-exact.

SUBST_SCHEMA = "paktx-substitute/1"
KNOWN_DECISIONS = ("allow-partial", "accept-unproven-opaque")
# the reviewed derived-package writer (mod-manager/material-fallback/mic_reparent.py)
MIC_REPARENT_SHA256 = "520a805580a6750e9f799c8740095d4cc5d6f3e3a0a3574e6052914e82c06af5"
# Pinned read-only format readers (integrated copy:
# outputs/2026-10-08-l25-prep/mod-manager/material-fallback/, Team F cap12).
# Independent implementations from overlay_builder's: used as the second
# reader of every hash and structure this feature relies on.
READER_SHA256 = {
    "zen.py": "b3c421d448585ab51a91af7d935ff610633968f11dc258158cae4b3bcc889f4f",
    "cityhash.py": "95955e5872a067d38e024b1f0bad74753842bc24eb79be689afb588898b5167a",
    "blake3.py": "0613633f746a8e4f6c49baee1602f411cbdc1adbb793bf401b2a8e8c17b1355e",
    "container_header.py": "e703f080adf49c9ff5bd90a41b13733b367dafe9f74e0b8306c1e1ddad964459",
}


def default_readers_path() -> str:
    env = os.environ.get("PAKTX_READERS")
    if env:
        return env
    return os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "mod-manager", "material-fallback")


def load_readers(path: str):
    """Exec the pinned readers from verified bytes (no import path, no cache)."""
    out = {}
    for fn, pinned in sorted(READER_SHA256.items()):
        p = os.path.join(path, fn)
        require(os.path.isfile(p), "substitute_reader_missing", p)
        data = open(p, "rb").read()
        got = sha256_bytes(data)
        require(got == pinned, "substitute_reader_sha_mismatch", "%s is %s, pinned %s" % (p, got, pinned))
        mod = types.ModuleType("paktx_reader_" + fn[:-3])
        mod.__file__ = p
        exec(compile(data, p, "exec"), mod.__dict__)
        out[fn[:-3]] = mod
    return types.SimpleNamespace(**out)


def manifest_self_sha(doc: dict) -> str:
    return sha256_bytes(canonical({k: v for k, v in doc.items() if k != "manifest_sha256"}))


def _hex16(v: int) -> str:
    return "%016x" % v


def _pid(text, code: str) -> int:
    require(isinstance(text, str) and re.fullmatch(r"[0-9a-f]{16}", text) is not None, code, repr(text))
    return int(text, 16)


def plan_decisions(row: dict) -> List[str]:
    """User decisions a reparent-plan row needs (the plan, not the manifest,
    is the source: it is what the user reviewed)."""
    out = []
    if row.get("action") == "reparent-partial":
        out.append("allow-partial")
    if (row.get("mod_class") or {}).get("unproven_opaque"):
        out.append("accept-unproven-opaque")
    return sorted(out)


def load_substitute_manifest(path: str, accepted: Sequence[str], plan_path: Optional[str]) -> dict:
    """The manifest is bound to the reviewed reparent plan by sha256; every
    row must equal exactly one plan row; the decisions come from the plan and
    must be restated EXACTLY on the command line."""
    doc = read_json(path)
    require(isinstance(doc, dict) and doc.get("schema") == SUBST_SCHEMA, "substitute_manifest_schema")
    require(manifest_self_sha(doc) == doc.get("manifest_sha256"), "substitute_manifest_self_hash_mismatch")
    require(doc.get("mic_reparent_sha256") == MIC_REPARENT_SHA256, "substitute_mic_reparent_not_pinned",
            "manifest made by mic_reparent %s, pinned %s" % (doc.get("mic_reparent_sha256"),
                                                             MIC_REPARENT_SHA256))
    rows = doc.get("rows")
    require(isinstance(rows, list) and rows, "substitute_manifest_has_zero_rows")
    require(plan_path is not None and os.path.isfile(plan_path), "substitute_needs_reparent_plan",
            "pass --reparent-plan (the plan the user reviewed)")
    pbytes = open(plan_path, "rb").read()
    require(sha256_bytes(pbytes) == (doc.get("source_plan") or {}).get("sha256"),
            "substitute_plan_sha_mismatch", "manifest made from another reparent plan")
    prows = [r for r in json.loads(pbytes).get("rows", [])
             if r.get("action") in ("reparent", "reparent-partial")]
    by_pkg: Dict[str, List[dict]] = {}
    for r in prows:
        by_pkg.setdefault(r.get("mod_package"), []).append(r)
    for d in accepted:
        require(d in KNOWN_DECISIONS, "substitute_user_decision_unknown", d)
    needed = set()
    seen = set()
    for r in rows:
        pid = _pid(r.get("package_id"), "substitute_bad_package_id")
        require(pid not in seen, "substitute_duplicate_package", r.get("package_id"))
        seen.add(pid)
        cands = by_pkg.get(r.get("package"), [])
        require(len(cands) == 1, "substitute_row_not_in_plan",
                "%s: %d matching reparent rows in the plan" % (r.get("package"), len(cands)))
        pr = cands[0]
        rp = r.get("reparent") or {}
        rep_ = pr.get("store_entry_repoint") or {}
        require(r.get("action") == pr.get("action") and
                rp.get("old_parent") == pr.get("old_parent") and
                rp.get("new_parent") == pr.get("new_parent") and
                rp.get("new_export") == pr.get("new_export") and
                sorted(map(tuple, rp.get("renamed_parameters", []))) ==
                sorted((pr.get("param_renames") or {}).items()) and
                rep_.get("package_id") == r.get("package_id"),
                "substitute_row_disagrees_with_plan", r.get("package"))
        dec = plan_decisions(pr)
        require(sorted(r.get("user_decisions", [])) == dec, "substitute_row_decisions_disagree_with_plan",
                r.get("package"))
        needed |= set(dec)
    require(sorted(doc.get("user_decisions", [])) == sorted(needed), "substitute_manifest_decisions_inconsistent")
    missing = sorted(needed - set(accepted))
    require(not missing, "substitute_user_decision_missing",
            "the plan rows need the user's explicit decision(s): %s (pass --accept-decision for each)"
            % ", ".join(missing))
    require(set(accepted) == needed, "substitute_user_decision_not_needed",
            "restate exactly the decisions the rows need: %s" % sorted(needed))
    shipped = {r.get("package") for r in rows}
    doc["_plan_rows_not_shipped"] = sorted(
        ({"package": r.get("mod_package"), "action": r.get("action"),
          "needs": plan_decisions(r)} for r in prows if r.get("mod_package") not in shipped),
        key=lambda x: x["package"] or "")
    doc["_plan_sha256"] = sha256_bytes(pbytes)
    doc["_dir"] = os.path.dirname(os.path.abspath(path))
    doc["_file_sha256"] = sha256_file(path)
    return doc


def _man_file(man: dict, ent: dict, code: str) -> bytes:
    rel = ent.get("path")
    require(isinstance(rel, str) and rel and not os.path.isabs(rel) and ".." not in rel.split("/"),
            code + "_path", repr(rel))
    p = os.path.join(man["_dir"], *rel.split("/"))
    require(os.path.isfile(p), code + "_missing", rel)
    data = open(p, "rb").read()
    require(len(data) == ent.get("bytes") and sha256_bytes(data) == ent.get("sha256"),
            code + "_sha_mismatch", rel)
    return data


def _export_hashes(R, data: bytes, top_level_only: bool = False) -> Dict[str, int]:
    p = R.zen.ZenPackage(data)
    return {e["name"]: e["public_hash"] for e in p.exports
            if not top_level_only or e["outer"][0] == "Null"}


def base_stores(B, R, rec: dict, mirror: str, man: dict) -> Dict[str, dict]:
    """Every base container's package store, each tied to its TOC by IoHash
    (two blake3 implementations) and read by two parsers that must agree."""
    out = {}
    extra = {}
    for ent in man.get("base_stores", []):
        data = _man_file(man, ent, "substitute_base_store")
        extra[ent.get("container")] = data
    for stem, c in sorted(rec["containers"].items()):
        if not c.get("admitted"):
            continue
        toc = rec["_tocs"][stem]
        data = None
        if c.get("header") is not None:
            data = record_header_bytes(rec, stem, mirror, B)
            if stem in extra:
                require(extra[stem] == data, "substitute_base_store_disagrees_with_record", stem)
        elif stem in extra:
            data = extra[stem]
        if data is None:
            continue
        hid = toc.container_id + bytes.fromhex("00000006")
        require(hid in toc.ids, "substitute_base_store_header_row_missing", stem)
        meta = toc.meta(toc.ids.index(hid))[:20]
        require(B.blake3_20(data) == meta and R.blake3.iohash20(data) == meta.hex(),
                "substitute_base_store_iohash_mismatch", stem)
        h1 = B.parse_container_header(data)
        _cid, h2 = R.container_header.parse(data)
        require(set(h1.packages) == set(h2) and
                all(tuple(h1.packages[p].imported_packages) == tuple(h2[p]["imports"]) for p in h2),
                "substitute_base_store_parsers_disagree", stem)
        out[stem] = {"packages": h1.packages, "sha256": sha256_bytes(data), "bytes": len(data)}
    for stem in extra:
        require(stem in out, "substitute_base_store_unknown_container", repr(stem))
    require(out, "substitute_zero_base_stores")
    return out


def _base_export_chunk(B, rec: dict, pid: int):
    hits = []
    want = struct.pack("<Q", pid) + bytes.fromhex("00000001")
    for stem, toc in sorted(rec["_tocs"].items()):
        if want in toc.ids:
            hits.append((stem, toc, toc.ids.index(want)))
    return hits


def _raw_name_batch(d: bytes, pos: int, limit: int):
    """FName batch read with struct only (independent of zen.py): returns
    (names, stored hashes, end).  Only the ASCII form is admitted."""
    require(pos + 4 <= limit, "substitute_raw_names_truncated")
    n = struct.unpack_from("<I", d, pos)[0]
    pos += 4
    if n == 0:
        return [], [], pos
    require(n <= 200000 and pos + 12 + 10 * n <= limit, "substitute_raw_names_truncated")
    nbytes, algo = struct.unpack_from("<IQ", d, pos)
    pos += 12
    require(algo == 0xC1640000, "substitute_raw_names_hash_algorithm")
    hashes = list(struct.unpack_from("<%dQ" % n, d, pos))
    pos += 8 * n
    lens = struct.unpack_from(">%dh" % n, d, pos)
    pos += 2 * n
    names = []
    for ln in lens:
        require(0 <= ln and pos + ln <= limit, "substitute_raw_names_not_ascii_or_truncated")
        raw = d[pos:pos + ln]
        require(all(b < 0x80 for b in raw), "substitute_raw_names_not_ascii_or_truncated")
        names.append(raw.decode("ascii"))
        pos += ln
    require(sum(lens) == nbytes, "substitute_raw_names_byte_count")
    return names, hashes, pos


def raw_header_compare(R, orig: bytes, der: bytes, hslot: int, pslot: int,
                       renames: Dict[str, str]) -> dict:
    """Byte-level comparison of the original and derived package headers with
    struct only: everything outside the name map, the one public-hash slot
    and the one ImportedPackageNames slot must be byte-identical; every
    stored FName hash must follow the law (the original's prove it on the
    cooker's own data); offsets move by exactly the name-map delta."""
    so = struct.unpack_from("<6I9i", orig, 0)
    sd = struct.unpack_from("<6I9i", der, 0)
    require(orig[0:4] == der[0:4] and orig[8:24] == der[8:24], "substitute_raw_summary_changed")
    ho, hd = so[1], sd[1]
    # fields: iph, imp, exp, bundle, dep_h, dep_e, ipn, cimp, cexp; physical order:
    # iph, imp, exp, cimp, cexp, bundle, dep_h, dep_e, ipn (then HeaderSize)
    oo, od = list(so[6:]), list(sd[6:])
    require(all(x > 0 for x in oo) and all(x > 0 for x in od), "substitute_raw_offsets_unsupported")
    phys = (0, 1, 2, 7, 8, 3, 4, 5, 6)
    order_o = [60] + [oo[i] for i in phys] + [ho]
    order_d = [60] + [od[i] for i in phys] + [hd]
    require(order_o == sorted(order_o) and order_d == sorted(order_d), "substitute_raw_offsets_unsupported")
    delta = od[0] - oo[0]
    require(all(b - a == delta for a, b in zip(oo, od)), "substitute_raw_offsets_moved_unequally")
    law64 = R.cityhash.cityhash64
    # name map [60, ImportedPublicExportHashes): batch, u64 pad count, pad, bulk map
    n_o, h_o, e_o = _raw_name_batch(orig, 60, oo[0])
    n_d, h_d, e_d = _raw_name_batch(der, 60, od[0])
    require(len(set(n_d)) == len(n_d), "substitute_raw_name_collision")
    require(len(n_o) == len(n_d) and all(b == renames.get(a, a) for a, b in zip(n_o, n_d)),
            "substitute_name_change_not_declared")
    for names, hashes in ((n_o, h_o), (n_d, h_d)):
        require(all(law64(n.lower().encode()) == h for n, h in zip(names, hashes)),
                "substitute_raw_name_hash_law")
    require(all(a == b for a, b, x in zip(h_o, h_d, n_o) if x not in renames), "substitute_raw_name_hash_changed")

    def tail(d, e, end):
        require(e + 8 <= end, "substitute_raw_bulk_map_truncated")
        pad = struct.unpack_from("<Q", d, e)[0]
        require(pad < 8 and not any(d[e + 8:e + 8 + pad]) and (e + 8 + pad) % 8 == 0,
                "substitute_raw_padding")
        return d[e + 8 + pad:end]
    require(tail(orig, e_o, oo[0]) == tail(der, e_d, od[0]), "substitute_raw_bulk_map_changed")
    # [ImportedPublicExportHashes, ImportedPackageNames): one 8-byte slot may differ
    a, b = orig[oo[0]:oo[6]], der[od[0]:od[6]]
    require(len(a) == len(b), "substitute_raw_maps_resized")
    lo = 8 * hslot
    require(oo[0] + lo + 8 <= oo[1] and a[:lo] == b[:lo] and a[lo + 8:] == b[lo + 8:],
            "substitute_raw_maps_changed")
    # ImportedPackageNames (last header region): one name + its number may differ
    p_o, ph_o, pe_o = _raw_name_batch(orig, oo[6], ho)
    p_d, ph_d, pe_d = _raw_name_batch(der, od[6], hd)
    require(len(p_o) == len(p_d) and pe_o + 4 * len(p_o) == ho and pe_d + 4 * len(p_d) == hd,
            "substitute_raw_package_names_layout")
    require([i for i in range(len(p_o)) if p_o[i] != p_d[i]] == [pslot], "substitute_unexpected_import_package_change")
    for names, hashes in ((p_o, ph_o), (p_d, ph_d)):
        require(all(law64(n.lower().encode()) == h for n, h in zip(names, hashes)),
                "substitute_raw_name_hash_law")
    k_o = struct.unpack_from("<%di" % len(p_o), orig, pe_o)
    k_d = struct.unpack_from("<%di" % len(p_d), der, pe_d)
    require(all(x == y for i, (x, y) in enumerate(zip(k_o, k_d)) if i != pslot),
            "substitute_raw_package_numbers_changed")
    require(der[hd:] == orig[ho:], "substitute_payload_after_header_changed")
    return {"name_map_delta": delta, "header_delta": hd - ho, "names_checked": len(n_o) + len(p_o),
            "identical_map_bytes": len(a) - 8}


def verify_substitutions(B, R, rec: dict, mirror: str, man: dict, mods) -> dict:
    """Checks every manifest row against the mod, the base game and the hash
    laws.  Returns {mod_id: {"chunks": {cid: bytes}, "imports": {pid: tuple}}}
    plus the ledger.  Refuses (nothing planned) on any mismatch."""
    stores = base_stores(B, R, rec, mirror, man)
    base_pids = {}
    for stem, s in stores.items():
        for p in s["packages"]:
            base_pids.setdefault(p, []).append(stem)
    by_sha = {(m.utoc_sha, m.ucas_sha): m for m in mods}
    mm = man.get("mod") or {}
    mod = by_sha.get((mm.get("utoc_sha256"), mm.get("ucas_sha256")))
    require(mod is not None, "substitute_mod_identity_mismatch",
            "the manifest was made for another mod build (utoc/ucas sha256)")
    mstore = B.parse_container_header(B.read_mod_header_bytes(mod))
    _c, mstore2 = R.container_header.parse(B.read_mod_header_bytes(mod))
    require(set(mstore.packages) == set(mstore2), "substitute_mod_store_parsers_disagree")
    mod_stores = {m.mod_id: B.parse_container_header(B.read_mod_header_bytes(m)) for m in mods}
    all_mod_pids = set()
    winner_of: Dict[int, object] = {}
    for m in B.effective_mod_order(mods):  # later wins, as in build_overlay
        all_mod_pids |= set(mod_stores[m.mod_id].packages)
        for p_ in mod_stores[m.mod_id].packages:
            winner_of[p_] = m
    law = R.cityhash.ue_name_hash
    headers = {}
    for ent in man.get("base_headers", []):
        pid = _pid(ent.get("package_id"), "substitute_bad_package_id")
        data = _man_file(man, ent, "substitute_base_header")
        hits = _base_export_chunk(B, rec, pid)
        require(len(hits) == 1, "substitute_base_header_not_in_base_toc",
                "%s in %d base TOCs" % (ent.get("package_id"), len(hits)))
        stem, toc, row = hits[0]
        meta = toc.meta(row)[:20]
        require(toc.offset_length(row)[1] == len(data), "substitute_base_header_not_whole_chunk",
                ent.get("package_id"))
        require(B.blake3_20(data) == meta and R.blake3.iohash20(data) == meta.hex(),
                "substitute_base_header_iohash_mismatch", ent.get("package_id"))
        try:
            q = R.zen.ZenPackage(data)
        except R.zen.ZenError as e:
            raise Refused("substitute_base_header_unparsable", str(e))
        require(law(q.name) == pid, "substitute_base_header_wrong_package", q.name)
        require(stem == ent.get("container"), "substitute_base_header_container_mismatch", stem)
        headers[pid] = {"pkg": q, "container": stem, "sha256": sha256_bytes(data), "iohash": meta.hex()}
    mrows = {cid: i for i, cid in enumerate(mod.toc.ids)}
    chunks, imports, ledger = {}, {}, []
    mod_targets: Dict[int, set] = {}
    with open(mod.ucas_path, "rb") as fh:
        store = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
        try:
            originals = {}
            for r in man["rows"]:
                pid = int(r["package_id"], 16)
                require(pid in mstore.packages, "substitute_package_not_in_mod", r["package_id"])
                cid = struct.pack("<Q", pid) + bytes.fromhex("00000001")
                require(r.get("chunk_id") == cid.hex() and cid in mrows, "substitute_chunk_missing",
                        r["package_id"])
                orig = B.extract_chunk_bytes(mod.toc, mrows[cid], store)
                require(B.blake3_20(orig) == mod.toc.meta(mrows[cid])[:20] and
                        R.blake3.iohash20(orig) == mod.toc.meta(mrows[cid])[:20].hex(),
                        "substitute_original_iohash_mismatch", r["package_id"])
                o = r.get("original") or {}
                require(sha256_bytes(orig) == o.get("sha256") and len(orig) == o.get("bytes"),
                        "substitute_original_mismatch", r["package_id"])
                originals[pid] = orig
        finally:
            store.close()
    derived_by_pid = {}
    for r in man["rows"]:
        pid = int(r["package_id"], 16)
        derived_by_pid[pid] = _man_file(man, r.get("derived") or {}, "substitute_derived")

    def target_pkg(tpid: int):
        """Header of an import target: the package that WINS for that id
        (derived bytes if substituted, else the winning mod's own chunk), or a
        verified base header when no mod provides it; None if unknown."""
        if tpid in derived_by_pid:
            return R.zen.ZenPackage(derived_by_pid[tpid])
        wm = winner_of.get(tpid)
        if wm is not None:
            tc = struct.pack("<Q", tpid) + bytes.fromhex("00000001")
            if tc not in wm.toc.ids:
                return None
            with open(wm.ucas_path, "rb") as fh2:
                st = mmap.mmap(fh2.fileno(), 0, access=mmap.ACCESS_READ)
                try:
                    return R.zen.ZenPackage(B.extract_chunk_bytes(wm.toc, wm.toc.ids.index(tc), st))
                finally:
                    st.close()
        h = headers.get(tpid)
        return h["pkg"] if h else None

    for r in man["rows"]:
        pid = int(r["package_id"], 16)
        orig, der = originals[pid], derived_by_pid[pid]
        try:
            p = R.zen.ZenPackage(orig)
            q = R.zen.ZenPackage(der)
        except R.zen.ZenError as e:
            raise Refused("substitute_derived_unparsable", "%s: %s" % (r["package_id"], e))
        rp = r.get("reparent") or {}
        require(p.name == q.name == r.get("package") and law(q.name) == pid,
                "substitute_identity_changed", r["package_id"])
        require(len(p.exports) == len(q.exports) and
                all({k: v for k, v in a.items() if k != "data_offset"} ==
                    {k: v for k, v in b.items() if k != "data_offset"}
                    for a, b in zip(p.exports, q.exports)),
                "substitute_export_map_changed", r["package_id"])
        require(all(p.export_bytes(a) == q.export_bytes(b) for a, b in zip(p.exports, q.exports)),
                "substitute_export_bytes_changed", r["package_id"])
        require(q.cooked_header_size == p.cooked_header_size and q.flags == p.flags,
                "substitute_summary_changed", r["package_id"])
        require(len(der) - q.header_size == len(orig) - p.header_size and
                der[q.header_size:] == orig[p.header_size:],
                "substitute_payload_after_header_changed", r["package_id"])
        require(p.imports == q.imports, "substitute_import_map_changed", r["package_id"])
        # name map: only the declared parameter renames
        ren = {a: b for a, b in rp.get("renamed_parameters", [])}
        require(len(p.names) == len(q.names) and
                all(b == ren.get(a, a) for a, b in zip(p.names, q.names)) and
                all(a in p.names for a in ren),
                "substitute_name_change_not_declared", r["package_id"])
        # imported package names: exactly the parent slot changes
        diff_pk = [i for i, (a, b) in enumerate(zip(p.imported_packages, q.imported_packages)) if a != b]
        require(len(p.imported_packages) == len(q.imported_packages) and len(diff_pk) == 1 and
                p.imported_packages[diff_pk[0]] == rp.get("old_parent") and
                q.imported_packages[diff_pk[0]] == rp.get("new_parent") and
                diff_pk[0] == rp.get("parent_import_slot"),
                "substitute_unexpected_import_package_change", r["package_id"])
        slot = diff_pk[0]
        # public export hash slot: exactly the one used by the parent import
        uses = [v[1] for k, v in q.imports if k == "PackageImport" and v[0] == slot]
        require(len(uses) == 1 and uses[0] == rp.get("public_hash_slot"), "substitute_parent_import_ambiguous",
                r["package_id"])
        hslot = uses[0]
        diff_h = [i for i, (a, b) in enumerate(zip(p.import_hashes, q.import_hashes)) if a != b]
        require(len(p.import_hashes) == len(q.import_hashes) and diff_h == [hslot],
                "substitute_unexpected_public_hash_change", r["package_id"])
        require(_hex16(p.import_hashes[hslot]) == rp.get("old_public_hash"),
                "substitute_old_public_hash_disagrees", r["package_id"])
        raw = raw_header_compare(R, orig, der, hslot, slot, ren)
        new_hash = q.import_hashes[hslot]
        new_export = rp.get("new_export")
        require(isinstance(new_export, str) and law(new_export) == new_hash and
                _hex16(new_hash) == rp.get("new_public_hash"),
                "substitute_public_hash_law_mismatch", r["package_id"])
        to_pid = law(rp["new_parent"])
        from_pid = law(rp["old_parent"])
        h = headers.get(to_pid)
        require(h is not None, "substitute_base_header_missing", _hex16(to_pid))
        tops = [e for e in h["pkg"].exports if e["outer"][0] == "Null" and e["name"] == new_export]
        require(len(tops) == 1 and tops[0]["public_hash"] == new_hash, "substitute_base_export_hash_mismatch",
                "%s: %d top-level exports named %s" % (rp["new_parent"], len(tops), new_export))
        # store entry: slot order == header import names (cooker's own data), one slot repointed
        entry = mstore.packages[pid]
        require(not entry.shader_map_hashes and not mstore2[pid]["shader_maps"],
                "substitute_entry_has_own_shader_maps",
                "%s: its own (mod) shader maps would still be used; reparenting cannot fix it" % r["package"])
        require(sum(1 for m in mods if pid in mod_stores[m.mod_id].packages) == 1,
                "substitute_package_in_several_mods", r["package_id"])
        before = tuple(entry.imported_packages)
        require(before == tuple(law(n) for n in p.imported_packages) and
                tuple(mstore2[pid]["imports"]) == before,
                "substitute_store_order_mismatch", r["package_id"])
        after = tuple(law(n) for n in q.imported_packages)
        se = r.get("store_entry") or {}
        require([_hex16(v) for v in before] == se.get("imports_before") and
                [_hex16(v) for v in after] == se.get("imports_after") and
                before[slot] == from_pid and after[slot] == to_pid and
                [i for i in range(len(before)) if before[i] != after[i]] == [slot],
                "substitute_store_imports_disagree", r["package_id"])
        require(to_pid in base_pids, "substitute_parent_not_in_base_store", _hex16(to_pid))
        require(h["container"] in base_pids[to_pid], "substitute_parent_store_container_mismatch",
                _hex16(to_pid))
        require(to_pid not in all_mod_pids, "substitute_parent_overridden_by_mod", _hex16(to_pid))
        # one level further: the base parent's own imports are base packages
        pstem = h["container"]
        pimps = stores[pstem]["packages"][to_pid].imported_packages
        require(all(i in base_pids for i in pimps), "substitute_parent_imports_unresolved",
                _hex16(to_pid))
        require(tuple(pimps) == tuple(law(n) for n in h["pkg"].imported_packages),
                "substitute_parent_store_order_mismatch", _hex16(to_pid))
        # export level: every package import of the derived package resolves
        resolved = 0
        for k, v in q.imports:
            if k != "PackageImport":
                continue
            ti, hi = v
            require(ti < len(q.imported_packages) and hi < len(q.import_hashes),
                    "substitute_import_index_out_of_range", r["package_id"])
            tpid = law(q.imported_packages[ti])
            require(tpid == after[ti], "substitute_store_order_mismatch", r["package_id"])
            tp = target_pkg(tpid)
            require(tp is not None, "substitute_import_unresolvable",
                    "%s: no header for %s" % (r["package"], q.imported_packages[ti]))
            require(q.import_hashes[hi] in {e["public_hash"] for e in tp.exports},
                    "substitute_import_unresolvable",
                    "%s: %s exports no %016x" % (r["package"], q.imported_packages[ti], q.import_hashes[hi]))
            if tpid not in all_mod_pids:
                require(tpid in base_pids, "substitute_import_unresolvable",
                        "%s not in any base store" % q.imported_packages[ti])
            else:
                mod_targets.setdefault(pid, set()).add(tpid)
            resolved += 1
        require(resolved > 0, "substitute_zero_imports_checked", r["package_id"])
        dhash = B.blake3_20(der)
        require(R.blake3.iohash20(der) == dhash.hex() and (r.get("derived") or {}).get("iohash") == dhash.hex(),
                "substitute_derived_iohash_mismatch", r["package_id"])
        cid = struct.pack("<Q", pid) + bytes.fromhex("00000001")
        chunks[cid] = der
        imports[pid] = after
        ledger.append({
            "package": r["package"], "package_id": r["package_id"], "chunk_id": cid.hex(),
            "action": r.get("action"), "user_decisions": sorted(r.get("user_decisions", [])),
            "original": {"bytes": len(orig), "sha256": sha256_bytes(orig),
                         "iohash": B.blake3_20(orig).hex()},
            "derived": {"bytes": len(der), "sha256": sha256_bytes(der), "iohash": dhash.hex()},
            "old_parent": rp["old_parent"], "new_parent": rp["new_parent"], "new_export": new_export,
            "new_public_hash": _hex16(new_hash),
            "new_public_hash_sources": "cityhash law + base export map (header IoHash-tied to %s TOC)"
                                       % h["container"],
            "new_parent_package_id": _hex16(to_pid),
            "new_parent_package_id_sources": "cityhash law + base store %s + base TOC chunk id"
                                             % ",".join(base_pids[to_pid]),
            "store_imports_before": [_hex16(v) for v in before],
            "store_imports_after": [_hex16(v) for v in after],
            "renamed_parameters": [list(x) for x in rp.get("renamed_parameters", [])],
            "imports_resolved_export_level": resolved,
            "raw_header_check": raw,
            "parent_imports_in_base_stores": len(pimps),
            "parent_imports_overridden_by_mod": sorted(_hex16(i) for i in pimps if i in all_mod_pids),
            "base_header": {"container": h["container"], "sha256": h["sha256"], "iohash": h["iohash"]},
        })
    return {"mod": mod, "chunks": chunks, "imports": imports, "ledger": ledger,
            "mod_targets": mod_targets,
            "base_stores": {s: {"sha256": v["sha256"], "bytes": v["bytes"], "packages": len(v["packages"])}
                            for s, v in sorted(stores.items())},
            "base_pids": base_pids}


def _in_sidecar(toc, row: int, base_ucas_size: int, what: str) -> None:
    """Every block of the row lies in the sidecar partition (offset >= the base
    .ucas size), so _OffsetView never wraps into a wrong slice."""
    off, ln = toc.offset_length(row)
    first, last = off // toc.block_size, (off + ln - 1) // toc.block_size
    require(ln > 0 and all(toc.block(i)[0] >= base_ucas_size for i in range(first, last + 1)),
            "substitute_row_not_in_sidecar", what)


def post_build_checks(B, R, subs: dict, dest: str, derived: bytes, sidecar: bytes,
                      base_ucas_size: int, mods_in_dest) -> dict:
    """Independent re-read of what will ship: the derived index re-parses,
    each substituted chunk extracts as the derived bytes with a matching
    IoHash (two blake3 implementations), the merged store (two parsers) holds
    the repointed entry, and every import of every mod entry resolves to a
    package of the merged store or another base store (package level)."""
    dtoc = B.parse_toc(derived, strict_admission=False)
    B.validate_lookup(dtoc)
    view = B._OffsetView(sidecar, base_ucas_size)
    hid = dtoc.container_id + bytes.fromhex("00000006")
    require(hid in dtoc.ids, "substitute_merged_header_row_missing", dest)
    _in_sidecar(dtoc, dtoc.ids.index(hid), base_ucas_size, "merged header")
    mh = B.extract_chunk_bytes(dtoc, dtoc.ids.index(hid), view)
    require(B.blake3_20(mh) == dtoc.meta(dtoc.ids.index(hid))[:20], "substitute_merged_header_iohash", dest)
    h1 = B.parse_container_header(mh)
    _c, h2 = R.container_header.parse(mh)
    require(set(h1.packages) == set(h2) and
            all(tuple(h1.packages[p].imported_packages) == tuple(h2[p]["imports"]) for p in h2),
            "substitute_merged_header_parsers_disagree", dest)
    checked_chunks = 0
    for cid, der in sorted(subs["chunks"].items()):
        pid = B.package_id_of_chunk(cid)
        if pid not in h1.packages or pid not in subs["dest_pids"].get(dest, set()):
            continue
        require(cid in dtoc.ids, "substitute_derived_row_missing", cid.hex())
        row = dtoc.ids.index(cid)
        _in_sidecar(dtoc, row, base_ucas_size, cid.hex())
        got = B.extract_chunk_bytes(dtoc, row, view)
        require(got == der, "substitute_derived_payload_mismatch", cid.hex())
        meta = dtoc.meta(row)[:20]
        require(meta == B.blake3_20(der) and R.blake3.iohash20(der) == meta.hex(),
                "substitute_derived_iohash_mismatch", cid.hex())
        require(tuple(h1.packages[pid].imported_packages) == tuple(subs["imports"][pid]),
                "substitute_merged_store_entry_mismatch", cid.hex())
        checked_chunks += 1
    require(checked_chunks == len(subs["dest_pids"].get(dest, set())), "substitute_rows_not_all_shipped",
            dest)
    known = set(h1.packages) | set(subs["base_pids"])
    mod_pids = set()
    for m in mods_in_dest:
        mod_pids |= set(m)
    here = subs["dest_pids"].get(dest, set())
    # substituted packages: every store import must name a known package
    bad = sorted({_hex16(i) for p in here for i in h1.packages[p].imported_packages if i not in known})
    require(not bad, "substitute_import_package_unresolved",
            "%s: substituted entries import unknown packages %s" % (dest, bad[:8]))
    # every other mod entry is the mod's own, unchanged by #12: unknown imports
    # there are the mod's (also in the plain install); listed, not introduced
    pre = {}
    for p in sorted(mod_pids - here):
        miss = [_hex16(i) for i in h1.packages[p].imported_packages if i not in known]
        if miss:
            pre[_hex16(p)] = miss
    n_sub = sum(len(h1.packages[p].imported_packages) for p in here)
    n_all = sum(len(h1.packages[p].imported_packages) for p in mod_pids)
    return {"container": dest, "derived_index_reparsed": True, "lookup_validated": True,
            "substituted_chunks_verified": checked_chunks,
            "substituted_store_imports_resolved": n_sub,
            "mod_store_entries_checked": len(mod_pids),
            "mod_store_imports_checked": n_all,
            "mod_store_imports_resolved": n_all - sum(len(v) for v in pre.values()),
            "unchanged_mod_entries_with_unknown_imports": pre,
            "merged_header_sha256": sha256_bytes(mh)}


# ---------------------------------------------------------------------------
# classic .pak base rebuild (pinned repak, config-trial command pattern)
# ---------------------------------------------------------------------------

def _repak(repak: str, *args) -> str:
    # one worker: repak's parallel pack order is otherwise nondeterministic
    # (observed on the host); the 2026-10-01 config trial pinned this too
    env = dict(os.environ, RAYON_NUM_THREADS="1")
    proc = subprocess.run([repak, *args], capture_output=True, text=True, timeout=600, env=env)
    require(proc.returncode == 0, "repak_failed", " ".join(args[:1]) + ": " + proc.stderr[-800:])
    return proc.stdout


def _hash_list(repak: str, pak: str, mount: str) -> Dict[str, str]:
    out = {}
    for line in _repak(repak, "hash-list", pak, "--strip-prefix", mount).splitlines():
        line = line.strip()
        if not line:
            continue
        digest, _, path = line.partition(" ")
        require(re.fullmatch(r"[0-9a-f]{64}", digest) is not None, "repak_hash_list_unparsed", line[:120])
        out[path.strip()] = digest
    return out


def rebuild_pak(B, repak: str, base_pak: str, overrides: Dict[str, str], out_pak: str,
                work: str) -> dict:
    """overrides: entry path (mount-relative) -> local file with new bytes."""
    data = open(base_pak, "rb").read()
    info = B.parse_pak(data)
    del data
    mount, seed = info["mount"], info["path_hash_seed"]
    lower = {}
    for p in info["entries"]:
        require(p.lower() not in lower, "base_pak_case_only_alias", p)
        lower[p.lower()] = p
    for p in overrides:
        require(p.lower() not in lower or lower[p.lower()] == p, "override_case_only_alias", p)
    before = _hash_list(repak, base_pak, mount)
    require(set(before) == set(info["entries"]), "repak_and_reader_disagree_on_entries")
    tree = os.path.join(work, "pak-tree")
    require(not os.path.exists(tree), "work_tree_exists", tree)
    _repak(repak, "unpack", base_pak, "--output", tree, "--strip-prefix", mount, "--quiet")
    for path, src in sorted(overrides.items()):
        dst = os.path.join(tree, *path.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
    _repak(repak, "pack", tree, out_pak, "--version", "V11", "--mount-point", mount,
           "--path-hash-seed", str(seed), "--quiet")
    again = out_pak + ".determinism-check"
    _repak(repak, "pack", tree, again, "--version", "V11", "--mount-point", mount,
           "--path-hash-seed", str(seed), "--quiet")
    require(sha256_file(again) == sha256_file(out_pak), "repak_pack_not_deterministic")
    os.remove(again)
    after = _hash_list(repak, out_pak, mount)
    expected = dict(before)
    for path, src in overrides.items():
        expected[path] = sha256_file(src)
    require(set(after) == set(expected), "rebuilt_pak_entry_set_mismatch")
    changed = sorted(p for p in after if after[p] != before.get(p))
    for p, d in after.items():
        require(d == expected[p], "rebuilt_pak_entry_bytes_mismatch", p)
    second = B.parse_pak(open(out_pak, "rb").read())
    require(second["mount"] == mount and second["path_hash_seed"] == seed and
            second["entry_count"] == len(expected), "rebuilt_pak_reader_check_failed")
    shutil.rmtree(tree)
    return {"mount": mount, "path_hash_seed": seed, "entries": len(expected),
            "changed_entries": changed, "unchanged_entries_verified": len(expected) - len(changed)}


# ---------------------------------------------------------------------------
# planning
# ---------------------------------------------------------------------------

def _names(paks_dir: str, live: str, nonce: str) -> dict:
    return {"backup": live + ".original-" + nonce + ".bak",
            "temp": live + ".paktx-" + nonce + ".tmp",
            "retired": live + ".derived-" + nonce + ".retired"}


def make_install_plan(B, rec: dict, mirror: str, mod_dir: str, work: str,
                      new_into: Optional[str] = None,
                      repak: Optional[str] = None,
                      tool_sha: str = "",
                      only_into: Optional[str] = None,
                      substitute: Optional[str] = None,
                      accepted: Sequence[str] = (),
                      readers: Optional[str] = None,
                      reparent_plan: Optional[str] = None) -> dict:
    """Deterministic plan.  Writes derived artifacts under work/targets/.
    `substitute`: a derived-packages manifest (capability #12); `accepted`:
    the user decisions restated on the command line; `readers`: folder of
    the pinned second-source readers."""
    os.makedirs(work, exist_ok=True)
    # mirror must equal the record before anything is derived from it
    gate_local_mirror(rec, mirror)
    rec["_tocs"] = {s: B.parse_toc(open(os.path.join(mirror, c["utoc"]), "rb").read())
                    for s, c in rec["containers"].items() if c.get("admitted")}
    inv = inventory_mod(B, mod_dir)
    mods = inv["iostore"]
    man = None
    if substitute is not None:
        man = load_substitute_manifest(substitute, accepted, reparent_plan)
        require(mods, "substitute_needs_an_iostore_mod")
    nonce_parts = [rec["record_sha256"], tool_sha, B.PAKTX_SHA256,
                   *sorted("%s:%s" % (f["name"], f["sha256"]) for f in inv["files"]),
                   new_into or "", "only_into=" + (only_into or "")]
    if man is not None:
        nonce_parts.append("substitute=" + man["manifest_sha256"])
    nonce = nonce_from(*nonce_parts)
    targets = []
    own = None
    skipped = None
    subs = None
    sub_checks = []
    if mods:
        own = ownership(B, rec, mods, new_into)
        if man is not None:
            R = load_readers(readers or default_readers_path())
            subs = verify_substitutions(B, R, rec, mirror, man, mods)
        if only_into is not None:
            # diagnostic fallback (one container only, the console-proven shape):
            # packages owned by any other container are NOT installed (listed)
            require(only_into in own["groups"], "only_into_is_not_a_destination",
                    "%s not in %s" % (only_into, sorted(own["groups"])))
            skipped = {d: own["summary"][d] for d in sorted(own["groups"]) if d != only_into}
            own["groups"] = {only_into: own["groups"][only_into]}
        if subs is not None:
            smid = subs["mod"].mod_id
            subs["dest_pids"] = {}
            for dest, mm_ in own["groups"].items():
                got = set(mm_.get(smid, set())) & set(subs["imports"])
                if got:
                    subs["dest_pids"][dest] = got
            shipped = set().union(*subs["dest_pids"].values()) if subs["dest_pids"] else set()
            require(shipped == set(subs["imports"]), "substitute_package_not_installed",
                    "not installed by this plan: %s" % sorted(_hex16(p) for p in
                                                             set(subs["imports"]) - shipped))
            installed = set()
            for mm_ in own["groups"].values():
                for ps_ in mm_.values():
                    installed |= set(ps_)
            lost = sorted({_hex16(t) for ts in subs["mod_targets"].values() for t in ts} -
                          {_hex16(t) for t in installed})
            require(not lost, "substitute_import_target_not_installed",
                    "mod packages the derived packages import are not in this plan: %s" % lost[:8])
        by_id = {m.mod_id: m for m in mods}
        # fail fast: every destination's ContainerHeader must be available
        headers = {dest: record_header_bytes(rec, dest, mirror, B) for dest in sorted(own["groups"])}
        for dest in sorted(own["groups"]):
            c = rec["containers"][dest]
            tdir = os.path.join(work, "targets", dest)
            if os.path.isdir(tdir):
                shutil.rmtree(tdir)
            os.makedirs(tdir)
            specs = []
            base_utoc = open(os.path.join(mirror, c["utoc"]), "rb").read()
            btoc = B.parse_toc(base_utoc, strict_admission=False)
            bdir = B.parse_directory_index(btoc.directory_index, btoc.ids, "base")
            for mid, pids in sorted(own["groups"][dest].items()):
                mod = by_id[mid]
                header = B.parse_container_header(B.read_mod_header_bytes(mod))
                same_mount = mod.directory["mount_point"] == bdir["mount_point"]
                sub_here = (subs is not None and mid == subs["mod"].mod_id and
                            bool(subs["dest_pids"].get(dest)))
                if set(pids) == set(header.packages) and same_mount and not sub_here:
                    specs.append(mod)  # whole mod: the proven unprojected shape
                elif sub_here:
                    here = subs["dest_pids"][dest]
                    specs.append(project_mod(
                        B, mod, set(pids), os.path.join(tdir, "proj-" + mid),
                        target_dir=None if same_mount else bdir,
                        substitute={c: d for c, d in subs["chunks"].items()
                                    if B.package_id_of_chunk(c) in here},
                        store_imports={p: subs["imports"][p] for p in here}))
                else:
                    specs.append(project_mod(B, mod, set(pids),
                                             os.path.join(tdir, "proj-" + mid),
                                             target_dir=None if same_mount else bdir))
            for spec in specs:
                require(spec.toc.block_size == c["block_size"], "mod_block_size_differs_from_base",
                        "%s %d vs %s %d" % (spec.mod_id, spec.toc.block_size, dest, c["block_size"]))
            hdr = headers[dest]
            derived, sidecar, receipt = B.build_overlay(base_utoc, c["ucas_bytes"], hdr,
                                                        specs, conflict_policy="reject")
            # independent spot checks on the derived index
            dtoc = B.parse_toc(derived, strict_admission=False)
            B.validate_lookup(dtoc)
            for spec in specs:
                for cid in spec.toc.ids:
                    if cid == spec.header_chunk_id:
                        continue
                    require(cid in dtoc.ids, "derived_index_missing_mod_chunk", cid.hex())
            if subs is not None and subs["dest_pids"].get(dest):
                sub_checks.append(post_build_checks(
                    B, R, subs, dest, derived, sidecar, c["ucas_bytes"],
                    [pids for _mid, pids in sorted(own["groups"][dest].items())]))
            dpath = os.path.join(tdir, c["utoc"])
            spath = os.path.join(tdir, dest + "_s1.ucas")
            with open(dpath, "wb") as f:
                f.write(derived)
            with open(spath, "wb") as f:
                f.write(sidecar)
            for f in os.listdir(tdir):
                if f.startswith("proj-"):
                    os.remove(os.path.join(tdir, f))
            require(dest + "_s1.ucas" not in rec["files"], "sidecar_name_is_a_game_file", dest)
            n = _names(rec["paks_dir"], c["utoc"], nonce)
            targets.append({
                "kind": "iostore", "container": dest,
                "live": {"name": c["utoc"], "bytes": rec["files"][c["utoc"]]["bytes"],
                         "sha256": rec["files"][c["utoc"]]["sha256"]},
                "derived": {"local": dpath, "bytes": len(derived), "sha256": sha256_bytes(derived)},
                "sidecar": {"local": spath, "name": dest + "_s1.ucas", "bytes": len(sidecar),
                            "sha256": sha256_bytes(sidecar),
                            "temp": dest + "_s1.ucas.paktx-" + nonce + ".tmp"},
                "backup": n["backup"], "temp": n["temp"], "retired": n["retired"],
                "base_ucas": {"name": c["ucas"], "bytes": c["ucas_bytes"]},
                "packages": own["summary"][dest],
                "build_receipt": {k: receipt[k] for k in ("counts", "output_utoc_sha256",
                                                          "output_sidecar_sha256",
                                                          "output_sidecar_bytes")},
            })
    if inv["paks"]:
        targets.extend(_plan_pak_targets(B, rec, mirror, inv["paks"], work, repak, nonce))
    require(targets, "plan_has_zero_targets")
    if subs is not None:
        require(len(sub_checks) == len(subs["dest_pids"]) and
                sum(x["substituted_chunks_verified"] for x in sub_checks) == len(subs["ledger"]),
                "substitute_post_build_check_count_mismatch")
        for t in targets:
            if t["kind"] == "iostore":
                t["substituted_packages"] = sorted(_hex16(p) for p in
                                                   subs["dest_pids"].get(t["container"], set()))
    plan = {
        "schema": PLAN_SCHEMA, "kind": "install", "tool": TOOL, "tool_sha256": tool_sha,
        "builder_sha256": B.PAKTX_SHA256, "nonce": nonce,
        "record": {"record_sha256": rec["record_sha256"], "title_id": rec["title_id"],
                   "version": rec["version"], "paks_dir": rec["paks_dir"]},
        "mod": {"files": inv["files"], "ignored": inv["ignored"]},
        "ownership": None if own is None else {"primary": own["primary"],
                                               "primary_rule": own["primary_rule"]},
        "only_into": only_into, "skipped_by_only_into": skipped,
        "gate": gate_spec(rec),
        "targets": targets,
    }
    if subs is not None:
        plan["substitution"] = {
            "manifest_sha256": man["manifest_sha256"],
            "manifest_file_sha256": man["_file_sha256"],
            "user_decisions": sorted(man["user_decisions"]),
            "reparent_plan_sha256": man["_plan_sha256"],
            "plan_rows_not_shipped": man["_plan_rows_not_shipped"],
            "decisions_restated_on_command_line": sorted(set(accepted)),
            "readers_sha256": dict(READER_SHA256),
            "rows": subs["ledger"],
            "base_stores": subs["base_stores"],
            "post_build": sub_checks,
            "law": "mod files read-only; derived bytes only in the derived index/sidecar; "
                   "restore = original index back by rename",
        }
    plan["operations"] = install_operations(plan)
    plan["plan_sha256"] = plan_sha(plan)
    return plan


def _plan_pak_targets(B, rec, mirror, mod_paks, work, repak, nonce):
    base_paks = {}
    for name, f in rec["files"].items():
        if name.lower().endswith(".pak") and f["bytes"] > 0:
            try:
                base_paks[name] = B.parse_pak(open(os.path.join(mirror, name), "rb").read())
            except B.Refused:
                continue
    require(base_paks, "no_parseable_base_pak")
    owner_by_full = {}
    for name, info in base_paks.items():
        for p in info["entries"]:
            owner_by_full.setdefault((info["mount"] + p).lower(), []).append((name, p))
    assign: Dict[str, Dict[str, str]] = {}
    newfiles = []
    extract_root = os.path.join(work, "mod-pak-extract")
    if os.path.isdir(extract_root):
        shutil.rmtree(extract_root)
    os.makedirs(extract_root)
    for mp in mod_paks:
        info = mp["pak"]
        data = open(mp["path"], "rb").read()
        for p, e in sorted(info["entries"].items()):
            full = (info["mount"] + p).lower()
            require(not p.lower().endswith(COOKED_EXT), "cooked_classic_pak_assets_need_conversion",
                    "%s: %s (conversion is not part of step 1)" % (mp["name"], p))
            local = os.path.join(extract_root, mp["name"], *p.split("/"))
            os.makedirs(os.path.dirname(local), exist_ok=True)
            with open(local, "wb") as f:
                f.write(data[e.offset:e.offset + e.size])
            owners = owner_by_full.get(full, [])
            require(len(owners) <= 1, "loose_file_owned_by_multiple_base_paks", p)
            if owners:
                assign.setdefault(owners[0][0], {})[owners[0][1]] = local
            else:
                newfiles.append((info["mount"], p, local))
    if newfiles:
        require(assign, "new_loose_files_without_primary_pak")
        best = max(len(v) for v in assign.values())
        top = sorted(k for k, v in assign.items() if len(v) == best)
        require(len(top) == 1, "ambiguous_primary_pak", str(top))
        pinfo = base_paks[top[0]]
        for mount, p, local in newfiles:
            full = mount + p
            require(full.startswith(pinfo["mount"]), "new_loose_file_outside_primary_mount", p)
            assign[top[0]][full[len(pinfo["mount"]):]] = local
    repak = check_repak(repak)
    out = []
    for name in sorted(assign):
        tdir = os.path.join(work, "targets", name)
        if os.path.isdir(tdir):
            shutil.rmtree(tdir)
        os.makedirs(tdir)
        dpath = os.path.join(tdir, name)
        info = rebuild_pak(B, repak, os.path.join(mirror, name), assign[name], dpath, tdir)
        n = _names(rec["paks_dir"], name, nonce)
        out.append({"kind": "pak", "container": name,
                    "live": {"name": name, "bytes": rec["files"][name]["bytes"],
                             "sha256": rec["files"][name]["sha256"]},
                    "derived": {"local": dpath, "bytes": os.path.getsize(dpath),
                                "sha256": sha256_file(dpath)},
                    "sidecar": None, "backup": n["backup"], "temp": n["temp"],
                    "retired": n["retired"], "pak_rebuild": info})
    shutil.rmtree(extract_root)
    return out


def gate_spec(rec: dict) -> dict:
    return {"files": {n: {"bytes": f["bytes"], "sha256": f["sha256"]}
                      for n, f in sorted(rec["files"].items())},
            "unrecorded_mountable": "refuse"}


def install_operations(plan: dict) -> List[dict]:
    ops = [{"op": "gate"}]
    owned = []
    for t in plan["targets"]:
        owned += [t["backup"], t["temp"], t["retired"]]
        if t["sidecar"]:
            owned += [t["sidecar"]["name"], t["sidecar"]["temp"]]
    ops.append({"op": "absent", "names": sorted(owned)})
    for t in plan["targets"]:  # phase A: uploads only, nothing live changes
        if t["sidecar"]:
            ops.append({"op": "upload", "local": t["sidecar"]["local"], "remote": t["sidecar"]["temp"],
                        "bytes": t["sidecar"]["bytes"], "sha256": t["sidecar"]["sha256"]})
        ops.append({"op": "upload", "local": t["derived"]["local"], "remote": t["temp"],
                    "bytes": t["derived"]["bytes"], "sha256": t["derived"]["sha256"]})
    for t in plan["targets"]:  # phase B: per target rename sequence
        if t["sidecar"]:
            ops.append({"op": "rename", "src": t["sidecar"]["temp"], "dst": t["sidecar"]["name"],
                        "bytes": t["sidecar"]["bytes"]})
        ops.append({"op": "rename", "src": t["live"]["name"], "dst": t["backup"],
                    "bytes": t["live"]["bytes"]})
        ops.append({"op": "rename", "src": t["temp"], "dst": t["live"]["name"],
                    "bytes": t["derived"]["bytes"]})
    for t in plan["targets"]:  # phase C: verify by full re-read
        ops.append({"op": "verify", "remote": t["live"]["name"], "bytes": t["derived"]["bytes"],
                    "sha256": t["derived"]["sha256"]})
        ops.append({"op": "verify", "remote": t["backup"], "bytes": t["live"]["bytes"],
                    "sha256": t["live"]["sha256"]})
    return ops


# ---------------------------------------------------------------------------
# backends
# ---------------------------------------------------------------------------

class LocalBackend:
    """A folder standing in for the console paks folder (tests, Mac mirrors)."""

    def __init__(self, root: str, fail_after: Optional[int] = None,
                 fail_after_change: Optional[int] = None):
        self.root = os.path.abspath(root)
        self.mutations = 0
        self.attempted = 0  # mutations started (the console may have changed)
        self.fail_after = fail_after                # lost reply BEFORE mutation k
        self.fail_after_change = fail_after_change  # lost reply AFTER mutation k

    def _p(self, rel: str, listing: bool = False) -> str:
        parts = _check_rel(rel, listing)
        if parts[0] == "..":
            return os.path.join(os.path.dirname(self.root), *parts[1:])
        return os.path.join(self.root, *parts)

    def _tick(self):
        if self.fail_after is not None and self.mutations >= self.fail_after:
            raise IOError("injected_failure_before_mutation_%d" % self.mutations)
        self.mutations += 1
        self.attempted += 1

    def _after(self):
        if self.fail_after_change is not None and self.mutations - 1 == self.fail_after_change:
            raise IOError("injected_lost_reply_after_mutation_%d" % (self.mutations - 1))

    def listing(self, rel_dir: str = "", strict: bool = False) -> Dict[str, Tuple[str, int]]:
        d = self._p(rel_dir, listing=True) if rel_dir else self.root
        out = {}
        if not os.path.isdir(d):
            require(not strict, "listing_failed", rel_dir or "<paks>")
            return out
        for n in os.listdir(d):
            p = os.path.join(d, n)
            if os.path.islink(p):
                out[n] = ("l", 0)
            else:
                out[n] = ("d", 0) if os.path.isdir(p) else ("-", os.path.getsize(p))
        return out

    def sha256(self, rel: str, size: int) -> str:
        p = self._p(rel)
        require(os.path.getsize(p) == size, "remote_size_mismatch", rel)
        return sha256_file(p)

    def upload(self, local: str, rel: str) -> None:
        self._tick()
        shutil.copyfile(local, self._p(rel))
        self._after()

    def rename(self, src: str, dst: str) -> None:
        self._tick()
        os.rename(self._p(src), self._p(dst))
        self._after()

    def delete(self, rel: str) -> None:
        self._tick()
        os.remove(self._p(rel))
        self._after()

    def mkdir(self, rel: str) -> None:
        require(_check_rel(rel)[0] == "..", "mkdir_only_outside_paks", rel)
        self._tick()
        os.mkdir(self._p(rel))
        self._after()

    def close(self):
        pass


def _check_rel(rel: str, listing: bool = False) -> List[str]:
    """Relative to the paks dir; only a leading '..' (the quarantine sibling).
    A bare '..' (the paks dir's parent) is allowed for LISTING only."""
    require(isinstance(rel, str) and rel and not rel.startswith("/"), "bad_relative_path", str(rel))
    parts = rel.split("/")
    require(all(p not in ("", ".") for p in parts) and
            all(p != ".." for p in parts[1:]) and
            (parts[0] != ".." or len(parts) >= 2 or listing), "bad_relative_path", rel)
    return parts


LIST_RE = re.compile(r"([-dl][rwxstST-]{9})\s+\d+\s+\S+\s+\S+\s+(\d+)\s+\S+\s+\d+\s+\S+\s+(.+)")


def parse_listing(data: bytes) -> Dict[str, Tuple[str, int]]:
    require(len(data) <= 1 << 20, "listing_limit")
    out = {}
    for line in data.decode("utf-8", "strict").splitlines():
        if not line.strip() or line.startswith("total "):
            continue
        m = LIST_RE.fullmatch(line.rstrip("\r"))
        require(m is not None, "invalid_listing_line", line[:120])
        name = m[3]
        if m[1][0] == "l":
            name = name.split(" -> ", 1)[0]
        require("/" not in name and name not in out, "ambiguous_listing_name", name)
        if name in (".", ".."):
            continue
        out[name] = (m[1][0], int(m[2]))
    return out


class FtpBackend:
    """Plain FTP (anonymous, passive, binary) rooted at the record's paks dir.

    Every path is relative to the paks dir (or its parent for quarantine),
    LAN addresses only, and recorded .ucas names are never RETR'd or mutated.
    Not console-proven: tested on the host against tests/fake_ftp.py only.
    """

    def __init__(self, host: str, port: int, paks_dir: str, protected: Sequence[str],
                 timeout: float = 30.0):
        addr = ipaddress.IPv4Address(host)
        require(addr.is_private or addr.is_loopback, "ftp_host_must_be_lan_or_loopback")
        self.paks_dir = paks_dir
        self.protected = set(protected)
        self.attempted = 0  # mutations started (the console may have changed)
        self.ftp = ftplib.FTP()
        self.ftp.encoding = "utf-8"
        self.ftp.connect(host, port, timeout=timeout)
        self.ftp.login()
        self.ftp.set_pasv(True)
        self.ftp.voidcmd("TYPE I")

    def _abs(self, rel: str, listing: bool = False) -> str:
        parts = _check_rel(rel, listing)
        if parts[0] == "..":
            return posixpath.join(posixpath.dirname(self.paks_dir), *parts[1:])
        return self.paks_dir + "/" + "/".join(parts)

    def _guard(self, rel: str):
        require(posixpath.basename(rel) not in self.protected, "protected_base_ucas", rel)

    def listing(self, rel_dir: str = "", strict: bool = False) -> Dict[str, Tuple[str, int]]:
        path = self._abs(rel_dir, listing=True) if rel_dir else self.paks_dir
        buf = bytearray()
        try:
            self.ftp.retrbinary("LIST " + path, buf.extend)
        except ftplib.error_perm as e:
            require(not strict, "listing_failed", "%s: %s" % (rel_dir or "<paks>", e))
            return {}
        return parse_listing(bytes(buf))

    def sha256(self, rel: str, size: int) -> str:
        self._guard(rel)
        h = hashlib.sha256()
        count = [0]

        def take(b):
            count[0] += len(b)
            require(count[0] <= size, "remote_read_exceeds_size", rel)
            h.update(b)
        self.ftp.retrbinary("RETR " + self._abs(rel), take, blocksize=BLOCK)
        require(count[0] == size, "remote_size_mismatch", rel)
        return h.hexdigest()

    def upload(self, local: str, rel: str) -> None:
        self._guard(rel)
        self.attempted += 1
        with open(local, "rb") as f:
            self.ftp.storbinary("STOR " + self._abs(rel), f, blocksize=BLOCK)

    def rename(self, src: str, dst: str) -> None:
        self._guard(src)
        self._guard(dst)
        self.attempted += 1
        self.ftp.rename(self._abs(src), self._abs(dst))

    def delete(self, rel: str) -> None:
        self._guard(rel)
        self.attempted += 1
        resp = self.ftp.sendcmd("DELE " + self._abs(rel))
        require(resp[:3] in ("200", "250", "226"), "delete_unexpected_reply", resp)

    def mkdir(self, rel: str) -> None:
        require(_check_rel(rel)[0] == "..", "mkdir_only_outside_paks", rel)
        self.attempted += 1
        self.ftp.mkd(self._abs(rel))

    def close(self):
        try:
            self.ftp.quit()
        except Exception:
            self.ftp.close()


# ---------------------------------------------------------------------------
# gate + executor
# ---------------------------------------------------------------------------

def gate_local_mirror(rec: dict, mirror: str) -> None:
    for name, f in rec["files"].items():
        p = os.path.join(mirror, name)
        if f["sha256"] is None:
            if os.path.isfile(p):
                require(os.path.getsize(p) == f["bytes"], "mirror_differs_from_record", name)
            continue
        require(os.path.isfile(p), "mirror_missing_recorded_file", name)
        require(os.path.getsize(p) == f["bytes"] and sha256_file(p) == f["sha256"],
                "mirror_differs_from_record", name)


def walk_tree(backend) -> Dict[str, Tuple[str, int]]:
    """Recursive listing of the paks folder: {relpath: (kind, bytes)} for every
    entry at every depth.  The engine mounts pak files found anywhere under
    Paks/ (a `~mods/` subfolder too), so nothing below it may go unseen.
    Fails closed on a link, a tree deeper than MAX_WALK_DEPTH or larger than
    MAX_WALK_ENTRIES, or a folder whose listing cannot be read."""
    out: Dict[str, Tuple[str, int]] = {}
    todo = [("", 0)]
    while todo:
        rel, depth = todo.pop()
        rows = backend.listing(rel, strict=True)
        for name, (kind, size) in sorted(rows.items()):
            path = rel + "/" + name if rel else name
            require(kind != "l", "link_in_paks_tree", path)
            out[path] = (kind, size)
            if kind == "d":
                require(depth + 1 <= MAX_WALK_DEPTH, "paks_tree_too_deep", path)
                todo.append((path, depth + 1))
        require(len(out) <= MAX_WALK_ENTRIES, "paks_tree_too_large", str(len(out)))
    return out


def _mountable(name: str) -> bool:
    return name.lower().endswith(MOUNTABLE_EXT)


def run_gate(backend, gate: dict, log) -> dict:
    """Every recorded file must match; no unrecorded mountable file allowed
    anywhere under the paks folder (subfolders included)."""
    rows = walk_tree(backend)
    checked = 0
    for name, f in sorted(gate["files"].items()):
        row = rows.get(name)
        require(row is not None, "recorded_file_absent", name)
        require(row == ("-", f["bytes"]), "recorded_file_size_mismatch",
                "%s live %s recorded %d" % (name, row, f["bytes"]))
        if f["sha256"] is not None:
            got = backend.sha256(name, f["bytes"])
            require(got == f["sha256"], "recorded_file_hash_mismatch",
                    "%s live %s recorded %s" % (name, got, f["sha256"]))
        checked += 1
    require(checked > 0, "gate_checked_zero_files")
    unrecorded = sorted(n for n, r in rows.items() if n not in gate["files"] and r[0] == "-")
    folders = sorted(n for n, r in rows.items() if r[0] == "d")
    mountable = [n for n in unrecorded if _mountable(n)]
    require(not mountable, "unrecorded_container_files_present", ", ".join(mountable))
    inert = [n for n in unrecorded if n not in mountable]
    log("gate: %d recorded files match (%d by sha256, %d by size); %d entries walked in "
        "%d subfolders; %d inert extra names"
        % (checked, sum(1 for f in gate["files"].values() if f["sha256"]),
           sum(1 for f in gate["files"].values() if not f["sha256"]), len(rows),
           len(folders), len(inert)))
    return {"checked": checked, "inert_extra": inert, "folders": folders,
            "entries_walked": len(rows)}


class Journal:
    def __init__(self, path: str, plan: dict, action: str):
        self.path = path
        if os.path.exists(path):
            self.doc = read_json(path)
            require(self.doc.get("schema") == JOURNAL_SCHEMA and
                    self.doc.get("plan_sha256") == plan["plan_sha256"], "journal_plan_mismatch")
        else:
            self.doc = {"schema": JOURNAL_SCHEMA, "plan_sha256": plan["plan_sha256"], "events": []}
        self.action = action
        self.gate = plan.get("gate")

    def event(self, step: int, op: dict, state: str, detail: str = ""):
        self.doc["events"].append({"action": self.action, "step": step, "op": op["op"],
                                   "args": {k: v for k, v in op.items() if k not in ("op", "local")},
                                   "state": state, "detail": detail})
        write_json(self.path, self.doc)


def describe(op: dict) -> str:
    k = op["op"]
    if k == "gate":
        return "CHECK every recorded game file (size + sha256) and refuse unrecorded containers"
    if k == "absent":
        return "CHECK these names are free: " + ", ".join(op["names"])
    if k == "upload":
        return "UPLOAD %s (%d bytes, sha256 %s) as %s, then re-read and verify" % (
            os.path.basename(op["local"]), op["bytes"], op["sha256"][:16], op["remote"])
    if k == "rename":
        return "RENAME %s -> %s" % (op["src"], op["dst"])
    if k == "verify":
        return "VERIFY %s is %d bytes, sha256 %s" % (op["remote"], op["bytes"], op["sha256"][:16])
    if k == "mkdir":
        return "MKDIR %s" % op["remote"]
    if k == "present":
        return "CHECK %s is there (%d bytes) and gone from %s" % (
            op["remote"], op["bytes"], op.get("moved_from", "-"))
    if k == "delete":
        return "DELETE %s (owned, sha256 %s)" % (op["remote"], op["sha256"][:16])
    return json.dumps(op)


def execute(plan: dict, ops: List[dict], backend, journal: Journal, log,
            recovery: str = "run restore") -> None:
    """Run ops in order.  Exit semantics (main):
      * Refused  -> nothing on the target changed (no mutation was started);
      * Stopped  -> a mutation was started, so the target MAY have changed: every
        error after the first started mutation (a refusal, a failed verify, an
        I/O error) becomes Stopped and the operator runs `recovery`."""
    try:
        _execute(ops, backend, journal, log)
    except Stopped as e:
        raise Stopped("%s -- %s" % (e, recovery))
    except Exception as e:
        if getattr(backend, "attempted", 0) > 0:
            journal.event(-1, {"op": "stop"}, "stopped_after_change", repr(e))
            raise Stopped("stopped after the target changed: %s -- %s"
                          % (getattr(e, "code", repr(e)), recovery))
        if isinstance(e, Refused) or (isinstance(e, ValueError) and hasattr(e, "code")):
            raise
        raise Refused("io_error_before_any_change", repr(e))


def _execute(ops: List[dict], backend, journal: Journal, log) -> None:
    for i, op in enumerate(ops):
        k = op["op"]
        log("[%02d] %s" % (i, describe(op)))
        if k == "gate":
            run_gate(backend, journal.gate, log)
            continue
        if k == "absent":
            rows = backend.listing("")
            clash = [n for n in op["names"] if n in rows]
            require(not clash, "reserved_name_exists", ", ".join(clash))
            continue
        if k == "verify":
            rows = backend.listing(posixpath.dirname(op["remote"]))
            require(rows.get(posixpath.basename(op["remote"]), (None, -1))[1] == op["bytes"],
                    "verify_size_mismatch", op["remote"])
            require(backend.sha256(op["remote"], op["bytes"]) == op["sha256"],
                    "verify_hash_mismatch", op["remote"])
            continue
        if k == "present":  # size-level check after a move (no RETR of large files)
            rows = backend.listing(posixpath.dirname(op["remote"]), strict=True)
            require(rows.get(posixpath.basename(op["remote"])) == ("-", op["bytes"]),
                    "moved_file_not_found_at_destination", op["remote"])
            src = op.get("moved_from")
            if src:
                rows2 = backend.listing(posixpath.dirname(src))
                require(posixpath.basename(src) not in rows2, "moved_file_still_at_source", src)
            continue
        # mutating ops: every pre-check first, then durable intent, then act
        if k == "rename":
            rows = backend.listing(posixpath.dirname(op["src"]))
            require(rows.get(posixpath.basename(op["src"]), (None, -1)) == ("-", op["bytes"]),
                    "rename_source_identity", op["src"])
            rows2 = backend.listing(posixpath.dirname(op["dst"]))
            require(posixpath.basename(op["dst"]) not in rows2, "rename_destination_exists", op["dst"])
        elif k == "upload":
            require(sha256_file(op["local"]) == op["sha256"], "local_artifact_changed", op["local"])
        elif k == "delete":
            require(backend.sha256(op["remote"], op["bytes"]) == op["sha256"],
                    "delete_target_identity", op["remote"])
        elif k == "mkdir":
            rows = backend.listing(posixpath.dirname(op["remote"]))
            require(posixpath.basename(op["remote"]) not in rows, "mkdir_destination_exists",
                    op["remote"])
        else:
            raise Refused("unknown_op", k)
        journal.event(i, op, "pending")
        try:
            if k == "upload":
                backend.upload(op["local"], op["remote"])
            elif k == "rename":
                backend.rename(op["src"], op["dst"])
            elif k == "mkdir":
                backend.mkdir(op["remote"])
            elif k == "delete":
                backend.delete(op["remote"])
        except Refused:
            journal.event(i, op, "refused")
            raise
        except Exception as e:  # lost reply / IO error: outcome unknown
            journal.event(i, op, "unknown_may_have_completed", repr(e))
            raise Stopped("step %d (%s) stopped: %r" % (i, k, e))
        if k == "upload":
            got = backend.sha256(op["remote"], op["bytes"])
            if got != op["sha256"]:
                journal.event(i, op, "verify_failed", got)
                raise Stopped("uploaded %s does not verify" % op["remote"])
        journal.event(i, op, "done")


def _identity(backend, rows, name, size_sha: Dict[str, Tuple[int, str]]):
    """Return the label whose (bytes, sha) matches the live file, or 'absent'/'unknown'."""
    row = rows.get(name)
    if row is None:
        return "absent"
    for label, (size, sha) in size_sha.items():
        if row == ("-", size) and backend.sha256(name, size) == sha:
            return label
    return "unknown"


def restore_operations(plan: dict, backend, cleanup: bool) -> Tuple[List[dict], List[str]]:
    """Identity-driven: inspect the live state now and return the ops that bring
    every target back to its recorded original.  Works after any interruption."""
    ops: List[dict] = []
    notes: List[str] = []
    retiring: set = set()
    rows = backend.listing("")
    for t in reversed(plan["targets"]):
        live, der = t["live"], t["derived"]
        ids = {"original": (live["bytes"], live["sha256"]), "derived": (der["bytes"], der["sha256"])}
        state = _identity(backend, rows, live["name"], ids)
        bstate = _identity(backend, rows, t["backup"], {"original": ids["original"]})
        require(bstate in ("absent", "original"), "backup_identity_unknown", t["backup"])
        if state == "unknown":
            # e.g. a derived index that did not verify after its rename.  With the
            # verified original backup present, the unknown file is renamed aside
            # (kept, never deleted) and the original goes back; without it, refuse.
            require(bstate == "original", "live_file_unknown_identity",
                    "%s matches neither the recorded original nor this plan's derived "
                    "file, and no verified backup %s; preserved untouched"
                    % (live["name"], t["backup"]))
            row = rows[live["name"]]
            require(row[0] == "-", "live_file_not_a_regular_file", live["name"])
            foreign = live["name"] + ".foreign-" + plan["nonce"] + ".retired"
            require(foreign not in rows, "foreign_name_exists", foreign)
            ops.append({"op": "rename", "src": live["name"], "dst": foreign, "bytes": row[1]})
            rows[foreign] = row
            notes.append("%s matched neither original nor derived; renamed aside to %s (kept)"
                         % (live["name"], foreign))
            state = "absent"
        if state == "derived":
            require(t["retired"] not in rows, "retired_name_exists", t["retired"])
            ops.append({"op": "rename", "src": live["name"], "dst": t["retired"], "bytes": der["bytes"]})
            state = "absent"
            rows[t["retired"]] = ("-", der["bytes"])
            retiring.add(t["retired"])
        if state == "absent":
            require(bstate == "original", "original_unrecoverable",
                    "%s absent and no verified backup %s" % (live["name"], t["backup"]))
            ops.append({"op": "rename", "src": t["backup"], "dst": live["name"], "bytes": live["bytes"]})
        else:
            notes.append("%s already original" % live["name"])
            if bstate == "original":
                notes.append("%s is an identical leftover copy of the original (kept)" % t["backup"])
        ops.append({"op": "verify", "remote": live["name"], "bytes": live["bytes"], "sha256": live["sha256"]})
        owned = [(t["temp"], der["bytes"], der["sha256"]), (t["retired"], der["bytes"], der["sha256"])]
        if t["sidecar"]:
            sc = t["sidecar"]
            owned += [(sc["name"], sc["bytes"], sc["sha256"]), (sc["temp"], sc["bytes"], sc["sha256"])]
        for name, size, sha in owned:
            if name not in rows:
                continue
            if name == t["retired"] and name in retiring:
                verified = True  # identified as this plan's derived file a moment ago
            else:
                verified = _identity(backend, rows, name, {"owned": (size, sha)}) == "owned"
            if not verified:
                notes.append("%s does not match this plan's bytes (partial or foreign); preserved"
                             % name)
            elif cleanup:
                ops.append({"op": "delete", "remote": name, "bytes": size, "sha256": sha})
            else:
                notes.append("%s left in place (inert with the original index; delete with "
                             "cleanup --allow-delete)" % name)
    return ops, notes


def live_state(plan: dict, backend) -> dict:
    rows = backend.listing("")
    out = {}
    for t in plan["targets"]:
        live, der = t["live"], t["derived"]
        out[live["name"]] = _identity(backend, rows, live["name"], {
            "original": (live["bytes"], live["sha256"]), "derived": (der["bytes"], der["sha256"])})
        if t["sidecar"]:
            out[t["sidecar"]["name"]] = _identity(backend, rows, t["sidecar"]["name"], {
                "sidecar": (t["sidecar"]["bytes"], t["sidecar"]["sha256"])})
    vals = set(v for k, v in out.items() if not k.endswith("_s1.ucas"))
    sc = [v for k, v in out.items() if k.endswith("_s1.ucas")]
    if vals == {"derived"} and all(v == "sidecar" for v in sc):
        verdict = "INSTALLED"
    elif vals == {"original"}:
        verdict = "PRISTINE" if all(v == "absent" for v in sc) else "PRISTINE_WITH_INERT_SIDECAR"
    else:
        verdict = "PARTIAL_OR_DRIFT"
    return {"verdict": verdict, "files": out}


# ---------------------------------------------------------------------------
# repair: back to the recorded build using our own backups; quarantine extras
# ---------------------------------------------------------------------------

def record_is_partial(rec: dict) -> bool:
    return bool((rec.get("provenance") or {}).get("PARTIAL"))


def make_repair_plan(rec: dict, backend, tool_sha: str,
                     only: Optional[Sequence[str]] = None) -> dict:
    """Back to the recorded build: drifted recorded files come back from their
    sha-verified `.original-*.bak`, and every unrecorded container file found
    ANYWHERE under the paks folder (subfolders too) is moved to a quarantine
    folder beside it (outside the paks folder).  Nothing is deleted.
    `only`: restrict the plan to these names (recorded names to repair and/or
    paths to quarantine, exactly as a full dry run prints them) so that each
    console step can get its own OK; the final full gate is then left to
    `check`."""
    require(not record_is_partial(rec), "record_is_partial",
            "finish the record first (a partial record would quarantine real game files)")
    rows = walk_tree(backend)
    only_list = sorted(set(only)) if only else []
    nonce = nonce_from("repair", rec["record_sha256"], tool_sha, "only=" + ",".join(only_list),
                       *sorted("%s:%s:%d" % (n, r[0], r[1]) for n, r in rows.items()))
    quarantine = "../paktx-quarantine-" + nonce
    ucas_names = {n for n in rec["files"] if n.lower().endswith(".ucas")}
    stems = set(rec["containers"])
    fixes, extras = [], []
    fix_ops: Dict[str, List[dict]] = {}
    for name, f in sorted(rec["files"].items()):
        row = rows.get(name)
        ok = (row == ("-", f["bytes"]) and
              (f["sha256"] is None or backend.sha256(name, f["bytes"]) == f["sha256"]))
        if ok:
            continue
        if only_list and name not in only_list:
            fixes.append({"name": name, "skipped": "not in --only"})
            continue
        require(f["sha256"] is not None, "size_only_file_drifted_cannot_repair", name)
        cands = sorted(n for n in rows if n.startswith(name + ".original-") and n.endswith(".bak")
                       and rows[n] == ("-", f["bytes"]))
        good = [n for n in cands if backend.sha256(n, f["bytes"]) == f["sha256"]]
        require(good, "no_verified_backup_for_drifted_file", name)
        src = good[0]
        ops = []
        if row is not None:
            require(row[0] == "-", "drifted_name_is_not_a_file", name)
            retired = name + ".foreign-" + nonce + ".retired"
            ops.append({"op": "rename", "src": name, "dst": retired, "bytes": row[1]})
        ops.append({"op": "rename", "src": src, "dst": name, "bytes": f["bytes"]})
        ops.append({"op": "verify", "remote": name, "bytes": f["bytes"], "sha256": f["sha256"]})
        fix_ops[name] = ops
        fixes.append({"name": name, "from_backup": src, "live_was": None if row is None else row[1]})
    for path in sorted(rows):
        kind, size = rows[path]
        if path in rec["files"] or kind != "-" or not _mountable(path):
            continue
        base = posixpath.basename(path)
        # defence in depth: a file that belongs to a recorded container (same stem)
        # or carries a base .ucas name is never moved; the record must be fixed
        require(posixpath.splitext(base)[0] not in stems or "/" in path,
                "unrecorded_file_of_recorded_container", path)
        require(base not in ucas_names, "quarantine_name_is_a_base_ucas_name", path)
        if only_list and path not in only_list:
            extras.append({"name": path, "bytes": size, "skipped": "not in --only"})
            continue
        extras.append({"name": path, "bytes": size})
    if only_list:
        known = {f["name"] for f in fixes} | {e["name"] for e in extras}
        missing = [n for n in only_list if n not in known]
        require(not missing, "only_name_not_in_repair_plan", ", ".join(missing))
    ops: List[dict] = []
    for f in fixes:
        ops += fix_ops.get(f["name"], [])
    moving = [e for e in extras if "skipped" not in e]
    if moving:
        ops.append({"op": "mkdir", "remote": quarantine})
        made = set()
        for e in moving:
            parts = e["name"].split("/")[:-1]
            for i in range(1, len(parts) + 1):
                d = quarantine + "/" + "/".join(parts[:i])
                if d not in made:
                    made.add(d)
                    ops.append({"op": "mkdir", "remote": d})
        for e in moving:
            dst = quarantine + "/" + e["name"]
            ops.append({"op": "rename", "src": e["name"], "dst": dst, "bytes": e["bytes"]})
            ops.append({"op": "present", "remote": dst, "bytes": e["bytes"], "moved_from": e["name"]})
    require(ops or not only_list, "only_selects_nothing")
    plan = {"schema": PLAN_SCHEMA, "kind": "repair", "tool": TOOL, "tool_sha256": tool_sha,
            "nonce": nonce, "only": only_list,
            "record": {"record_sha256": rec["record_sha256"], "title_id": rec["title_id"],
                       "version": rec["version"], "paks_dir": rec["paks_dir"]},
            "gate": gate_spec(rec), "fixes": fixes, "quarantine": quarantine if moving else None,
            "quarantined": extras, "targets": [],
            "operations": ops + ([] if only_list else [{"op": "gate"}])}
    plan["plan_sha256"] = plan_sha(plan)
    return plan


# ---------------------------------------------------------------------------
# optional in-game probe (Lua, for the UE4SS runtime): proves the engine
# loaded objects that exist ONLY in the mod (its new packages)
# ---------------------------------------------------------------------------

PROBE_LUA = """-- paktx probe (generated by paktx.py probe; data only, no mod names in code)
local P={%s}
local function d(s) print("[pk] "..tostring(s).."\\n") end
local seen,n={},0
local function scan()
  n=n+1 local k=0
  for i,p in ipairs(P) do
    if not seen[i] then
      local ok,o=pcall(StaticFindObject,p)
      if ok and o~=nil then local okv,v=pcall(function() return o:IsValid() end)
        if okv and v then seen[i]=true d("FOUND "..p) end end
    end
    if seen[i] then k=k+1 end
  end
  d("T"..n.." found="..k.."/"..#P)
  return n>=%d or k==#P
end
d("probe start plan=%s paths="..#P)
LoopAsync(%d,function() local ok,r=pcall(scan) if not ok then d("ERR "..tostring(r):sub(1,60)) return true end return r end)
"""


def object_path(virtual: str) -> Optional[str]:
    """../../../<Project>/Content/A/B.uasset -> /Game/A/B.B ; Engine -> /Engine.
    Anything else (plugins, odd mounts) is skipped, never guessed."""
    if not virtual.startswith("../../../") or not virtual.endswith((".uasset", ".umap")):
        return None
    parts = virtual[9:].split("/")
    if len(parts) < 3 or parts[1] != "Content":
        return None
    root = "/Engine" if parts[0] == "Engine" else "/Game"
    name = parts[-1].rsplit(".", 1)[0]
    if not re.fullmatch(r"[A-Za-z0-9_]+", name) or \
            not all(re.fullmatch(r"[A-Za-z0-9_]+", x) for x in parts[2:-1]):
        return None
    return root + "/" + "/".join(parts[2:-1] + [name]) + "." + name


def make_probe(B, plan: dict, mod_dir: str, limit: int = 8, cycles: int = 30,
               period_ms: int = 6000) -> str:
    inv = inventory_mod(B, mod_dir)
    added = set()
    for t in plan["targets"]:
        for p in (t.get("packages") or {}).values():
            added |= {int(x, 16) for x in p["added"]}
    paths = []
    for mod in inv["iostore"]:
        for f in mod.directory["files"]:
            if f.chunk_id[11] == EXPORT_CHUNK_TYPE and B.package_id_of_chunk(f.chunk_id) in added:
                op = object_path(f.path)
                if op:
                    paths.append(op)
    paths = sorted(set(paths))[:limit]
    require(paths, "probe_has_zero_mod_only_objects",
            "the mod adds no package with a /Game or /Engine object path")
    lua = PROBE_LUA % (",".join('"%s"' % p for p in paths), cycles,
                       plan["plan_sha256"][:12], period_ms)
    require(len(lua.encode()) < 4096, "probe_over_4096_bytes", str(len(lua.encode())))
    return lua


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _backend(args, rec_or_plan: dict, fail_after=None):
    if args.local:
        return LocalBackend(args.local, fail_after=fail_after)
    require(args.ftp, "backend_required", "--local DIR or --ftp HOST:PORT")
    host, _, port = args.ftp.partition(":")
    paks = rec_or_plan["record"]["paks_dir"] if "record" in rec_or_plan else rec_or_plan["paks_dir"]
    protected = [n for n in rec_or_plan["gate"]["files"] if n.lower().endswith(".ucas")] \
        if "gate" in rec_or_plan else [n for n in rec_or_plan["files"] if n.lower().endswith(".ucas")]
    return FtpBackend(host, int(port or 2121), paks, protected)


def _tool_sha() -> str:
    return sha256_file(os.path.abspath(__file__))


def _print_plan(plan: dict, log) -> None:
    log("PLAN %s  kind=%s  sha256=%s" % (plan["schema"], plan["kind"], plan["plan_sha256"]))
    log("game: %s %s  paks: %s" % (plan["record"]["title_id"], plan["record"]["version"],
                                   plan["record"]["paks_dir"]))
    if plan["kind"] == "install":
        for f in plan["mod"]["files"]:
            log("mod file: %s %d bytes sha256 %s" % (f["name"], f["bytes"], f["sha256"][:16]))
        for g in plan["mod"]["ignored"]:
            log("mod file ignored: %s (%s)" % (g["name"], g["reason"]))
        if plan.get("ownership"):
            log("new packages go to: %s (%s)" % (plan["ownership"]["primary"],
                                                 plan["ownership"]["primary_rule"]))
        for d, mm in sorted((plan.get("skipped_by_only_into") or {}).items()):
            for mid, p in sorted(mm.items()):
                log("NOT INSTALLED (--only-into %s): %d packages of %s owned by %s"
                    % (plan["only_into"], len(p["replaced"]) + len(p["added"]), mid, d))
        sub = plan.get("substitution")
        if sub:
            log("SUBSTITUTE (capability #12): manifest %s, %d derived packages, user decisions: %s"
                % (sub["manifest_sha256"][:16], len(sub["rows"]), ", ".join(sub["user_decisions"]) or "none"))
            for r in sub.get("plan_rows_not_shipped", []):
                log("  NOT SUBSTITUTED (plan row needs %s): %s stays as the mod ships it"
                    % (", ".join(r["needs"]) or "nothing", r["package"]))
            for r in sub["rows"]:
                log("  %s: parent %s -> %s (derived %d B, iohash %s; mod file unchanged)"
                    % (r["package"], r["old_parent"], r["new_parent"], r["derived"]["bytes"],
                       r["derived"]["iohash"][:16]))
        for t in plan["targets"]:
            log("TARGET %s [%s]" % (t["container"], t["kind"]))
            log("  change: %s (%d B, %s) -> derived (%d B, %s)" % (
                t["live"]["name"], t["live"]["bytes"], t["live"]["sha256"][:16],
                t["derived"]["bytes"], t["derived"]["sha256"][:16]))
            log("  backup: %s (the original, by rename; never rewritten)" % t["backup"])
            if t["sidecar"]:
                log("  new file: %s (%d B, %s)" % (t["sidecar"]["name"], t["sidecar"]["bytes"],
                                                   t["sidecar"]["sha256"][:16]))
                log("  untouched: %s (%d B)" % (t["base_ucas"]["name"], t["base_ucas"]["bytes"]))
                for mid, p in t["packages"].items():
                    log("  packages from %s: %d replaced, %d new" % (mid, len(p["replaced"]),
                                                                     len(p["added"])))
            else:
                log("  pak entries changed: %s" % ", ".join(t["pak_rebuild"]["changed_entries"]))
    else:
        if plan.get("only"):
            log("ONLY this step: %s (a later step or `check` does the rest)"
                % ", ".join(plan["only"]))
        for f in plan["fixes"]:
            if f.get("skipped"):
                log("LATER (not in this step): repair %s" % f["name"])
            else:
                log("REPAIR %s from %s" % (f["name"], f["from_backup"]))
        for e in plan["quarantined"]:
            if e.get("skipped"):
                log("LATER (not in this step): move %s out of the paks folder" % e["name"])
            else:
                log("QUARANTINE %s (%d B) -> %s/%s (outside the paks folder; nothing deleted)"
                    % (e["name"], e["bytes"], plan["quarantine"], e["name"]))
    for i, op in enumerate(plan["operations"]):
        log("  op %02d: %s" % (i, describe(op)))
    log("every other file in the paks folder: unchanged")


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--builder", default=None)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("record")
    r.add_argument("--mirror", required=True)
    r.add_argument("--sizes")
    r.add_argument("--headers")
    r.add_argument("--title-id", required=True)
    r.add_argument("--version", required=True)
    r.add_argument("--paks-dir", required=True)
    r.add_argument("--provenance")
    r.add_argument("--out", required=True)
    pr = sub.add_parser("probe")
    pr.add_argument("--plan", required=True)
    pr.add_argument("--mod", required=True)
    pr.add_argument("--out", required=True)
    for name in ("install", "restore", "verify", "repair", "cycle", "show", "check"):
        p = sub.add_parser(name)
        p.add_argument("--record")
        p.add_argument("--plan")
        p.add_argument("--mirror")
        p.add_argument("--mod")
        p.add_argument("--work")
        p.add_argument("--new-packages-into")
        p.add_argument("--repak")
        p.add_argument("--local")
        p.add_argument("--ftp")
        p.add_argument("--journal")
        p.add_argument("--dry-run", action="store_true")
        p.add_argument("--confirm")
        p.add_argument("--game-closed", action="store_true")
        p.add_argument("--cleanup", action="store_true")
        p.add_argument("--allow-delete", action="store_true")
        p.add_argument("--only", help="repair: comma-separated names/paths for this step only")
        p.add_argument("--only-into", help="install/cycle: diagnostic, one container only")
        p.add_argument("--substitute", help="install/cycle: derived-packages manifest (capability #12)")
        p.add_argument("--accept-decision", action="append", default=[],
                       help="install/cycle: restate a user decision the manifest needs "
                            "(allow-partial, accept-unproven-opaque); once per decision")
        p.add_argument("--reparent-plan", help="install/cycle: the reviewed reparent plan the "
                                               "--substitute manifest was made from (sha-bound)")
        p.add_argument("--readers", help="folder of the pinned second-source readers "
                                         "(default ../mod-manager/material-fallback)")
    args = ap.parse_args(argv)
    log = lambda s: print(s, file=sys.stderr)
    result: dict = {"tool": TOOL}
    opened: list = []

    def backend_for(*a, **kw):
        be = _backend(*a, **kw)
        opened.append(be)
        return be
    try:
        B = load_builder(args.builder or default_builder_path())
        tsha = _tool_sha()
        if args.cmd == "record":
            sizes = read_json(args.sizes) if args.sizes else None
            prov = read_json(args.provenance) if args.provenance else None
            rec = make_record(B, args.mirror, args.title_id, args.version, args.paks_dir,
                              sizes, args.headers, prov,
                              relative_to=os.path.dirname(os.path.abspath(args.out)))
            write_json(args.out, rec)
            result.update({"result": "recorded", "files": len(rec["files"]),
                           "containers": len(rec["containers"]),
                           "record_sha256": rec["record_sha256"]})
            log("record: %d files, %d containers" % (len(rec["files"]), len(rec["containers"])))
        elif args.cmd == "probe":
            plan = read_json(args.plan)
            require(plan_sha(plan) == plan.get("plan_sha256"), "plan_self_hash_mismatch")
            lua = make_probe(B, plan, args.mod)
            os.makedirs(args.out, exist_ok=True)
            with open(os.path.join(args.out, "main.lua"), "w") as f:
                f.write(lua)
            result.update({"result": "probe-written", "bytes": len(lua.encode()),
                           "paths": lua.split("local P={", 1)[1].split("}", 1)[0].count('"/'), "file": os.path.join(args.out, "main.lua")})
        elif args.cmd in ("install", "cycle"):
            rec = load_record(args.record)
            require(args.mirror and args.mod and args.work, "install_needs_mirror_mod_work")
            require(args.substitute or not args.accept_decision, "accept_decision_needs_substitute")
            plan = make_install_plan(B, rec, args.mirror, args.mod, args.work,
                                     args.new_packages_into, args.repak, tsha,
                                     only_into=args.only_into, substitute=args.substitute,
                                     accepted=args.accept_decision, readers=args.readers,
                                     reparent_plan=args.reparent_plan)
            ppath = os.path.join(args.work, "plan.json")
            write_json(ppath, plan)
            _print_plan(plan, log)
            result.update({"plan": ppath, "plan_sha256": plan["plan_sha256"],
                           "targets": [t["container"] for t in plan["targets"]]})
            if args.dry_run:
                result["result"] = "dry-run"
            elif args.cmd == "install":
                require(args.confirm and plan["plan_sha256"].startswith(args.confirm) and
                        len(args.confirm) >= 12, "confirm_must_match_plan_sha",
                        "re-run with --confirm %s" % plan["plan_sha256"][:12])
                require(args.game_closed, "game_closed_attestation_required")
                be = backend_for(args, plan)
                try:
                    execute(plan, plan["operations"], be,
                            Journal(args.journal or ppath + ".journal.json", plan, "install"), log)
                finally:
                    be.close()
                result["result"] = "installed"
            else:  # cycle: local self-test install -> verify -> restore -> verify
                require(args.local, "cycle_is_local_only")
                be = LocalBackend(args.local)
                opened.append(be)
                j = Journal(ppath + ".journal.json", plan, "cycle")
                execute(plan, plan["operations"], be, j, log)
                s1 = live_state(plan, be)
                ops, notes = restore_operations(plan, be, cleanup=True)
                execute(plan, ops, be, j, log)
                s2 = live_state(plan, be)
                run_gate(be, plan["gate"], log)
                result.update({"result": "cycled", "after_install": s1, "after_restore": s2})
        elif args.cmd in ("restore", "verify", "show"):
            plan = read_json(args.plan)
            require(plan_sha(plan) == plan.get("plan_sha256"), "plan_self_hash_mismatch")
            if args.cmd == "show":
                _print_plan(plan, log)
                result["result"] = "shown"
            elif args.cmd == "verify":
                be = backend_for(args, plan)
                try:
                    result.update({"result": "verified", "state": live_state(plan, be)})
                finally:
                    be.close()
            else:
                require(plan["kind"] == "install", "restore_is_for_install_plans")
                be = backend_for(args, plan)
                try:
                    require(not args.cleanup or args.allow_delete, "cleanup_needs_allow_delete")
                    ops, notes = restore_operations(plan, be, cleanup=args.cleanup)
                    for n in notes:
                        log("note: " + n)
                    for i, op in enumerate(ops):
                        log("  restore op %02d: %s" % (i, describe(op)))
                    result.update({"restore_operations": len(ops), "notes": notes})
                    if args.dry_run:
                        result["result"] = "dry-run"
                    else:
                        require(args.confirm and plan["plan_sha256"].startswith(args.confirm) and
                                len(args.confirm) >= 12, "confirm_must_match_plan_sha")
                        require(args.game_closed, "game_closed_attestation_required")
                        execute(plan, ops, be, Journal(args.journal or args.plan + ".journal.json",
                                                       plan, "restore"), log,
                                recovery="run restore --dry-run again (it re-reads the live files)")
                        result.update({"result": "restored", "state": live_state(plan, be)})
                finally:
                    be.close()
        elif args.cmd == "repair":
            rec = load_record(args.record)
            require(args.work, "repair_needs_work")
            os.makedirs(args.work, exist_ok=True)
            be = backend_for(args, rec)
            try:
                only = [x.strip() for x in args.only.split(",") if x.strip()] if args.only else None
                require(args.only is None or only, "only_is_empty")
                plan = make_repair_plan(rec, be, tsha, only)
                ppath = os.path.join(args.work, "repair-plan.json")
                write_json(ppath, plan)
                _print_plan(plan, log)
                result.update({"plan": ppath, "plan_sha256": plan["plan_sha256"],
                               "fixes": len(plan["fixes"]), "quarantined": len(plan["quarantined"])})
                if args.dry_run:
                    result["result"] = "dry-run"
                else:
                    require(args.confirm and plan["plan_sha256"].startswith(args.confirm) and
                            len(args.confirm) >= 12, "confirm_must_match_plan_sha",
                            "re-run with --confirm %s" % plan["plan_sha256"][:12])
                    require(args.game_closed, "game_closed_attestation_required")
                    execute(plan, plan["operations"], be,
                            Journal(args.journal or ppath + ".journal.json", plan, "repair"), log,
                            recovery="run repair --dry-run again (it re-reads the live files) "
                                     "and show it to the user")
                    result["result"] = "repaired" if not plan["only"] else "repair-step-done"
            finally:
                be.close()
        elif args.cmd == "check":
            # read-only: recursive listing + sha256 of every recorded small file
            rec = load_record(args.record)
            require(not record_is_partial(rec), "record_is_partial")
            be = backend_for(args, rec)
            try:
                tree = walk_tree(be)
                for n, (k, size) in sorted(tree.items()):
                    log("  %s %12d  %s" % (k, size, n))
                g = run_gate(be, gate_spec(rec), log)
                result.update({"result": "checked", "verdict": "PRISTINE",
                               "recorded_files_checked": g["checked"],
                               "entries_walked": g["entries_walked"],
                               "subfolders": g["folders"], "inert_extra": g["inert_extra"]})
            finally:
                be.close()
        print(json.dumps(result, indent=1, sort_keys=True))
        return 0
    except Exception as e:  # builder refusals carry .code too
        changed = any(getattr(b, "attempted", 0) > 0 for b in opened)
        if isinstance(e, Stopped) or changed:
            # a change was started: never report this as "nothing changed"
            result.update({"result": "stopped", "console_changed": "maybe",
                           "detail": str(e) if isinstance(e, Stopped) else repr(e)})
            print(json.dumps(result, indent=1, sort_keys=True))
            return 3
        if isinstance(e, Refused) or (isinstance(e, ValueError) and hasattr(e, "code")):
            result.update({"result": "refused", "console_changed": False, "code": e.code,
                           "detail": getattr(e, "detail", "")})
        else:  # unexpected error before any change (connect, read, local file)
            result.update({"result": "refused", "console_changed": False,
                           "code": "unexpected_error_before_any_change", "detail": repr(e)})
        print(json.dumps(result, indent=1, sort_keys=True))
        return 2


if __name__ == "__main__":
    sys.exit(main())
