# MANIFEST-SPEC — composition-change manifest for gate_check_composition5_v2.py

Worker: worker-gate-evolution · 2026-10-04 · proposals + host-tested tooling in
this directory only; the live gate
`outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py`
is untouched.

## Why

A legitimate composition change (Resolution B configured roots, D2 text_base,
composition replacement mechanics) can alter the composed object's undefined
(UND) set and the unit list. Today the gates cannot distinguish LEGITIMATE,
receipted drift from accidental drift:

- The packaging script's extended contract is NOT derived from the composed
  object's UND set: `derive_contract()` in
  `worker-prx-link-gaps/package_prx.py` appends the five HARDCODED
  `EXTENDED_IMPORTS` rows to the pinned 191-import contract and the patched
  Packager harness pins `required_strong == 196`. Any other UND change either
  refuses the build (harness count pins) or fails
  `imports_equal_extended_contract` -> P1 + I1 FAIL.
- The gate's only escape hatch (`IMPORT-DELTA-JUSTIFIED.md`, check I1) accepts
  ANY positive `imports` delta whenever a markdown file exists — it never
  verifies WHICH symbol was added, never allows a removal (`delta > 0` only),
  and is not linked to any unit receipt. That is exactly the rubber-stamp
  shape the gate must never become.

v2 replaces the count-only escape hatch with a per-run, fail-closed manifest.

## Invocation

```bash
python3 outputs/2026-10-04-zcode-team/worker-gate-evolution/gate_check_composition5_v2.py \
  --build-dir <candidate build dir> \
  [--compare-dir <second candidate dir>] \
  --manifest <composition-change-manifest.json> \
  [--baseline <pinned baseline JSON>] \
  --json <receipt>
```

- WITHOUT `--manifest` v2 behaves byte-identically to the live gate (8 checks
  W1 W2 W3 I1 D1 F1 S1 P1, same stdout, same exit codes). Existing receipts
  stay valid. Host-test A proves this on the r11 build dir.
- `--baseline` defaults to `baseline-composition5-r11.json` next to the script
  (frozen r11 combined-object UND set, derived-contract import names, bridge
  external-callee set, identity/D2 evidence strings). A manifest whose
  `baseline_id` does not match the loaded baseline FAILS M0 (fail-closed).

## Manifest format (manifest_version 1)

```json
{
  "manifest_version": 1,
  "manifest_id": "composition5-change-<run>-<n>",      // unique per run
  "baseline_id": "composition5-r11",                   // must equal gate baseline
  "justification": "<why this composition change is legitimate>",
  "design_receipts": ["<REPORT/DESIGN paths backing the change>"],

  "changed_units": [                                   // may be [] for delta-only
    {
      "unit": "<composition unit name>",
      "role": "replace | extra | header",
      "source_paths": ["<optional documentation>"],
      "sha256_after": {"<source path>": "<sha256 after the change>"},
      "receipt": "<path to a replace/extra receipt JSON with a files:{path:sha} map>",
      "justification": "<why this unit changed>"
    }
  ],

  "import_delta": {                                    // EXACT, both directions
    "added":   ["<new strong import name>", "..."],
    "removed": ["<no-longer-imported name>", "..."],
    "evidence": {"<name>": "<sdknm-*.stdout or equivalent provider evidence>"} // optional
  },

  "resolution_b": {                                    // Resolution B only
    "expected_symbols": [
      {"symbol": "<mangled or exact symbol>", "unit": "combined | <build>/<unit>.o",
       "binding": "defined | undefined", "match": "exact | prefix"}
    ],
    "bridge_unchanged": true,                          // false requires bridge_callee_delta
    "bridge_callee_delta": {"added": [], "removed": []},
    "game_identity_facts": {
      "path_untouched": true,                          // MUST be true (see below)
      "evidence_strings": ["<optional extra strings to require>"]
    }
  },

  "d2_text_base": {                                    // D2 option-(c) only
    "guard_strings": ["<honesty-guard message strings that must survive>"],
    "source_pins": {"<changed source/header path>": "<sha256>"}
  }
}
```

## Check semantics (manifest mode)

| id | check | PASS condition |
|---|---|---|
| M0 | manifest valid | `manifest_version == 1`; `baseline_id` equals the loaded baseline; non-empty `justification` |
| M1 | changed units receipted | every `changed_units[i].receipt` exists and parses; every `sha256_after` entry matches the file on disk AND the sha attested in the receipt's `files` map; empty `changed_units` allowed (delta-only manifest) |
| I1 (base, re-targeted) | count | `imports == len(baseline.contract_import_names) + len(added) - len(removed)`; `imports_equal_extended_contract` still PASS |
| I1+ | delta EXACT | with `U` = UND set of `build/combined-supplier.o` (pure-python ELF parse, extended-numbering safe): `U \ baseline.und == set(added)` AND `baseline.und \ U == set(removed)` — additions AND removals itemized; UNDECLARED additions/removals and DECLARED-BUT-ABSENT entries are named in the receipt and FAIL; the derived contract's name set must equal `(baseline.names ∪ added) \ removed` |
| B1 | configured-roots symbols | every `resolution_b.expected_symbols[i]` resolves in the named artifact (`combined` = combined-supplier.o, else `build/<unit>.o`) with the declared binding, exact or prefix match |
| B2 | bridge refuses substitution | external callee set of `build/runtime_start_bridge.o` (call-range relocs PC32/PLT32/GOTPCREL/GOTPCRELX/REX_GOTPCRELX against symbols NOT defined in the object, excluding `.L*`/`.LCPI*` labels and data-address GOT refs `_ZTI*`/`_ZTV*`/`__dso_handle`) equals the baseline set when `bridge_unchanged: true`; otherwise equals `baseline ∪ added \ removed` exactly |
| B3 | game-identity facts path untouched | `path_untouched` is `true` (a composition manifest may NOT rewire the identity path — that needs its own design receipt); every identity evidence string (default: the adapter's "observed executable path is empty; refusing to fabricate game_executable_path" marker) is present in the combined object; the `game_executable_path` string count in the combined object did not regress vs baseline |
| D2 | text_base analog (optional) | every `guard_strings` entry ("Mapped metadata or spans exceed supplied readable bytes", "Mapped text base must be registered readable executable bytes") still present in the combined object — proves the honesty guards were not deleted; every `source_pins` entry exists on disk and its sha256 matches |

Any manifest-mode FAIL, like any base-check FAIL, makes the verdict NO-GO and
exit 1. A manifest section absent (`resolution_b`/`d2_text_base` omitted)
skips B1/B2/B3/D2 — it does not weaken I1+/M0/M1.

## What is NOT weakened

- W1/W2/W3/F1/S1/P1 run unchanged in both modes; P1 still demands the full
  `STATIC_CHECKS_PASSED` set including `imports_equal_extended_contract`
  (which the new derived contract must satisfy).
- D1 still demands real double-build determinism evidence for every new
  candidate.
- The manifest cannot whitewash an undeclared delta: I1+ compares against a
  FROZEN baseline file (sha-pinned inputs, `baseline_id` match enforced), not
  against anything the manifest itself supplies.
- `--baseline` exists so the controller can pin the NEXT frozen baseline
  (e.g. composition5-r12) after a change is accepted; it never accepts a
  baseline shipped inside the build dir or the manifest.

## Baseline file format

`baseline-composition5-r11.json` (frozen 2026-10-04 from
`worker-composition-5-rebuild/prx-candidate-composition5-1`):
`baseline_id`, `combined_object_sha256`, `combined_und` (211 names),
`supplier_contract_sha256` + `contract_import_names` (196 names),
`bridge_call_refs` (40 external callees) + their definition,
`game_identity_evidence_strings`, `game_executable_path_string_count`,
`d2_guard_strings`. Regenerate ONLY for a new accepted lineage, under a NEW
`baseline_id`, with the generator commands recorded in that file's directory
(see REPORT.md "Rolling the baseline forward").
