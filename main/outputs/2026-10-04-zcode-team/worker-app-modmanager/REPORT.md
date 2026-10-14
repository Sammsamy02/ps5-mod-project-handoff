# REPORT.md — worker-app-modmanager: mod-manager core loop (host) + app-track inventory

Origin: worker-app-modmanager, 2026-10-04 (ZCode team, PS5 mod-manager).
Everything here is HOST-side. No console, no network, no nested subagents; no
existing file outside this directory was modified (the lineage dirs were read
only; the composed binary was copied in and sha-pinned). Claim bounds: 90
tool calls, writes only under `outputs/2026-10-04-zcode-team/worker-app-modmanager/`.

## 1. Result

**The mod-manager core loop now exists as tested app logic against the REAL
formats, reusing the proven installer stack instead of re-deriving any of
it.** Deliverables, in order:

| Deliverable | Path | Status |
|---|---|---|
| Claim | `CLAIM.json` | first, per contract |
| App-track inventory + maturity table | `APP-STATE.md` | file:line / receipt-cited |
| Core loop library | `app/modmgr.py` (686 lines, Python 3 stdlib) | 18/18 host tests |
| Console-contract implementation | `app/console_link.py` (209 lines) | 13/13 staged files byte-identical |
| Host tests | `test/run_tests.py` (18 tests) + `receipts/run-tests.log` | **OK, 18/18** (also clean under `-W error::ResourceWarning`) |
| Console contract | `CONSOLE-CONTRACT.md` | protocol + 9 safety gates |
| Provenance | `vendored/modinst_composed` + pins in CONSOLE-CONTRACT §7 | sha `f2215f2f...` (worker-f-r2-fix build: D1+F-R1+F-R2 chain) |
| Receipts | `receipts/{run-tests.log, smoke-cli.log, staged-byte-compare.log, cli-fixture.json}` | rerunnable |

## 2. What the core loop does (and what it deliberately reuses)

All mutations go through the proven composed installer binary
(`vendored/modinst_composed`, sha-pinned, refused to run on sha mismatch):
register-mod, prepare, prepare-archive (F-R1 auto-bridge), install (+`--remove`,
the F-R2 removal semantics), recover. Compose, journal, staged-root swap,
cancel points and crash recovery are NEVER re-implemented. The app layer adds:

- **list** — `roots/<TITLE>/loadlist.txt` parsed per the FORMATS.md grammar
  (TAB rows, <=5 fields, `#` comments, unknown kinds skipped but preserved
  byte-exact on rewrite), joined with `records.txt` states and the app
  disable-overlay into an honest projection: per-mod
  `{state, enabled, files, bytes, bundle, note}` + game
  `{generation, records_sha, live_sha, loadlist_diverged}`. `enabled` comes
  from live loadlist mod rows, never inferred; out-of-band edits are flagged
  `diverged`, not hidden.
- **install (zip or mfb)** — `.mfb`: register -> prepare -> install. `.zip`:
  register -> `prepare-archive --mod` (auto-bridge composes
  `state/bundles/<id>.mfb`; records bundle path repointed — proven by
  `test_05`). The install always passes the FULL desired set
  (records-installed ∪ overlay ∪ new), because the CLI recomposes exactly the
  listed set — this is the correctness property that keeps one install from
  silently dropping other mods (`test_06`). A fixed bug is worth naming: the
  desired set must be records-`installed` ONLY — including `prepared` would
  have resurrected previously uninstalled mods.
- **disable** — guarded loadlist row edit: `.modforge-root` check, header
  check, mod-row check; the mod's `file` rows are attributed EXACTLY via its
  own bundle index (records bundle path -> mfb index paths), with a
  documented prefix fallback and a refuse-rather-than-guess floor. Files
  stay on disk (the runtime sees only loadlisted bytes, so this is the
  correct disable for UE4SS whose startup list derives from the preloaded
  `enabled.txt` row). Atomic tmp+rename+fsync; pre-edit backup under
  `state/modmgr/backups/<TITLE>/`; rows captured in
  `state/modmgr/disabled-<TITLE>.tsv`; records game-row sha resynced, then
  VERIFIED through the binary's own `records` reader (`test_09`).
- **enable** — exact inverse from the overlay; `test_10` asserts the
  loadlist is restored byte-identically and the sha matches.
- **uninstall** — the CLI's `--remove` path: full root recompose without the
  mod, records back to `prepared`, generation++ (`test_13`); overlay cleaned,
  other disabled mods stay disabled (`test_14`), and a later install does not
  silently re-enable them (`test_11`).

The CLI entry (`modmgr.py --binary ... --state ... --roots ... list|install|
uninstall|enable|disable`) was smoke-run end-to-end
(`receipts/smoke-cli.log`).

## 3. Console-contract deliverable

`app/console_link.py` implements the app halves of the contract:
`build_command`/`validate_command` (E2E-CMD-SCHEMA.md grammar with eager
refusals — `test_16` covers verb allow-list, deny-list, mods/remove rules,
unknown keys, true-only flags), `staged_sequence` (the 13-command shape), and
`E2eTranscript` (begin/end pairing with dead-command detection, truncation
cap, status events — `test_17`).

Key evidence: **`staged_sequence()` reproduces all 13 pinned files in
`worker-e2e-staging-final/cmd/` byte-identically** — 13/13 in
`receipts/staged-byte-compare.log`, plus `test_15` against the published
SHASUMS constants. `CONSOLE-CONTRACT.md` documents the drive protocol
(command file -> transfer gate -> identity-gated dispatch -> `.consumed`
consumption proof -> transcript verdict -> records state check) and 9
safety gates (payload identity, stale-command, transcript pairing, ownership
marker, transactional swaps, 3-layer backup/restoration, records-sha resync
verification, capability honesty, no blind retry).

## 4. Honest limits

- **Nothing ran on a console.** The payload dispatch, MODE B config entry
  (revision does not exist), and any `/data` write remain controller-only
  PENDING. My proof surface is the host binary + real file formats.
- **Host binary is the F-R2 host flavor** (`f2215f2f...`); it embeds the
  F-R1 bridge and F-R2 transcript fix but not the console-only RDWR/link
  work (worker-ps5io-rdwr owns that, link-proven only).
- **Disable is app-level, not a CLI verb.** Between installs it edits
  loadlist.txt + records sha directly under the listed guards; the next
  transactional install recomposes and the overlay is deterministically
  re-applied. A future CLI `enable/disable` verb owned by the binary would
  remove the need for the app-side edit path; this worker did not modify the
  C chain (existing files immutable).
- **Enable-order caveat:** restoring rows uses captured indexes clamped to
  the current row count; exact byte restoration is proven for the real case
  (single disable->enable round-trip, `test_10`). Multi-mod interleaved
  disable/enable ordering is deterministic but not asserted byte-exact.
- `running` remains ack-derived only; no in-game execution is claimed
  anywhere (per the studio honesty rules).

## 5. Traceability

Inputs consumed (read-only): worker-8-app-backend (AUDIT/PATCHES/REPORT),
worker-7-installer (modinst core + FORMATS/SERVICE docs + receipts),
worker-installer-ps5io, worker-ps5io-rdwr, worker-zip-to-mfb (bridge design +
e2e_status.c), worker-e2e-final, worker-f-r2-fix (composed binary + F-R2
semantics), worker-e2e-console-dry (E2E-CMD-SCHEMA.md),
worker-e2e-staging-final (STAGED-E2E-COMMANDS.md + cmd/), worker-loadlist-final
(enabled.txt semantics), worker-rollback-runbook. The maturity table in
`APP-STATE.md` cites each; **the pre-existing app (worker-8) had NO install
path reachable and NO enable/disable at all — that gap is what this worker
closes host-side.**
