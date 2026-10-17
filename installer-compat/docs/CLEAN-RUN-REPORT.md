# CLEAN-RUN REPORT — ps5-installer-compat

**Date:** 2026-10-07, 14:16–14:25 local (America/Chicago, UTC-5)
**Scope:** Mac-only. No console contact, no FTP, no payloads, no launches.
**Work confined to:** `/WORKSPACE/Projects/ps5-installer-compat/` and the
disposable clean-run copy at `/tmp/clean-run-test/`.

Covers three tasks: (1) contract-proposal merge, (2) clean-run verification,
(3) catalog source-attribution audit.

---

## 1. Contract-proposal merge — DONE

### What was merged

| before | after |
|---|---|
| `docs/CONTRACT-PROPOSAL.md` (five core interfaces, v1) | merged into the single `docs/CONTRACT-PROPOSAL.md` **v1.1** |
| `compat/CONTRACT-PROPOSAL.md` (pre-download badge, duplicate of the frozen original) | deleted; content is now **Section 4** of the merged doc; `compat/README.md` is a pointer |
| `compat/src-compat-checker/CONTRACT-PROPOSAL.md` | untouched — frozen provenance original (verified byte-identical to the deleted copy before deletion) |

### Merged document structure

1. Status and scope (ONE version: v1.1 merged, 2026-10-07; proposal-only; combined scope guarantee)
2. Components and boundaries (adds the `ps5_runtime_status` owner row)
3. Core interfaces (2.1 install, 2.2 uninstall, 2.3 compatibility-check pointer, 2.4 config entries, 2.5 conformance fill-in)
4. **Pre-download compatibility badge (two-phase contract)** ← the compat proposal, intact (gap, Phase 0, Phase 1, metadata asks, wire contract, caching, honesty rules)
5–7. Requests: mod manager / GPT / catalog maintainers (compat's catalog asks became Section 7)
8. **Open questions — ONE merged list** (7 items: 4 from the core proposal + catalog-version bump ownership + Phase-0 grade-name acceptance + compat_manifest adoption)
9. Implementation checklist (when approved)

`compat/README.md` points to "docs/CONTRACT-PROPOSAL.md, Section 4".

### Contradictions resolved (one version each)

1. **`lua_api_calls_detected` in the pre-download sketch vs "no sources before
   download"**: the core proposal's Section-3 sketch implied pre-download API
   detection; the badge proposal proves a source scan is impossible
   pre-download. Resolution: the field is **dropped**; pre-download grading is
   a description mention-scan only (Phase 0), explicitly never call-site
   analysis (documented in Section 4.1).
2. **Grade spelling**: core proposal used `partly`; badge proposal and the
   checker use `partly-compatible`. Resolution: single vocabulary in Section 3,
   `partly-compatible` canonical, with a note about the earlier spelling.
3. **Catalog version field**: core sketch's `runtime_catalog_version` vs badge
   contract's `catalog_version` wrapper. Resolution: one name,
   `catalog_version`, in the `{catalog_version, entries}` wrapper.
4. **Grade table + badge UI mapping** existed in both docs in different forms:
   now one table (Section 3) including the Phase-0 provisional grades.
5. Duplicate scope/guarantee preambles merged into one header block.

`REVIEW-PACKAGE.md` updated: 3 references to `compat/CONTRACT-PROPOSAL.md` now
point to `docs/CONTRACT-PROPOSAL.md` Section 4; its badge flow line now uses
the canonical `partly-compatible`. `REVIEW-PACKAGE.v1.md` is a pre-existing
historical backup and was left as-is.

---

## 2. Clean-run verification — DONE, ALL TESTS PASS

### 2a. `/tmp/sideproject` reference cleanup

Original grep found references in 21 files. Working copies fixed (5 files):

| file | fix |
|---|---|
| `catalog/SUMMARY.md` | scope + reproduce lines now reference `catalog/src-catalog-globals/` relative paths; added a caveat that the frozen build script still writes to its original absolute output path |
| `conformance/conformance_live.lua` | provenance comment now says "this module's staging directory" (no absolute path) |
| `tests/REVIEW.md` | header + 2 references now point to the preserved relative copy `tests/src-tester/` |
| `REVIEW-PACKAGE.md` | 2 explanatory notes rephrased to reference the frozen harness under `tests/src-tester/` and its caveat documented |
| (`compat/CONTRACT-PROPOSAL.md`) | deleted entirely by the merge (Section 1) |

Post-fix state: **zero** `/tmp/sideproject` references outside `src-*`
directories. The remaining references are all inside the frozen `src-*`
provenance originals (`catalog/src-catalog-*` build scripts and summaries,
`compat/src-compat-checker/demo-report.json`, `conformance/src-*`,
`tests/src-tester/*`) — intentionally preserved verbatim per the task premise
"the src-* directories already contain the originals"; the originals were NOT
edited. Consequence documented in `REVIEW-PACKAGE.md` and `catalog/SUMMARY.md`:
the frozen tester harness and build scripts reference their original absolute
staging layout and must have their path constants ported before being run from
this project.

### 2b. Copy to fresh temp directory

```
rm -rf /tmp/clean-run-test                                   -> exit 0
cp -R /WORKSPACE/Projects/ps5-installer-compat/ /tmp/clean-run-test/   -> exit 0
file count: source 192, clean copy 192 (equal)
```

### 2f. Copy errors

**None.** `cp -R` exit code 0, stderr empty (captured verbatim — no
`2>/dev/null` anywhere in this run), file counts equal.

### 2c/2d. All tests from the clean copy — PASS

Full raw output (stdout + stderr, unfiltered, includes ResourceWarnings) is
saved at **`test-results/raw-test-output.txt`** (77 lines). Summary:

| command (from clean copy) | result |
|---|---|
| `cd /tmp/clean-run-test/installer && python3 -m unittest test_installer test_detector test_uninstall test_conflicts` | **Ran 102 tests — OK**, exit 0 |
| `cd /tmp/clean-run-test/compat && python3 -m unittest test_checker` | **Ran 42 tests — OK**, exit 0 |

Total: **144/144 unit tests pass from the fresh copy.** (Counts reflect the
current tree: checker has 42 tests, not the 35 listed in the older
REVIEW-PACKAGE table; test files were extended during 2026-10-07 review work.)
Environment: Darwin arm64, Python 3.9.6. The only stderr content is 7
`ResourceWarning: unclosed file` notices from test code reading manifests —
cosmetic, preserved in the raw log.

### 2e. No test depends on paths outside the project

- `grep -rn '/tmp/sideproject'` over the clean copy's `installer/` and
  `compat/`: only hit is `compat/src-compat-checker/demo-report.json` (frozen
  provenance artifact, not read by any test).
- `grep -rn '/WORKSPACE'` over the same: only hit is
  `compat/townfall-item-spawner-grade.json`, a checker *output artifact*
  (created 14:16, concurrent with this run) whose provenance fields record the
  original host paths; no test reads it (verified: zero references from
  `test_checker.py`/`checker.py`).
- Project-wide `--include=*.py,*.lua,*.json` grep for `/tmp/sideproject`
  excluding `src-`: zero hits.
- Functional proof: both suites ran green from `/tmp/clean-run-test`, a
  different path than the original, with test sandboxes created under the OS
  temp dir (`tempfile`, `/var/folders/...`), i.e. throwaway run-scratch, not
  input dependencies.

### Anomaly noted

The project was being modified concurrently while this cleanup ran (new files
`compat/townfall-item-spawner-grade.json`, `compat/catalog-real-merged.json`,
`compat/merge_catalogs.py`, updated `REVIEW-PACKAGE.md`, etc. appeared between
reads). The clean-run snapshot is point-in-time **2026-10-07 14:16:33 -0500**
and is internally consistent (192/192 files copied, all tests green).

---

## 3. Catalog source-attribution audit — REPORT ONLY (JSON not edited)

**Question:** do `catalog/types.json` and `catalog/globals.json` reference the
PORT's source files (native-coherent-runtime-1) or UPSTREAM UE4SS? Upstream
should be the pinned subject (3.0.1); port learnings belong only in `notes`.

### Answer in one line

The catalogs' **prose fields correctly pin upstream UE4SS 3.0.1**, but the
**location-citation fields carry PORT-tree line numbers presented as bare
upstream-looking paths** — port-derived data sitting in signature-adjacent
fields, which is what needs to move.

### 3.1 What is correct (verified against the local trees)

- **Upstream 3.0.1 pin is real:** vendored upstream tree
  `.../ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/generated_src/version.cache`
  contains `3.0.1.0.0`. 96/98 globals state "vendored UE4SS 3.0.1
  (version.cache 3.0.1.0.0)" in `ue4ss_version`.
- **`metatable_name` values are upstream-faithful, NOT port-specific.** The
  task's example — `FNameUserdata` — exists verbatim in upstream 3.0.1:
  `src/LuaType/LuaFName.cpp:17: auto metatable_name = "FNameUserdata";`.
  Same for `FSoftObjectPathUserdata`, `FURLUserdata`, `ThreadIdUserdata`
  (upstream LuaFSoftObjectPath.cpp:17, LuaFURL.cpp:13, LuaThreadId.cpp:18).
  **Nothing to move here** — this corrects the assumption in the task
  description.
- **The 2 port-addition entries (`loadfile`, `dofile`) are honestly labeled:**
  `ue4ss_version` says "PS5 port addition in native-coherent-runtime-1 (absent
  from vendored UE4SS 3.0.1)", `pc_only_behavior` explains the divergence, and
  `native_lua_loadfile`/`native_lua_dofile` indeed exist only in the port
  (verified: no hits upstream; port LuaMod.cpp:803/1175). Port info in these
  entries is explicitly flagged, not disguised.

### 3.2 What is port-derived in the wrong fields (needs to move)

Both catalogs cite **bare relative paths** (`src/Mod/LuaMod.cpp:4565`) that
are ambiguous between the two trees — and spot-checks prove the line numbers
match the **PORT** tree, not upstream:

| citation (catalog) | port tree | upstream 3.0.1 tree |
|---|---|---|
| globals `RegisterHook` → `src/Mod/LuaMod.cpp:4565` | exact: `m_lua.register_function("RegisterHook", ...)` | that registration is at **:4199**; upstream :4565 is unrelated game-thread code |
| types `FName.global_constructor` → `src/Mod/LuaMod.cpp:6190-6193` | exact: pre-load global FName/NAME_None block | upstream :6190 is UnregisterHook cleanup |
| `src/Mod/LuaMod.cpp` overall | **7781 lines** (port adds module loader etc.) | **7331 lines** |
| LuaType cpp/hpp files | port inserts Prospero include blocks (e.g. `+5` lines atop LuaUObject.cpp: `#include <type_traits>`, `#ifdef __PROSPERO__`) and renames `DynamicOutput/DynamicOutput.hpp` → `Output.hpp`, shifting line numbers | unshifted |
| `include/LuaType/LuaUObject.hpp:172` (GetAddress) | matches | also matches (header diffs are in-place line substitutions) — so *some* citations happen to be valid in both trees |

Affected fields and volume:

- **`globals.json`** (98 entries): `registration_sites` — 100 citations
  (94 × `src/Mod/LuaMod.cpp`, 6 × `src/Signatures.cpp`), port line numbers,
  no tree label. This is the primary "signature field" holding port data.
- **`types.json`** (42 types / 456 methods / 20 metamethods): `source_file`
  (42), `methods[].implementation` + `metamethods[].implementation` (~370
  citations), and `FName.global_constructor` — same bare-path, port-line
  pattern. `Signatures.cpp` is also port-diverged.
- Nowhere in either JSON is the citation tree identified; the tree attribution
  exists only in `catalog/SUMMARY.md` prose ("primary source = port,
  cross-check = vendored upstream 3.0.1").

### 3.3 Documented remediation (per instructions, NOT applied)

1. Decide the catalog's canonical subject: **upstream UE4SS 3.0.1 pinned**.
2. Re-derive `registration_sites` / `source_file` / `implementation` /
   `global_constructor` line numbers against the **upstream** tree (e.g.
   RegisterHook main-state registration → upstream LuaMod.cpp:4199).
3. Where the port location is itself the useful fact (e.g. the FName pre-load
   block, port-only behavior), keep it as a clearly-labeled second citation or
   move it into the entry's `notes` field — never as the unlabeled primary
   citation in signature/status-adjacent fields.
4. `loadfile`/`dofile`: acceptable as-is (explicitly flagged port extensions);
   optionally add an `is_port_extension: true` marker so tools can filter.
5. Add the wrapper the badge contract already asks for
   (Section 4.4 of the merged proposal): `{catalog_version, entries}` plus a
   `citation_tree: "upstream-ue4ss-3.0.1"` stamp.
6. No change needed for `metatable_name` (upstream-faithful, see 3.1).

---

## Artifacts produced by this cleanup

- `docs/CONTRACT-PROPOSAL.md` — merged v1.1 (single version, single open-questions list)
- `compat/README.md` — pointer to docs/CONTRACT-PROPOSAL.md Section 4
- `compat/CONTRACT-PROPOSAL.md` — deleted (original preserved at `compat/src-compat-checker/`)
- `test-results/raw-test-output.txt` — full raw clean-run output (copy + 144 tests + verification greps)
- `docs/CLEAN-RUN-REPORT.md` — this file
- Updated: `REVIEW-PACKAGE.md`, `catalog/SUMMARY.md`, `conformance/conformance_live.lua`, `tests/REVIEW.md`
