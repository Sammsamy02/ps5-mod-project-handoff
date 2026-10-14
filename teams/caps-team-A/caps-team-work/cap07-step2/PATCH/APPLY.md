# APPLY — cap #7 step 2: no-file-change runtime pak+IoStore mount (data-only)

Staged by caps-team-A, Mac-only, 2026-10-12. Nothing here has run on console.
Rollback for the launch that would carry it: **R11 bfd95db6** (last clean build).

## What this stages

A generic, data-only runtime unit that mounts a PC pak mod (pak + utoc + ucas
trio) **without rewriting any game file and without any detour**, by invoking
the engine's own already-bound `FCoreDelegates::MountPak` delegate with an
`FString` path to a mod container under our mod root. In UE 5.6.1 that single
call does the classic-pak mount **and** the IoStore container mount **and** the
package-store merge (verified: Epic `IPlatformFilePak.cpp:5928..6055` + the
decrypted executable's `FPakPlatformFile::Mount` tail at `0x176f188+`; see
`evidence/pins.json` pin P8). This corrects the earlier GL/HJ note that A1
"covers classic paks only, IoStore is a separate GAP".

Files:
- `RuntimePakMount.hpp` / `.cpp` — the unit. No game/mod names, no container-name
  decision constants, no file or code-page writes, no detour. Offsets are not
  hand-written here; they arrive in `EnginePins`.
- `pak_mount_pins.inc` — GENERATED (`tools/gen_pins_inc.py`) from
  `evidence/pins.json`: the per-version (UE 5.6.1) pin table, each field
  two-source-verified. Regenerate, never hand-edit.

## Integration steps

1. Drop `RuntimePakMount.{hpp,cpp}` into the runtime candidate tree as a new unit
   (suggested: `src/ModBridgePakMount.cpp` + `include/ModBridgePakMount.hpp`).
   It has no dependency on the rest of the runtime beyond `<cstdint>`/`<string>`.
2. Include `pak_mount_pins.inc` where the per-version table is assembled (it is
   the same per-version-table discipline the layout generator already uses). The
   image sha in it gates admission — a different image refuses.
3. Implement `IEngineBridge` over the live process, reusing existing runtime
   infrastructure:
   - `module_view_base()` → the runtime's `module_view_base()` (UnrealInitializer).
   - `platform_chain_head()` → resolve via pins P3/P4: call the pinned
     `FPlatformFileManager::Get()` (0xb8ed0, rebased) then read `[&manager + 0]`
     (TopPlatformFile). Both accessors are verified; read-only.
   - `image_sha256()` → the runtime's code-cache image sha (the `[Seeds][L18]`
     image-SHA256 already logged).
   - `read_ptr/read_u32` → bounded reads through the existing
     ImageSegments/VirtualQuery admission (never read unmapped memory).
   - `verify_code()` → the existing signature gate (`cached_code_signature` /
     `bridge_code_cache_signature`).
   - `is_game_thread()` → `Unreal::IsInGameThreadRaw()`.
   - `make_fstring/free_fstring` → the runtime's engine-allocator FString
     marshaling (the `push_fstring`/`from_fname` family, round-2/3).
   - `call_execute(exec_fn, inst, fstr, order)` → an indirect call with the
     SysV shape `IPakFile* (void* inst, const FString*, int32)`.
   - `mounted_pak_count()` → read `[pak_layer + 0x28]` (MountedPaks TArray count;
     HJ §3, image-pinned) OR the engine's `GetMountedPaks` accessor.
4. Call `mount(pins, req, bridge)` from the game thread AFTER engine init (same
   window the Lua mod loader already runs in). `req.pak_path_abs` is produced
   automatically from the game's own files + the installed mod (per-game DATA,
   no names in code); `req.order = -1` lets the engine rank it.
5. Log the `MountOutcome.verdict` + `why` with a `[PAKMOUNT]` prefix for the
   harvest.

## Gates that must pass before this ships

- `tests/run.sh` (this folder): pin verifier + no-hardcode/no-detour gate +
  host unit tests. All must be green.
- The repo's existing **`no_detour_gate.py`** and **`fs_throw_gate.py`** before
  compile (this unit adds no detour and no file write, so it passes; the gate is
  the standing proof).
- `verify_build.sh --manifest` for the exact PRX after packaging (standing rule).

## Data-only / law compliance

- No game file is written (the mod trio already lives, unchanged, under the mod
  root; nothing is staged into the game's Paks dir).
- No engine code page is written; no detour is installed. The only engine
  interaction is invoking an already-bound engine delegate — the same call the
  engine makes itself for chunk-install/streaming mounts.
- Shipped values have two independent sources (Epic source + decrypted
  executable); missing/mismatched values fail closed (game runs vanilla).
- Generic: zero game/mod names or container-name constants in logic (gate-proven).
