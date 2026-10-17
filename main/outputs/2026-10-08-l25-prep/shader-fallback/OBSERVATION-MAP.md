# OBSERVATION-MAP — where the 43 broken-overridden + 17 added-broken materials sit on the model

Agent HB, 2026-10-08. Mac-only, battery-safe, offline, read-only on records.
Derived exclusively from: `shader-fallback/{fallback.md, leon-classification.json,
classify_materials.py}` (GM), `leon-real-mod-intake-2026-10-01/utoc-analysis.json`
(the 161-file directory), `.superpowers/native-dependency-spike/leon-material-observation-1/notes.md`,
and `unreal-mod-local-dependency-closure-1/proof-3/graph.json` (the decoded
package graph GM cited). Nothing was built, launched, or fetched.

**Inference law:** every placement below is labeled. `[GRAPH]` = directly proven
by the decoded import graph. `[NAME]` = inferred from asset naming convention
only. `[INFERRED]` = reasoned from graph shape + mod design, weaker. Visibility
verdicts are predictions for the A/B launch, not observations — the only
observed defect remains the single glasses-lens checkerboard (screenshot,
observation-1 record).

**The architecture the graph proves (read this first):**
The mod contains NO Leon meshes. Leon's look is Simon's replaced meshes wearing
new Leon materials:

- All 9 body meshes (`SK_Simon_{FirstStage,FinalStage,SeamPull,SideStep,Outfit_01}`
  + 3 Jacket variants + 3 `*_FirstPerson`) import ONLY `/Game/Leon/MI_Leon_*`
  materials (11 slots) + `Simon_Slot0-7` + skeletons/ABPs `[GRAPH]`.
- All 3 face meshes (`Simon_Head_FaceMesh`, 2x `SKM_Simon_Head_FaceMesh`) import
  the 5 head-region Leon materials (`MI_Leon_{Face,Hair,Eye,Lash,Mouth}`) —
  **plus one broken hide-material, `MI_Face_Skin_Baked_LOD1`** `[GRAPH]`.
- The 42 pakchunk0 broken overrides are ALL invisible-intent hide-materials
  chained to one of two mod-local PC-cooked parents: `/Game/Townfall/Characters/
  Simon/Materials/M_Invisible` (1,662,774 B) for the 13 Townfall rows, and
  `/Game/MetaHumans/Simon_Head_5_6_Optimised/Face/Materials/M_Invisible`
  (1,539,341 B) for the 29 MetaHuman rows `[GRAPH]`. The mod is one big
  hide-Simon operation (GM's verdict).
- Because the mod's own meshes no longer import the Simon materials, most of
  the 42 are consumed by **base-game geometry outside the mod's 127 packages**
  (the glasses checkerboard proves at least one such retained binding exists)
  `[INFERRED]`. Which base meshes bind which replaced pid is NOT in any
  retained record (see §5 unknowns).

---

## 1. THE 43 broken overridden material-family packages

All 43 are invisible-intent (hide-Simon); correct behavior = the covered
geometry renders nothing. Broken = fallback rendering (checkerboard /
default-grey) **if that geometry is drawn**. "Base hashes" = the base pakchunk0
store entry's PS5 shader hashes still on record (Option-A suppression is
byte-feasible for those; 24 of 43 have them).

### Region counts (43 total)

| Region group | Count |
|---|---|
| Head/face skin (synthesized + baked LOD sets, side-step variants) | 12 |
| Eyes (refractive, baked, eye shell, lacrimal fluid) | 6 |
| Scalp hair + hair cards + peach fuzz | 6 |
| Eyelashes (all variants) | 5 |
| Beard | 3 |
| Teeth / mouth interior | 3 |
| Accessories (incl. THE observed glasses defect) | 2 |
| Beanie/bodywarmer tag (+Deluxe) | 2 |
| IV bag / medical fluids | 4 |

### 1a. Head/face skin — 12

| Package (under `/Game/MetaHumans/`) | pid | What it overrides | Where it sits | Base hashes | Visibility prediction |
|---|---|---|---|---|---|
| `Simon_Head/Face/Materials/Baked/MI_HeadSynthesized_Baked` | fdf50b1f37af2d44 | base head-skin synthesized material at same path | whole head skin, MetaHuman synthesized cook `[NAME]` | 1 | likely-not-visible (Simon_Head variant likely not the active head) `[INFERRED]` |
| `.../MI_HeadSynthesized_Baked_LOD1..LOD5` (5 pkgs) | d07f1ddd…, 47af0ea2…, a6262ca0…, 5062375c…, eb76c926… | same, per-LOD | head skin at LOD1-5 `[NAME]` | 1 each | likely-not-visible (LOD-only + variant) |
| `Simon_Head_5_6_Optimised/Face/Materials/MI_Face_Skin_Baked_LOD1` | 86b09be921ef6fd5 | base optimised-head skin LOD1 | **face skin LOD1 — imported directly by all 3 replaced face meshes** `[GRAPH]` | 0 | **at-risk-visible: the second-strongest defect candidate after the glasses; watch face at LOD-forcing distance** |
| `.../MI_Face_Skin_Baked_LOD3` | 017759c0ab6922d9 | same, LOD3 | face skin far-LOD `[NAME]` | 3 | likely-not-visible (far LOD only) |
| `.../MI_Face_Skin_Baked_LOD5to7` | 603b97ee37069a13 | same, LOD5-7 | face skin far-LOD | 0 | likely-not-visible |
| `.../MI_Face_SideStep_Skin_Baked_LOD1` | 03d43fa61b0cc3f8 | base side-step head skin | face skin of the "side step" cutscene/quality variant head `[INFERRED]` | 3 | variant-gated (only if that head variant is used) |
| `.../MI_Face_SideStep_Skin_Baked_LOD3` | bc2a77e6a3e2c3bb | same | as above, LOD3 | 3 | variant-gated |
| `.../MI_Face_SideStep_Skin_Baked_LOD5to7` | 781a48ce8032f1f1 | same | as above, LOD5-7 | 0 | variant-gated |

### 1b. Eyes — 6

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Simon_Head/Face/Materials/MI_EyeRefractive_Inst_L` | 52a2e29d59dcbcf3 | left eyeball (refractive cornea) `[NAME]` | 0 | likely-bypassed (face meshes import `MI_Leon_Eye` instead `[GRAPH]`) — unless a base consumer still binds it |
| `.../MI_EyeRefractive_Inst_R` | ebbf04b5214f14db | right eyeball | 0 | same |
| `Simon_Head_5_6_Optimised/.../MI_EyeL_Baked` | 45caf5d6a3029565 | left eyeball, optimised head | 2 | likely-bypassed (same reasoning) |
| `.../MI_EyeR_Baked` | 161594be23b6c923 | right eyeball, optimised head | 2 | same |
| `.../MI_Face_EyeShell` | a87ddccb9bbea202 | eyeball shell/backing sphere `[NAME]` | 0 | likely-bypassed / hidden inside head geometry |
| `.../MI_Face_LacrimalFluid` | 4abde7539dc72651 | tear-duct/lacrimal fluid film at inner eye corners `[NAME]` | 0 | likely-bypassed; sub-pixel at gameplay distance |

### 1c. Scalp hair / fuzz — 6

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Simon_Head/Materials/HairDefaultMaterial` | 1baabff05ddecec0 | default scalp-hair material (fallback slot) `[NAME]` | 0 | likely-bypassed (Leon hair = `MI_Leon_Hair` on face mesh `[GRAPH]`) |
| `.../MI_Hair` | 8801efcd4939000e | scalp hair | 1 | same |
| `.../MI_Hair1` | a6d3d2dc23b5b80d | scalp hair, alt slot | 1 | same |
| `.../MI_Hair_Cards` | 63448aa0f194f42c | hair-card strips geometry | 0 | groom geometry (SK_Simon_Head_Hair etc.) also replaced; binds at runtime — watch for stray grey patches in hair `[INFERRED]` |
| `.../MI_PeachFuzz` | 7c43a7093ee241a1 | fine facial peach-fuzz cards `[NAME]` | 0 | likely-bypassed |
| `Townfall/Characters/Simon/Materials/MI_Simon_Hair` | 9dc627b16a65d404 | Simon's game-side scalp hair | 1 | likely-bypassed (replaced meshes don't import it `[GRAPH]`) |

### 1d. Eyelashes — 5

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Simon_Head_5_6_Optimised/.../MI_Face_EyelashesHiLODs` | fdd721cd7f137a5f | eyelash geometry, high LODs | 0 | likely-bypassed; tiny strips — check eye close-up |
| `Simon_Head_side_step_5_6/.../MI_Face_EyelashesHiLODs` | e4d8d02cd050af96 | same, side-step head variant | 0 | variant-gated |
| `Simon_Head/Materials/M_EyeLash_HigherLODs_Inst` | ea3cdb3f85216570 | eyelashes, higher LODs | 0 | likely-bypassed |
| `Simon_Head/Materials/M_EyelashLowerLODs_Inst` | c8b921d5b8725b7a | eyelashes, lower LODs | 0 | likely-bypassed (far LOD) |
| `Townfall/.../MI_Simon_Eyelashes` | 3ed2e2db3056e6ef | Simon's game-side lashes | 1 | likely-bypassed |

### 1e. Beard — 3

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Townfall/.../MI_Simon_Beard_Dark` | eafa3f71af10ecb0 | Simon's beard geometry (dark) `[NAME]` | 1 | likely-bypassed (screenshot shows no beard defect; groom replaced) |
| `.../MI_Simon_Beard_Dark_02` | 15827438bfb3c3b4 | beard alt set | 1 | same |
| `.../MI_Simon_Beard_Light` | d68ab1517f15454e | beard (light) | 0 | same |

### 1f. Teeth / mouth interior — 3

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Simon_Head_5_6_Optimised/.../MI_Teeth_Baked` | 90d38a941b171a92 | teeth + inner mouth, optimised head | 2 | likely-bypassed (mouth covered by `MI_Leon_Mouth` on face mesh `[GRAPH]`); check open-mouth/talking close-up |
| `Simon_Head_side_step_5_6/.../MI_Teeth_Baked` | 1f3e980a87e26665 | same, side-step variant | 2 | variant-gated |
| `Simon_Head/Materials/M_TeethCharacterCreator_Inst` | 75e692fe84d0a3b7 | teeth (CC-style head) | 0 | likely-not-visible (CC variant likely unused) `[INFERRED]` |

### 1g. Accessories — 2 (contains THE observed defect)

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Townfall/.../MI_Simon_Accessories_Glass` | 96cd0f10e42af022 | the glasses/lens geometry — Simon's retained accessories component on base-game geometry (mod meshes don't import it `[GRAPH]`, so the wearer is a base mesh `[INFERRED]`) | 1 | **observed-defect: pale checkerboard on the lens (screenshot). The one confirmed visible break.** |
| `Townfall/.../MI_Simon_Accessories` | b974a5c17574e197 | same accessories family, non-glass parts (watch straps/belt/buckles — unknown split) `[NAME]` | 1 | **at-risk-visible: same geometry family as the confirmed defect; a second checkerboard here is the top discovery candidate** |

### 1h. Beanie/bodywarmer tag — 2

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Townfall/.../MI_Simon_BodywarmerBeanieTag` | ff82407fbd169af6 | tag/label on the bodywarmer-with-beanie outfit variant `[NAME]` | 1 | outfit-gated: only if that outfit is equipped |
| `.../MI_Simon_BodywarmerBeanieTag_Deluxe` | 2c77facba1d32785 | same, Deluxe (DLC) outfit variant `[NAME]` | unknown — **this is the 43rd row: its base path sits in pakchunk10 whose container-header/store is NOT captured** (GM fallback.md), so the classifier filed it under the added-broken bucket despite the TOC overlap | outfit-gated + store-entry unknown |

### 1i. IV bag / medical fluids — 4

| Package | pid | Where it sits | Base hashes | Visibility |
|---|---|---|---|---|
| `Townfall/.../MI_Simon_IVBagExterior` | 8b94224fe9f4ee0f | IV-drip bag outer surface — medical prop in Simon's scene(s) `[INFERRED from FirstStage/FinalStage hospital-stage naming]` | 0 | scene-gated: visible only when the IV prop is on screen; likely-not-visible in general gameplay |
| `.../MI_Simon_IVBagExterior_Back` | 6b1b4461f56ae12f | IV bag rear face | 0 | same |
| `.../M_Medical_Liquid` | 19ddc0bf1e98aa74 | liquid inside the IV bag/tube `[NAME]` | 1 | same |
| `Townfall/VisualEffects/.../MI_Liquid_Oil_01` | 9ab0f4ccf81c417a | VFX liquid material (EffectsLibrary IV_Fluid group) `[NAME]` | 1 | same scene-gating; also chains to the Simon `M_Invisible` (hide intent) |

---

## 2. THE 17 added-broken packages (Leon's own PC-cooked materials)

These have NO base-game pid — they are the mod's new look, and per GM they
land in NEEDS_REPOINT_OR_DROP (warn-only, never silently changed). The 11
instances are tiny (792–929 B, zero shader tokens — instances carry no shaders
of their own `[GRAPH]`); the brokenness of the set rests entirely on the three
PC-inline parents (DXBC/DXIL, zero PS5 tokens — GM's byte facts).

### 2a. Leon instances that SHOULD be visible (11) — all referenced by the mod's replaced meshes `[GRAPH]`

| Package | pid | Parent `[GRAPH]` | Where it sits | Bound by |
|---|---|---|---|---|
| `/Game/Leon/MI_Leon_Face` | 9c5f74b56bb1bbe2 | `M_Leon_Skin` | face skin | all 3 face meshes |
| `/Game/Leon/MI_Leon_Mouth` | eca41bf4254b2521 | `M_Leon_Skin` | mouth interior/teeth | all 3 face meshes |
| `/Game/Leon/MI_Leon_Eye` | 57acb4a0855f9c47 | `M_Leon_Body` | eyeballs | all 3 face meshes |
| `/Game/Leon/MI_Leon_Lash` | b559bc84c177efe3 | `M_Leon_Body` | eyelashes | all 3 face meshes |
| `/Game/Leon/MI_Leon_Hair` | 943d6af7554526e0 | `M_Leon_Hair` | scalp hair / hair cards on head | all 3 face meshes |
| `/Game/Leon/MI_Leon_UpperBody` | ff5f4f7491ac2462 | `M_Leon_Body` | torso/jacket/sleeves (incl. hands region `[INFERRED]`) | all 9 body meshes incl. FP |
| `/Game/Leon/MI_Leon_LowerBody` | f6db4d4c39916649 | `M_Leon_Body` | legs/pants | all 9 body meshes incl. FP |
| `/Game/Leon/MI_Leon_Props` | 999a141324f78722 | `M_Leon_Body` | belt/holster/gear props `[NAME]` | all 9 body meshes incl. FP |
| `/Game/Leon/MI_Leon_Knife` | c74672b103d272c0 | `M_Leon_Body` | knife (held/sheathed) | all 9 body meshes incl. FP |
| `/Game/Leon/MI_Leon_Pistol` | 760dd9e0862e03a6 | `M_Leon_Body` | pistol (held/holstered) | all 9 body meshes incl. FP |
| `/Game/Leon/MI_Leon_Watches` | 1826460a40d26c69 | `M_Leon_Body` | wristwatch | all 9 body meshes incl. FP |

**First-person fact for the checklist `[GRAPH]`:** the 3 `*_FirstPerson`
meshes bind exactly 6 Leon materials — UpperBody, LowerBody, Props, Knife,
Pistol, Watches (no face materials). FP view is a clean 6-material
observation window.

### 2b. The parents + hide-machinery (6)

| Package | pid | Role | Where |
|---|---|---|---|
| `/Game/Leon/M_Leon_Body` | 1532e3f95d0095fd | PC-inline parent (15 DXBC/10 DXIL) of 8 instances | shader source for everything body/prop/eye/lash |
| `/Game/Leon/M_Leon_Skin` | 9893672936c06526 | PC-inline parent | shader source for face/mouth |
| `/Game/Leon/M_Leon_Hair` | 4ca774b444bcbdb8 | PC-inline parent (364,845 B) | shader source for hair |
| `Simon_Head_5_6_Optimised/.../M_Invisible` | d7ead7ce2462b4f1 | PC-inline hide parent (1,539,341 B) | serves the 29 MetaHuman hide rows of §1 |
| `Townfall/.../M_Invisible` | 5dd733019f47199a | PC-inline hide parent (1,662,774 B; 296 DXBC/205 DXIL) | serves the 13 Townfall hide rows of §1; imported by the glasses chain |
| `Townfall/.../MI_Simon_BodywarmerBeanieTag_Deluxe` | 2c77facba1d32785 | hide-instance, added/broken (also the §1h 43rd overlap row) | Deluxe outfit tag |

### 2c. THE PUZZLE (the A/B's key question): why did Leon look right?

Files prove the mod ships zero PS5-consumable shader data for these chains as
well (`mod_store_hash_total: 0`; parents are D3D-only), yet the screenshot
showed a correctly-looking Leon with only the glasses broken. Leading
explanations, to be discriminated by the A/B close-ups — not by more files:

1. **Masked fallback `[INFERRED]`:** the engine substituted a default/grey
   material whose flat look reads as skin/cloth at screenshot resolution,
   distance, and lighting. Prediction for A/B: native-resolution close-ups of
   face/hands/props reveal flat, untextured, or wrongly-lit surfaces
   (vs the 2K `T_Leon_*_D/N/ORM` detail the mod ships).
2. **Per-case engine tolerance / conservative classifier scoring (GM's
   framing):** the replaced-instance chain (glasses) and the added-material
   chains take different runtime paths; the classifier scores both broken from
   the same DXBC evidence, but only the replaced-pid path showed a defect.
   Possibly the added chains resolve through base-game shader-library lookups
   the byte-level classifier cannot credit (no store hashes exist for added
   pids, so files can neither confirm nor exclude runtime resolution).
3. Open residual from GM's verdict §1d: "why PC-inline added materials
   differed visibly from the replaced-instance chain" needs an
   engine-observer hook or the A/B's visual evidence — mark every Leon-region
   row expected-vs-observed accordingly.

---

## 3. THE A/B CHECKLIST — per-body-region observation sheet

Conditions: ONE A/B launch pair on the same save with byte-identical inputs —
(A) control: mod mounted as-is (glasses checkerboard expected to persist);
(B) treatment: GM's hide-substitution overlay applied. If the lens checkerboard
disappears in B while A keeps it, the shader-platform cause is confirmed
operationally (fallback.md §1c).

**Screenshot law: every shot goes straight into
`outputs/2026-10-08-l25-prep/evidence/SCREENSHOTS/` (HA is creating the
directory).** Suggested names: `AB-<A|B>-<region>-<UTC>.png`. Klog capture per
agent-T conventions (TCP 3232 tap, bracketed `[klog-capture <UTC>]`), with
fallback.md §1c grep list (engine shader-miss surface is a predicted negative
— record the zero, never treat it as "no defect").

| # | Region / shot | What to photograph | "Broken" looks like | "Fixed-by-hide" looks like | Expected (files) | Observed |
|---|---|---|---|---|---|---|
| 1 | **Glasses/lens (control defect)** | TP face close-up, both lens faces | pale checkerboard (observed today) | **glasses gone entirely — README says hidden is CORRECT** | A: checkerboard; B: gone | |
| 2 | Other accessories | watch straps, belt, buckles, anything non-glass Simon kept | second checkerboard patch | vanilla/no artifact | A: at-risk (§1g row 2); B: gone/no artifact | |
| 3 | Face skin | TP native-res close-up, cheek/jaw in even light | flat grey / default material / no pore detail vs 2K `T_Leon_Face_D` | n/a (Leon face is SHOW, not hide) | textured = puzzle explanation 2; flat = explanation 1 | |
| 4 | Eyes | eye close-up, catchlight/refraction | grey balls, no refractive highlight | n/a | same discriminator | |
| 5 | Mouth/teeth | talking/open-mouth moment | grey interior | n/a | same | |
| 6 | Lashes/brows | eye-frame close-up | missing or grey strips | n/a | same | |
| 7 | Hair | head turn in light, strand detail | grey mass, no strand/flow texture | n/a | same (M_Leon_Hair is the biggest PC parent, 364,845 B) | |
| 8 | Beard zone | jaw/cheek | stray grey patch (Simon beard geometry leaking) | nothing (hidden) | A: likely nothing; B: nothing | |
| 9 | Torso/upper body | chest/back/sleeves, cloth folds | flat untextured cloth | n/a | same discriminator | |
| 10 | Lower body | legs/waist | same | n/a | same | |
| 11 | **Hands in FP** | raise hands to camera (FP view) | grey/flat hands — FP binds exactly UpperBody+Props(+watch) `[GRAPH]` | n/a | cleanest 6-material window | |
| 12 | Held props FP | knife out, pistol aim, watch check | checkerboard or default-grey weapon | n/a | same | |
| 13 | Headwear/outfit variants | cycle outfits if menu allows (bodywarmer/beanie, Deluxe) | tag/label checkerboard | tag gone (§1h hide) | outfit-gated; may be unreachable — record as not-reachable | |
| 14 | IV/medical prop | if the hospital-stage scene is reachable in the save | bag/liquid checkerboard | invisible bag liquid surfaces | scene-gated; likely not reachable — record honestly | |
| 15 | Face at distance/LOD | walk camera far-near to force LOD1 (§1a `MI_Face_Skin_Baked_LOD1` is mesh-bound `[GRAPH]`) | face flips to checkerboard at LOD switch | face flips to Leon LOD texture | the #2 defect candidate | |
| 16 | Full-body control shots | TP front/side/back, gameplay lighting, both conditions | — | — | baseline for archive | |
| 17 | Regression guard | whole session | any crash/fatal-signal/hang beyond known | — | none (fallback.md §1c grep list 3) | |

Fill "Observed" during the launch; attach screenshot filenames per row.

---

## 4. THE QUEUE NOTE

- **Ordering:** the A/B launch queues AFTER the round-4 Townfall launch
  (round-4 PRX scope locked to FR+FW only — `WAKE-RUNBOOK-20261010.md` §10.3;
  it carries no overlay content). It may share the Stellar Blade dump session
  (the first console session after the Townfall launch per §10.4): one boot,
  Townfall A/B segment (two title launches with a clean exit between — no
  rapid-fire relaunches, PS5 launch-pacing law), then the Blade dump segment.
- **Staged before the A/B (cross-reference, do not block on it):**
  1. GK's overlay application — `pak-overlay/overlay_builder.py` applying GM's
     directives: repoint/suppress the 42+1 hide rows toward
     `/Game/FluidFlux/Surface/Materials/M_FluxSurfaceInvisible` (fallback to
     base-suppression), Leon's 17 untouched (warn-only). Self-verify = re-run
     `classify_materials.py` against the derived overlay, expect zero broken
     replaced rows (fallback.md Part 3 step 3).
  2. HA's `evidence/SCREENSHOTS/` directory (this sheet references it).
  3. Klog capture running + §1c grep list staged.
  4. Decrypt-off readback verification law for anything uploaded (PS5 console
     rules; overlay is derived data, never signed executables — no fakesign
     needed, but verify bytes after transfer).
- If GK's builder is not ready at session time, the A/B degrades gracefully:
  run condition A only (control confirmation + the region sweep of §3), and
  re-queue condition B.

---

## 5. Honest unknowns (static data cannot place these — launch to discover)

1. **Which base-game meshes bind each replaced pid.** The graph covers only
   the mod's 127 packages; the base UCAS (128 GB) was never read (GM's bound).
   Every §1 "where it sits" beyond the graph facts is name/convention
   inference. The glasses prove at least one retained binding; the others are
   unverified.
2. **Groom runtime binding.** Hair-card meshes import only
   `WorldGridMaterial` + skeleton `[GRAPH]`; which material the runtime
   actually applies to groom geometry is invisible to package imports.
3. **Active head variant.** Three head families overlap (Simon_Head,
   5_6_Optimised, side_step_5_6) — which one the observed scene uses, and
   which LODs are active, is runtime state.
4. **`Simon_Slot0-7`** (added, 105,091 B each): referenced by every body mesh
   `[GRAPH]` but their role (morph targets? clothing physics? LOD source?) is
   undetermined; they import only `MF_PhongToMetalRoughness`.
5. **Exact engine fallback branch** (default material vs missing texture) —
   runtime behavior; files cannot prove it (GM §1d residual 1).
6. **The 17-added puzzle** (§2c) — why PC-cooked Leon materials rendered
   acceptably; leading explanations listed, discriminated only by §3 rows
   3-12 at native resolution (or a future engine-observer hook).
7. **pakchunk10 store entry for `MI_Simon_BodywarmerBeanieTag_Deluxe`** —
   header not captured; base hashes unknown; its overlap status rests on the
   TOC path match alone.
8. **IV/medical scene reachability** and **outfit-variant switching** in the
   current save — gameplay facts, not file facts.
9. `SP_Leon_Skin` (318 B, PASS) — likely a skin-profile/subsurface asset
   `[NAME]`; no shader evidence, unplaced.

## Provenance

Read-only inputs (never modified):
`shader-fallback/{fallback.md, leon-classification.json}`,
`.superpowers/native-observations/leon-real-mod-intake-2026-10-01/utoc-analysis.json`
(+ `base-pak-metadata/townfall-leon-overlap-summary.json`),
`.superpowers/native-dependency-spike/leon-material-observation-1/notes.md`,
`.superpowers/native-dependency-spike/unreal-mod-local-dependency-closure-1/proof-3/graph.json`,
`WAKE-RUNBOOK-20261010.md` (queue facts), `pak-overlay/` (existence check).
Created: only this file and `agent-HB-obsmap.md`.
