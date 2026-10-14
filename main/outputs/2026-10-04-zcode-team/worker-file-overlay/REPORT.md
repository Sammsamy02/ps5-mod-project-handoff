# REPORT.md — worker-file-overlay (2026-10-06)

Scope honored: research + design only; no console contact; no game-state,
mount, or acceptance claims; writes confined to this worker directory.
Design: `OVERLAY-DESIGN.md`. Source census: `EVIDENCE-NOTES.md`.

## Task answers

**1. Current file loading paths.** The game runs from the decrypted dump at
`/data/homebrew/Silent Hill Townfall PPSA33286/`, which shadowmount (in pldmgr
autoload) presents as `app0` inside the game sandbox
(`/mnt/sandbox/PPSA33286_000/`). UE 5.6 reads its content from
`/app0/townfall/content/paks/` (IoStore `.utoc/.ucas` + legacy PAK v11, both
unencrypted on this title) through libSceFios2. Decisive constraint (measured
contract, `modforge_activation.h:6-8`, PATH_TEST-corroborated): `/data` is
openable from the game process **only during the injected module_start
window**; fresh opens afterwards fail ENOENT. Today's loadlist/modbridge root
preloads only small roles (lua_mod/config/script; 8/64 MiB caps); `pak` roles
are listed but never delivered to the engine — that gap is the overlay's job.

**2. Ranked approaches** (full analysis OVERLAY-DESIGN §2):

1. **Container projection** — installer writes owned, add-only, correctly
   named containers (`pakchunk9zz-ModBridge_<uuid>_P.*`) into the dump's
   `townfall/content/paks/`; the engine's own startup pak/IoStore discovery
   mounts them. No runtime code, no kernel work, decoupled from the current
   module-loading regression, rides a channel already exercised once
   (`pakchunk9999-ModBridge_..._P` trio; `engine_mount_or_asset_origin_proved:
   false` remains the open verdict). **RECOMMENDED, build now.**
2. **Engine platform-file wrapper + late mount** via the UE4SS port
   (`FileOverrideTable` + `pread`-backed fds kept from module_start; the same
   wrapper slot `-fileopenlog` uses). Full in-process loose-file overlay;
   depends on the engine-binding critical path (current B1–B3 ladder) → phase 2.
3. **Fios overlay API** (`sceFiosOverlayAdd` family) called pre-main —
   transparent directory overlay without touching the dump; PS5 availability
   in the game's libSceFios2 is UNMEASURED → bounded research track (W3), with
   W2 (post-closure path census) deciding the source-dir question.
4. **libc open/read hook** — misses fios-routed engine IO; C1 still binds the
   redirect source. Fallback only.
5. **Kernel VFS hook** — panic-class risk, per-boot-fragile kernel layout,
   outside the project's kernel-code boundary. Rejected, documented non-goal.

**3. UE5 specifics.** Engine enumerates the project `Content/Paks` folder at
startup and mounts PAK/IoStore TOCs by pak-priority order (`_P` patch suffix
raises priority; higher priority wins package/file lookups — the documented
patch/DLC mechanism and the basis of the PC `zz_MyMod_P.pak` convention).
Asset overrides must be IoStore (same package IDs, Leon-bundle pattern);
loose resources ride PAK v11; console shipping builds read config only from
containers (so `configs/` gets repacked, not served loose). `-fileopenlog`
itself is a diagnostic platform-file wrapper — the insertion point it proves
is what phase 2 reuses. Upstream UE4SS has **no** file-override feature to
port (settings verified); this overlay is this project's own extension.

**4/5. Directory structure + plan.** Drop zone
`/data/modbridge/overlay/PPSA33286/{overlay.txt,paks/,~mods/,configs/,textures/}`
with an installer-owned manifest and a projection map into the dump's paks
dir (names/priorities calibrated from the title's own `_P` vocabulary).
Phases: P0 this design; **P1 = W1 one-launch engine-mount verdict**
(re-stage one original-only trio; no composition module required);
P2 productize the `project-overlay` installer verb (journal, readback,
rollback, audit, uninstall = delete owned names); P3 phase-2 in-process
wrapper/late-mount; P4 Fios-overlay research with kill-fast probes. Honest
limits: everything console-side is cited from retained probes; the
engine-mount verdict, PS5 fios overlay availability, and post-closure sandbox
readability are explicitly unproved and owned by W1–W3.

## Sources (external, fetched 2026-10-06)

- [Epic — How to Create a Patch (pak `_P` priority)](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-patch-in-unreal-engine)
- [Pak Patching — UE modding guide (pak priority mechanics)](https://buckminsterfullerene02.github.io/dev-guide/Basis/PakPatching.html)
- [UE4SS upstream settings (no file-override feature)](https://github.com/UE4SS-RE/RE-UE4SS/blob/main/assets/UE4SS-settings.ini)
- [RE-UE4SS repository](https://github.com/UE4SS-RE/RE-UE4SS)
- [ConsoleMods — PS4 ripping/backups (decrypted dumps)](https://consolemods.org/wiki/PS4:Ripping_PS4_Game_Backups)
- [PSXHAX — UE game modding via extracted paks](https://www.psxhax.com)
- [PSDevWiki PS5 Libraries](https://www.psdevwiki.com/ps5/Libraries)
