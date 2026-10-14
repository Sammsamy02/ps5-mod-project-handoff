# LAUNCH CARD — cap02 shadow check (rides along, no launch of its own)

## In one sentence
A small log-only check that rides along with the NEXT capability launch that
happens anyway (#7 paks, first in the order). It needs no extra launch and
changes nothing the game runs.

## What it is, plainly
- The runtime already uses a fixed, proven field layout (R11).
- This adds one log line at startup. It rebuilds the layout from the new
  engine-level data and checks it is still identical to the proven one.
- It is a **drift alarm** for future layout edits. It does **not** prove
  capability #2 ("a new game makes its own profile"). That needs a separate
  test later (see the end).

## What the integrator does
1. Merge with the #7 changes: `python3 -B PATCH/apply_cap02.py`, then the same
   command with `--execute` (details in `PATCH/APPLY.md`).
2. Run the normal gates, build once, record the build sha in the ledger with
   label `+cap02-shadow`.
3. Do NOT apply `PATCH/02-NOT-SHIPPABLE-...diff`. It is not shippable.

## What the user does
Nothing extra. Play the #7 launch as planned.

## What to look for in the log (decoded `ue4ss.decoded.log`)
PASS:
`[AutoProfile] shadow parsed=1 verdict=MATCH pack=ue5.6-prospero-x86_64-sysv-shipping-statsoff pack_digest=4711476663f42838e3afd6bd5b788a4a38cc57962b255e69202cc7cd3f99aaf5 member_rows=81 vtable_rows=24 detail=pack and renderer reproduce the compiled layout byte-for-byte (drift check, not arming)`

Then the usual `[FIELDS] staged layout facts: members=81 vtables=24` and the
`[BuildProfile]` / `[LayoutPreflight]` PASS lines, same as the last clean launch.

## If something looks wrong
- `verdict=DIVERGE` or `parsed=0`: a finding, not a crash. The game still uses
  the proven layout. Note it in the report; it does not fail the #7 launch.
- A crash or a `[BuildProfile] ... result=REFUSE` that the last clean launch did
  not have: roll back as the #7 card says. Last clean build: R11 `bfd95db6`.

## Not in this launch: the real capability #2 test
Later, one Townfall launch with the compiled layout turned OFF and the
automatic path ON, expecting the same `[FIELDS]` / `[BuildProfile]` PASS lines.
That needs the PATCH/02 pieces listed in `PATCH/APPLY.md` first (live engine
detection, a profile generator, and engine data cleared for arming).
