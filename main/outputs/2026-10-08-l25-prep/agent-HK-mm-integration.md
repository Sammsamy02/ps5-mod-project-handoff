# agent-HK-mm-integration.md — mod manager end-to-end wiring (overlay + keymap + loadlist)

Agent HK, 2026-10-10, l25-prep round-7. Mac-only: python edits + offline
tests; no builds, no console. Retry after a provider outage; no prior HK
progress existed.

## Files changed (mine only)

- `outputs/2026-10-08-l25-prep/mod-manager/modmanager.py` (1.0 -> 1.1)
- `outputs/2026-10-08-l25-prep/mod-manager/test_modmanager.sh`
- `outputs/2026-10-08-l25-prep/mod-manager/DESIGN.md` (section 10 appended)
- NOT touched: `pak-overlay/overlay_builder.py` (HI's, extended concurrently),
  `pak-overlay/mod-manager/INTEGRATION.md` (GK's). The manager couples to
  the builder ONLY by subprocess (stable JSON-out CLI contract).

## The one-command flow

```
python3 modmanager.py install <mod-dir-or-zip> --enable
python3 modmanager.py deploy            # ONE unified plan document (dry-run default)
python3 modmanager.py deploy --apply --i-know-console-is-on
```

1. `install` now recognizes a third source shape — an IoStore asset dir
   (.utoc+.ucas, no lua). Staged byte-exact (never-edit law, sha-proven
   for binary assets); preflight records NOT-APPLICABLE. Mods carrying
   .pak/.utoc/.ucas get an OVERLAY ADMISSION dry-run:
   `overlay_builder.py --paks-dir <game_dump_root> --index <overlay_index>
   --state-dir staging/overlay-state install --mod <Mod>--<stem>=<store>.utoc
   --dry-run`. The builder's plan/ledger JSON is captured verbatim into
   `receipts/install-<ts>/overlay-plan.json` + a manifest `overlay`
   verdict (PLANNED/REFUSED/UNAVAILABLE). Refusal at install = WARN +
   record (blocks only when enabled). Asset files NEVER get loadlist
   rows / ue4ss uploads (scope law unchanged).
2. Keymap wiring completed: after EVERY install/enable/disable/uninstall
   the manager runs FV regen AND `--check` (violations abort the state
   change); pad-size drift re-pins the loadlist pad row in the same step.
3. `deploy` (dry-run) prints + saves ONE `deploy-plan.json` receipt
   document: ordered uploads (loadlist LAST) + overlay journal plan
   (desired/journal mods, derived-index/sidecar shas, verify verdict) +
   pad (exact re-pinned MFA row) + loadlist (base provenance) + rollback
   refs (receipts/last prev pair + overlay journal backup path). Never
   PRX/settings.
4. Conflicts block deploy end-to-end, both families, before any upload
   planning: FV shared-key conflicts (--allow-cofire) and builder
   package-ID conflicts `mod_conflict_rejected` (--allow-overlay-conflict
   = reviewed later-wins), plus journal STALE/DRIFT refusals with
   reconcile remedies. New config keys: `game_dump_root`,
   `overlay_index`, `overlay_state_dir`, `overlay_builder`, optional
   `overlay_base_ucas_size`/`overlay_base_container_header`; CLI
   overrides --game-dump-root/--overlay-index.
5. `--apply`: after console uploads + readbacks, publishes the overlay
   LOCALLY against the dump mirror with a determinism sha check
   (real == dry-run plan) + verify OK. Console shipping of the overlay
   trio stays Phase B (builder doc), stated in the plan output.
   audit gains a builder list/verify section (pending-removal,
   STALE/DRIFT, REFUSED warnings); `list` gains an overlay column.

## Tests — 88/88 green (grew honestly from 49)

`bash test_modmanager.sh`: PASS=88 FAIL=0, twice consecutively, all
offline (probe only against 127.0.0.1:9). T1-T15 semantics unchanged
(49 checks still green; T13's loose `mfa|MFA|refused` grep tightened to
the real `REFUSED` marker — the unified plan legitimately prints an
informational "MFA row re-pinned" line). New T16-T18 (39 checks):
synthetic dump + 3 IoStore trios built via the builder's OWN fixture
functions (synth_base/synth_mod, distinct salts; shared replace index =
real conflict); e2e pak-mod install (sha law, receipt capture, zero
asset rows, dump mirror untouched, unified doc, scope law); keybind Lua
mod composed in one deploy (check-after-state-change, pad re-pin,
deploy-plan.json, audit clean); conflict refusal end-to-end (install
WARN + manifest record -> deploy rc=2 naming mod_conflict_rejected +
remedy before upload planning -> later-wins composes both -> disable
recovers). python3 -m py_compile + bash -n clean on everything.

## Notes / honest limits

- Mid-run, HI's concurrent extension left overlay_builder.py transiently
  broken (`args.classic_pak` before argparse); I did not touch it —
  waited for their edit to land (21:42) and the suite passes against
  the extended builder. The manager surfaces any builder rc!=0-without-
  JSON as a hard "error" refusal with stderr — no blind retries.
- The apply path (console FTP + local overlay publish) remains
  unexecuted against real hardware (console documented OFF; guard
  enforces). Determinism, gates, plans, receipts, and state machine are
  test-proven offline.
