#!/usr/bin/env python3
"""Capability #8 — whole-chunk intake gate for PC IoStore mods (Mac-only tool).

Reads ONE downloaded PC mod container (.utoc/.ucas, never modified) and the
game's OWN base files (base .utoc files, base container headers, the global
ScriptObjects chunk, plus any base chunks the integrator fetched read-only)
and decides, before any install:

  1. FORMAT  — is the mod container in the same IoStore/Zen profile the game
               reads (TOC version, block size, flags, container-header
               version, Zen summary layout)?  Mismatch = refuse.
  2. IDENTITY — every mod chunk matches its TOC IoHash; every package's name
               hashes (CityHash64) to its chunk id and store id.
  3. CLOSURE — every import of every mod package resolves: script classes in
               the game's ScriptObjects, package imports inside the mod or in
               the base containers (package level from base TOCs; export level
               from fetched base headers).  Replaced base packages must still
               export every public export the base exported.  Broken = refuse;
               not-yet-provable = INCOMPLETE + exact fetch plan.
  4. VISUAL  — for every material, follow its parent chain to the shader
               source and say whether the game can draw it with the game's own
               shader format (taken from the base shader-library names).  If
               not, DISCLOSE the visual loss (the engine draws its default
               material: the grey/white checker grid) together with the meshes
               that use it.  Textures: pixel formats + platform-layout markers
               compared against base textures when base samples are present.

Nothing here names a game or a mod.  All per-game facts come from the base
files passed in.  Exit codes: 0 admitted, no visual loss; 3 admitted but
visual loss disclosed (install only with --accept-visual-loss recorded);
4 incomplete (fetch plan written); 2 refused; 1 usage error.
"""
import argparse
import json
import os
import re
import struct
import sys
from typing import Dict, List, Optional, Set, Tuple

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue_formats as uf  # noqa: E402

TOOL_VERSION = "cap08-intake-2"
EXIT_OK, EXIT_USAGE, EXIT_REFUSED, EXIT_VISUAL_LOSS, EXIT_INCOMPLETE = 0, 1, 2, 3, 4

# Engine class paths (engine-level names, /Script/Engine is the engine module).
MATERIAL_CLASSES = {"/Script/Engine.Material"}
MATERIAL_INSTANCE_CLASSES = {"/Script/Engine.MaterialInstanceConstant",
                             "/Script/Engine.MaterialInstanceDynamic"}
TEXTURE_CLASSES = {"/Script/Engine.Texture2D", "/Script/Engine.TextureCube",
                   "/Script/Engine.Texture2DArray", "/Script/Engine.VolumeTexture"}
MESH_CLASSES = {"/Script/Engine.SkeletalMesh", "/Script/Engine.StaticMesh"}

# Public engine shader-format names a PC/mobile cook writes into material
# packages that carry inline shader maps.  The TARGET format is never listed
# here: it is read from the game's own ShaderArchive file names.
PC_SHADER_FORMAT_RE = re.compile(r"^(PCD3D_[A-Z0-9_]+|SF_VULKAN_[A-Z0-9_]+|SF_METAL[A-Z0-9_]*|"
                                 r"GLSL_[A-Z0-9_]+|VULKAN_[A-Z0-9_]+)$")
FSTRING_TOKEN_RE = re.compile(rb"[\x05-\x30]\x00\x00\x00([A-Z][A-Z0-9_]{3,46})\x00")
SHADER_ARCHIVE_RE = re.compile(r"ShaderArchive-(?P<lib>.+)-(?P<fmt>[A-Za-z0-9_]+)-(?P<plat>[A-Za-z0-9_]+)\.ushaderbytecode$")

# bytes per 4x4 block (block formats) or per pixel (others); engine pixel-format names
BLOCK_BYTES = {"PF_DXT1": 8, "PF_BC4": 8, "PF_DXT3": 16, "PF_DXT5": 16, "PF_BC5": 16,
               "PF_BC6H": 16, "PF_BC7": 16}
PIXEL_BYTES = {"PF_B8G8R8A8": 4, "PF_R8G8B8A8": 4, "PF_G8": 1, "PF_G16": 2, "PF_FloatRGBA": 8,
               "PF_A32B32G32R32F": 16, "PF_R16F": 2, "PF_R32_FLOAT": 4}


# ------------------------------------------------------------------ inputs
class Container:
    def __init__(self, name: str, toc: uf.Toc, reader: Optional[uf.ChunkReader]):
        self.name = name
        self.toc = toc
        self.reader = reader
        self.header: Optional[uf.ContainerHeader] = None
        self.package_rows: Dict[int, int] = {}
        for row, cid in enumerate(toc.chunk_ids):
            if uf.Toc.chunk_type(cid) == 1 and struct.unpack_from("<H", cid, 8)[0] == 0:
                self.package_rows[uf.Toc.chunk_object_id(cid)] = row

    def header_row(self) -> Optional[int]:
        rows = [r for r, c in enumerate(self.toc.chunk_ids) if uf.Toc.chunk_type(c) == 6]
        return rows[0] if len(rows) == 1 else None

    def shader_archives(self) -> List[Dict[str, str]]:
        out = []
        for row, cid in enumerate(self.toc.chunk_ids):
            if uf.Toc.chunk_type(cid) == 8:
                path = self.toc.files.get(row, "")
                m = SHADER_ARCHIVE_RE.search(path)
                out.append({"path": path, "format": m.group("fmt") if m else None,
                            "platform": m.group("plat") if m else None})
        return out


def load_fetched(dirpath: Optional[str]) -> Dict[Tuple[str, str], str]:
    """Integrator-fetched base chunks: <dir>/<container stem>/<chunk id hex>.bin."""
    out = {}
    if not dirpath:
        return out
    for stem in sorted(os.listdir(dirpath)):
        sub = os.path.join(dirpath, stem)
        if not os.path.isdir(sub):
            continue
        for fn in sorted(os.listdir(sub)):
            if fn.endswith(".bin") and re.fullmatch(r"[0-9a-f]{24}\.bin", fn):
                out[(stem, fn[:-4])] = os.path.join(sub, fn)
    return out


class Intake:
    def __init__(self, args):
        self.args = args
        self.report: Dict[str, object] = {"tool": TOOL_VERSION, "label": "Mac-only",
                                          "inputs": {}, "refusals": [], "incomplete": [],
                                          "counts": {}}
        self.refusals: List[str] = self.report["refusals"]
        self.incomplete: List[str] = self.report["incomplete"]
        self.base: List[Container] = []
        self.fetched = load_fetched(args.base_fetched)
        self.fetched_used: List[Dict[str, str]] = []
        self.fetch_plan: List[Dict[str, object]] = []
        self.script: Optional[uf.ScriptObjects] = None
        self.base_headers: Dict[int, Tuple[str, uf.ZenPackage]] = {}  # base pid -> parsed header
        self.target_formats: Set[str] = set()

    # ---------------- base side
    def load_base(self):
        a = self.args
        for spec in a.base_utoc:
            utoc = spec
            ucas = None
            if "=" in spec:
                utoc, ucas = spec.split("=", 1)
            name = os.path.basename(utoc)[:-5] if utoc.endswith(".utoc") else os.path.basename(utoc)
            toc = uf.parse_toc(uf.read_bytes(utoc))
            reader = uf.ChunkReader(toc, ucas) if ucas else None
            c = Container(name, toc, reader)
            self.base.append(c)
            self.report["inputs"].setdefault("base_utoc", []).append(
                {"name": name, "path": utoc, "sha256": uf.sha256_file(utoc), "chunks": len(toc.chunk_ids),
                 "packages": len(c.package_rows)})
        uf.need(len(self.base) > 0, "no base containers given")
        # container headers: captured files (name=path) or fetched chunk files
        for spec in a.base_container_header or []:
            name, path = spec.split("=", 1)
            c = self._base_by_name(name)
            uf.need(c is not None, "container header for unknown base container %s" % name)
            data = uf.read_bytes(path)
            self._admit_base_header(c, data, path)
        for c in self.base:
            if c.header is None and c.header_row() is not None:
                data = self._fetched_chunk(c, c.header_row())
                if data is not None:
                    self._admit_base_header(c, data, "fetched")
            if c.header is None and c.header_row() is not None:
                self.incomplete.append("base container header missing: %s" % c.name)
                self._plan(c, c.header_row(), "container header (store entries, shader-map hashes)")
            for sa in c.shader_archives():
                if sa["format"]:
                    self.target_formats.add(sa["format"])
        self.report["target_shader_formats"] = sorted(self.target_formats)
        self.report["target_shader_formats_source"] = "base ShaderArchive-*.ushaderbytecode names in base TOC directory indexes"
        if not self.target_formats:
            self.incomplete.append("no base shader library found: target shader format unknown")
        # global script objects
        so_toc = uf.parse_toc(uf.read_bytes(a.script_objects_utoc))
        rows = [r for r, cid in enumerate(so_toc.chunk_ids) if uf.Toc.chunk_type(cid) == 5]
        uf.need(len(rows) == 1, "global container must hold exactly one ScriptObjects chunk")
        data = uf.ChunkReader(so_toc, a.script_objects_ucas).read_verified(rows[0])
        self.script = uf.parse_script_objects(data)
        self.report["inputs"]["script_objects"] = {"utoc": a.script_objects_utoc, "ucas": a.script_objects_ucas,
                                                   "utoc_sha256": uf.sha256_file(a.script_objects_utoc),
                                                   "entries": len(self.script.entries), "iohash_verified": True}

    def _base_by_name(self, name: str) -> Optional[Container]:
        for c in self.base:
            if c.name == name:
                return c
        return None

    def _admit_base_header(self, c: Container, data: bytes, src: str):
        row = c.header_row()
        uf.need(row is not None, "base container %s has no header chunk" % c.name)
        uf.need(len(data) == c.toc.lengths[row] and uf.io_hash(data) == c.toc.hashes[row],
                "base container header %s does not match its TOC IoHash" % c.name)
        c.header = uf.parse_container_header(data)
        uf.need(c.header.container_id == c.toc.header["container_id"], "container id mismatch %s" % c.name)
        self.report["inputs"].setdefault("base_container_headers", []).append(
            {"container": c.name, "source": src, "store_entries": len(c.header.store), "iohash_verified": True})

    def _fetched_chunk(self, c: Container, row: int) -> Optional[bytes]:
        key = (c.name, c.toc.chunk_ids[row].hex())
        path = self.fetched.get(key)
        if path is None and c.reader is not None:
            return c.reader.read_verified(row)
        if path is None:
            return None
        data = uf.read_bytes(path)
        if len(data) != c.toc.lengths[row] or uf.io_hash(data) != c.toc.hashes[row]:
            # A short/bad transfer is not a defect of the mod: INCOMPLETE, and the
            # caller re-plans the same chunk so the fetch plan says what to re-fetch.
            self.incomplete.append("fetched base chunk %s/%s fails its TOC IoHash: not used, re-fetch it "
                                   "(transfer error, not a mod defect)" % key)
            return None
        self.fetched_used.append({"container": c.name, "chunk": key[1], "path": path})
        return data

    def _plan(self, c: Container, row: int, why: str):
        cid = c.toc.chunk_ids[row].hex()
        if any(p["container"] == c.name and p["chunk_id"] == cid for p in self.fetch_plan):
            return
        self.fetch_plan.append({"container": c.name, "ucas": c.name + ".ucas", "chunk_id": cid,
                                "chunk_type": uf.CHUNK_TYPES.get(uf.Toc.chunk_type(c.toc.chunk_ids[row])),
                                "virtual_path": c.toc.files.get(row), "length": c.toc.lengths[row],
                                "expected_iohash": c.toc.hashes[row].hex(),
                                "physical_ranges": [{"offset": o, "length": n} for o, n in c.toc.physical_ranges(row)],
                                "save_as": "%s/%s.bin" % (c.name, cid), "why": why,
                                "priority": 1 if "container header" in why else (3 if why.startswith("replacement") else 2)})
        self.fetch_plan.sort(key=lambda p: (p["priority"], p["length"]))

    def base_owner(self, pid: int) -> List[Container]:
        return [c for c in self.base if pid in c.package_rows]

    def base_store(self, pid: int) -> Optional[dict]:
        for c in self.base:
            if c.header and pid in c.header.store:
                return c.header.store[pid]
        return None

    def base_header(self, pid: int, why: str) -> Optional[uf.ZenPackage]:
        if pid in self.base_headers:
            return self.base_headers[pid][1]
        owners = self.base_owner(pid)
        if not owners:
            return None
        c = owners[-1]
        row = c.package_rows[pid]
        data = self._fetched_chunk(c, row)
        if data is None:
            self._plan(c, row, why)
            return None
        z = uf.parse_zen_package(data)
        uf.need(uf.package_id_of(z.name) == pid, "fetched base package name does not hash to its id")
        z._data = data
        self.base_headers[pid] = (c.name, z)
        return z

    # ---------------- mod side
    def load_mod(self):
        a = self.args
        self.mod_toc = uf.parse_toc(uf.read_bytes(a.mod_utoc))
        self.mod_reader = uf.ChunkReader(self.mod_toc, a.mod_ucas)
        t = self.mod_toc
        self.report["inputs"]["mod"] = {"utoc": a.mod_utoc, "utoc_sha256": uf.sha256_file(a.mod_utoc),
                                        "ucas": a.mod_ucas, "ucas_sha256": uf.sha256_file(a.mod_ucas),
                                        "mount_point": t.mount_point, "chunks": len(t.chunk_ids)}
        self.check_profile()
        # integrity
        verified = 0
        self.mod_chunks: Dict[int, bytes] = {}
        for row, cid in enumerate(t.chunk_ids):
            ty = uf.Toc.chunk_type(cid)
            if ty == 2 and not a.verify_bulk:
                continue
            data = self.mod_reader.read_row(row)
            if len(data) != t.lengths[row] or uf.io_hash(data) != t.hashes[row]:
                self.refusals.append("mod chunk %s fails its TOC IoHash" % cid.hex())
                continue
            verified += 1
            if ty in (1, 6):
                self.mod_chunks[row] = data
        self.report["counts"]["mod_chunks_iohash_verified"] = verified
        self.report["counts"]["mod_bulk_chunks_skipped"] = 0 if a.verify_bulk else sum(
            1 for c in t.chunk_ids if uf.Toc.chunk_type(c) == 2)
        hrows = [r for r, c in enumerate(t.chunk_ids) if uf.Toc.chunk_type(c) == 6]
        uf.need(len(hrows) == 1 and hrows[0] in self.mod_chunks, "mod container header missing or unverified")
        self.mod_header = uf.parse_container_header(self.mod_chunks[hrows[0]])
        uf.need(self.mod_header.container_id == t.header["container_id"], "mod container id mismatch")
        if self.mod_header.optional_store:
            self.incomplete.append("%d optional-segment packages are not analyzed by this tool" % len(self.mod_header.optional_store))
        # packages
        self.mod_pkgs: Dict[int, Dict[str, object]] = {}
        for row, cid in enumerate(t.chunk_ids):
            if uf.Toc.chunk_type(cid) != 1 or row not in self.mod_chunks:
                continue
            pid = uf.Toc.chunk_object_id(cid)
            data = self.mod_chunks[row]
            try:
                z = uf.parse_zen_package(data)
            except uf.FormatError as e:
                self.refusals.append("mod package chunk %s: %s" % (cid.hex(), e))
                continue
            ok_id = uf.package_id_of(z.name) == pid
            store = self.mod_header.store.get(pid)
            if not ok_id or store is None:
                self.refusals.append("mod package %s: name/chunk/store id disagree" % z.name)
                continue
            names_ids = [uf.package_id_of(n) for n in z.imported_package_names]
            if names_ids != store["imported_packages"]:
                self.refusals.append("mod package %s: imported package names disagree with store entry" % z.name)
                continue
            self.mod_pkgs[pid] = {"pid": pid, "row": row, "zen": z, "store": store, "data": data,
                                  "path": t.files.get(row)}
        self.report["counts"]["mod_packages"] = len(self.mod_pkgs)
        uf.need(len(self.mod_pkgs) > 0, "mod container holds zero packages")
        # Which mod packages import which other mod packages (data only; used to pick
        # live probes that the engine must load together, see diag_entries).
        self.report["load_graph"] = {
            p["zen"].name: {"replaces_base": bool(self.base_owner(pid)),
                            "imports_in_mod": sorted(n for n in p["zen"].imported_package_names
                                                     if uf.package_id_of(n) in self.mod_pkgs)}
            for pid, p in sorted(self.mod_pkgs.items())}
        extra = set(self.mod_header.store) - set(self.mod_pkgs)
        if extra:
            self.refusals.append("mod store lists %d packages without a verified chunk" % len(extra))

    def check_profile(self):
        """The mod container must be in the same container profile as the game's own containers."""
        t = self.mod_toc
        rows = []
        for c in self.base:
            if not c.package_rows:
                continue
            for key in ("version", "compression_block_size"):
                rows.append((key, t.header[key], c.toc.header[key], c.name))
        mism = sorted({"%s: mod=%s base(%s)=%s" % (k, m, n, b) for k, m, b, n in rows if m != b})
        self.report["format_profile"] = {"toc_version": t.header["version"],
                                         "compression_block_size": t.header["compression_block_size"],
                                         "container_flags": t.header["container_flags"],
                                         "checked_against_base_containers": len({r[3] for r in rows}),
                                         "mismatches": mism}
        uf.need(len(rows) > 0, "no base package container to compare the container profile with")
        for m in mism:
            self.refusals.append("container profile mismatch " + m)

    # ---------------- analysis helpers
    def cls_path(self, raw: int, z: uf.ZenPackage) -> Optional[str]:
        k = uf.obj_kind(raw)
        if k == "ScriptImport":
            return self.script.path(raw)
        if k == "Export":
            e = z.exports[raw & uf.M32]
            return "local:" + str(e["name"])
        if k == "PackageImport":
            return "package-import"
        return None

    def resolve_import(self, owner: Dict[str, object], raw: int) -> Dict[str, object]:
        z: uf.ZenPackage = owner["zen"]
        k = uf.obj_kind(raw)
        if k == "Null":
            return {"kind": "Null", "status": "ok"}
        if k == "ScriptImport":
            p = self.script.path(raw)
            return {"kind": "Script", "target": p, "status": "ok" if p else "broken",
                    "why": None if p else "script object not in the game's ScriptObjects (missing C++ class/module)"}
        pkg_i, hash_i = uf.package_import_ref(raw)
        uf.need(pkg_i < len(z.imported_package_names), "import package index outside imported package names")
        tname = z.imported_package_names[pkg_i]
        tpid = uf.package_id_of(tname)
        phash = z.imported_public_export_hashes[hash_i]
        out = {"kind": "Package", "package": tname, "package_id": "%016x" % tpid, "public_export_hash": "%016x" % phash}
        if tpid in self.mod_pkgs:
            exps = [e for e in self.mod_pkgs[tpid]["zen"].exports if e["public_export_hash"] == phash]
            out.update(where="mod", status="ok" if len(exps) == 1 else "broken",
                       target_class=self.cls_path(exps[0]["cls"], self.mod_pkgs[tpid]["zen"]) if len(exps) == 1 else None,
                       target_export=exps[0]["name"] if len(exps) == 1 else None)
            if len(exps) != 1:
                out["why"] = "public export hash not exported exactly once by the mod package"
            return out
        owners = self.base_owner(tpid)
        if not owners:
            out.update(where="missing", status="broken",
                       why="package exists neither in the mod nor in any base container given")
            return out
        out["where"] = "base:" + ",".join(c.name for c in owners)
        store = self.base_store(tpid)
        out["base_store_shader_map_hashes"] = None if store is None else len(store["shader_map_hashes"])
        bz = self.base_header(tpid, "export-level import closure for mod package %s" % z.name)
        if bz is None:
            out.update(status="unverified-export", why="base package header not available yet (fetch plan)")
            return out
        exps = [e for e in bz.exports if e["public_export_hash"] == phash]
        out.update(status="ok" if len(exps) == 1 else "broken",
                   target_class=self.cls_path(exps[0]["cls"], bz) if len(exps) == 1 else None,
                   target_export=exps[0]["name"] if len(exps) == 1 else None)
        if len(exps) != 1:
            out["why"] = "base package does not export this public hash"
        return out

    # ---------------- passes
    def closure(self):
        """Structural imports (an export's class/super/template/outer) must resolve
        or the export cannot be created: refuse.  Plain object references that do
        not resolve become null at load: disclosed, not refused."""
        imports_checked = 0
        broken, missing_refs, unverified = [], [], 0
        self.edges: Dict[int, List[Dict[str, object]]] = {}
        for pid, p in sorted(self.mod_pkgs.items()):
            z = p["zen"]
            structural = set()
            for e in z.exports:
                for key in ("cls", "super", "template", "outer"):
                    raw = e[key]
                    if uf.obj_kind(raw) != "Null" and uf.obj_kind(raw) != "Export":
                        structural.add(raw)
            res = []
            for raw in z.import_map:
                r = self.resolve_import(p, raw)
                if r["kind"] == "Null":
                    continue
                imports_checked += 1
                r["structural"] = raw in structural
                res.append(r)
                row = {"from_package": z.name, "import": r}
                if r["status"] == "broken" and (r["structural"] or r["kind"] == "Script" or r.get("where") != "missing"):
                    broken.append(row)
                elif r["status"] == "broken":
                    missing_refs.append(row)
                elif r["status"] == "unverified-export":
                    unverified += 1
            for e in z.exports:
                for key in ("cls", "super", "template", "outer"):
                    raw = e[key]
                    if uf.obj_kind(raw) == "ScriptImport" and self.script.path(raw) is None:
                        broken.append({"from_package": z.name, "import": {"kind": "Script", "raw": "%016x" % raw,
                                                                          "why": "export %s %s not in ScriptObjects" % (e["name"], key)}})
            self.edges[pid] = res
        self.report["counts"]["imports_checked"] = imports_checked
        uf.need(imports_checked > 0, "zero imports checked")
        self.report["closure"] = {"broken": broken, "missing_references": missing_refs,
                                  "unverified_export_level": unverified,
                                  "verdict": "BROKEN" if broken else ("PACKAGE-LEVEL-OK, EXPORT-LEVEL-PENDING"
                                                                      if unverified else "CLOSED")}
        seen = set()
        for b in broken:
            msg = "broken import in %s -> %s (%s)" % (b["from_package"], b["import"].get("package") or b["import"].get("target") or b["import"].get("raw"), b["import"].get("why"))
            if msg not in seen:
                seen.add(msg)
                self.refusals.append(msg)
        names = sorted({m["import"]["package"] for m in missing_refs})
        self.report["closure"]["missing_reference_packages"] = names
        if unverified:
            self.incomplete.append("%d package imports need base headers for export-level proof" % unverified)

    def replacements(self):
        rows = []
        checked = 0
        for pid, p in sorted(self.mod_pkgs.items()):
            owners = self.base_owner(pid)
            if not owners:
                continue
            z = p["zen"]
            row = {"package": z.name, "base_containers": [c.name for c in owners]}
            store = self.base_store(pid)
            row["base_shader_map_hashes"] = None if store is None else len(store["shader_map_hashes"])
            row["mod_shader_map_hashes"] = len(p["store"]["shader_map_hashes"])
            if store is None:
                self.incomplete.append("base store entry for replaced package %s not available" % z.name)
            bz = self.base_header(pid, "replacement must keep base public exports: %s" % z.name)
            if bz is None:
                row["public_exports"] = "pending base header"
            else:
                checked += 1
                base_pub = {e["public_export_hash"] for e in bz.exports if e["public_export_hash"] not in (0, uf.M64)}
                mod_pub = {e["public_export_hash"] for e in z.exports if e["public_export_hash"] not in (0, uf.M64)}
                lost = sorted(base_pub - mod_pub)
                row["public_exports"] = {"base": len(base_pub), "mod": len(mod_pub), "missing_in_mod": ["%016x" % h for h in lost]}
                bcls = {e["name"]: self.cls_path(e["cls"], bz) for e in bz.exports}
                mcls = {e["name"]: self.cls_path(e["cls"], z) for e in z.exports}
                row["class_changes"] = sorted("%s: %s -> %s" % (n, bcls[n], mcls.get(n)) for n in bcls
                                              if n in mcls and bcls[n] != mcls[n])
                if lost:
                    self.refusals.append("replacement %s drops %d public exports base packages may import" % (z.name, len(lost)))
            rows.append(row)
        self.report["replacements"] = rows
        self.report["counts"]["replaced_packages"] = len(rows)
        self.report["counts"]["added_packages"] = len(self.mod_pkgs) - len(rows)
        self.report["counts"]["replacements_export_checked"] = checked

    def exports_of_class(self, classes: Set[str]):
        for pid, p in sorted(self.mod_pkgs.items()):
            z = p["zen"]
            for e in z.exports:
                if self.cls_path(e["cls"], z) in classes:
                    yield pid, p, e

    def inline_formats(self, z: uf.ZenPackage, data: bytes) -> List[str]:
        """Shader-format names a cook writes as FStrings inside inline shader maps
        (export payload), e.g. a PC cook's platform records.  Name-map entries
        are included too.  Target formats come from the game's own files."""
        found = {n for n in z.names if PC_SHADER_FORMAT_RE.match(n) or n in self.target_formats}
        payload = data[z.header_size:]
        for m in FSTRING_TOKEN_RE.finditer(payload):
            ln = struct.unpack_from("<i", m.group(0), 0)[0]
            tok = m.group(1).decode("ascii")
            if ln == len(tok) + 1 and (PC_SHADER_FORMAT_RE.match(tok) or tok in self.target_formats):
                found.add(tok)
        return sorted(found)

    @staticmethod
    def bytecode_markers(data: bytes) -> Dict[str, int]:
        """Counts of D3D shader-container magics (supporting evidence only)."""
        return {"DXBC": data.count(b"DXBC"), "DXIL": data.count(b"DXIL")}

    def material_verdict(self, pid: int, depth: int = 0, seen: Optional[Set[int]] = None) -> Dict[str, object]:
        """Shader source for the material exported by package pid (mod or base)."""
        seen = seen or set()
        if pid in seen or depth > 16:
            return {"verdict": "UNKNOWN", "why": "material parent cycle/too deep", "chain": []}
        seen.add(pid)
        if pid in self.mod_pkgs:
            p = self.mod_pkgs[pid]
            z = p["zen"]
            cls = {self.cls_path(e["cls"], z) for e in z.exports}
            hashes = p["store"]["shader_map_hashes"]
            formats = self.inline_formats(z, p["data"])
            target_inline = sorted(set(formats) & self.target_formats)
            mexp = [e for e in z.exports if self.cls_path(e["cls"], z) in (MATERIAL_CLASSES | MATERIAL_INSTANCE_CLASSES)]
            node = {"package": z.name, "where": "mod", "class": sorted(c for c in cls if c),
                    "object": (z.name + "." + mexp[0]["name"]) if mexp else None,
                    "store_shader_maps": len(hashes), "inline_shader_formats": formats,
                    "bytecode_markers": self.bytecode_markers(p["data"][z.header_size:]) if formats else None}
            own = bool(cls & MATERIAL_CLASSES) or bool(formats) or bool(hashes)
            if own:
                if target_inline:
                    return {"verdict": "NATIVE", "why": "carries inline shaders for the game's format %s" % target_inline, "chain": [node]}
                if hashes:
                    return {"verdict": "UNKNOWN", "why": "shader maps by hash; the game's shader library holding them is not verified", "chain": [node]}
                why = "own shader map needed; only %s shader code shipped, game uses %s" % (
                    ",".join(formats) or "no", ",".join(sorted(self.target_formats)) or "?")
                return {"verdict": "FALLBACK-DEFAULT-MATERIAL", "why": why, "chain": [node]}
            parents = self.parent_candidates(pid)
            node["parent_rule"] = "single material-class package import (metadata inference)"
            if len(parents) != 1:
                return {"verdict": "UNKNOWN", "why": "parent not identifiable (%d material-class imports)" % len(parents), "chain": [node]}
            sub = self.material_verdict(parents[0], depth + 1, seen)
            return {"verdict": sub["verdict"], "why": sub["why"], "chain": [node] + sub["chain"]}
        # base material
        owners = self.base_owner(pid)
        if not owners:
            return {"verdict": "UNKNOWN", "why": "parent package not found", "chain": []}
        store = self.base_store(pid)
        node = {"package_id": "%016x" % pid, "where": "base:" + owners[-1].name,
                "store_shader_maps": None if store is None else len(store["shader_map_hashes"])}
        if store is not None and store["shader_map_hashes"]:
            return {"verdict": "NATIVE", "why": "base material with the game's own cooked shader map", "chain": [node]}
        bz = self.base_header(pid, "material parent chain")
        if bz is None:
            return {"verdict": "UNKNOWN", "why": "base parent header not fetched yet", "chain": [node]}
        node["package"] = bz.name
        cls = {self.cls_path(e["cls"], bz) for e in bz.exports}
        bexp = [e for e in bz.exports if self.cls_path(e["cls"], bz) in (MATERIAL_CLASSES | MATERIAL_INSTANCE_CLASSES)]
        node["object"] = (bz.name + "." + bexp[0]["name"]) if bexp else None
        if cls & MATERIAL_CLASSES:
            return {"verdict": "UNKNOWN", "why": "base material without store shader maps", "chain": [node]}
        parents = self.parent_candidates(pid)
        if len(parents) != 1:
            return {"verdict": "UNKNOWN", "why": "base parent not identifiable", "chain": [node]}
        sub = self.material_verdict(parents[0], depth + 1, seen)
        return {"verdict": sub["verdict"], "why": sub["why"], "chain": [node] + sub["chain"]}

    def package_classes(self, pid: int) -> Optional[Set[str]]:
        if pid in self.mod_pkgs:
            z = self.mod_pkgs[pid]["zen"]
            return {self.cls_path(e["cls"], z) for e in z.exports}
        if pid in self.base_headers:
            z = self.base_headers[pid][1]
            return {self.cls_path(e["cls"], z) for e in z.exports}
        store = self.base_store(pid)
        if store is not None and store["shader_map_hashes"]:
            return {"base-material(shader-maps)"}
        return None

    def parent_candidates(self, pid: int) -> List[int]:
        if pid in self.mod_pkgs:
            z = self.mod_pkgs[pid]["zen"]
        else:
            z = self.base_headers[pid][1]
        out = []
        for name in z.imported_package_names:
            tp = uf.package_id_of(name)
            cls = self.package_classes(tp)
            if cls is None:
                # class unknown until its base header is fetched: plan that fetch (mod and
                # base materials alike), so an UNKNOWN chain always says what is missing.
                self.base_header(tp, "material parent identification")
                cls = self.package_classes(tp)
            if cls is None:
                out.append(tp)  # unknown class: keep as candidate so ambiguity is visible
                continue
            if cls & (MATERIAL_CLASSES | MATERIAL_INSTANCE_CLASSES) or "base-material(shader-maps)" in cls:
                out.append(tp)
        return out

    def visual(self):
        mats = []
        loss = 0
        unknown = 0
        mat_pids = set()
        for pid, p, e in self.exports_of_class(MATERIAL_CLASSES | MATERIAL_INSTANCE_CLASSES):
            if pid in mat_pids:
                continue
            mat_pids.add(pid)
            v = self.material_verdict(pid)
            row = {"package": p["zen"].name, "object": p["zen"].name + "." + e["name"],
                   "class": self.cls_path(e["cls"], p["zen"]),
                   "replaces_base": bool(self.base_owner(pid)), **v}
            if v["verdict"] == "FALLBACK-DEFAULT-MATERIAL":
                loss += 1
                row["what_you_see"] = ("drawn with the engine's default material (grey/white checker grid) "
                                       "instead of the mod's look")
            elif v["verdict"] == "UNKNOWN":
                unknown += 1
                row["what_you_see"] = "not provable on Mac; may render wrong"
            else:
                row["what_you_see"] = "drawn with the game's own shaders"
            mats.append(row)
        self.report["counts"]["materials_checked"] = len(mats)
        # meshes -> materials they import
        bad = {r["package"] for r in mats if r["verdict"] != "NATIVE"}
        meshes = []
        for pid, p, e in self.exports_of_class(MESH_CLASSES):
            used = [n for n in p["zen"].imported_package_names]
            hit = sorted(n for n in used if n in bad)
            meshes.append({"mesh": p["zen"].name, "object": p["zen"].name + "." + e["name"],
                           "replaces_base": bool(self.base_owner(pid)),
                           "materials_with_visual_loss": hit})
        self.report["counts"]["meshes_checked"] = len(meshes)
        tex = self.textures(mats)
        live_tex = [t for t in tex["mod"] if not t["only_used_by_fallback_materials"]]
        tex["textures_drawn_by_working_materials"] = len(live_tex)
        if live_tex and tex["verdict"].startswith("UNVERIFIED"):
            self.incomplete.append("%d textures are drawn by working materials but their console layout is unproven" % len(live_tex))
        if live_tex and tex["verdict"].startswith("LAYOUT-DIFFERS"):
            loss += 1
            tex["what_you_see"] = "textures drawn by working materials will look wrong (scrambled/blocky) until converted"
        self.report["visual"] = {"materials": mats, "meshes": meshes, "textures": tex,
                                 "materials_with_visual_loss": loss, "materials_unknown": unknown}
        if unknown:
            self.incomplete.append("%d materials cannot be judged on Mac yet" % unknown)

    def texture_profile(self, z: uf.ZenPackage, data: bytes, e: Dict[str, object]) -> Dict[str, object]:
        x = uf.export_data(data, z, e)
        pfs = [n for n in z.names if n.startswith("PF_")]
        out = {"pixel_formats": pfs}
        for pf in pfs:
            needle = struct.pack("<i", len(pf) + 1) + pf.encode() + b"\0"
            at = x.find(needle)
            if at < 12:
                continue
            sx, sy, packed = struct.unpack_from("<iiI", x, at - 12)
            if not (0 < sx <= 32768 and 0 < sy <= 32768):
                continue
            q = at + len(needle)
            has_opt = bool(packed & (1 << 30))
            row = {"pixel_format": pf, "size": [sx, sy], "packed": packed, "has_opt_data": has_opt,
                   "cubemap": bool(packed & (1 << 31)), "slices": packed & ((1 << 29) - 1)}
            if has_opt and q + 8 <= len(x):
                row["ext_data"], row["num_mips_in_tail"] = struct.unpack_from("<II", x, q)
                q += 8
            if q + 8 <= len(x):
                row["first_mip"], row["num_mips"] = struct.unpack_from("<ii", x, q)
            sizes = [b[2] for b in z.bulk_data if b[2] > 0]
            lin = linear_mip_sizes(pf, sx, sy, len(sizes))
            row["bulk_mip_sizes"] = sizes
            row["bulk_sizes_match_linear_layout"] = (lin is not None and sizes == lin[:len(sizes)]) if sizes else None
            out.setdefault("platform_data", []).append(row)
        return out

    def textures(self, mats):
        used_by_loss = set()
        for r in mats:
            if r["verdict"] == "FALLBACK-DEFAULT-MATERIAL":
                pid = uf.package_id_of(r["package"])
                for n in self.mod_pkgs[pid]["zen"].imported_package_names:
                    used_by_loss.add(n)
        rows = []
        for pid, p, e in self.exports_of_class(TEXTURE_CLASSES):
            prof = self.texture_profile(p["zen"], p["data"], e)
            rows.append({"texture": p["zen"].name, "object": p["zen"].name + "." + e["name"], **prof,
                         "only_used_by_fallback_materials": p["zen"].name in used_by_loss})
        self.report["counts"]["textures_checked"] = len(rows)
        # base sample profile (from fetched base headers that are textures)
        base_rows = []
        for bpid, (cname, bz) in sorted(self.base_headers.items()):
            for e in bz.exports:
                if self.cls_path(e["cls"], bz) in TEXTURE_CLASSES:
                    base_rows.append({"texture": bz.name, "container": cname, **self.texture_profile(bz, bz._data, e)})
        mod_opt = any(pd.get("has_opt_data") for r in rows for pd in r.get("platform_data", []))
        base_opt = [pd.get("has_opt_data") for r in base_rows for pd in r.get("platform_data", [])]
        base_lin = [pd.get("bulk_sizes_match_linear_layout") for r in base_rows for pd in r.get("platform_data", [])]
        base_fmts = sorted({pd["pixel_format"] for r in base_rows for pd in r.get("platform_data", [])})
        mod_fmts = sorted({pd["pixel_format"] for r in rows for pd in r.get("platform_data", [])})
        if not base_rows:
            verdict = "UNVERIFIED: no base texture sample yet (fetch plan); texture data layout on the console unknown"
        elif base_opt and all(base_opt) and not mod_opt:
            verdict = "LAYOUT-DIFFERS: game textures carry platform layout data, mod textures do not -> expect wrong-looking textures"
        elif base_lin and any(v is False for v in base_lin):
            verdict = "LAYOUT-DIFFERS: game texture mip sizes are padded (tiled), mod mips are linear -> expect wrong-looking textures"
        else:
            verdict = "NO-DIFFERENCE-FOUND in headers (pixel data layout itself is not provable on Mac)"
        if base_rows and set(mod_fmts) - set(base_fmts):
            verdict += "; formats not seen in base sample: %s" % ",".join(sorted(set(mod_fmts) - set(base_fmts)))
        return {"mod": rows, "base_sample": base_rows, "verdict": verdict,
                "mod_pixel_formats": mod_fmts, "base_pixel_formats": base_fmts}

    def plan_texture_samples(self, limit: int = 8):
        """Load (or plan a fetch of) a few base texture-like leaf packages, so the
        target texture layout is learned from the game's own files.  Candidates,
        in order: leaves imported by the base versions of replaced packages,
        leaves imported by base packages the mod imports, then any store leaf
        without shader maps (deterministic order)."""
        want: List[int] = []

        def add_from(pids):
            for ip in pids:
                if len(want) >= limit:
                    return
                if ip in self.mod_pkgs or ip in want or not self.base_owner(ip):
                    continue
                st = self.base_store(ip)
                if st is not None and not st["imported_packages"] and not st["shader_map_hashes"]:
                    want.append(ip)

        for pid in sorted(self.mod_pkgs):
            if self.base_owner(pid):
                st = self.base_store(pid)
                if st:
                    add_from(st["imported_packages"])
        for pid, p in sorted(self.mod_pkgs.items()):
            for name in p["zen"].imported_package_names:
                st = self.base_store(uf.package_id_of(name))
                if st:
                    add_from(st["imported_packages"])
        for c in self.base:
            if c.header and len(want) < limit:
                add_from(sorted(c.header.store))
        for ip in want:
            self.base_header(ip, "base texture sample for the target texture layout profile")
        self.report["counts"]["base_texture_candidates"] = len(want)

    # ---------------- run
    def run(self) -> int:
        self.load_base()
        self.load_mod()
        self.closure()
        self.replacements()
        self.plan_texture_samples()
        self.visual()
        self.report["fetch_plan"] = self.fetch_plan
        self.report["fetched_used"] = self.fetched_used
        self.report["counts"]["items_checked_total"] = sum(
            self.report["counts"].get(k, 0) for k in ("mod_chunks_iohash_verified", "imports_checked",
                                                      "materials_checked", "textures_checked", "meshes_checked"))
        if self.report["counts"]["items_checked_total"] == 0:
            self.refusals.append("zero items checked")
        if self.refusals:
            code, verdict = EXIT_REFUSED, "REFUSED"
        elif self.incomplete:
            code, verdict = EXIT_INCOMPLETE, "INCOMPLETE"
        elif self.report["visual"]["materials_with_visual_loss"]:
            code, verdict = EXIT_VISUAL_LOSS, "ADMITTED-WITH-VISUAL-LOSS"
        else:
            code, verdict = EXIT_OK, "ADMITTED"
        self.report["verdict"] = verdict
        self.report["exit_code"] = code
        return code


def linear_mip_sizes(pf: str, w: int, h: int, n: int) -> Optional[List[int]]:
    out = []
    for _ in range(max(n, 0)):
        if pf in BLOCK_BYTES:
            out.append(max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * BLOCK_BYTES[pf])
        elif pf in PIXEL_BYTES:
            out.append(w * h * PIXEL_BYTES[pf])
        else:
            return None
        w, h = max(1, w // 2), max(1, h // 2)
    return out


def _reach(graph: Dict[str, Dict[str, object]]) -> Tuple[Dict[str, Set[str]], Dict[str, Set[str]]]:
    """reach[p]: mod packages p imports (transitively).  by[p]: REPLACED mod
    packages (other than p) that import p transitively.  A hard import loads with
    its importer, so whenever the game loads a replaced package from the mod,
    everything it reaches must be loaded too (data only, from the mod's files)."""
    reach: Dict[str, Set[str]] = {}
    for root in graph:
        seen: Set[str] = set()
        stack = [root]
        while stack:
            x = stack.pop()
            for y in graph.get(x, {}).get("imports_in_mod", []):
                if y in graph and y not in seen:
                    seen.add(y)
                    stack.append(y)
        reach[root] = seen
    by: Dict[str, Set[str]] = {p: set() for p in graph}
    for rp, info in graph.items():
        if info.get("replaces_base"):
            for q in reach[rp]:
                if q != rp:
                    by[q].add(rp)
    return reach, by


def diag_entries(r: Dict[str, object], cap_link: int = 8, cap_rep: int = 3, cap_roots: int = 6,
                 cap_children: int = 8, cap_tex: int = 2, cap_mesh: int = 2) -> List[Dict[str, object]]:
    """Choose a small, deterministic, generic probe set from the report (data only).

    g="R" probes are objects in packages that REPLACE a game package: the game
    loads them only if it uses that asset, so on their own they prove nothing.
    "Linkers" are replaced packages that import many other mod packages (e.g. a
    replaced mesh); they come first.  g="N" probes are objects in the mod's NEW
    packages; they are only chosen when a chosen linker imports them, and each
    probe lists (b=) the replaced probes that import it.  Live rule (zzintake):
    an object imported by a found replaced probe MUST be found (else MISSING);
    an object nothing found imports is "not loaded" (inconclusive), never FAIL.
    Objects reached only from themselves are skipped."""
    v = r.get("visual", {})
    graph = r.get("load_graph") or {}
    reach, by = _reach(graph)
    code = {"FALLBACK-DEFAULT-MATERIAL": "F", "NATIVE": "N"}
    cands: List[Dict[str, object]] = []
    for m in sorted(v.get("materials", []), key=lambda m: m["package"]):
        chain = m.get("chain") or []
        if not m.get("object"):
            continue
        root = chain[-1].get("package", "?") if chain else "?"
        if len(chain) == 1:
            cands.append({"o": m["object"], "k": "M", "v": code.get(m["verdict"], "U"), "pkg": m["package"],
                          "rep": bool(m.get("replaces_base")), "root": root})
        elif len(chain) >= 2 and chain[1].get("object"):
            cands.append({"o": m["object"], "k": "I", "v": code.get(m["verdict"], "U"), "p": chain[1]["object"],
                          "pkg": m["package"], "rep": bool(m.get("replaces_base")), "root": root})
    for t in sorted(v.get("textures", {}).get("mod", []), key=lambda t: t["texture"]):
        if t.get("object"):
            cands.append({"o": t["object"], "k": "T", "v": "-", "pkg": t["texture"],
                          "rep": bool(graph.get(t["texture"], {}).get("replaces_base"))})
    for x in sorted(v.get("meshes", []), key=lambda x: x["mesh"]):
        if x.get("object"):
            cands.append({"o": x["object"], "k": "S", "v": "-", "pkg": x["mesh"], "rep": bool(x.get("replaces_base"))})
    cand_pkgs = {c["pkg"] for c in cands}
    # 1. linkers: replaced objects that import the most other probe-able mod packages
    # (at least half as much of the mod as the biggest one), so a replaced object that
    # reaches little and that no other replaced package imports is not a linker.
    linkers = [c for c in cands if c["rep"] and (reach.get(c["pkg"], set()) & cand_pkgs) - {c["pkg"]}]
    top = max([len(reach.get(c["pkg"], set())) for c in linkers] or [0])
    linkers = [c for c in linkers if 2 * len(reach.get(c["pkg"], set())) > top]
    linkers.sort(key=lambda c: (-len(reach.get(c["pkg"], set())), c["o"]))
    linkers = linkers[:cap_link]
    link_pkgs = {c["pkg"] for c in linkers}

    def linked(c):
        return len(by.get(c["pkg"], set()) & link_pkgs)
    chosen = {c["o"] for c in linkers}
    # 2. replaced objects that a chosen linker imports (parent check on a replaced object)
    rep = [c for c in cands if c["rep"] and c["o"] not in chosen and linked(c)]
    rep.sort(key=lambda c: (0 if c.get("p") else 1, -linked(c), c["o"]))
    rep = rep[:cap_rep]
    chosen |= {c["o"] for c in rep}
    # 3. the mod's own new objects that a chosen linker imports, most-linked first
    new = [c for c in cands if not c["rep"] and c["o"] not in chosen and linked(c)]
    new.sort(key=lambda c: (-linked(c), c["o"]))
    caps = {"M": cap_roots, "I": cap_children, "T": cap_tex, "S": cap_mesh}
    taken: Dict[str, int] = {}
    per_root: Dict[str, int] = {}
    picked = []
    for c in new:
        if taken.get(c["k"], 0) >= caps[c["k"]]:
            continue
        if c["k"] == "I":
            if per_root.get(c["root"], 0) >= 2:
                continue
            per_root[c["root"]] = per_root.get(c["root"], 0) + 1
        taken[c["k"]] = taken.get(c["k"], 0) + 1
        picked.append(c)
    if not linkers:
        # No replaced package imports anything the mod adds (e.g. a pure-add mod): no
        # load graph to reason with, so probe the new objects unlinked (all must be found).
        picked = [c for c in cands if not c["rep"]][:cap_roots + cap_children + cap_tex + cap_mesh]
    entries = linkers + rep + picked
    index = {c["pkg"]: i + 1 for i, c in enumerate(entries) if c["rep"]}
    out = []
    for c in entries:
        e = {"o": c["o"], "k": c["k"], "v": c["v"], "g": "R" if c["rep"] else "N",
             "b": sorted(index[p] for p in by.get(c["pkg"], set()) if p in index and p != c["pkg"])}
        if c.get("p"):
            e["p"] = c["p"]
        out.append(e)
    return out


def emit_diag(r: Dict[str, object], outdir: str) -> Dict[str, object]:
    entries = diag_entries(r)
    uf.need(len(entries) > 0, "zero diagnostic probes selected")
    tmpl = uf.read_bytes(os.path.join(os.path.dirname(os.path.abspath(__file__)), "zzintake_template.lua")).decode()

    def q(s):
        uf.need('"' not in s and "\\" not in s and "\n" not in s, "unsafe object path for Lua literal")
        return '"' + s + '"'
    rows = []
    for e in entries:
        parts = ["o=" + q(e["o"]), "k=" + q(e["k"]), "v=" + q(e["v"]), "g=" + q(e["g"]),
                 "b={" + ",".join(str(int(i)) for i in e["b"]) + "}"]
        if e.get("p"):
            parts.append("p=" + q(e["p"]))
        rows.append(" {" + ",".join(parts) + "},")
    text = tmpl.replace("--@@DATA@@", "\n".join(rows))
    scripts = os.path.join(outdir, "zzintake", "Scripts")
    os.makedirs(scripts, exist_ok=True)
    with open(os.path.join(scripts, "main.lua"), "w", newline="\n") as f:
        f.write(text)
    with open(os.path.join(outdir, "zzintake", "enabled.txt"), "w") as f:
        pass
    return {"probes": len(entries), "replaced_probes": sum(1 for e in entries if e["g"] == "R"),
            "new_probes": sum(1 for e in entries if e["g"] == "N"),
            "linked_probes": sum(1 for e in entries if e["b"]),
            "main_lua_bytes": len(text.encode()), "dir": os.path.join(outdir, "zzintake")}


def summary_text(r: Dict[str, object]) -> str:
    c = r["counts"]
    v = r.get("visual", {})
    lines = ["VERDICT %s (exit %s)  [Mac-only]" % (r.get("verdict"), r.get("exit_code")),
             "checked: %d mod chunks (IoHash), %d packages (%d new, %d replacing game packages), %d imports, "
             "%d materials, %d textures, %d meshes" % (c.get("mod_chunks_iohash_verified", 0), c.get("mod_packages", 0),
                                                        c.get("added_packages", 0), c.get("replaced_packages", 0),
                                                        c.get("imports_checked", 0), c.get("materials_checked", 0),
                                                        c.get("textures_checked", 0), c.get("meshes_checked", 0)),
             "game shader format (from the game's own shader library names): %s" % ",".join(r.get("target_shader_formats", [])),
             "import closure: %s" % r.get("closure", {}).get("verdict")]
    mr = r.get("closure", {}).get("missing_reference_packages") or []
    if mr:
        lines.append("missing references (load as empty, disclosed): %s" % ", ".join(mr))
    mats = v.get("materials", [])
    loss = [m for m in mats if m["verdict"] == "FALLBACK-DEFAULT-MATERIAL"]
    if loss:
        lines.append("VISUAL LOSS (disclosed): %d of %d materials will draw as the engine default "
                     "grey/white checker grid:" % (len(loss), len(mats)))
        for m in loss:
            root = m["chain"][-1].get("package", "?") if m["chain"] else "?"
            lines.append("  - %s  (shader source: %s)" % (m["package"], root))
    unk = [m for m in mats if m["verdict"] == "UNKNOWN"]
    if unk:
        lines.append("NOT PROVABLE ON MAC: %d materials" % len(unk))
    hit = [m for m in v.get("meshes", []) if m["materials_with_visual_loss"]]
    if hit:
        lines.append("meshes affected: %d of %d" % (len(hit), len(v.get("meshes", []))))
    if v.get("textures"):
        lines.append("textures: %s" % v["textures"]["verdict"])
    for x in r.get("refusals", []):
        lines.append("REFUSED: " + x)
    for x in r.get("incomplete", [])[:20]:
        lines.append("INCOMPLETE: " + x)
    if r.get("fetch_plan"):
        lines.append("fetch plan: %d base chunks, %d bytes" % (len(r["fetch_plan"]), sum(p["length"] for p in r["fetch_plan"])))
    return "\n".join(lines)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--mod-utoc", required=True)
    ap.add_argument("--mod-ucas", required=True)
    ap.add_argument("--base-utoc", action="append", required=True,
                    help="base .utoc (optionally utoc=ucas for a locally readable base ucas)")
    ap.add_argument("--base-container-header", action="append",
                    help="<container stem>=<captured container header chunk file>")
    ap.add_argument("--base-fetched", help="dir of integrator-fetched base chunks <stem>/<chunkid>.bin")
    ap.add_argument("--script-objects-utoc", required=True)
    ap.add_argument("--script-objects-ucas", required=True)
    ap.add_argument("--verify-bulk", action="store_true", help="also IoHash-verify bulk (texture mip) chunks")
    ap.add_argument("--out", required=True, help="report JSON path")
    ap.add_argument("--fetch-plan", help="write the base fetch plan JSON here")
    ap.add_argument("--emit-diag", help="write the generated zzintake diagnostic mod folder under this dir")
    a = ap.parse_args(argv)
    # Never leave an older report (or plan) in place: a failed run must not look like a fresh one.
    for stale in (a.out, a.fetch_plan):
        if stale and os.path.exists(stale):
            os.remove(stale)
    intake = Intake(a)
    try:
        code = intake.run()
    except (uf.FormatError, OSError, struct.error) as e:
        intake.report["refusals"].append("fatal: %s" % e)
        intake.report["verdict"] = "REFUSED"
        intake.report["exit_code"] = code = EXIT_REFUSED
    except Exception as e:  # any other parse error (bad FString, bad index...): fail closed, still write a report
        intake.report["refusals"].append("fatal (unexpected %s): %s" % (type(e).__name__, e))
        intake.report["verdict"] = "REFUSED"
        intake.report["exit_code"] = code = EXIT_REFUSED
    if a.emit_diag and intake.report.get("visual") and code != EXIT_REFUSED:
        try:
            intake.report["diag"] = emit_diag(intake.report, a.emit_diag)
        except uf.FormatError as e:
            intake.report["diag"] = {"error": str(e)}
    with open(a.out, "w") as f:
        json.dump(intake.report, f, indent=1, sort_keys=True, default=str)
    if a.fetch_plan:
        with open(a.fetch_plan, "w") as f:
            json.dump({"schema": 1, "label": "Mac-only plan; integrator performs read-only FTP range reads",
                       "chunks": intake.report.get("fetch_plan", [])}, f, indent=1)
    print(summary_text(intake.report))
    return code


if __name__ == "__main__":
    sys.exit(main())
