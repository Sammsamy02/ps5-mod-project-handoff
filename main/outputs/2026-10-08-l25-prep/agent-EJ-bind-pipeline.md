# Agent EJ — post-bind launch pipeline (pre-staged, 2026-10-09)

Deliverable: `harvest/bind_launch.sh` (executable, bash, zero manual steps after
agent EI's report lands). Nothing else written; trees read-only; no console
contact; no driver --execute run (pre-flight is the only executed part, and it
is read-only). Work files under /tmp/bind-pipeline/ only.

## 0. The one-command contract

```
outputs/2026-10-08-l25-prep/harvest/bind_launch.sh --all [--preflight-only] [--skip-upload] [--console-ip IP]
outputs/2026-10-08-l25-prep/harvest/bind_launch.sh --minimal [same flags]
outputs/2026-10-08-l25-prep/harvest/bind_launch.sh --watch   # acceptance-ladder live watcher
```

Steps, each with its own abort gate + exit code (0 ok / 2 usage / 3
WAITING-FOR-EI / 4 pre-flight or plan gate / 5 build / 6 packaging / 7 prep):

- **STEP 0 pre-flight** (read-only): EI-report gate → G2 mirror of every
  `files[]` row (profile/typescan-r4/thread) vs disk → DA/EB/EI content
  markers in UObjectGlobals.cpp → mode pin-set (below). Emits
  `STATUS: WAITING-FOR-EI` + live-tree context and exits 3 while
  `agent-EI-tostring-bind.md` is absent.
- **STEP 0b**: `build_variants.py --plan` + `l26_packaging_driver.py --plan`
  must pass BEFORE anything is archived (catches Q-patch composition/order
  regressions from EI's UnrealInitializer edits while the old build still
  exists).
- **STEP 1 archive**: moves `l26-A`, `l26-B`, `staging-copyin-probe`,
  `l26-package-A`, `l26-package-B` into `l26-build-variants/archive-<ts>/`
  (NEXT-BUILD step 2's list; drivers refuse non-fresh dirs).
- **STEP 2 build**: `build_variants.py --execute` from the `what/` cwd; gates
  on rc + twin/pair receipts.
- **STEP 3 package**: `l26_packaging_driver.py --execute`; gates on
  FINAL-RECEIPT-VARIANT-A, PRX presence, signed-SELF magic (4f153d1d/5414f5ee),
  and a NEW guard: refuses if the new PRX equals the already-tested 5ea2c845
  (a byte-identical rebuild means the staged changes never reached the
  artifact — never re-launch the same build).
- **STEP 4 upload**: CALLS `prep_modded_launch.py --variant A --prx-path …`
  (decrypt-off backup/readback, PRX upload, ORIGINAL cache-off settings
  restore — its default — ploader arm, klog tap, LAUNCH-READY receipt). Per
  Claude 2026-10-09 there is NO settings override afterwards;
  `wakeup_prep.sh` (cache-on) is deliberately NOT invoked. Plus one read-only
  INFO check that zzdiag v8 main.lua is the 1488 B auto-prober ('db=v' watch
  lines depend on it). On prep abort its own LAUNCH-PREP-FAILED receipt
  stands; the script says reconcile, never blind-replay.
- **STEP 5 printout**: user sequence (one launch → 90 s menu wait → same save
  → 30 s → L3+R3 once → CLEAN EXIT; crash ⇒ harvest + reboot) + watch
  commands + harvest command (`harvest.py --label l26-bind-<mode>-1`).

## 1. Task 1 findings — built vs staged, and the stale UObjectGlobals row

Ground truth re-derived, not taken from prose:

- `build_variants.py:120` consumes **`d['files']`** rows of the three part
  CHANGES.json files (NOT the `changes[]` array — that array is
  provenance-only for the drivers). Gate G2 compares each `files[]` row's
  `candidate_sha256`/`original_sha256` against disk and hard-fails at
  `--execute`.
- **BUILT (the tested 5ea2c845 PRX-A state, still in
  l26-package-A/pass-1, sha 5ea2c845… re-verified today):** runtime sources =
  UObjectArray.cpp `ea7221e8…`, UnrealInitializer.cpp `cac95e48…`,
  UObjectArray.hpp `69a2391a…`, outer_bridge `f510c014…` (ungated), DA
  verified-query in UObjectGlobals.cpp.
- **STAGED-ON-DISK but never built:** EE's variant C (UObjectArray.cpp
  `cbe50d15…`, UObjectArray.hpp `51e44f9a…`, UnrealInitializer.cpp Phase-6
  `4b831a7a…`, new `EngineListenerOpsProfile.hpp` `39cbe0e8…`, files[] rows
  for these REFRESHED by EE and matching disk); Z15 gated bridge
  `94f81a17…` (copyin-probe MANIFEST pin updated); EC's StaticFindObject
  fix (`SplitLongNameIntoPathParts` in UObjectGlobals.cpp; EC refreshed
  `changes[29]` to `83e20b38…` but NOT `files[29]`); EB's enriched
  `[FFO] … num:{} ts:'{}'` marker — already applied inside the in-flight
  UObjectGlobals.cpp (verified by grep; EB's diff draft is therefore
  landed-in-tree, not just a diff on disk); EE's repointed drivers
  (build `549b54dd…`, packaging `68146c3f…`, both re-verified on disk).
- **THE STALE ROW:** `typescan-r4/CHANGES.json files[29]`
  (UObjectGlobals.cpp) still declares `4abf743b…` = DA's verified-query
  state. Disk has since moved 83e20b38 (EC) → `dcefad5f…` (EI, in flight —
  the hash changed twice DURING this staging). This is exactly the plan-false
  EE flagged; it is the ONE row that will hard-fail driver G2 at --execute
  until its owner refreshes it.
- **Script handling (task-1 requirement):** the pre-flight's G2 mirror
  reports every stale row with owner attribution, plus a targeted diagnostic
  naming the EE-flagged UObjectGlobals row explicitly (old DA value cited).
  The tree edit itself is left to EI/parent per my instructions — the script
  never writes the trees. NOTE for EI: refresh BOTH `files[29]` and
  `changes[29]` (and the files[]+changes[] rows for its NameTypes.cpp/.hpp +
  UnrealInitializer.cpp edits — those four rows were live-drifting while I
  staged: files[10], files[26], files[27], files[29]).

## 2. Modes

- `--all`: builds the tree as staged (EI bind + EB marker + EC
  StaticFindObject + variant C + Z15 gating). Pin-set verified: both EE
  driver hashes, CTOKEN present, EngineListenerOpsProfile.hpp pinned,
  MANIFEST bridge = 94f81a17 gated candidate. Ready the moment EI lands +
  refreshes rows.
- `--minimal`: EI bind + EB marker on top of the 5ea2c845 state only.
  Pre-flight enforces the pivot BEFORE running: CM-era build driver
  (`e1d6ca0c…`, read from l26-build-variants/EXECUTION-RECEIPT.json — **no
  archived copy exists anywhere on disk**, verified twice: manual find + the
  script's own live search), variant-C files/rows reverted to
  ea7221e8/69a2391a (+ UnrealInitializer variant-C CONTENT absent — EI's own
  bind edits on top of cac95e48 are wanted, so this is a content check, not a
  hash check), EngineListenerOpsProfile.hpp withdrawn, EC's fix absent,
  copyin-probe MANIFEST bridge pin back at ungated f510c014. Every unmet pin
  prints as a [FAIL] with the exact owner + action; nothing auto-pivots.

## 3. Dry run (task 3) — actual output, current tree, both modes identical:

```
$ ./bind_launch.sh --all --preflight-only        (same for --minimal)
[14:50:34Z] STEP 0: pre-flight (--all)
STATUS: WAITING-FOR-EI
  EI report not present yet: …/2026-10-08-l25-prep/agent-EI-tostring-bind.md
  Nothing else runs. Re-run this script (same mode flag) once it lands.
  live tree context (UObjectGlobals.cpp):
    disk sha256 : dcefad5f5ee04813…  (mtime 09:36:06 -- in-flight if very recent)
    DA verified-query block present : yes
    EB enriched marker present      : yes
    EI tostring-bind marker present : NO (bind not landed yet)
    files[] row declares 4abf743ba5f2… ; STALE vs disk (row owner refreshes it when EI lands)
[14:50:34Z] clean wait -- nothing else will run
exit=3
```

Also validated the post-landing path with a simulated EI report in /tmp
(deleted after testing): `--all` correctly reports READY-pending only on
EI's row refreshes + marker (5 stale-row errors + 1 missing-marker error,
all attributed); `--minimal` enumerates its full 13-item pivot bill
including the NONE-FOUND driver search. The watcher's grep was validated
against synthetic ladder lines (all 6 marker families match; noise lines
don't).

## 4. Watch helper (task 4)

`--watch` (and the printed copy-paste one-liner) polls read-only
`ftp://<ip>:2121/data/plugins/UE4SS.log` every 5 s, dedup-printing lines
matching: `[FN] tostring-bind` (EI bind state; `disabled` = fell back),
`[FFO] query-fallback` (DA fix engaging — expected-healthy), `db=v`
(zzdiag v8 T-tokens: database lookups work), `Menu created` (rung 2),
`Spawned ` (rung 3, mission marker), and the --all-only tripwires
`[Seeds][L26] engine delete-listener`, `engine-call resolved`,
`copyin_probe_gate_l26_early_release`, plus `seed_refused`/`l25_dl_refused`
= loud fail-closed → capture addresses, do NOT retry (NEXT-BUILD step 4).

## 5. RECOMMENDATION (decision is the parent's)

**Run `--all`.** Rationale:

1. The launch's instruments — EB's enriched `[FFO] num/ts` marker, EI's
   direct ToStringInternal bind, `[FFO] add-fallback` — read the FName /
   string / FindXOf machinery. Variant C changes the delete-listener
   REGISTRATION path only (engine-call via admission-verified pointers); it
   does not touch the machinery the discriminator reads, and its first-live
   risk is LOUD fail-closed (`seed_refused`, terminal, nothing binds — one
   glance at the watch attributes it; no silent corruption, no wasted
   debugging). Offline proof is unusually strong: host+prospero compile
   receipts, 22/22 mock-fixture checks at the REAL production labels under
   ASan/UBSan, 16/16 hand-rolled regression, prologue bytes re-verified
   against the pinned image at exact file offsets.
2. `--minimal`'s pivot bill is heavy and itself risky: the CM-era driver
   `e1d6ca0c…` has NO copy on disk (must be reconstructed), plus 3 file
   reverts, EC's revert, a MANIFEST re-pin, and row re-syncs across CHANGES
   files — the exact stale-row/drift failure class this wave has been
   fighting. Cost in agent time and new failure surface exceeds the risk it
   removes.
3. Claude's "two small things inside the same build" (EI bind + EB marker)
   is honored in both modes; the additional riders are all offline-proven,
   labelled, and have dedicated watch markers.
4. The EC confound is real but bounded: EC changes the S-path evidence
   stream (v7 S*e throws were pre-fix context), not the [FFO]/[FN]
   discriminator shapes. If the parent's priority is attribution purity for
   the ToString discriminator above mission progress, `--minimal` is the
   scientifically cleaner experiment and the script fully supports it —
   after the pivot bill in §2 is paid.

Either way: `--preflight-only` first; the script's gates then carry the run.

## 6. What I did NOT do (honesty section)

- No writes to any tree (all source/driver/CHANGES files untouched; the
  two packaging drivers, prep tool, and harvest.py unmodified).
- No build/package/upload execution (waiting on EI by design; STEP 0b
  onward is untested against a real execute — the driver invocations mirror
  NEXT-BUILD step 2 exactly and are gated).
- No console/network contact; the `--watch` poller was tested against a
  synthetic local log only.
- The simulated EI report used to exercise the post-landing pre-flight path
  lived only in /tmp and was deleted.

— agent EJ (ZCode), 2026-10-09. Sources: NEXT-BUILD.md (EE),
gating-proposal/CHANGES.json, typescan-r4/CHANGES.json (files[]/changes[]
rows re-hashed live), fname-rootfix/DECISION-MEMO.md + diff (EB),
build_variants.py / l26_packaging_driver.py (read in full),
prep_modded_launch.py + wakeup_prep.sh, markers.json + agent-EH harvest
update, ACCEPTANCE-LADDER.md (CK), STATE-L26-WAVE.md (ED),
l26-build-variants/{EXECUTION,PACKAGING-EXECUTION}-RECEIPT.json,
FINAL-RECEIPT-VARIANT-A.json (5ea2c845 re-hashed today).
