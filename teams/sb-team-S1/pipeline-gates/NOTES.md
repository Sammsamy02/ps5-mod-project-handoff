# Build-pipeline gates — adaptation for the profile-as-data runtime (Team S1)

Mac-only. These are the Mac build-pipeline gates that bake in Townfall/UE 5.6
assumptions. B3 (profile as data) moves the per-game values out of compiled
headers into `profile/<sha>.ini`, so the gates must check the GENERATED
profile's layout at ITS engine version, not Townfall's.

## field_gate.py — NO code change; invocation + a generated shim

`field_gate.py` already takes `--decl PATH` and reads the target engine version
from the declaration's `[NativeLayout]` header, pruning version-dead branches
for that version (so a UE4 profile makes it check the UE4 branches — READINESS
S2). Today the pipeline points `--decl` at the compiled
`CompiledNativeLayoutDeclarations.hpp`. In the data world there is no per-game
compiled header, so the pipeline instead points `--decl` at a **gate-input
shim** generated from the profile:

```
python3 -I gen/emit_gate_decl.py --profile profile/<sha>.ini --out /tmp/gate-decl.hpp
python3 -I harvest/field_gate.py --decl /tmp/gate-decl.hpp --root <candidate tree>
```

The shim (`gen/emit_gate_decl.py`, included) wraps the profile's
`[layout.members]`/`[layout.vtables]` blocks in the exact line layout
`field_gate`/`layout_binder_gate` expect; it is a gate INPUT only and is never
compiled into the runtime. Verified on the Mac: field_gate's own parser reads
81 members + `EngineMajor=5 EngineMinor=6 NonShipping=0` from the Townfall
shim. For Stellar Blade (UE 4.27) the shim carries `EngineMajor=4
EngineMinor=27`, so the prune targets 4.27 and the UE4 branches get checked
(satisfies S2). `TARGET_CONST_SPECS` are about hash-table policy, unaffected by
B5's ToString-pin removal.

## layout_binder_gate.py — NO code change; same shim

`layout_binder_gate.py` reads its declaration from `sys.argv[1]` (falling back
to the compiled header) and parses the members literal from `split('\n')[3]`.
The shim places the members literal on exactly that line, so:

```
python3 -I harvest/layout_binder_gate.py /tmp/gate-decl.hpp
```

runs the binder-width checks against the target game's layout. (It still
compiles the width-table TU with its archived argv, unchanged — that TU is
engine-generic.)

## no_detour_gate.py / fs_throw_gate.py — unchanged

Both already scan a tree via `--root`. Run them over the candidate tree AND the
new units (`ModProfile.hpp`, `ModProfileLoader.hpp`) as usual. Both PASS on the
S1 `new/` tree (8 C++ files, 0 violations) and on `base/` (6 files, 0) — no new
violations. The loader does ALL profile parsing in memory with no std::filesystem
and installs no detour, so neither gate's scope changes.

## l26_packaging_driver.py — real change (prose + pin list)

Today the packaging driver pins the per-game profile HEADERS as lineage/outer
overlays (`CodeSignatureProfile.hpp` 6-pin copy, `RuntimeBuildProfile.hpp`,
`JoinedLayoutPreflight.hpp`, `PakMountCodeProfile.hpp`, `pak_mount_pins.inc`,
etc.) and records `l26_profile_digest`/`l26_profile_promoted_facts` so the PRX
sha covers the per-game values (`l26_packaging_driver.py:94-203`). Under B3:

1. **The per-game data leaves the PRX.** Those headers become generic
   (loader-backed, identical for every game), so their overlay pins still exist
   but no longer vary per game — the SAME PRX serves every title. The driver's
   `l26_profile_digest`/promoted-facts machinery can keep pinning the generic
   headers; it just stops being a per-game identity.
2. **The profile file becomes a deployed, pinned loadlist row.** `profile/<sha>.ini`
   is added to the title root with role `profile` and a size+sha loadlist row.
   The existing `prelaunch_rowcheck.py` / loadlist row-check already enforces
   size+sha for every row, so the profile gets the tamper-evidence the compiled
   pins used to give — no new packaging gate needed, only the new row.
3. **B5 pin-count change.** `CodeSignatureProfile.hpp` now has 11 pins (the
   ToString pin id 4 removed). Any packaging assertion that the code profile has
   exactly 12 pins must become 11 (grep the driver for `,12` / `count` around
   the CodeSignatureProfile overlay). `kMaxPins` (16) is unchanged. No console
   `code_image_verified_pins` value is asserted by the driver (it is logged
   only), so the drop from 12→11 verified pins is informational.
4. **B3 install TU.** If the integrator adds `ModProfileInstall.cpp` (APPLY.md),
   it joins the runtime source selection like any other unit; its import delta
   is zero (the loader uses only the C++ runtime it already links).

No staged .py diff is provided for the packaging driver: its pins are keyed on
exact shas that only exist after the integrator rebuilds the profile headers in
loader-backed form, so the pin values must be re-derived at integration. The
change is mechanical (generic headers + one new loadlist row + 12→11), and every
existing packaging gate keeps its behaviour.
