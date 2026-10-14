# Agent HB — OBSERVATION-MAP report

Deliverable: `outputs/2026-10-08-l25-prep/shader-fallback/OBSERVATION-MAP.md`
(map + A/B checklist + queue note + unknowns). Mac-only, offline, read-only on
records. All placements inference-labeled ([GRAPH]/[NAME]/[INFERRED]).

## Region counts — the 43 broken overridden (all invisible-intent hide-materials)

| Region | Count | Visibility headline |
|---|---|---|
| Head/face skin (synthesized + baked LOD sets, side-step) | 12 | `MI_Face_Skin_Baked_LOD1` is mesh-bound on all 3 replaced face meshes [GRAPH] — #2 defect candidate |
| Eyes | 6 | likely-bypassed (face meshes import `MI_Leon_Eye`) |
| Scalp hair/fuzz | 6 | likely-bypassed; groom binding is runtime-invisible |
| Eyelashes | 5 | likely-bypassed |
| Beard | 3 | likely-bypassed |
| Teeth/mouth | 3 | one variant-gated, rest bypassed |
| Accessories | 2 | **glasses = the one observed checkerboard; non-glass `MI_Simon_Accessories` = top discovery candidate** |
| Beanie/bodywarmer tags | 2 | outfit-gated; the Deluxe row IS the 43rd (pakchunk10, store uncaptured) |
| IV bag/medical fluids | 4 | scene-gated, likely not reachable |

Why only one visible defect is coherent from the graph: the mod's replaced
meshes import ONLY `/Game/Leon/MI_Leon_*` materials, so most broken
hide-materials are consumed by base-game geometry that never draws them; a
defect appears only where retained geometry still binds a broken pid (glasses
proves one such binding exists).

## The 17 added-broken (Leon's own)

11 should-be-visible instances (5 head: Face/Mouth/Eye/Lash/Hair on the 3 face
meshes; 6 body/props: UpperBody/LowerBody/Props/Knife/Pistol/Watches on all 9
body meshes — the 3 FP meshes bind exactly the 6 body ones, a clean FP
observation window) + 3 PC-inline parents (M_Leon_Body/Skin/Hair) + 2
M_Invisible hide-parents + the Deluxe tag.

**Puzzle framing (A/B key question):** files prove zero PS5-consumable shader
data for these chains too, yet the screenshot showed a good-looking Leon.
Leading explanations: (1) masked fallback — default/flat material reads as
skin/cloth at screenshot resolution/lighting (A/B close-ups would reveal flat
untextured surfaces); (2) per-case engine tolerance — added-material chains
may resolve through base shader-library lookups the byte classifier cannot
credit (no store hashes exist for added pids), i.e., conservative scoring;
replaced-pid chains (glasses) demonstrably do not tolerate. Native-res
close-ups of face/hands/props discriminate; files cannot.

## A/B mechanics captured in the map

17-row per-region sheet (glasses control, accessories, face/eyes/mouth/lashes/
hair/beard, torso/legs, FP hands, FP props, outfit variants, IV prop, LOD-
forcing face shot, regression guard) with broken/fixed-by-hide descriptors and
expected-vs-observed columns; screenshots straight to
`evidence/SCREENSHOTS/` (HA). Queue: after round-4 Townfall launch, may share
the Blade-dump session; needs GK's overlay builder applying GM's 42+1
HIDE directives (cross-referenced, non-blocking; condition A degrades
gracefully if GK isn't ready). Nine honest unknowns listed, led by "which
base meshes bind which replaced pid" (base UCAS never read).
