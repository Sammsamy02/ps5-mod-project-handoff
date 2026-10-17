# ORIGINAL-MOD-IDENTITY — acceptance-proof prep for the unmodified TownfallItemSpawner

Agent FN, 2026-10-10 (console in REST MODE — prep only; no console contact,
no edits to any mod file, no builds). Companion to `FROZEN-KNOWN-GOOD-20261010.md`.

Purpose: Claude's directive — the acceptance proof for the frozen build
efbfc656 is a rerun with the BYTE-IDENTICAL ORIGINAL mod. This record pins
the original's identity, proves the archived console copy already matches it
byte-for-byte, and gives the wake-time re-verification commands.

## 1. The console copy (archived at menu-up session)

- Path: `outputs/2026-10-08-l25-prep/evidence/townfall-main.lua.console-copy`
- sha256: `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`
- Size: 21,415 B — ASCII text, 746 lines
- Archived 2026-10-10 (mtime 00:58 local) alongside the menu session log;
  referenced by CLAUDE-MENU-UP-SPAWN-BLOCKED.md §9 ("Mod's console main.lua
  archived").

## 2. The ORIGINAL unmodified source — FOUND locally (no user zip needed)

The original Nexus release is retained in full, with its intake receipt:

- Original ZIP: `.superpowers/native-observations/item-spawner-real-mod-intake-1/original.zip`
  - sha256 `a9f7ed004dd053e02f6206d8ed6886174f9b5aaecf55244447a2c5fb9d6fa7fb`, 5,939 B
  - Per `intake.json` (same dir): source_url
    `https://www.nexusmods.com/silenthilltownfall/mods/95?tab=files&file_id=218`
    (TownfallItemSpawner V1.0 — Nexus mod 95, file 218), original download
    `~/Downloads/TownfallItemSpawner V1.0 95 1 2026-09-26T16-33Z pZAtDj5g8.zip`,
    `mod_edits: false`, `console_write: false`. Exactly 2 members:
    1. `TownfallItemSpawner/Scripts/main.lua` — extracted at
       `member-00000000.bin`, 21,415 B, sha256 **f586043a…b6559658**
    2. `TownfallItemSpawner/enabled.txt` — empty (0 B), sha256
       e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
       (sha of the empty file), extracted at `member-00000001.bin`
- The zip and members were re-hashed by agent FN tonight and match intake.json exactly.

## 3. Diff verdict: console copy == original — BYTE-IDENTICAL

```
cmp  .superpowers/native-observations/item-spawner-real-mod-intake-1/member-00000000.bin \
     outputs/2026-10-08-l25-prep/evidence/townfall-main.lua.console-copy
# -> exit 0, no output  (CMP: BYTE-IDENTICAL)
diff (same pair) -> no differences
```

- Original main.lua sha f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658
- Console copy sha    f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658
- Both 21,415 B. **Zero byte differences.** The mod that ran at menu-up was
  the unmodified Nexus release.

Corroborating trail (all f586043a…658, 21,415 B — the staged/release lineage
never diverged): the 2026-10-04 staged packages
(`outputs/2026-10-04-zcode-team/worker-settings-v3-package/package-v3/`,
`worker-itemspawner-bundle/bundle/`, `worker-loadlist-final/final-package/`,
…/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua); the l15/l16 console-test
provenance archives
(`outputs/2026-10-07-l15-full-build/console-test-20261007T170125Z-b1b2fd00/provenance/original/TownfallItemSpawner/Scripts/main.lua`,
`outputs/2026-10-07-l16-full-build/console-test-20261007T183202Z-2318a17f/provenance/TownfallItemSpawner/Scripts/main.lua`);
and the Oct-8 audit copies (`2026-10-08-l21-audit/.../prior-console-evidence/main.lua`,
`2026-10-08-l22-package-r2/claude-review-.../unchanged-mod/main.lua`,
`2026-10-08-menu-path-audit/source-cache/main.lua`). The l22 Claude review
packet already recorded: "The retained original TownfallItemSpawner main.lua
is unchanged, 21,415 bytes, SHA f586043a… (Nexus Townfall mod 95, file 218,
v1.0)"; the l16 mission records: "original ZIP SHA a9f7ed00…: both release
files exactly match the console copy; no mod edits/extra files."

False candidates (do NOT use as "the original" — test fixtures/faults, listed
so nobody mistakes them):
- sha 91276e66… (14 B `print("spawn")`) — uninstall-fault test roots (31 copies)
- sha 7bfeac56… (12 B `SPAWNER_MAIN`) — worker-7-installer test fixtures
- sha 8952e9d7… (21,414 B) — deliberate `root-badsize` fault-injection copy
- sha 2dd9d05e… (589 B) — claude-takeover example-lua-mod packaging stub
  (MOD-CENSUS.md: "built example, NOT Nexus mod 95")

## 4. Wake-time acceptance verdict + re-verification commands

Verdict template (fill in after the wake-time fetch):

```
console mod sha == original sha == f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658
  (21,415 B; cmp vs member-00000000.bin empty; enabled.txt 0 B)
=> UNMODIFIED PROOF — the menu-up acceptance run used the byte-identical
   original TownfallItemSpawner V1.0 (Nexus mod95/file218, zip a9f7ed00…7fb).
```

Commands (run at wake, console at ps5-host-not-configured.invalid, FTP 2121; read-only fetch —
NEVER edit/normalize the mod, never push anything to the mod paths):

```
CONSOLE=ps5-host-not-configured.invalid
MODR=/data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner
ORIG=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/item-spawner-real-mod-intake-1

# 1) fetch the live console copy (plain RETR; .lua is not a SELF/SPRX, no decrypt applies)
curl -s --max-time 15 "ftp://$CONSOLE:2121$MODR/Scripts/main.lua" -o /tmp/console-main.lua
curl -s --max-time 15 "ftp://$CONSOLE:2121$MODR/enabled.txt"      -o /tmp/console-enabled.txt

# 2) identity
shasum -a 256 /tmp/console-main.lua   # expect f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658
wc -c        /tmp/console-main.lua   # expect 21415
shasum -a 256 /tmp/console-enabled.txt  # expect e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 (0 B)

# 3) byte-exact comparison against the ORIGINAL release member
cmp "$ORIG/member-00000000.bin" /tmp/console-main.lua && echo "MAIN IDENTICAL"
cmp "$ORIG/member-00000001.bin" /tmp/console-enabled.txt && echo "ENABLED IDENTICAL"

# 4) (belt+braces) re-verify the original zip itself is intact
shasum -a 256 "$ORIG/original.zip"   # expect a9f7ed004dd053e02f6206d8ed6886174f9b5aaecf55244447a2c5fb9d6fa7fb
```

Then run the acceptance session (frozen PRX efbfc656 per FROZEN-KNOWN-GOOD
§6(a), original mod already live on console) and only then write the verdict.
If any check fails: STOP, archive the fetched bytes + shas, report — never
"fix" the console mod to match, never re-upload a mod file as part of the
acceptance proof.

## 5. Notes for the 12-mod generality ladder (context only)

The multimod staging area is `outputs/2026-10-08-l25-prep/multimod-prep/`
(CANDIDATE-INVENTORY.md, CANDIDATE-SHEETS.md, FETCH-PLAN.md, TEST-PLAN.md,
ZZDIAG-RETIREMENT.md — agent EF). Its source trees:
`.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/`
(TREE-A, CRLF: stock UE4SS set, TownfallMiniMap v3.1, TownfallModMenu v0.66)
and `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/`
(TREE-B, LF — preferred for console staging; pin exact uploaded byte counts
in loadlist rows, MFA size-exact law). TownfallItemSpawner itself is the
proving-ladder baseline, not a ladder candidate. Start the ladder only after
the acceptance proof above and the ItemId reverse-read fix.

— agent FN (ZCode), 2026-10-10.
