# LAUNCH CARD — capability #10 (C# mods, embedded Mono)

Plain words. #10 is **last** in the capability queue and must never hold up
Stellar Blade. This is a self-test of the C# runtime (a tiny built-in C#
program), not a loader for downloaded C# mods yet.

## What this build adds

A C# runtime (Mono, interpreter only) that can run one built-in C# program (the
"probe"). It stays completely asleep unless a C# mod is switched on. With no C#
mod, the game runs exactly like the pre-#10 build.

## What gets deployed, and where

Console root is `/data/modbridge/roots/PPSA33286` (the title's fixed root).

1. **Runtime assets** (needed only when the C# runtime starts). Each needs the
   user's OK to upload; read each back after upload (decrypt-off), then row-check:
   - `mscorlib.dll` sha `86364b78…` → `/data/modbridge/roots/PPSA33286/lib/mono/4.5/mscorlib.dll`
   - `config` sha `30cea9e8…` → `/data/modbridge/roots/PPSA33286/etc/mono/config`
   (The code sets Mono's library prefix to `<root>/lib` and config prefix to
   `<root>/etc`; Mono itself appends `mono/4.5` and `mono/config`.)
2. **A C# probe mod** so the runtime actually starts. Create one enabled mod
   folder with a `csharp/` directory holding the built-in probe assembly:
   - `/data/modbridge/roots/PPSA33286/ue4ss/Mods/CsProbe/csharp/Probe.dll`
     — the file is `cap10/managed/Probe.dll` sha `c328dca1…` (byte-identical to
     the probe embedded in the PRX; that exact match is what arms the runtime).
   - enable it with a line `CsProbe : 1` in
     `/data/modbridge/roots/PPSA33286/ue4ss/Mods/mods.txt` (safest: the folder has
     no Scripts, so an `enabled.txt` might not reach the activation snapshot).
   - the runtime also refuses to start unless both runtime files from step 1 exist
     at exactly their sizes (4,634,112 and 3,097 bytes); if not, UE4SS.log shows
     `[CS] refused: C# runtime file missing…` / `…wrong size…` and the game runs
     without C#.
   With no such enabled C# mod, the C# runtime does **not** start and the launch
   only confirms that (see "no-C#-mod check").

## The one launch (after the PRX upload + read-back + row-check, user OK each)

Read the log only after confirming the game is closed. Lines come from
`diagnostic_event`, printed as `… CAP10_<tag> value=0x%08x`.

PASS needs ALL of:
- **ready callback / interpreter startup**: `CAP10_STAGE value=0x00000007`
  (Invoked). `0x00000008` (SignalStateFailed) or anything else = FAIL.
- **exact probe image loaded**: `CAP10_IMAGE value=0x00000001`.
- **both native exports called, with the resolver**: `CAP10_RESOLVE value=0x00000007`
  (all three bits: load + abi + probe).
- **two distinct nonzero cookies**: `CAP10_COOKIE1` and `CAP10_COOKIE2` both
  nonzero and different from each other.
- **Run → exact managed exception → recovery → cleanup**:
  `CAP10_RUN value=0x00000001`, `CAP10_THROW value=0x00000001`,
  `CAP10_RECOVER value=0x00000001`, `CAP10_DONE value=0x00000001`.
  Call tallies: `CAP10_R1_CALLS value=0x01010101`,
  `CAP10_R2_CALLS value=0x01010101`, `CAP10_T_CALLS value=0x00000000`.
- **the `[CS]` lines** (UE4SS.log): `[CS] C# probe requested by an enabled C# mod…`
  then `[CS] C# probe finished: stage=7 … done=1`.
- game reaches the playable scene; **all other mods still work**; **no stutter**;
  self-test runs once; klog clean; record the init→cleanup time.

## no-C#-mod check (prove Mono stays out)

On a boot with **no** enabled C# mod (remove the `CsProbe` enable, or test before
adding it): UE4SS.log must show
`[CS] C# runtime not started: no enabled C# mod requested it (no init, no threads, no signal changes)`
and **none** of the `CAP10_*` lines. This is the proof that, by default, nothing
from Mono runs.

## Note for the user (plain)
- Mono takes over the crash-handler signals and maps extra memory **only while
  the C# runtime is running** — i.e. only when a C# mod is on. No C# mod = none
  of that.
- The game stays offline: the C# runtime's network-name lookups are wired to a
  built-in "network is down" answer, so no SceNet call leaves the console.
- This proves the C# runtime works. Running arbitrary downloaded C# mods
  (BepInEx/MelonLoader/Unity) is later, bigger work.
