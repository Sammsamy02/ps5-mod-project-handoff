# Adversarial review: fallback-freeze (2026-10-12, Mac-only)

**Verdict: not refuted.** I found no wrong claim and no rule break. Two small polish points are listed below.
Every check here is Mac-only. I made no console contact, ran no PRX build and changed nothing in git.

## What I re-checked myself

- **Frozen copies.**
  - `shasum -a 256` of each frozen PRX gives exactly the task values: 40b8972b..., 74fab85a... and b9cc5248....
  - Each value matches its row in BUILD-LEDGER-20261012.md (lines 5, 7 and 8).
  - Each snapshot tar passes `shasum -c`, and its sha prefix matches the ledger's snapshot id (73e75292, 4fe7345b, 39e11f3e).
  - Modes are 555 on the dirs and 444 on all 21 files. Each dir has the same 7 files as the B4 frozen dir.
- **Copy, not move.** The PRX (pass-1 and pass-2), the receipt and the symbols file in each frozen dir are byte-identical (`cmp`) to the files still present in archive-r6/<label>/2026-10-08-l26-package-A/.
- **verify_build re-run** with the worker's manifests:
  - B3: 23/23, B2b: 21/21, B1: 19/19.
  - All three: RESULT MATCH, magic 4f153d1d, VERDICT PASS, exit 0.
- **Marker manifests.**
  - B3 and B1 are byte-identical to the markers-declared.json inside their own snapshot tars.
  - B2b differs from its snapshot copy, as disclosed (the snapshot predates the B2 markers).
  - Every marker id in all three manifests appears in that build's original evidence verify log. The marker counts in those logs are 23, 21 (verify_build-with-B2-markers.txt) and 19.
- **Shim switch-off claim.** I extracted the snapshots into my scratchpad:
  - The B3 UE4SSProgram.cpp native branch sets `"ue4ss/Mods"`. B4 reads `configured_working_directory`.
  - The LuaMod.cpp root block uses two lexical parent_path steps.
  - LuaPathShim.cpp:1529-1533 returns on an empty root, before any io/os wrapping. Its sha starts 821d74a2 in B1, B2b and B3; B4's is different.
  - The host test recompiled and ran: 3/3 PASS.
  - check_shim_state.py re-run: 73/73 PASS.
- **Build differences.**
  - B1's LuaMod.cpp is byte-identical to 1e49188b's (bfc392c5).
  - B1 to B2b adds only two blocks.
  - B3 to B4 differs only in LuaPathShim.cpp, UE4SSProgram.cpp, the build stamp, CHANGES.json and the marker file.
- **Self-test expectations.** The cited main.lua lines (79-80, 105-106, 111-112, 119), the ONE() FAIL paths (41, 43) and part11/part9 match what FALLBACKS.md says. hygiene-audit cannot FAIL either: S.hyd starts empty, and entries are only ever set to true.
- **Dry run.** prep_modded_launch.py:1374-1375 returns from dry_run() before any live code. The PRX gate (596-627) checks only exists/magic/sha, so the "fresh verify_build is required" warning is correct.
- **Other script checks.**
  - freeze_fallbacks.py --check: 66/66.
  - Every worker script fails on zero checks.
  - The harvest/ file names and the shas of prep_modded_launch.py, verify_build.sh and markers-declared.json are unchanged.
- **Console IP.** It appears only in the dryrun-*.log files, which the worker disclosed. It does not appear in any .md, .json, .py or .txt file in the dir.

## Small points (polish only)

1. The claim "harvest/ unchanged afterwards" rests on a before-listing only, with no saved after-file. I confirmed it now: names and input shas match. The harvest/ folder's own timestamp is 07:44, after the worker's last run at 07:41. Something added and removed a file there; the driver is the likely cause, but I cannot tell who.
2. The open condition ("activation code outside candidate/ unknown") can be narrowed. B1's and B3's EXECUTION-RECEIPT.json share 146 sha-valued inputs, and only the build stamp and its gate entry differ. That makes the native-mode assumption more likely, though still not console-proven.
