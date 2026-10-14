# agent-BD — harvest pipeline rehearsal against a SYNTHETIC variant-B SUCCESS (B-1) launch

Written 2026-10-08. Mac-local, zero console contact, zero network beyond
loopback-refused curl (documented below). The harvest tool was NOT edited
(task contract); bugs found are reported for a fix agent.

## 0. Scope and method

Rehearsed the per-launch evidence flow for the variant-B first launch
(LAUNCH-PLAN v4.2 outcome B-1: B survives + mod starts + menu opens + spawn
works) BEFORE any real launch:

1. Built synthetic evidence from the documented marker vocabularies only
   (LAUNCH-PLAN v3.3/v3.4/v4.2; agent-O §4-5; agent-AO §1-3 exact strings;
   agent-P §1 tag table; agent-Q §3; agent-AI §3; l24 real log
   `evidence/l24-ue4ss-4.log`; real runtime-status samples from l18/menu-complete
   harvests; klog vocabulary from `evidence/vanilla-saveload-klog-20261008.log`).
2. Ran the REAL `harvest/harvest.py` ANALYSIS layer (imported, unmodified) over
   the synthetic files via a driver, including the live branch of
   `generate_report` — because the tool has NO offline/synthetic mode: its only
   no-network mode is `--dry-run`, which performs NO file analysis (plan +
   skeleton only). See §3.
3. Ran the exact live COMMAND shape on a byte-identical COPY of the tool
   (`sha256` verified) with `--console-ip 127.0.0.1` so the FTP pulls fail
   against loopback (nothing listens on 127.0.0.1:2121; verified) instead of
   contacting the console. Documents what the live command adds over the
   driver (§4).
4. Verified every verdict against the expected B-1 table (§5) and probed the
   variant-B init-passed-:528 semantics specifically (§6) — a real tool bug
   was found in exactly the interaction the task flagged.

All synthetic artifacts live under `/tmp/l25-harvest-rehearsal/` (list + hashes
in §7). Nothing was written into the repo `harvest/` directory or into any
`KLOG_RANKED_DIRS` entry (`/tmp/l25-harvest` was never created; `/tmp/l24-harvest`
untouched) — so no rehearsal file can contaminate a future live klog pickup.

## 1. Synthetic evidence built (exact marker strings, one session)

### 1.1 `ue4ss.log` (62 lines, 6061 B, sha256 d11316e099d1caf68a3ca470dd02d8d34283e71744208f11bc9ad2070afd034b)

One session, banner `[2026-10-08 21:04:11] UE4SS - v3.0.1 - Git SHA #e3ba101…`
(matching the l24 real banner format), timestamps monotonic 21:04:11 →
21:08:40, zero `[t=…ms pid=…]` modbridge appends, zero `mfa_*` strings.
Sequence (line numbers from the synthetic log):

- startup banner + config dirs (l24 format), `[NativeLayout]`, `[Seeds]` slot
  arming, `[BuildProfile]` digest line (:15)
- `[LayoutPreflight] checked=66 startup-required=50 conditional-map=14
  runtime-deferred=2 missing-required=0` (:20, the real l24 summary line 493 format)
- type-scan pass `[TypeScan] scan-complete missing-keys=3 core-ready=true` (:21)
- admission family: `[Seeds][L14] total admission … outcome=admitted` (:24),
  `slot 1 fname pool 0x9765EC0: admitted…` (:25), `FName pool validated…` (:26),
  slot-2 ctor bound (:28)
- agent-Q liveness: `[Seeds][L25] copyin probe state after admission:
  kernel-write dispatch STOPPED (bank retired…; oracle-probes=0; …)` (:29)
- `Post-initialization: GMalloc…` (:33), **`Adding shutdown listener` (:34)**
- **variant-B UnrealInitializer SKIPPED** (:35): `[L25][DeleteListener]
  delete-listener registration SKIPPED (variant B; unsafe long-term: stale
  object pointers not cleared)` — agent-O §4/§5 item 17 exact string
- **G3 suppression** (:36): `[L25][DeleteListener] UObjectArray cache block
  suppressed under variant B: searcher pools + class create/delete listeners
  skipped (ini had bUseUObjectArrayCache=true; forced off)` — agent-AO §3
  exact string (present per task instruction; NOTE per v4.2 item 1 the real
  1947 B ini has the cache false, so on the REAL launch this line is expected
  ABSENT — rehearsal exercises it deliberately)
- `Starting callback garbage collector!` (:37, the :542 proof), `Unreal Engine
  modules (modular/non-modular):` (:38)
- agent-P tags: `[l25] event loop started` (:40), `[l25] on_program_start
  entered` (:41)
- **G1 LuaMod SKIPPED** (:42): `[L25][DeleteListener] Lua-mod delete-listener
  registration SKIPPED (variant B; unsafe long-term: Lua-held stale object
  pointers never cleared and Lua onDelete callbacks never fire)` — agent-AO §1
  exact string
- `[l22] Lua mod started: TownfallItemSpawner (main script returned)` (:43) +
  `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7
  opens the menu.` (:44)
- R3 arm/pump: `[R3] event carrier scan: 1 override(s) found, 1 newly armed`
  (:45), `[R3] FuncPtr-marshal armed on carrier [n: 1F2B] @0x0000001904C31E00
  (callback id 3)` (:46, G2 identity style), `[R3] marshal is the game-thread
  pump …` (:47), `[l25] marshal hook first invocation (carrier fired)` (:48)
- menu open (L3+R3→F7): `[l22] keybind callback entry key=118 ref=7` (:49,
  LuaMod.cpp:2040 `key={} ref={}` format), `ExecuteInGameThread queued
  method=2 state=1 ref=2` + throttled `queued depth=1` + `drained … (callback
  entry)` (:50-52), menu-open pair `12 spawnable pickups found. Pages=2`
  (:53) + `Menu created.` (:54) — fx04-fixture formats
- spawn (CROSS→RETURN): `keybind callback entry key=13 ref=8` (:55), queued/
  drained pair (:56-57), **`[Lua] [TownfallItemSpawner] Spawned Antibiotics x1
  ID=DeprecatedItem`** (:58, the fixture sample string)
- menu close F7 (:59-61) and clean exit `Stopping mod 'TownfallItemSpawner'
  for uninstall` (:62)

Not synthesized (diag-stream side, out of rehearsal scope): `r3_marshal_pumped`
heartbeat, `pad_*` chain tags, `l25_dl_*`, `copyin_probe_*` events,
`runtime_start_ok`. Consequence: those markers read 0 hits in the rehearsal
report — expected, not a defect (see §5 note).

### 1.2 `runtime-status.txt` (68 B, sha256 57e74eaf15bcbf303c0230da5608e7651a2ec2c0e35ba7fa104af118a2c9fd8d)

Exact real format (tab-separated, from the harvested l18/menu-complete
samples), fresh session block, task-specified content:

```
modforge-runtime-status\t1
session\t631\t63205851
state\t1\tactivated\t0\t
```

(i.e. `state 1 activated 0`; fresh session id 631 vs l24-era 184/355; no
`state 2 failed` row — success.)

### 1.3 `klog-20261008-210300.log` (21 lines, 1779 B, sha256 edbee3c82ec9a6ff23936fb6f3f8558046e176b450397a776fd6a6704059c799)

Tap header lines (real capture format), `<118>[SceLncService]
launchApp(PPSA33286)`, `Big App started`, a few `FMEM`/`VM Stats` rows, five
`libSceSaveData` lines (init / api 90 mount / `Mounted > … /savedata0` /
api 92 / api 91 umount), clean game `exit() exit_value=0`. ZERO
`copyin`/`nonsleeping`/`fatal`/`mfa_` strings (verified by grep) — the B-1
success expectation.

## 2. Driver run of the analysis layer (real harvest.py, unmodified)

`/tmp/l25-harvest-rehearsal/driver_rehearsal.py` inserts the REAL
`outputs/2026-10-08-l25-prep/harvest/` on `sys.path`, imports `harvest`, and
calls the same chain `main()` calls on the live path:
`load_markers` (real markers.json) → `read_lines` → `analyze_sessions` →
`scan_markers` → `verdict` (which itself calls `init_passed_528`,
`listener_verdict`, `mod_verdict`) → `generate_report(…, dry=False)`.
Pull rows are shaped exactly like `do_pull()` dicts (status ok/absent, size,
sha256, note); the klog dict is shaped exactly like `resolve_klog()`'s ok
return but supplied directly, so no file was placed in any ranked dir.

Output report: `/tmp/l25-harvest-rehearsal/HARVEST-rehearsal-b1.md`
(27,375 B, sha256 e7d468d3d3ee42f0f2baea7393048943f85302f04b4cb1a86f2d2294dffad40d)
— full live-format report: pull-status table, session boundary analysis, every
marker hit with line numbers and +/-1 context, complete VERDICT section.

## 3. Does the tool have an offline/synthetic mode? — NO

- `--dry-run` is the only no-network mode and it performs NO evidence
  analysis: it prints the would-run curl commands, the marker table, and a
  VERDICT skeleton ("auto-fills on the live run"), writes nothing.
- Any other invocation performs the four FTP pulls via curl to
  `--console-ip` (default `ps5-host-not-configured.invalid` — the console; must NOT be used in
  rehearsal). There is no flag to point at local files for the FTP-sourced
  evidence. `--skip-klog` only skips the klog pickup, not the pulls.
- Therefore the rehearsal split is mandatory: driver for the analysis layer
  (§2), live-command shape on a loopback IP for the pull/report-writing path
  (§4).

## 4. Live-command rehearsal (tool copy, loopback, pulls fail by design)

Tool copy at `/tmp/l25-harvest-rehearsal/tool/` — `harvest.py` and
`markers.json` `sha256`-verified byte-identical to the repo tool
(`537f5c02…`, `51916e17…`). Loopback check: nothing listens on 127.0.0.1:2121;
`curl -o` on refused connection (rc 7) creates NO dest file (verified
empirically) — so failed pulls leave the analysis inputs absent, verdicts
UNKNOWN, never stale.

Run A (task-specified command shape):
`python3 harvest.py --label rehearsal-b1 --skip-klog --console-ip 127.0.0.1`
→ all four pulls `failed (0 bytes)` (rc 7), exit 0, report written with every
verdict UNKNOWN — correct honest behavior for "no evidence pulled".

Run B (klog pickup exercised; synthetic klog placed in the tool copy's own
directory = ranked dir 1):
`python3 harvest.py --label rehearsal-b1-klog --console-ip 127.0.0.1`
→ `resolve_klog` found `klog-20261008-210300.log` in ranked dir 1, and the
report correctly computed `crash-signature=NO` and `admission=OK` from the
synthetic klog alone, everything ue4ss-dependent UNKNOWN.

**What the live command adds over the driver** (and nothing else — the
downstream code path is identical):

1. the four FTP pulls with per-item classification (ok / absent rc 78 /
   empty / failed), bytes and sha256 in the report's Pull status table;
2. `resolve_klog` pickup over `KLOG_RANKED_DIRS` (newest `klog-*.log` in the
   first ranked dir with hits);
3. the report file written next to the script + the one-line stdout verdict
   summary; exit code 0 whenever a report is produced (pull failures
   included, by design).

## 5. Verdict verification — synthetic B-1 run, real analysis layer

All 15 driver checks PASS. Verdict table (expected vs actual):

| verdict field | expected (B-1) | actual | result |
|---|---|---|---|
| init-passed-:528 | YES | YES — "28 same-session line(s) after the last 'Adding shutdown listener' (line 34)" | PASS |
| mod-started | YES | YES (line 43) | PASS |
| crash-signature | NO | NO — no copyin/nonsleeping in klog | PASS |
| admission | OK | OK — no mfa_* anywhere | PASS |
| listener registered | NO | NO (no registration-returned marker — B registers none) | PASS |
| listener refused (code) | NO | NO | PASS |
| variant-B skip detected | YES | YES (lines 35, 42 — both SKIPPED lines) | PASS |
| engine called listener | NO | NO | PASS |
| mod menu opened | YES | YES (lines 53, 54 — the menu-open pair) | PASS |
| item spawned | YES + parsed | YES; parsed `Antibiotics x1 ID=DeprecatedItem` (line 58) | PASS |
| menu failure mode | none | NONE (no UI-fail/WidgetBP/Scan-fail marker) | PASS |
| session count (banners) | 1 | 1 | PASS |
| modbridge appended lines | 0 | 0 | PASS |
| spawn parse name/qty/id | Antibiotics/1/DeprecatedItem | identical | PASS |
| last timestamp | 21:08:40 (clean-exit line) | 2026-10-08 21:08:40, line 62 | PASS |

Expected-marker hit survey: 25 of the 64 markers.json entries hit, all at the
intended lines (`Adding shutdown listener`, GC line, `Unreal Engine modules`,
`TypeScan] scan-complete`, `[LayoutPreflight] checked=`, `outcome=admitted`,
R3 armed/scan/pump, `[l22] Lua mod started`, keybind 118/13 pairs,
`Stopping mod`, `[l25]` family, SKIPPED exact string, Q probe-state, mod
Loaded/menu/spawn family). Success-expected-absent markers all 0-hit
(REFUSED, RISK, header pre/post, registration returned/begin, engine-called,
marshal arm failed, Scan failed, UI creation failed, WidgetBP missing,
mfa_activate_failed, module_start failed).

Two expected 0-hit notes (NOT defects):

- `copyin` (crash-phase marker) hits ue4ss.log ONCE — line 29, the BENIGN
  `[Seeds][L25] copyin probe state after admission` liveness line. The crash
  VERDICT is computed from klog only, so no false crash; but every
  Q-instrumented launch report will show this 1 ue4ss `copyin` marker hit —
  readers must not read it as crash evidence.
- `runtime_start_ok` 0-hit on runtime-status: the synthetic file (per task
  spec) carries only header/session/state; the `runtime_start_ok` ack lives
  in the diag stream per l24 evidence, and no real harvested runtime-status
  sample ever contained it.

## 6. The variant-B init-passed-:528 semantics — TOOL BUG FOUND

Question from the task: does `init passed :528` require a
registration-returned marker that variant B never emits?

**Answer: NO for the success path — but the SKIPPED-line interaction is
mis-implemented and produces a false YES in one failure sub-case.**

How `init_passed_528` (harvest.py:340-389) actually decides: it takes every
non-empty, non-modbridge-format line after the LAST `Adding shutdown
listener`; if any exists → YES. Registration-returned markers only upgrade
the WHY text (agent-O "item 10" rule); they are NOT required for YES. On the
synthetic B-1 success the verdict is YES via the generic path — correct,
because `Starting callback garbage collector!` (:542, v3.4 item 3: "first
proof init passed :528 in B") and 26 more lines follow the listener line.
The rehearsal confirms a real successful B launch will read init-passed YES.

**Bug**: the function's own docstring (harvest.py:349-352) states
"Variant-B SKIPPED lines after it do NOT count (no call made)" — the Z14
rule — but the code (harvest.py:363-379) never excludes them: the SKIPPED
line and the G3 suppression line are ordinary non-modbridge lines and DO
land in `after`. Demonstrated with the real function on truncated synthetic
logs:

- EDGE A — variant-B log ends exactly at the UnrealInitializer SKIPPED line
  (no GC line; process died in the :528→:542 window): verdict **YES**,
  why "1 same-session line(s) after the last 'Adding shutdown listener'".
  FALSE POSITIVE — reaching the SKIPPED log only proves the :528 REGION was
  reached, not passed (the whole point of the docstring rule).
- EDGE B — ends at SKIPPED + G3 suppression: verdict **YES** ("2 … lines").
  Same false positive.
- EDGE C (control, variant A): registration-returned + GC → YES with the
  agent-O why. Correct.
- EDGE D (control): l24 classic stall (listener line last) → NO. Correct,
  unchanged.
- EDGE E (control): only a later-session `[t=…ms pid=…]` append → NO.
  Correct, unchanged.

Severity: on the SUCCESSFUL B-1 launch (this rehearsal) the verdict is
correct because the GC line follows. But B-1 has a sibling failure mode —
B stalls/crashes right after the skip log (one line past the removed
consumer, the exact container-audit hazard v3.4 flags) — and there the tool
would report init-passed-:528 YES while init actually died before :542.
That is the rung where a truthful NO matters most.

**Fix recommendation** (for the fix agent; ~5 lines in `init_passed_528`):
classify post-listener lines into `after`, `appended`, and a new
`variant_b_skip` bucket — any line containing `L25_VARIANT_B`
("delete-listener registration SKIPPED (variant B") or the G3 suppression
string "UObjectArray cache block suppressed under variant B" goes to the
skip bucket, reported but NOT counted as post-:528 progress. Then:
B-success still YES (GC/:542 or later markers present); B-stall-at-SKIPPED
becomes NO with a variant-B-specific why ("only the variant-B skip/suppression
line(s) follow the listener line — the :528 region was REACHED, not passed;
no 'Starting callback garbage collector!' (:542)"). Variant A paths and the
appended-lines rule are untouched.

## 7. Other tool observations (minor, for the fix agent / root)

1. **`listener registered` why-text misleading on variant-B logs.** When
   variant-B SKIPPED lines are present, `registered=NO` explains "pre-l25
   build, or the call hung/refused at the stall point" — none of which is
   true for B (no call was made by design). Suggest appending "or variant-B
   build (see variant-B skip detected above/below)" when the skip line is
   present. Verdict value itself is correct.
2. **KLOG_RANKED_DIRS / PENDING note stale vs the landed agent-T report.**
   harvest.py:103-121 and markers.json `_meta.pending` say the ranked-path
   procedure is "PENDING agent-T-klog-capture.md" — but that report HAS
   landed (2026-10-08 14:26, before harvest.py's own 15:22 mtime). agent-T's
   recipe captures into per-test dirs (e.g. `--out-dir /tmp/l25-vanilla`, or
   beside the console-test session), NOT into `/tmp/l25-harvest` (ranked dir
   2). A tap run per agent-T will therefore NOT be found by the current
   ranked dirs unless root manually copies it. Recommend updating
   `KLOG_RANKED_DIRS` (add the agent-T out-dir convention / the dated
   console-test harvest dirs) and dropping the PENDING notes in harvest.py
   and markers.json before the real launch.
3. **Stale-dest hazard on non-refused pull failures.** `files` is built from
   `os.path.isfile(dest)` AFTER the pull, not from pull success. Connect
   refusal (rc 7) creates no file (verified), but a MID-TRANSFER failure
   (timeout/partial) or a same-label re-run after an earlier success would
   leave a non-empty dest that the tool then ANALYZES while the pull row
   says failed. Suggest: include dest in `files` only when result status is
   ok/empty, or unlink dest before each pull. Low likelihood (labels are
   per-launch unique; --max-time 20), worth a guard anyway.
4. Confirmed-good behaviors worth recording: exit 0 with failed pulls (by
   design, "read the report"); `--label` regex validated; marker scan
   context lines trimmed at 240 chars; the 'copyin' ue4ss marker hit from
   the Q liveness line is descriptive-only (crash verdict is klog-gated).

## 8. Artifacts

| file | bytes | sha256 (first 16) |
|---|---|---|
| /tmp/l25-harvest-rehearsal/ue4ss.log | 6061 | d11316e099d1caf6 |
| /tmp/l25-harvest-rehearsal/runtime-status.txt | 68 | 57e74eaf15bcbf30 |
| /tmp/l25-harvest-rehearsal/klog-20261008-210300.log | 1779 | edbee3c82ec9a6ff |
| /tmp/l25-harvest-rehearsal/driver_rehearsal.py | — | (driver; imports real tool, no tool edits) |
| /tmp/l25-harvest-rehearsal/HARVEST-rehearsal-b1.md | 27375 | e7d468d3d3ee42f0 |
| /tmp/l25-harvest-rehearsal/tool/harvest.py (copy) | 46074 | 537f5c027da5ba52 (= repo tool) |
| /tmp/l25-harvest-rehearsal/tool/markers.json (copy) | 34415 | 51916e17d97367b9 (= repo markers) |
| /tmp/l25-harvest-rehearsal/tool/HARVEST-rehearsal-b1.md | — | live-cmd A output (failed pulls, verdicts UNKNOWN) |
| /tmp/l25-harvest-rehearsal/tool/HARVEST-rehearsal-b1-klog.md | — | live-cmd B output (klog pickup: crash NO, admission OK) |

The repo `harvest/` directory was not modified by this rehearsal (two files
that appeared there during the session, `l24-dryrun-sign-receipt.json` and
`sign_and_verify.sh`, are from other parallel work, not this agent).

## 9. Conclusion

The harvest pipeline's evidence flow for a successful variant-B launch is
PROVEN end-to-end offline: every B-1 verdict field comes out correct from
the documented marker vocabulary, including the full mod-side chain (Loaded
v1.0 → menu-open pair → `Spawned Antibiotics x1 ID=DeprecatedItem` parsed),
admission OK, crash NO, and the listener family reading exactly as variant-B
intends (registered NO + skip detected YES + engine-called NO). One real tool
bug must be fixed before the real launch: `init_passed_528` counts the
variant-B SKIPPED/G3-suppression lines as post-:528 progress, contradicting
its own Z14 docstring rule — harmless for the B-1 success verdict, a false
YES for a B run that dies between the skip log and :542. Fix per §6.
