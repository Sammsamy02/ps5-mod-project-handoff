# agent-BE — harvest tool fixes from the agent-BD rehearsal findings

Written 2026-10-08. Mac-local, fully offline (zero network, zero console
contact; all verification reuses agent-BD's synthetic evidence at
`/tmp/l25-harvest-rehearsal/`). Edited exactly two files:

- `harvest/harvest.py` sha256 `537f5c027da5ba52...` (pre-fix, = BD §8) ->
  `bee23a6013918dd72649ac689e1959ce4e556295c6c77970a95ba12c92f89c08`
- `harvest/markers.json` sha256 `51916e17d97367b9...` (pre-fix) ->
  `f3a1c2cdfd41ea48dbe27769a9e6526e9d7ca26a3c2d67f701ba3b2d39eb35f0`

Nothing else in the repo was touched (`prep_modded_launch.py`,
`sign_and_verify.sh`, `l24-dryrun-sign-receipt.json` untouched).

## Fix status x4

### 1. DONE (PRIMARY) — variant-B init-passed-:528 false positive
`init_passed_528` now classifies post-listener lines into THREE buckets:
`after` (same-session progress), `appended` (later-session modbridge), and a
new `variant_b_skip` bucket. New constant
`L25_VARIANT_B_SUPPRESSION = "UObjectArray cache block suppressed under
variant B"` (agent-AO §3 string) alongside `L25_VARIANT_B` (agent-O §4/§5
item 17 string; matches both the UnrealInitializer and the Lua-mod SKIPPED
lines). Lines matching either string are EXCLUDED from `after` and stored as
`(lineno, text)` pairs. Z14 rule now implemented exactly as the docstring
always claimed:
- `after` non-empty -> YES (registration-returned still upgrades the why for
  variant-A logs — EDGE C passes);
- `after` empty, `variant_b_skip` non-empty -> **NO** with the variant-B
  why ("the :528 region was REACHED, not passed: variant B makes no
  registration call, and no 'Starting callback garbage collector!' (:542)
  proof line follows"); checked BEFORE the appended-lines branch so a
  B-stall + later-session appends still reads as the B stall.
- `appended`-only and listener-last branches unchanged (l24 behavior).
The report's VERDICT section now prints each skip line as
`variant-B skip/suppression line (REPORTED, not counted as post-:528
progress; line N)` under init-passed-:528 — presence + line numbers, in
every outcome (YES included). The --dry-run skeleton line documents the Z14
rule.

### 2. DONE — registered=NO why-text
`listener_verdict` computes `vb` (variant-B SKIPPED lines) BEFORE building
the `registered` why. When `vb` is non-empty and no registration-returned
marker exists, the why reads "no L25 registration-returned marker (pre-l25
build, or variant-B build (registration skipped by design); see 'variant-B
skip detected' below)" — the hung/refused wording is used only when no
variant-B line is present (verified preserved on the l24 log).

### 3. DONE — stale PENDING agent-T notes + KLOG_RANKED_DIRS
- `markers.json` `_meta.pending` is now `[]`; new `_meta.klog_procedure` key
  records that agent-T-klog-capture.md LANDED 2026-10-08 14:26 and that
  KLOG_RANKED_DIRS was updated to its recipe; agent-T added to
  `known_today_citations`.
- `harvest.py` `KLOG_RANKED_DIRS` now: script dir, `/tmp/l25-harvest`,
  `/tmp/l25-vanilla`, `/tmp/l25-variant-B`, `/tmp/l25-variant-A`,
  `/tmp/l24-harvest`, plus `KLOG_CONSOLE_TEST_GLOB` =
  `outputs/*/console-test-*` (3 dirnames up from the script; cheap glob —
  8 dirs / 17 files today; l13/l14/l15/l16/l18/l22/l23/menu-complete
  precedent). **Resolution semantics changed** from "first ranked dir with
  hits" to "newest klog-*.log (mtime) across ALL ranked dirs": required
  because the agent-T family leaves stale captures in sibling out-dirs
  (e.g. today's `/tmp/l25-vanilla/klog-20261008-192654.log` must never
  shadow a newer variant-A/B tap). Tap files are create-only, never
  truncated, so mtime = recency of the tap's last write. `KLOG_TAP_NOTE`,
  the ranked-dir comment block, the dry-run listing (new
  `klog_search_dirs()` helper), and the report Provenance line all updated.

### 4. DONE — pull-dest guard
- `do_pull` unlinks `dest` (via `os.path.lexists`) BEFORE running curl, so a
  mid-transfer failure or same-label re-run can never leave a stale/partial
  file.
- `main()` builds `files` ONLY from items whose pull status is `ok` or
  `empty` (0 bytes fetched, recorded per l23 autotune precedent) AND whose
  dest exists on disk; `failed`/`absent` items are excluded even if a file
  survives. (BD §7.3's recommendation followed verbatim.)

## Verification matrix (all offline, `/tmp/l25-harvest-rehearsal/verify_BE_fixes.py` — 43/43 PASS)

| check | expected | actual | result |
|---|---|---|---|
| BD driver re-run (`driver_rehearsal.py`, fixed tool, synthetic B-1) | all 15 checks PASS | all 15 PASS | PASS |
| B-1 init-passed-:528 | YES, unchanged | YES — why now "25 same-session line(s)" (was 28; 3 skip lines excluded) | PASS |
| B-1 skip lines reported-not-counted | lines 35/36/42 as REPORTED bullets | exactly that (report lines 318-320); after[:3] now begins at the GC line | PASS |
| B-1 registered why | variant-B wording | "or variant-B build (registration skipped by design)"; no hung/refused text | PASS |
| EDGE A (B log ends at UnrealInitializer SKIPPED :35) | NO (was false YES) | NO; skip=[35]; why = REACHED-not-passed + :542 | PASS |
| EDGE B (ends at SKIPPED + G3 :36) | NO (was false YES) | NO; skip=[35,36] | PASS |
| EDGE B2 (skip lines + later-session append only) | NO | NO | PASS |
| EDGE C (variant-A: registration-returned + GC) | YES, agent-O why | YES; skip bucket empty | PASS |
| EDGE D (l24 stall: listener line last) | NO, l24 why | NO, "l24 stall signature" | PASS |
| EDGE E (later-session `[t=..ms pid=..]` append only) | NO, appended why | NO, "LATER session's ... appends" | PASS |
| l24 regression `/tmp/l24-ab2-ue4ss.log` (old tool copy vs fixed) | every verdict field + why identical | identical incl. init NO (stall, line 602), registered keeps hung/refused wording (0 variant-B strings in log), after bucket unchanged | PASS |
| fix 3 ranked dirs | agent-T dirs + console-test glob present | all 6 literal dirs + 8 console-test dirs globbed; resolve_klog ok; newest-file-wins; stale-higher-ranked does NOT shadow newer (both monkeypatched-dir tests) | PASS |
| fix 4 pull guard | failed pull analyzed never | dest unlinked on failed pull; `failed`/`absent` excluded from `files`, `ok`/`empty` included | PASS |
| `--dry-run` | exit 0, no writes | exit 0; `HARVEST-be-dryrun-check.md` NOT created; output lists new ranked dirs + Z14 skeleton note | PASS |

Driver re-run output + regenerated report: `/tmp/l25-harvest-rehearsal/`
(`HARVEST-rehearsal-b1.md` regenerated by the fixed tool, 15/15 PASS);
dry-run transcript `dryrun-be.txt`.

## Remaining known-stale items

1. **Pre-launch klog resolution picks an old capture by design.** With no
   live tap running, `resolve_klog` right now resolves to the newest
   existing file — the l23 CRASH capture
   `2026-10-08-l23-package/console-test-.../klog-20261008-173728.log`. This
   is the mtime heuristic working as intended (old code had the same class
   of behavior via /tmp/l24-harvest). At the real launch the agent-T tap
   (started BEFORE launch, create-only, appending) will be newest and win;
   root must still start the tap per agent-T §2 — the harvest cannot conjure
   a capture that was never run.
2. `/tmp/l25-harvest-rehearsal/tool/` is now the PRE-FIX snapshot (it was
   byte-identical to the repo tool when BD wrote it). Anyone re-running BD's
   live-command rehearsal against that copy tests the old code; my verify
   script deliberately imports it as the old baseline for the l24
   old-vs-new regression.
3. Informational, unchanged from BD §5: on Q-instrumented launches the
   `copyin` marker hits ue4ss.log once from the benign `[Seeds][L25] copyin
   probe state after admission` liveness line (crash verdict remains
   klog-gated); and per LAUNCH-PLAN v4.2 item 1 the G3 suppression line is
   expected ABSENT on the real 1947-B launch (ini cache=false) — the new
   bucket handles its presence or absence either way.

No other known-stale items. The tool remains --dry-run-host-tested only;
first live run is root's post-launch harvest.
