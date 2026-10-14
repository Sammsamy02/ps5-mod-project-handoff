# Step 2 — installer adapter and preview state

The installer preview lets someone review a plan and its Yes/No questions before confirming a change, while keeping the real service boundary explicit.

Changed files: new `web/config.js`, `web/installer-service.js`, `web/installer-mock.js`, `tests/installer_service_tests.cjs`, and this note. No existing screen, download, account, host, native, runtime, game, mod or console file was changed by this worker. Screens and screenshots belong to the separate installer UI step.

## Four calls

`ModBridgeInstaller.create({config: ModBridgeConfig, mock: session.adapter, fetch, csrf})` returns only these Promise methods:

- `plan(game_title_id, archive_path)` → `{plan_id, plan_sha256, changes, warnings, decisions}`.
- `apply(plan_id, plan_sha256, answers)` → `{ok, install_id, report}`.
- `restore(install_id)` → `{ok, report}`.
- `status(game_title_id)` → `{installed_mods, capability_file}` plus optional service fields.

The selected observed title supplies the game title ID. A Nexus domain is a separate identity. Archive paths are trusted internal service handles, never a user path input. The adapter has no file-writing methods.

The centralized config defaults explicitly to `installer.mode: 'mock'`. Real mode uses configured same-origin paths, POST JSON for plan/apply/restore, and GET status with `game_title_id` in the query. Configured paths reject backslashes, control characters and fragments, and their resolved URL must retain the current origin. CSRF is supplied as a value or current-token callback. A real transport failure never falls back to preview success. Real results need no mock-only optional fields. Malformed nested result items refuse before rendering; change/warning strings and service records without optional display fields remain supported. An empty capability object becomes unavailable, while nonempty capability schemas stay owned by the runtime team.

Errors carry a known `code` and plain `message`; raw server errors/paths are not copied into public error wording. A transport failure does not prove whether a real operation changed anything. Only confirmed mock refusals carry `nothing_changed: true`.

## Explicit preview

Load `config.js`, `installer-service.js`, then `installer-mock.js` before the UI module. Node also supports CommonJS require without external dependencies.

`ModBridgeInstallerMock.create({games, archives, scenario, storage})` returns `{adapter, controls}`. Fixtures are parameterized, with no game or mod special branches and no automatic installed mods.

Game fixture:

```js
{game_title_id: 'DEMO_GAME_1', name: 'Sample game',
 capability_file: {supported_mod_types: ['textures', 'scripts']}}
```

Archive fixture:

```js
{archive_path: 'mock:sample', name: 'Readable menus', mod_type: 'textures',
 changes: [{action: 'add', description: 'Add readable menus'}],
 warnings: ['This mod changes the menus.'],
 decisions: [{id: 'keep_settings', question: 'Keep your existing settings?', default: true}]}
```

Missing/null capability files mean unavailable. Plans bind their contents, game/capability revision, archive revision and decisions with SHA-256. Web Crypto is used when available; an equivalent SHA-256 implementation keeps the explicit preview working on ordinary HTTP origins that lack `crypto.subtle`. The real runtime remains the authority for real plan hashes. Apply requires exactly the Boolean answers requested by that plan. Replaying identical answers returns one installation; changing answers refuses. Game-running state is checked again at apply. A changed archive/game invalidates an unapplied plan.

The mock is memory-only unless a caller explicitly injects storage. Only its own `modbridge.installer.mock.v1` JSON is read/written; malformed envelopes and duplicate identities refuse. No filesystem API is used. Persistence failure rolls the preview state back so a refused apply can retry safely. Plans themselves are ephemeral and must be reviewed again after reloading.

Separate demo controls are `registerGame(record)`, `registerArchive(record)`, `setScenario(codeOrNull)`, `setGameRunning(titleId, boolean)`, `setEnabled(titleId, installId, boolean)`, `reorder(titleId, orderedInstallIds)`, `setSetup(titleId, object)` and `setProgress(titleId, objectOrNull)`. These are not added to the real installer contract.

Mock-only status fields are `mock: true`, `game_running`, `conflicts` (an array of `{mod_ids: [...]}`), `last_session` (an array of `{install_id, result, reason}`), `self_test: {state, message}`, `setup: {state, message}` and `progress` (null or `{stage, message, estimated_minutes, percent, can_leave}`). New preview records default to setup needed and self-test not run. They never imply console execution.

Scenario codes: `no_internet`, `nexus_expired`, `corrupt_download`, `unsupported_type`, `game_running`, `console_busy`, `install_refused`, `plan_drift`.

## Central config names

`routes`: capabilities, account, games, jobs, downloads, installed, authStart, authDisconnect, detectedTitles, detectedTitlesRefresh, titlePages, titlePagesRefresh, titlePage(cursor), titleCurrent(snapshot), gameMods(game), mod(game, mod), modFiles(game, mod), job(id), modService, modServiceRecords.

`nexus`: authorizationOrigin, modPage(game, mod), filePage(game, mod, file). `installer.paths`: plan, apply, restore, status. No game, console address, live archive location or mod name is encoded in configuration.

## Verification

The test-first baseline failed all 27 initial behavioral checks because the factories were missing. The first implementation passed 27/27. Additional edge regressions were reproduced and fixed: persistence failure leaving an in-memory install, duplicate saved identities silently replacing records, and generated installation IDs colliding with seeded records. Four further checks first failed for absent SHA fallback and dropped conflict fixtures. Independent peer review found malformed nested replies reaching UI code and slash/backslash routes that browsers can normalize to another origin; three targeted checks reproduced those issues and the empty-capability case before the fixes. The final focused run passes **38/38**, including concurrent replay, exact HTTP bodies, missing capabilities, malformed arguments/replies, all eight requested scenario codes, real failure with no fallback, browser globals, restore, preview management and storage isolation. SHA fallback checks cover NIST empty, `abc`, 56-byte and million-`a` vectors; Unicode; ten padding/block boundaries; and exact equivalence with Node crypto/Web Crypto. An ordinary-HTTP browser environment without `crypto.subtle` can plan/apply with the same content hash.

Command: `node --test tests/installer_service_tests.cjs`. No console or external network was used. Root owns the combined UI and project-wide regression run.
