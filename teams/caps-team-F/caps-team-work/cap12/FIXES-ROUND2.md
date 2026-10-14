# Capability #12, round 2: a safer parent chooser (Team F)

2026-10-13. **Everything here is Mac-only.** There was no console contact, no
build, and no change to mod files, the candidate tree or other teams' folders.
The inputs are the game's own files that are already on the Mac, plus the
integrator's verified fetches.

## Status

- **Fixed on the Mac.** The chooser no longer picks the liquid material, and it
  now fails closed when a class has only one source.
- **By default it reparents 0 of 11. Why:** the files cannot prove whether
  Leon's body and skin materials are opaque or masked. They are surely not
  translucent, and the hair is provably masked. The reviewer rated this a
  blocker under the two-source rule, so the tool refuses.
- **With two explicit user decisions** (`--allow-partial` and
  `--accept-unproven-opaque`), 10 of Leon's 11 instances get the game's own
  opaque, lit cloth material `M_MRO_Cloth_NonVT`. Both sources agree on that
  parent's class, and 74 other game materials compile the same shader types.
- **Still not ready to launch:** the user must take those two decisions, and
  the round-1 launch-card steps still apply. A round-3 fetch is optional.

## What was wrong

The round-1 chooser matched on parameter names only. It picked
`M_Simon_Medical_Liquid_Inside_Tube` for all 11 instances because that
material shares the name "BaseColor" and has few names. Read from data, that
material is **masked** (both sources agree), not translucent. Masked is still
wrong for Leon's opaque body: it would cut holes.

## What changed

### 1. Material class from two sources (`material_class.py`, `asset_registry.py`)

For each candidate parent, the tool reads the domain, the blend group
(opaque / masked / translucent family) and lit or unlit from two sources:

- **Source 1: the game's own `AssetRegistry.bin`.** This file is in its main
  pak. Two separate extractions have the same sha256 `bd905265…`. It holds
  `MaterialDomain`, `BlendMode`, `ShadingModel` and `ShadingModels` for all 418
  game materials. A material instance follows the registry's `Parent` tags up
  to its master. Where the instance's header was fetched, that header must
  import the same parent.
- **Source 2: the candidate's own compiled PS5 shaders.** The path is: the
  store entry, then the shader library manifest, then each shader's type
  hashes. The type hashes come from the game's `ShaderTypeInfo-*.stinfo`, one
  entry per library shader, and 31,893 of 31,893 shaders match. The tool
  compares that set with every **other** game material that compiles exactly
  the same set (the candidate and its own master chain are left out). The
  value counts only when at least 3 peers exist and all of them agree.

The two sources must agree, or the tool fails closed and writes why.

Then the gate refuses any parent that is translucent, non-surface or unlit. It
also refuses special shading models (Hair, Eye, SingleLayerWater,
ThinTranslucent): one source is enough to refuse. The names of the shading
model bits are learned from the registry itself, not hard-coded.

**Honest limit:** the exact lit model (DefaultLit vs Cloth vs Subsurface) is not
visible in compiled shader types. I measured this on the 418 game materials. It
has only one source, so it is never used to accept a parent. It is shown as a
look note.

### 2. The mod side: two readers

- **Reader A:** the shader type hashes inside the mod material's own PC shader
  map. They are the same hashes as the game's `.stinfo`. The tool rules out
  every class of game material that cannot produce that set.
- **Reader B:** the base-pass buffer named in the mod's PC shader code
  (`OpaqueBasePass` or `TranslucentBasePass`).

Real result:

- `M_Leon_Hair` is **masked**. Both readers agree, and its type set matches 15
  masked game materials and nothing else.
- `M_Leon_Body` and `M_Leon_Skin` are **not translucent**, but opaque vs masked
  is **not provable**. Their 7 type hashes are also compiled by game materials
  of other groups: 40 opaque, 2 masked and 13 translucent. Masked materials
  with no masked-only shader types exist in the game.

So these are marked `unproven_opaque` and end as known-checker. The only way
past that is `--accept-unproven-opaque`: a user decision, written into every
row it affects. If the mod material were really masked, its cut-outs would be
filled in. That is a look problem, not a crash. It would not make Leon
see-through.

Only a parent of the same group is allowed: an opaque instance gets an opaque
parent, and a masked one gets a masked parent. Every refused parent is listed
in the row with the reason.


### 3. Parameter mapping by role (`param_roles.py`)

- **Exact name:** the engine itself binds parameters by name. It now counts
  only when a fetched game instance binds a texture to that name, which proves
  it is a texture slot. This follows the review.
- **Role rename** (base colour, normal), allowed only with two sources:
  1. The name tokens on both sides.
  2. The textures on both sides:
     - **Mod side:** the bound mod texture's format. BC5 means a normal map.
       DXT1/DXT5/BC7 mean a colour texture.
     - **Base side:** a base texture bound to that parameter in a fetched game
       instance. Its registry tags must say `SRGB=True` (colour) or
       `TC_Normalmap`.

  Texture bindings are decoded from the instance bytes. The decoder works on
  both the PC mod cook and the PS5 game cook, and it fails closed when the
  layout is ambiguous.
- **Packed masks (ORM / MRO …) are never remapped.** The channel order comes
  from the name only (one source), and a rename cannot swap channels. The loss
  is written into the row.

Two mod parameters that would map to the same base slot are both dropped. The
row records the reason.

Real result for Leon's body: `BaseColor → BaseColour` (renamed, two sources),
`Normal` (exact). `ORM` is lost, and the parent's default MRO texture is used.
The derived package was built with `mic_reparent.py` against the **real**
fetched parent header (the two-source hash law). Its texture bindings decode
as `BaseColour`, `Normal`, `ORM`. A host test covers this.

New rule: a partial parent must carry at least half of the mod's parameters
(`MIN_PARTIAL_COVERAGE = 0.5`). Leon's hair would get only 1 of 4 (on the masked
liquid material), so it stays a known checker.

### 4. A wider pool: new fetch plan (`master_fetch_plan.py`)

Plan only; nothing was fetched. The file is
`evidence/fetch-round3-masters.json`: **356 rows, 21,045,009 bytes**, in the
#12 row shape (`remote_file`, `pieces`, `local_name`, `length`,
`expected_iohash20` when the row is a whole chunk).

The plan contains every registry master that passes the two-source gate (127
in the game), has PS5 shader maps, is not overridden by the mod and is not
fetched yet:

| Priority | What | Rows | Bytes |
|---|---|---|---|
| 1 | Masters with skeletal-mesh evidence now: `M_MRO_Cloth`, `M_SkinBasic_Hands`, `M_Head_Baked`, `M_skin_unified_baked`, `M_teeth_unified` (whole chunks) | 5 | 293,794 |
| 2 | Skeletal-mesh headers not fetched yet (256 KiB prefix, or the whole chunk if shorter) | 66 | 12,769,805 |
| 2 | Masters without usage evidence yet (whole chunks) | 116 | 4,316,552 |
| 2 | Up to 2 small game instances per master (whole chunks), for texture-binding evidence | 169 | 3,664,858 |

The `.p1.json` and `.p2.json` files hold each priority alone. This split is
needed because `ftp_range_fetch.py` drops `priority` for #12-shaped rows, so
`--max-priority` has no effect on them.

## Real-data results (Mac-only)

| Plan | Reparent | Partial | Known-checker |
|---|---|---|---|
| Round 1 default | 0 | – | 67 |
| Round 1 `--allow-partial` | – | 11 (all to the masked liquid material) | 56 |
| **Round 2 default** (`evidence/reparent-plan-r2.json`) | 0 | – | 67 |
| Round 2 `--allow-partial` (`evidence/reparent-plan-r2-partial.json`) | – | 0 (opaque not provable) | 67 |
| **Round 2 `--allow-partial --accept-unproven-opaque`** (`evidence/reparent-plan-r2-partial-accept-opaque.json`, user decision) | – | **10, all to opaque lit `M_MRO_Cloth_NonVT`** | 57 |

The candidate pool is now 14. Excluded, each with its reason:

- 2 translucent
- 2 unlit
- 1 Hair model
- 1 ThinTranslucent model
- 2 where the two sources could not be read

## Tests

`bash tests/run_tests.sh` gives **ALL PASS: 51 Python tests** (39 before) plus
the Lua probe test (21 slot lines).

New tests, positive and negative:

- **Registry reader:** real counts; a corrupt magic, an off-by-one count and a
  truncated file all fail closed.
- **`.stinfo` pairing:** a wrong library pairing is refused.
- **Two-source cases:** agree, disagree, too few peers, mixed peers, a missing
  source 1 or 2, a header-parent mismatch, a candidate that is never its own
  peer, and an empty population.
- **Translucent candidate refused:** synthetic, and real (`M_Medical_Tubes`).
- **Masked parent refused for opaque instances,** and the reverse.
- **Mod readers:** real Leon data, plus planted disagreement. Also: unproven
  opaque fails closed, and a too-small blend group is refused.
- **User decision honoured:** with `--accept-unproven-opaque` only, and written
  into the row. Duplicate rename targets are dropped, and an exact name that is
  not proven to be a texture slot is not counted.
- **Role table.**
- **Binding decode:** real PC and PS5 data, plus header-only data.
- **Mapping negatives:** wrong format, a non-colour base texture, an ambiguous
  target, a name collision, an unknown role.
- **End-to-end on real data:** the plan, then `mic_reparent.py` with the real
  parent header.
- **Fetch-plan row shape and content.**

The runner is stricter. It fails when:

- zero tests run, or any test fails
- a test is skipped
- the number of tests run differs from the number defined
- the staged `PATCH/` copy differs from `tools/`

I checked each of these with a planted fault on a scratch copy. Four planted
code faults were each caught: the translucent gate turned off, the blend
matching turned off, the candidate counted as its own peer, and the mod-format
check turned off.

## Independent review (one reviewer, 14 findings) and what I did

The reviewer confirmed that the fetch rows, the leave-one-out peers, the
parent gate and the packed-mask rule are correct, and that the tools hard-code
no game names.

| # | Finding | Done |
|---|---|---|
| 1 BLOCKER | "Opaque or masked" was treated as opaque | Fails closed now (`unproven_opaque`); only `--accept-unproven-opaque` overrides it |
| 2 | A small translucent group could be "ruled out" with no evidence | Each surface blend group needs at least 10 game materials, else the class is unreadable |
| 3 | Two mod parameters could rename to one base slot | Both are dropped, with the reason |
| 4 | An exact name was counted without knowing it is a texture slot | Counted only when a game instance binds a texture to it |
| 5 | Skeletal rows were not filtered against mod overrides | Filtered |
| 6 | A base instance's own blend overrides are not in the registry | Disclosed. A Material parent is always ranked first, and the chosen parent is a Material |
| 7 | The mod's lit/unlit is not read | Disclosed in `look_notes` (unchanged) |
| 8–11, 13, 14 | Notes | Range checks on all registry value kinds; unknown handle bits refused; the `.stinfo` file name must name its library; the scan's off-by-one fixed; docstring fixed; argument limits; the runner sync check runs both ways |
| 12 | Source 2 uses other materials' registry tags | Said so in the docstring |

## Open items / disclosed gaps

0. **User decision needed:** accept "opaque, not provable against masked" for
   Leon's body and skin (`--accept-unproven-opaque`). Without it, #12 fixes
   nothing for Leon.
1. **Look loss (needs the user's OK):** Leon's ORM texture is not used.
   Roughness, metal and AO come from the parent's default. The shading model
   is the parent's (DefaultLit + Cloth), which is one source only.
2. **Leon's hair stays checkered** until a masked, lit, non-Hair game parent
   with at least 2 of 4 parameters is found. The round-3 fetch may find one.
3. Morph-target and cloth usage of the parent are still not checked.
4. A derived package for each `reparent-partial` row still needs the overlay
   action "ship derived package bytes" (#7/#8 owner, unchanged from round 1).
   Pass `--rename BaseColor=BaseColour` to `mic_reparent.py`, as listed in
   `param_renames`.
