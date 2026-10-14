# Stellar Blade blind test — generic steps (2026-10-12, Mac-only prep)

Rule: no Stellar Blade-specific code, hand-written values or tuning. Only the
steps any new game needs. Anything that fails is fixed at engine/version level.

## Status tonight

- **No readable copy of its executable is on the Mac** (searched the worktree;
  only metadata files exist: `.superpowers/native-observations/stellar-blade-param.json`,
  `stellar-blade-metadata-receipt.json`). So the profile generator could not be run.
- Earlier discovery (2026-09-30) recorded the install at
  `/data/Stellar Blade 01.014.001 PPSA13197` (title PPSA13197, content
  version 01.014.001, `eboot.bin` present). Engine version is still unknown
  until the generator classifies the executable.

## Steps (after Townfall is done)

1. **Get a readable copy of the executable** (read-only FTP, game closed;
   nothing is ever uploaded back):
   - profiling copy: normal connection (the console's FTP server decrypts on
     download; expect magic `7f454c46`)
   - provenance check: a second download with decrypt OFF (`-Q "SITE SELF"`),
     first 4 bytes only (expect `4f153d1d` or `5414f5ee`)
   - record both sizes and sha256s; store under
     `.superpowers/native-observations/<title>-static-executable-<date>/`
     (local only — never committed or uploaded).
   Exact commands: `blade-prep/PREREQ-CHECKLIST.md` §1 (console IP comes from
   AGENTS.md; keep it out of shared reports).
2. **Run the profile generator automatically**:
   `python3 outputs/2026-10-08-l25-prep/blade-prep/run-blade-profile.py <copy>`
   (despite the folder name, the generator has no game names in its logic; it
   was tested positive on Townfall and negative on Astro Bot).
   - ACCEPTED -> profile JSON, then a normal labelled build with it.
   - REFUSED -> that is the result: the engine version needs its measurement
     row (engine-level work, not game-level).
3. **Deploy the same runtime + self-test + one Lua mod per type** under that
   title's own mod root and loadlist (row-check, writable folders, decrypt-off
   readback — the same gates as Townfall), then one launch.

## What the user needs to download (PC Nexus mods, one per type)

From the Stellar Blade Nexus page. These are **candidates**: I could only read
search summaries tonight, not the files, so each must be checked after download
(Lua-only? which UE4SS APIs? any Windows-only parts?) before it goes anywhere.

| Type | Candidate | Why this one | Watch out |
|---|---|---|---|
| Keybind + config file | SpeedMasterEve — nexusmods.com/stellarblade/mods/2205 | UE4SS Lua mod with a `config.lua` | key names must map to the PS5 pad |
| File read (save data) | NanoSuitRandomiser — nexusmods.com/stellarblade/mods/1344 | `Scripts/main.lua` that reads the save file | save path differs on PS5; tests our io shim |
| Camera / hooks | First-person mod — nexusmods.com/stellarblade/mods/243 | changes the camera via UE4SS | may include C++ parts; check first |

Not suitable: "Stellar Blade Mod Suite" (mods/3479) is a standalone Windows
panel (Windows-only = out of scope).

**Version caveat:** many of these were made for PC game versions (e.g. "tested
on 1.4.1"). The PS5 copy is content version 01.014.001. Mods for a different
game version are out of scope, so each candidate's stated version must match,
or the result is not a fair test.

Sources: [mods/3479](https://www.nexusmods.com/stellarblade/mods/3479),
[mods/2952](https://www.nexusmods.com/stellarblade/mods/2952),
[mods/243](https://www.nexusmods.com/stellarblade/mods/243),
[mods/284](https://www.nexusmods.com/stellarblade/mods/284),
[mods/1344](https://www.nexusmods.com/stellarblade/mods/1344),
[mods/2205](https://www.nexusmods.com/stellarblade/mods/2205),
[articles/84](https://www.nexusmods.com/stellarblade/articles/84).
