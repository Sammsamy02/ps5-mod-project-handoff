# agent-GI — first-launch automation + Rule-10 audit (RETRY delivery)

Agent GI (ZCode), 2026-10-08-l25-prep. This is the RETRY of the attempt
that died on a transient network error: the earlier session had written
`FIRST-LAUNCH-AUTOMATION.md`, the example table, and a first
`release_audit.py` but delivered NOTHING (no audit run was saved, no
report existed). This retry verified every artifact against the landed
sources, ran the audit for real, fixed two real defects found during the
run, and completed the delivery. Mac-only, battery mode: no builds, no
console, no candidate-tree edits.

## Deliverables

1. `FIRST-LAUNCH-AUTOMATION.md` — the architecture (sections 1-6), now
   reconciled with GJ's LANDED table schema + honest pattern findings.
2. `phasea-tools/release_audit.py` — Rule-10 blind-test law gate.
3. `phasea-tools/release-audit-run1-runtime.txt` — runtime-scope receipt
   (re-run this session): **exit 2, 10 violations, blind-test game CLEAN**.
4. `phasea-tools/release-audit-run2-full.txt` — full-scope receipt
   (re-run this session): 5177 files, 10 violations, 82 REVIEW warnings.
5. `tables/ue5.6-prospero-shipping-nostats.layout-table.example.json` —
   shipped-cell schema example (Townfall 5.6 cell as data), updated to the
   census-arm anchor schema.

## 1. Architecture verdict (compact)

The compiled-in pinned profile (`RuntimeBuildProfile.hpp` /
`CodeSignatureProfile.hpp` / SeedTable) becomes a runtime-GENERATED,
runtime-VERIFIED, PERSISTED JSON, produced on console from shipped
per-version tables. The admission phases do not change; their inputs
change from compile-time constants to generated+verified pins. Pipeline:
DETECT (marker pack, engine_match.py tiers/thresholds verbatim: triage 16,
REFUSED <3/<8, TENTATIVE >=4, CONFIDENT >=12) -> GENERATE (census-arm
anchors + behavioral verify: slot-2 ctor ByteProperty probe, pool words,
GMalloc V1/V2, SCO empty-callback — all existing console-proven phases
retargeted) -> PERSIST (game mod-root `profile.json`, runtime-owned,
image-sha keyed; re-verify each launch, fail-closed -> regenerate) ->
SELF-TEST (GB ueselftest auto-runs once, log-only verdict; PASS >=95%
AND critical families; WARN carries diag bundle; SAFE-MODE = critical
family FAIL => mods off) -> MOD-PER-TYPE (GG manager deploys GJ's 5-mod
game-agnostic pick set; GC LADDER-V2 3-launch acceptance law applies).
Critical families (GI's definition, the task's ask): `fname_pool`,
`fname_ctor`, `guobjectarray`, `gmalloc`, `process_event_reflection` —
failure of any means every mod's every call can read garbage (crash
class); non-critical shortfalls are WARN-only. Skeleton + [FL] marker
contract + NEW_UNITS/integration checklist: doc section 5. Sequencing
(dogfood law): the generator must pass TOWNFALL itself with the pinned
profile removed BEFORE the second game's blind test — this also forces
the F1 root-derivation fix below.

## 2. Audit findings (honest)

Runtime scope (the release-gate form), terms
`StellarBlade|Stellar Blade|PPSA13197` +
`Townfall|PPSA33286|Silent Hill Townfall`, current game = Townfall:

- **Blind-test game: CLEAN — zero occurrences in RUNTIME + PROFILE-TABLE
  scope.** The law as proposed holds today for the second game.
- **Townfall: 10 violations / 12 occurrences, exit 2.** One is
  BEHAVIORAL: `include/LuaPathShim.hpp:80`
  `kDefaultModBridgeRoot="/data/modbridge/roots/PPSA33286"` — a compiled-in
  per-game default that would silently point any new game at Townfall's
  mod root. Real generality bug, must become runtime-derived (the doc
  marks it F1/MUST GO). The other four RUNTIME hits are comments
  (LuaMod.cpp:8566, LuaPathShim.cpp:89, LuaPathShim.hpp:9,
  UnrealInitializer.cpp:1961) — scrub at release. The five PROFILE-TABLE
  hits are the generated pinned-profile units — resolved BY this
  architecture (generated per-game profiles are runtime state, not
  compiled strings).

Full scope: 82 REVIEW warnings, all blind-test-game DIAGNOSTIC hits.
Dispositions: session receipts/mission docs (allowed per the boundary);
pre-pivot translation-core docs + legacy `ps5-mod-bridge` test fixtures
(never ride the release; scrub if shipped under A4); ONE actionable —
`blade-prep/per-version-table.json` L4 has the blind-test name in its
`generated_by` provenance string: the packer must whitelist provenance
fields so no game name flows into a shipped cell.

## 3. Defects found and fixed during this retry

1. `release_audit.py` hung forever on the full-scope walk: clr-debug-pipe
   FIFOs under `outputs/*/tmp/` blocked `read_bytes()`. Fixed with a
   stat/S_ISREG regular-files-only guard; full scope now completes in ~40s
   with identical runtime-scope results (exit 2, same 10 violations).
2. The doc's first draft pinned anchor generation to pattern-only
   multi-pattern agreement — GJ's landed runner proved that bar WRONGLY
   REFUSES the Townfall positive control (patternsleuth sets scored 0/9
   GUO and 0/14 GM image-wide; compiler idiom drift). Doc + example table
   now specify census/consensus ARMS (LEA-xref census, prefix consensus,
   UTF-16 census) with patterns demoted to advisory probes
   (`agreement.min_arms`, not `min_patterns`).

## 4. Schema alignment with GJ (the reconciliation GJ requested)

GJ's `blade-prep/per-version-table.json` (authoring form: 7 version-keyed
bands, per-field status/confidence, laws block = invariant contract) ->
shipped per-cell tables (packed form). Rules now in doc section 3a: laws
survive verbatim; field map (fname_pool/guobjectarray/... ->
structures/anchors/verify_probes; marker_classifier -> detection); seed
bands pack to NOTHING (generator refuses unmeasured versions; only 5.6 is
measured today); base_policy adopted (live base observation, base_status
emitted); `min_arms` schema change. GJ's confirm_law governs upgrades.

## 5. Honest unknowns (unchanged from doc section 3d)

Licensee forks (~5%, refuse + A4 adaptation path), toolchain drift within
a version, non-shipping configs on real PS5 eboots (unverified), Win64->
PS5 code-pattern divergence (structures shared, anchors PS5-measured),
UE4.0-4.22 legacy shapes (nothing measured; authored only when a real
target exists), patch-level deltas (5.6.1 vs 5.6.2 — verify probes catch,
refusal triggers maintenance). The 95% coverage bet rests on engine-ABI
offsets being game-independent within a cell — inferred from Townfall +
PC UE4SS per-version layouts, to be confirmed per version by measurement.

## Knowledge sources

[LOCAL-SOURCE] CLAUDE-ROUND5-ADDENDA.md; FZ-GAME2-PROFILE-DRYRUN.md;
agent-GJ-blade-prep.md + blade-prep/{per-version-table.json,
PER-VERSION-TABLE.md, run-blade-profile.py receipts}; phasea-tools/
engine_match.py (tiers/thresholds read from code); profile/units/
{RuntimeBuildProfile.hpp, CodeSignatureProfile.hpp}; candidate/src/
UnrealInitializer.cpp (phases, spans, budget, guards read from code);
ueselftest/CALL-SHEET.md (GB); mod-ladder/LADDER-V2.md (GC);
mod-manager/DESIGN.md (GG). [TRAINING-KNOWLEDGE] patternsleuth pattern
sets, PC UE4SS per-version member layouts. [INFERENCE] the 95% per-cell
ABI-stability bet. [DESIGN] everything labeled as such in the doc.
