# Fallback builds, frozen and ready (2026-10-12, Mac-only)

**In short:** if B4 crashes or breaks a mod, the next launch can use one of
these without rebuilding anything. Each fallback is now a read-only copy,
checked against the task value, the build ledger, the build evidence and the
archived files. Each one passed `verify_build.sh` with the exact marker list
it was originally built and checked with. Each also passed a
`prep_modded_launch.py --dry-run`, which I ran inside a macOS sandbox that
blocks all network access and all file writes.

Nothing here touched the console. Every claim below is **Mac-only**.

Order to step back: **B4 -> B3 -> B2b -> B1 -> 1e49188b** (the known-good
build, already frozen in `2026-10-08-l26-frozen/1e49188b-acceptance-pass1/`
and left untouched). This follows the launch plan in
`MORNING-REPORT-20261012.md:21-22`: "launch B4; if it crashes or a mod breaks,
the next launch uses B3 (everything except the shim) to tell the two apart".

## The three frozen fallbacks

| Step back to | Label | PRX sha256 (full) | Bytes | Snapshot | Build id | verify_build (own markers) |
|---|---|---|---|---|---|---|
| 1st | B3-iterators | `40b8972b58e313c61b84e2665d8c166887b49712753b000ff4f0cf919cfb9175` | 31,591,399 | 73e75292 | l26-20261012T035151Z | **PASS**, 23/23 markers, receipt MATCH |
| 2nd | B2b-error-wrapper-memlog | `74fab85a5165c17a4d169f8efdb25e0e647fa7b867ed7843a7b4869a5f8c58b2` | 31,583,703 | 4fe7345b | l26-20261012T034011Z | **PASS**, 21/21 markers, receipt MATCH |
| 3rd | B1-lookup-nocache | `b9cc52485c51e5ebbaf1944e01f472f65d19136e28d3a0ef965756b1936aa909` | 31,565,351 | 39e11f3e | l26-20261012T032343Z | **PASS**, 19/19 markers, receipt MATCH |

Frozen paths (each dir is mode 555 and holds the same 7 files as the B4 frozen
dir, each mode 444):

- `outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/`
- `outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/`
- `outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/`

The 7 files are: `modbridge_supplier_composition.prx`, `.prx.sha256`,
`.prx.symbols.json`, `FINAL-RECEIPT-VARIANT-A.json`, and
`snapshot-<ts>.manifest.json` / `.sha256` / `.tar.gz`.

All three are variant A ("unsafe": false in each receipt), so they are valid
bisect points under CLAUDE.md section 4.

## How each copy was checked (Mac-only)

- **Before copying** (`preflight-20261012.log`, 96 of 96 checks PASS), for each
  build:
  - The PRX sha matched in 6 places: the task value; the
    `BUILD-LEDGER-20261012.md` row; the evidence `RESULT` file; the archived
    pass-1 PRX; the archived pass-2 twin PRX; and the receipt `prx` and `twin`
    blocks.
  - Magic was `4f153d1d` and the size matched RESULT and the receipt.
  - The receipt was byte-identical to the evidence copy.
  - The symbols file matched the receipt's `symbols_sidecar` sha.
  - The snapshot tar sha matched its `.sha256` file, `snapshot.stderr`, the
    snapshot manifest and the ledger's snapshot id.
  - The build id matched the archived CHANGE-SELECTION build stamp.
- **Copy** (`freeze-20261012.log`, 183 of 183 checks PASS): every file was
  copied (never moved), re-hashed and found identical to its source, then
  made read-only.
- **System tool cross-check** (`shasum-check-frozen.log`):
  `shasum -a 256 -c` reports OK for the PRX and the snapshot tar in all three
  dirs.
- **Protected dirs untouched:** B4 and 1e49188b were fingerprinted (mode,
  size, mtime, sha256 of every file) before and after. There is no difference
  (`protected-frozen-fingerprint-before.txt` / `-after.txt`).

## Which marker list each build was verified with

Today's `harvest/markers-declared.json` has 24 markers, including markers from
later builds, so each fallback needs its own older list. I rebuilt each list
in this dir (`reconstruct-manifests-20261012.log`, 36 of 36 checks PASS). Four
independent sources agree for each one:

1. `harvest/markers-declared.json.pre-B2markers-20261012` (19 markers), plus
   the B2 and B3 marker sets that `harvest/add_build_markers.py:11-23`
   defines. I read these with `ast` and did not run the script. I appended
   them the way its `main()` does (`add_build_markers.py:41-44`).
2. Today's harvest manifest, cut to its first N markers.
3. The copy of `markers-declared.json` inside each build's own source
   snapshot tar.
4. The marker ids and entry texts printed in each build's original evidence
   verify log.

| Build | Manifest in this dir | Markers | sha256 | Matches |
|---|---|---|---|---|
| B3 | `markers-B3-iterators-23.json` | 23 | `6223d303...` | byte-identical to the copy in B3's own snapshot; ids and texts match `evidence/builds-20261012/B3-iterators/verify_build.txt` |
| B2b | `markers-B2b-error-wrapper-memlog-21.json` | 21 | `8539f0a0...` | sources 1, 2 and 4 agree (`.../B2b-error-wrapper-memlog/verify_build-with-B2-markers.txt`). B2b's snapshot holds only the 19-marker base, because it was taken before the 2 B2 markers were added. |
| B1 | `markers-B1-lookup-nocache-19.json` | 19 | `f3f57f74...` | byte-identical to the pre-B2 backup and to the copy in B1's own snapshot; matches `.../B1-lookup-nocache/verify_build.txt` |

Full hashes are in `MANIFESTS-PROVENANCE.json`. `harvest/markers-declared.json`
was only read: its sha256 was the same before and after
(`harvest-markers-sha-before.txt`).

## What each fallback lacks compared with B4

There are three kinds of evidence:

- **Source:** I compared each build's snapshot manifest with B4's, file by
  file.
- **Change notes:** `2026-10-08-l26-runtime/typescan-r4/CHANGES.json:943-978`
  and `MORNING-REPORT-20261012.md:113-117`.
- **Binary:** a negative-control run of `verify_build.sh` with the full
  24-marker list. It is *expected* to FAIL, and the markers it reports
  missing are exactly the later builds' markers (`negative-control-24markers-*.log`).

| Build | Lacks vs B4 | B4-or-later markers absent from its PRX |
|---|---|---|
| **B3** | Only the B4 shim work: the shim write-area fix (absolute os-table index, lazy one-shot mkdir, spill fallback), the try_original fix (each retry gets its own copy of the arguments), and the shim root / mod-folder setup. Files: `LuaPathShim.cpp`, `UE4SSProgram.cpp` (CHANGES.json:972-978). | `b4-shim-write-area-dir` (1 of 24) |
| **B2b** | Everything B3 lacks, **plus** the B3 changes: the ForEachUObject stack fix (stack restored each iteration; callback return value ignored as upstream), IterateGameDirectories support for the PS5 package layout, and the UEnum.ForEachName `[UEN]` logging. Files: `LuaMod.cpp`, `LuaType/LuaUEnum.cpp` (CHANGES.json:965-971). | `b3-foreachuobject`, `b3-uenum-foreachname`, `b4-shim-write-area-dir` (3) |
| **B1** | Everything B2b lacks, **plus** the B2 changes: the error-wrapper fix (the old wrapper leaked the caught C++ exception on every Lua error from a binding; `MORNING-REPORT:156-163`), and the Lua memory counters (the `[MEM]` line, the `[lua-mem]` out-of-memory note, `GetLuaMemory`). Files: `LuaMadeSimple.hpp/.cpp`, `LuaMod.cpp` (CHANGES.json:954-964). B1 keeps only the lookup-speed change (CHANGES.json:943-953). | `b2-mem-line`, `b2-lua-mem-note`, `b3-foreachuobject`, `b3-uenum-foreachname`, `b4-shim-write-area-dir` (5) |

What this means at a launch (Mac-only reading, not console-proven):

- **The shim in all three fallbacks is the same as in the known-good
  1e49188b.** `LuaPathShim.cpp` (sha `821d74a2...`) and `UE4SSProgram.cpp`
  (sha `3cc011d7...`) are identical in the 1e49188b, B1, B2b and B3 snapshot
  manifests. Only B4 changes them.
  - Both earlier shim crashes were builds that added the "shim root" patch:
    `1c48239d` and `d971cd2f` (`HANDOFF-20261012.md:17-18`). None of the
    fallbacks has that patch, so they stay on the known-good side of the shim
    question.
  - The cost: the read-back bug is still there (`MORNING-REPORT:173-180`). A
    file a mod saved in the second location cannot be read back, so the
    self-test's write-then-read check is expected to fail, as it would on
    1e49188b.
- **B1 has no `GetLuaMemory`.** The staged self-test calls it inside `pcall`
  (`morning-launch-20261012/upload/ue4ss/Mods/ueselftest/Scripts/main.lua:77-80, 105-106`),
  so on B1 its memory lines print "unavailable" and nothing breaks. B1 also has
  the old error wrapper, so memory growth during the self-test is expected
  there.
- **B2b and B1** also lack B3's IterateGameDirectories PS5 layout support.
  They behave as 1e49188b does. A mod that depends on that call may differ;
  which mods do is unknown.

## Exact dry-run commands (run from any directory; paths are absolute)

Each command below was run, with output saved in this dir. It prints the plan
and runs only local read-only checks.

I first read the dry-run code:

- `dry_run()` is `prep_modded_launch.py:1106-1309`. It calls only
  path/hash/print helpers. The `curl` commands are built as text
  (`:299-324`) and never run. The only `open()` is a read (`:1220`).
- `main()` returns from `dry_run()` before any live code runs (`:1374-1375`).

Then I ran each command under
`sandbox-exec -p '(version 1)(allow default)(deny network*)(deny file-write*)'`,
so network access and file writes were impossible, not just unused. Every run
exited 0 with STEP 2 (file exists, magic, sha) PASS for the frozen PRX, and
both local ini checks PASS (`run-verify-dryrun-20261012.log`, 51 of 51 checks).

**B3 (first fallback)** -> output `dryrun-B3-iterators.log`

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/modbridge_supplier_composition.prx --dry-run
```

**B2b (second fallback)** -> output `dryrun-B2b-error-wrapper-memlog.log`

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B2b-error-wrapper-memlog-74fab85a/modbridge_supplier_composition.prx --dry-run
```

**B1 (third fallback)** -> output `dryrun-B1-lookup-nocache.log`

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B1-lookup-nocache-b9cc5248/modbridge_supplier_composition.prx --dry-run
```

The live command is the same line without `--dry-run`. It uploads and arms on
the console, so it needs the user's explicit OK for that action, with the game
closed. CLAUDE.md section 2 also requires these steps before the launch: the
decrypt-off readback (full sha + magic `4f153d1d`) and the loadlist row-check.

To re-verify a fallback just before using it (local, read-only, about 1 s):

```
bash /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/verify_build.sh /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b/modbridge_supplier_composition.prx --manifest /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/round6-prep-20261012/fallback-freeze/markers-B3-iterators-23.json /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-frozen/B3-iterators-40b8972b
```

For B2b or B1, swap in its frozen dir and its own manifest (21 or 19
markers). Or re-check all three frozen dirs at once with
`python3 freeze_fallbacks.py --check`.

## Watch-outs

- **The dry-run logs contain the console's LAN address** because the script
  prints it. Do not paste them into reports meant for others (CLAUDE.md
  section 1). This file and README.md do not contain it.
- **A live run reuses fixed file names** in
  `evidence/runtime/l25-variant-A/`, for example
  `backup-resident.decrypt-off.prx`, `prx-readback.decrypt-off.prx` and
  `processes-precond.json` (`prep_modded_launch.py:519, 641, 680`). A fallback
  launch right after B4 overwrites the B4 launch's copies of those files.
  The driver session already archived that folder before B4
  (`evidence/runtime/l25-variant-A-archive-before-B4-20261012T115530Z/`). Do
  the same before a fallback launch. The klog and eboot-monitor logs are
  timestamped and are not overwritten.
- **The B4 deploy steps** (per-mod writable folders, new self-test files) stay
  on the console for a fallback launch. Whether the writable folders change
  anything for B1-B3 is unknown. Their shim code equals 1e49188b's, which ran
  before those folders existed.
