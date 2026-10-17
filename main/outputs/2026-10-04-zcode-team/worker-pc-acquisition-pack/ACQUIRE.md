# ACQUIRE.md — PC Townfall acquisition checklist (one page, for the user)

You offered to provide PC-side material. This is exactly what to grab, where it lives,
and where to drop it. The moment files land here, the PC-vs-PS5 comparison starts with
zero further setup — the intake gate (`verify_intake.py`) is already written and tested.

The law this follows (COMPARISON-PROTOCOL.md §2): prefer a PC build of the **SAME title
update** as the PS5 pinned image (sha `3b6cfad4…3622fc`, UE **5.6**). If you can only get
a different update, provide it anyway — the version delta becomes first-class output, not
a blocker.

## 1. What to copy (in priority order)

| # | item | required? | what it is |
|---|------|-----------|------------|
| 1 | **Main Win64 executable** | REQUIRED | `Townfall.exe` or `<Game>-Win64-Shipping.exe`, typically under `<install>/Townfall/Binaries/Win64/`. Copy it byte-exact (do not zip-convert, do not "clean" it). |
| 2 | **Store build manifest for the same update** | strongly preferred | Steam: `appmanifest_<AppID>.acf` from `steamapps/`. Epic: the game's `*.item` manifest. This is what pins the title update. |
| 3 | Main content set: `.pak` / `.utoc` / `.ucas` | optional | Only from the SAME update as the executable. The executable alone is acceptable. |

## 2. Where to find the installed version

**Steam**
- Files: `steamapps/common/<Townfall install folder>/` (macOS: `~/Library/Application Support/Steam/steamapps/common/`; Windows: `C:\Program Files (x86)\Steam\steamapps\common\`).
- Version: open `steamapps/appmanifest_<AppID>.acf` and read `buildid` (and `name` to confirm the title).

**Epic**
- Files: the game's `InstallLocation` from its manifest.
- Version: `~/Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests/*.item` (macOS) or `C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests\*.item` (Windows) — read `BuildVersion`, `AppName`, `InstallLocation` from the JSON entry whose `DisplayName` matches the game.

**Other stores**: send the executable plus whatever version string the launcher shows; note the store name in capture.json.

## 3. Where to put the files

```
inputs/pc-townfall/          <- workspace root; this dir already exists (placeholder README inside)
├── <the executable>         <- copy byte-exact, keep original filename
├── <appmanifest_*.acf or *.item>   (if you have it)
└── capture.json             <- fill the template below
```

## 4. capture.json — fill every field YOU can (create the file in `inputs/pc-townfall/`)

```json
{
  "utc": "2026-10-__T__:__:__Z",
  "scope": "read-only PC-side executable intake for PC-vs-PS5 comparison",
  "store": "steam | epic | other",
  "store_version": "e.g. Steam BuildID 12345678, or Epic BuildVersion +1.2.3, or UNKNOWN",
  "store_version_source": "exact path of the .acf/.item you read it from, or UNKNOWN",
  "remote_path": "absolute path of the executable on the machine you copied it from",
  "expected_bytes": 0,
  "complete": false
}
```

- `store`, `remote_path` are **required** (provenance law: recorded, never assumed).
- `expected_bytes`: the file size in bytes as shown on the source machine (Finder/Explorer
  "size", not "size on disk"). Put `0` only if truly unknown. If it mismatches the copy,
  the verifier flags the copy as incomplete.
- You do NOT need to compute sha256 — the verifier computes and prints it (and it must
  come from the actual bytes, not from any store page).
- Set `"complete": true` only after the verifier reports the copy intact.

## 5. Run the intake gate (read-only; it never modifies your files)

```bash
python3 outputs/2026-10-04-zcode-team/worker-pc-acquisition-pack/verify_intake.py inputs/pc-townfall
```

It checks presence, computes sizes/sha256, confirms the PE/ELF class via lief, prints the
version-divergent vs platform-divergent classification (protocol §2.2 — the classes are
never merged), and prints the ready-to-run 5-op comparison list with the PS5 anchors
already filled. Exit codes: 0 READY, 1 NOT-READY (with the exact blocker named), 2 staging
dir missing.

## Honesty notes

- No PC Townfall file exists locally today (INVENTORY.md, 2026-10-04: all stores searched).
- The UE4SS `dwmapi.dll` / payload already on this machine is the mod *runtime*, not game
  material — do not stage it as the game.
- Until the title update is pinned, every future anchor mismatch is reported
  **version-divergent**, never platform-divergent.
