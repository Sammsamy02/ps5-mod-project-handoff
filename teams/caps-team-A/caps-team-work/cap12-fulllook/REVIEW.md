# Independent review: #12 full look (one reviewer, Mac-only)

**Verdict: APPROVE WITH FIXES.**

The reviewer worked read-only (no edits to the patch, no console). Both test suites passed as submitted: `test_fulllook` 24/24 and `test_cap12` 51/51. The Leon outcome was confirmed as sound:
- 10 instances go to `M_MRO_Cloth_NonVT`;
- BaseColor is renamed to BaseColour;
- Normal is kept exact;
- ORM is lost;
- the hair is refused.

The reviewer found three fail-open paths. **Leon's data does not reach any of them**, but other mods could. The reviewer tried fixes on a private copy: the real-data result was unchanged and the real-data tests still passed.

## Findings (as reported)

1. **BLOCKER — unreadable mod model.** When reader C cannot read the mod master, no special model is detected, so every non-special parent was allowed. Readers A/B never see the shading model, so this accepted missing evidence. Probe: Leon's hair with reader C off went to a DefaultLit masked parent. *Fix:* refuse every parent when the mod model is unreadable.
2. **BLOCKER — colour+alpha gate bypass.** A mod parameter that is itself named colour+alpha ("Diffuse Alpha") skipped the masked + cut-out check. `mod_evidence('base_color_alpha')` also accepted DXT1. *Fix:* always gate `base_color_alpha` slots, and accept only formats that carry alpha.
3. **BLOCKER — exact names ignore packed letters.** Probe: a mod "Mask" bound to `T_Body_ORM` mapped exact onto a game "Mask" bound to `X_MRO`, so ORM would be read as MRO. *Fix:* for every mapping, the packed letters in the texture names must agree.
4. **SHOULD.** Reader C training skipped any undecodable row silently, not only rows blocked by an unknown property. A corrupt row could not stop learning. *Fix:* skip only rows blocked by an unknown property; any other decode error rejects the assignment.
5. **SHOULD.** A raw property size could be learned from a single testing row, and the `rows` field over-counted. *Fix:* need at least 3 rows that really test it, and report only those.
6. **SHOULD.** The "second source" for a plain slot was any non-virtual default of the same class. That proves nothing about this particular slot. *Fix:* tie it to the slot, or label it honestly.
7. **SHOULD.** Only a heuristic shows that the colour alpha is the cut-out. *Fix:* refuse when the mod instance binds another texture named alpha, opacity or mask.
8. **NIT.** With a per-pixel model enum, the mod model rests on the bit field alone. *Fix:* disclose it, or refuse the special route.
9. **NIT.** The part-role vocabulary contains Leon's part words; document its origin. A comment in `param_roles.py` named Leon.
10. **NIT (tests).**
    - The "cloth maps MORE" test did not really test "more mapped beats role".
    - Many gate lines reported fixed numbers.
    - Negatives for findings 1-3 were missing.

## Verified correct by the reviewer

- **Ranking:** coverage always first; a worse-covering parent cannot win.
- **Translucent parents:** excluded twice, by the class gate and by the blend-group check.
- **Leave-one-out:** the chain includes the master, so a material never trains its own reader.
- **Bit names:** shading-model bit names cannot come from the candidate's own registry entry.
- **Virtual textures:** a virtual texture fails the plain-mip proof.
- **DXT5 alpha:** the decode matches the BC3 specification.
- **Zero mask:** the zero-mask sizing matches UE5.

How each finding was folded in: `FIXES.md`.
