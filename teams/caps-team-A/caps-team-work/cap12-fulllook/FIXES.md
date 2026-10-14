# Review fixes folded in (no second review, per the rules)

All findings are fixed in `PATCH/tools/`. After the fixes:
- both suites pass: `tests/test_cap12.PASS.txt` 51/51 and `tests/test_fulllook.PASS.txt` 24/24;
- the real Leon result is unchanged (`evidence/reparent-plan-r4-fulllook.json`).

| # | Fix | Where | Test that proves it |
|---|---|---|---|
| 1 | An unreadable mod shading model now refuses **every** parent, with the reason "mod shading model unreadable (reader C)". The integrated T10 fixture gained a reader-C stub. | `reparent_plan.plan` | `R05 test_same_model_hair_and_its_negatives`: hair with reader C off goes to known-checker against both a Hair parent and a DefaultLit parent (the reviewer's probe). |
| 2 | Every `base_color_alpha` slot runs `alpha_ok()`, whatever the mod parameter's own role: masked, an alpha-carrying format (DXT5 / BGRA8 / RGBA8), a proven cut-out, and no other alpha-named texture. `mod_evidence('base_color_alpha')` accepts alpha formats only. | `param_roles.map_params`, `mod_evidence` | `R03 test_colour_alpha`: "Diffuse Alpha" with no cut-out is lost; with DXT1 it is lost; the positive control still maps. |
| 3 | `gate()` checks the packed letters for every mapping, exact or renamed. If the mod texture name or any bound game texture name ends in packed letters, all of them must name the same letters. | `param_roles.map_params.gate` | `R03 test_slot_gate_negatives`: "Mask" (`T_Body_ORM`) on an `X_MRO` slot is lost; on an `X_ORM` slot it maps. |
| 4 | Training skips a row only when it is blocked by an unknown-size property. Any other decode error rejects that index assignment. | `material_props.Decoder._check` | `R01 test_decoder_negatives`: one corrupted nested header means not learned. Real data still learns 12/18/20 and decodes 135/135. |
| 5 | A raw size needs at least `MIN_RAW_ROWS` = 3 rows that really test it (the walk passes it and reaches a present learned field). `rows` reports only those. | `material_props.Decoder._learn_raw`, `_row_ok` | `R01`: synthetic `rows` is 5 (was 8); with only 1 testing row the size is not learned and those materials are unreadable. Real: index 0 is 4 bytes, from 4 testing rows. |
| 6 | The second source for a plain slot is now a real argument: the parent's own default textures **of that slot's class** must **all** be non-virtual (and at least one must exist). A virtual sampler must have a virtual default of its class, so this proves no virtual sampler of that class exists. Any virtual or unknown default of the class means the slot is lost. | `param_roles.slot_gate` | `R03`: a virtual **colour** default makes a colour slot lost; a virtual **linear** default does not affect a colour slot. Real: `M_MRO_Cloth_NonVT` still keeps BaseColour and Normal (its only virtual default is linear). |
| 7 | The colour+alpha route is refused when the mod instance binds another texture whose name says alpha, opacity or mask. | `param_roles.alpha_ok` | `R03 test_colour_alpha`: BaseColor + OpacityMask means BaseColor is refused for the alpha slot. |
| 8 | A per-pixel enum is flagged `field_only` by reader C. A mod special model named only by the bit field refuses the same-model route. | `material_props.Decoder.read`, `reparent_plan.plan` | `R05`: `field_only` hair goes to known-checker, reason "bit field". |
| 9 | The vocabulary's origin is documented in the `part_roles` docstring (ranking only, includes the first test mod's part words). The Leon mention was removed from the `param_roles` comment. | `part_roles.py`, `param_roles.py` | - |
| 10 | Every rule-test gate now counts the assertions the test really made (`Counted` base class); no fixed numbers. Real "coverage beats role" test: a skin part goes to a cloth parent that maps 2 rather than a skin parent that maps 1, with a role-mismatch note. | `test_fulllook.py` | All `[gate]` lines in `tests/test_fulllook.PASS.txt` show counted values. |

Not changed:
- Decoding each slot's own default texture from the master's cached parameters (the reviewer's first option for #6). The class-wide rule above is a valid second source and fails closed, so it was kept.
- The texel cut-out check stays a disclosed heuristic. It is one of three mod-side conditions for #7.
