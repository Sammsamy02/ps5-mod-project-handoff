# FIELDS track — what it does (plain version)

All of this is **Mac-only**. Nothing was sent to the console, nothing was built, nothing was
committed. The real candidate tree and the real profile are untouched.

## The problem

The runtime reads engine data at fixed positions ("fields"). Each position must come from this
game's proven layout list. The field gate is the check that stops a build when code reads a
field that is not on that list. Until now the gate quietly accepted 15 old fields as "known debt"
(a snapshot from earlier in the day). Launch 8 crashed exactly because a field was missing.

## What I found for the 15

- **9 were false alarms.** The code never reads them. The old gate matched functions by name
  only, so `GetSize`, `GetChildren`, `GetOwner`, `GetFlags`, `GetWorld`, `GetEnum` from one class
  were blamed on another class with the same function name.
- **3 do not exist on UE 5.6** (two old item-flag layouts and the 4.8–4.10 object list). The code
  that reads them only runs on other engine versions. For the object list I added an explicit
  version check inside the two functions themselves, so this is visible right where the read is.
- **4 are real and needed** (one of them is the launch-8 field). For each I found the position
  twice, from two independent places, and added it to the layout list:
  - object serial number 0x10 (used by "notify on new object"),
  - the current world inside a world context 0x2C0 (used by map-load hooks),
  - enum name list 0x40 (used by Lua enums and enum properties),
  - delegate signature 0x70 (used once at startup to find the object-property type).
  Sources: the game's own machine code, the game's own reflection data, and Epic's 5.6 headers
  compiled for PS5. UE4SS's own 5.6 table agrees on every value (used only as a cross-check).
  Values that could not be proven twice were not added. Details: FIELDS.md.

## The stricter gate (field_gate.py, staged copy; harvest/ is unchanged)

- No silent "known debt" any more. The old snapshot can still be used, but only when asked for
  (`--baseline-snapshot`), and it is printed as debt.
- `FIELD-GATE-OK` on a line counts only with a written reason. A bare marker is rejected.
- Only the layout list the PS5 actually loads counts as proof (the compiled declaration). The
  console ini is optional input, because the PS5 runtime ignores it.
- It understands version checks (`Version::IsAtLeast(5, 7)` etc.) and skips code that cannot run
  on this engine version. It also knows the PS5 build has the object hash tables switched off
  (checked against the source of that switch before it is used).
- It works out which class a call is made on (casts, `this`, variable declarations, base classes)
  before deciding which field is read. When it cannot tell, it stays strict, except that it drops
  a class that never appears in the file at all (audit list with `--verbose`; on our tree the
  only unproven field this dropped is the UGameViewportClient false alarm).
- It still prints how many files/calls/fields it checked and fails (exit 2) if it checked nothing.

Results (all in `gate-runs-final/`):
- patched tree + new layout list: **PASS**; all four tracks merged + new layout list: **PASS**;
- launch-8 tree: **FAIL** (UClass::ClassFlags, also SerialNumber with the old list);
- empty tree: **FAIL, exit 2**; unpatched base tree: FAIL; patched tree with the old list: FAIL.

## How it differs from PC

On PC, UE4SS ships these positions in its own table for every engine version. On PS5 we only use
positions proven for this exact game build, so each one has to be measured and added.

## How the next launch proves it ran

- `[FIELDS] staged layout facts: members=81 vtables=24` (was 74 member rows before).
- `[FIELDS] indirect-array walk refused: …` must **not** appear (it would mean the 4.8–4.10 path
  ran on 5.6).
- Existing markers that now get real data instead of an error: `[FL-MAP] … phase=loaded …` shows
  the world name; "notify on new object" no longer throws on the serial number; Lua enum calls work.

## Risks

- The declaration edit changes what the PS5 loads. It must go through one labelled, gated build
  and the one shipping-code reviewer. A wrong position would read wrong memory; each value here
  has two agreeing sources plus the UE4SS cross-check.
- The new gate is a smarter text scanner, not a compiler. Its class guessing can still miss a
  read in unusual code. Use `--no-receiver-filter --verbose` to audit. It takes about 80 seconds.
- The build script greps for "NEW UNPROVEN"; the new gate keeps that wording.
- The pinned console ini has stale listener offsets. Not used today. Fixing it is a console
  action and needs your OK.
