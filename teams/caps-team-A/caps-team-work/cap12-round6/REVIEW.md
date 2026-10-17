# REVIEW: #12 round 6 (one independent reviewer, verbatim)

Reviewer: one independent general-purpose agent (fresh context), Mac-only, read-only; scratch in
`review-scratch/`. Scope: the shipping logic (`invisible_route.py` round-6 changes) plus the report-only and
not-shipped tools, tests and documents. Verdict: **APPROVE WITH FIXES**. Fixes: see FIXES.md.

---

Review of #12 round 6 (Team A). Mac-only and read-only: no console, network or FTP contact. All scratch files are in `.../cap12-round6/review-scratch/` (probe scripts, `plan1.json`, `plan2.json`, `vf_probe.json`, `strings.json`). Nothing else was touched.

**What I checked and found correct**
- **Determinism:** two fresh `--apply` runs on the real data gave sha256 `0126f476b65d…`, the same as the staged `reparent-plan-r6.json` and the sha paktx binds to.
- **Rows changed:** only 15 rows changed versus the round-5 console plan, and every one of them was `known-checker` before (now on the invisible route). The other 52 rows are byte-equal as JSON.
- **Diff and tests:** the diff reproduces `PATCH/tools/invisible_route.py` (sha `b6aa444b…`), and `work/tools` equals `PATCH/tools`. `test_round6.py` ran 33 tests: 33 pass, 0 skipped.
- **zen.py claim (task item 4) is confirmed.** All 14 multi-export mod packages are SkeletalMeshes.
  - The bundle order is [1,0]; the CookedSerialOffset (CSO) order is [0,1].
  - Read by CSO, the mesh starts with a proper unversioned header (6 fragments, 133 values, then 1.0f). Read the zen.py way, it starts with an empty `0101 00000000` header, so zen.py is wrong for these packages.
  - All 113 single-export mod packages and all 135 game materials read identically both ways, so the round-5 shipping reads are unaffected today.
- **Vertex-factory tokens:** they are length-prefixed, NUL-terminated type-layout names inside the frozen shader map. M_Hide has GPUSkin + Local only. The two R3b masters (`M_skin_unified_baked`, `M_MRO_TranslucentGlass_OneSided_ThinTranslusent`) have exactly GPUSkin + Local: no cloth, no Nanite shaders.
- **Nanite on the other users:** the glasses, IV-bag and cannula static users have `NaniteEnabled=False`. The three skeletal users have `NaniteEnabled=False` and `MorphTargets=0`.

**Findings**

**BLOCKER 1 — the bed-tag row ships without a valid proof.**
- Where: `invisible_route.py:239-242` (proven classes) and `:289` (count of proven users); the result is the row `MI_Simon_BodywarmerBeanieTag`.
- What is wrong:
  - The row is a full `reparent` only through G6. Its one user, `Simon_Bodywarmer_Bed`, is `NaniteEnabled=True` in the asset registry.
  - M_Hide has no Nanite shading shaders (no `TBasePassCS*`). Those shaders are present in 68 of the 69 game materials that Nanite static meshes draw, and in its game original `M_MRO_Cloth_NonVT`.
  - 7,859 of the game's 8,884 static meshes are Nanite. G6 only proves the local vertex-factory path, and Nanite leaves no vertex-factory token, so it is invisible to both G6 and R3b.
  - On the console the tag will most likely stay checker. Nothing gets worse, but the row and NOTES:20 / NOTES:113 claim "fixed".
- Fix:
  - A StaticMesh user counts as proven only if the registry says `NaniteEnabled != 'True'`, or the target's own shader map has `TBasePassCS*` (two sources: registry tag + the target's shader types).
  - In R3b, treat "master has `TBasePassCS*`, target lacks it" as a missing path.
  - Re-run route → derive → paktx dry run. Expected result: the bed tag is refused; 13 full + 1 partial; 24 derived instances; 24 `parent` lines (14 to M_Hide).

**MAJOR 1 — the token pattern misses vertex-factory names starting with "T", so R3b can wrongly pass.**
- Where: `invisible_route.py:105`, used by R3b at `:296-305`.
- What is wrong:
  - `F[A-Za-z0-9]+VertexFactoryShaderParameters` does not match `TGPUSkinAPEXClothVertexFactoryShaderParameters`, which is in 18 of 135 game materials (including `MM_Prop_Basic` and `M_MRO_Cloth_NonVT`).
  - Demonstrated: a master carrying the cloth layout plus a Blueprint-only user gives a full `reparent` (`original-master-vertex-factories`).
  - No impact on this plan, because the two R3b masters have no cloth layout.
- Fix:
  - Read any length-prefixed string containing `VertexFactory` (for example `[A-Z][A-Za-z0-9_]*VertexFactory…`, bounded by the length prefix and the NUL).
  - In R3b, refuse on any vertex-factory string that is not in `VF_PARAMS`.
  - Add a test for this case.

**MAJOR 2 — G6's "learned" second source cannot tell static-capable materials apart.**
- Where: `invisible_route.py:399-423`, NOTES:133-139.
- What is wrong:
  - `FLocalVertexFactoryShaderParameters` is in all 135 game materials, including all 7 skeletal-only ones (M_Hide among them). Learning it with or without the candidate proves only that the name exists in this engine build.
  - The same layout may come from the GPU-skin pass-through vertex factory, which has no separate token in the data. So the data alone cannot show that static-mesh shaders exist.
  - The conclusion still holds only through an engine rule: FLocalVertexFactory is compiled for every MD_Surface material, and G3 already proves MD_Surface for M_Hide.
- Fix:
  - Make that engine rule the stated second source, in code (`static_mesh.sources`) and in NOTES. Keep the learning as a name-consistency check only.
  - Scope the claim: a StaticMesh user is proven only when drawn as a plain static-mesh component. Nanite (BLOCKER 1), instanced, spline and geometry-collection draws of the same mesh are not covered.

**MAJOR 3 — invisible_route and other shared tools still read bytes through zen.py.**
- Where: `invisible_route.py:564`, `:610`, `:643` (`mprops.top_export_bytes`); `texture_package.py:109-111` (`data_offset`); NOTES:148.
- What is wrong:
  - The usage-flag reader, reader C and the mod-parent reader still use zen.py's bundle-order bytes.
  - A future multi-export material or texture would be misread, and this fails open.
  - NOTES:148, "Every export is read by its CookedSerialOffset", overstates: only the vertex-factory tokens and the mesh decoder use CSO.
- Fix:
  - In the route, refuse any package where `own_export(z) != top_export_bytes(z)`.
  - Integrator: make `zen.ZenPackage.export_bytes` return None when the serialize order differs from the CSO order (or switch it to CSO plus the tiling check).

**MINOR findings**
1. **The two proof paths give opposite answers for the same evidence** (`decide_row:287-317`). The user-class path ignores master paths the target lacks:
   - `MI_Simon_Accessories` (master `MM_Prop_Basic`: instanced, spline, geometry collection, cloth) and `MI_Simon_Accessories_Glass` (`MM_Prop_Glass_Cutscenes`: instanced, geometry collection) are full.
   - `MI_Simon_IVBagExterior_Back`, with the same kind of instanced-mesh gap in its master, is partial; `BeanieTag_Deluxe` is refused.
   - Fix: compute `original_master` for every overridden row and write the missing paths into `look_notes` (strict option: mark such rows partial).
2. **R3b checks mod users on the wrong graph** (`:752`, `:298`). Mod packages reached through a game child instance are counted as users but are "covered" by the game master, which says nothing about mod packages. A missing `mod_user_classes` key also means "no mod users". No impact today: no user walk passes through an instance node (verified). Fix: tag mod-package users in the same `effective_users` walk, and refuse when the key is missing.
3. **R3b ignores runtime usage checks that have no vertex-factory layout** (morph targets, clothing usage, static lighting). No impact here: the skeletal users have `MorphTargets=0`, and the game has no lightmap shaders. Add it to the open-risks list.
4. **The token pattern is not anchored** (`:105`): `b'FooFLocal…'` yields `'FooFLocal…'`. Harmless with the real layout, but anchor it to the length prefix.
5. **`base_material_route.py:117-120` returns `placeholder` when no section uses the material at all.** That is not proven (the material could be an overlay or a per-LOD override). It should be refused / "no-section". Report-only.
6. **`mesh_sections.py:389-396` and `:411-415` take the first LOD header that parses**, not a unique, exactly chained one; LODs whose geometry is not inline get no cross-check. All the mod meshes have LOD0 only, so this is fine now; tighten it before using it on multi-LOD meshes.
7. **`mesh_slot_repoint.py:63-66` (not shipped)** only checks slot uses of the carrier import. Any other reference to it inside the export (for example an overlay material or a LOD override) would be silently re-pointed. Fix: scan the export for the carrier index outside the slot table.
8. **Report-only code runs inside the shipping tool and writes into the plan.** A wording change alters the plan sha, and a crash there blocks the plan. Move it to its own tool, or wrap it so it can never touch rows.
9. **Wrong statements in the documents:**
   - LAUNCH-CARD:30 says "glasses, IV bag front, face part" became fully proven. Team B's 4 partial rows were face LOD1, Accessories, Accessories_Glass and M_Medical_Liquid; all 4 are now full, and the IV bag front was refused in Team B's run, not partial.
   - NOTES:155 says the cannula was "never partial"; it was partial in `baseline-tb`.
   - NOTES:20 says "7 face/cutscene parts"; there are 6 rows.
   - NOTES:46 says the face placeholders are "inside the head"; the team's own test says they are not enclosed.
   - Correct the bed tag text everywhere (BLOCKER 1).
10. **The package was incomplete at review time:**
    - `tests/output/` is empty, though NOTES:175 and NOTES:200 cite `run.log` and the repoint receipts there.
    - `evidence/plan-r6/paktx-dry-run.txt` is 0 bytes; a `paktx --dry-run` was still running at 17:25.
    - `PATCH/SHA256SUMS` does not exist, so `run_tests.sh` fails at step 0.
11. **Test gaps:**
    - No unit tests for `original_master()` (2 material imports, cycle, depth limit, unverified master, unreadable export, empty token set, not an override).
    - No tests for a T-prefixed or unknown token, or for a Nanite user.
    - No R3b tests for zero users, a truncated chain, an "unknown" mod user, or a missing `mod_user_classes` key.
    - No test that `own_export` picks the CSO bytes when the bundle order differs, and none for the 4-byte tail rule.
    - `test_vf_tokens_reader` uses strings without length prefixes, so it passes regardless of the real layout.
    - The synthetic training set has LOCAL in every row, so no test can notice that the static token is not discriminating.

**Verdict: APPROVE WITH FIXES.** Fix BLOCKER 1 and MAJOR 1 before the launch, then re-run the plan, derive and paktx dry run and update the launch card. MAJOR 2 and 3 need code or wording changes. None of the findings can make the console worse than round 5: at worst a part keeps its checker.
