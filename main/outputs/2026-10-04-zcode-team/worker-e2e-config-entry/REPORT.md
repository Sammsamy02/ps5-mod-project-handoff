# REPORT — worker-e2e-config-entry (FINISH pass), 2026-10-04

Task: close out the partial deliverable set from a prior attempt that died
mid-work. The original goal stands: worker-e2e-staging-final found the
D1-fixed payload sha `e3489297…` has NO e2e-cmd.json reader, so RUN-CONFIG B
(config-file dispatch) had nothing to run against. This worker delivers the
config-entry revision DESIGN + host validation so an implementer can build the
new payload without re-deriving anything.

## What survived from the prior attempt (verified, reused — not redone)

- `CLAIM.json` skeleton (design decisions; two claims proved FALSE on
  verification and were corrected — see below).
- `owned/modinst_core.c` (612-line config-entry addition; complete and
  well-formed against `E2E-CMD-SCHEMA.md` sec 1-6 and the staging harness).
- First-pass build artifacts (.o, censuses, compile log).

## What FINISH found and fixed

1. **Import-census defect (would have busted the delta-zero target).** The
   prior .o introduced `+_strtol`; plain `strtol` is NOT among the console
   payload's 116 imports. Replaced with a bounded sign-aware saturating
   decimal fold; during the first host run this exposed a second bug — the
   fold loop did not advance its cursor, so `"schema":1` saturated (caught as
   `schema must be exactly 1` on every staged file). Fixed, then proven with a
   standalone parser harness (1/12/-1/0/007/huge all match the harness `int()`
   observably).
2. **"0 warnings" claim was false.** The compile emits the same 3 pre-existing
   warnings as the D1 receipt itself (lines 643/1365/1379, untouched code).
   Corrected everywhere: 3 pre-existing, ZERO new.
3. **Patch/, host-test script, receipt, design doc, report did not exist.**
   All authored this pass.

## Final evidence (all host-side, this session)

- Pristine base `45c04d08…` → owned `43ea66c1…`; `patch -p0` of
  `patch/modinst_core-config-entry.diff` onto pristine reproduces owned
  BYTE-EXACT.
- .o compiled with the exact D1 `host-build.json` argv: `nm -u` delta vs
  pristine .o EMPTY (70=70); linked binary import set vs the pristine D1
  binary EMPTY (104=104), no `strtol` anywhere.
- `host-test/host_cmd_entry_patched_test.py` → `HOST-TEST-RECEIPT.md`
  **OVERALL: PASS (32 checks)**, driving the COMPILED-IN entry of
  `modinst_composed_cfgentry` (`4659c24c…`):
  - bare dispatch with the `/data` slot absent → exit 2 + refusal line +
    usage, no state created (console-harmless);
  - 15-step stage 0..6 replay over the 13 staged cmd files (SHASUMS
    re-verified OK) through the entry: all exit 0, consumed-rename each,
    transcript byte-identical to staging-final's 35-line ledger, sha
    `31a4764a…`;
  - legacy-argv twins on the SAME patched binary: byte-identical to the entry
    transcript AND to staging-final's twin transcript from the PRISTINE D1
    binary → legacy argv unaffected;
  - recover probe; stale re-dispatch → exit 2, zero state change; argv≥2
    without the flag never consults the entry (leftover file untouched);
    valueless `--e2e-cmd` stays legacy grammar;
  - 14/14 refusal cases: 13 × EX_CONFIG 6 + absent-file EX_BAD 2; host-adapted
    refusals append EXACTLY ONE `stage:"e2e-cmd" outcome:"refused"` report
    line, no journal/records side effects, verb never executed.

## Handed to the build worker + controller (CONFIG-ENTRY-DESIGN.md)

- sec 6: census analysis + the honest strtol incident; console 116-import
  re-proof still required.
- sec 7: controller-flagged deltas needing ack or schema amendment:
  EX_CONFIG 6 for present-but-refused (schema §6/§8 say EX_BAD), the refusal
  report line, and the `--e2e-cmd` override slot.
- sec 8: exact build-worker steps (console argv from `console-build.json`,
  patches.json tuple extension `g1-cmd-entry-*` after `r2-install-status-ok`,
  suites re-run, new receipt).
- sec 9: Step P re-pin — replace exactly `CONFIG_ENTRY_SHA` /
  `CONFIG_ENTRY_BYTES` in STAGED-E2E-COMMANDS.md from the new build receipt;
  the identity gate then resolves MODE=B and can never pass with `e3489297…`.

## Honesty

Everything above is HOST-side evidence via the designed `--e2e-cmd` slot. No
console contact: no STOR/readback/consumed-on-console behavior, no suite
re-runs, no new payload ELF, no [TARGET] claims. All writes stayed inside
`outputs/2026-10-04-zcode-team/worker-e2e-config-entry/`; D1 composition and
staged cmd files read-only.
