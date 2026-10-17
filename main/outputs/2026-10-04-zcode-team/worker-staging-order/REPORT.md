# REPORT — worker-staging-order (2026-10-04)

## What this worker fixed (the launch9 incident)

**Incident (real):** `stage-ridealongs` published the 9-entry `ploader.ini` (8 plugins
enabled) BEFORE verifying the plugin files existed on the console, then halted at an
xwp readback check. The console was left with 4 ENABLED entries whose files were
missing (`fc1pool`, `qh_driver`, `observer`, `carrier` — 0 B). At the next game
launch ploader spent time on the dead entries and the user waited ~10–15 min.

**Law implemented (user directive):** upload + verify every required file FIRST;
publish the enabling configuration LAST. An interrupted staging must never leave
missing plugins enabled.

## What changed

All changes are in the controller-owned tool, patched in place
(`outputs/2026-10-04-zcode-team/worker-console-batch-runner/`), plus its fake-console
test harness. Unified diff: `DIFF.md`. Full pass output: `SELFTEST-RECEIPT.txt`.

### console_batch.py — new sha256 `6940971e…7432040c`
(pre-patch dirty snapshot was `b8a9dd62…`; see "Broken dirty baseline" below)

1. **Decision/publish split (the core fix).** `final_ini_decision_step` is now
   LOCAL-ONLY: it computes the bootstrap-or-ensure plan, saves `ploader.ini.new` +
   `ploader-edit-decision.json`, and stores NOTHING to the console. The bootstrap
   status is armed as `stor: "bootstrap-pending"`. `final_ini_stor_fn` is now THE
   single publish step: it handles `pending` (ensure-append STOR),
   `bootstrap-pending` (the ONE sanctioned whole-file canonical STOR, gated as its
   own `INI-BOOTSTRAPPED` verdict — written only after the publish lands byte-exact)
   and `none` (no-op). On any publish failure the console keeps its previous ini.

2. **stage-ridealongs reordered** to the order law (step ids kept stable; `ride.4`
   deliberately retains its historical handle but now runs LAST before the handoff):
   `ride.0` preflight → `ride.1` §3 constants cross-check → `ride.2` SC-0 pre-image +
   console `.bak` → `ride.2b` decision (LOCAL ONLY) → `ride.3` local byte-exact proof
   → `ride.5` freshness baselines → `ride.6..ride.9b` the FIVE PRX uploads, each under
   the established SHT-strip readback law (exact-length + body-sha + Ehdr-exception;
   ONE re-STOR then abort) → **`ride.9c` NEW pre-publish guard** → `ride.4` publish
   (+ byte-exact RETR readback) → `ride.10` handoff.

3. **Pre-upload guard (`ini_publish_guard_fn` + `ini_enabled_paths`), shared.** Before
   any ini STOR, every enabled (`= 2`) entry of the NEW ini must be verified-present:
   - the 5 staged PRXs via THIS run's SHT-strip law verdict JSONs (a no-op decision's
     readback-only verification satisfies it equally — same established content law);
   - every other enabled entry (the historical pre-existing `composition`,
     `aligned_entry`, `path_test` lines this tool never uploads, plus any line staged
     by an earlier flow) via a fresh RETR presence proof: fetchable + non-empty +
     sha recorded. A 0-byte or unfetchable file — the exact launch9 signature — is a
     loud REFUSAL: the ini is NOT published and the old (safe) ini stays. Receipt:
     `<label>-publish-guard.json`. A no-op decision publishes nothing, so the guard
     records SKIP-GUARD-NOT-NEEDED (existing noop behavior unchanged).
   Guard + publish share the `ini` dependency group, so a guard refusal also skips
   the publish even under `--continue-on-failure`.

4. **Guard steps added to the other ini-publishing flows.** `stage-observer` gets
   `obs.3b` and `stage-qdrv1` gets `qdrv1.3b` (both flows already uploaded their own
   PRX before the ini STOR; the guard adds the enabled-entry presence law for the
   rest of the new ini, including the legacy numbered `[modules]` shape that
   `ini_enabled_paths` also parses). `rollback-ridealongs` is intentionally NOT
   guard-gated: its publish only flips `= 2` → `= 0` (disables), never enables.

5. **NEW `restore-ini` flow (rollback helper).** Restores `ploader.ini` BYTE-EXACT
   from a locally saved pre-publish copy — default
   `outputs/2026-10-04-zcode-controller/launch9-evidence/ploader-ini-9entry-restore.txt`
   (verified on host: 440 B, sha `fd9c7393…`, byte-identical to
   `ploader.ini.canonical`); override with `--ini-src`. Sequence: `rs.0` local source
   identity + ini shape law → `rs.1` freeze-classify the console pre-state
   (already-restored → honest no-op; recorded post-incident 3-enabled minimal sha
   `32d0c0d9…` → sanctioned; anything else → REFUSED, never overwrite divergent
   content blind, with `--allow-divergent-pre` as a loudly recorded operator
   override) + console `.bak` FIRST → `rs.2` the SAME pre-publish guard → `rs.3`
   STOR + fresh RETR readback byte-exact + `restore-verdict.json`.

6. **Minimal repair required for runnability.** The file was untracked-dirty and the
   dirty edit had broken it: `_dll_payload_flow`'s manage:check step referenced an
   undefined `alias`, so BOTH suites crashed with `NameError` at import/plan time
   (nothing ran). Fixed to `d["alias"]` (the evident intent of that hunk; the e2e
   hunk of the same dirty edit was already correct). No other prior-edit content
   was altered.

7. Selftest additions (A3/A4): flow-order assertions (decision-local → uploads →
   guard → publish → handoff; observer/qdrv1 order), the decision step stores
   nothing, exactly one publish-STOR command exists, guard-helper fixtures
   (canonical enables exactly the 8 pinned; numbered §1 shape; unparseable ini
   yields problems, never a silently empty enable set; mode-flip joins the enabled
   set), restore-ini anchors.

### tests/fake_console_drive.py — new sha256 `bd309ccb…9ec8b4281`
- Fake console now serves the 3 historical pre-existing PRXs (presence content) and
  logs an ordered `stor_sequence` (`bak`/`prx`/`ini`) so the law's ORDER is asserted,
  not just the end state.
- Four new scenarios (the requested selftest cases, run against the REAL execution
  engine): **(a)** happy path — console sequence is exactly `.bak` → five PRX law
  STORs → ONE ini publish STOR, byte-exact canonical, guard receipt with 8 enabled /
  0 missing; **(b)** xwp readback law failure mid-flow — ride.6 FAILs, guard+publish
  SKIPPED, no ini STOR ever, old ini byte-identical on the console (the launch9
  incident is now impossible), exactly one re-STOR then abort; **(c)** a `= 2` entry
  whose file is 0-byte — all five PRXs still upload+verify first, ride.9c REFUSES and
  names `modbridge_path_test.prx`, publish skipped, console ini untouched;
  **(d)** restore-ini — byte-exact restore over a 3-enabled post-incident state,
  honest no-op on re-run, divergent-pre refusal by default, and guard refusal of the
  canonical restore while `fc1pool` is 0-byte (restoring would re-create launch9).

## Verification (host-only)

- `python3 console_batch.py selftest` → **430 ok, 0 failed** (exit 0). Last good
  revision was 405/0; +25 from the new order-law/guard/restore checks.
- `python3 tests/fake_console_drive.py` → **117 checks ok, 0 failed** (exit 0).
  Last good revision was 92/0; +25 from the four staging-order scenarios.
- `plan --only stage-ridealongs` / `plan --only restore-ini` inspected: the publish
  step is strictly after uploads + guard; restore-ini steps are rs.0–rs.3.
- Full output: `SELFTEST-RECEIPT.txt`.

## What could NOT be tested locally (honest limits)

- **No real-console contact.** Everything is simulated (in-process curl interception).
  The new steps' real behavior against the actual ftpsrv/HTTP services is unproven
  until the controller runs them.
- **Pre-existing enabled entries get presence proofs, not byte-identity.** The tool
  has no pinned identities for `modbridge_supplier_composition.prx`,
  `modbridge_runtime_aligned_entry.prx`, `modbridge_path_test.prx` (they are
  historical console files it never uploads), so the guard can only prove
  fetchable + non-empty + recorded sha. The 5 staged PRXs get the full established
  SHT-strip content law. If byte-identity pinning for the historical files is later
  wanted, the guard is the single place to add it.
- **Presence proof races are narrowed, not eliminated.** A file could in principle
  vanish between the guard RETR and the ini STOR milliseconds later; the window is
  tiny and the same class of risk existed in the prior law (verify-then-rely), but
  it is not provably closed.
- **`--continue-on-failure` interaction:** guard+publish share a group, so a guard
  refusal skips the publish; however the `ride.10` handoff (group `handoff`, pinned
  by an existing selftest assertion) still evaluates after an upload-group failure in
  continue mode — pre-existing behavior, unchanged. The default operating mode is
  stop-on-first-failure, where everything after a failure is skipped.
- **`--allow-divergent-pre` is intentionally an operator authority, not a
  recommendation.** It is recorded loudly in `ploader-edit-decision.json`
  (DIVERGENT-PRE-ACCEPTED) but it does allow overwriting an unclassified console ini.
- **The classified post-incident pre-state sha (`32d0c0d9…`) is pinned from the
  controller's report**; its exact bytes were not available locally, so the
  "sanctioned pre-state" branch is exercised in the fake only via the explicit
  override path (the fake's 3-enabled minimal does not hash to `32d0c0d9…`).
- **Prior flows' fixtures unchanged:** stage-observer/stage-qdrv1 gains are additive
  steps; their delivered laws (append-only, single-character rollback flips,
  full-identity readback) are untouched, as is every other flow.

## Operational note for the controller

To get from the current post-incident console (3-enabled minimal) back to the
9-entry canonical: run `stage-ridealongs` — it now uploads + law-verifies all five
PRXs FIRST, refuses the publish if ANY enabled file is missing, and only then
publishes the canonical ini. `restore-ini` is for byte-exact recovery from a saved
copy whose enabled files are present (it will refuse the canonical while files are
missing — that refusal is the incident class, working as directed).
