# NOTES — cap #7 step 2 (no-file-change runtime mount). caps-team-A, 2026-10-12

Mac-only. No console contact. Every "console-proven" item cites EXISTING evidence
(not a launch this session). Everything else is Mac-only / file-level.

## 1. Headline result

The data-only runtime mount is **buildable now** on verified pins. In UE 5.6.1 a
single engine call mounts the whole mod — classic pak + IoStore(utoc/ucas) +
package store — so we do not need the unproven IoStore "late-add" path HJ/GL
flagged as A2. The route is the engine's own already-bound
`FCoreDelegates::MountPak` delegate: invoke it with an `FString` to our mod-root
container, on the game thread, and the engine does the rest. No game file is
written, no code page is written, no detour is installed.

All 8 pins the mount needs are verified from **two independent sources**
(Epic UE 5.6.1 source + the decrypted executable sha `3b6cfad4…`) and
re-derived by `tests/verify_pins.py` straight from the local eboot:

| Pin | What | vaddr | 2nd source |
|---|---|---|---|
| P1 | `FCoreDelegates::MountPak` delegate object | 0x934faa8 | bind site in `Initialize` @0x176a9f5 → functor P7 |
| P2 | `TDelegateInstance::Execute` vtable slot | +0x60 | 3 engine invoke sites `call [vt+0x60]` |
| P3 | `FPlatformFileManager::Get()` | 0xb8ed0 | reused by the Mount tail @0x176f2bc |
| P4 | `GetPlatformFile()` | 0xb8a70 | @0x176f2c4 |
| P5 | `FPakPlatformFile` vtable identity | 0x7fee8c0 | ctor LEA @0x178aa1c (unique anchor) |
| P6 | `GetLowerLevel` slot (chain walk) | +0x68 | impl `mov rax,[rdi+8]` @0xb3500 |
| P7 | `HandleMountPakDelegate` (fallback entry) | 0x176b900 | "Mounting pak file" string xref |
| P8 | `Mount` does pak+IoStore+store in one call | 0x176ec30 | Epic `IPlatformFilePak.cpp:6047/6055` + utoc tail @0x176f188 |

Re-derivation tool is independent of the earlier `phasea-tools/pak_vtable_pins.py`
(fresh `tools/elfimg.py`), so the two derivations corroborate rather than copy.
HJ's `pak-pin-pass-pins.json` values all reproduced byte-identically; HJ's
"RDX = OutError not priority" ABI correction and the `[pak+0x328]` container /
`[this+0xd0]` registry tail are all confirmed here.

## 2. What is proven vs still open

- **Mac-verified (this session):** every pin above; the mount logic
  (census, fail-closed gates, delegate resolve + invoke, before/after count
  verify) via 23 host assertions against a mock engine; the generic-runtime law
  (no names, no container constants, no detour) via the gate.
- **console-proven (EXISTING evidence, cited):** the Leon derived-index overlay
  shape loads and the game runs (`leon-index-overlay-trial-2026-10-01/result.json`
  — but that trial REWROTE the on-disc index, so it proves CONTENT acceptance,
  not the no-file-change seam). A controller-converted IoStore mod mounted the
  same overlay way and the user saw the effect
  (`third-person-index-overlay-trial-2026-10-01/result.json`: "IT WORKS").
- **OPEN (needs one reviewed observe launch, pins.json R1/R2):** that the
  engine's `FileExists`/`OpenRead` for our mod-root pak + its sibling `.utoc/.ucas`
  actually reach our mod-root files (in-process reads from that root are
  PORT-proven; the engine using the same syscall surface for these opens is
  INFERRED). This is the ONE runtime unknown; it does not block step 1 and the
  mount fails closed (game vanilla) if the path doesn't resolve.

## 3. DIRECTLY ANSWERS THE USER'S OBSERVATION (checkerboard textures + "bright as day")

The user reports, on their recent launch: Leon's textures are "checkered gray
and white", and outside "it was just bright as day… not scary darkness… you can
see the monster very clearly." Two separate causes, both from trial overlays
that were **staged on the console in Oct 1–2 and (per the Oct 8 storage scan)
never rolled back**:

**(a) Checkerboard textures on Leon — the shader-platform mismatch (cap #8).**
File-level STRONGEST-HYPOTHESIS (`shader-fallback/fallback.md`, byte-proven):
the Leon mod ships **zero PS5-consumable shader data** — its materials
(`M_Invisible`, `M_Leon_*`) carry only PC D3D shader containers (DXBC/DXIL,
`PCD3D_SM5/6`), and the merged store dropped the base PS5 shader-map hashes for
the replaced packages. A PS5 runtime with no shader map for a material falls
back to a default/`WorldGridMaterial`-style surface → a pale gray/white
checkerboard. This is exactly cap #8 (PC→PS5 asset recook), NOT the mount
mechanism. The mount works; the mod's bytes are PC-cooked.

**(b) "Bright as day / no scary darkness" — the config-ini mod still installed.**
The Oct 8 console scan shows `pakchunk0-ps5.pak` is the **config-modded** pak
(297,483,249 B, sha `8bcf5eb1…`; original backed up as
`…pakchunk0-ps5.pak.original-9a914e38….bak`). I diffed its `PS5Engine.ini`
against the original this session (Mac-only, via the pinned repak):

```
r.ShadowQuality            3 -> 1
r.Streaming.MaxTempMemoryAllowed  35 -> 128
+ r.Shadow.Virtual.Enable=0      + r.DistanceFieldShadowing=0
+ r.ContactShadows=0             + r.AllowPointLightCubemapShadows=0
+ r.Shadow.MaxResolution=256     + r.VolumetricFog=0
+ r.Lumen.Reflections.Allow=0    + r.EyeAdaptationQuality=0
+ r.TonemapperGamma=1.3          + r.Color.Mid=0.42   (… full list in evidence)
```

Shadows minimized/disabled, volumetric fog off, eye-adaptation off, raised
tonemapper gamma / mid — all of which flatten and brighten the image and remove
the dark/foggy mood. This is a **coherent, byte-grounded explanation** of the
"bright, no darkness, see everything" report (it is a hypothesis about the
visible result, same honesty as fallback.md; the INSTALLED-state fact is
console-proven from the Oct 8 scan `…/harvest-20261008T174239Z-25929d/storage/`).

**The fix is a console rollback** (not a code change): restore the two retained
originals and remove the Leon overlay, so the game is pristine again before the
clean cap-#7 launch. Exact paths are in `needs_from_integrator`. I am Mac-only
and cannot do it.

## 4. Why the delegate route (not the vtable-read-hook B2)

B2 (hook OpenRead/FileSize to serve a derived index from memory) still has the
unproven "does `.ucas` read flow through the IPlatformFile chain" seam question
(HJ §4). The MountPak-delegate route sidesteps it entirely: we hand the engine a
real path and it opens the files through its own normal path. It needs fewer
pins, installs no vtable swap, and is the engine's own public mount entry.

## 5. Files in this folder

- `evidence/pins.json` — the 8 pins, each with Epic cite + eboot evidence + a
  per-version re-pin anchor + verified/blocked status + the R1/R2 runtime unknowns.
- `tools/elfimg.py`, `tools/explore.py` — independent read-only ELF reader + RE
  helper used to derive/verify the pins.
- `tools/gen_pins_inc.py` — emits `PATCH/pak_mount_pins.inc` from pins.json.
- `PATCH/RuntimePakMount.{hpp,cpp}` + `pak_mount_pins.inc` — the staged unit.
- `tests/verify_pins.py` (12 checks), `tests/test_pak_mount_host.cpp` +
  `mock_engine.hpp` (23 checks), `tests/gate_no_hardcode.py`, `tests/run.sh`
  (+ saved `run-output.txt`).

## 6. Open questions / honest limits

1. R1/R2 (section 2) — the single runtime unknown; one observe-only launch
   closes it (read back MountedPaks count + a FileExists for a mod-only file).
2. Per-version re-pin on a different engine build is designed (anchors in
   pins.json) but only exercised on this one image; a second same-version UE
   title was not available locally (same caveat as HJ §7.6).
3. The mount delivers the mod bytes; it does NOT fix PC-cooked assets. Leon's
   checkerboard is cap #8, independent of this capability.
4. Mount ORDERING / multiple mods / uninstall: `req.order = -1` lets the engine
   rank; multi-mod and unmount reuse the same delegate (OnUnmountPak = P-static
   0x934fae8, functor 0x176ba30) — staged as a follow-on, not in this unit.
