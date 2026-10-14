# ADMISSION — worker-r3-ladder staged unit into the composition

Worker-r3-ladder, 2026-10-05. Proposal only; the controller composes after the
current candidate cycle. Shape follows worker-initializer-integration
`REPLACE-ARGV.md` and the l11 receipt discipline (`--replace-runtime-unit` +
`--replace-receipt`; `luamod` ∈ RETAINED_REPLACEABLE of the fixed l11 driver,
`build_runtime_composition6_fixed_l11.py:92`).

## The one-line delta vs the current l11 compose argv

Keep the proven l11 argv verbatim and redirect/re-add exactly these two flags:

```
  --replace-runtime-unit luamod=outputs/2026-10-04-zcode-team/worker-r3-ladder/staging/luamod-l11-r3-ladder/LuaMod.cpp \
  --replace-receipt outputs/2026-10-04-zcode-team/worker-r3-ladder/receipts/replace-receipt.json \
```

(Replace any earlier `--replace-runtime-unit luamod=…` line pointing at
`worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` or the older lineage
stagings; `--extra-*` / `--replace-bootstrap-*` / other receipts / defines stay
verbatim. Fresh run dir required. Serialized heavy build.)

## What the flags do here (driver facts, fixed l11 driver)

- `luamod` is in the driver's RETAINED_REPLACEABLE allow-list, so the retained
  replacement path applies: the source MUST be covered by the
  `--replace-receipt` attestation (it is: `files` map in
  `receipts/replace-receipt.json`, single file).
- No `--extra-unit` is needed: this is a pure replacement of one existing
  runtime unit's source (same 172-token frozen compile argv, only
  source/-o/-MF redirected — exactly what this worker's compile gate ran,
  `work/argv-r3-ladder.json`).

## Comparators for the compose acceptance

| check | expected value |
|---|---|
| staged source sha256 | `6f97f8a1727b7e49d32a43041288bfb1f8772a866067d748752f950aafc955cd` |
| revision diff sha256 | `29535f023c0105f3cc6ade6068423327b482e1526c9073d4f777c4eaf3bcb8ab` |
| base (current tree) sha256 | `c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0` |
| `luamod.o` under the frozen argv | `761b610a20e9d6cbac494976f907cbd0bd053e21005a675e2d7339cf31cd8941` (path-independent, re-measured twice; 7,028,928 B) |
| control baseline for the DRIFTED tree | `8343578bfa735843189165ff6ed96d5721c53ae586680f5ec331890fb8044de` (c92c92ff under the same argv; NOT the merge worker's `813b249d` — that belonged to pre-drift `dc687602`) |
| UND delta vs control | EMPTY (540 = 540; bridges already UND in lineage) |
| warnings | 0 errors, 24 warnings both sides, exact warn-kind parity |
| composed final object | NEW sha — do NOT reuse any prior comparator; the controller's pass measures it |

## Pairing law (unchanged, restated)

The staged unit keeps the DIFF-1/DIFF-2 rev-B pairing contract: the three
`Unreal::R3MarshalPumpPending` / `R3MarshalPublishGameThreadIdentity` /
`UFunction::GetFuncPtr` references must RESOLVE at final link. They resolve
against the ARMED-fold initializer (`UE4SS_R3_MARSHAL_MODE=1`).
`launch11-combined/units/UnrealInitializer.cpp` (`e7f349af…`) has the fold at
:178-179 (`#define UE4SS_R3_MARSHAL_MODE 1` default). If either bridge symbol
is UNRESOLVED at final link, the initializer fold is wrong — stop; that is the
rev-B pairing law refusing loudly.
