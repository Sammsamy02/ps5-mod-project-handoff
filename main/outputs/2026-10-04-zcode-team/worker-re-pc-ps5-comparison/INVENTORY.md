# INVENTORY.md — STEP 0: what PC-side Townfall material exists? (worker-re-pc-ps5-comparison, 2026-10-04)

Verdict up front: **CASE B. No usable PC Townfall executable exists on this machine.**
Every search below was bounded and read-only. The only PC-side PE material this project
(or the machine) holds is the Win64 **UE4SS runtime release** for the game (proxy
`dwmapi.dll` + `ue4ss/` payload) — adapter-layer reference material, not the game binary.

---

## 1. Searches performed (commands abbreviated, all run 2026-10-04 local)

| # | location | method | result |
|---|---|---|---|
| S1 | workspace `.superpowers/native-observations/**`, `outputs/**` | `find -maxdepth 4/5 -iname "*townfall*" -o -iname "*PPSA33286*"` | ~30 hits, **all PS5-side** (eboot capture, pak/utoc/manifest captures, notes, mod intakes). Zero PC executables. |
| S2 | same dirs | `find -maxdepth 5 -name "*.exe" -size +1M` | **none** |
| S3 | `/WORKSPACE/Projects/**` | `find -maxdepth 3 -iname "*townfall*" -o -iname "*PPSA33286*"` | **none** (`/WORKSPACE/Projects/re-tools/` contains only `ghidra.zip`) |
| S4 | `/Applications/**` | `find -maxdepth 3` + `mdfind -onlyin /Applications` | **none** |
| S5 | `~/Library/Application Support/**` | `find -maxdepth 3 -iname "*townfall*" -o -iname "*PPSA33286*"` | **none** |
| S6 | Steam library | listing `~/Library/Application Support/Steam/steamapps/common` | 10 games (Balatro, CoD BO3, ClusterTruck, Dead Cells, Paint the Town Red, Slay the Spire 2, SlayTheSpire, Swords & Sandals, Binding of Isaac Rebirth, TABS). **No Townfall.** |
| S7 | Epic library | grep of `~/Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests/*.item` | **no Townfall match** |
| S8 | whole indexed drive | `mdfind -name Townfall`, `mdfind -name PPSA33286` | Only `~/Downloads` mod zips (below) and this project's own docs. **No game install.** |
| S9 | Downloads zips | `unzip -l` (contents listing only) | See §3. |

## 2. What the ledger's windows_* entries actually point to (followed to their dirs)

The `transport-experiments-progress.json` entries named in the mission reference **owned,
self-built PE runtime fixtures**, never a PC build of the game:

- `windows_image_preparation` → `outputs/ps5-mod-bridge/windows-image/` (`pe_image.c/.h`
  PE loader, tests) + `.superpowers/native-dependency-spike/windows-image-original-inputs-1/`.
  Its "original_dlls: 2" are **`dwmapi.dll` (279,552 B, sha256 `4e3e5324…`) and the
  UE4SS payload DLL** taken from the Win64 UE4SS release zip — used as PE-parser
  test inputs, compared via `llvm-readobj`. Plus a compiler-built PE fixture.
- `callable_pe_runtime` → `outputs/2026-10-04-callable-pe-runtime/`: owned PE host
  execution (6 imports called, unload clean). All fixture DLLs are project-owned.
- `windows_entry_runtime` → `outputs/2026-10-04-windows-entry-runtime/`: owned DllMain
  entry lifecycle fixtures (4 cases). No game material.
- `windows_worker_runtime` → `outputs/2026-10-04-dll-worker-runtime/` (+ `SOURCE-MANIFEST.json`):
  real host PE workers + native SDK composition. No game material.
- `native_dll_candidate_and_tick_fix`, `native_dll_entry_clock_notify`,
  `native_dll_clock_composition` → owned-dll-sdk-candidate ELF objects (PS5-side, SDK
  crt1 startup), clock provider/bridge objects. No PC game material.
- `native_tick_file_witness`, `native_tick_mapped_connection` → UE4SS-composition
  runtime objects (PS5-side). No PC game material.

**Conclusion: the project has never held a PC Townfall executable.** The PC-side
inventory is exactly: the two UE4SS-release PE images (§3) and the Win64 UE4SS
reference behavior they imply.

## 3. The complete PC-side material list (what exists)

| artifact | path | size | sha256 | role |
|---|---|---|---|---|
| `dwmapi.dll` (UE4SS proxy, Win64) | `~/Downloads/UE4SS for SILENT HILL Townfall 4 1 2026-09-22T04-56Z JNCU6WyIQ.zip` → also retained at `.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/dwmapi.dll` | 279,552 B | `4e3e5324e68e033e372193d2420367614eb13016cb98b22b8e85e9844f5c0c9a` | UE4SS Win64 load proxy; PE-parser fixture input; adapter-layer reference |
| UE4SS Win64 release payload | same zip: `ue4ss/` (Mods/, settings, main UE4SS DLL) | (zip) | not hashed here | the upstream Win64 UE4SS this project ports from |
| `TownfallItemSpawner V1.0 …zip` | `~/Downloads/` → `TownfallItemSpawner/Scripts/main.lua` (21,415 B, 746 lines) + `enabled.txt` | 21,415 B | not hashed here | the Lua mod whose required engine operations define the comparison list |
| Leon Townfall 2K/4K, MiniMap, ModMenu zips | `~/Downloads/` | — | — | other PS5-side Lua/pak mods; not PC game material |

PS5-side (for contrast, all retained and sha-pinned): the decrypted eboot capture
(`.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
194,241,424 B, sha256 `3b6cfad4…3622fc`, + 64 KB prefix, rela index, region names),
pak/utoc/manifest captures, and the full derivation corpus in
`outputs/2026-10-04-zcode-team/`.

## 4. Decision

Mission rule: "CASE B — no PC executable available (likely): do NOT fabricate."
Delivered instead: `ADAPTER-FRAMEWORK.md` (existing artifacts → three layers, reuse
notes) and `COMPARISON-PROTOCOL.md` (acquisition protocol + free-tooling comparison
method + the prioritized comparison list with the PS5 half already filled from pinned
derivations).
