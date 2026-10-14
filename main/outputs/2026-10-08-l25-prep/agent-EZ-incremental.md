# Agent EZ — incremental compilation for the l26 build driver (2026-10-09)

## What landed

`outputs/2026-10-08-l26-runtime/build_variants.py` (the ONLY file edited; sha256
`f61af2e8640c9e315c97e40c94d535359fc6b785ef7551f0e5762fb70162a966`, 911 lines)
now supports `--incremental` and `--base-build DIR`. Full mode (the default)
is byte-for-byte the previous behavior: zero new gates, same selection
semantics (verified against the preserved pre-EZ plan and the executed builds).

## The flag

```
python3 outputs/2026-10-08-l26-runtime/build_variants.py --plan --incremental [--base-build DIR]
```

1. **`--incremental`** — the fresh set is computed ONLY from units whose
   SOURCE .cpp carries a CHANGES row (`G-inc-cpp-only-fresh-set`). Header
   rows still populate the vfs overlay/replacements (headers still RESOLVE to
   the l26 candidates — the G12 routing checks run on every compiled unit),
   but they no longer cascade consumers into the fresh set: consumers keep
   the retained objects from the previous build.
2. **`--base-build DIR`** — which previous build's objects to retain from.
   Accepts a variant dir (its `VARIANT-MARKER.json` key must match the
   variant being built) or a pair root containing `l26-A*/l26-B*`. Default:
   the most recent COMPLETED `l26-A*` / `l26-B*` per variant key (by pass2
   receipt mtime, marker-key matched, so a skip-token B object can never be
   retained into production variant A). The l25-A BASE is never an
   incremental base (its objects predate the variant defines).
3. Retained units adopt the previous build's pinned provenance (object,
   source, .d, argv) via per-pass remap gates
   (`G-inc-retain-objects-from-previous-build`), and the existing link census
   (G6, 391 inputs) consumes those exact files.

## Fresh-count comparison (final clean pair, same tree state, 2026-10-09 ~21:30)

| mode | fresh units | retained units | plan steps | G2 row drift |
|---|---|---|---|---|
| full (default, unchanged) | **199** | 175 | 810 | 0 |
| `--incremental` | **20** | 354 | 94 | 0 |

- 20 = exactly the current .cpp-row units (`unreal-uobject`,
  `unreal-uobjectarray`, `unreal-unrealinitializer`, `luamod`, `lua-luauobject`,
  the three MemberVariableLayout wrapper TUs, etc.). A future single-.cpp
  edit after a full build is 1–5 units → ~1–2 min instead of ~15.
- Both variants resolved to the new `l26-A` / `l26-B` builds and verified
  clean: `G-inc-retained-inputs-unchanged-A/B` ok=true, **0 forced**, 354/354
  remapped per pass; define coverage proven for both tokens; G2t true; G12
  resolution checked all 20 compiled units, 0 violations.
- Full mode on the same tree: 199 fresh, 810 steps, **no G-inc gates** —
  identical to the executed 20:5x builds and to the pre-EZ driver (the 12:41
  pre-EZ plan's 177/66-gate/722-step numbers differ only because the tree had
  36 rows then vs 38 now).

Evidence: `outputs/2026-10-08-l26-build-variants/plans/`
`PLAN.full-post-EZ.json` + `.stdout.log`, `PLAN.incremental-post-EZ.json` +
`.stdout.log` (the live `PLAN.json` is the last incremental plan). The prior
coordinator plan is preserved untouched as `PLAN.preserved-pre-EZ-20261009T1241.json`.

## Safety notes

- **Operator warning on every incremental run**: a JSON warning lists each
  skipped header row with its consumer count (today: NameTypes.hpp 168,
  String.hpp 196, Utf16Utf8.hpp 177, UObjectArray.hpp 162, three
  MemberVariableLayout wrappers at 125 each, LuaUObject.hpp 33, FileDevice.hpp
  16, …) plus the mixed-header ODR risk and the upload-grade reminder.
- **No silent staleness (stronger than the directive)**: every unit slated
  for retention is re-verified against the previous build's OWN pins (source,
  object, .d, every dep file pinned by its SOURCE-SELECTION). Any drifted
  unit is forced back into the fresh set (`stale_forced_fresh`). This was
  exercised for real tonight: the 20:04 run forced 151 units after a parallel
  agent edited NameTypes.hpp/LuaUObject.hpp at 19:59 (G2 drift = 4 rows), and
  the 20:46 run refused 179 variant-A objects whose base dir was being
  renamed/rebuilt under it. Both are the safety layer working, not failures.
  Consequence: right after real HEADER changes, the first incremental build
  legitimately rebuilds their consumers ONCE; the 20-unit steady state (and
  the 1–5 unit .cpp-only case) applies once the base build pair reflects the
  current tree.
- **Waived gates, explicitly**: G12b consumer-recompile coverage becomes a
  `mode: incremental` risk report; G2t listener presence waives (with proof
  requirement) only if a listener unit falls outside the cpp set (today all
  four have .cpp rows, so it still passes true). G12 RESOLUTION is never
  waived. G10 cross-variant deltas accept, beyond current define units, only
  units byte-identical to each variant's previous same-variant receipt
  (inherited divergence); anything else still fails.
- **Define coverage is hard**: a variant define reaching a retained unit must
  be proven by the previous same-variant marker's `define_units`, else
  `G-inc-define-units-covered-*` fails the build (prevents retaining
  pre-define l25-A objects into a variant runtime).
- **Upload grade**: the full build remains the default and the ONLY
  upload-grade mode; `--incremental` must be passed explicitly and is stamped
  `NOT upload-grade` in every receipt/plan/marker it produces. `--base-build`
  without `--incremental` is a CLI error.
- **Execute unchanged**: `--execute` still refuses to overwrite existing
  variant dirs — archive/rename the previous build first (the established
  `l26-A.<tag>-<hhmmss>` workflow), then `--execute --incremental` (the
  default base resolution or `--base-build` picks the archived dir).

No source trees, no receipts, and no other drivers were touched; the base
driver `build_runtime.py` is still imported, never rewritten. No console
contact, no native execution, plan runs only.
