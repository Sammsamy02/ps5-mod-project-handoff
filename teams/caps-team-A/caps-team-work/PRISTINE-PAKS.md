# PRISTINE PAKS: put Townfall's paks folder back to the original game

One shared runbook for every Team A pak launch. Run it **before any pak launch**. The
step ends when the last check prints `"verdict": "PRISTINE"`.

**Why:** old experiments are still on the console (2026-10-08 scan and the 2026-10-13
LIST in `CONSOLE-FACTS.md`):
- the raw PC Leon files `zzz_Leon_Townfall_2K_P.{utoc,ucas,pak}`,
- an old Leon index `pakchunk0-ps5.utoc` (29,870,724 B) with its sidecar `pakchunk0-ps5_s1.ucas`,
- a changed `pakchunk0-ps5.pak` (297,483,249 B, the PC Engine.ini that made the outdoors bright).

The game's own originals are still there as `.original-*.bak` copies.

**Rules for the operator (the integrator):**
- Only the integrator touches the console, one agent at a time.
- Every numbered console step below is a separate action. Ask the user for an OK for
  that step, show them what the tool printed, and wait. One OK never covers the next step.
- Ask "Is the game closed?" before step 1, and again before every step if the game could
  have been started in between.
- Keep FTP decrypt OFF (`SITE SELF` off). No executable is touched here.
- Never write the console's IP into a report. `FTP=<console LAN address>:2121`.
- Save every printed output to the evidence tree
  (`outputs/2026-10-08-l25-prep/evidence/pristine-paks-<UTC>/`), never `/tmp`.

Paths: `T=outputs/2026-10-08-l25-prep/pak-overlay/paktx.py` (copied from
`caps-team-A/caps-team-work/cap07-step1/PATCH/tools/paktx.py`).
`R=records/townfall.record.json` is the complete game record (APPLY steps 2-3 of cap07-step1:
17 files, 6 containers). `ROOT=/data/homebrew/Silent Hill Townfall PPSA33286/townfall/content/paks`.

**Important: "out" means out of `content/paks` entirely.** The engine also loads pak
files from subfolders of `paks/` (for example `~mods/` or `~mods-off/`). A subfolder of
`paks/` is NOT out. paktx moves files to `content/paktx-quarantine-<id>/`, which is next
to `paks/`, not inside it. Nothing is deleted.

## What the tool's answers mean

- **exit 0:** done.
- **exit 2 `refused`, `"console_changed": false`:** nothing on the console changed. Stop
  and tell the user the `code`.
- **exit 3 `stopped`, `"console_changed": "maybe"`:** a change had started. Do not guess.
  Run `repair --dry-run` again: it re-reads the live files and plans from what is really
  there. Show it to the user, and continue only with their OK.

## Steps

**0. Mac only, no console:** the complete record `R` exists and loads. If it does not, do
APPLY steps 1-3 of cap07-step1 first: a read-only LIST, read-only fetches of 6 files, then
`record`. `repair` and `check` refuse a partial record.

**1. Console READ (user OK #1): look first, change nothing.**
```
python3 $T repair --record $R --work W/pristine/dry --ftp $FTP --dry-run
```
This makes a recursive LIST of `paks/` (every subfolder too). It reads the small files and
the `.bak` copies with sha256. Then it prints the full plan. Expected plan, and nothing else:
- `REPAIR pakchunk0-ps5.utoc from pakchunk0-ps5.utoc.original-603c4879-….bak`
- `REPAIR pakchunk0-ps5.pak from pakchunk0-ps5.pak.original-9a914e38-….bak`
- `QUARANTINE zzz_Leon_Townfall_2K_P.pak / .ucas / .utoc` (the path may start with a
  subfolder such as `~mods/`)
- `QUARANTINE pakchunk0-ps5_s1.ucas`

**STOP and tell the user** if the plan:
- shows anything else (another mod, another game file),
- or refuses.

Note the exact names it prints. Steps 2-4 use them.

**2. Console CHANGE (user OK #2): move the raw Leon trio out of `paks/`.**
First the dry run for this step only, shown to the user:
```
python3 $T repair --record $R --work W/pristine/s2 --ftp $FTP --dry-run \
  --only "<the 3 zzz_Leon paths exactly as step 1 printed them, comma-separated>"
```
After the OK, run the same command with `--confirm <first 12 of its plan sha> --game-closed`
in place of `--dry-run`. It does three things:
- creates `content/paktx-quarantine-<id>/`,
- renames the 3 files into it,
- checks that each one is there with the same size and gone from `paks/`.

Expected: `"result": "repair-step-done"`.

**3. Console CHANGE (user OK #3): put the original index and the original pak back.**
This also brings the darkness back.
```
python3 $T repair --record $R --work W/pristine/s3 --ftp $FTP --dry-run \
  --only pakchunk0-ps5.utoc,pakchunk0-ps5.pak
```
After the OK, run it with `--confirm <sha12> --game-closed`. For each of the two files:
1. The changed live file is renamed aside to `<name>.foreign-<id>.retired`. It is kept, not
   deleted, and the engine ignores it.
2. The sha-verified `.original-*.bak` is renamed back to the real name.
3. The tool re-reads it in full: it must match the recorded sha256 (table below).

**4. Console CHANGE (user OK #4): move the old sidecar out of `paks/`.**
```
python3 $T repair --record $R --work W/pristine/s4 --ftp $FTP --dry-run --only pakchunk0-ps5_s1.ucas
```
After the OK, run it with `--confirm <sha12> --game-closed`. This moves the file to a second
`content/paktx-quarantine-<id>/` folder. That takes it out of `paks/`. Deleting it for good
is not needed, and would be its own action with its own OK.

**5. Console READ (user OK #5): recursive LIST + sha256 readback.**
```
python3 $T check --record $R --ftp $FTP
```
It prints every file in `paks/` and in every subfolder. It re-reads every recorded small
file with sha256 and compares it with the record. The big `.ucas` files are checked by size
only, and are never read. PASS:
- `"verdict": "PRISTINE"`,
- `recorded_files_checked: 17`,
- no `.utoc/.ucas/.pak/.sig` file anywhere that is not a game file.

Anything else is a FAIL: stop and tell the user.

Recorded originals that step 5 must find (sha256 from the 2026-10-01 captures, taken
before any experiment; the `.pak` one also matches the `.bak` pulled on 2026-10-13):

| File | Bytes | sha256 |
|---|---|---|
| `pakchunk0-ps5.utoc` | 29,707,562 | `35de27526c2ed91a7d21ab027b4e2ad6dc58cca137ab92c31e1f60c9e0bb75b5` |
| `pakchunk0-ps5.pak` | 297,572,462 | `ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867` |
| `pakchunk1-ps5.utoc` | 32,829 | `b550c82763f3fad876b9e6ee5638ba9b7de2002e72e075f4cc63258eb29335ee` |
| `pakchunk10-ps5.utoc` | 22,838 | `892a6327cb3d2264855ec678a9aec33ea0aca16f6485a0d356b4dc4329f409c0` |
| `pakchunk11-ps5.utoc` | 11,187 | `7da3416eb01ecc237d0c8031a63c0d3671dd8e8c15ab3a369d98704fa4c18078` |
| `pakchunk12-ps5.utoc` | 36,781 | `0525dc9330110a3c0e52cc2789655c5ac55790e5d6cba1ea29616e655e4ac55f` |
| `pakchunk1/10/11/12-ps5.pak`, `global.utoc` | see record | single source (today's file, size + date) |
| `.ucas` files | size only | never read |

Allowed leftovers inside `paks/` (the engine does not load these names):
- `pakchunk0-ps5.utoc.original-8988cacd-….bak` (a second identical copy of the original),
- the two `*.foreign-<id>.retired` files from step 3,
- an empty `~mods/` folder, if there was one.

## If something goes wrong

- **Step 2 or 4 stops (exit 3):** the console's FTP server has never been tested with
  MKD or with a rename into another folder. Run the step-1 dry run again, then show it to
  the user. If moving out does not work on this server, the fallback is two actions, each
  with its own OK:
  1. read-only RETR of those files into the evidence tree, then check their sha256 against
     the copies already on the Mac (Leon intake / `console-pulls-20261013-paks/SHA256SUMS`);
  2. a `DELE` of exactly those files.
- **Step 3 stops:** run the step-1 dry run again. It sees which file is where and plans
  only what is still missing. The original bytes are only ever renamed, never rewritten.
- **The last change was a paktx install** (not the old experiments): use that install's
  own `paktx restore … --plan <its plan.json>` first (cap07-step1 APPLY step 8). Then do
  step 4 for any `_s1.ucas` left behind, and step 5.

All five steps together are one "pristine" pass. They make no runtime change and need no
build. The rollback build stays R11 `bfd95db6`.
