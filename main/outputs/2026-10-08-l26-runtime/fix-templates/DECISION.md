# DECISION.md — audit verdict → fix-template map (walk-finds-nothing, l26)

Status: CR/CT/CU/CW/CS audit reports had NOT landed under
`outputs/2026-10-08-l25-prep/` when this was authored (verified by listing;
no `*CR*/*CT*/*CU*/*CW*/*CS*` report files exist yet). The map below binds
each audit's POSSIBLE verdict to a template variant so the landing is
mechanical once the reports arrive. Fill `<<AUDIT_REPORT_PATH>>` slots when
they land. Baseline: `agent-CO-walk-audit.md` (landed) — the walk mechanics
themselves are binary/profile/live-PA verified, which is exactly why the
three SURVIVING suspects are the filter's value sources (A/B/C).

## Provisional audit→template feed (bind on landing)

| Audit | Provisional scope (from the dispatch list in the tasking) | Templates fed |
|---|---|---|
| CU | UObject/flags offset audit | A (both variants) |
| CR | class-resolution audit | B |
| CT | type-scan/canonical audit | B |
| CW | walk-mechanics audit | A/B/C (arbitrates between them) |
| CS | string-compare audit + walk harness | C + harness extensions for all three |

## Verdict → template matrix

### If the audit convicts the ObjectFlags (or member-offset) read → TEMPLATE A

| Possible verdict | Apply |
|---|---|
| "HasAnyFlags reads a hardcoded +0x8 that disagrees with the engine's UObjectBase::ObjectFlags" | **A-code** (offset override or accessor routing). A-profile alone is INSUFFICIENT — the profile row is not consumed by this read. |
| "FUObjectItem::Flags / Object / stride (or ClassPrivate/NamePrivate/SuperStruct/TUObjectArray member) profile value is wrong" | **A-profile** (AS/AM five-layer; these reads ARE profile-driven via MemberOffsets). |
| "offsets all verified correct" | A is dead; go to B or C per the other audits; do not land any A variant. |

Evidence bar: an A-code verdict must present engine-binary disassembly
showing the flag read at a different UObjectBase offset, contradicting CO
§2/§3.5's binary pins (`[obj+0x18]` NamePrivate, `[obj+0xC]` InternalIndex).
An A-profile verdict must show the member's true offset with AA-style
receipt (path+sha), and the value must survive the INI total-size guard.

### If the audit convicts the canonical class pointer → TEMPLATE B

| Possible verdict | Apply |
|---|---|
| "UClass::StaticClassStorage (or another of the five) points at the wrong, non-null object" | **B1** identity gate (fail-closed; default). |
| "...and the wrong resolution happened because the scan ran too early" (order window evidence) | **B1 + B2** late re-verification. |
| "wrong resolution proven, and an engine-binary measured address for the canonical is available" | **B1 + B3** pinned seed slot (image-SHA-gated, schema row added). |
| "GetFullName rendering in the slow walk mis-matched (ToString/number-form), so the seed matched the wrong object" | **B4** FName-parts seeding (plus B1 to catch recurrence). |
| "canonicals verified correct by live evidence" | B is dead; go to A or C. |

Mechanism check for the verdict: wrong canonical must be IN every object's
super-chain to exclude everything (e.g. resolves to the `Object` class);
ask the audit for the observed wrong pointer + its rendered name — they go
into values.json `variant_b1_identity_gate`.

### If the audit convicts the class-name compare → TEMPLATE C

| Possible verdict | Apply |
|---|---|
| "NamePrivate stored at a different UObjectBase slot than +0x18" | **C1** profile offset (five-layer). |
| "pool is case-preserving; Equals compares the wrong index slot (display vs comparison id)" | **C2-index** tolerant compare. |
| "string rendering exonerated/condemned — compare must not rely on any index slot" | **C2-string** (console-proven iequal mode; heavier). |
| "numbered duplicate / number-form mismatch survives the Find-first fix" | **C3** (diagnostic-first; behavioral retry only with exact number evidence). |
| "compare verified correct end-to-end" | C is dead; go to A or B. |

Cross-check with CO's ranked list: CO #1 (FNAME_Add duplicate) is the C3
family and is ALREADY mitigated by `MakeFindXOfClassName` in the l26
candidate — a CS verdict re-confirming CO #1 means verifying that landed
mitigation (harness + `[FFO] add-fallback` silence on console), not
re-landing it.

### Cross-audit arbitration (CW's expected job)

| Combined readout | Decision |
|---|---|
| All three audits acquit | Do not land any template; the failure is outside the filter (re-run CO §5 #3 instrumentation: NumChunks/NumElements/first-slot pointer before touching the walk). |
| Exactly one convicts | Land that template alone; run its full verification set. |
| A and C both convict profile rows | Land both profile corrections in ONE digest pass (single `profile_digest` recompute, one suite run) + A-code if HasAnyFlags named. |
| B and C convict (canonical + compare) | Land B1 + C2 together; both are additive guards/compares and compose; harness must cover both fixtures. |
| Verdicts conflict (one names A-code with an offset another audit verified correct) | STOP; reconcile against the engine-binary receipts before landing; never land contradictory offsets. |

## Landing sequence (any conviction)

1. Fill values.json (decimal authority) → 2. Apply the template diffs to
   l26 candidates only → 3. Update CHANGES.json rows (candidate_sha256, and
   digest family if profile touched) → 4. Verification set green
   (harness + suites + build_runtime.py) → 5. Packet per the standing
   approval process; console evidence only via the authorized launch ladder.
