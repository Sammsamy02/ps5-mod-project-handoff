#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""merge_catalogs.py -- merge the two REAL catalog files into one checker catalog.

MAC-ONLY HOST TOOL. Reads (read-only):
    ../catalog/globals.json   ({catalog_version, ue4ss_version, ue4ss_source,
                                citation_tree, entries: [...]} since catalog
                                version 2026-10-07-v2; a bare JSON array is
                                still accepted for older catalogs)
    ../catalog/types.json     (same wrapper; per-type entries inside)

Writes:
    ./catalog-real-merged.json   (checker.py schema: {catalog_version, entries,
                                  ue4ss_version, ue4ss_source, citation_tree})

Merge rules (documented for provenance):
  * globals.json entries pass through unchanged (including their
    upstream_reference citations); checker.py derives kind from
    the name shape ("A.B" -> static, otherwise global).
  * types.json entries are flattened:
      - one "type"-kind entry per source type (name = type name), carrying the
        type-level ps5_runtime_status and notes (plus upstream_source_file);
      - one "method"-kind entry per listed method ("Type:Method"), typed to the
        owner; methods have no own ps5_runtime_status in the source catalog, so
        they inherit the owning type's status verbatim (including empty string).
        upstream_implementation citations ride along when present.
  * metamethods are not flattened: they are never invoked via the ./: syntax the
    checker scans.
  * Exact-name collisions between the two files are fused, not refused: the
    string constructors FString / FUtf8String / FAnsiString appear in BOTH
    files (globals.json documents the constructor function, types.json the
    type + methods). They become ONE "type"-kind entry carrying the
    constructor's signature/registration_sites; checker.py resolves
    FString('x') constructor calls through its lookup_global -> types
    fallback, so nothing is lost. Any OTHER unexpected collision is refused.
  * types.json sometimes lists an inherited method AND the type's own override
    under the same name (UScriptStruct:IsValid, AActor:GetWorld). Those intra-type
    duplicates are merged to a single entry, keeping the own override (the last
    listing, which is the type-specific implementation).
  * The two files' metadata (catalog_version / ue4ss_version / ue4ss_source /
    citation_tree) must agree and is propagated into the merged catalog.

No FTP, no payloads, no launches, no console contact, no catalog edits: the two
source files are opened read-only and left untouched.
"""

from __future__ import annotations

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GLOBALS_PATH = os.path.join(HERE, "..", "catalog", "globals.json")
TYPES_PATH = os.path.join(HERE, "..", "catalog", "types.json")
OUT_PATH = os.path.join(HERE, "catalog-real-merged.json")
META_KEYS = ("catalog_version", "ue4ss_version", "ue4ss_source", "citation_tree")


def load_catalog(path):
    """Accept {metadata..., entries: [...]} (v2) or a bare JSON array (v1)."""
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    if isinstance(data, list):
        return data, {}
    if isinstance(data, dict) and isinstance(data.get("entries"), list):
        meta = {k: data[k] for k in META_KEYS if k in data}
        return data["entries"], meta
    raise SystemExit(
        "error: %s must be a JSON array or an {entries: [...]} wrapper object"
        % os.path.basename(path))


def main():
    globals_entries, g_meta = load_catalog(GLOBALS_PATH)
    types_entries, t_meta = load_catalog(TYPES_PATH)
    meta = g_meta
    if g_meta and t_meta and g_meta != t_meta:
        for k in META_KEYS:
            if g_meta.get(k) != t_meta.get(k):
                raise SystemExit(
                    "error: catalog metadata mismatch on %r (%r vs %r)"
                    % (k, g_meta.get(k), t_meta.get(k)))

    entries = []
    seen = {}
    deduped_overrides = set()

    def claim(name, source):
        prev = seen.get(name)
        if prev is not None and prev != source:
            if prev == "globals.json" and source == "types.json:type":
                pass  # known fusion case (FString-family constructors)
            else:
                raise SystemExit(
                    "error: catalog name collision on %r (%s vs %s)"
                    % (name, prev, source))
        seen[name] = source

    def upsert(entry, source):
        """claim() + replace an existing same-source entry (override dedupe)."""
        claim(entry["name"], source)
        for i, e in enumerate(entries):
            if e["name"] == entry["name"]:
                deduped_overrides.add(entry["name"])
                entries[i] = entry  # keep the later, type-specific listing
                return
        entries.append(entry)

    for e in globals_entries:
        if not isinstance(e, dict) or "name" not in e:
            continue
        claim(e["name"], "globals.json")
        entries.append(e)

    n_types = n_methods = 0
    fused = set()
    for t in types_entries:
        if not isinstance(t, dict) or "type" not in t:
            continue
        tname = t["type"]
        status = t.get("ps5_runtime_status", "")
        claim(tname, "types.json:type")
        type_entry = {
            "name": tname,
            "kind": "type",
            "category": t.get("category", ""),
            "ps5_runtime_status": status,
            "notes": t.get("notes", ""),
            "parent_type": t.get("parent_type"),
            "upstream_source_file": t.get("upstream_source_file"),
            "source": "types.json(type)",
        }
        # Fuse a same-named globals.json constructor entry (FString family).
        g_hit = next((e for e in entries if e["name"] == tname), None)
        if g_hit is not None:
            fused.add(tname)
            type_entry["constructor_signature"] = g_hit.get("signature", "")
            type_entry["registration_sites"] = g_hit.get("registration_sites", [])
            type_entry["upstream_reference"] = g_hit.get("upstream_reference")
            if g_hit.get("notes"):
                type_entry["notes"] = (
                    (type_entry["notes"] + " | " if type_entry["notes"] else "")
                    + "[globals.json constructor] " + g_hit["notes"])
            entries[entries.index(g_hit)] = type_entry
        else:
            entries.append(type_entry)
        n_types += 1
        for m in t.get("methods", []):
            mname = "%s:%s" % (tname, m.get("name"))
            upsert({
                "name": mname,
                "kind": "method",
                "type": tname,
                "category": t.get("category", ""),
                # source methods carry no own status -> inherit the type's
                "ps5_runtime_status": status,
                "notes": m.get("notes", ""),
                "lua_signature": m.get("lua_signature", ""),
                "inherited_from": m.get("inherited_from"),
                "upstream_implementation": m.get("upstream_implementation"),
                "source": "types.json(method)",
            }, "types.json:method")
            n_methods += 1

    blank = sum(1 for e in entries
                if not (e.get("ps5_runtime_status") or "").strip())
    doc = {
        "catalog_version": (
            "real-catalog-merged: globals.json(%d entries, passthrough) + "
            "types.json(%d types flattened + %d method entries, methods "
            "inherit type ps5_runtime_status); %d/%d merged entries have "
            "EMPTY ps5_runtime_status"
            % (len(globals_entries), n_types, n_methods, blank, len(entries))),
        "entries": entries,
    }
    for k, v in meta.items():
        doc["source_" + k if k == "catalog_version" else k] = v
    with open(OUT_PATH, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=1)
        fh.write("\n")

    print("merged %d globals + %d type entries + %d method entries = %d total"
          % (len(globals_entries), n_types, n_methods, len(entries)))
    if deduped_overrides:
        print("intra-type override duplicates collapsed to own override: %s"
              % ", ".join(sorted(deduped_overrides)))
    if fused:
        print("globals.json constructor entries fused into type entries: %s"
              % ", ".join(sorted(fused)))
    print("entries with empty ps5_runtime_status: %d/%d" % (blank, len(entries)))
    print("wrote %s" % os.path.abspath(OUT_PATH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
