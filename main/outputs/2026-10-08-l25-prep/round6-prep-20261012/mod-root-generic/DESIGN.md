# Generic mod root — design (track mod-root-generic, 2026-10-12, revision 2)

All claims here are **Mac-only**. Nothing was uploaded, built into a PRX, or applied.
No console contact. Older console observations are cited as *design input only*
(labelled "earlier session"); they are not used as a verdict.

Revision 2 answers the adversarial review (`REVIEW.md`): the self-test and the
mod manager were missing from the inventory (M1), and the process's own title was
not evaluated as a source (M2). Minor points m1-m11 are fixed or answered inline
(marked "(review mN)").

## Verdict (plain)

1. **The mod-folder problem in the runtime is fixable without naming any game and
   without new imports.** Patch `0001` is staged (not applied): the runtime works out
   the running game's title from two places that must agree, and loads no mods if they
   don't. Mac tests: 35/35 folder cases, 17/17 file-format cases, 1/1 "kernel call not
   bound" case; 6/6 deliberately broken copies caught; the PS5 compiler accepts it.
2. **Honest limit of those two places:** both describe the console as a whole (which
   games are set up for the loader, which game folders are mounted). Neither looks at
   *this* game process. That is safe as long as only one game runs at a time. The one
   source that would look at this process (the game's own app info, which the loader
   itself uses) cannot be shipped yet: its value layout has one conflicting local
   source, and every way to call it either needs a new import or a value we have no
   source for (section 3.2). Decision for you.
3. **The self-test also has Townfall's folder built in** (review M1). Patch `0002`
   (staged) makes it title-free. Mac check: with the path shim it already landed in the
   running game's folder, but the shim's **writable area is shared by all games**
   (`/data/modbridge/writable`, no title), so Townfall and Stellar Blade would share one
   self-test progress file and one settings area per mod name. Proposed fix: A9.
4. **The runtime still carries a Townfall "image profile"** (executable checksum, code
   addresses, layout pins). On Stellar Blade it refuses to start (safe: game runs
   unmodded) until the automatic profile generator supplies these values. 21 Mac tools
   also hard-code Townfall's title.
5. **Console proof is still needed** (section 6), each step with your OK, driver
   session only.

## 1. Which file the current build compiles (Mac-only)

| Step | Evidence |
|---|---|
| `build_variants.py` builds only the runtime object; it never compiles the outer entry | B4 snapshot manifest lists 63 files, none is `runtime_module_entry.c` (`2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/snapshot-20261012T040240Z.manifest.json`) |
| The packaging driver takes `outer_entry` unchanged from the frozen l24 manifest | `2026-10-08-l26-runtime/l26_packaging_driver.py:92-203` (`derive_manifest`): it rebuilds `lineage_overlays` (:107-126, :203), appends `outer_headers` rows (:131-153) and replaces `outer_bridge` (:179); `outer_entry` is never written and is not in `substituted_keys` (:303) (review m11) |
| `outer_entry` = `outputs/2026-10-08-combined-outer-lifecycle/candidate-v2/runtime_module_entry.c`, sha256 `bc490893…f5802` | `2026-10-08-l26-package-A/BUILD-INPUTS.json` key `outer_entry` (same in `2026-10-08-l24-package/BUILD-INPUTS.json`) |
| The controller copies it to `lineage/lifecycle/runtime_module_entry.c` and compiles that | `2026-10-08-l26-package-A/package_combined.py:53` (copy) and `:76` (unit swap) |
| B4's object came from that copy | `2026-10-08-l26-package-A/pass-1/build/runtime_module_entry.d:2`; `STAGED-SOURCE-PINS.json` pins `bc490893…` |
| Package-A now holds B4 | `FINAL-RECEIPT-VARIANT-A.json` prx sha `d044cf0b…` = ledger row B4 |

Side note: `lineage/lifecycle/runtime_module_entry.c.sha256` says `c97055f4…`: a stale
l19 left-over, not the compiled file. Nothing reads it, but it is misleading.

## 2. Inventory: game-specific values (Mac-only)

Two scanners, both print how many files they checked and exit 1 on zero (and, now,
when any single input yields zero paths — review m10; negative controls in
`evidence/scanner-zero-input-negative.log`):

- **Runtime (PRX):** `scan_game_constants.py` over the B4 compiled set (4,165 files) +
  1e49188b list + B4 snapshot + generator input: **4,182 files, 894,197 lines, 176 hits,
  124 in B4-compiled files** (`evidence/game-constant-scan-B4.*`; re-run after the m10
  fix gives byte-identical output). Profile headers were also read by hand.
- **Tools, in-game diagnostic Lua, launch data:** `scan_tool_constants.py` over
  `outputs/2026-10-08-l25-prep` (skipping evidence/, packet/, round6-prep-20261012/,
  runs/, regression-logs/): **211 files, 105 with hits** (`evidence/tool-constant-scan.*`).

### A. Must become generic before Stellar Blade

| # | Where | What | What happens on another game |
|---|---|---|---|
| A1 | entry `lineage/lifecycle/runtime_module_entry.c:73-74` (uses :239, :280-281, :316) | title `PPSA33286`, mod root `/data/modbridge/roots/PPSA33286` | reads Townfall's mod folder. **Fixed by patch 0001.** |
| A2 | `lineage/lifecycle/CodeSignatureProfile.hpp:28-39` (pin 1 bytes come from `lineage/FNameAbiProfile.hpp:3-5`, "Generated game profile", 32-byte FName ctor prologue — review m8); checked in `CodeImageCache.hpp:86-119` | path to Townfall's decrypted executable on `/data`, image sha, size 194241424, extent 0xbc602c8, 6 code pins | **Corrected (review m1):** the file path is *not* tied to the running game; Townfall's dump stays on `/data`, so open and hash would succeed. What refuses is the live base/extent check `CodeImageCache.hpp:89` ("actual startup base/extent differs from pinned image"), which runs before the open (:93); if a game happened to share base and extent, the A3 live anchors refuse. For the generator: the profile must be tied to the live image, not to a file path. |
| A3 | `lineage/lifecycle/runtime_start_bridge.cpp:219-241`, gate :431-482 | data-window anchors at fixed Townfall vaddrs, base 0x400000, extent 0xbc602c8 | anchors do not match → conversion refused |
| A4 | `lineage/SeedTable.h:11-36`, `lineage/SeedValidation.hpp:129`, `lineage/ImageLoadProfile.hpp:3-12` | seed labels (GUObjectArray, FName ctor, GMalloc …), image sha, program headers | seed checks refuse |
| A5 | `lineage/lifecycle/EngineListenerOpsProfile.hpp:48-69`, `EngineToStringProfile.hpp:125`, `ThreadNameProfile.hpp:4-9` | engine-function labels + prologue bytes + image sha ("re-derive per title", :34-35) | pins refuse |
| A6 | `profile/units/RuntimeBuildProfile.hpp:5-13` (evaluated `UnrealInitializer.cpp:1610-1631`), `CompiledNativeLayoutDeclarations.hpp:6` | profile name, image sha/size, `Townfall-PS5-Shipping.self`, layout + vtable pins | profile does not match |
| A7 | `lineage/generated/probe_config.h:2-5` (used in `diagnostic.c`, `probe/probe.c:33`) | diagnostic tag `[PPSA33286]`, `/data/townfall_*` log and input paths | Stellar Blade diagnostic lines tagged with Townfall's title: mislabelled evidence |
| **A8** (new, review M1) | in-game self-test: `ueselftest/gen_selftest.py:737`, `ueselftest/main.lua:18`, deploy copy `morning-launch-20261012/upload/ue4ss/Mods/ueselftest/Scripts/main.lua:18` (same sha `318f68e0…`), staged copy `round5-staging/lua-alloc/staged/ueselftest/main.lua:18` | `ROOT = "/data/modbridge/roots/PPSA33286/ue4ss/Mods/ueselftest"`; `PFIND` opens `ROOT.."/progress.txt"` with `"w"` first (`gen_selftest.py:739-746`, the `"w"` open at :744) | With the path shim (installed per mod, `LuaMod.cpp:8122-8155`) the literal is re-anchored at the embedded `ue4ss` component to the *running* game's root (`LuaPathShim.cpp:258-282`; host check `evidence/selftest-root-test.log`), so the reviewer's "writes into Townfall's folder" holds only when the shim is off (then raw io gets the Townfall path). Either way it is a Townfall literal in code section 6.2 requires on Stellar Blade. **Patch 0002** replaces it with the logical, title-free `"ue4ss/Mods/ueselftest"` (same shim key, checked). |
| **A9** (new) | path shim writable area: `typescan-r4/candidate/src/LuaPathShim.cpp:476-483` (`derive_writable_root`), used :1685-1687; mirrored `host-lua-harness/path-shim.lua:134-139`; deploy tool `round5-staging/shim-fix/deploy-precreate-writable.py:58` | `/data/modbridge/roots/<title>` → `/data/modbridge/writable` (title dropped) | Every game shares one writable area: the self-test's `progress.txt` and every same-named mod's saved files land at the same `/data/modbridge/writable/<mod>/…` for Townfall and Stellar Blade. Harmless only if in-process writes there never persist (unknown: the shim comment says no console experiment has shown either way). **Proposed** (not staged as code: it is a runtime change for its own labelled build, plus a matching change in the precreate tool and the host mirror): return `prefix + "/writable/" + <title>`, i.e. `/data/modbridge/writable/PPSA33286`. Host check of the proposed rule in `evidence/selftest-root-test.log`. |

A2-A6 are the "image profile": legitimately per game, but today hand-curated Townfall
values compiled into the PRX. Section 6.2 allows only the automatic generator.

Observation for the self-test owner (Mac-only source reading, not fixed here): `PFIND`
opens each candidate with `"w"` at script load (`gen_selftest.py:744`), which empties
an existing `progress.txt` before `RUN` reads it (`PFR`, :748), so crash-resume may
never fire. Unknown whether that is intended.

### B. Should change, not a blocker

- `lineage/lifecycle/runtime_start_bridge.cpp:536-539`: failure-path `stat` of the
  Townfall root. Fix: stat the entry's derived root, or drop it.
- **Stale status file (review m3).** On a failed title derivation no
  `runtime-status.txt` is written (staged entry, `write_status_failed` returns when no
  root is known). The previous launch's file stays, and `harvest/harvest.py:129` reads
  the Townfall path. Rule for harvest (tool change, not done here): accept
  `runtime-status.txt` only when its `session <pid>` line (`modforge_activation.h:140-142`)
  matches the pid in this launch's `UE4SS.log` line (`[t=…ms pid=N] modbridge …`, written
  on success and on failure, appended); otherwise report "stale status, not this launch".

### C. Fine (comment, documentation, platform convention, host-only)

- Comments/examples in `modforge_activation.h`, `runtime_ack_supplier.hpp`, `LuaPathShim.*`,
  `LuaMod.cpp`, `LuaUObject.hpp`, `UObjectArray.cpp`, `UnrealInitializer.cpp`,
  `shadow_copy.hpp`, `SeedValidation.hpp` (lines in `evidence/game-constant-scan-B4.tsv`).
- PS5 conventions: `/app0`, `/app0/eboot.bin`, `/data/plugins/…prx`, `/data/plugins/UE4SS.log`.
- B3 `IterateGameDirectories` (`LuaMod.cpp:3355-3417`): generic rule, no names.
- Diagnostic Lua that is Townfall-only by purpose and never deployed on another game:
  `zzminimapdiag-v1/main.lua:41,44` (Townfall minimap classes), `zzdiag-v3/main.lua:2` (comment).
- Host harness/test files (`host-lua-harness/path-shim.lua:35` default root, `host-tests/*`,
  `logicmods/run-bpml.lua:41,48`): host-only.

### D. Unsure

- Whether A6 layout/vtable pins are engine-version facts Stellar Blade shares: unknown.
- Whether a per-game profile compiled into the PRX counts as "the same runtime": decision.

### E. Mac tools and launch data (not in the PRX; the blind test uses them) — review M1

`scan_tool_constants.py` class "tool": **21 tools** hard-code the title, the Townfall root
or Townfall names (line lists in `evidence/tool-constant-scan.txt`):
`harvest/` harvest.py (18 hits, e.g. :129 `RUNTIME_STATUS_REMOTE`), prep_modded_launch.py (13),
wakeup_prep.sh (12), lua_fast_deploy.sh (10), bind_launch.sh (4), pack_layout_tables.py (4),
prelaunch_rowcheck.py (4, :67 `REMOTE_ROOT`), fast_cycle.sh (2);
`mod-manager/modmanager.py` (11: :95 `remote_root`, :152 load-list `title` line);
`morning-launch-20261012/` stage_morning_launch.py (10), deploy_mirror.py (3), readonly_check.py (2);
`round5-staging/shim-fix/deploy-precreate-writable.py:57`; `ueselftest/gen_selftest.py:737` (A8);
`keymap/keymap-gen.py` (2); `pak-overlay/overlay_builder.py:93`; `phasea-tools/` engine_match.py (8),
pak_vtable_pins.py (2), symbolize_crash.py (2); `shader-fallback/classify_materials.py` (12);
`blade-prep/run-blade-profile.py` (5).
Plus 24 test/fixture files (e.g. `host-tests/run-path-shim.sh`, `host-tests/run-regression.sh`,
`mod-manager/test_modmanager.sh`), 34 launch-data files (Townfall load lists / ini written
per launch: records of Townfall launches, regenerated per game) and 20 records.
Every "tool" needs a `--title` parameter (or reads the title from the derived
`UE4SS.log` line) before the Stellar Blade run. Tool work, not runtime work.

## 3. Design: how the runtime learns the running title

### 3.1 Constraint (earlier-session observation, design input only)

During `module_start`, `/data` is readable but `/app0` is not (`stat_app0`=0, errno 2;
`evidence/l24-diag.jsonl:495-496`). `KERN_PROC_PATHNAME` → ENOENT and VMMAP paths are
empty (`runtime_start_bridge.cpp:324-325,385-388`). `/data` closes after `module_start`,
so the mod root must be chosen inside it.

### 3.2 Sources considered (revised, review M2)

| Source | Observes THIS process? | Readable in module_start? | Two local sources for every value? | Used? |
|---|---|---|---|---|
| `/data/plugins/ploader.ini` sections enabling this module | no (console config) | yes | path yes; grammar effectively one (see 3.5) | **source 1** |
| `/mnt/sandbox/<T>_<NNN>/app0/sce_sys/param.json` | no (mounted packages; path built from source 1's title) | expected; **console proof needed** | yes (table 3.6) | **source 2** |
| Process app info via `sceKernelGetAppInfo(getpid())` — what ploader matches on (`ploader main.c:766-770`) | **yes** | unknown (ploader calls it from another process) | **no** — see below | not yet: decision |
| same, through `sysctl` (an existing import) with an app-info `KERN_PROC` subtype | yes | unknown | **no: zero local sources for the mib number** | no |
| same, through `sceKernelDlsym` (an existing import) | yes | unknown | **no: zero local sources for the libkernel handle value** | no |
| `KERN_PROC_CWD` (42) / `KERN_PROC_ARGS` (7) via `sysctl` (SDK `sys/sysctl.h:889,913`) | yes | never tried (unknown whether they name the sandbox or title) | mib yes; meaning unknown | no; console diagnostic candidate (6.7) |
| `/app0/sce_sys/param.json` | yes | no (ENOENT at module_start, earlier session) | — | later re-check option (8.3) |
| Load list `title` in the derived root | no (our installer wrote it) | yes | — | `mfa_activate` folder consistency check only (review m2) |

**Process app info, evaluated (review M2).** It is the strongest source: it is the
exact value the loader injects on, and it belongs to this process. Local facts
(Mac-only):
- Function exists in the public homebrew SDK's libkernel stub
  (`2026-10-08-native-thread-audit/sdk-libkernel-symbols.stdout:532`) and ploader declares
  and calls it (`ploader main.c:119`, `:767`). It is **not** a PRX import today
  (`EXACT-IMPORT-IDENTITIES.json` has `sysctl`, `sysctlbyname`, `sceKernelDlsym`, `getpid`,
  no `sceKernelGetAppInfo`).
- Result layout: three local declarations. ploader `main.c:101-106` and SDK
  `samples/test_privileges/main.c:49-54` put `title_id` at byte offset 16; SDK
  `samples/ps/main.c:29-35` puts a `uint32_t app_type` at 16 and `title_id` at 20. The
  project spec `docs/superpowers/specs/generic-ps5-title-inventory-design.md:11,17` already
  flagged this conflict and said "return unavailable until the layout is resolved".
- The review's "sceKernelGetAppInfo is a thin wrapper over a KERN_PROC app-info sysctl"
  has **no local source** (SDK `sys/sysctl.h:880-914` lists no app-info subtype).
  Unknown; not used. No web access was used to look further.

So under the two-source rule none of the three call routes can ship from local
material today: the sysctl route needs a mib number (0 sources), the dlsym route needs
a handle value (0 sources), the direct route adds one import row and has a 2-vs-1
layout conflict. What would make it shippable (decision 7.1): (1) one diagnostic
console launch (your OK) that logs the raw first 0x60 bytes of the app-info result for
`getpid()` — this settles the layout from the console itself, as a second source next
to ploader/test_privileges; (2) then add `sceKernelGetAppInfo` as one new import in its
own labelled, gated build, and make it a **third must-agree source**: process title P
must be title-shaped, must be in E (source 1), and must be confirmed by its sandbox
`param.json` (source 2); any missing value or mismatch fails closed. A wrong layout
cannot pick a wrong game: the bytes at the wrong offset fail the 4-letter+5-digit
shape check, so it fails closed.

### 3.3 Rule in patch 0001 (unchanged logic)

1. **Source 1.** Read `/data/plugins/ploader.ini` with the loader's grammar (ploader
   `main.c:579-683`). Result: set E of section titles enabling **this** module path
   (mode 1 or 2).
2. **Source 2.** For every T in E and NNN = 000..099 read
   `/mnt/sandbox/<T>_<NNN>/app0/sce_sys/param.json` (no symlink follow, ≤ 64 KiB).
   T is confirmed when that file is strict JSON with exactly one root `"titleId"` = T.
3. **Decision.** Exactly one confirmed title → root `/data/modbridge/roots/<T>`. Then
   `mfa_activate` checks that this folder's load list names the same title — a folder
   consistency check, **not** a third source of which game is running (review m2).
4. Title shape 4 × A-Z + 5 × 0-9, so a built path cannot leave its parent folder.
5. **New in revision 2 (review m7):** before its first kernel call, `mbt_read_file`
   checks the Open/Pread/Close bindings are non-NULL (same volatile-pointer rule as
   `diagnostic.c:38-46`); unbound → code -102 "kernel open/pread/close unbound", nothing
   read, nothing activated.

**What "agree" really proves (review M2).** Source 2's path is built from source 1's
title, so code -107 fires only when a sandbox folder named for T holds a package whose
`param.json` names another title (or the file is corrupt). The link "this process is
that sandbox" is inferred from "one game runs at a time". A wrong pick would need two
configured games with live sandboxes at once; the patch then refuses (-109).

### 3.4 Sandbox numbering (review m4; earlier-session listing, design input only)

`2026-10-04-zcode-team/worker-boot-protocol/probe/ftp-sandbox-list.txt` shows system app
NPXS40093 at instances `_000`…`_044` created over ~26 h (Oct 05 00:12 → Oct 06 01:58,
lines 4-50): **numbers climb per launch and old folders stay**. The old ones are small
(link count 3, 64 B) while live ones are larger (`PPSA33286_000` 8 links/384 B, `_044`
14 links/768 B, lines 50-51), which suggests old instances have no mounted `app0`, so
they read as "absent" (not as a confirmation). Consequences: a game relaunched more
than 100 times between reboots would fail closed (-108); a stale but still-mounted
sandbox of another configured game would give -109 (blocks mods, safe). Townfall was
seen only at `_000`. Option (not used): `opendir`/`readdir` are PRX imports and could
list `/mnt/sandbox` instead of probing numbers, but libc in `module_start` is unproven.

### 3.5 Fail-closed codes (game runs unmodded; reason in UE4SS.log + diagnostic events)

| Code | Case |
|---|---|
| -101 | ploader.ini missing |
| -102 | ploader.ini oversized (>64 KiB), unreadable, has a NUL byte, **or a kernel binding is unbound** |
| -103 | no section enables this module |
| -104 | more than 16 titles enable it |
| -105 | a sandbox param.json is unreadable or >64 KiB |
| -106 | a sandbox param.json is not strict JSON / no or two root `titleId` / etc. |
| -107 | a sandbox param.json names a different title |
| -108 | no live sandbox confirms any configured title |
| -109 | two or more configured titles confirmed (ambiguous) |

**Grammar sources (review m9).** The ini grammar effectively has one source (ploader
`main.c`); the deployed ini (`evidence/ploader-ini-backup-20261008.ini`) only exercises
`[section]` and `path = 0|2`. For that subset both agree; comments, the no-slash prefix,
the `..` rule and long lines rest on `main.c` alone, and which ploader build is deployed
is unproven. A mismatch normally fails closed (-103/-108).

### 3.6 Values and their sources

| Value | Source 1 | Source 2 |
|---|---|---|
| ini path `/data/plugins/ploader.ini` | ploader `main.c:25` | deployed file at that path (`evidence/ploader-ini-backup-20261008.ini`, earlier session) |
| sandbox layout `/mnt/sandbox/<T>_<NNN>/app0` | `.superpowers/native-observations/townfall-ready-recon-2026-10-01/running-mount.json` (earlier session) | `ftp-sandbox-list.txt` (earlier session) — **console re-proof required** |
| `app0/sce_sys/param.json` | l23 harvest listing (`…/storage/files.json`) | public SDK sample + app dumper (cited in the title-metadata spec) |
| root `"titleId"` | Townfall's param.json (`townfall-ready-recon-2026-10-01/param.json`) | SDK sample; Stellar Blade local metadata (`2026-09-30-stellar-blade-content-discovery.md:7-11`) |
| title shape | ploader comment `[CUSA12345]` (`main.c:592`) | observed ids in the l23 harvest |
| `sceKernelPread` declaration | `runtime_start_bridge.cpp:901` | PRX import row in `EXACT-IMPORT-IDENTITIES.json` |
| 64 KiB, depth 16, 100 instances, 16 titles | policy bounds (fail closed beyond them) | — |

## 4. The staged patches (not applied)

**Never patch in place (review m5).** `outputs/2026-10-08-combined-outer-lifecycle/candidate-v2/`
is a frozen, sha-pinned input that every l24/l26 build restages (`package_combined.py:53`);
editing it would break every archived build's pin check. To ship: copy
`staged/lifecycle/runtime_module_entry.c` to a new labelled location and repoint
`outer_entry` there, as its own labelled, gated build (driver session).

- `0001-runtime-entry-generic-mod-root.patch` (sha `3ccf0364…d2283`), against the
  compiled file (sha `bc490893…`). Reproduces `staged/lifecycle/runtime_module_entry.c`
  (sha `721c4037…0530c`, 1,244 lines) byte for byte on a scratch copy.
  Changes since revision 1: binding check (m7), comment wording (m2).
- `0002-ueselftest-title-free-root.patch` (sha `a804068e…3c4e`), against
  `ueselftest/gen_selftest.py` and `ueselftest/main.lua`: one line each (`ROOT`), plus a
  one-line comment. Reproduces `staged/ueselftest/{gen_selftest.py,main.lua}` byte for
  byte; `luac -p` OK. The generator alone no longer reproduces the committed `main.lua`
  (v1.1 was edited after generation), so both files are patched. The deploy copy under
  `morning-launch-20261012/upload/` must be re-staged from the patched `main.lua` and
  mirror-deployed with a row-check (CLAUDE.md section 2); `round5-staging/lua-alloc/staged/ueselftest/main.lua`
  needs the same line. Without the shim, the relative ROOT resolves against the
  process working directory (unknown), fails, and `PFIND` falls back to the script
  folder or disables resume — safe.
- Imports: entry object gains exactly one undefined symbol, `sceKernelPread`, already a
  PRX import (`evidence/target-check.log`). No libc calls, no allocation; one 64 KiB
  static buffer; extra stack about 2 KiB plus up to 16 small JSON frames.

## 5. Mac tests (all Mac-only)

- `host-test/run_host_test.sh` (`evidence/host-test-run-3.log`): **35/35 fixture cases +
  17/17 JSON cases PASS**, and a second build with `sceKernelPread` unbound (NULL weak
  import): **1/1 PASS** — refuses with -102 before any read-open, nothing activated, no
  status file, reason in UE4SS.log.
- `host-test/run_mutants.sh` (`evidence/host-test-mutants-2.log`): **6/6 mutants caught**
  (new: removing the binding check crashes the unbound build, exit 139).
- `host-test/test_selftest_root.lua` (`evidence/selftest-root-test.log`, Lua 5.5.1, host
  shim mirror): 6/6 checks — old and new ROOT give the same shim key; new ROOT has no
  title; today's writable area is shared by two titles (finding A9); the proposed
  per-title rule separates them.
- `target_check.sh` (`evidence/target-check.log`): logs the full command, PATH prefix
  (LLVM 21.1.8), toolchain version and staged sha; `-fsyntax-only` with the packaging
  flags from `pass1-controller.log:7` exit 0; object compile exit 0; one new undefined
  symbol, `sceKernelPread`, found in `EXACT-IMPORT-IDENTITIES.json` (review m6).
- Memory guard in every compile script: skip under 2 GB reclaimable
  (free+inactive+speculative+purgeable) or under 15% `memory_pressure` free. At run
  time: ~7.1-7.4 GB reclaimable, 74% free; strictly "Pages free" was only 0.1-0.3 GB
  (macOS keeps free pages low and caches in inactive). One compile at a time.

## 6. What only the console can prove (each step: your OK, driver session only, game closed before any log read)

1. Read-only: while Townfall runs, FTP `LIST /mnt/sandbox/` and `RETR`
   `/mnt/sandbox/PPSA33286_000/app0/sce_sys/param.json` (expect 3,485 B, root `titleId`).
2. That the game process itself can read that file during `module_start` (only a launch shows it).
3. That `sceKernelPread` resolves from the entry at that moment.
4. Sandbox numbering for games and that closed games' sandboxes have no mounted `app0` (3.4).
5. Townfall acceptance with 0001: mods still load; UE4SS.log shows `modbridge title: PPSA33286`.
6. Self-test with 0002: `[ST2]` lines and `progress.txt` land where expected (shim on).
   Whether writes in `/data/modbridge/writable` persist at all (decides A9's urgency).
7. Optional diagnostic launch for M2: raw app-info bytes for `getpid()`, and
   `KERN_PROC_CWD` / `KERN_PROC_ARGS` results at `module_start`.
8. Stellar Blade: `[PPSA13197]` enabled in ploader.ini picks `/data/modbridge/roots/PPSA13197`
   — after the image profile (A2-A6) exists for it.

## 7. Decisions needed

1. Process app info as a third source (3.2): run the diagnostic launch, then accept one
   new import (`sceKernelGetAppInfo`) in its own build? Or ship 0001 with the two
   console-wide sources and the "one game at a time" assumption?
2. Image profile (A2-A6): compiled in per game, or loaded as data at run time?
3. A9 writable area per title: its own runtime build (with precreate tool + host mirror)?
4. A7 rename and 0002: fold into the 0001 build or separate (one fix per build)?
5. Accept the policy bounds (64 KiB, 100 instances, 16 titles, depth 16)?

## 8. Next steps

1. Driver session: console step 6.1 (read-only), with your OK.
2. Labelled build "mod-root-generic" = B4 + 0001 (repoint `outer_entry`), all gates,
   `verify_build --manifest`; one Townfall launch (6.2-6.6).
3. Later re-check option: once `/app0` is visible, read `/app0/sce_sys/param.json` on the
   init thread and stop before any mod starts if `titleId` differs.
4. Separate tracks: A2-A6 from the generator; A7; A9; harvest stale-status rule (B);
   `--title` parameter for the 21 tools (E).
