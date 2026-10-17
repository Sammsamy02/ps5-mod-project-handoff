# Console facts for Team A (integrator, read-only FTP LIST, 2026-10-13 ~02:25 UTC)

The game install is at `/data/homebrew/Silent Hill Townfall PPSA33286/`. The path comes from
`/user/app/PPSA33286/mount.lnk`.

Paks folder: `townfall/content/paks/` (lowercase).

| File | Bytes | Date |
|---|---|---|
| global.ucas | 3,412,992 | 10-01 |
| global.utoc | 830 | 10-01 |
| pakchunk0-ps5.ucas | 128,240,377,200 | 10-01 (base, untouched) |
| pakchunk0-ps5.utoc | 29,870,724 | 10-02 00:11 (REWRITTEN by the Leon method) |
| pakchunk0-ps5.utoc.original-8988cacd-...bak | 29,707,562 | 10-01 23:52 |
| pakchunk0-ps5.utoc.original-603c4879-...bak | 29,707,562 | 10-02 00:10 |
| pakchunk0-ps5_s1.ucas | 187,713,177 | 10-02 00:10 (Leon sidecar partition) |
| pakchunk0-ps5.pak | 297,483,249 | 10-02 00:47 (MODIFIED) |
| pakchunk0-ps5.pak.original-9a914e38-...bak | 297,572,462 | 10-02 00:46 |
| pakchunk1..12-ps5 .pak/.ucas/.utoc | various | 10-01 (untouched) |
| zzz_Leon_Townfall_2K_P.utoc / .ucas / .pak | 50,364 / 185,446,608 / 347 | 10-01 22:56 (raw PC mod trio, also present) |

Notes:
- pakchunk0-ps5.pak was changed too: it is 89,213 bytes smaller than its backup. Find out what that change was, and
  whether it touches shared assets, shaders or the asset registry. It is a candidate for the "bright outdoors" problem.
- The raw PC Leon trio sits in the same folder. Check whether the engine also mounts it directly (PC-cooked content
  and PC shader code on the PS5): another candidate for the gray/white checker.
- Local copies: `.superpowers/native-dependency-spike/leon-iostore-adapter-trial-1/` (raw/ chunks of the mod,
  overlay/derived/ = the deployed utoc + _s1.ucas).
- Ask the integrator for anything else (e.g. the two .bak files or the current pakchunk0-ps5.pak).

## UPDATE (integrator, ~02:35 UTC): bright outdoors explained

pakchunk0-ps5.pak differs from its .bak in ONE entry only: Townfall/Platforms/PS5/Config/PS5Engine.ini. The new ini
turns off volumetric fog, eye adaptation and most shadows, and raises gamma: a PC "performance tweak" config. That fully
explains the bright, fog-free outdoors.

Details: outputs/2026-10-08-l25-prep/evidence/console-pulls-20261013-paks/FINDING-bright-outdoors.md (both pak files and
the extracted inis are in that folder). Focus the texture work on the Leon gray/white CHECKER (materials/shaders).
