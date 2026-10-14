# CONTRACT PROPOSAL — Pre-Download Compatibility Badge via compat-checker

**Status:** proposal only (nothing integrated, nothing sent, nothing downloaded).
**Scope guarantee:** compat-checker is a Mac-only static host tool. No FTP, no
payloads, no launches, no console contact, no mod edits. Any manager
integration inherits the same guarantee: the badge is computed from text that
is already on the host (or text the manager already shows); it never triggers
a console operation.

---

## 1. The gap

`checker.py` grades a mod by statically scanning its `.lua` sources against
the runtime API catalog. That works **after** download (the files exist on
disk). BEFORE download, the mod manager only has:

1. the Nexus mod **description** (markdown/HTML text), and
2. a **file-list preview** (archive entry names + sizes, e.g. from the
   Nexus "preview file list" endpoint — names only, no content).

A static source scan is impossible without sources. This proposal adds a
two-phase contract so the manager can still show a badge honestly.

## 2. Two-phase badge flow

### Phase 0 — PRE-DOWNLOAD badge (no sources): `--mode predownload`

The manager passes the description text and file list to the checker. The
checker runs a **word-boundary mention scan** of catalog API names
(`RegisterHook`, `ExecuteAsync`, `ImGui.Begin`, `FName:ToString`, …) plus
high-signal phrases ("requires ImGui", "custom UI", "network/socket",
"external DLL") over the description, and inspects the file list for markers
(`Scripts/*.lua`, `UEVR`, `dll`, `pak`). Output is a **provisional** badge
with `confidence: "description-only"`:

- no risky mentions + `.lua` files present → `grade: "unverified-likely-ok"`
- mentions APIs whose catalog status is `missing`/`behaves-differently` →
  `grade: "likely-partly-compatible"` or `"likely-unsupported"` (same central
  heuristic: registration APIs or >3 mentions), listing the mentioned APIs as
  `evidence: "description-mention"` — never as call sites
- no `.lua` detected or description mentions nothing → `grade: "unknown"`

Phase 0 NEVER upgrades to green. The only positive badge it can show is
"unverified", because descriptions routinely under-report or over-report API
usage. The UI must render the confidence level next to the badge.

### Phase 1 — POST-DOWNLOAD PRE-ENABLE GATE: `--mode scan` (current tool)

The manager already unpacks archives into a staging area before installing.
Run `checker.py <staged_lua_path> <catalog.json> --json badge.json` there and
show the full badge (grade + blockers + runtime-side workarounds) before the
user enables the mod. Result is cached (see §5) so it is computed once per
(mod version, catalog version).

## 3. Metadata needed (the actual ask)

### 3.1 From the runtime catalog (owner: catalog agents)

The checker already consumes the minimal schema
`{name, category, ps5_runtime_status}` and optional
`kind / type / workaround / notes`. For the badge contract it additionally
needs:

| Field | Why |
|---|---|
| `catalog_version` (wrapper `{catalog_version, entries}`) | badge cache key + reproducibility; the checker already reports it |
| stable `name` grammar (`Global`, `Type:Method`, `Type.Member`) | lets Phase 0 build one regex per entry |
| `workaround` text (optional) | shown verbatim in the "why?" panel |

### 3.2 From the mod (owner: mod author, optional but recommended)

An optional `compat_manifest.json` shipped at the archive root, filled in by
the author or by a one-time Phase 1 scan the manager offers to submit back:

```json
{
  "schema": 1,
  "entrypoints": ["Scripts/main.lua"],
  "declared_apis": ["RegisterHook", "ExecuteAsync", "ImGui.Begin"],
  "framework": "UEVR-Lua",
  "min_runtime_version": "0.43"
}
```

`declared_apis` lets Phase 0 grade exactly (mention scan is then confirmed by
author declaration), and `entrypoints` lets the manager scan only the files
that matter on huge mods.

### 3.3 From the manager (owner: manager branch)

- mod identity tuple: `{nexus_mod_id, file_id, version, archive_sha256}`
- the description text + file-list preview it already fetched
- catalog pin: which `catalog.json` version the badge was computed against
- a small badge cache store (§5)

## 4. Wire contract

### Request (manager → checker), Phase 0

```json
{
  "schema": 1,
  "mode": "predownload",
  "mod": {"nexus_mod_id": 12345, "file_id": 67890, "version": "1.2.0",
          "archive_sha256": null},
  "description_text": "<plain text of the Nexus description>",
  "file_list": ["Scripts/main.lua", "Scripts/config.lua", "README.md"],
  "compat_manifest": null,
  "catalog": "<path or inline pinned catalog>"
}
```

### Response (checker → manager), both phases

```json
{
  "schema": 1,
  "badge": {
    "grade": "unsupported",
    "grade_reason": "...",
    "confidence": "source-scan" | "description-only" | "author-declared",
    "catalog_version": "2026-10-07.1",
    "computed_at": "2026-10-07T14:20:00+08:00",
    "blockers": [
      {"api": "ExecuteAsync", "status": "missing", "central": true,
       "evidence": [{"file": "Scripts/main.lua", "line": 53}]
                 | [{"kind": "description-mention"}],
       "runtime_workaround": "shim onto ExecuteInGameThread (serialized) ..."}
    ],
    "risks": [], "behaves_differently": [], "unknown_apis": [],
    "totals": {"total_calls": 31, "matched": 26, "unmatched": 5}
  },
  "mac_only": true,
  "console_contact": false
}
```

The `grade` vocabulary maps 1:1 to the tool today: `compatible`,
`partly-compatible`, `unsupported`, `unknown`, plus Phase-0-only
`unverified-likely-ok` / `likely-*` variants. The existing JSON report
(`grade, total_calls, matched_calls, unmatched_calls, missing,
behaves_differently, unknown, details_per_file`) is a superset of this badge
payload, so the manager can pass it through unchanged.

### Badge UI mapping

| grade | badge | tooltip |
|---|---|---|
| compatible | green check "PS5 OK" | all cataloged APIs console-implemented (source scan) |
| partly-compatible | amber | works, listed APIs missing/different; expandable list |
| unsupported | red X | central missing API; list + runtime-side workarounds |
| unknown | gray "?" | uncataloged APIs; nothing verified |
| unverified-likely-* (Phase 0) | outline icon | description-only estimate; confirm after download |

## 5. Caching and invalidation

- cache key: `(nexus_mod_id, file_id, archive_sha256, catalog_version, checker_version)`
- hit → reuse badge, zero recompute
- invalidate on: new mod file id, catalog version bump, checker version bump
- Phase 0 badges are always recomputed on new file_id (descriptions change)

## 6. Failure modes and honesty rules

1. Unreadable/absent sources → `unknown`, never a guess.
2. Description-only results are never shown as verified; the confidence field
   is mandatory in the UI.
3. Catalog entries with unrecognized `ps5_runtime_status` are surfaced as a
   catalog problem (treated behaves-differently, flagged in `notes`), not
   silently passed.
4. Unresolved static calls (untyped method calls) are reported but never
   counted against a grade.
5. The checker never edits, executes, or uploads the mod; every suggestion is
   runtime-side. Badges are computed on the host (Mac-only) with zero console
   contact.

## 7. Implementation checklist (when approved)

- [ ] add `--mode predownload` (description + file-list scan) to checker.py
- [ ] accept `compat_manifest.json` input in both modes
- [ ] emit the §4 badge envelope (`schema:1`) alongside today's full report
- [ ] manager: badge cache table + UI mapping + "scan now" pre-enable gate
- [ ] pin catalog version in manager settings; bump policy documented

Nothing in this proposal requires touching the mod manager branch, any
`launch11-combined/units` file, or any `outputs/2026-10-07-l1*` path; it is a
proposal written from the side-project directory only.
