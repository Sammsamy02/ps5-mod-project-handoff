# #12 Leon "full look": result and honest limits (Team A, Mac-only)

Everything here is **Mac-only**. It was worked out from the game's own files already on the Mac (read-only). Nothing has been seen on the console yet.

## The short answer

- **10 of 11 Leon parts** get a game parent that can really show Leon's own colour and surface-detail textures. The parent is `M_MRO_Cloth_NonVT`, the game's own character-clothing shader.
- **The hair stays checkered.** This was proven, not guessed. Details below.
- **Leon's ORM texture is lost on every part.** ORM holds three things: the dark shading in creases, how shiny each spot is, and which spots are metal. No game shader for animated characters takes those three channels in Leon's order. Re-ordering the channels would mean changing texture bytes, which is forbidden.
- **Correction of round 3.** Round 3 sent 10 parts to `M_SkinBasic_Hands`. Those parts would most likely **not have shown Leon's textures at all**: that shader's slots only accept virtual textures, and Leon's textures are plain ones.

## Per-part table

All rows: Leon model = Leon's own shading model (from the material's own settings). Coverage = share of Leon's texture slots that are kept.

| Part | Chosen parent | Parent model | Leon model | Coverage | Kept | Renamed | Lost |
|---|---|---|---|---|---|---|---|
| Eye | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| Face | M_MRO_Cloth_NonVT | Cloth+DefaultLit | SubsurfaceProfile (skin) | 0.667 | Normal | BaseColor → BaseColour | ORM |
| **Hair** | **none (checker)** | - | Hair | 0 | - | - | BaseColor, HairAO, Normal, ORM |
| Knife | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| Lash | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| LowerBody | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| Mouth | M_MRO_Cloth_NonVT | Cloth+DefaultLit | SubsurfaceProfile (skin) | 0.667 | Normal | BaseColor → BaseColour | ORM |
| Pistol | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| Props | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| UpperBody | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |
| Watches | M_MRO_Cloth_NonVT | Cloth+DefaultLit | DefaultLit | 0.667 | Normal | BaseColor → BaseColour | ORM |

The table is generated from `evidence/reparent-plan-r4-fulllook.json` by `per_part_table.py`.

### Why each column says what it says

**Kept: Normal (exact name).**
- The game shader has a slot with the same name.
- The game binds only plain normal maps to that slot (registry).
- The shader's own default textures include a plain normal map.
- Leon's normal map is a plain mip chain (its mip table, plus its data size matching exactly).

**Renamed: BaseColor → BaseColour.**
- Same role (colour) on both sides.
- The colour role is proven by the name on both sides, by Leon's texture format, and by the game's own textures in that slot (sRGB colour, not virtual).
- Only the parameter name changes inside the derived package. Leon's texture bytes are untouched.

**Lost: ORM.**
- Leon packs Occlusion-Roughness-Metal. The game's character shaders pack Metal-Roughness-Occlusion (MRO). Other packings in the game: RSSO, IDRO, SRMF.
- The only game materials that take ORM in Leon's order are environment props. They are not compiled for animated (skinned) meshes, so they cannot draw Leon at all.

**Why the cloth parent, even for face, pistol and eyes.**
- The game's skin shaders cannot take Leon's textures:
  - `M_SkinBasic_Hands`: every Leon slot is virtual-texture only, so it would take 0 of 3 Leon textures.
  - The MetaHuman skin shaders (`M_skin_unified_baked`, `M_Head_Baked`) are refused for three reasons:
    - their slot names carry extra words ("Basecolor Baked", "Color_MAIN"), so no colour or normal role can be proven;
    - each slot has a plain twin and a virtual twin, and which one is live is unknowable;
    - they carry face-region masks made for MetaHuman faces.
- The game's prop shader (`MM_Prop_Basic`) and the virtual-texture cloth shader (`M_MRO_Cloth`) are also virtual-texture only.
- So `M_MRO_Cloth_NonVT` keeps the most Leon textures for every part. Coverage always ranks first.
- The tool still records the mismatch in a look note, for example "role mismatch: skin part on a cloth parent".

**Same parent for every part.** It is not a shortcut. Among game shaders that can draw animated meshes and take plain textures, only this one takes both Leon's colour and normal maps.

## Hair: what was tried and why it fails closed

1. **Leon's hair settings were read from the material itself.**
   - Leon's hair is "Hair" shading and "masked" (cut-out).
   - Two separate fields agree on this: the model setting and the model bit field.
   - Leon's shader-type and render-pass readers also agree on "masked".
2. **The same-model route found exactly one game parent: `M_Hair`.**
   - The asset registry and the material's own settings agree that it is Hair and masked.
   - Compiled shader-type peers agree that it is masked and lit.
   - It is used by game skinned meshes (Simon's beard and eyebrows).
   - Every other parent was refused as "never cross shading models", including `M_Hair_Beard` (DefaultLit).
3. **`M_Hair` cannot take Leon's hair textures.**
   - Its colour slot is "Hair Diffuse + Alpha". But every game texture bound there has **no alpha channel** (registry, 3 of 3).
   - So the game's hair cut-out does not come from that alpha. It must come from the separate packed "IDRO Mask" texture.
   - Leon's cut-out is in the alpha of its colour texture. The data confirms it: 76% of texels are clear, 10% are solid.
   - Leon has no IDRO-type texture.
   - Normal map: `M_Hair` has no normal slot. ORM and HairAO: no matching slot.
4. **Result.** If we forced it, the game shader would cut Leon's hair with Zoe's default IDRO cut-out pattern, which comes from another character's texture layout. That gives random holes. Fail closed: the hair stays checkered, and the exact reason is in the plan row.

**What would fix the hair (not done; each one needs a user decision):**
- (a) A PS5 shader for Leon's own hair material. That needs a PS5 shader compiler, which is out of scope (Sony SDK).
- (b) New texture data (an IDRO-style mask built from Leon's alpha). Forbidden: texture bytes are never changed.
- (c) Hiding Leon's hair. That changes the mod's intent, so it would need the user's call.

## How close this is to the PC look (plain words)

**Close:** the shape, all of Leon's clothing colours and patterns, the face colours, and the fine surface bumps (seams, wrinkles, stubble), on 10 of 11 parts.

**Not the same:**
- **Shine and metal.** Everything gets one even shine from the game shader's default. Metal parts (pistol, knife, watch, buckles) look painted rather than metallic. The dark shading in creases and folds is gone.
- **Skin.** The face and mouth lose the soft "light through skin" glow (Leon uses a skin shader on PC). They look a little flat and dry.
- **Lighting style.** The game's cloth shader may add a soft fabric sheen to parts that were plain on PC.
- **Unmapped slots.** They show the game shader's own default textures. These are mostly flat. The shader also references one 4K MRO texture of Simon's trousers. Which slot uses it is not decoded, so a faint trouser-like shine pattern on some parts cannot be ruled out.
- **Hair.** Still a checker.

**What cannot be matched without PC shaders:**
- Leon's own material graphs (`M_Leon_Body`, `M_Leon_Skin`, `M_Leon_Hair`) exist only as PC shader code. Their extra settings are lost: Specular, OcclusionDark, Scatter, RoughnessMul, HairDensity.
- The ORM channel order.
- The hair alpha cut-out.

## Open issue for the full look (not fixed here)

- **The problem.** The Leon mod also hides parts of Simon on PC. It does this with 43 "invisible" material instances plus 8 "Simon_Slot" materials. These are the mod's own materials, with PC shaders only. On PS5 they cannot run. If a mesh draws them, the engine's default checker appears where PC shows nothing.
- **Where it may show** (from fetched headers):
  - Simon's glasses (`SK_SimonGlasses` uses `MI_Simon_Accessories` and `_Glass`);
  - the arm canula liquid (`SK_Canula` uses `M_Medical_Liquid`);
  - the enemy `SK_TheWall_SimonHead` eyes;
  - Leon's own face mesh at one distance level (`MI_Face_Skin_Baked_LOD1`);
  - the 8 `Simon_Slot` sections of Leon's body meshes. These may be empty placeholders; that is unknown without decoding mesh sections.
- **What a fix needs.** An "invisible" route: a game material proven by two sources to draw nothing. That is a separate design and is not in this patch. The launch card asks the user to look for these.

## New rules in this patch (all generic, Mac-only)

| Rule | Two sources | Fails closed when |
|---|---|---|
| Reader C: blend and shading model from the material's own property block | Base: asset registry + own bytes, with a decoder learned without that material (leave-one-out). Mod: model field + bit field, and blend must match readers A/B. | Training is ambiguous or contradicts itself; a field is unknown; the fields disagree |
| Same-model route (hair, eye ...) | Model set from registry + reader C; blend from registry + shader peers + reader C | Model differs, model is one source only, or the parent is an instance |
| Non-special part to a special parent | - | Always refused (never cross) |
| Slot gate: virtual-texture slots, sampler class, packed letters | Bound game textures (registry) + the shader's own default textures of that class are all plain (a virtual sampler would need a virtual default); mod mip table + data size; texture names agree on packed letters | Any virtual or unknown binding or default of that class, a class mismatch, an unproven mod texture, or texture names naming different channel orders |
| Packed mask, same order under another name | Parameter names + texture names, on both sides | Different order (swizzle), names disagree, game texture not linear |
| Colour+alpha slot | Mod: masked + alpha format + texel cut-out + no other alpha/opacity/mask texture. Game: name + every bound texture sRGB **with** alpha | Any part missing (this is what refuses the hair) |
| Role / model ranking | Model (two sources) + names | Only ranks parents that are already allowed; never overrides coverage |

Also:
- An unreadable mod shading model now refuses every parent, because the other readers cannot rule out a hair or eye model.
- A special model named only by the per-pixel bit field refuses the same-model route.
- The opaque-vs-masked user decision (`--accept-unproven-opaque`) is now refused if reader C reads the material as masked. For Leon, reader C reads opaque, which agrees with the decision.

## Review

- One independent reviewer: **APPROVE WITH FIXES**.
- It found 3 blockers (fail-open paths that Leon's data does not reach), 4 should-fix items and 3 nits.
- All are fixed and tested; Leon's result is unchanged. See `REVIEW.md` and `FIXES.md`.
- Tests: 51/51 + 24/24 PASS (`tests/`).

## Fetch

- Not needed for Leon. All skinned-mesh game masters and all 91 skeletal meshes are already fetched.
- The optional plan (171 rows, 5.76 MB) is described in `APPLY.md`.
