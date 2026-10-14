# REPORT.md — worker-e2e-staging-final

Date: 2026-10-04. Role: prepare the exact console E2E staging commands using the
D1-fixed installer payload and the config-file-driven entry design. Budget used:
~14 of 40 calls. CLAIM.json written first; this file last.

## Deliverables (all under outputs/2026-10-04-zcode-team/worker-e2e-staging-final/)

- `STAGED-E2E-COMMANDS.md` — the literal, in-order controller sequence: pre-flight
  cmd-file verification, Step P payload staging + identity gate, dispatch sequence
  D0/0 → 1 → 2a → 2b(×3) → 3 → 4 → records → 5 → records → 6(×3) → closing records,
  recover block, Step C evidence collection, full-tree reset, corrected line ledger.
- `cmd/` — the 13 dispatch files, byte-exact compact schema §4 JSON (no trailing
  newline), with `cmd/SHASUMS.txt` (verified with `shasum -a 256 -c`: 13/13 OK).
- `host-test/host_cmd_entry_test.py` — host-side emulation of the entry rule:
  bounded JSON-subset parser + schema/allow-list/deny-list validation (schema
  §2/§3/§5/§6) + argv synthesis, driving the REAL host binary.
- `host-test/HOST-TEST-RECEIPT.md` (+ `host-test.log`, scratch transcripts,
  refuse-cases/) — OVERALL: PASS.

## Key findings

1. **Payload gate (governing constraint).** The D1-fixed payload
   `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436` (681528 B,
   sha/size recomputed this session; D1+F-R1+F-R2) contains NO `e2e-cmd.json`
   reader — grep of the vendored sources is negative and no implementing revision
   exists anywhere under the team outputs. The config-file entry is DESIGN ONLY
   (E2E-CMD-SCHEMA.md is explicit about this). Therefore STAGED-E2E-COMMANDS.md
   encodes a hard identity gate in Step P: MODE B (config entry) runs only when
   the staged bytes equal the FUTURE revision's re-pinned sha/bytes; against
   `e3489297…` the gate resolves MODE=A (argv per the G1 probe grammar) and the
   same stage order/expected lines apply without cmd files. It can never bare-
   dispatch the current payload.
2. **CORRECTION to CONSOLE-E2E-EXECUTION.md stage 6 (evidence-backed).** The doc
   expects all three removals to emit NO status line (33-line ledger). The cited
   zb-pak transcript's single removal was of the ONLY mod (silent because the set
   empties). Host replay of this exact dispatch set shows removals 1–2 emit the
   F-R2 status line with detail = remaining mods (`movspeed,mm-skin`, then
   `movspeed`); only the final removal is silent. Corrected all-green ledger:
   **35 lines** (35/35 verbatim in the host replay). STAGED §4.6/§4.7 carry the
   corrected blocks plus the divergence note so the controller neither aborts on
   a false mismatch nor accepts a real one silently.
3. **Host test results (task 3).** With the entry not compiled into any binary,
   the test proves the design host-side: (A) all 13 staged console files validate
   against the bounded grammar; (B) the exact synthesized argv vectors drive the
   real host binary `worker-d1-chain-relink/composition/build/host/modinst_composed`
   (arm64, sha `8dffe3cf…`) through the full sequence, exit 0 everywhere, 35-line
   transcript sha `31a4764a4ae13afac76db4e50e0905789a6b07294acb5943ebd2affb3b5ebeed`;
   (C) each dispatch byte-identical to its hand-written argv twin (schema §8
   item-3 behavior); recover on a clean tree = begin/end `recovery` exit 0;
   (D) 14/14 refusal-matrix cases refuse (unknown keys, deny-listed verb/option,
   schema≠1, mods+remove, mods on records, \uXXXX escape, oversize, bool-false,
   eager per-verb option check, relative path, unknown verb, absent file).
   Fixtures used the real corpus (movspeed.zip `3850a2b5…`, mm-skin `7b9888b9…`,
   mm-lua `eefc774a…`); all host writes confined to `host-test/scratch/`.

## Honesty / bounds

No network, no console dispatch, no nested subagents. The host test is a faithful
EMULATION of the entry rule (validator + argv synthesis) feeding the real binary
parser/dispatch — it is not the console entry compiled in. Console-side results
(STOR/readback/consumed-rename, all expected lines) remain [TARGET] until the
controller run, which for MODE B additionally requires the config-entry revision
to be implemented, re-pinned, and its suites re-run per schema §8. All other
files read-only; nothing outside this worker's outputs directory was written.
