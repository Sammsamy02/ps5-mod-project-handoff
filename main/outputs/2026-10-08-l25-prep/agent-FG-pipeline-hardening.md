# Agent FG — build-discipline pipeline hardening (marker manifest + stamp + frozen snapshot)

Date: 2026-10-09/10 (local Oct 9 late evening). Mac-local only, zero console
contact. Trigger: the lfb-211254 phantom build (the Lua/FName bridge fix was in
the tree but never composed into the PRX; caught only after a wasted console
launch). Claude's hardening items (b) per-fix declared markers and (c) frozen
snapshot + recorded commit-equivalent, plus the stamp plumbing for agent FF's
[BUILD] first-log-line consumer.

## Deliverables (all four files, nothing else touched)

| Artifact | Path | Status |
|---|---|---|
| Marker manifest (per-fix REQUIRED markers) | `outputs/2026-10-08-l25-prep/harvest/markers-declared.json` | NEW, 13 markers |
| verify_build.sh `--manifest` mode | `outputs/2026-10-08-l25-prep/harvest/verify_build.sh` | EDITED (old CLI/behavior byte-compatible) |
| Stamp generator + gates in the build driver | `outputs/2026-10-08-l26-runtime/build_variants.py` | EDITED (additive only; EZ's --incremental untouched) |
| Frozen-snapshot script | `outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh` | NEW, executable |

Not touched (per rules): CHANGES.json, candidate sources, bind_launch.sh,
l26_packaging_driver.py, grep_markers.py (verify_build.sh's manifest scan is
self-contained for exactly this reason).

## 1. Marker manifest — `harvest/markers-declared.json`

Schema `markers-declared/1`, 13 per-fix markers. Every marker that has landed
was declared from the ACTUAL literal bytes in the candidate sources (file:line
recorded per entry); runtime-rendered lines are declared as their composable
literals (a logged `[ST] lua-ffo PASS` line never appears contiguously in a
PRX — its proof is the wide `[ST] lua-` template PLUS the narrow
`ffo PlayerController` probe/chunk literal, both required).

Encoding semantics (documented in the file's `encoding_rules`): UTF-16LE is
primary (wide STR/StringType literals were exactly the phantom class); each
`required[]` entry lists the encodings that must EACH contain the text
(`either_encoding: true` = at least one, used only for markers whose source
has not landed yet — concurrent agents FD/FF own those; both encodings are
still checked and reported, and the owner tightens the entry once the literal
lands).

| id | canonical marker | fix | required literals (encoding) |
|---|---|---|---|
| lua-fn-bridge-log | `[LUA-FN]` | EY Lua->FName bridge | `[LUA-FN] in='` (utf-8; LuaFNameBridge.hpp:460 snprintf) |
| lfb-composed | `[LFB]` | EY bridge composition | `[LFB] lua-fname-bridge composed` (utf-16le; :553 STR) |
| ffo-query-fallback | `[FFO] query-fallback` | DA/CP verified query | `[FFO] query-fallback` (utf-16le; UObjectGlobals.cpp:835) |
| ffo2-next-round | `[FFO2]` | FD next round (unlanded) | `[FFO2]` (either) |
| lfb-m-marshaling | `[LFB-M]` | FD marshaling (unlanded) | `[LFB-M]` (either) |
| build-stamp-first-line | `[BUILD]` | FF stamp consumer (unlanded) | `[BUILD]` (either) |
| st-wcslen-16units | `[ST] wcslen-16units` | FD wide-libc self-test (unlanded) | `[ST] wcslen-16units` (either) |
| st-wstring-rt | `[ST] wstring-rt` | FD wide-libc self-test (unlanded) | `[ST] wstring-rt` (either) |
| st-lua-ffo | `[ST] lua-ffo` | EV/EY Lua seam battery | `[ST] lua-` (utf-16le; LuaMod.cpp:159) + `ffo PlayerController` (utf-8; :171) |
| fn-tostring-pool-decoder | `[FN] tostring` | EI/EQ/ER L27 supersede | `[FN] tostring` (utf-16le; UnrealInitializer.cpp:1347) |
| fn-pool-decoder-verified | `[FN] pool-decoder verified` | ER pool decoder | `[FN] pool-decoder verified` (utf-16le; NameTypes.cpp:445) |
| variant-c-admission | `[Seeds][L26] engine delete-listener ops admitted` | AE/BC/EE/EQ Phase 6 | same text in BOTH utf-16le AND utf-8 (send_narrow_diag wide+narrow pair, UnrealInitializer.cpp:1450-1455) |
| slot2-fname-ctor-bound | `slot 2 FName ctor bound` | slot-2 admission | `slot 2 FName ctor bound` (utf-16le; UnrealInitializer.cpp:1324) |

The five unlanded markers are deliberately REQUIRED for the next build: the
gate fails until their fixes both land and compose in — that is the point of a
declared-marker gate (a marker that only guards what already exists cannot
catch the next phantom).

## 2. verify_build.sh — `--manifest` mode

`verify_build.sh <prx> --manifest [markers.json] [receipt]` (manifest defaults
to `markers-declared.json` next to the script). Section [1/3] becomes the
declared-marker scan: per-marker PASS/MISS with per-entry utf-8/utf-16le
counts, UTF-16LE primary; ANY miss → `FAIL (declared-marker-missing)`, exit 1.
Identity ([2/3] magic) and FINAL-RECEIPT ([3/3] sha/bytes) checks still run.
Unreadable manifest → exit 2. The old invocation
`verify_build.sh <prx> [receipt]` is unchanged (delegates to grep_markers.py,
same exit codes; verified). Usage line and VERIFY-TIMESTAMP now carry `mode=`.

## 3. Stamp generator in build_variants.py (gates added: `G-stamp-present`, `G-stamp-stale-cleanup`)

Additive changes only (new constants + `collect_change_ids()` +
`generate_build_stamp()` + one call in `main()` after selection/before any
compile + receipt fields; no existing gate, path, or EZ incremental logic
modified):

- `--execute`: writes `typescan-r4/candidate/include/GeneratedBuildStamp.hpp`
  BEFORE compiling — `#pragma once` + `#define BRIDGE_BUILD_ID "l26-<UTC ts>"`,
  `#define BRIDGE_CHANGE_IDS "<part>:<scope key>;..."` (derived from the
  CHANGES.json scope keys of the parts that actually contribute file rows:
  profile + typescan-r4 = 12 ids; thread has zero rows), `#define
  BRIDGE_SNAPSHOT_ID "<env BRIDGE_SNAPSHOT_ID>"`. Gate **G-stamp-present**
  then verifies the file exists, round-trips byte-exact, carries all three
  defines and no not-generated value — else it records False and RAISES
  before a single unit compiles (upload-grade discipline: env unset/invalid
  (must be 8-64 lowercase hex) refuses the build).
- `--plan`: gate recorded as PLANNED with the would-be values; **G-stamp-stale-cleanup**
  DELETES any stale stamp so an unstamped tree compiles to FF's `__has_include`
  fallback instead of a leftover header from an older build.
- `<GeneratedBuildStamp.hpp>` is made resolvable by appending
  `typescan-r4/candidate/include` to the injected -I dirs (after all existing
  dirs; G12 re-proved routing with it in place — see tests).
- Receipts record the stamp: `VARIANT-MARKER.json`/`TWIN-RECEIPT.json`
  (`build_stamp` in the variant marker), `SOURCE-SELECTION.json`
  (`pin(GeneratedBuildStamp.hpp)` in source_pins), `CHANGE-SELECTION.json`
  and `PLAN.json`/`EXECUTION-RECEIPT.json` (`build_stamp` section).

## 4. snapshot_tree.sh — frozen snapshot (item c)

`outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh` freezes the exact build
inputs into `outputs/2026-10-08-l26-runtime/snapshots/snapshot-<UTC ts>.tar.gz`
(arc paths relative to outputs/): the whole candidate tree
(typescan-r4/candidate), the three part CHANGES.json files, the three drivers
(build_variants.py, build_runtime.py, l26_packaging_driver.py), and the
verifier stack (verify_build.sh, grep_markers.py, markers-declared.json,
snapshot_tree.sh). Sidecars: `snapshot-<ts>.manifest.json` (schema
tree-snapshot/1: tar sha256 + EVERY file's sha256, 50 files) and a plain
`snapshot-<ts>.sha256` (`shasum -a 256 -c` compatible). Prints
`SNAPSHOT-ID <first 8 hex of tar sha256>` on stdout.

### THE TWO-COMMAND BUILD SEQUENCE (snapshot -> build -> package -> verify)

Run from the WORKTREE ROOT `…/translation-core/what` (the drivers' base pins
are relative to it — running from outputs/ crashes G1 on a missing pinned
path):

```bash
cd …/translation-core/what
# 1) freeze the tree (prints SNAPSHOT-ID <8hex>)
SNAP=$(outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh | awk '/^SNAPSHOT-ID /{print $2}')
# 2) build with the stamp (refuses without a valid BRIDGE_SNAPSHOT_ID)
BRIDGE_SNAPSHOT_ID="$SNAP" python3 outputs/2026-10-08-l26-runtime/build_variants.py --execute
# 3) package
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --prepare
python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --execute
# 4) pre-upload gate: EVERY declared marker must be composed into the PRX
outputs/2026-10-08-l25-prep/harvest/verify_build.sh <built.prx> --manifest
```

The env path is the wired mechanism (driver reads BRIDGE_SNAPSHOT_ID; the
snapshot script prints SNAPSHOT-ID; the parent joins them). Additive-only:
`--incremental` local-iteration builds without the env still stamp but fail
G-stamp-present exactly like full builds — pass the env (or a previously
printed id) whenever executing.

## 5. Test evidence (all offline, no console contact)

### a) Old-PRX manifest gate — catches the phantom class (REQUIRED test)

```
$ outputs/2026-10-08-l25-prep/harvest/verify_build.sh \
    outputs/2026-10-08-l26-package-A.lfb-211254/pass-1/modbridge_supplier_composition.prx --manifest
manifest: …markers-declared.json (schema markers-declared/1, 13 declared markers)
prx: sha256=58a30cd1…fa1adb bytes=31417863
[MISS] lua-fn-bridge-log   canonical '[LUA-FN]'        entry "[LUA-FN] in='"      utf-8=0          (REQUIRED-MISSING)
[MISS] lfb-composed        canonical '[LFB]'           entry '[LFB] lua-fname-bridge composed' utf-16le=0 (REQUIRED-MISSING)
[PASS] ffo-query-fallback  … utf-16le=1
[MISS] ffo2-next-round / lfb-m-marshaling / build-stamp-first-line / st-wcslen-16units / st-wstring-rt  (unlanded: utf-8=0 utf-16le=0)
[PASS] st-lua-ffo          entries '[ST] lua-' utf-16le=3 + 'ffo PlayerController' utf-8=1
[PASS] fn-tostring-pool-decoder (utf-16le=1) · fn-pool-decoder-verified (utf-16le=1)
[PASS] variant-c-admission BOTH required  utf-16le=1 utf-8=1
[PASS] slot2-fname-ctor-bound utf-16le=1
SUMMARY: declared=13 pass=6 miss=7
MISSING DECLARED MARKERS (fix the build, not the gate): lua-fn-bridge-log, lfb-composed, ffo2-next-round,
       lfb-m-marshaling, build-stamp-first-line, st-wcslen-16units, st-wstring-rt
magic=4f153d1d OK · receipt RESULT: MATCH (sha+bytes)
VERDICT: FAIL (declared-marker-missing) — DO NOT UPLOAD; fix the build, not the gate.
VERIFY-TIMESTAMP … mode=manifest … result=FAIL:declared-marker-missing      EXIT=1
```

`[LUA-FN]` MISS on the artifact that shipped without the bridge = the gate
proves the phantom class. Structure markers (variant C BOTH encodings, slot-2,
ST lua battery, FN tostring/pool-decoder, FFO query-fallback) all PASS on the
same PRX, so the misses are the phantom/unlanded set, not a broken scanner.
Backward compat: old-CLI run on the same PRX reproduces the previous output
(`FAIL (critical-marker-missing)`, registry mode, exit 1); bad usage exits 2.

### b) Stamp generator unit test (direct call, all four behaviors)

1. plan, no stale stamp → `G-stamp-stale-cleanup` (PLANNED, none present) +
   `G-stamp-present` (PLANNED, snapshot_env set/valid=false,
   execute_refusal_expected=true); no file written; `-I …/candidate/include`
   injected.
2. plan with a stale stamp → stale file DELETED, `G-stamp-stale-cleanup` True.
3. execute without env → stamp carries the not-generated value,
   `G-stamp-present` **False**, RuntimeError "refusing to build… run
   snapshot_tree.sh, export BRIDGE_SNAPSHOT_ID…" BEFORE any compile.
4. execute with `BRIDGE_SNAPSHOT_ID=1cf5be93` → header written:
   `BRIDGE_BUILD_ID "l26-20261010T034304Z"`, `BRIDGE_CHANGE_IDS
   "profile:scope;typescan-r4:scope;…scope_ey_lua_fname_bridge"` (12 ids),
   `BRIDGE_SNAPSHOT_ID "1cf5be93"`; `G-stamp-present` True; tree cleaned after.

### c) build_variants.py --plan regression (run from the worktree root)

Baseline (pre-edit) vs post-edit, identical tree state otherwise:

| | baseline | post-edit |
|---|---|---|
| exit | 0 | 0 |
| gates | 69 | **71** (+G-stamp-present, +G-stamp-stale-cleanup) |
| steps | 810 | **811** (+generate-build-stamp) |
| units / affected | 374 / 199 | 374 / 199 (unchanged) |
| G12 header-routing (4 gates) | all True | **all True** (with the stamp -I present in 796/796 compile-unit argvs — no routing regressions) |
| False gates | 2× G2-part-change-pins, 1× G2q-base | 3× G2-part-change-pins, 1× G2q-base |

The one extra G2-part-change-pins drift row is `candidate/src/LuaUObject.cpp`
(mtime 22:45 local, edited by a parallel agent BETWEEN the two plan runs;
UnrealInitializer.cpp 22:22 / UObjectGlobals.cpp 22:21 were already the two
baseline drift rows). None are FG files; plan mode tolerates mid-flight drift
by design and --execute will hard-fail until those agents refresh their row
hashes. The G2q unrealinitializer.base False is the pre-existing tolerated
base-pin expectation (Q rebase path), unchanged.

### d) snapshot_tree.sh end to end

```
$ outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh
manifest: …snapshots/snapshot-20261010T033742Z.manifest.json (50 files)
SNAPSHOT-ID 1cf5be93
SNAPSHOT-TAR …/snapshots/snapshot-20261010T033742Z.tar.gz
snapshot: 50 files -> …snapshot-20261010T033742Z.tar.gz
  (sha256 1cf5be93ec5bafec36c03d5299a4983cd09b78c59aa8e5c972c3d4c744d1ef60)
$ shasum -a 256 -c snapshot-20261010T033742Z.sha256   -> OK
manifest spot-check: per-file sha256 of grep_markers.py matches disk; schema tree-snapshot/1
```

## 6. Notes / handoffs

- FF: the stamp header is `<GeneratedBuildStamp.hpp>`, macros
  `BRIDGE_BUILD_ID` / `BRIDGE_CHANGE_IDS` / `BRIDGE_SNAPSHOT_ID`; the -I that
  makes it resolvable is injected by the driver in EVERY mode; the not-generated
  value your consumer falls back to never appears in a stamped header. Please
  tighten the `[BUILD]` manifest entry (exact literal + encoding) once your
  consumer lands; same for FD's four markers.
- The manifest's unlanded markers intentionally fail today's artifacts — do
  not "fix" the gate; fix the compose. `[LUA-FN]`/`[LFB]` must go green on the
  NEXT real build before any upload.
- Driver receipts now carry `build_stamp`; `--execute` also refuses if a
  stale `GeneratedBuildStamp.hpp` survived into an execute (it is
  overwritten fresh before the gate check).
- No console contact at any point; no source files, CHANGES.json,
  bind_launch.sh, l26_packaging_driver.py or grep_markers.py modified.
