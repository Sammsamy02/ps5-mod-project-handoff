# agent-GM — Leon glasses cause + generic PC-shader fallback

2026-10-08, Mac-only offline. Deliverables (created; nothing else touched,
`.superpowers` read-only, no console/build/network):

- `shader-fallback/fallback.md` — full forensics + design
  (sha256 `5a26733f…1d675874`)
- `shader-fallback/classify_materials.py` — install-time analyzer prototype,
  pure stdlib, 15 pinned checks must reproduce or it hard-fails
  (sha256 `ba782601…cfecacd2`)
- `shader-fallback/leon-classification.json` — its verified output
  (sha256 `50cedd27…37b9160`)

## Cause verdict: STRONGEST-HYPOTHESIS at file level (not confirmed)

Files now prove, beyond the observation's UNCONFIRMED status:

1. The mod's own container header (parsed end-to-end, 127 packages) has
   **zero shader-map hashes**; the base pakchunk0 header has 5,387 across
   38,439 store entries; the old PS5 glasses entry's hash
   `233d367d…97cddf4` and its 2 imports (`MM_Prop_Glass_Cutscenes` + grunge
   bulk) reproduce exactly. The published replacement is hash-less and
   imports the new PC parent — source-replacement semantics re-verified.
2. `M_Invisible`'s 1.66 MB payload is genuine D3D shader code, not token
   coincidence: `DXBC` magic + `SFI0`/`ISG1`/`OSG1` container chunks
   (216/170/… hits), `PCD3D_SM5`/`SM6` name records, zero PS5 tokens. So the
   engine has **neither** of its two shader sources for that material on
   PS5: no store hash (library lookup) and no SP_PS5 inline data. A missing
   hash alone is NOT diagnostic (33,222 base entries, incl. all 24 overlap
   meshes, are legitimately hash-less) — the pair of absences is.
3. The engine's fallback targets exist natively: `WorldGridMaterial`,
   `DefaultMaterial`, `DefaultDeferredDecalMaterial`,
   `RemoveSurfaceMaterial`, and game-side `M_FluxSurfaceInvisible` are all
   in the base TOCs, each with a PS5 store hash — consistent with a pale
   grid/checker fallback on the retained accessories geometry.

Residual unknowns (why not CONFIRMED): the runtime fallback branch, the lens
component identity, and why Leon's own PC-cooked materials
(`M_Leon_Body/Skin/Hair` carry the same DXBC evidence) appeared acceptable in
the same screenshot.

**Grep list for the next ladder launch** (in fallback.md §1c): expected-
NEGATIVE engine greps (`LogMaterial|LogShaders|shader map|shader library…` —
the shipping eboot provably contains no UE log strings at all, whole-file
search, so the negative is predicted, not hopeful), launch/save-load
anchors, crash-signature guards, and our zzdiag/modbridge tags. The decisive
confirmation is a one-launch A/B with the substitution overlay, not a log.

## Fallback recommendation (ladder: C for invisible-intent, A for others, B refused)

- **C HIDE**: repoint invisible-intent instances to base
  `M_FluxSurfaceInvisible` (or suppress to base) — verified candidate with
  PS5 store hash.
- **A SUBSTITUTE**: for replaced broken materials, suppress the override
  (store-entry + chunk-row drop) so the engine uses the base export's own
  PS5 cook; pid + export-name + class match; `look_loss` flag forces
  warn-only for re-skinned slots.
- **B CONVERT**: **REFUSED** — cooking SP_PS5 shader maps requires Sony-
  licensed Epic compiler backends we cannot legally obtain or redistribute;
  synthesizing one would fabricate payload. Refused, not deferred.
- All substitution output is derived-overlay-only (GK's builder); originals
  untouched; directives JSON schema sketched in fallback.md §Part 3.

## Leon-set classification (127 packages, 15/15 pinned checks)

- 59 material-family, **all 59 shader-broken** (5 PC-inline: `M_Invisible`×2
  + `M_Leon_Body/Skin/Hair`; 54 chain to a PC parent).
- Of the **68 overlapping uassets**: 43 broken material-family (42 in
  pakchunk0 + 1 in pakchunk10) — every one invisible-intent → **HIDE**;
  25 meshes/other → flag/pass. 24 of the 43 have base store hashes.
- 17 added broken materials (Leon's own look) → NEEDS_REPOINT_OR_DROP,
  warn-only, never silently changed; 44 PASS.
- Notable: the mod's Simon-material overlay is one big hide-Simon operation,
  so C-at-scale is the correct default install for this mod.
