# Claude round-5 ADDENDA (user-confirmed) — 2026-10-10

Binding additions to the round-5 plan. These extend, never weaken, the
existing rules (Rule 10 no-game-names law, never-edit-mods, size-exact
admission, battery/no-build discipline, console-off).

## A1. COVERAGE IS PER ENGINE, NOT PER GAME
Target: UE4SS works on ~95% of Unreal games with ZERO per-game human
work. Each game's profile and self-test happen AUTOMATICALLY at first
launch. Same principle for every later engine.

## A2. STELLAR BLADE IS A BLIND TEST of the finished Townfall runtime
- Profile from the GENERATOR ONLY — no hand-written values.
- Same self-test (GB's game-agnostic ueselftest, unchanged).
- One PC UE4SS Lua mod per type: event/hook, value/array-write, config
  file (io), sustained tick, menu (Item Spawner class covers menu).
- Any failure is fixed in the GENERIC runtime or generator at
  engine/version level (e.g. a UE4 handling that every UE4 game gets).
- Rule 10 extends to games: NO Stellar Blade names, offsets, or special
  cases anywhere in the runtime. Ever.
- Pak/asset mods wait for capabilities #7/#8.
- User's own copy only (decrypt-off law applies to acquisition).

## A3. PART 2 PRIORITY ORDER (after Townfall Mac-only work)
Run in PARALLEL:
  (a) Stellar Blade prep: per-version layout tables (UE4 through 5.6)
      + the generator, since it is next on the console;
  (b) Capabilities B -> C -> A, Mac-only, staged ready for console
      testing.
Then other-engine research (GH's output is input, not priority).

## A4. LATER, NOT NOW: special-case kit
For unsupported or modified-engine games: open-source release + an
adaptation prompt users can give their own AI + reading PC per-game
UE4SS config files. The open-source release MUST EXCLUDE Epic engine
source and any Sony SDK material (licensing law — audit gate before any
release).

## A5. END-USER FLOW (the product shape)
- The profile generator AND self-test run ON THE PS5 at a new game's
  first launch.
- They use per-version layout tables that SHIP WITH THE TOOL — measured
  once on the Mac per Unreal version, never per game.
- No Mac, no AI, no manual step per game.
- The mod manager (GG) replaces all manual FTP/loadlist installs.

## Work mapping
- GB self-test: already conformant (game-agnostic law).
- GF Phase A #2 scanner: REORIENT to per-version layout tables +
  on-console generation (agent GI carries the unified design).
- GC ladder v2: the standard check becomes the AUTOMATIC first-launch
  flow (profile -> self-test -> mod-per-type), documented as such.
- GH: priority context only — its ranking informs, execution follows
  (a)/(b) first.
- NEW GI: first-launch automation + 95% coverage architecture.
- NEW GJ: Stellar Blade prep (per-version tables UE4-5.6 + generator),
  blind-test law baked in.
