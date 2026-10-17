# Real-file arguments (local copies of the downloaded mod and of the game's own files; read-only)
N=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations
LEON_ARGS=(--mod-utoc $N/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc
 --mod-ucas $N/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas
 --base-utoc $N/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc
 --base-utoc $N/townfall-main-utoc-2026-10-01/pakchunk1-ps5.utoc
 --base-utoc $N/townfall-main-utoc-2026-10-01/pakchunk10-ps5.utoc
 --base-utoc $N/townfall-main-utoc-2026-10-01/pakchunk11-ps5.utoc
 --base-utoc $N/townfall-main-utoc-2026-10-01/pakchunk12-ps5.utoc
 --base-container-header pakchunk0-ps5=$N/townfall-original-container-header-1/container-header.bin
 --script-objects-utoc $N/townfall-manifest-capture-2026-10-01/global.utoc
 --script-objects-ucas $N/townfall-global-scriptobjects-1/global.ucas)
