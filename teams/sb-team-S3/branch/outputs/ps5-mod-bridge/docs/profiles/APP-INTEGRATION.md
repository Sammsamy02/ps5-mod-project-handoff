# Runtime profiles in the app: routes, statuses, rules

Status: BUILT on the Python host and the page, 2026-10-07. The profile contract itself (`modbridge.runtime-profile/1`, `PROFILE-CONTRACT-PROPOSAL.md`) is still a proposal until GPT confirms it; this file says how the app reads and shows it. The native PS5 host does not read or generate profiles yet and advertises neither capability, so its page is unchanged.

A runtime profile is made by reading a game's decrypted executable (`tools/profilegen`). It is **not tested on the console**: the page says "Made by reading the game's executable; not tested on the console", and only the console test's receipt can make it verified. The app never says a game supports mods, and a profile says nothing about a Nexus file.

## Where profiles live

`--runtime-profiles DIR` (constructor `BridgeApplication(runtime_profiles=DIR)`), default `<project>/profiles`:

```
<DIR>/<TITLE_ID>/<CONTENT_VERSION>/runtime-profile.json     (the contract's folder layout; the other members are the generator's)
```

Nothing is created until the first generation; then the folders are made private (0700) and never through a link. A title id and a content version are folder names, so both must match `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`; anything else is refused (`invalid_input`, or `no_version` for a game whose version does not fit). The host only **reads** these files. The one writer is the generator process the host starts; `verification` and `console-verified` belong to the receipt ingester, and no host code assigns, builds, edits or removes either (a test greps the source for that).

## Capabilities

`capabilities.runtime_profiles: true` (the status route) and `capabilities.profile_generate: true` (the generate route), both from the Python host. The page reads them as exactly `true`: without `runtime_profiles` it shows no profile line and sends no request; with it but without `profile_generate` it shows the status and a Generate button that is off with "This host cannot generate profiles."

## Routes

Both are for a title the host's observed title inventory lists (else `not_found` 404) and sit behind the same gates as the other routes: GET needs the session cookie and a local Host; POST also needs the exact `Origin`, `X-CSRF-Token` and `application/json`.

### `GET /api/titles/{TITLE_ID}/profile?content_version=V`

Exactly one query field, `content_version`; any other query, a repeated field, a fragment or a value that is not a folder name is `invalid_input` 400. Returns the slim record, always these six keys:

```json
{"status": "none|generated|console-verified|stale|generating|failed",
 "profile_id": "PPSA33286/02.003.000/3b6cfad4" , "generated_utc": "2026-10-07T12:00:00Z",
 "confidence": {"high": 12, "medium": 5, "low": 3}, "unresolved": 4, "reason": null}
```

`profile_id`, `generated_utc`, `confidence` and `unresolved` are null unless the status is `generated`, `console-verified` or `stale` (a profile the host read and validated). `reason` is null except for `failed` and `stale`.

### `POST /api/titles/{TITLE_ID}/profile/generate` with body `{}`

Starts the generator in the background (below). The content version is the inventory's, never the caller's (any body other than `{}` is `invalid_input`). Replies with the same record: **202** and `generating` when a run was started or is already running for this title; **200** and `failed` with a reason when the inputs are unusable or the profile is verified (nothing was started and nothing was written); **409 `busy`** when another title is being generated (one at a time).

## Status rules (contract section 5)

Checked in this order for (title id, content version):

1. A generation of this title is running: `generating`.
2. The latest generation attempt for it failed (kept in memory until the next attempt or a restart): `failed` with that attempt's reason. The file on disk is left alone.
3. No `runtime-profile.json`: `none`.
4. The file is malformed in any way: `failed` with a code, **never** `generated`. Not a plain readable file (a link, a folder, over 16 MiB, unreadable): `profile_unreadable`. Not UTF-8 JSON (duplicate keys and NaN refused), not `schema: modbridge.runtime-profile/1`, a bad `profile_id`, `profile_digest` (64 lowercase hex), `generated.utc`, `status` (only `generated` or `console-verified`), `binding.executable.sha256`, `unresolved` (a list of objects), a section that is not an object, a value envelope whose `confidence` is not high, medium or low, or a `verification` that is not null or a well-formed block (`receipt_id`, `date_utc`, `profile_digest`, `result` text; `runtime_build` an object; `verified` a list): `profile_invalid`. A profile whose `binding` names another title id or content version than its folder: `profile_mismatch`. A file that says `status: console-verified` while its `verification` is null: `status_inconsistent`.
5. `verification` is null: `generated`.
6. `verification` is present:
   - its `profile_digest` differs from the file's `profile_digest`: `stale`, reason `digest_mismatch`;
   - else the host looks for the game's executable (below), hashes it (cached while the file is unchanged) and compares it with `binding.executable.sha256`: not readable, `stale`/`executable_unavailable`; different, `stale`/`executable_mismatch`;
   - else, with `verification.result` equal to `pass`: **`console-verified`**. (A matching verification whose result is not `pass` is shown as `generated`: a failed test is not a verification.)

The file's own `status` string never upgrades or downgrades anything beyond rule 4; `console-verified` comes only from rule 6. The host does not recompute `profile_digest` (its canonical form is the generator's), so it trusts the file's digest for the comparison; a hand-edited file can be forged by whoever can write the folder. Stronger binding to `receipts/` is open.

**Counts.** `confidence` counts the value envelopes (any object holding `value` and `confidence`) under `binding, version_map, elf, load, build, symbols, structs, vtables, ue4ss`, by their `confidence`. An envelope whose `value` is null is an unresolved gap and counts nowhere (the generator lists it in `unresolved`); `alternatives` lists hold rejected candidates and are not counted. `unresolved` is the length of the file's `unresolved` list.

## Reason codes

| Code | Meaning (the page's frozen sentence is in `web/app.js`, `profileReasons`) |
|---|---|
| `no_executable` | the title is not a dump, or its folder has no readable executable |
| `executable_encrypted` | an `eboot.bin` is there but is not an ELF (a SELF, or encrypted) |
| `no_version` | no content version, or one that cannot be a folder name |
| `storage_unavailable` | the profiles folder could not be prepared |
| `generator_unavailable` | the generator process could not be started |
| `generator_failed` | it exited with an error, or exited 0 and left no file |
| `generator_timeout` | it ran past the limit (default 600 s) and was stopped |
| `profile_unreadable`, `profile_invalid`, `profile_mismatch`, `status_inconsistent` | rule 4 above; also a generated file the host will not accept |
| `verified_profile_kept` | the file carries a verification that still holds (console-verified): Regenerate is refused (see Generating); not remembered, so the next status read says what the file says |

Stale reasons: `digest_mismatch`, `executable_mismatch`, `executable_unavailable`.

Nothing the generator prints, no path and no exception text ever reaches a reply: its output is discarded and every failure is one of these codes.

## Generating

The host only starts a generation for a title whose **source is `homebrew_dump`** (a game folder under the console's dump root; the folder is found by a fresh bounded scan of the same root that built the inventory, `TitlesRootProvider.locate_dump`, and its version must still be the inventory's). Any other source is `no_executable`.

**Finding the executable** inside the dump folder: `<dump>/decrypted/eboot.bin`, else `<dump>/eboot.bin`. A candidate counts only if it is a regular file starting `7f 45 4c 46` (ELF). A candidate that exists but is not an ELF is remembered, and if nothing better is found the reason is `executable_encrypted` (nothing at all: `no_executable`). Folders are never followed through links; the executable file itself may be a link to a regular file (it is only read). The inputs to the generator are that file and `<dump>/sce_sys/param.json` (a regular file).

**Running it**, one at a time, in a background thread with a time limit (`generator_timeout`, default 600 s; the process group is killed on timeout or when the host closes):

```
python3 -I -B -c <bootstrap> --eboot <executable> --param <param.json> --out <DIR>/<TITLE_ID>/<CONTENT_VERSION>
```

with the project root as the working directory, a minimal environment (`PATH`, `HOME`, `TMPDIR`, `LANG`, `LC_ALL`), no stdin and discarded stdout and stderr. The bootstrap is `python3 -I -m tools.profilegen` plus the project root appended to `sys.path`: with `-I` Python drops the current directory from the path, and `tools` is a namespace package found only there, so the bare `python3 -I -m tools.profilegen` cannot import it. `BridgeApplication(generator_command=[...])` replaces the command prefix (tests use `tests/fake_profilegen.py`).

Exit codes of the real generator (`tools/profilegen/README.md`): 0 done (unresolved values are listed, not fatal), 2 unreadable or invalid input (`generator_failed`), 3 refused to overwrite a profile that carries a verification (`verified_profile_kept`). Before starting, the host reads the existing file. When its verification still holds (a pass for this digest and the executable the host can read now, i.e. status `console-verified`) the host answers `failed`/`verified_profile_kept` at once, starts no process and remembers nothing. When the verification no longer applies (status `stale`, or a result other than pass) the host passes `--force`, so the regenerated profile replaces the file with status `generated` and no verification, as contract section 5 requires for a changed digest; the host itself still never writes a verification. Exit 3 from a run (a race with the receipt ingester) is `verified_profile_kept` and is not remembered.

When the generator exits 0, the host reads the file it wrote exactly as it reads any profile (rule 4): a missing or invalid file is a failure, not a success. The real file is read as the generator writes it: every leaf of a data section is a value envelope, so `binding.title_id`, `binding.content_version` and `binding.executable.sha256` are read as `{"value": ...}` (a plain string, as in the contract's example, is accepted too); `evidence` is a list of strings; the host's `confidence` counts equal the generator's own `confidence_counts` (checked on the real Townfall profile: 51 high, 3 medium, 0 low, 12 unresolved).

## The page (Your games, `docs/INSTALL-FLOW-UX.md` 3.12)

Per card, when `runtime_profiles` is true: a "Runtime profile:" line and a button.

| Status | Line |
|---|---|
| checking | "Checking…" (`aria-busy`) |
| `none` | "None yet." |
| `generated` | "Generated <date> · N high, N medium, N low confidence · N unresolved. Made by reading the game's executable; not tested on the console." |
| `console-verified` | "Console-verified for this build · <date> · counts." (only for that host status) |
| `stale` | "Stale: Verified for a different build; regenerate. <date> · counts." (`executable_unavailable`: "Verified earlier, but this game's executable cannot be read here to check it.") |
| `generating` | "Generating… this can take several minutes." (`aria-busy`) |
| `failed` | "Failed: <frozen sentence of the reason>" |
| not checkable | "Could not be checked. Refresh games to try again." (a reply the page could not validate, or a failed request) |
| start refused | "Could not start: <catalog sentence of the error code>" |

The page reads the record fail-closed (exactly six keys, a known status, facts only where the status has a profile, a reason only where the status needs one and only from its frozen tables; ids, dates and counts by pattern), shows a date only as `YYYY-MM-DD`, and escapes everything. It never works a status out for itself.

Button: "Generate profile" (none, failed, not checkable) or "Regenerate" (generated, console-verified, stale); "Generating…" while it runs. Off (`disabled`, with the reason beside it) when the host cannot generate, the game is not a dump ("Only a game dump can be profiled."), or its version cannot name a folder. Waiting (`aria-disabled`, still a focus stop, a press sends nothing) while its status is being read, while it generates, and while another game generates ("Another profile is being generated."). One empty POST per press; the page then asks again each second (at most 15 minutes, tolerating three failed asks) and announces the outcome through the status region. Refresh games, and any reload of the title list, reads every status again. Selectors: `[data-yg-profile="<TITLE_ID>"]` (with `data-profile-status`), `[data-yg-generate="<TITLE_ID>"]`, `[data-yg-profile-reason]`.

## Tests

`tests/runtime_profile_host_tests.py` (status rules, every stale and verified combination, forged and malformed files, executable location, generation with a stand-in generator: success, failure, timeout, garbage, encrypted, no executable, one at a time, close; gating; the options); `tests/browse_ui_tests.cjs` ("runtime profile: ..."); a real-host flow in `tests/host_ui_integration.cjs` over `tests/serve_ui_fixture.py` `FIXTURE_MODE=profiles` (a console-shaped mirror with a tiny fake ELF; `tests/fake_profilegen.py`). One Python test runs the real `tools/profilegen` (about 20 s) on the real Townfall executable (`MODBRIDGE_TOWNFALL_EBOOT`), linked and never copied, through the host's own default command and status route; it is skipped when the generator or the executable is absent.

## Open

- Replacing a console-verified profile whose verification still holds: refused by design; only a stale verification is replaced.
- `receipts/` binding of a verification, and recomputing `profile_digest`, are not done.
- The native host: no route, no capability.
- `profiles/` is the default output folder inside the project; generated profiles are derived data and are not meant to be committed.
