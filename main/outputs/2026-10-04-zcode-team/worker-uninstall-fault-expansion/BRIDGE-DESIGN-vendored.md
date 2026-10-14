# BRIDGE-DESIGN.md — worker-zip-to-mfb: quarantine -> modforge bundle (F-R1)

Origin: worker-zip-to-mfb, 2026-10-04 (ZCode team, PS5 mod-manager).
Closes the F-R1 gap found by worker-e2e-rehearsal REPORT.md §3: a downloaded
real archive downloads and quarantines fine, but `install --mods <archive-mod>`
refuses (exit 2, "not a modforge bundle") because nothing composes a bundle
from the quarantine. **mfb stays the single bundle format** — the bridge is an
ADAPTER onto the existing composer inputs, not a new format.

## 1. Where the bridge sits (all existing components reused)

```
download --job            (worker-e2e-final, REAL sink + records; proven)
   -> record: state=downloaded, bundle=<intake>/<id>.zip     [NOT installable; the F-R1 refusal at this point is CORRECT]
prepare-archive --mod ID  (worker-installer-composition REAL libarchive intake; proven)
   -> quarantine: member-%08u.bin + manifest-v1.bin (the intake's OWN durable map:
      row_index, size, member sha256, member name, original name, SAFE path)
   -> *** NEW: auto-bundle step (this worker) ***             [default ON exactly when --mod given]
        reads manifest-v1.bin -> classifies members -> synthesizes modinfo.txt
        -> writes <state>/bundles/<id>.mfb (kind 1, mfb_write format identical
        to worker-7 make-bundle / modforge bundle builder)
   -> record: state=prepared, bundle=<state>/bundles/<id>.mfb  [INSTALLABLE]
install --title --mods ID  (worker-7 transactional; proven)   [mfx_compose validates the bridged
                                                               bundle exactly like a supplier bundle]
simulate-runtime / uninstall: unchanged.
```

The bridge reuses, unmodified: the intake (quarantine + manifest), the mfb
index/header format (byte-identical layout to `cmd_make_bundle`), the vendored
`mfb_sha256`/`mfb_path_ok` validators, and the whole install/compose path. The
only new code is the manifest reader + member classifier + bundle writer
(`owned/modinst_zip_bridge.c`), linked into the composed binary through a
recorded patch chain (§5).

## 2. Mod identity (archive-sourced mods)

| modinfo record | Bridge value | Why |
|---|---|---|
| `mod` id | the `--mod` ID (the download job's id) | the record already exists from `download`; the bridge must not invent a second identity |
| `mod` version | `1.0.0` (fixed default) | archives carry no machine-readable version; documented default, deterministic rebuilds |
| `mod` priority | `100` (fixed default) | same default as `make-bundle`; archive mods order among themselves by id after priority |
| `title` | `--title` (PPSA33286 in tests) | must match the game; composer enforces |
| `engine` | `unreal` (fixed default) | corpus mods are UE (UE4SS Lua / Content/Paks); honest default, documented |
| `sdk` | omitted | no evidence in archives |
| `load_order` | `manifest_priority` | non-Bethesda default; no plugin records are synthesized |
| `framework`/`requires`/`base`/`plugin`/`master` | none | archives carry no dependency metadata; inventing requirements would fake knowledge |

## 3. Members mapping (classification; deterministic, on the intake's SAFE path)

Input: each manifest row's SAFE path (the intake already rejected `..`, `:`,
absolute paths, control chars; backslashes normalized to `/` — intake.c
`name_row`). First matching rule wins:

| Rule | Matches (ci = case-insensitive) | Role | Final path in bundle |
|---|---|---|---|
| L | `ue4ss/Mods/...` prefix | `lua_mod` (`.lua`), `config` (`enabled.txt`, `load_order.txt`) | unchanged |
| M | `Mods/<X>/...` (no ue4ss prefix) | same as L | `ue4ss/Mods/<X>/...` |
| W | `<X>/Scripts/**` or `<X>/enabled.txt` (single wrapper dir — the corpus UE4SS package shape) | same as L | `ue4ss/Mods/<X>/...` |
| P | any `.../Content/Paks/<tail>` | `pak` (`.pak`), `asset_override` (`.ucas`/`.utoc`/`.sig`) | `mods/<tail>` — matches the proven supplier-bundle pak layout `mods/~mods/*_P.pak` |
| I | `.ini` member | `config` | `config/<tail after Saved/Config/>` if a `Saved/Config` segment pair exists, else `config/<basename>` |
| — | anything else (ReadMe, dlls, screenshots) | skipped as non-installable metadata | — |

Rules L/M/W mirror how the proven 2b supplier bundles lay Lua mods out
(`ue4ss/Mods/<Name>/scripts/main.lua`); rule P mirrors the proven pak layout.
Refusals (specific reasons, quarantine retained, record stays `downloaded`):

- **no recognizable mod content**: zero members classified (only metadata)
  — `error bad_request auto-bundle refused: no mod content recognized ...`
- **duplicate final path**: two members map to one final path
- **unsafe/oversized mapping**: final path fails `mfb_path_ok` or exceeds
  `MFB_MAX_PATH`; id/title containing tabs/newlines; >256 members; >64 MiB
  total member bytes (v1 bridge budget; larger paks refused honestly, not
  truncated); member size/hash mismatch vs the manifest at copy time
  (quarantine changed after prepare).

## 4. Conflict / order handling (defaults, by reuse not invention)

- **Inside one archive mod**: duplicates are REFUSED (§3) — a silent "later
  member wins" inside one mod would hide authoring errors.
- **Across mods**: unchanged composer semantics — `mfx_compose` orders by
  priority then id, a later mod wins a shared path, and writes loadlist.txt
  last (modforge_compose.h). Archive mods participate with their fixed
  defaults (§2), i.e. they lose/gain paths purely by id order against
  supplier mods of equal priority.
- **Record states for archive-sourced mods** (documented meaning):
  - `downloaded` — bundle field is the intake zip; NOT installable (the
    historical F-R1 refusal is the correct behavior at this state).
  - `prepared` — bundle field is the composed `<state>/bundles/<id>.mfb`
    (auto-bridge output); installable. Quarantine is retained (intake rule)
    and the bundle is deterministic in its inputs (same archive bytes +
    mapping rules => same mfb).
  - `installed` / back to `prepared` on remove: unchanged worker-7 semantics.
- **Idempotency**: a second `prepare-archive --mod` re-runs the bridge
  (deterministic rewrite of the same bundle path), then the existing
  `records unchanged ... already prepared` path applies. Refusals never
  mutate the record (the bridge runs BEFORE the records transition and a
  non-zero bridge return skips it).

## 5. Composition (vendored extension + recorded patch chain)

`compose_bridge.py` (this dir) vendors `worker-e2e-final/compose_e2e.py`
(which itself vendors `worker-installer-composition/compose_installer.py`),
applies recorded text splices (each anchored, sha-logged into
`e2e-receipts/`), and runs the adapted inner script for this directory:

- vendors worker-e2e-final's owned download/stub adapters into this `owned/`
  (sha-verified) and adds this worker's `modinst_zip_bridge.c/.h`;
- adds the bridge TU to the host target list (`core_flags`, same include set
  as the other owned TUs);
- appends 4 modinst.c patch tuples AFTER the e2e chain (order preserved):
  `zb-inc-bridge` (include), `zb-flag-no-auto-bundle` (main flag-loop skip
  for the value-less opt-out), `zb-dispatch-bridge` (the auto-bundle step in
  the prepare-archive dispatch, before `records_mark_archive_prepared`;
  on success the record's bundle is repointed to the mfb),
  `zb-usage` (usage text).

Flag control: **default ON exactly when `--mod` is given** (the
archive-sourced-mod flow); `--no-auto-bundle` reverts to the previous
reporting-only behavior; without `--mod` the bridge never runs.

## 6. Honest limits

- Host-tested only (no console run here; the TU compiles in both flavors —
  console link is a remaining controller step, same as the e2e chain's).
- Fixed identity defaults (§2) are documented conventions for the corpus
  class, not metadata extraction; a supplier-provided `.mfb` (2b path)
  remains the higher-fidelity route when available.
- v1 budget: <=256 members, <=64 MiB total member bytes per archive mod.
- Windows-only payloads (dwmapi.dll etc.) are skipped, not installed; a
  PS5-native payload bridge is out of scope.
