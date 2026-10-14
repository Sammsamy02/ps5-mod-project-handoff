# Fallback builds, frozen and ready (2026-10-12, Mac-only, revision 2)

**In short:** if B4 crashes or breaks a mod, the next launch can use one of
these three builds without rebuilding anything. Each is now a locked,
read-only copy. Each matches the task value, the build ledger, the build
evidence and the archived files. Each passed `verify_build.sh` with the marker
list it was built and checked with. Each passed a `prep_modded_launch.py
--dry-run`, which made no network or write attempts.

**The main difference from B4 (corrected in revision 2):** on all three
fallbacks the file-path helper (the "path shim") **switches itself off at
startup**, just as it did on the known-good 1e49188b. So mods read and write
files directly, as on 1e49188b. None of B4's shim fixes or its writable folders
come into play. The log shows one `[SHIM] path-resolver disabled
reason=no-root` line per mod. This is expected, not a regression.
Revision 1 of this file said the opposite (a read-back bug and a failing
self-test check). That was wrong and is fixed below.

Nothing here touched the console. Every claim below is **Mac-only** unless it
explicitly cites an earlier session's report. Those citations explain
expectations; they are not verdicts for a new launch.

## Order to step back

- **B4 -> B3** comes from the launch plan (`MORNING-REPORT-20261012.md:21-22`):
  "launch B4; if it crashes or a mod breaks, the next launch uses B3
  (everything except the shim) to tell the two apart".
- **B3 -> B2b -> B1 -> 1e49188b** is **my proposal**, not part of that plan.
  It steps back one labelled build at a time, ending at the known-good
  1e49188b. That build is already frozen in
  `2026-10-08-l26-frozen/1e49188b-acceptance-pass1/` and was left untouched.
  The user or reviewer decides the next step after each launch.

## The three frozen fallbacks

| Step back to | Label | PRX sha256 (full) | Bytes | Snapshot | Build id | verify_build (own markers) |
|---|---|---|---|---|---|---|
| 1st | B3-iterators | `40b8972b58e313c61b84e2665d8c166887b49712753b000ff4f0cf919cfb9175` | 31,591,399 | 73e75292 | l26-20261012T035151Z | **PASS**, 23/23 markers, receipt MATCH |
| 2nd (proposal) | B2b-error-wrapper-memlog | `74fab85a5165c17a4d169f8efdb25e0e647fa7b867ed7843a7b4869a5f8c58b2` | 31,583,703 | 4fe7345b | l26-20261012T034011Z | **PASS**, 21/21 markers, receipt MATCH |
| 3rd (proposal) | B1-lookup-nocache | `b9cc52485c51e5ebbaf1944e01f472f65d19136e28d3a0ef965756b1936aa909` | 31,565,351 | 39e11f3e | l26-20261012T032343Z | **PASS**, 19/19 markers, receipt MATCH |

Frozen paths. Each dir is mode 555 and holds the same 7 files as the B4 frozen
dir, each file mode 444:

- `outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/`
- `outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/`
- `outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/`

The 7 files are `modbridge_supplier_composition.prx`, `.prx.sha256`,
`.prx.symbols.json`, `FINAL-RECEIPT-VARIANT-A.json`, and
`snapshot-<ts>.manifest.json` / `.sha256` / `.tar.gz`. All three are
variant A (`"unsafe": false` in each receipt), so they are valid bisect points
under CLAUDE.md section 4.

## How each copy was checked (Mac-only)

- **Before copying** (`preflight-20261012.log`, re-run as
  `preflight-r2-20261012.log`: 96 of 96 PASS each time), for each build:
  - The PRX sha matched in 6 places: the task value, the
    `BUILD-LEDGER-20261012.md` row, the evidence `RESULT` file, the archived
    pass-1 PRX, the archived pass-2 twin, and the receipt `prx` and `twin`
    blocks.
  - The magic was `4f153d1d`. The size matched both RESULT and the receipt.
  - The receipt was byte-identical to the evidence copy.
  - The symbols file matched the receipt's `symbols_sidecar` sha.
  - The snapshot tar sha matched its `.sha256` file, `snapshot.stderr`, the
    snapshot manifest and the ledger's snapshot id.
  - The build id matched the archived CHANGE-SELECTION build stamp.
- **Copy** (`freeze-20261012.log`, 183 of 183 PASS). Every file was copied
  (never moved), re-hashed and found identical to its source, then made
  read-only.
- **Re-check** (`check-final-20261012.log`, then `check-final-r2-20261012.log`:
  66 of 66 PASS each time). `shasum -a 256 -c` reports OK for every PRX and
  snapshot tar (`shasum-check-frozen.log`).
- **Protected dirs untouched.** The B4 and 1e49188b frozen dirs were
  fingerprinted (mode, size, mtime and sha256 of every file) before, after,
  and again in revision 2. All 34 lines are identical each time
  (`protected-frozen-fingerprint-before.txt`, `-after.txt`, `-r2.txt`).

## Which marker list each build was verified with

Today's `harvest/markers-declared.json` has 24 markers, including later
builds' markers, so each fallback needs its own older list. I rebuilt each list
in this dir (`reconstruct-manifests-r2-20261012.log`, 36 of 36 PASS) and
compared it with four sources:

1. **S1:** `harvest/markers-declared.json.pre-B2markers-20261012` (19
   markers), plus the B2 and B3 sets defined in
   `harvest/add_build_markers.py:11-23`, appended the way its `main()` does
   (`:41-44`). I read the script with `ast` and never ran it.
2. **S2:** today's harvest file, cut to its first N markers.
3. **S3:** the copy of `markers-declared.json` inside each build's own source
   snapshot tar.
4. **S4:** the marker ids and entry texts printed in each build's original
   evidence verify log.

**Independence (corrected in revision 2).** S1 and S2 are *not* independent:
`add_build_markers.py` wrote today's file (S2) from the same base and sets that
S1 uses, so they agree by construction. The truly independent sources are S3
and S4:

| Build | Manifest in this dir | Markers | sha256 | Independent sources that agree |
|---|---|---|---|---|
| B3 | `markers-B3-iterators-23.json` | 23 | `6223d303...` | S3: byte-identical. S4: ids and texts match `evidence/builds-20261012/B3-iterators/verify_build.txt` |
| B2b | `markers-B2b-error-wrapper-memlog-21.json` | 21 | `8539f0a0...` | Base 19: S3 and S4. **The 2 B2 markers: S4 only** (`.../B2b-error-wrapper-memlog/verify_build-with-B2-markers.txt`), because B2b's snapshot was taken before they were added |
| B1 | `markers-B1-lookup-nocache-19.json` | 19 | `f3f57f74...` | S3: byte-identical. S4 matches `.../B1-lookup-nocache/verify_build.txt` |

Full hashes and these notes are in `MANIFESTS-PROVENANCE.json`.
`harvest/markers-declared.json` was only read; its sha256 is unchanged
(`harvest-markers-sha-before.txt`, `harvest-inputs-sha-r2-before.txt`).

## What each fallback lacks compared with B4

Evidence comes from three places:

- **Source:** snapshot manifest diffs.
- **Change notes:** `2026-10-08-l26-runtime/typescan-r4/CHANGES.json:943-978`.
- **Binary:** a negative-control `verify_build.sh` run with the full 24-marker
  list. It is *expected* to FAIL, and it misses exactly the later builds'
  markers (`negative-control-24markers-*.log`).

| Build | Lacks vs B4 | Later markers absent from its PRX |
|---|---|---|
| **B3** | Only the B4 shim work: the IP shim root (UE4SSProgram.cpp gives the shim a real root), and the IQ shim fixes (absolute os-table index, lazy one-shot mkdir, spill fallback, try_original argument copies). Files: `LuaPathShim.cpp`, `UE4SSProgram.cpp` (CHANGES.json:972-978). | `b4-shim-write-area-dir` (1 of 24) |
| **B2b** | Everything B3 lacks, **plus** the B3 changes: the ForEachUObject stack fix, IterateGameDirectories support for the PS5 package layout, and the `[UEN]` lines for UEnum.ForEachName (logging only). Files: `LuaMod.cpp`, `LuaType/LuaUEnum.cpp` (CHANGES.json:965-971). | `b3-foreachuobject`, `b3-uenum-foreachname`, `b4-shim-write-area-dir` (3) |
| **B1** | Everything B2b lacks, **plus** the B2 changes: the error-wrapper fix (the old wrapper leaked the caught C++ exception on every Lua error from a binding, `MORNING-REPORT:156-163`), and the Lua memory counters (`[MEM]`, `[lua-mem]`, `GetLuaMemory`). Files: `LuaMadeSimple.hpp/.cpp`, `LuaMod.cpp` (CHANGES.json:954-964). B1 keeps only the lookup-speed change (CHANGES.json:943-953). | `b2-mem-line`, `b2-lua-mem-note`, `b3-foreachuobject`, `b3-uenum-foreachname`, `b4-shim-write-area-dir` (5) |

## How the shim behaves on the fallbacks (revision 2, Mac-only)

**The path shim is switched off on B3, B2b and B1, exactly as on 1e49188b.**
`check_shim_state.py` checks this in each build's own snapshot
(`check-shim-state-20261012.log`, 73 of 73 PASS). The chain:

1. **Relative mods folder.** `UE4SSProgram.cpp:927-962`
   (`setup_mod_directory_path`, the same lines in all four builds) sets the
   native mods folder to the relative text `"ue4ss/Mods"` at `:932`, inside the
   native-mode branch at `:930`. None of these builds reads
   `configured_working_directory`, which is where B4 gets the real root
   (B4 `UE4SSProgram.cpp:948`).
2. **Empty root.** The shim root is derived by text operations on that
   folder. The block is `LuaMod.cpp:7953-7986` (B1 and 1e49188b), `:8005-8038`
   (B2b) and `:8122-8155` (B3), identical in all five builds including B4. It
   strips `Mods`, then `ue4ss`, and is left with an **empty** root. A host test
   with the real `std::filesystem` (Apple clang 17) confirms it:
   `"ue4ss/Mods"` gives `""`, while `"/<root>/ue4ss/Mods"` gives `"/<root>"`
   (`shim_root_lexical_host_test.cpp`, `shim-root-lexical-host-test.log`, 3
   of 3 PASS).
3. **The shim switches itself off.** With an empty root,
   `LuaPathShim.cpp:1529-1533` (sha `821d74a2`, identical in 1e49188b, B1,
   B2b and B3) prints `[SHIM] path-resolver disabled reason=no-root ...` and
   returns. That return comes before the io wrapping at `:1578-1579`, before the
   os wrapping at `:1592-1593`, and before the writable folder is worked out at
   `:1540-1542`. The only other way to set a root is a test-only override. It
   has no caller in any snapshot (`LuaPathShim.hpp:105` says it is for tests;
   the only caller found is `host-tests/run-regression.sh`).

**Second source:** earlier sessions' reports describe this same switch-off.
- `IP-SHIM-ROOT-FIX.md:15-33` cites 7 `reason=no-root` lines in the launch-2
  log.
- `IQ-SHIM-AUDIT.md:124-126` says that "in the clean build 1e49188b the shim
  self-disabled at the no-root guard".
- `CLAUDE-LAUNCH2-VERDICT.md:25-28` says mods "run raw-io".
- B4's own comment (B4 `UE4SSProgram.cpp:945-947`) says that an empty root
  keeps the relative path "so the shim still disables itself loudly
  (reason=no-root)".

**What this means:**

- **B4's shim fixes do not matter on the fallbacks.** The read-back bug fixed
  in B4 (`MORNING-REPORT:173-180`) lives in code that never runs when the shim
  is off. Revision 1 was wrong to say the fallbacks keep that bug or that the
  self-test's write-then-read check would fail.
- **Mods use plain file access**, as on 1e49188b.
- **B4's per-mod writable folders** (`/data/modbridge/writable/<mod>/`, made
  by the B4 deploy step) stay on the console but are **never used** by these
  builds. Only the shim works out that path, and it stops before doing so.
- **There is a latent crash path; never feed these builds a root.** The
  fallback shim still contains the relative-index os wrap at
  `LuaPathShim.cpp:1592-1593`. `MORNING-REPORT:164-172` names it as the likely
  PANIC cause of both shim crashes (1c48239d, d971cd2f). It is safe here only
  because the no-root return at `:1533` comes first. So never combine the IP
  shim-root change with B1-B3 without IQ's shim fix: that combination is what
  crashed twice.
- **This depends on one condition.** The switch-off happens because the
  runtime starts in native mode (`UE4SSProgram.cpp:930`), as it did on
  1e49188b. The activation code outside `candidate/` is not in the snapshots,
  so whether it is identical across these builds is **unknown** from here. If
  a fallback launch shows `[SHIM] write-area ...` lines instead of
  `reason=no-root`, that is unexpected: stop and report it.

## What to expect in the log on a fallback launch (Mac-only expectations, not console-proven)

**All three fallbacks:**

- One `[SHIM] path-resolver disabled reason=no-root (no title-id default
  exists; derive from the mods directory)` line per Lua mod. This is the
  same as on 1e49188b, not a regression.
- No `[SHIM] write-area ...` lines.
- Self-test boot: probably `[ST2] boot v1.1 M=168 armed=true pf=false
  one-run=true` and `[ST2] progress-io unavailable (resume disabled)`
  (`main.lua:111-112`, `:119`). This is *likely, not certain*. It depends on
  whether the game can create a new file with plain file access in the
  self-test folder. Earlier shim-less sessions printed exactly these lines
  (`CLAUDE-ROUND8-RECEIPT.md:23`, `agent-IJ-round8-receipt.md:38, 48`, with
  the older v1 self-test).
- Self-test `io-roundtrip` (`part11.lua:8`) **cannot FAIL**. It has no
  `return false`, and `ONE()` marks FAIL only on a Lua error or a `false`
  return (`main.lua:41, 43`). Expect `SKIP no-io-path` when progress-io is
  unavailable, otherwise a PASS with a detail (`rw roundtrip ...` or
  `w-only ...`). Crash-resume is off when `pf=false`.

**B3 only:** the same as B4 apart from the shim. Expect up to 3
`[UEN] ForEachName: expected ...` lines, and `[FOU]` lines only if an object
cannot be built during ForEachUObject.

**B2b (also applies to B1):** these builds lack the B3 changes, and the staged
self-test calls exactly those functions:

- `ForEachUObject` (`part9.lua:12`): **expected FAIL `iterate-err`**. B1's
  ForEachUObject code is byte-identical to 1e49188b's: the whole `LuaMod.cpp`
  is the same, sha `bfc392c5`. B2b differs from B1 only in two B2 blocks, not
  in ForEachUObject (B2b `LuaMod.cpp:2918`, B1 `:2896`). On 1e49188b this
  entry failed with `iterate-err` (`CLAUDE-SELFTEST-RESULTS.md:46`, an earlier
  session). The cause is the pre-B3 stack leak (B3 snapshot
  `LuaMod.cpp:2919-2936`).
- `IterateGameDirectories` (`part9.lua:13`): the log shows
  `IterateGameDirectories: Could not locate the root directory because the
  directory structure is unknown (not <RootGamePath>/Game/Binaries/Win64)`
  (B2b `LuaMod.cpp:3301-3304`, B1 `:3279-3282`) and the call returns nil. The
  staged entry only reports the type, so expect **`PASS ret=nil`**. That PASS
  means the call returned, not that it found the game folders. B3 and B4 should
  print `ret=table` if the PS5 layout rule matches. With the older self-test,
  the 1e49188b run listed this entry as failing
  (`CLAUDE-SELFTEST-RESULTS.md:47`), so the exact line is not certain.
- `UEnum.ForEachName` (`part7.lua:8`): B3 added **logging only**
  (CHANGES.json:968), so the result should match what B3 or B4 print for this
  entry, just without `[UEN]` lines. B3's source note says the 2026-10-11
  console run iterated `n=0` names (B3 snapshot `LuaUEnum.cpp:84-85`), which
  this entry reports as PASS `n=0 v0=nil`.
  `CLAUDE-SELFTEST-RESULTS.md:45` lists it as `iterate-err`, so the exact line
  is uncertain on every build.
- B2b still prints the `MEM before ...` and `MEM after ...` numbers and the
  `[MEM]` lines.

**B1 only:**

- `[ST2] MEM before unavailable` and `[ST2] MEM after unavailable`. B1 has no
  `GetLuaMemory`, and the self-test calls it inside `pcall`
  (`main.lua:79-80, 105-106`).
- No `[MEM]` or `[lua-mem]` lines.
- The old error wrapper leaks on every Lua error from a binding, so memory
  growth during the self-test is expected. The self-test triggers errors on
  purpose. On the Mac the old wrapper leaked 1,000 of 1,000 caught errors
  (`MORNING-REPORT:156-163`).

## Before a live fallback launch (each console action needs its own explicit OK)

**Required local steps (Mac-only, no console contact):**

1. **Run a fresh verify_build PASS with the manifest gate for that exact sha.
   This is required, not optional** (CLAUDE.md section 2: no PRX upload without
   it). The live prep script does **not** run it. Its PRX gate,
   `step2_prx_gate` (`prep_modded_launch.py:596-627`), checks only that the
   file exists, its magic and its sha, and the live path (`:1377-1431`) never
   calls verify_build. Commands:

   ```
   bash /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/verify_build.sh /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/modbridge_supplier_composition.prx --manifest /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/round6-prep-20261012/fallback-freeze/markers-B3-iterators-23.json /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b
   bash /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/verify_build.sh /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/modbridge_supplier_composition.prx --manifest /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/round6-prep-20261012/fallback-freeze/markers-B2b-error-wrapper-memlog-21.json /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a
   bash /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/verify_build.sh /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/modbridge_supplier_composition.prx --manifest /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/round6-prep-20261012/fallback-freeze/markers-B1-lookup-nocache-19.json /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248
   ```

   Expected: `VERDICT: PASS`, `SUMMARY: declared=N pass=N miss=0`, and
   `RESULT: MATCH`. Also run `python3 freeze_fallbacks.py --check` (expected
   66 of 66 PASS).
2. **Archive `evidence/runtime/l25-variant-A/` first (copy, never move).** The
   live run reuses fixed file names there (`prep_modded_launch.py:519, 641,
   680`) and would overwrite B4's readback and backup files. The driver did
   the same before B4
   (`l25-variant-A-archive-before-B4-20261012T115530Z/`).
3. **Ask the user to confirm the game is closed** before any console step.

**The live line** is the dry-run command below without `--dry-run`. In a single
run it does several **console actions, and the user's OK has to name each of
them** (CLAUDE.md section 1: approval for one action does not carry over):

| Step | What it does on the console | Kind |
|---|---|---|
| 1 | reads the process list (game must not be running) | read-only |
| 3 | decrypt-off backup readback of the resident PRX, **PRX upload**, decrypt-off readback (full sha + magic `4f153d1d`), one verified restore on mismatch | upload |
| 4 | **uploads `UE4SS-settings.ini`** (pinned 1947-byte copy), readback + compare | config edit |
| 5 | **uploads `ploader.ini`** (pinned enabled state), readback + compare | config edit |
| 5.5 | loadlist row-check (every row exists at its exact size) | read-only, required before launch |
| 6 | **arms ploader** | arming |
| 7 | starts the klog and eboot monitors in the background | read-only |

After that comes **the one game launch**, a user action with its own OK. The
live run also writes a `LAUNCH-READY-A-<ts>.md` (or `LAUNCH-PREP-FAILED-...`)
into `harvest/` (`prep_modded_launch.py:1432-1440`). Its printed user steps
still describe the older l24 sequence (`:378-404`), so follow the current
launch card for the in-game steps.

## Exact dry-run commands (run; output saved here)

Before running them, I read the dry-run code:

- `dry_run()` is `prep_modded_launch.py:1106-1309` (file sha `f1ef5049...`,
  unchanged during my runs). It builds the `curl` commands only as text
  (`:299-324`), and its only `open()` is a read (`:1220`).
- `main()` returns from `dry_run()` before any live code runs
  (`:1374-1375`).

**Revision 2 guards.** Each dry run ran inside
`sandbox-exec -p '(version 1)(allow default)(deny network*)(deny file-write*)'`
and under `audit_dryrun_wrapper.py`. The wrapper is a Python audit hook that
records every write-open, socket, process start or file change *attempt*
before it happens.

- **Positive control first** (`sandbox-audit-positive-control.log`). A probe
  script deliberately tried one file write and one loopback connect
  (127.0.0.1:9, so nothing left the Mac). The hook recorded both attempts, and
  the sandbox refused both with `PermissionError [Errno 1] Operation not
  permitted`. So both guards really work.
- **Each dry run:** exit 0, 141 audit events seen, and **0 write, network,
  process or file-change attempts**. STEP 2 (file exists, magic, sha) passed
  for the frozen PRX, and both local ini checks passed
  (`run-verify-dryrun-r2-20261012.log`: 63 of 63 PASS).
- **Correction:** revision 1's "the sandbox denied nothing" rested on a grep
  of the script's own output. macOS sandbox denials do not show up there, so
  the audit line above replaces that check.

**B3 (first fallback)** -> output `dryrun-B3-iterators.log`

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/modbridge_supplier_composition.prx --dry-run
```

**B2b (second fallback, proposal)** -> output `dryrun-B2b-error-wrapper-memlog.log`

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/modbridge_supplier_composition.prx --dry-run
```

**B1 (third fallback, proposal)** -> output `dryrun-B1-lookup-nocache.log`

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/modbridge_supplier_composition.prx --dry-run
```

## For the driver: build-ledger lines to add (I may not edit the ledger)

`BUILD-LEDGER-20261012.md:5, 7, 8` still say "`outputs/2026-10-08-l26-package-A/`
(moves to `archive-r6/...` when the next build starts)". Suggested additions,
in the same style as the 1e49188b row (`:26`):

```
| B1-lookup-nocache | ... | ... | ... | ... | archive-r6/B1-lookup-nocache/ + read-only copy in 2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/ (frozen 2026-10-12, fallback-freeze) |
| B2b-error-wrapper-memlog | ... | archive-r6/B2b-error-wrapper-memlog/ + read-only copy in 2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/ |
| B3-iterators | ... | archive-r6/B3-iterators/ + read-only copy in 2026-10-08-l26-frozen/B3-iterators-40b8972b/ |
```

## Watch-outs

- **The dry-run logs contain the console's LAN address**, because the script
  prints it. Do not paste them into reports for others (CLAUDE.md section 1).
  This file and README.md do not contain it.
- **Archive before a fallback launch** (step 2 above), or B4's readback and
  backup files in `evidence/runtime/l25-variant-A/` get overwritten. The klog
  and eboot-monitor logs are timestamped and are not overwritten.
- **The frozen dirs are protected only by mode 444/555**, with no immutable
  flag, the same as the B4 frozen dir. Re-run `python3 freeze_fallbacks.py
  --check` before use.
- **The revision 1 logs** are kept unchanged in `r1-logs/` (copies). The
  top-level `verify-*.log`, `negative-control-*.log` and `dryrun-*.log` files
  are the revision 2 runs.
