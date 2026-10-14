# Review fixes folded in (no review of a review)

Leon's result did not change from any fix: 10 parts, 30 derived PS5 textures, all 4 tiling gates pass,
all 10 swaps exact on both decoders. The tests that prove each fix are in `tests/test_textures.py` and the
adapted `tests/test_substitute.py` (saved output: `tests/output/`).

## Review 1

| # | Fix | Where | Proof |
|---|---|---|---|
| 1 | Importers that do not route the texture are walked through their imports, across the merged mod + base stores. Any package with shader maps refuses ("can draw"), and so does an unresolved package or a walk that goes too deep. paktx builds the importer set from **all** installed mods. The test now requires the exact code. | `derive_substitutes.can_draw`, `paktx._can_draw` + `verify_texture_substitutions` | X04 `test_drawing_importer_refused_by_paktx_too` → `substitute_texture_importer_can_draw`; X04 generator case "can draw" |
| 2 | The GPU proof must decode exactly `len/block` blocks. Its count of texels where red ≠ blue must track the reference decoder's (tolerance 0.5% for BC1-3 rounding, at least 16). Checked at derive time and again in paktx. | `derive_substitutes.gpu_proof` / `decoded_agree`; paktx proof check | X02 `test_gpu_proof_claim_false`; real data (e.g. 4135393 vs 4135398) |
| 3 | The claim is reworded: exact for the two decoders used. Any decoder can differ by at most one 8-bit step, only on endpoint-exchanged BC1-3 blocks. | `bcn_swap.py` docstring, NOTES "Open risks" 5 | - |
| 4 | `validate()` also requires the paired default's virtual-texture status to match the bound textures'. Every reparented row records `slot_proofs` (master, default texture, virtual flag, cross-checked slots). | `material_defaults.validate`, `reparent_plan` (`slot_default(..., info=True)`) | U05 shifted pairing refused; plan rows show e.g. `MRO -> Default_MRO, virtual False, 4 slots` |
| 5 | **Not changed.** The sRGB flag belongs to the texture resource, and PC sampled the same texture with the same flag. So the shader sees the same values as on PC. Decoding Leon's unversioned texture properties would have only one source. | NOTES "Open risks" 4 | - |
| 6 | Hair statistics use mip 0, located by the two-source layout reader. PS5-form textures are de-tiled first. A cut-out also needs ≥ 95% near-binary texels. | `hair_mask_check.mip0`, `channel_stats` | U08 (statistics cases, real verdict REFUSED) |
| 7 | BC2/BC3: only the case with no exact output under both readings is refused: c0 < c1, the order flips, and index 2/3 is used. | `bcn_swap._colour_block` | U01 (all formats, both decoders), U03 ambiguous block refused |
| 8 | `need(s >= 48)`; `struct.error` becomes `LayoutError`. | `texture_package.layout` | U04 planted corruptions |
| 9 | The Metal source is compiled from its verified bytes copied into the work folder, and the `swiftc` version is recorded. All texture tools are exec'd from verified bytes, never imported. | `derive_substitutes.verified_module`, `gpu_checker` | pins gate (`pin_tools --check`) |
| 10 | The plan's `derive_rules` states that the per-slot proof applies to every slot when `--derive-textures` is on. Default mode is byte-identical to round 4. | `reparent_plan` | real A/B check in `tex_real` |

## Review 2 (final)

| # | Fix | Where | Proof |
|---|---|---|---|
| 1 | SETTLED now needs a **verified** census row with the same block size, the same mode **and the same fit class**: does its size reach the 64 KB and the 4 KB tile on each axis? Sizes whose class the game data does not show are refused, not guessed, even if another rule would agree. All of Leon's sizes are shown. | `ps5_tiling.fit_class`, `census_check`, `choose` | U10: DXT1 256x256 refused; BC7 128x256 refused (manual check); Leon groups unchanged (U09) |
| 2 | **Every schema:** paktx refuses a manifest whose shipped instance imports any package an installed mod provides, unless it ships as a derived texture row. The generator refuses a plan whose shipped rows import mod packages without a planned derivation. This retires the round-3 MIC-only install: the copied round-3 suite now derives its fixture's 3 textures, and the round-3 real dry run is dropped from the gate (`tex_real` covers the real path). | `paktx.verify_substitutions` (end), `derive_substitutes.run` | X05: `substitute_instance_imports_underived_texture`; generator "has no derived PS5 copy in the plan" |
| 3 | The census builder reads the 5 local 4096² game textures from `pakchunk10-ps5.ucas` (on the Mac) and checks them against its TOC (length + IoHash). Rows not verified this way cannot settle a mode. Census: 101 of 101 rows verified. | `game_layout_census.py --local`, `census_check` | U10: unverified rows refused |
| 4 | Verdicts are kept as a list. Any "CONTRADICTS" or "does not fit" fails the whole census. Agreement needs exactly one verdict. | `game_layout_census.py`, `ps5_tiling.census_check` | U10: contradiction refused; two different verdicts do not count |
| 5 | Disclosed: the eyelash group (SW_4KB_S) is marked lower-confidence in NOTES and the launch card, which tells the user to check it closely. | NOTES, LAUNCH-CARD | - |
| 6 | Gate 4 passes only "header and pixels agree", "undecided (texture too uniform…" or "no decodable mip pair" (gates 1-3 always required). The verdict is recorded per texture. | `derive_substitutes._tiling_gates`, paktx | U09 Leon textures through the four gates |
| 7 | An unresolved package means "can draw" (refuse). | `can_draw`, `_can_draw` | - |
| 8 | The instance-import check uses all installed mods' stores. | `paktx.verify_substitutions` | X05 |
| 9 | Hair statistics de-tile PS5-form textures first. | `hair_mask_check.mip0` | - |
| 10 | `sys.path` is saved and restored around the oracle's exec. | `derive_substitutes.load_oracle`, `paktx.load_tiling_reference` | - |
