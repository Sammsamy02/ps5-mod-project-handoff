# #12 Leon full look, round 6: where the last checker comes from, and what this round fixes (Team A, Mac-only)

Everything here is **Mac-only**: worked out from the game's own files already on the Mac and Leon's
unchanged mod files. The starting point is **console-proven** by the integrator (launch H, R15e, round 5
installed): Leon's textured parts look right; the user still saw the grey/white checker on (a) the glasses'
lens and (b) "some parts of the clothes"; the hair is checkered (expected).

## The short answer

- **The checker is not on Leon's own outfit.** Every part of Leon's own meshes that has real geometry was
  converted in round 5, except the hair. The checker comes from **Simon's own things that the game still
  attaches to the character and that the mod hides on PC** with its own "invisible" material: Simon's
  **glasses** (lens + frame, two versions), his **IV bag** (front and back, hanging on his clothes), some
  face parts used in cutscenes, and the IV-bag **liquid**. That invisible material exists only as PC shader
  code, so the PS5 draws its checker there.
- **This round** points those hidden parts at the game's own `M_Hide` material (draws nothing, as on PC).
  New proofs this round: `M_Hide` also works on **normal (non-Nanite) static objects**, not only on skinned
  meshes; and for parts used only by Blueprints / cutscenes, the game's **own original** material shows which
  drawing paths the game can use for them.
  - **Fully proven (10 parts):** IV bag front, 6 face/cutscene skin parts, 3 enemy "TheWall" eye parts.
  - **Partial (4 parts, need `allow-partial`):** glasses **lens** and **frame** (hidden on the skinned and the
    normal version the cutscenes use; the game's original glasses material can also be drawn instanced or
    as Nanite / geometry-collection, which `M_Hide` cannot), the IV bag back half, and an enemy cannula.
- **Still checker:** the **hair** (unchanged), the **IV-bag liquid** where the game draws it with its effects
  system, a **cap tag** of the deluxe outfit in cutscenes, and a tag on a **bed prop** (a Nanite mesh).
- **The 8 `Simon_Slot` materials are not clothes.** They are **8 placeholder triangles, each 1 mm and inside
  Leon's hips**, on every one of the 11 body meshes (three independent proofs). They cannot be seen. Left
  unchanged (a builder to change them exists and is tested, but it is not shipped).
- **No translucent material is involved.** The lens checker is the mod's "hide the glasses" instance, not a
  glass shader. The game does have PS5 glass shaders, but using them would *show* glasses the mod hides.
- **No console fetch is needed.** User decisions: `accept-unproven-opaque` (as round 5) and `allow-partial`
  (for the 4 partial parts).
- **Independent review:** APPROVE WITH FIXES (1 blocker, 3 major, 11 minor); all fixes folded in (FIXES.md).

## What Leon will look like now, per part (plain words)

| Part | Where you see it | Round 5 (console-proven) | Round 6 (Mac-only) |
|---|---|---|---|
| Jacket, trousers, boots, props, pistol, knife, watches | always | Leon's look, PS5 layout | unchanged |
| Face, mouth, eyes, eyelashes | always | Leon's look | unchanged |
| **Hair** | always | checker | **still checker** (refused: no proven hair mask) |
| **Glasses lens + frame** (skinned version and 4-piece normal version) | cutscenes | **checker** | **hidden, as on PC** (partial: see above) |
| **IV bag, front half** (on Simon's clothes) | gameplay + cutscenes | **checker** | **hidden** |
| **IV bag, back half** | gameplay + cutscenes | **checker** | **hidden where drawn as a normal object** (partial) |
| **IV-bag liquid** | gameplay + cutscenes | checker (if drawn) | **still checker where the effects system draws it** |
| Cap tag (deluxe outfit) | cutscenes | checker (if visible) | **still checker** (refused) |
| Face skin parts used by cutscenes / the organ box (5 instances) | some scenes | checker | **hidden** |
| 9 tiny face placeholders (1 mm, centre of the head) | never visible | checker (1 mm) | hidden |
| 8 tiny body placeholders `Simon_Slot0..7` (inside the hips) | never visible | checker (1 mm) | unchanged (cannot be seen) |
| Hair cards / beard / eyebrows / cap meshes (11, the mod's 1-triangle stand-ins) | never visible | engine grid on a 0.5 mm triangle in the head, same as PC | unchanged |
| Enemy "TheWall" eye parts | that enemy | checker | hidden |
| Enemy cannula liquid | that enemy | checker | hidden where drawn as a normal/skinned mesh |

## Task 1: attribution, from the game's and the mod's own data

How it was found (all generic tools, see PATCH/):
- `mesh_sections.py` decodes every SkeletalMesh the mod ships: material slots (`FSkeletalMaterial` table),
  skeleton, and every render LOD's sections, index buffer and positions, all cross-checked (index count =
  3 x triangles, base-index and base-vertex chains, every index inside its section, vertex totals).
- `attribution.py` lists, for each mod mesh, every slot -> material -> sections/triangles -> status, and for
  every **game** Blueprint that attaches a mod mesh (the "character" Blueprints), the other meshes it
  attaches whose materials the mod overrides. Full table: `evidence/attribution/attribution.md`
  (25 mod meshes, 252 slots; 10 character Blueprints; 7 accessory meshes; 9 Blueprint-set materials).

### A) The meshes the mod ships (25)

| Mesh (count) | Slots | What draws | Status |
|---|---|---|---|
| Body, 3rd person: `SK_Simon_FirstStage`, `_FinalStage`, `_SeamPull`, `_SideStep`, `_Outfit_01`, 3 x `_Jacket` (8) | 19 | 6 Leon sections (jacket 66,204 tris, watches, props, trousers, pistol, knife) + **8 placeholder sections** (`Simon_Slot0..7`, 1 triangle each) | Leon parts: converted (round 5). `Simon_Slot*`: **placeholder** |
| Body, 1st person: 3 x `_FirstPerson` (3) | 14 | same 6 Leon sections + the 8 placeholders | same |
| Face: `SKM_Simon_Head_FaceMesh` x2, `Simon_Head_FaceMesh` (3) | 14 | hair 35,958 tris, mouth, face, eyes, lashes + **9 placeholder-sized sections** (`Simon_FSlot0..8`, 1 triangle each, material `MI_Face_Skin_Baked_LOD1`) | Leon parts: converted, **hair: checker**; the 9 small sections: **invisible route** |
| Hair cards / beard / eyebrows / eyelashes / cap (11) | 1-2 | one 0.5 mm triangle near the head centre each, engine `WorldGridMaterial` | game material: same as PC, not visible |

Every mod mesh has **one LOD** (LOD0 only). Leon's face mesh has no LOD1: Team B's "face LOD1" row is the
9 one-triangle sections above (1 mm, at the centre of the head; not proven enclosed, one ray leaves through
the open neck of the face mesh), not a visible face band.

**The `Simon_Slot` sections are placeholders, from three independent sources** (all 11 body meshes,
`base-material-report.json` verdict `placeholder`; every small section: `evidence/attribution/small-sections.json`):
1. the section table: 1 triangle, 3 vertices, skinned to the `pelvis` bone only;
2. the geometry: 0.005 cm2 (a 1 mm x 1 mm triangle) at the centre of the hips (x=0, y=0, z=98-100 cm),
   **enclosed** by Leon's own body: all 6 axis rays from it hit the body surface;
3. the engine's own build data: the slot's UV densities are 0 (a slot with visible geometry has non-zero
   values, e.g. jacket 146.5 / 297.1).
The materials themselves are FBX-import placeholders: no texture, no parameter, one material function
(`MF_PhongToMetalRoughness`, which the game does not even contain). The modder kept 8 slots so the game's
Blueprints that set materials by slot number land on invisible triangles.

### B) Game meshes the character Blueprints attach, with materials the mod overrides

| Game mesh | Kind (registry `NaniteEnabled`) | Material (mod version: "invisible" instance) | Status now |
|---|---|---|---|
| `SM_Glasses_Lenses` | normal object (False) | `MI_Simon_Accessories_Glass` | **invisible-partial** |
| `SM_Glasses_Frame`, `SM_Glasses_Leg_L/R` | normal objects (False) | `MI_Simon_Accessories` | **invisible-partial** |
| `SK_SimonGlasses` | skinned (False) | `MI_Simon_Accessories` + `_Glass` | **invisible-partial** |
| `Simon_FirstStage_IVBagFront` | normal object (False) | `MI_Simon_IVBagExterior` | **invisible** (full, via the game's original) |
| `Simon_FirstStage_IVBagBack` | normal object (False) | `MI_Simon_IVBagExterior_Back` | **invisible-partial** |

Who attaches them (game store graph): the gameplay character `BP_Bill` (body, 1st-person body, the 3 IV-bag
meshes) and the 6 cutscene "hero puppets" (glasses in both versions, IV bags, face, hair cards).
Materials the Blueprints set directly: `MI_Liquid_Oil_01` (IV-bag liquid, 5 Blueprints + 2 effects),
`MI_Simon_BodywarmerBeanieTag_Deluxe` (6 puppets), the IV-bag exteriors, and 5 hair/groom instances of an
unused head Blueprint.

### Status of every overridden material that something draws

"Paths" = the drawing paths the game's own original master was built for (its PS5 shader map).

| Material | Users (game data) | Round-6 result | Why |
|---|---|---|---|
| `MI_Simon_Accessories_Glass` (lens) | 1 skinned + 1 normal mesh | **partial** | users proven; original `MM_Prop_Glass_Cutscenes` also has instanced + geometry-collection paths |
| `MI_Simon_Accessories` (frame) | 1 skinned + 3 normal | **partial** | users proven; original `MM_Prop_Basic` also has instanced, spline, geometry-collection, cloth, Nanite paths |
| `MI_Simon_IVBagExterior` | 4 Blueprints + 1 normal mesh | **invisible** | original `M_MRO_TranslucentGlass_OneSided_ThinTranslusent`: normal + skinned only |
| `MI_Simon_IVBagExterior_Back` | 4 Blueprints + 1 normal mesh | **partial** (1 of 5 users) | its original also has the instanced path |
| `MI_Face_Skin_Baked_LOD1` | 3 face meshes (small sections) + 1 Blueprint | **invisible** | original `M_skin_unified_baked`: normal + skinned only |
| `MI_Face_Skin_Baked_LOD3`, `_LOD5to7` | 1 Blueprint (organ box) | **invisible** | same original |
| `MI_Face_SideStep_Skin_Baked_LOD1/3/5to7` | 1 cutscene | **invisible** | same original |
| `MI_EyeR_Baked`, `MI_Face_EyeShell`, `MI_Face_LacrimalFluid` | enemy TheWall head (skinned) | **invisible** | skinned user proven (Team B); originals not fetched (optional fetch) |
| `M_Medical_Liquid` | enemy cannula (skinned + normal) | **partial** | users proven; original `M_MRO_Dither_Glass` also has the Nanite path |
| `MI_Simon_BodywarmerBeanieTag` | 1 **Nanite** static mesh (a bed prop) | **still checker** | Nanite meshes draw through Nanite shaders, which `M_Hide` lacks (review blocker) |
| `MI_Liquid_Oil_01` (IV-bag liquid) | 5 Blueprints + 2 effects | **still checker** | original `M_IVBag_Liquid` is built for normal meshes **and effect meshes (Niagara)**; `M_Hide` has no effect version |
| `MI_Simon_BodywarmerBeanieTag_Deluxe` | 6 Blueprints | **still checker** | original `M_MRO_Cloth_NonVT` also has instanced, cloth and Nanite paths; no proven user |
| `HairDefaultMaterial` (+4 hair instances) | 1 unused head Blueprint | **still checker** (if ever loaded) | groom hair paths; `M_Hide` has none / originals not fetched |
| `MI_Leon_Hair` | Leon's face meshes | **still checker** | refused since round 5 (hair mask) |
| `Simon_Slot0..7` | 11 body meshes, placeholders | **unchanged** | cannot be seen (above) |
| 21 other overridden instances | nothing loads them | unchanged | no run-time user (Team B) |

## Task 2: Team B's invisible route, integrated into the round-5 pipeline

Pipeline on the **exact console plan** (`cap12-launch-prep-20261013/reparent-plan-r5-textures.json`):
`invisible_route.py --apply` (round-6 version) -> `derive_substitutes.py` (integrated, unchanged) ->
`paktx.py install --substitute` (integrated, unchanged).

| Step | Result (Mac-only) |
|---|---|
| plan `evidence/plan-r6/reparent-plan-r6.json` (sha `889c986a...`) | 20 `reparent` (10 Leon rows byte-equal to round 5 + 10 invisible) + 4 `reparent-partial` + 43 `known-checker` |
| manifest `evidence/manifest-r6/` (sha `a29bbcaf...`) | 24 derived instance packages + the same 30 derived PS5 textures as round 5 |
| paktx dry run | 24 `parent ... ->` lines, 30 `DERIVED TEXTURE` lines, 84 substituted chunks verified |

Changes to Team B's `invisible_route.py` (diff: `PATCH/invisible_route.py.diff`; Team B's rules otherwise kept):
- **G6, plain static (normal) meshes, two sources.** (1) The target's own PS5 shader map names the static-mesh
  vertex factory's parameter layout (`FLocalVertexFactoryShaderParameters`; the game's materials store these
  layout names as length-prefixed strings inside the compiled shader map). (2) Engine rule: that vertex factory
  is compiled for every surface material, and G3 already proves `M_Hide` is a surface material from two
  sources. Learning the same name from the game's 82 static-mesh materials is only a name check (it is in
  every material). Scope: plain static-mesh draws; Nanite, instanced, spline and geometry-collection draws are
  other paths and are not proven.
- **Nanite users are not proven.** A mesh user counts only when the asset registry says `NaniteEnabled=False`
  (7,859 of the game's 8,884 static meshes are Nanite; `M_Hide` has no Nanite shaders).
- **Original master.** For an overridden instance, its **game-side** parent chain (store graph) ends in a game
  master; its drawing paths (every vertex-factory layout name, F- or T-prefixed, plus Nanite) are what the base
  game can draw it with. All paths proven + every mod user of a proven kind -> **full**; users proven but the
  master has other paths -> **partial**; master unknown -> Team B's user-class rule.
- **Bytes.** The shared reader (`zen.py`) assigns export bytes in bundle order, which is wrong for packages
  whose first serialized export is not first in the data region (every mod mesh). The route now **refuses**
  any package where those bytes differ from the CookedSerialOffset bytes (12 of 377 fetched game packages,
  none of them a material). Integrator follow-up: make `zen.ZenPackage.export_bytes` CSO-based (not done
  here: `zen.py` is pinned by the overlay).
- The report-only base-material decisions now go to a **separate file** (`--base-material-report`); the plan
  bytes are identical with or without it (checked).

**`allow-partial` (plainly, for the launch card):** in round 6 it covers **4** parts: the glasses lens, the
glasses frame, the IV bag back half and the enemy cannula. "Hidden wherever the game draws them as normal or
skinned objects; where the game draws them another way (instanced, Nanite, effects), the checker stays as
today." Team B's 4 partial rows were face LOD1, glasses frame, glasses lens and the cannula: face LOD1 is now
fully proven; the other three stay partial for a new, stricter reason (their originals have other paths).

## Task 3: mod-only BASE Materials (like `Simon_Slot*`)

`base_material_route.py` (report only; `invisible_route.py --base-material-report`):
- **Option B, placeholder:** every section of every LOD of every mesh using it is a placeholder (three
  sources above) -> `placeholder`. **Leon: the 8 `Simon_Slot` materials.** A material whose slots no section
  uses at all is refused (not provably unused).
- **Option B, draws nothing:** Team B's mod-parent proof (name + own blend + constant graph). Leon: none
  used directly by a mesh (the two `M_Invisible` are parents only).
- **Option A, real surface:** needs the textures it references, a role per texture from **two sources**
  (name suffix + pixel format), and its blend from **two readers** (own property block + compiled PC
  shaders); the parent family follows the blend (translucent only to translucent). Refused otherwise.
  Leon: **0 candidates** (`M_Leon_Body/Skin/Hair` are parents of instances only).
- Nothing from this route ships in round 6: a placeholder cannot be seen, and building a derived instance for
  a base Material is not part of the shipping overlay.
- **Builder, ready but not shipped:** `mesh_slot_repoint.py` re-points chosen slots of a mesh to one game
  material (carrier import re-pointed with the reviewed `mic_reparent.py`, other target slots' 4-byte
  references patched). Guards: the carrier import must serve target slots only and appear nowhere else in the
  export (outside the reference pose and the geometry buffers); after re-decoding, every non-target slot and
  every section is unchanged; the export changes only at the patched fields. Real-data check: all 11 body
  meshes (slots 0-7 -> `M_Hide`, 7 export bytes each), receipts in `tests/output/real/repoint/`.

## Task 4: translucent materials (the lens)

- **The mod ships no translucent material.** Its 13 Materials: `M_Leon_Body/Skin/Hair` (opaque),
  2 x `M_Invisible` (masked, unlit: own property block + PC shader readers), 8 x `Simon_Slot` (opaque).
- The lens instance the mod ships, `MI_Simon_Accessories_Glass`, has **no parameters** and the mod's
  `M_Invisible` as parent: it **hides** the lens on PC.
- The game has PS5 glass shaders (21 translucent "glass" masters, e.g. `MM_Prop_Glass_Cutscenes` = the game's
  original lens parent, `M_MRO_TranslucentGlass_OneSided` (DefaultLit), `..._ThinTranslusent`
  (ThinTranslucent)), used by game meshes through instances. **Not used:** the mod's lens material is masked
  (hide); a glass parent would cross blend modes (masked -> translucent) and would **show** a lens the mod
  hides. The route keeps masked -> masked (`M_Hide`: masked + unlit from registry, shader types and its own
  property block).

## Fetch

**None needed for this launch.** Optional, read-only, information only:
`evidence/fetch-optional/fetch-plan-optional.json` (7 whole chunks, 1,299,111 B): the 2 hair masters and the
3 TheWall eye masters (to check their drawing paths; the eye rows could become partial), the IV-bag liquid
effect and `BP_Bill` (how the liquid is drawn). None of it can make the liquid hideable.

## Tests (`sh tests/run_tests.sh`, saved output in `tests/output/`)

Final line in `tests/output/run.log`. Suites:
- `test_round6.py` (43): mesh decoding on Leon's real meshes (placeholder proof; a placeholder that is actually
  drawn, a tiny triangle outside the body, non-zero UV density, a tampered buffer, an export gap, the package
  tail rule, CSO vs bundle-order bytes: all checked), base-material rules (roles that cannot be proven,
  translucent never to opaque, two readers, no-section, scope), static-mesh proof (each source missing, the
  surface-domain rule), length-prefixed token reader (T-prefixed, unanchored), Nanite users, original-master
  finder (every fail-closed case), R3b and strict partial (missing paths, cloth, Nanite, mod users, zero
  users, truncation), the slot builder (a rewrite that would change a non-target slot, a carrier referenced
  elsewhere, a stray write: all refused).
- `test_invisible_route_r6.py`: Team B's 59 tests, unchanged except the real-data expectations
  (10 + 4 instead of 3 + 4) and the round-5 derive step.
- `test_cap12.py` (51) + `test_fulllook.py` (24): the integrated planner suites next to the new files.
- Real Leon dry run: plan (sha equals the staged plan, with the separate report), manifest (sha equals the
  staged one), paktx `install --dry-run`, attribution, builder check, `check_real_r6.py`.
Every gate prints its count; the runner fails on zero tests, any failure or skip, stale pins, a diff that
does not reproduce `PATCH/tools/invisible_route.py`, or a changed read-only input.

## Open risks / not proven

1. **Nothing in round 6 is console-proven.** In particular that `M_Hide` hides the normal-object glasses and
   the IV bag on the PS5 (its static-mesh shaders are proven present, not seen drawing).
2. "Draws nothing" for `M_Hide` rests on Team B's proof (name, masked blend from three readers, constant
   graph, the game's own use on face meshes). If a part shows a flat colour instead of nothing, report it.
3. Partial parts and the liquid may still show the checker where the game draws them another way.
4. The original-master rule assumes the base game draws each instance only through paths its original was
   built for (else the base game would show its default material too). Runtime usage checks without a
   vertex-factory layout (morph targets, clothing, static lighting) are not read; the skinned users here have
   no morph targets and the game has no lightmap shaders (review).
5. Mesh users' Nanite flag comes from the game's registry; a mod version of an overridden mesh could differ
   (its tagged properties are not decoded).
