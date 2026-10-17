# HOWTO-MOD-MANAGER — integrating with the installer/compat layer

**Audience:** the mod manager team. **Scope: Mac-only / host-tested** —
host-local paths only, no console contact; integrations inherit that.

**Honesty note up front:** the wire-level JSON envelopes in
`docs/CONTRACT-PROPOSAL.md` Sections 2.1/2.2 are a **proposal**, not
implemented. What exists and is tested today is the pipeline adapter and
the CLIs/Python APIs described here — build your adapter against these.

## 1. THE entry point: `InstallPipeline.install_mod`

The mod manager's ONE entry point is **`InstallPipeline.install_mod`** in
`installer/adapter.py` (line 666). It runs the whole pipeline in order:

```
detect (detector.py) -> compat grade (compat/checker.py)
     -> conflicts gate (conflicts.check_install)
     -> install + verify (installer.py, journalled)
     -> register into the unified install state
```

**Calling `Installer.install` directly SKIPS the compat and conflict
gates.** The raw `Installer` class (`installer/installer.py`) is the
transactional write engine the pipeline wraps; invoked alone it performs NO
compatibility grade and NO path/keybind/hook/dependency conflict check (it
keeps only its own case-conflict detection), and its manifest is not the
uninstaller's record — the pipeline does that conversion. Do not build user
facing installs on it. (`InstallPipeline.register_existing()` exists to
adopt installs that somehow bypassed the pipeline.)

```python
import sys; sys.path.insert(0, "installer")
from adapter import InstallPipeline

pipe = InstallPipeline(plugins_dir="/path/roots/plugins",   # default <game_root>/plugins
                       state_dir="/path/roots/state",       # default <game_root>/state
                       catalog_path="compat/catalog-real-merged.json",
                       game_version="1.004.000")            # seeds VERSION_MISMATCH checks

res = pipe.install_mod("/path/mod.zip",
                       {"name": "Townfall Item Spawner", "version": "1.2.0"},
                       title_id="PPSA33286",
                       game_root="/path/roots")
if not res.ok:
    print(res.stage, res.refusal or res.error)   # detect / compat / conflicts /
                                                 # install / verify
else:
    print(res.manifest_path, res.state_path, res.warnings)
```

`InstallResult` carries: `ok`, `stage`, `refusal`/`error`+`error_kind`,
`detection` (the 9-label classifier dict), `compat` (grade + reason +
catalog version), `conflict_blockers`/`conflict_warnings`, `manifest` +
`manifest_path`, `verify` (the post-install health check), `warnings`,
`state_path`. Refusals are REPORTED, not raised.

Pipeline policies (documented, never silent): `ue4ss-runtime` archives and
`runnable_on_ps5 == "no"` are refused at detect; compat grade
`unsupported` blocks (`unknown` blocks too with
`block_on_unknown_compat=True`); PATH_CONFLICT / VERSION_MISMATCH /
MISSING_DEPENDENCY blockers refuse BEFORE any write; keybind/hook conflicts
and `partly-compatible` grades install with warnings.

Companions on the same object: `check_mod(...)` (dry run — every pre-install
stage, zero writes), `uninstall_mod(manifest_path, game_root, ...)`,
`verify_mod(mod_name, title_id, game_root)`, `installed_mods(game_root,
title_id)` (the unified registry view), `register_existing(manifest_path,
game_root)`, `rollback_uninstall(token, title_id, game_root)`.

CLI (thin, same semantics):
`python3 installer/adapter.py install|check|uninstall|verify --archive …
--metadata … --title PPSA33286 --game-root … [--plugins --state --catalog
--game-version]`; exit 0 ok, 2 refused/failed, 3 verify failure.

### What the adapter closes (the old I-1/I-2 caveats)

The independent tester's two integration blockers (`tests/REVIEW.md`) are
closed by this adapter — you no longer carry their workarounds:

- **I-1 (record contracts differ):** the adapter maintains ONE unified
  registry at `<state_dir>/install_state/<TITLE_ID>.json`
  (`modbridge-install-state/1`) that feeds BOTH `uninstall.py` and
  `conflicts.py`, and converts installer manifests to uninstaller records
  itself (`installer_manifest_to_uninstall()`; also exported for your use).
  You do not maintain the uninstaller state by hand.
- **I-2 (conflicts never wired):** the pipeline calls
  `conflicts.check_install` before any write (adapter.py:648) — two mods
  writing the same path cannot collide through the pipeline.

Also handled for you: ploader.ini lives outside the uninstaller's game
root, so the adapter removes the mod's prx line AFTER the uninstall
transaction commits (shared lines kept while another installed mod needs
them; installer-created sections dropped when empty; a snapshot inside the
transaction makes `rollback_uninstall()` restore it too).

## 2. Install: request and response

```bash
python3 installer/adapter.py install --archive /path/mod.zip \
  --metadata /path/mod.json --title PPSA33286 --game-root /path/roots \
  [--plugins /path/plugins] [--state /path/state] [--catalog /path/catalog.json]
```

Exit codes: 0 ok · 2 refused or failed (detect/compat/conflicts/install).
Stdout: the full `InstallResult` as JSON. For a raw engine-level CLI
(no compat/conflict gates — not recommended for user installs),
`installer/installer.py` keeps its own `install|verify|recover` commands
(exit 3 verification failure, 4 rollback incomplete).

### Metadata request schema (`--metadata` file; only `name` + `version` required)

```json
{
  "name": "Townfall Item Spawner",
  "version": "1.2.0",
  "source_url": "https://www.nexusmods.com/townfall/mods/42",
  "file_list": ["MyMod/Scripts/main.lua", "MyMod/enabled.txt"],
  "requires": ["SomeFramework"],
  "game_versions": [">=1.003"],
  "ploader_prx": {"name": "modbridge_supplier_composition.prx", "mode": "2"},
  "ue4ss_settings": {"Engine": {"bUseFixedFrameRate": "True"}}
}
```

- `name` is sanitized into the install directory name (`[^A-Za-z0-9._-]` → `_`);
  `file_list` is cross-checked against the archive (mismatches land in the
  manifest; `--strict-file-list` aborts on them).
- `requires` / `game_versions` feed the conflict gate (MISSING_DEPENDENCY /
  VERSION_MISMATCH) and the unified state — pass them here rather than
  computing dependencies yourself.
- `ploader_prx.mode` must be `"0"`, `"1"` or `"2"`. `ue4ss_settings` values
  must be single-line strings, rendered into `<ue4ss>/overrides/<mod>.ini`,
  never into the runtime's settings file.

### Manifest (response artifact, schema `modbridge-manifest/1`)

Written to `<roots>/<TITLE>/manifests/<mod_dir>.json` — the authoritative
record of what was installed. Key fields:

```json
{
  "schema": "modbridge-manifest/1",
  "mod_name": "Townfall Item Spawner", "mod_dir": "Townfall_Item_Spawner",
  "title_id": "PPSA33286", "version": "1.2.0",
  "files": [
    {"archive_path": "MyMod/Scripts/main.lua", "role": "lua", "size": 746,
     "install_rel": "ue4ss/Mods/Townfall_Item_Spawner/Scripts/main.lua",
     "install_path": "/abs/<roots>/<TITLE>/ue4ss/Mods/.../main.lua",
     "original_sha256": "...", "installed_sha256": "..."}
  ],
  "config_entries": {"mods_txt_row": "Townfall_Item_Spawner : 1",
                     "enabled_marker_rel": "ue4ss/Mods/.../enabled.txt",
                     "ploader_prx_name": "...", "ploader_prx_mode": "2"}
}
```

Also recorded: `archive` sha256, `installed_at`, `transaction_id`,
`skipped_outside_mod_root`, `stale_files_left_in_place` (previous-version
files, left on disk), `file_list_mismatch`.

Four config outputs are written per install and restored on uninstall:
loadlist.txt block, `ue4ss/Mods/mods.txt` row `<Name> : 1`, empty
`ue4ss/Mods/<Name>/enabled.txt` marker (unless the archive ships its own),
and the ploader.ini line.

### Verify (post-install health check)

```bash
python3 installer/adapter.py verify --mod "Townfall Item Spawner" \
  --title PPSA33286 --game-root ... [--plugins ... --state ...]
```

Returns `{"ok": bool, "mod": "...", "title_id": "...", "files_checked": N,
"problems": ["missing: ...", "tampered: ..."]}`; exit 3 when not ok.
Journalled crash recovery is automatic on the next installer constructor
call after a crash (`installer/installer.py recover` for the raw CLI).

## 3. Uninstall: request and response

Use the pipeline (Python or CLI); it reads the INSTALLER manifest, converts
it, checks dependents, uninstalls, and cleans ploader.ini:

```python
res = pipe.uninstall_mod("/path/roots/PPSA33286/manifests/Townfall_Item_Spawner.json",
                         "/path/roots", confirm_modified=True)  # or dry_run=True
res.ok, res.stage, res.plan, res.core.removed_files, res.ploader
```

`stage` on refusal: manifest / plan / dependents / confirm / uninstall.
Dependent mods block with `ModStillRequiredError`; modified files require
`confirm_modified=True` (or a per-file callable); `dry_run=True` plans
without touching anything; `pipe.rollback_uninstall(res.core.rollback_token,
title_id, game_root)` undoes a completed uninstall (files, config, unified
state AND the ploader.ini snapshot).

Low-level equivalent (what the adapter automates): `uninstall.Uninstaller`
against the unified `install_state/<TITLE>.json` and a converted record of
shape:

```json
{
  "mod_id": "Townfall_Item_Spawner",
  "files": [{"path": "ue4ss/Mods/Townfall_Item_Spawner/Scripts/main.lua",
             "sha256": "<original sha256>"}],
  "config_entries": {"loadlist.txt": ["mod\tTownfall_Item_Spawner\t1.2.0"],
                     "ue4ss/Mods/mods.txt": ["Townfall_Item_Spawner : 1"]},
  "enabled_markers": ["ue4ss/Mods/Townfall_Item_Spawner/enabled.txt"],
  "requires": []
}
```

Behavior: removes exactly the recorded files (moved to transaction trash,
never unlinked); shared files kept; config lines are multiset-removed so
other mods' rows survive byte-for-byte.

## 4. Pre-install conflict check

Already wired into `install_mod`/`check_mod` — you only need the raw module
for standalone tooling:

```python
from conflicts import ModSpec, check_install

spec = ModSpec(mod_id="MyMod", files=("ue4ss/Mods/MyMod/Scripts/main.lua",),
               requires=("SomeFramework",), game_versions=(">=1.003",),
               lua_sources={"Scripts/main.lua": open("main.lua").read()})
report = check_install(spec, state=".../install_state/PPSA33286.json")
```

`report.ok` is True when no blockers; `str(report)` lists them. Errors:
`PATH_CONFLICT`, `VERSION_MISMATCH`, `MISSING_DEPENDENCY`. Warnings:
`KEYBIND_CONFLICT`, `HOOK_CONFLICT`. `check_batch([...])` checks a candidate
set; batch peers satisfy each other's dependencies.

## 5. Compatibility badges (grade vocabulary)

Shared vocabulary (contract Section 3, plus the round-5 checker 1.1
additions); map the checker's JSON output to your UI:

| grade | meaning | suggested badge |
|---|---|---|
| `compatible` | every cataloged API the mod calls is implemented on PS5 | green check "PS5 OK" |
| `partly-compatible` | some calls missing / behave differently, none central; with NO blank statuses, a PC-only keyboard dependency also lands here | amber |
| `unsupported` | a central missing API (>3 uses, a registration call, or main code path) | red X |
| `unknown` | uncataloged APIs, OR cataloged with an empty `ps5_runtime_status` — this OUTRANKS everything except parse failures (round-5 policy) | gray "?" |
| `does not parse` | a scanned .lua failed the heuristic syntax check (file:line given); nothing in it can be analyzed | red "broken file" |

Round-5 grading rules to render honestly: any blank-status API the mod
calls forces `unknown` ("runtime status not filled"); known PS5 differences
(e.g. `Key.*` keyboard keybinds) ride INSIDE the `unknown` result as
`known_ps5_differences[]` and never upgrade the grade.

Post-download, pre-enable scan:

```bash
python3 compat/checker.py <staged-mod-dir-or-.lua> compat/catalog-real-merged.json --json badge.json
```

From the JSON: `grade` and `grade_reason` drive the badge; blockers =
`missing[]` with `central: true`; risks = `missing[]` with
`central: false`; caveats = `behaves_differently[]`; keyboard dependencies =
`pc_only_input[]` / `known_ps5_differences[]`; broken files =
`parse_errors[]`. Entries carry `call_sites` (file/line) and a
runtime-side `suggestion`. The report includes `catalog_version` — cache
badges by archive sha256 + catalog version and invalidate on either bump.

Honesty rules for the UI: unresolved method calls never count against a
grade; today the real catalog's statuses are all empty (599/599), so real
mods grade `unknown` until the runtime owner fills them; the Phase-0
pre-download grades (`unverified-likely-ok`, `likely-partly-compatible`,
`likely-unsupported`) are **proposal-only** — no pre-download mode exists.

## 6. What the mod manager must provide

1. **A zip archive** on a local path (7z/RAR rejected with conversion
   guidance). Installer safety limits: 4096 entries, 256 MiB/member,
   1 GiB total; no symlinks/devices/encryption, absolute/`..`/
   duplicate-normalized names.
2. **A metadata JSON** (Section 2 schema; minimum `name` + `version`) and
   a **title ID** matching `^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$` (`PPSA33286`).
3. **Three directories** you own: `--plugins` (ploader.ini), `--state`
   (journal/backups + the unified install state), and the `--game-root`
   roots tree (default `<game_root>/plugins` and `<game_root>/state` if you
   omit the first two).
4. **Serialization:** no locking exists — run one install/uninstall at a
   time per title root.
