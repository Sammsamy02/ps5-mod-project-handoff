# PREFLIGHT-VERDICT — composition-6 real one-pass run

Worker: worker-comp6-preflight-audit · 2026-10-04 (audit window ~21:36–22:0x UTC).
Read-only cross-audit of the complete comp6 input set. Flag-only; nothing fixed.

## Verdict: GO-WITH-FIXES

The mechanism, pin lattice, receipts, in-place landings, and rehearsal evidence
are internally consistent and re-verified against disk during this audit. Three
blocking items must be closed first; they are exact, small, and none requires
re-architecture. One caveat class is permanent: the tree was landing edits and
the driver was being edited DURING this audit (driver sha changed 57418b7f →
d760b5b7 at 21:49:32; Ps/Owned pins re-pinned; real receipts regenerated;
ONE-PASS-CHECKLIST/REPORT/ODR-SAFETY delivered mid-flight) — every sha below
was measured against the state on disk at the time shown, and the operator must
re-measure the driver sha at execution time.

## Blocking fixes (exact one-line actions)

- F1 (entry r3/r4 ownership conflict — the stale-pin class that bites):
  `change-manifest.json` packaging_units declares entry **r4**
  (`runtime_module_entry_r4.c`, sha `c97055f4…`, timestamp+pid freshness oracle,
  "worker-validated 34/34"), but `build_runtime_composition6.py` still defaults
  `ENTRY_DEFAULT/ENTRY_MEASURED_SHA` to **r3**/`51771ab5…` and
  ONE-PASS-CHECKLIST P7 says "entry unchanged (r3)". Executing the checklist
  verbatim ships r3 with every gate green — silent divergence. Action: bump the
  driver default to r4/`c97055f4…` AND rewrite checklist P7 to r4 (r4 passes all
  entry pre-flight text checks and keeps `force_align_arg_pointer`; see REPORT
  §3), or the controller strikes r4 from the manifest. Do not run with the
  current mismatch.
- F2 (gate v2 manifest does not exist yet): `change-manifest.json` declares
  "schema: MANIFEST-SPEC.md v1" but lacks `manifest_version`, `manifest_id`,
  top-level `justification`, per-unit `role`/`receipt`/`sha256_after` (sha is
  the literal `PENDING-FROM-ONE-PASS`), and has no `resolution_b` /
  `d2_text_base` sections — `gate_check_composition5_v2.py --manifest` would
  fail M0/M1 immediately. Action: after the build, the controller writes the
  v2-format manifest per gate-evolution REPORT §4 step 2 (or records the
  explicit decision to accept on the v1 live gate only — which contradicts
  input (e) being in the comp6 set).
- F3 (post-build acceptance gap): ONE-PASS-CHECKLIST §4 lacks ODR-SAFETY §6.5/6.6
  checks. Action: add two acceptance rows — (a) all 15 fresh-TU `.d` files
  resolve ONLY new-vintage paths for the 5 changed headers, zero frozen
  satellite paths remain; (b) emitter sets: `ValidateNativeProgramContext` =
  {program.o new body}, `MakeNativeProgramContext` = {native-loadercontext.o},
  `native_mod_source()` = {luamod.o, settings.o, program-signatures.o} with
  unchanged bodies; disassemble the scansource ctor for the text_base guard shape.

## Pre-flight mechanical steps the checklist already owns (verify executed)

- P8 pending on disk: `worker-comp6-driver/staging/lineage-overlay-2` still
  present — mv aside before the pass (P8). P9 verified clear (composition-6
  run-1 and prx-candidate-composition6-1 absent).
- DRYBUILD-RECEIPT §6 steps 1–2 (staging-d1 refresh + adapter-only intercept)
  are SUPERSEDED by the in-place strategy actually executed (Diff B/C landed in
  ps-scan-abi-1/ps-scan-provider-1 at 21:4x; ODR FINDING-2 remediation done).
  Action: add a supersession note to DRYBUILD-RECEIPT §6 so no operator
  re-introduces the intercept hack (which provably reaches only the adapter TU
  and re-creates the FINDING-1/2 mixed-vintage hazard).
- Record the blessed scaffold shas: manifest sha fields are still
  `PENDING-FROM-ONE-PASS`; the shas exist and were measured (REPORT §2). Also
  drop the `-pending-final` suffix from the bridge pin text (`feed5254…` is
  final on disk, matches driver `BRIDGE_MEASURED_SHA`).
- Recommended cheap hardening: r12 has NEVER been through a packaging compile
  (rehearsal compiled r11 `ebef44bf…`; dry-build compiled r11). Its include list
  is byte-identical to r11's (diff: no `#include` changes), so the 30-header
  staging set covers it, but the first r12 `-Werror -Wall -Wextra` compile will
  happen inside the one-pass. Action: run the dry-build Step-2.5 pattern once —
  bridge r12 syntax-only with the packaging argv — before the pass.

## Non-blocking flags (fix at leisure, do not block)

- N1 Wrong-count anchors: "31-header" in driver docstring (:23) and
  `--stage-header` help (:616); actual DEFAULT_STAGE_HEADERS = 30, == comp5-2
  overlay (30) == rehearsal overlay (30). Checklist/REPORT correctly say 30.
- N2 Drybuild doc drift: APPLY-ORDER Step 1 says "exactly 6 documented edits",
  copy sha `50e8d16c…`; actual file is `e0f494e6…` with 7 edits (edit 7 added
  post-write; DRYBUILD-RECEIPT §1 has the correct values). The stale line even
  references "edit 7" further down.
- N3 Static-check count phrasing: drybuild receipt/manifest say "20/20 static
  checks"; console logs print 17 `check …: PASS` lines; gate P1 reads "20
  checks, failing=none" from the packager receipt (3 structural checks do not
  print). Reconciled — footnote it to prevent a future wrong-count hunt.
- N4 Provenance: wiring worker's CLAIM.json (17:55) predates r4/r12/host_test_r8;
  the "34/34" r4 validation is durably recorded only in the transport-ledger
  mirror prose. A dedicated test receipt would be stronger.
- N5 Read-lease `OwnedMappedImageCaller.hpp` variant still old vintage (not in
  the composition include graph; satellite/test builds only) — ODR remediation
  2 tail, still open.
- N6 Contradictory guidance about port `UE4SSProgram.cpp`: ODR remediation 1
  says "land the edit"; checklist P5 says keep the port file UNMODIFIED (staged
  copy workflow; driver default `staging/comp6-program/UE4SSProgram.cpp` ==
  scaffold `75abf2ab…`). P5 is authoritative — an operator "fixing" the port
  file mid-flight would force `--file` re-admission. Note it in the checklist.
- N7 Moving-target hygiene: driver edited 21:49:32 during this audit; receipts
  regenerated mid-audit (verified OK against landed bytes); REHEARSAL-EVIDENCE/
  ONE-PASS-CHECKLIST/REPORT/ODR-SAFETY.md all appeared mid-audit. Re-measure
  the driver sha at execution; do not accept "as of audit" shas for it.

## Verified-good summary (the GO part)

- Pin lattice re-measured against disk: bridge r12 `feed5254…`, bridge.h
  `ecada77e…` (stale `c47f7184` lesson applied), entry r3 `51771ab5…` / r4
  `c97055f4…`, fixed edition `49fe2684…`, package_prx `e6fbe6c5…`,
  composition-4-fix script `36cc8199…`, comp5 driver `aaf47bda…`, dry-build
  object `ef19a4b8…`, dry gate receipt `7f5906c3…`. All 28 pinned rows of
  DEFAULT_STAGE_HEADERS match disk after the 21:49 re-pin (0 stale; the 3
  Resolution-B headers are unpinned-by-design, measured at run).
- Inventory: all 1328 dependency rows re-hashed against disk — 0 missing,
  0 drift; the 176-element program argv authoritative copy differs from the
  control re-run only in `-o`/`-MF` paths (expected).
- Manifest ↔ scaffold: all 8 scaffold sources declared exactly once (3 changed
  units + 5 staged headers); no orphans.
- 5-hop lesson absorbed: independent grep of every composition `.d` (comp1–4,
  dry1) plus the ODR worker's full 368-walk audit agree — no retained unit has
  an interceptable quoted-include path to a changed header; the only retained
  contacts (luamod/settings/signatures via `native_mod_source()`,
  ownedmappedresolvercollector reference-only) are proven offset-safe/benign.
- Bridge-header vintage coupling: RESOLVED by construction — r12 requires the
  merged vintages (`allow_configured_roots_for_missing_executable_path`,
  `configured_game_directory="/app0"`, `configured_working_directory`), and the
  in-place landings (port `0a068bb9…`, W `d857bc0d…`/`1c990d0b…`, satellites
  `f04ce4aa…`/`fb8aabec…` — all == scaffold shas) now feed the bridge TU the
  new vintages through the default staging set.
- Port-vs-staged identity plan: ODR sha-equality gate present, fail-closed,
  negative refusal rehearsed (`staging/odr-negative/`), evidence at
  `staging/odr-gate.json` (`byte_identical: true` at the edited sha).
- Entry r4 packaging interactions reasoned from the diff: r3→r4 touches only the
  module_start failure-path logging (+ static `log_append_u64`);
  `force_align_arg_pointer` macro, both exports, and the atomic guards are
  untouched → entry pre-flight text checks, packager prologue byte-evidence,
  and I1=196 all hold; `clock_gettime`/`getpid` were already imported in r3
  (write_status_failed), so the import census is unchanged.
- Dry1 mixed-vintage PRX candidates (`35ba028e…`/`0dede0ee…`) are quarantined
  DO-NOT-LAUNCH by ODR FINDING-1 — keep that quarantine.
