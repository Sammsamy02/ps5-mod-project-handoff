# REPORT — worker-bridge-r12-final-review

Worker: worker-bridge-r12-final-review · 2026-10-04 · read-only review of the FINAL
composition-6 bridge; proposed fixes as diffs only; no console contact; no nested
subagents. Workspace: `outputs/2026-10-04-zcode-team/worker-bridge-r12-final-review/`.

## What was reviewed

`outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp`
sha256 `feed52544023360326c17f7015336f90c4906789b344a805cfb6debb67bdd7a9` (702 lines),
cross-checked against: worker-entry-review (r3-era P-findings), Resolution B design
§2.7 (worker-configured-roots-design), D2 design (worker-d2-textbase-design), the seed
table incl. the corrected tick slot (worker-seed-table-derivation/SEED-TABLE.json),
the ODR single-vintage audit (worker-odr-header-safety/ODR-SAFETY.md), the production
test (worker-module-start-wiring/host_test_r8), the comp6 driver + rehearsal + gate
artifacts (worker-comp6-driver), the pinned SDK header
(`ps5-payload-sdk/target/include/sys/user.h:466-488`), and the retained image
(`townfall-static-executable-2026-10-02/decrypted-eboot.bin`, sha `3b6cfad4…`).

## VERDICT

**GO-WITH-FIXES** — full evidence per checklist item in REVIEW-VERDICT.md.
No P0/P1; nothing blocks composition-6. One P2 (carried-over activation-leak
contract hole, unreachable from the shipped entry) and five P3 hardening/hygiene
items, each with an exact diff.

## Evidence produced by this review (all local, read-only)

1. **Anchor table independently re-verified against the retained image** (python
   byte extraction, seed-table translation rule): B `0x7826782`, C `0x75cffda`,
   C2 `0x76c8e58`, D `0x7b8845d`, E `0x7dd2a40`, A `0xbc5f250` all byte-MATCH;
   B/D/E occur exactly once image-wide; C occurs exactly twice, at exactly the two
   table addresses — the either-site B&&D&&E&&(C||C2) gate rests on correct
   production data. `kImageExtentVaddr 0xBC602C8` = phdr8 `0x9633420+0x262cea8`
   (independently hex-added).
2. **SDK layout verified**: kve_start@8, kve_end@16, kve_protection@56, kve_path@136,
   KINFO_VMENTRY_SIZE 1160 — every bridge walk offset correct (sys/user.h:466-488).
3. **Import-set comparison (nm)**: compiled r11 and r12 from the production sources
   against the refreshed packaging overlay (stale rehearsal overlay + the three
   Resolution-B headers + ODR-gated Platform header from canonical paths). Result:
   r12 undefined set is a strict SUBSET of r11 (59→57; the two removed symbols are
   libc++ weak-instantiation codegen noise from the const→non-const `converted`
   change); r12 adds ZERO new symbols. My r12 object reproduces the controller's
   `/tmp/r12-preflight/r12.o` symbol set 1:1 modulo macOS host-toolchain artifacts.
4. **Compile-environment hazard reproduced (fail-closed)**: the on-disk comp6
   REHEARSAL overlay is OLD vintage — r12 refuses to compile against it (missing
   `allow_configured_roots_for_missing_executable_path` / `configured_game_directory`)
   while r11 compiles clean. The real driver restages the Resolution-B headers
   unpinned (`build_runtime_composition6.py:119-120,144`), after which r12 compiles
   clean (verified). Recorded as residue item 2; ODR FINDING-1 already owns the
   one-vintage rule.
5. **Tick cross-check**: zero references to any tick constant
   (`0x33ac8c0`/`0x2fac8c0`/`0x37AC8C0`) in the bridge — nothing carries the wrong
   tick.
6. **Tool tag cross-check**: post_launch_check `INTERESTING_OBSERVER_TAGS`,
   rung_check, and console_batch CP classifiers consume only ENTRY-emitted tags,
   all unchanged; every r12-expected tag (`bridge_exepath_source`,
   `bridge_lease_reg`, `stat_app0/ownmod/rootdir(+_ec)`, `bridge_cfg_gamedir`,
   `bridge_cfg_workdir`) is emitted with the exact design-specified spelling.
7. **Memory-safety sweep**: 19,005,440-byte zero-fill bss vmmap + two 384 KiB
   function-local `mappings[]` statics + `g_image_regions[128]`, all under the
   entry_guard-serialized single thread; all three kve walk loops bounds-consistent;
   `total > capacity` and truncated `entries+1` cases cannot over-index.

## Entry-review P-finding status (r3-era → r12)

| Finding | Status in r12 |
|---|---|
| P1-1 gate W2 split-TU | gate/integration side; comp6 driver stages entry+bridge as packaging lifecycle units; out of bridge scope, unchanged |
| P2-1 status O_TRUNC window | entry-side; out of bridge scope |
| P2-2 activation leak on early returns | **STILL PRESENT** (bridge:526-543) — P2-1 of this review, diff provided |
| P2-3 bridge compile evidence | **RESOLVED for r12** (/tmp/r12-preflight/r12.o + my independent compile reproduce its symbol set) |
| P3-1 probe diagnostic drift | accepted/documented (entry-side) |
| P3-2 STOP_EXN==CONTEXT==2 | still present — P3-4 of this review, diff provided |
| P3-3 start() return semantics | unchanged, accepted design (ack file is truth) |
| P3-4 unused ue4ss_backend.hpp include | still present (bridge:36), harmless |
| P3-5 ops stack-local borrow | unchanged; VmScan route retains no loader ref; record-only |

## Findings

- **P2-1** activation adopted after the early returns vs header ownership contract
  (carry-over; unreachable from shipped entry) — diff in REVIEW-VERDICT.md.
- **P3-1** lease 128-cap saturation silent — count readable spans + `bridge_lease_reg_spans` diff.
- **P3-2** `kve_sizeof_` constant vs `sizeof(kinfo_vmentry)` duplication — static_assert diff.
- **P3-3** `char probe[8]` uninitialized before base-op probe — zero-init diff.
- **P3-4** MRS_ERR_STOP_EXN renumber (carry-over) — header diff.
- **P3-5** host_test_r8 debris: r9-era `source=3` comment block, vacuous
  `bridge_kve_prefix` check, no assertions for the r12 delta, capture files stale
  (ledger receipt 12/12 SECTIONS_1_4 @ 2026-10-05T01:25Z is the governing evidence;
  section 5 lease validation honestly PENDING) — test diff provided.
- Residue (no diff needed): stale rehearsal overlay cannot compile r12 (fail-closed);
  ledger test text still says "compiles the ACTUAL runtime_start_bridge_r8.cpp" while
  the harness includes r12; comp6 `entry-preflight.json` references entry r3 while the
  driver default is r4 (integration note); `kConfiguredGameDirectory` could be
  `const char *const`.

## Honest limits

- No console contact; every runtime-behavior claim cites in-repo evidence (ledger
  mirror, launch records, compiled objects) or my own host compile/byte extraction.
- The r12 attach path (`bridge_cfg_*`, opt-in firing) has no host-level functional
  test; my evidence is compile-level (symbols, headers, vintage) — first live
  exercise remains the next in-game launch, with `bridge_exepath_source=0` +
  `bridge_cfg_gamedir/workdir=1` + `bridge_lease_reg` as the predicted observables
  (matches Resolution design §3.3(5) prediction shape).
- Review of the entry, gate scripts, and composition mechanics is limited to what
  the bridge boundary touches.

## Files

- `CLAIM.json` (first), `REVIEW-VERDICT.md` (verdict + per-item evidence + exact diffs), `REPORT.md` (this file, last).
- Scratch (outside the deliverable dir): `/tmp/br12-review-compile/` — compiled r11/r12 objects, nm dumps, refreshed overlay copy. No reviewed source was modified.
