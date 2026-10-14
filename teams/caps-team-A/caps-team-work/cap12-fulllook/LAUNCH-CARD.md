# Launch card: #12 Leon full look (one launch)

**For:** the integrator (console steps) and the user (what to look at).
**Status: Mac-only.** Nothing has been tested on the console yet.

## Before launch (integrator)

1. Merge per `APPLY.md` and run the tests (expect 51 + 24 OK).
2. Make the plan with all three header rounds and both user decisions (`--allow-partial --accept-unproven-opaque`).
3. Feed the plan to Team A's overlay step (cap12-overlay). That step builds the derived Leon packages and installs them. It has its own launch rules.
4. No console fetch is needed for this launch.
5. Usual rules apply: user OK for each console action, the row-check, one launch, game offline.

## What should change on screen (versus the round-3 plan)

| Part | Expected on PS5 | Why |
|---|---|---|
| Jacket, trousers (UpperBody, LowerBody) | Leon's own colours and fabric detail (bumps, seams). Even shine, no baked crease shadows. | Colour and normal maps kept; the ORM map is lost. |
| Pistol, knife, watches, props | Leon's colours and surface detail, but they look painted or plastic, not metal. | The ORM map (holds "metal" and "shininess") cannot be used. |
| Face, mouth | Leon's face colours and skin detail. Matte and a bit dry: no soft skin glow. | The game's skin shader only takes virtual textures, and Leon's textures are plain. So these use the game's cloth shader. |
| Eyes, eyelashes | Leon's eye and lash colours. Eyelashes are solid strips, as on PC. | Same cloth parent. |
| Hair | **Still the gray/white checker.** | The game's only hair shader takes its cut-out from a mask Leon does not have. |
| Simon's hidden parts (glasses, canula liquid, some face levels of detail) | May show as checker where Leon's PC version shows nothing. | PC hides them with an "invisible" material that has no PS5 version. Not fixed by this patch. |

Round 3 sent 10 parts to `M_SkinBasic_Hands`. Those parts would most likely have shown **no Leon texture at all**, because that shader's slots are virtual-texture only (Mac-only reasoning). This round sends them to `M_MRO_Cloth_NonVT`, whose slots take plain textures.

## What the user checks (one session, game closed before any log read)

Please answer each item with yes or no, and add a photo if you can.

1. Does Leon's jacket show its real pattern and colours, with no checker? (yes / no)
2. Do the trousers, boots and belt look right in colour? (yes / no)
3. Is the face Leon's face (skin colour, stubble, eyebrows painted in the texture)? (yes / no)
4. Do the eyes look like eyes (not white or checkered)? (yes / no)
5. Pistol, knife, watch: right colours? They may look "plastic"; that is expected. (yes / no)
6. Hair: still a checker? (expected yes; anything else, please describe)
7. Anywhere else with a checker: glasses, a tube or bag at the arm, or the face at a distance? Say where.
8. Any crash, freeze or stutter that was not there before? (yes / no)

## Pass / fail

- **Pass:** items 1 to 5 are yes, item 8 is no.
- **Expected limits (not failures):** item 6 (hair) and the "plastic" metal in item 5.
- **Fail:** any of parts 1 to 5 shows a checker, a plain white surface, or a black surface. In that case:
  - pull the klog (after the user confirms the game is closed);
  - look for material fallback or texture-type warnings;
  - roll back to the last clean build.
