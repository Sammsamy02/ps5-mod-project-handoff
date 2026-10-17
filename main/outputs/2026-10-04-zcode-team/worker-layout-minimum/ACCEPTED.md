# ACCEPTED.md — acceptance ledger for Townfall minimum layout data

**STATUS: EMPTY BY DESIGN.** Nothing is accepted. No in-process predicate has
run against the live Townfall game image for any value in
`candidates/layout-candidates.profile.json`. Every candidate remains a
candidate (Windows/port-template or survey derived, origin recorded) until the
RUNBOOK passes in-process and a row is appended here with evidence.

Do not pre-fill, do not copy candidate values here, do not re-rate unchanged
evidence. A row may only be appended after its full predicate chain returned
`Validated` inside the game module (or on a frozen span dump with both the
compiled probe and the host mirror agreeing).

## Ledger schema (one row per accepted value)

| Field | Meaning |
|---|---|
| `entry_id` | stable id, e.g. `TUObjectArray::Objects` or `callable:UGameEngine::Tick` |
| `kind` | `member_offset` \| `total_size` \| `structural_constant` \| `compiled_width` \| `file_image_label` \| `callable` |
| `value` | the accepted number (decimal or 0x hex) |
| `verdict` | `Validated` (worker-5 vocabulary; Refuted/Inconclusive never enter) |
| `hypothesis` | `A`/`B` for GUObjectArray-dependent entries, else `n/a` |
| `rationale_codes` | exact codes observed (e.g. none, or supporting classifier codes) |
| `evidence` | verdict record (JSON pointer or inline), `span_digest`, `image_base`, image sha `3b6cfad4…22fc` check result |
| `agreement` | compiled `tlp` probe vs host mirror result (both must be Validated and agree) |
| `identity_facts` | engine_revision/build_id/axes in force when accepted |
| `rung_served` | first ladder rung this value unblocks (1-5) |
| `accepted_at` / `accepted_by` | timestamp and controller identity |

## Rows

(none)

## Explicitly NOT accepted (tracking list)

- All member offsets in `candidates/layout-candidates.profile.json` (incl.
  TUObjectArray/FUObjectArray/FUObjectItem/UObjectBase/FField/FProperty/
  UStruct/UClass/UFunction).
- FName construction (separately-tracked gap): wchar ctor call-site label
  `0x23d590` is a candidate; no construction proven.
- Callable status of any file-image label (`0x2fac8c0` Tick, `0x490aa0`
  StaticConstructObject, `0x93ba110` GUObjectArray singleton,
  `0x9350950` GMalloc candidate): labels are validated-static at best, never
  proven called/mapped here.
- Staged-pair identity (EngineRevision/BuildId/axes): must come from the live
  scan; currently unresolved everywhere.
