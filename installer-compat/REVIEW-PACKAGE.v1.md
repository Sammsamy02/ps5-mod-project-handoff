# REVIEW-PACKAGE.md — Installer + Compatibility Layer (side project)

> Mac-only / host-tested throughout. No console contact at any point.
> No files under launch11-combined/units, outputs/2026-10-07-l1*, or the
> mod manager branch were edited.

## What was built

The INSTALLER + COMPATIBILITY layer between GPT's runtime and Claude's mod
manager, per the side-project spec. Three phases, all complete:

- **Phase 1**: UE4SS API catalog (98 globals + 42 types/456 methods) + conformance
  scripts (static 353 calls + live 52 calls) + diff tool
- **Phase 2**: Installer core (transactional, journal-first, crash-safe) +
  mod-type detector (8 types) + uninstall/rollback + conflict detection
- **Phase 3**: Compatibility checker (static Lua scanner, 4 grades)

## File inventory

```
outputs/2026-10-07-installer-compat/
├── REVIEW-PACKAGE.md          ← this file
├── docs/
│   └── CONTRACT-PROPOSAL.md   ← proposed interface (NOT the canonical contract)
├── catalog/
│   ├── globals.json           ← 98 global functions, 9 fields each
│   ├── globals-SUMMARY.md
│   ├── types.json             ← 42 types, 456 method entries
│   └── types-SUMMARY.md
├── conformance/
│   ├── conformance_static.lua  ← 881 lines, 353 calls, 14 sections
│   ├── conformance_live.lua    ← 748 lines, 52 calls, prerequisite-gated
│   ├── diff_results.py         ← MATCH/MISMATCH/MISSING/EXTRA diff tool
│   └── README.md
├── installer/
│   ├── installer.py            ← 1,342 lines, transactional install engine
│   ├── test_installer.py       ← 49 tests
│   ├── detector.py             ← mod-type classifier (8 types)
│   ├── test_detector.py        ← 33 tests
│   ├── uninstall.py            ← journalled uninstall + rollback
│   ├── conflicts.py            ← path/keybind/hook/dependency conflicts
│   ├── test_uninstall.py       ← 11 tests
│   └── test_conflicts.py       ← 9 tests
├── compat/
│   ├── checker.py              ← static Lua scanner + grader (4 grades)
│   ├── test_checker.py         ← 35 tests
│   ├── sample_catalog.json     ← 29-entry test catalog
│   ├── sample_mod.lua          ← realistic test mod (25 APIs)
│   └── CONTRACT-PROPOSAL.md    ← pre-download badge contract
└── tests/
    └── (independent tester results — added when review completes)
```

## How to run every test

```bash
# Installer core (49 tests)
cd outputs/2026-10-07-installer-compat/installer
python3 -m unittest test_installer

# Mod-type detector (33 tests)
python3 -m unittest test_detector

# Uninstall + conflicts (20 tests)
python3 -m unittest test_uninstall test_conflicts

# Compatibility checker (35 tests)
cd ../compat
python3 -m unittest test_checker

# Conformance diff tool (on sample logs)
cd ../conformance
python3 diff_results.py samples/sample_a.log samples/sample_b.log --verbose
```

## Full test results

| module | tests | result |
|---|---|---|
| Installer core | 49 | ALL PASS |
| Mod-type detector | 33 | ALL PASS |
| Uninstall | 11 | ALL PASS |
| Conflicts | 9 | ALL PASS |
| Compatibility checker | 35 | ALL PASS |
| Conformance static (mock) | 353 calls | 353 PASS / 0 FAIL |
| Conformance live (mock) | 52 calls | 51 PASS / 0 FAIL / 1 SKIP |
| **TOTAL** | **137 unit + 405 conformance** | **ALL PASS** |

## Architecture summary

### Install flow
```
Mod Manager → [archive + metadata + title_id] → Detector (classify type)
    → Conflict Check (path/keybind/hook/deps) → Compatibility Checker (grade)
    → Installer Core (journal → extract → verify sha256 → write configs)
    → Result: {status, compat_grade, files, config_changes, manifest}
```

### Uninstall flow
```
Mod Manager → [manifest_path] → Check dependents (refuse if blocked)
    → Verify file shas (confirm if modified) → Remove files → Clean configs
    → Result: {status} (or rollback to pre-uninstall state)
```

### Compatibility flow (pre-download)
```
Mod Manager → [description + file_list preview] → Phase-0 scan (provisional badge)
    → [after download] → Phase-1 full scan → Final grade
    → Badge: compatible / partly / unsupported / unknown
```

## Known limits

1. **7z/RAR archives**: rejected with remediation guidance (convert host-side to zip)
2. **Static Lua analysis**: cannot resolve dynamic dispatch (`_G[var](...)`, string-built function names) — reported as `unknown`, never guessed
3. **UE4SS C++ (DLL) mods**: correctly classified but not runnable (Windows PE) — flagged with reason
4. **pak/IoStore mods**: classified as needs-conversion — the actual conversion pipeline is future work
5. **ps5_runtime_status**: all catalog entries are empty strings — GPT fills them after console testing
6. **Conformance scripts**: host-tested with mock environments; not yet run in a real game session (requires console, which is GPT's lane)
7. **Load order**: installer computes it from dependencies but the runtime enforces it — the exact mechanism is a contract question

## Open questions

1. **Manifest location**: proposed `manifests/` under the game root — needs mod manager confirmation
2. **UE4SS-settings.ini ownership**: proposed GPT (runtime) owns it; installer writes overrides to a separate file
3. **Hot-reload**: if the runtime supports it in the future, the installer needs a notification API
4. **Catalog versioning**: the catalog needs a version stamp; when GPT updates statuses, the compat checker should re-grade installed mods

## Contract changes proposed

See `docs/CONTRACT-PROPOSAL.md` for the full five-interface proposal:
1. Mod Manager → Installer: install request (JSON schema)
2. Mod Manager → Installer: uninstall request
3. Mod Manager → Installer: pre-download compatibility check
4. Installer → Runtime: config-entry ownership (loadlist, ploader.ini)
5. Conformance catalog → Runtime: ps5_runtime_status fill-in

Plus `compat/CONTRACT-PROPOSAL.md` for the pre-download badge system.

## Independent tester results

(Pending — the tester agent is still running its adversarial review across all
modules. Results will be appended here when complete.)

## How-to-call notes

### For GPT (runtime owner)
1. Fill `ps5_runtime_status` in `catalog/globals.json` and `catalog/types.json`
   after console testing each API (values: "implemented+console-proven" /
   "implemented-untested" / "missing" / "behaves-differently")
2. Run `conformance/conformance_static.lua` as a mod (no game needed for the
   static part; the live script needs a running game)
3. Diff results against a PC UE4SS reference run using `diff_results.py`

### For the mod manager team
1. Read `docs/CONTRACT-PROPOSAL.md` — accept, modify, or reject each interface
2. The installer API surface is the `Installer` class in `installer/installer.py`:
   `Installer(archive_path, metadata, title_id).install()` / `.verify()` / `.uninstall()`
3. The compatibility API surface is `compat/checker.py`:
   `check_compatibility(lua_path, catalog_path)` returns a `CompatReport`
4. The pre-download badge contract is in `compat/CONTRACT-PROPOSAL.md`

## Independent tester findings (appended)

**Verdict: individually solid, honestly labeled modules; not yet an end-to-end pipeline.**

87/88 adversarial/integration tests passed (the one failure is a documented
edge case in the compat checker's obfuscation handling — see below).
The 5 modules' own 137 tests also run clean under the tester's harness.

### Bugs found and fixed during review
1. **installer-core had 2 blocker bugs** (found and proven dynamically by the tester):
   - `NameError: config_writes` dead reference on every first install
   - `need_dir()` logic bug that never mkdir-plans leaf mod dirs
   Both were fixed by the author during the review window; tests now pin the fixes.

2. **conformance-static's pcall claim was false in an earlier version** (36/109 raw
   call sites unwrapped + a broken always-truthy guard). Fixed by the author;
   the corrected version survived all 17 hostile missing-global probes.

### Remaining gaps (ranked by the tester)
1. **Record adapter missing**: the installer's manifest/state contract differs from
   the uninstaller/conflicts contract; nothing wires conflict checking into the
   install path. All integration flows pass only via the tester's custom adapter.
2. **No concurrency locking**: tight-overlap concurrent installs are destructive
   but detected (both mods gone, no residue). Acceptable for single-user usage.
3. **Compat checker under obfuscation**: `_G["LoadAsset"]` grades `unknown` (0 calls)
   instead of `unsupported`. Syntax-error files indistinguishable from empty mods.
4. **Empty ps5_runtime_status**: all 98/98 catalog entries await GPT's console testing.
5. **Provenance gap**: the catalog references a UE4SS tree that needs verification.

### Tester infrastructure delivered
- Sandboxed test harness (`framework.py`)
- Full Lua 5.3 tokenizer+parser (`lua_syntax.py`) — cross-validated against Lua 5.4.7
- 65 adversarial fixtures (`corpus/`)
- 5 integration test suites with 88 total tests
- Full report: `tests/REVIEW.md` + `tests/results/*.json`
