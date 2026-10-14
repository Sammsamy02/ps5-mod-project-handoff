# CONTRACT-PROPOSAL — Interface between the runtime, the mod manager, and the installer/compat layer

**Version:** 1.1 (merged) — 2026-10-07
**Status:** PROPOSAL ONLY. Nothing integrated, nothing sent, nothing downloaded.

> This is a proposal written from the installer-compat side project. The canonical
> contract (docs/INSTALL-PLAY-CONTRACT.md) is owned by the mod manager team.
> Nothing there is edited from here. If accepted, the mod manager team copies
> what they agree with into the canonical file.
>
> This file merges the former `docs/CONTRACT-PROPOSAL.md` (five core interfaces)
> and the former `compat/CONTRACT-PROPOSAL.md` (pre-download compatibility
> badge) into one document. The badge contract is Section 4.
>
> **Scope guarantee (applies to every interface in this document):** the
> installer and compat-checker are Mac-only host tools. No FTP, no payloads, no
> launches, no console contact, no mod edits, no writes to the mod manager
> branch, `launch11-combined/units`, or `outputs/2026-10-07-l1*` paths. Any
> manager integration inherits the same guarantee: badges and grades are
> computed from text that is already on the host; they never trigger a console
> operation.

## 1. The components and their boundaries

| component | owner | does | never does |
|---|---|---|---|
| Runtime | GPT | UE4SS composition PRX, ploader injection, engine ABI, console | install mods, manage mod files |
| Mod Manager | Claude (hyj9eg branch) | Nexus download, archive retrieval, title selection, UI | touch the runtime, edit mod files |
| Installer + Compat + Catalog (this project) | zcode | mod detection, compatibility grading, install/uninstall, conformance, API catalog authorship | touch the console, edit the runtime, edit mods |
| `ps5_runtime_status` values | GPT (console testing) | fills the status field per catalog entry after console runs | author catalog structure |

## 2. Core interfaces

### 2.1 Mod Manager → Installer: install request

```json
{
  "action": "install",
  "archive_path": "/tmp/nexus/downloads/mod_name_v1.2.3.zip",
  "metadata": {
    "nexus_mod_id": 12345,
    "nexus_file_id": 67890,
    "name": "Mod Display Name",
    "version": "1.2.3",
    "source_url": "https://www.nexusmods.com/...",
    "description": "...",
    "requirements": ["SomeFramework >= 2.0"]
  },
  "target_title_id": "PPSA33286"
}
```

The installer returns a result:

```json
{
  "status": "installed" | "conflict" | "incompatible" | "failed",
  "mod_type": "ue4ss-lua" | "ue4ss-cpp" | "pak-asset" | "config-only" | "mixed" | "unknown",
  "compatibility": {
    "grade": "compatible" | "partly-compatible" | "unsupported" | "unknown
- **does not parse** — a Lua file in the mod has a syntax error (reported with file:line)",
    "missing_apis": ["..."],
    "behaves_differently": ["..."]
  },
  "installed_files": [{"path": "...", "sha256": "...", "size": 0}],
  "config_changes": {"loadlist": "appended 1 row", "ploader": "no change needed"},
  "manifest_path": "/data/modbridge/roots/PPSA33286/manifests/mod_name.json",
  "conflicts": [],
  "rollback_available": true
}
```

### 2.2 Mod Manager → Installer: uninstall request

```json
{
  "action": "uninstall",
  "manifest_path": "/data/modbridge/roots/PPSA33286/manifests/mod_name.json"
}
```

Returns: `{"status": "uninstalled" | "blocked" | "failed", "blocked_by": ["DependentMod"]}`

### 2.3 Mod Manager → Installer: compatibility check

Two phases exist because a static source scan is impossible before download
(only the Nexus description and a file-list preview are available). The short
version: Phase 0 grades pre-download from description + file list with a
provisional badge; Phase 1 grades after download from a full source scan
before the mod is enabled. The complete two-phase contract — phases, metadata
asks, wire schemas, badge UI mapping, caching, and honesty rules — is
Section 4 of this document.

### 2.4 Installer → Runtime: config entries

The installer writes these files (never the runtime's source, never mod files):

| file | what the installer writes | format |
|---|---|---|
| /data/modbridge/roots/\<TITLE\>/loadlist.txt | one `file\tconfig\t<path>\t<size>` row per mod file | tab-separated, exact byte-size required |
| /data/plugins/ploader.ini | `\<prx_path\> = 2` under `[<TITLE>]` | existing format |
| /data/modbridge/roots/\<TITLE\>/ue4ss/Mods/mods.txt | `<ModName> : 1` row (load order = row order; strict `name:0|1` parser) | UE4SS Part 1 mod enumeration |
| /data/modbridge/roots/\<TITLE\>/ue4ss/Mods/\<ModName\>/enabled.txt | existence-only marker (empty file = enabled) | UE4SS Part 2 belt-and-braces |
| /data/modbridge/roots/\<TITLE\>/ue4ss/Mods/\<ModName\>/ | the mod's files, byte-for-byte from the archive | never edited |

The runtime (GPT) reads these at launch. The installer never touches the
runtime PRX, the UE4SS-settings.ini (that's runtime config owned by GPT), or
any engine file.

### 2.5 Conformance script → Runtime: API status fill-in

GPT runs the conformance scripts (static + live) on the console and fills the
`ps5_runtime_status` field in the catalog JSON. The catalog is then the shared
source of truth for the compatibility checker (see Section 4.4 for the exact
catalog fields the checker consumes).

## 3. Grade vocabulary (single set, all interfaces)

| grade | meaning | badge | tooltip |
|---|---|---|---|
| `compatible` | all cataloged APIs console-implemented (source scan) | green check "PS5 OK" | verified against catalog |
| `partly-compatible` | works, listed APIs missing/different; expandable list | amber | works with caveats |
| `unsupported` | central missing API; list + runtime-side workarounds | red X | will not work |
| `unknown` | uncataloged APIs; nothing verified | gray "?" | cannot judge |
| `unverified-likely-ok` / `likely-partly-compatible` / `likely-unsupported` | Phase-0 only, description-derived provisional grades | outline icon | estimate; confirm after download |

Notes:
- `partly-compatible` is the canonical spelling (earlier drafts of the core
  interface sketch wrote `partly`; implementations and this document use
  `partly-compatible`).
- Phase 0 grades are distinct names so a provisional estimate can never be
  confused with a source-scan grade (see honesty rules, Section 4.7).

## 4. Pre-download compatibility badge (two-phase contract)

### 4.1 The gap

`checker.py` grades a mod by statically scanning its `.lua` sources against
the runtime API catalog. That works **after** download (the files exist on
disk). BEFORE download, the mod manager only has:

1. the Nexus mod **description** (markdown/HTML text), and
2. a **file-list preview** (archive entry names + sizes, e.g. from the
   Nexus "preview file list" endpoint — names only, no content).

A static source scan is impossible without sources. The earlier interface
sketch's `lua_api_calls_detected` pre-download field is therefore dropped:
pre-download "detection" can only be a mention scan over description text,
never call-site analysis. This two-phase contract lets the manager show a
badge honestly anyway.

### 4.2 Phase 0 — PRE-DOWNLOAD badge (no sources): `--mode predownload`

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
  heuristic as the real grader: registration APIs or >3 mentions), listing
  the mentioned APIs as `evidence: "description-mention"` — never as call sites
- no `.lua` detected or description mentions nothing → `grade: "unknown"`

Phase 0 NEVER upgrades to green. The only positive badge it can show is
"unverified", because descriptions routinely under-report or over-report API
usage. The UI must render the confidence level next to the badge.

### 4.3 Phase 1 — POST-DOWNLOAD PRE-ENABLE GATE: `--mode scan` (current tool)

The manager already unpacks archives into a staging area before installing.
Run `checker.py <staged_lua_path> <catalog.json> --json badge.json` there and
show the full badge (grade + blockers + runtime-side workarounds) before the
user enables the mod. Result is cached (Section 4.6) so it is computed once
per (mod version, catalog version).

### 4.4 Metadata needed

From the runtime catalog (owner: catalog agents):

| Field | Why |
|---|---|
| `catalog_version` (wrapper `{catalog_version, entries}`) | badge cache key + reproducibility; the checker already reports it |
| stable `name` grammar (`Global`, `Type:Method`, `Type.Member`) | lets Phase 0 build one regex per entry |
| `workaround` text (optional) | shown verbatim in the "why?" panel |

The checker already consumes the minimal per-entry schema
`{name, category, ps5_runtime_status}` and optional
`kind / type / workaround / notes`.

From the mod (owner: mod author, optional but recommended) — an optional
`compat_manifest.json` shipped at the archive root, filled in by the author
or by a one-time Phase 1 scan the manager offers to submit back:

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
author declaration, `confidence: "author-declared"`), and `entrypoints` lets
the manager scan only the files that matter on huge mods.

From the manager (owner: manager branch):

- mod identity tuple: `{nexus_mod_id, file_id, version, archive_sha256}`
- the description text + file-list preview it already fetched
- catalog pin: which `catalog.json` version the badge was computed against
- a small badge cache store (Section 4.6)

### 4.5 Wire contract

Request (manager → checker), Phase 0:

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

Response (checker → manager), both phases:

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

The `grade` vocabulary is the single set in Section 3. The existing JSON
report (`grade, total_calls, matched_calls, unmatched_calls, missing,
behaves_differently, unknown, details_per_file`) is a superset of this badge
payload, so the manager can pass it through unchanged.

### 4.6 Caching and invalidation

- cache key: `(nexus_mod_id, file_id, archive_sha256, catalog_version, checker_version)`
- hit → reuse badge, zero recompute
- invalidate on: new mod file id, catalog version bump, checker version bump
- Phase 0 badges are always recomputed on new file_id (descriptions change)

### 4.7 Failure modes and honesty rules

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

## 5. Requests to the mod manager team

1. A stable API for passing downloaded archives + Nexus metadata (Section 2.1).
2. A directory for compatibility badges in their UI (they design the badge; we
   provide the grade data, mapped per Section 3).
3. Confirmation that the install layout matches their expectations (they own
   /data/modbridge/roots/\<TITLE\>/ structure).
4. Badge plumbing per Section 4: mod identity tuple, description text +
   file-list preview, catalog pin in manager settings, badge cache table +
   "scan now" pre-enable gate.

## 6. Requests to GPT (runtime owner)

1. Fill in `ps5_runtime_status` for every catalog entry after console testing
   (values: "implemented+console-proven" / "implemented-untested" / "missing" /
   "behaves-differently").
2. Confirm the config-entry formats (loadlist byte-size law, ploader ini format).
3. Confirm that the runtime never reads mod files outside the Mods/ directory
   (so the installer can safely place manifests elsewhere).
4. **Keybind controller mapping**: document how the runtime translates PC
   keyboard constants (`Key.F7`, `Key.RETURN`, etc.) to PS5 controller inputs.
   The current approach is PS5Pad.ini chord mapping (e.g., L3+R3 = F7).
   Questions:
   a. Is this mechanism generalizable to arbitrary `Key.*` constants?
   b. Does the runtime need a built-in translation table, or is the
      config-file approach sufficient?
   c. What happens when a mod uses a keybind the config doesn't map? (Current
      behavior: the keybind registers but never fires — is this acceptable, or
      should it warn?)

## 7. Requests to the catalog maintainers

1. Wrap the catalog as `{catalog_version, entries}` and stamp
   `catalog_version` on every release (badge cache key, Section 4.6).
2. Keep the stable `name` grammar (`Global`, `Type:Method`, `Type.Member`) so
   Phase 0 can build one mention-scan regex per entry.
3. Populate the optional `workaround` field where a runtime-side workaround
   exists (shown verbatim in the "why?" panel).

## 8. Open questions (single merged list)

1. Where do manifests live? Proposal: /data/modbridge/roots/\<TITLE\>/manifests/
2. Who owns the UE4SS-settings.ini? Proposal: GPT (runtime); the installer
   requests changes via a separate config-delta file, never edits directly.
3. Multi-mod load order: the installer computes it (dependencies, conflicts)
   but the runtime enforces it. Proposal: loadlist row order = load order.
4. Hot-reload: if the runtime supports mod hot-unload in the future, the
   installer needs a notification API. Low priority now.
5. Catalog versioning ownership: who bumps `catalog_version` when GPT fills
   statuses, and does the manager auto-re-grade installed mods on a bump?
6. Phase-0 grade names: manager must accept the `unverified-likely-*` /
   `likely-*` vocabulary (outline badges) — or propose their own naming,
   provided provisional grades can never render as verified.
7. `compat_manifest.json` adoption: will mod authors actually ship it, and
   does the manager offer the "submit-back" one-time scan flow?
8. PC keyboard → PS5 controller mapping: the compat checker now flags `Key.*`
   constant usage as a PC-only input dependency. The runtime needs a documented
   mapping strategy (config-file chords, built-in defaults, or both) before
   keyboard-dependent mods can grade higher than `partly-compatible`
   (see also Section 6, request 4).
9. Lowercase mod paths: a mod shipping lowercase `scripts/main.lua` works on
   Windows (case-insensitive filesystem) but is refused by our installer
   (case-conflict detection). Should the runtime look up mod files
   case-insensitively on PS5, or should the installer keep refusing?
   Current behavior (Mac-only / host-tested, `installer/installer.py`): the
   installer detects the case mismatch between `Scripts/` (expected) and
   `scripts/` (shipped) and refuses with a runtime/config note — it never
   renames mod files. (Also `docs/ANSWERS.md` Q5.)

## 9. Implementation checklist (when approved)

- [ ] add `--mode predownload` (description + file-list scan) to checker.py
- [ ] accept `compat_manifest.json` input in both modes
- [ ] emit the Section 4.5 badge envelope (`schema:1`) alongside today's full report
- [ ] manager: badge cache table + UI mapping + "scan now" pre-enable gate
- [ ] pin catalog version in manager settings; bump policy documented
