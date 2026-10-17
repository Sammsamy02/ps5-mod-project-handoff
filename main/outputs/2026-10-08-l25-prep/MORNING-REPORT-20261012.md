# Morning report — overnight run 2026-10-11/12

## In short

- **The final build is ready:** B4 `d044cf0b...` (all 4 fix builds stacked).
  Every gate passed on the Mac. Nothing was uploaded; no console contact tonight.
- **I found and fixed serious bugs in zcode's staged patches** before building:
  the lookup cache would have hidden real objects from every mod, and the shim
  could never read back a file it wrote. All fixes are host-tested on the Mac.
- **Likely cause of the two shim crashes found** (a Lua panic in shim setup);
  fixed in B4. Likely cause of the memory growth found too (Lua errors skip C++
  cleanup on PS5); the error wrapper is fixed in B2, the deeper fix needs your OK.
- **Several build checks were passing without checking anything.** Fixed the
  ones in the build path; the rest are listed.
- **The morning launch is staged** (`morning-launch-20261012/`): launch card,
  exact deploy list, and an offline row-check that passes 30 of 30 rows.
- **Stellar Blade:** no executable on the Mac yet; steps and 3 candidate mods are
  written. One blocker found: the runtime hard-codes Townfall's mod folder.
- **Shader milestone M1a done on the Mac:** Townfall's global shader cache decodes
  and rebuilds byte-identical.
- Launch plan: launch B4; if it crashes or a mod breaks, the next launch uses B3
  (everything except the shim) to tell the two apart.


Mac-only night: NO console contact of any kind (not even read-only FTP).
Everything below is **Mac-only** unless it cites the step-0 read-only FTP checks
done earlier in the evening, before the overnight instruction.

## Status at a glance

| Item | Status |
|---|---|
| 1. CLAUDE.md rules + goals | done |
| 2. fs-throw gate fixed, gates audited | done (see below) |
| 3. Builds B1-B4 + final stack | done: B1, B2b, B3, B4 all PASS (B4 = final stack) |
| 4. Self-test expectations vs upstream | done: 7 of 7 confirmed |
| 5. Morning launch staged (no upload) | done; offline row-check 30/30 |
| 6. Stellar Blade generic steps | written; no executable on the Mac |
| 7. Capability prep (#7, #12, Phase B) | 3 docs + #12 M1a byte-identical rebuild |
| 8. Disk cleanup list | done (nothing deleted) |

## What needs your OK in the morning

Console steps (each one separately, game closed, in this order — see
`morning-launch-20261012/LAUNCH-CARD.md`):
1. read-only console check (FTP reads only)
2. upload the 12 self-test files + new loadlist, with readback
3. delete stale self-test files, if the listing shows any
4. create the per-mod writable folders
5. PRX: back up the current one, upload the final build, decrypt-off readback,
   row-check all 30 rows, reload the plugin loader, start the kernel log
6. your one launch

Decisions for you / the reviewer (nothing was changed for these):
- **Compile Lua as C++** (upstream's `LUA_COMPILE_AS_CPP`) so Lua errors run
  C++ cleanup on PS5 — likely the memory-growth root cause. Own labelled build.
- **Convert the 49 throwing filesystem calls** to the safe form. Own build.
- **Make the outer entry find the mod root generically** (today it hard-codes
  Townfall's title id) — required before Stellar Blade.
- **Self-test GAPS**: whether refused hook registrations should count as FAIL
  instead of PASS (today: PASS + a separate GAPS line).
- Map the minimap's keys (F5/F6/Insert/Delete) to controller buttons? (config)

## Item 2 — fs-throw gate

- **The old gate checked nothing.** It looked for source files in a folder that
  doesn't exist, found zero, and printed PASS. So "fs-throw gate pass" on
  1e49188b proved nothing. Original kept as
  `harvest/fs_throw_gate.py.orig-20261012`.
- **Fixed gate** (`harvest/fs_throw_gate.py`): checks every file the build
  actually compiled, catches `fs::` aliases and calls split over several lines,
  prints how many files and calls it checked, and FAILS when it checked zero.
  Built-in self-test 23/23 PASS.
- **What it finds in 1e49188b** (exact compiled set, 3,558 files):
  66 filesystem calls, 17 safe (error-code form), **49 can throw** —
  35 in our candidate tree (mostly mod-folder scanning in UE4SSProgram.cpp
  and LuaMod.cpp), 14 in the shared port code. They only throw when a
  filesystem operation errors, which is likely why 1e49188b runs clean.
  Evidence: `evidence/fs-throw-gate-20261012/`.
- **Decision (conservative, reversible):** the 49 known calls are a tracked
  debt list (`harvest/fs_throw_baseline-1e49188b.txt`), printed as a count on
  every run. Any NEW or CHANGED throwing call fails the build. Converting the 49
  to the safe form is proposed as its own labelled build (after B4).

## Other gates (audit, item 2)

A read-only audit found the same "passes without checking" bug in 13 more
gates in the build/deploy path (plus ~40 smaller scripts). Fixed tonight, each
with a backup `*.orig-20261012`:
- **Packaging driver**: wrote a PASS receipt even if its own checks failed
  (P5-P8); now refuses to write the receipt. It also printed PASS before
  checking for failures. (1e49188b itself was fine: its P5-P8 really passed.)
- **Build driver**: printed PASS before checking failed gates; a selection with
  zero pins passed. Both fixed.
- **verify_build.sh**: a marker with zero entries passed. Fixed.
- **snapshot_tree.sh**: an empty source tree still snapshotted. Fixed.
- **prebuilt_object_census.py**: same wrong-folder bug as the fs gate; now
  checks 22,755 pins (was 0) and its bridge check finds the real object.
- **deploy-precreate-writable.py** (morning deploy): always exited 0; now exits 1
  on any failed step or zero mods.
Still to fix (not used in the build or the morning deploy): lua_fast_deploy.sh
(replaced for the morning by the new `deploy_mirror.py`), the host regression
runner (recorded GREEN for a failed suite), harvest.py, and ~40 latent scripts.
Listed in `evidence/builds-20261012/GATE-AUDIT.md`.
- **no_detour_gate.py scope gap:** it checks only our 45 candidate files, not
  the ~3,500 other compiled files (which include the PS5-adapted PolyHook
  detour library). Its PASS covers ~1-2% of compiled code. Not fixed tonight.

## Builds (item 3) — every sha in BUILD-LEDGER-20261012.md

| Build | What | PRX sha256 | Result |
|---|---|---|---|
| B1 | lookup speed, no cache: walk clamp, StaticFindObject name-first, per-walk memo (**with a fix**, below) | `b9cc5248...aa909` | all gates PASS |
| B2 (1st try) | error wrapper + memory logging | — | refused by the include-graph gate (a new `<atomic>` include); the gate was right |
| B2b | same, with the counters rewritten so no new header is needed | `74fab85a...8c58b2` | all gates PASS; both new B2 markers found in the PRX (and correctly missing from B1's) |
| B3 | ForEachUObject stack fix (+ callback return ignored, as upstream), IterateGameDirectories PS5 package layout, UEnum.ForEachName logging | `40b8972b...cfb9175` | all gates PASS; B2 + B3 markers found |
| **B4 = final stack** | shim write-area fix + mod-folder setup + my try_original fix, on top of B1-B3 | **`d044cf0b...c37599`** | all gates PASS; all 24 markers found; read-only copy in `2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/` |

Every build: fs-throw gate before and after compile (0 new throwing calls),
no-detour gate, 73-75 build gates, packaging P0-P13 (twins byte-identical),
verify_build --manifest. Full shas, snapshots and the corrected safe bisect
points are in `BUILD-LEDGER-20261012.md`. Lookup timings and the memory numbers
can only be measured at the launch (self-test SLOW lines, `[MEM]` lines).

**Bug found and fixed in zcode's staged lookup patch (B1):** the memo cached
"is this object valid?" per CLASS, but the check depends on each OBJECT's flags
(a class's default object is excluded, its real instances are not). The default
object is usually seen first, so FindFirstOf("PlayerController") and friends
would have returned nothing — every mod would have broken. Fixed (flags checked
per object, class part cached). Host test: staged version wrong in 1,705 of
3,200 random trials; fixed version 0 (`host-tests/b1-memo/`).

## Corrections I made to zcode's staged patches (all Mac-only, host-tested)

| Patch | Problem | What I did |
|---|---|---|
| lookup 03 (memo) | cached a per-object check per class -> FindFirstOf would miss real objects | cache only the class part; table moved off the stack |
| lookup 04 + cache setting | caches without a create listener miss new objects | left out (reviewer's rule: caches only if still >10 ms) |
| IS 0001 (memory logging) | new `<atomic>` include refused by the build's include gate; forced GC step; warn handler ignored `@on` | builtin counters; read-only GC count; Lua's own warn code copied verbatim; extra pool OFF at compile time |
| group-b 01 (ForEachUObject) | callback returning `true` stopped the walk (PC doesn't); result discarded at a fixed slot | return value ignored like upstream; stack restored to its saved height |
| group-b 02 (IterateGameDirectories) | looked for `/app0/Content`; Townfall's content is in `/app0/<project>/content` | new generic rule: the one child folder with a Content folder is "Game" (case-insensitive); otherwise nil as upstream |
| group-b 03 (UEnum log) | log budget allowed 4 lines, said 3 | fixed |
| shim fix (IQ) | second try of an open always ran `io.open(nil, nil)` | each try gets its own copy of the arguments |

## Problems found (Mac-only)

1. **Lua errors skip C++ cleanup on PS5.** This port builds Lua as C, so a Lua
   error is a `longjmp`. On Windows (PC) that still runs C++ destructors; on
   PS5 it does not. Every error raised from a C++ binding leaks the strings/
   vectors in the frames it jumps over, and a held lock would never be
   released. Leading candidate for "memory grows each self-test run" (the
   self-test triggers errors on purpose) and possibly the stuck spawner menu.
   Upstream UE4SS has an option to compile Lua as C++ (`LUA_COMPILE_AS_CPP`),
   which makes errors unwind properly. **Proposed as its own labelled build;
   needs your/the reviewer's OK** (it changes how every Lua error unwinds).
2. **The error wrapper (fixed in B2):** it raised the Lua error from inside the
   C++ catch block, so the caught exception was never freed. Host test with the
   real Lua C code: old wrapper leaked 1,000 of 1,000 exceptions, fixed wrapper
   0. Fixed wrapper also keeps the real text and records it for the "not enough
   memory" note.
   Reach: every Lua-callable binding goes through this wrapper
   (`process_lua_function` -> `TRY`), so the fix covers any C++ exception
   thrown by any binding.
3. **Likely real cause of both shim crashes (d971cd2f, 1c48239d):** the old shim
   wrapped os.remove with a relative stack index; by the time it stored the
   wrapper, that index pointed at the wrapper itself, so Lua raised "attempt to
   index a function value". Shim setup runs outside any protected Lua call, so
   that is a Lua PANIC -> abort, printed only to stderr — which is why UE4SS.log
   just stops at the "[SHIM] write-area" line. Reproduced on the Mac with the
   real Lua library (`host-tests/b4-shim/`). IQ's fixed shim already uses an
   absolute index, so B4 removes it. Console confirmation: a klog capture of
   stderr at the launch would show the PANIC line if this were still present.
4. **Second shim bug (fixed in B4, Mac-tested):** the shim tries a file in two
   places (mod folder, then writable folder; or a write, then a retry after
   creating the folder). The first try used up the Lua call's arguments, so
   the second try always ran `io.open(nil, nil)` and failed. Result: a file a
   mod saved could never be read back from disk, and the self-test's
   write-then-read check would fail. Fixed so each try gets its own copy of
   the arguments. Host test: old code fails the second location, fixed code
   opens it (`host-tests/b4-shim/try_original_host_test.cpp`).
5. **Overlay design (#7) uses a cook-specific file name** (`pakchunk0-ps5.utoc`)
   — that would be per-game, not per-engine; the target container must be
   found at runtime. Noted in `capability-prep-20261012/CAP7-...md`.

## Disk (item 8) — candidates only, NOTHING deleted

94 GB free; each build adds ~2 GB, so there is no pressure tonight. If you want
space back later, these are old build outputs (none are in `evidence/`, none is a
frozen or bisect build). About **53 GB** in total:

| What | Size | Why it is safe to drop |
|---|---|---|
| `2026-10-08-l26-package-B.*` (8 dirs) | ~6.4 GB | variant B = "delete listener skipped", marked unsafe, never shipped |
| `l26-build-variants/l26-B.*` (8 dirs) | ~7.2 GB | same variant B, build side |
| `l26-build-variants/archive-r5/` failed attempts (`l26-A.r5b`..`r5O`, `staging.*`) | most of 22 GB | failed/retried round-5 attempts; keep `pkg.*` + the named crash builds |
| `l26-build-variants/archive-r3-tmp`, `archive-20261009T151128Z` | ~6.3 GB | round-3 / early-l26 scratch |
| `2026-10-08-l22..l25-*` build/package dirs | ~10 GB | superseded runtime generations |

**Keep:** `2026-10-08-l26-frozen/1e49188b-acceptance-pass1/` (read-only copy I made),
`archive-p1` (59b0528b round-4 freeze), `archive-20261010T025518` (efbfc656 menu-up),
`archive-wf`, `archive-p2` (variant-A bisect points), all of `archive-r6/` (tonight),
and everything in `evidence/`.

## Self-test (item 4)

- **All 7 changed expectations checked against the vendored upstream UE4SS
  source, all confirmed, all kept:** PropertyTypes.ObjectProperty is a table
  with a numeric Size (call 8); `__OriginalReturnValue` is nil at rest (20);
  FName.Equals returns an undefined value upstream too (28, test now checks
  only that the call works); the four thread-id functions return a ThreadId
  userdata with ToString (58-61).
- Added IS's memory lines (`MEM before` / `MEM after`, using the new
  GetLuaMemory from B2) and "arm once" for the 8 callback families that
  can't be unregistered.
- **Found a hidden gap:** 12 entries count a REFUSED registration as PASS
  (BeginPlay, EndPlay, InitGameState, ProcessConsoleExec,
  CallFunctionByNameWithArguments, ULocalPlayerExec — pre and post). On PC
  these hooks register; on PS5 the port refuses them. I kept PASS/FAIL as is
  (comparable with earlier runs) but added a `GAPS n ...` line after the
  summary that names them. These hooks matter to many mods.
- Syntax checked with luac (Mac has Lua 5.5; console runs 5.4, so this is a
  syntax check only).

## Morning launch staging (item 5) — `morning-launch-20261012/`

- `LAUNCH-CARD.md` — the steps and buttons for you.
- `loadlist.txt` (30 rows, 1,772 B) = today's list + new self-test sizes +
  the minimap's 2 rows. `rowcheck-pins.json` = exact size + sha256 for every
  row. Every unchanged file was checked against what step 0 read on the
  console (all match).
- `upload/` = the only files to upload: 12 self-test files + the loadlist.
  The minimap is already on the console and matches its original download.
- `deploy_mirror.py` — uploads them with readback, lists stale files, deletes
  them only with a separate OK. Exits 1 on any failure.
- The writable-folder script now exits 1 on failure (it always returned 0).
- **Confirmed in the admission code** (modforge_activation.c `join_path`):
  admission only opens `<mod root>/loadlist.txt` and the listed rows, so
  `/data/modbridge/writable` is outside it — nothing to exempt.

## Stellar Blade (item 6) — `STELLAR-BLADE-STEPS-20261012.md`

- No readable copy of its executable is on the Mac, so the generator was not
  run. The file lists the generic steps and 3 candidate mods to download.
- **Blocker found for the blind test:** the runtime's outer entry hard-codes
  Townfall's mod root (`/data/modbridge/roots/PPSA33286` in
  `runtime_module_entry.c`). That is a game-specific value in runtime code; on
  any other game it points at the wrong folder. It needs a generic lookup of
  the running title before Stellar Blade (proposed, not changed tonight).

## Capability prep (item 7) — `capability-prep-20261012/` (Mac-only, docs)

- `CAP7-NO-FILE-CHANGE-OVERLAY.md`: the Mac-side overlay builder is tested
  (32/32); the earlier console trial replaced a game file, so no
  no-file-change mechanism is proven yet. Next Mac step: make the container
  choice generic. First console step (needs your OK): an observe-only launch.
- `CAP12-M1-FEASIBILITY.md`: Townfall's **global shader cache is already on the
  Mac** (`GlobalShaderCache-SP_PS5.bin`, 4.5 MB, extracted from the game's
  own pak), so M1 can start Mac-only on that file. The material shader
  library still needs a read-only console fetch.
- `PHASEB-DOC-STATUS.md`: what is designed, what is blocked, and that every
  kernel step needs your explicit OK; 3 passes across restarts apply here.
- **#12 M1a (done, Mac-only):** `cap12-m1a-20261012/` — Townfall's own
  `GlobalShaderCache-SP_PS5.bin` (4.5 MB) decodes into 492 shader maps / 14,645
  shaders and re-encodes **byte-identical** (sha `ed52f157...`; I re-ran it
  independently). Correction to the plan: this file holds no shader code, only
  hashes into the shared code library, which needs a read-only console fetch
  (your OK) for M1b.
