# PS5 Mod Project — private developer handoff

Source snapshot prepared on 2026-10-14 for a successor developer. This combines the
runtime port, preparation/install tools, Mod Bridge app, controller-first Nexus UI,
capability team work, and DLL research. It is a development handoff, not a finished
release or a claim that every PC mod works on PS5.

Start with [the handover](handoff/HANDOVER.md), [source map](handoff/SOURCE-INDEX.md),
and [dependency/rebuild notes](handoff/DEPENDENCIES.md). The export has fresh Git
history. Original worktrees and their Git history were not pushed.

The handoff history was rebuilt after a privacy review. Personal computer/network
identifiers and unrelated saved-session records are excluded. Retained `sessions/`
content is project source, reproducibility scripts and technical handoffs.

The [#1/#3/DLL transfer supplement](supplements/cap01-cap03-dll-20261014/README.md)
links the latest stages and preserves reviewed offline evidence from the complete
location inventory. All 62 stages and all 95 listed scratch scripts are included.

| Folder | Contents |
| --- | --- |
| `main/outputs/2026-10-08-l26-runtime/` | Current runtime overlays, C++ source, build/package drivers, tests |
| `main/outputs/2026-10-08-l25-prep/` | Installer/pak/material tools, launch/build gates, own diagnostic mods, staged changes |
| `main/outputs/ps5-mod-bridge/` | Main app, native bridge and service/core work |
| `ui-tv/outputs/ps5-mod-bridge/` | Separate TV/controller UI branch, installer mock contract, tests, screenshots |
| `installer-compat/` | Compatibility catalog, installer planning/rollback source, tests and review documents |
| `teams/` | Capability A–K, #7/#10/#13 and kernel/DLL work, Stellar Blade prep, individual handoffs |
| `legacy-port-source/` | Vetted text/source subset of older port baselines referenced by runtime builds |
| `sessions/` | Earlier PS5 source sessions and GPT handoffs |
| `third-party-licenses/` | Retained upstream attribution and licenses |

## Start locally

The UI uses an explicit mock installer until the runtime service is connected.
The app contract is PLAN → confirmed APPLY → RESULT, and RESTORE for uninstall.
Read `ui-tv/outputs/ps5-mod-bridge/docs/ui/HANDOFF.md` before changing its source.
Do not overwrite the main app with the entire older UI snapshot: integrate its web
modules and two static-asset adapters after comparing current native changes.

Basic host checks (no PS5 access):

```sh
python3 tools/verify_export.py
cd installer-compat/installer
python3 -m unittest test_installer test_config_outputs test_zip_safety test_detector test_uninstall test_conflicts test_adapter test_adapter_edge
```

Browser tests also need Node and a locally installed Chromium-compatible browser;
their source is included. See `handoff/VERIFICATION.md` for checks actually run on
this exported copy and their limits.

The separate `legacy-nexus-source/` folder retains authored Nexus transport/client
research source. The earlier exclusion list misclassified those folders and the
app's `companion/modbridge_nexus` package; they were reviewed and restored.

## Private configuration

Original console addresses were replaced with
`ps5-host-not-configured.invalid`; original personal home paths with `/WORKSPACE`.
This deliberately leaves console tools unconfigured. Host-only UI tests need no
console address. If authorized later, copy `config/local.example.json` to
`.local/config.json`, fill in the new owner's console host, and run
`python3 tools/prepare_local_workspace.py --component main`. It renders a separate
gitignored local copy, never connects to a console, and leaves tracked source clean.
It does not restore missing dependencies or make old pinned receipts current.

## What stays out

Game executables/assets/containers and fetched game data; downloaded Nexus mod
copies; Epic Unreal source; console/system/game logs; private credentials and
network details; frozen builds, compiled binaries, object files, downloads, full
duplicated Mono trees, and caches. Source patches and dependency recipes remain.
Owned UI mock screenshots are retained. Some historical references point to
excluded evidence; they are not downloadable artifacts in this repository.

`handoff/SOURCE-MANIFEST.json` records the original and exported hashes of retained
files, sanitization, and origins without personal paths. Sanitized historical
receipts must be regenerated before any release; do not bypass their hash checks.

No new project-wide license was selected. Existing third-party licenses remain in
force for their files; see `LICENSE` and `handoff/DEPENDENCIES.md`. Keep the repository
private unless the owner separately authorizes redistribution and its licensing.
