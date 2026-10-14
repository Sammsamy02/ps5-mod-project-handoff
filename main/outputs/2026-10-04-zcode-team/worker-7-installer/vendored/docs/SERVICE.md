# modforge service — contract `modforge-service/1`

The C header is [`contract/modforge_service.h`](../contract/modforge_service.h).
This document specifies every JSON request, reply, event and record behind it.
The app owns the UI, the Nexus account and downloading. The backend owns
everything from "this archive was downloaded" to "the runtime is running these
mods in the game". Everything runs on the console.

Versioning: the contract string is `modforge-service/1`. Within version 1,
new methods, new optional request fields and new reply fields may be added,
and clients must ignore reply fields they don't know. Anything else is
version 2.

## Configuration (`mfs_open`)

```json
{
  "state_dir": "/data/modforge",          // backend-owned: records, prepared mods, journal
  "mod_root_base": "/data/modbridge",     // one managed mod root per game: <base>/<TITLE_ID>
  "data_dir": "/data/modforge/share",     // engines/*.json, games.json, profiles/ shipped with the app
  "limits": {                             // optional; these are the defaults
    "max_archive_entries": 10000,
    "max_archive_bytes": 4294967296,      // total expanded bytes per archive
    "max_entry_bytes": 2147483648,
    "max_jobs": 2
  }
}
```

The backend creates `state_dir` if missing. It never writes outside
`state_dir` and the mod roots under `mod_root_base`. Game files are never
modified: a mod root is generated content that the in-game runtime and the
pak/overlay loaders read.

## Requests and replies

```json
request:  {"method": "mod.prepare", "params": {...}, "id": "optional client tag"}
success:  {"ok": true,  "id": "...", "result": {...}}
failure:  {"ok": false, "id": "...", "error": {"code": "unknown_title", "message": "...", "detail": {...}}}
```

`id` is echoed back if given. Error `code`s are stable identifiers (listed
under [Errors](#errors)). `message` is English text for logs, not for parsing.

## States the app shows

| State | Meaning | Where it comes from |
|-------|---------|---------------------|
| `downloaded` | The app has the original archive and registered it | `mod.register_download` |
| `prepared` | The archive was read, classified, FOMOD-resolved and checked against the game; ready to install, or blocked with reasons | `mod.prepare` job |
| `installed` | The mod is part of the game's mod root and load list | `install.apply` job |
| `running` | The game's runtime read the load list in this session and started the mods | `runtime` in `records.list` / `game.status` |

A mod moves forward through these states. It goes back when the app removes
it (`mod.uninstall` takes it from installed to prepared, `mod.forget` drops
the record). `running` is per game: it describes what the runtime did, read
from the runtime's status file, plus whether that game process is still alive.

## Jobs

`mod.prepare`, `install.apply` and `mod.uninstall` start a job and return
right away:

```json
{"ok": true, "result": {"job_id": "j-000017", "state": "queued"}}
```

Job states: `queued`, `running`, `cancelling`, `succeeded`, `failed`,
`cancelled`. `job.status {"job_id"}` returns
`{"job_id","kind","state","phase","done","total","result"?,"error"?}`.
`job.cancel {"job_id"}` asks the job to stop at its next safe point. A
cancelled install leaves the previous installed set exactly as it was. Jobs
that change one game's mod root run one at a time per game.

## Events (`on_event`)

```json
{"event": "job.progress", "job_id": "j-000017", "phase": "extract", "done": 12, "total": 40, "detail": "Scripts/main.lua"}
{"event": "job.finished", "job_id": "j-000017", "state": "succeeded", "result": {...}}
{"event": "records.changed", "title_id": "PPSA33286"}
```

Phases: `read-archive`, `extract`, `classify`, `fomod`, `check`, `plan`,
`stage`, `swap`, `cleanup`.

## Methods

### `service.describe`
Returns `{"contract": "modforge-service/1", "backend_version", "engines": [...], "archive_formats": [...]}`.

### `capabilities.query`
`{"title_id"?: "PPSA33286"}`. For each known engine (or just this game's):
which mod types work today, which need conversion, which wait on a runtime
port, and why. Roles use the same statuses as `modforge coverage`:
`works`, `after-conversion`, `rebuild-from-source`, `in-progress`,
`waiting-on-framework`, `not-applicable`. Runtime components report
`available`, `integrating` or `planned`. The app can grey out mods with this.

### `game.register`
`{"title_id", "param_json": "<path to the installed game's sce_sys/param.json>", "engine"?}`.
Resolves and stores the game's profile from its engine definition plus the
param.json facts: title id, SDK version, required firmware, content
version. Game paths and build facts live in the profile, not in code. Reply:
the stored profile. Errors: `unknown_engine` (pass `engine`), `param_json_unreadable`.

### `mod.register_download`
```json
{
  "download_id": "dl-…",                  // the app's id, stable across restarts
  "title_id": "PPSA33286",
  "archive_path": "/data/…/TownfallItemSpawner-12-1-0.zip",
  "source": {"provider": "nexus", "domain": "townfall", "mod_id": 12, "file_id": 34,
             "version": "1.0", "name": "Item Spawner", "author": "…"},
  "requirements": { …the normalized object from decode_nexus_metadata(Requirements)… }
}
```
Records the mod as `downloaded`. The backend doesn't copy or alter the
archive. `sha256` is computed during prepare.

### `mod.prepare` (job)
`{"download_id", "fomod_choices"?: {...}}`. Reads the original archive
(zip, 7z, rar and the other formats in `service.describe`), extracts it into
the backend's prepared store, runs a FOMOD installer if present, classifies
every file into roles, checks it against the game (PS5 module checks and SDK
stamping, Bethesda plugin masters, engine roles), and plans prerequisites.
Result (`prepared` record):

```json
{
  "prepared_id": "p-…", "download_id": "dl-…", "title_id": "PPSA33286",
  "sha256": "…", "tier": "native" | "converted" | "unsupported",
  "components": [{"role": "lua_mod", "root": "TownfallItemSpawner", "files": 2, "dest": "ue4ss/Mods/TownfallItemSpawner"}],
  "blockers": [{"code": "waiting_on_framework", "message": "…", "component": "…"}],
  "warnings": ["…"],
  "fomod": null | {"needs_choices": true, "steps": [ …the installer's pages/groups/options with defaults… ]},
  "prerequisites": {
    "provided_by_runtime": [{"requirement": {…}, "component": "ue4ss-ps5", "maturity": "integrating"}],
    "satisfied": [{"requirement": {…}, "by": "p-…"}],
    "download": [{"requirement": {…}, "domain": "townfall", "mod_id": 7}],
    "unsupported": [{"requirement": {…}, "reason": "external tool for Windows"}],
    "dlc": [{"requirement": {…}, "satisfaction": "unverified"}]
  }
}
```

`fomod.needs_choices`: the installer has steps without a usable default. Show
them, then call `mod.prepare` again with `fomod_choices` (format in
[FOMOD choices](#fomod-choices)). `prerequisites.download`: Nexus mods the app
should fetch, register and prepare before this one can be installed.
`provided_by_runtime`: requirements such as "UE4SS" that the PS5 runtime
replaces, so the app must not download the Windows build.

### `install.plan`
`{"title_id", "enable": ["p-…"], "disable": ["mod id"], "priority": {"mod id": 600}}`.
A dry run over the game's current installed set plus these changes: the
deployment's files, conflicts (which mod wins), load order, required
runtime components, missing prerequisites, blockers. Changes nothing.
`ready: false` comes with `blockers`.

### `install.apply` (job)
Same parameters as `install.plan`. Builds the new mod root next to the old
one, writes the load list last, then swaps it in with a journaled rename.
Interrupted at any point, the service finishes or rolls back the swap on the
next `mfs_open`, and the game sees either the old set or the new set,
never a mix. Result: the game's `installed` record (below).

### `mod.uninstall` (job)
`{"title_id", "mod_ids": ["…"]}`. Same transaction as `install.apply`, without
those mods. Their prepared records stay (`prepared`).

### `mod.forget`
`{"prepared_id" | "download_id"}`. Deletes the backend's prepared copy and
record. Refused with `in_use` while installed.

### `records.list`
`{"title_id"?}`. Every record the backend keeps, for the app's lists and for
restoring state after a restart:

```json
{
  "mods": [{"download_id","prepared_id"?,"title_id","state":"downloaded|prepared|installed","source":{…},"tier"?,"blockers"?}],
  "games": [{"title_id","engine","sdk_version","installed": {"mods":["…"],"generation":7,"load_list_sha256":"…"},
             "runtime": {"state":"not-started|activated|engine-ready|mods-started|failed|stopped",
                         "pid": 1234, "code": 0, "detail": "…", "session_ms": 123}}]
}
```

`runtime` is read from `<mod root>/runtime-status.txt`, which the in-game
runtime writes (format in `modforge/runtime_status.py`). Show *running* when
`runtime.state` is `mods-started` and the app sees that pid alive.

### `game.status`
`{"title_id"}`. The `games[]` entry above, freshly read.

### `job.status`, `job.cancel`, `job.list`
As described under [Jobs](#jobs).

## FOMOD choices

```json
{"steps": [{"name": "Main", "groups": [{"name": "Textures", "options": ["2K"]}]}]}
```

Steps and groups are matched by name, in installer order. A group that isn't
listed keeps its default. Choices that break a group's type (for example two
options in a SelectExactlyOne group) are rejected with `fomod_invalid_choice`.

## Errors

`bad_request`, `unknown_method`, `unknown_title`, `unknown_engine`,
`unknown_download`, `unknown_prepared`, `unknown_job`, `param_json_unreadable`,
`archive_unreadable`, `archive_unsupported_format`, `archive_too_large`,
`archive_unsafe_path` (absolute, `..`, links leaving the root), `fomod_invalid`,
`fomod_invalid_choice`, `not_ready` (install with blockers), `busy`
(another job holds this game), `in_use`, `cancelled`, `io_error`, `no_space`,
`internal`.

## Durable state (`state_dir`)

```
records.json            all records; replaced atomically (write temp, fsync, rename)
prepared/<prepared_id>/ extracted, classified files + manifest.json (modforge package layout)
journal/<title>.json    present only while a swap is in flight
```

Mod roots (`<mod_root_base>/<TITLE_ID>/`) carry the `.modforge-root` marker
and are replaced wholesale by `install.apply`. The swap is:

1. Build `<TITLE_ID>.next/`. Its load list is written last.
2. Write `journal/<title>.json` with `{"phase": "swap", "generation": N}`.
3. Rename `<TITLE_ID>` to `<TITLE_ID>.prev`, then `<TITLE_ID>.next` to `<TITLE_ID>`.
4. Delete `<TITLE_ID>.prev`, then the journal.

Recovery reads the journal. If `<TITLE_ID>` is missing, it renames `.next`
or `.prev` back into place. It then finishes the cleanup.
