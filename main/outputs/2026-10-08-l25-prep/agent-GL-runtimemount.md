# agent-GL-runtimemount — runtime no-game-file pak mount design (2026-10-10)

Claude round-6 item 3b. DESIGN ONLY: no builds, no console, no tree edits, no
network; read-only on .superpowers records. Deliverable:
`outputs/2026-10-08-l25-prep/PAK-RUNTIME-MOUNT-DESIGN.md`.

## Recommended mechanism

**Ladder, not a single pick** (evidence decides between the two IoStore
deliverers):

1. **Phase 0 observation** (one reviewed launch, read-only): timestamp
   `module_start_entered` vs the engine's Paks-folder enumeration, and log
   which IPlatformFile seam opens `pakchunk0-ps5.{utoc,ucas}`. This single
   launch settles both open timing/seam questions below.
2. **Rung 1 — M-A1 engine mount call (classic paks)**: call the RE-pinned
   engine mount helper (0x176ec30 / wrapper 0x176b900) from the game thread
   with a pak under `/data/modbridge/roots/PPSA33286/paks/`, verify by
   engine-state readback (MountedPaks TArray at [this+0x20/+0x28] gains our
   0x38-byte entry + engine-side FileExists). Smallest safe rehearsal of
   "engine's own path"; delivers loose-file mods data-only.
3. **Rung 2 — B1 or B2 by Phase-0 evidence**:
   - **B1 pre-scan enumeration lie** (preferred): VMT-pump vtable swap on the
     platform-file instance before `FPakPlatformFile::Initialize`'s scan;
     engine's own boot path discovers/mounts/registers our `_P` trio from
     virtual directory entries. Engine-native everything, no per-update
     rebuild; missed window degrades gracefully to vanilla.
   - **B2 content substitution** (Leon-at-runtime): hook OpenRead/FileSize;
     serve the derived merged utoc + a virtual `_s1.ucas` sidecar from our
     root; everything else passthrough. Content acceptance is GAME-proven
     (the Leon trial); the base-utoc sha guard makes a game update fail
     CLEAN to vanilla instead of feeding stale offsets.
4. **Rung 3 — A2 native IoStore container add**: end-state, blocked on new
   offline RE (zen/FIoDispatcher entry unpinned).

Update-survival contract met by design: nothing is ever written to the dump
(pass criterion = full-dump sha manifest identical before/after), and every
mechanism fails closed (pin mismatch or sha mismatch → mod quiet, game
vanilla).

## Blockers (honest)

1. A2 zen container-registration entry + late store-merge semantics: UNPINNED
   (needs offline eboot RE; may require boot-time discovery → then only B1).
2. B1's pre-scan window: unmeasured on this build (stock-UE5 order says it
   exists — training-knowledge until Phase 0 receipts it).
3. B2's seam: if PS5 .ucas reads bypass IPlatformFile (direct handles),
   substitution can't redirect the sidecar — Phase 0 must observe the open.
4. Foreign-container acceptance unproved (B1's gamble); merged-index
   acceptance proven (B2's foundation); PC-cook platform compat is a separate
   #8 question.
5. Missing static pins (small offline pass): GPlatformFileManager static,
   FPakPlatformFile vtable identity, OpenRead/FileSize slots (IterateDirectory
   +0x1c0 and FileExists +0x80 are already RE-pinned).
6. Escalation law honored: if Phase 0 shows both userspace seams unusable,
   "no game file change at all" needs kernel mediation (Phase B L4,
   prohibited) — STOP and escalate to Claude; Leon-disk/GK-builder stays the
   proven fallback.

Division with GK (in flight): GK's generalized builder produces the derived
index/sidecar artifacts; this design consumes them at runtime without ever
writing them into the dump.
