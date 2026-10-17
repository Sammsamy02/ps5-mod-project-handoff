# RECONCILIATION — fresh r11 gate verdict pair vs cycle receipt vs stale r5 archive

Worker: worker-fresh-r11-gate-receipt · 2026-10-04 · read-only on build dirs.

## Inputs

- Build dir: `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/prx-candidate-composition5-1`
- Compare dir: `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/prx-candidate-composition5-2` (STILL PRESENT — two-dir D1 was run; the NOT-RE-RUN fallback did not apply)
- Cycle receipt: `outputs/2026-10-04-zcode-controller/console/cycle-receipt.json` (utc `2026-10-05T01:47:03Z`)
- Stale archive under challenge: `outputs/2026-10-04-zcode-team/worker-composition5-gate/gate-receipt-composition5-r5.json`
- Gates run (cwd `what/`):
  - `python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py --build-dir <candidate-1> --compare-dir <candidate-2> --json <this dir>/live-r11-receipt.json` → exit 0
  - `python3 outputs/2026-10-04-zcode-team/worker-gate-evolution/gate_check_composition5_v2.py --build-dir <candidate-1> --compare-dir <candidate-2> --json <this dir>/v2-r11-receipt.json` → exit 0 (no `--manifest`: parity mode)

## Verdicts

- live gate: **GO (offline gate)**, `failed_checks: []`, 8/8 PASS (W1 W2 W3 I1 D1 F1 S1 P1)
- v2 gate: **GO (offline gate)**, `failed_checks: []`, 8/8 PASS, same check outcomes
- Receipts are identical content-wise; v2 adds only provenance fields `gate_variant:"v2"`, `manifest:null`, `baseline:null` (confirms parity mode, no manifest).

## Bare-PRX / staged identity each party sees

The gate resolves "bare" to the FIRST existing bare candidate: `bare-original/modbridge_supplier_composition.prx` (gate_check_composition5.py `find_prxs`, line ~290). "Wrapped" is the top-level fself.

| Artifact | Cycle receipt (console staging) | Current dir (candidate-1 = candidate-2, independently hashed) | Fresh r11 gate receipts |
|---|---|---|---|
| staged fself `modbridge_supplier_composition.prx` | `staged_sha256 = 0911af0cd5b4a1f2…537cab9` | `0911af0cd5b4a1f22eac49a79f8b3c9a3d9cac7358b246e16ae2a969b537cab9`, 30,792,151 B — EXACT MATCH, both candidates byte-identical | P1 corroborates via `verification-evidence/static-checks.json` `candidate` entry (same path/sha); file independently re-hashed by this worker |
| bare-sdk | `bare_sha256 = a0df5d9be720c2a0…3e70aa` | `a0df5d9be720c2a0838382cdcbebc1a316be3dfd6c14ea07682e6353ea3e70aa`, 30,774,360 B — EXACT MATCH, both candidates | n/a (gate hashes bare-original, see below) |
| bare-original (gate's "bare") | not recorded | `7774b663310cf3eaf65b999bf42ba4c27e558bccbecd94aec1cdd7f00d36dcc7`, 30,774,360 B, identical in both candidates | D1 sha1 = sha2 = `7774b663…dcc7` (candidate-1 vs candidate-2 identical); S1 bytes 30,774,360, sha `7774b663…dcc7` |

## Reconciliation outcome

1. **r11 staged identity confirmed**: the cycle receipt's `staged_sha256` equals the top-level fself in BOTH candidate dirs today. The live staged build is what the gates just passed. No drift between cycle receipt and dir.
2. **Bare-variant naming, not drift**: the cycle receipt's `bare_sha256` is the `bare-sdk` variant; the gates hash the `bare-original` variant. Both variants exist on disk in both candidates at 30,774,360 B and each matches its respective recorded sha exactly. Every sha in every receipt resolves to a current file.
3. **LOUD NOTE — `readback_len = 30,772,344` matches NO current file size** (wrapped 30,792,151; bare 30,774,360). It is 2,016 B short of the bare size and sits below the receipt's own `e_shoff = 30,772,632`, i.e. the console readback window ended before the section-header table. SHAs are the identity match and they match exactly; `readback_len` is a partial console readback length, not a full-file transfer size. Recorded here as an observed fact; no local artifact disagrees with any recorded sha.
4. **Stale r5 archive drift characterized**: `gate-receipt-composition5-r5.json` records bare sha `3695d1009d20a3ef…932f82d` at 30,772,008 B. NO file in the current dir matches either value — it describes the pre-rebuild dir state, exactly as worker-gate-evolution's side-finding claimed. It is superseded by this fresh pair (same build_dir path, current contents, both GO).

## D1 status

Ran as a real two-dir determinism check (candidate-2 present): sha1 == sha2 == `7774b663…dcc7`. The `--baseline-bytes`-only fallback and the "D1 NOT-RE-RUN-here" note were NOT needed.

## Recommendation

Move `worker-composition5-gate/gate-receipt-composition5-r5.json` (and, for the same reason, any other receipt whose recorded shas no longer resolve on disk — r2/r3/r4 should be checked by whoever owns that dir) into an `archive/` subfolder alongside a pointer note: "superseded for prx-candidate-composition5-1 by worker-fresh-r11-gate-receipt/live-r11-receipt.json + v2-r11-receipt.json (r11 staged sha 0911af0c…, bare 7774b663…), 2026-10-04". Preserve raw provenance; do not edit the r5 file itself.
