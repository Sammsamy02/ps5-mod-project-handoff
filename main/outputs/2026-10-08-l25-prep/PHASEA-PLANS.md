# PHASE A PLANS — Claude round-5 Part 2 (items #2, #5, #7, #8)

Agent GF (ZCode), 2026-10-08. DESIGN DOCS ONLY: Mac on battery, no builds,
no code-to-tree, no console. Prepared tools live in `phasea-tools/` and are
tested locally (18/18 checks, receipt `phasea-tools/phasea-test-receipt.json`).

**LAW (applies to every console step in every section below): NOTHING runs on
console without Claude review + explicit user OK.** Every "console test plan"
here is a proposal for review, not an authorization. The permanent PS5 rules
(AGENTS.md) stay in force: decrypt-off `-Q "SITE SELF"` readback for any
executable, never upload downloaded/modified executables unsigned, one launch
per test, user launches, klog captures to NEW files, no rapid-fire launches,
kstuffSharp quarantined.

Sources read for this pass: FZ-GAME2-PROFILE-DRYRUN.md, STATE-L26-WAVE.md
(§1-§9), agent-F-crash-forensics.md, the profile generator machinery
(`2026-10-07-l19-local-profile-r4/profiles/generate_profile.py`,
`worker-axes-detector/tools/ref_axes.py`, patternsleuth
`engine_version.rs` 30 versions / 244 markers, `offline_anchor_scan.py`),
`worker-kernel-pak/PAK-OVERLAY.md`, `worker-asset-convert/ASSET-CONVERT.md`,
`2026-09-30-unreal-container-survey.md`,
`2026-09-30-stellar-blade-content-discovery.md`, `MOD-CENSUS.md`, the
vendored UE4SS source (LuaMod.cpp pak/LogicMods handling), tonight's klog
crash captures, and the l26 build receipts (runtime.map + bare PRX).

---

## #2 SCANNER — profile generator generalization (UE4 + UE5)

### What the machinery actually is today (verified, not assumed)

- The **marker table already covers UE 4.7–5.8** (30 versions, 244 markers,
  parsed from `engine_version.rs`). The Townfall-UE5 shape is NOT in the
  version markers — `ref_axes.py`'s cost model is engine-family-agnostic and
  this pass validated it on synthetic UE4 images (below).
- The UE5-shaped parts are stages 3–4: the anchor packs
  (`guobject_array.rs` 9 patterns, `gmalloc.rs` 5, FName ctor slot), the
  static-layout window evidence families, and `FNameAbiProfile`
  (case_preserving, output_bytes 8 / alignment 8).
- Stage 1 (image identity: sha256+size+extent, SELF-container refusal) and
  stage 2 (axes: build_configuration + stats via markers) are already
  game-generic. Verified on Astro Bot: full pipeline refuses correctly with 0
  anchors (FZ §2).

### UE4 differences that need NEW packs (design)

| Concern | UE5 (Townfall, done) | UE4 (new pack needed) |
|---|---|---|
| FName storage | FNamePool block/stride allocator; slot-2 ctor decode (CB: 0x23d590 chain) | < 4.23: `TNameEntryArray` per-thread 2^k slots; ≥ 4.23: FNamePool (different block geometry). `WITH_CASE_PRESERVING_NAME` compile variants change the ctor signature itself |
| GUObjectArray | chunked FUObjectItem stride 0x18, `num_minus_available` spacing known | spacing "depends on version" (patternsleuth comment on the spacing capture); pre-4.23 FUObjectArray layout differs; hash-table arm dead on PS5 may not generalize |
| Reflection | FField (4.25+) | UProperty (pre-4.25) — different member-walk evidence family |
| Member/vtable offsets | STATIC-FACTS / REFLECTION-FACTS windows | same machinery, new offsets; two-source agreement rule unchanged |

Design: keep ONE marker table + ONE cost model; add per-family anchor packs
selected by the classifier's tier output. The `engine_version.rs` resolver
files (fname.rs, guobject_array.rs, gmalloc.rs,
fuobject_hash_tables.rs) stay the pattern sources; UE4-specific patterns get
authored in Phase B as `ue4-legacy` / `ue4-chunked` packs with the same
multi-pattern-agreement + refusal discipline. No pattern is trusted on a
single hit; anchors missing = REFUSED (the Astro Bot behavior generalized).

### Second-game dry-run procedure (Stellar Blade — PPSA13197, UE, IoStore v3)

PREREQUISITE (documented, NEVER performed here): the user's own copy must be
dumped first, per the decrypt-off law — download `eboot.bin` from
`/data/Stellar Blade 01.014.001 PPSA13197/` on a DEFAULT (decrypt-on) FTP
connection, cross-check on a second connection with `-Q "SITE SELF"` (real
on-disk magic `4f153d1d`/`5414f5ee`, true signed size), record both
sizes+sha256s, stage under a NEW
`.superpowers/native-observations/stellarblade-static-executable-<date>/`
dir. NEVER upload any downloaded/modified executable back (POSTMORTEM rule
2). Full procedure: FZ-GAME2-PROFILE-DRYRUN.md §4.

Dry-run itself (all Mac-static, after acquisition):

1. `engine_match.py <stellar-blade-eboot>` → tier + anchor-pack
   recommendation (expect UE5 FAMILY at minimum; version per markers).
2. Anchor scan (offline_anchor_scan.py pattern subset) with the recommended
   pack; multi-hit pin candidates only; misses = confidence tier drop.
3. Static-layout windows + two-source agreement; unknown/mismatch/conflict =
   refusal, per the l23→l26 lineage.
4. Emit profile via `generate_profile.py` generalized to take game name +
   facts JSON path (today it hard-pins Townfall's sha — that hard-pin is the
   deliberate single-game guard; game 2 gets its own instance script).
5. Only then does any runtime step get proposed — through Claude review.

### Auto-fallback / confidence tiers (generalized refusal)

Implemented in `phasea-tools/engine_match.py` (tested):

- **REFUSED** — triage (16 widest-coverage ungated markers) finds < 3, or
  full scan < 8 markers: "not Unreal / below evidence floor". Astro Bot:
  REFUSED in 0.24 s, 0/244 markers.
- **FAMILY_ONLY** — UE4 vs UE5 discriminated (all near-best version
  candidates share a major), minor version ambiguous. Anchor scan allowed
  with family pack; NO versioned profile.
- **VERSION_TENTATIVE** — version resolved at the ref_axes threshold
  (margin ≥ 4) but below the profile margin (12). Pins stay candidates.
- **VERSION_CONFIDENT** — margin ≥ 12 AND markers ≥ 25. Townfall lands here:
  UE5 5.6, shipping (margin 60), stats off (margin 12), 54/244 — byte-exact
  reproduction of the recorded classification, sha/extent match
  (`3b6cfad4…`, `0xbc602c8`).

Fake-SELF inputs are refused with the law quoted (never treat a download as
the real file without decrypt-off). `--diff A B` reports which markers a new
game adds/drops vs a profiled one (Townfall vs Astro: 54 differing, all
Townfall-side).

### Local validation (this pass, read-only fixtures)

18/18 checks green (`phasea-tools/run_phasea_tests.py`): Astro refusal +
sha; Townfall tier/family/version/axes/present-54/image-id; fake-SELF
refusal; synthetic UE4.27/4.7/5.8 cost-model resolution (margins 24–33) with
3-marker noise robustness; diff mode; symbolizer checks (§5).

### Console test plan (3-pass law) — PROPOSAL, needs Claude review + user OK

Static-only item; the only console content is the prerequisite dump (above)
plus one ordinary user launch of Stellar Blade UNMODDED with a klog tap to
observe its normal boot (no PRX targeting it exists yet).

- Pass criteria (x3 across separate console restarts, one launch each):
  the staged decrypted eboot's sha256 is identical across the three
  acquisitions attempts (it is one console; identity must not drift), and the
  static tier output is identical across the three runs of the tool on the
  same bytes (determinism).
- Fail criteria: any marker-count or tier change across runs on identical
  sha256 (tool nondeterminism — stop and fix before any profile work), or
  acquisition magic mismatch (wrong view — stop, never proceed to stage 2).

**Verdict: ready-to-build** for the classifier/tier tool (done, tested);
UE4 anchor packs are needs-X (author in Phase B before any UE4 game).

---

## #5 DEBUGGER — crash dump symbolization

### Survey: what PS5 crash artifacts exist (evidence-cited)

- **klog kernel crash blocks** (the real artifact — captured live in
  `/tmp/l25-variant-A/klog-20261009-021938.log`, two blocks): `signal`
  (SIGSEGV etc.), thread ID/name, proc ID/name, fault VA, **all registers
  including RIP**, a `backtrace:` section (observed empty — kernel noise
  interleaves), and the **dynamic libraries list**: every module with
  `xotext: <start>:<end>` range + `fingerprint` = the module's ELF GNU
  build-id. Verified cross-check: the eboot fingerprint
  `04bb4330a7d71dcefe1cc2c4d19769d5` == the profile's recorded build_id.
- **eboot-monitor logs** (`/tmp/l25-variant-A/eboot-monitor-*.log`): process
  start/stop transitions only — timeline corroboration, no addresses.
- **CE-108255-1** = Sony's generic "game crashed" UI code (no local Sony
  meaning table exists; receipts treat it as the crash-family label). The
  kernel block is the real evidence; never mine klogs for bare
  "CRASH"/"SIGSEGV" ("CRASH KILL_END" is the NORMAL quit path; "SIGSEGV:
  normal" lines are Mono/PSM banners — STATE §8).
- **No minidumps, no core files** on retail/homebrew PS5. Nothing else to
  survey locally.

### What a debugger would have caught tonight (crash history, cited)

- l24 crash (CE-108255-1 at save-load, pid 144): **zero instrumentation
  between init end and crash** (agent-F) — the crash RIP was in
  libkernel.sprx 0x80004920 with "copyin: has nonsleeping lock"; with the
  klog module list + our map we could have immediately said WHICH module and
  offset, and whether the PRX was even resident.
- The 20:58 "ghost crash" (pid 305, RTHeartBeat, libkernel+0x10ec) happened
  with ploader dead — PRX 23c8483d never executed; a module-list check
  (now automated) would have prevented the misattribution chase.
- FNAME_Add GC-storm (delete counts 6,181→13,970): symbolized function
  attribution would have named the storm site from the first crash artifact
  instead of a multi-agent binary hunt (CB eventually decoded
  Store 0x23c0b0 vs shard-Find 0x23c340).

### Symbolizing against OUR artifacts — verified state + gap

- `runtime.map` EXISTS in the l26 pass dirs (`l26-A/pass{1,2}/runtime.map`,
  514k lines) **but is the `-r` partial-link map: every VMA is 0**. It still
  carries the full per-function catalog (207,852 input sections; function
  granularity via `.text.<mangled>` sections).
- The **bare (pre-fake-sign) PRX**
  (`l26-package-A/pass-1/bare-original/…prx`) has section headers with final
  vaddrs (.text @ 0x10, size 0x123edcf) but **only .dynsym** (2 defined,
  NID-obfuscated): DT_INIT=0x15a0 = module_start, DT_FINI=0x1e00 =
  module_stop (verified from `.dynamic`).
- **Reconstruction** (implemented, flagged MAP-RECONSTRUCTED / order-assumed):
  place map input sections cumulatively at the PRX's final section vaddrs →
  85,487 function rows; map .text totals 0x11b8dc8 = 97.1% of the final
  .text (cross-build fixture: crash1-era map vs package-A PRX; same-build
  ratio will differ by the bootstrap units).
- **BUILD GAP (the needs-X)**: the final link (the .NET Packager) must emit a
  final-link map or a sidecar vaddr→symbol table, and/or keep an unstripped
  ELF next to the shipped PRX. Design: Packager writes
  `modbridge_supplier_composition.symbols.json` (final vaddr, size, name per
  section/symbol) beside the bare PRX — it knows the layout it just wrote.
  One harness change, Mac-safe to make in a build round; until then
  MAP-RECONSTRUCTED is honest-but-approximate and DYNSYM-EXACT covers exactly
  2 anchors.
- **Image-base law**: klog xotext start IS the live base. Townfall eboot:
  0x400000 (klog `0000000000400000:000000000787c000`, matching the proven
  in-game base). Our PRX's base comes from the same list when resident.

### Prepared tool (Mac-safe, tested now)

`phasea-tools/symbolize_crash.py`:
- `--klog FILE`: parses every crash block (signal, registers incl. RIP,
  fault VA, module list with base:end + fingerprint), symbolizes RIP +
  fault address against all loaded sources.
- Sources, best-first with explicit tiers: `SYMTEXACT-ELF` (future unstripped
  ELF), `DYNSYM-EXACT` (PRX dynsym + DT_INIT/DT_FINI naming),
  `MAP-RECONSTRUCTED` (order-assumed from `-r` map + PRX sections),
  `GAME-SIDECAR` (`game-symbols-townfall.json`: 22 cited addresses —
  LoadMap/InitGameState/ProcessEvent (UE4SS.log), the CB FName chain, the
  Variant-C Add, GUObjectArray/pool pins, and the full PAK-OVERLAY mount
  chain — each entry carries image-vs-runtime space + its receipt).
- `--addr A --base B` for single-address mode. Output human/JSON; exit 0
  even for "no symbols" (a clean negative is a result).

Tested (8/8): real klog → 2 blocks, eboot base law, build-id fingerprint
match, RIP→libkernel+offset; synthetic LoadMap+0x123 via sidecar;
module_start+0x0 EXACT via dynsym; MAP-RECONSTRUCTED tier at .text+0x4;
map .text coverage ratio 0.9713.

### Console test plan (3-pass law) — PROPOSAL, needs Claude review + user OK

**Opportunistic only — do NOT deliberately crash anything** (no
crash-inducing probes; the CE-108255-1 class is historically reproducible on
its own). Ride the ALREADY-AUTHORIZED launch windows (the pending l26-A
in-world test / future rounds): klog tap is already part of every runbook.

- Per natural crash (if one occurs): harvest per runbook → run
  `symbolize_crash.py --klog <new-capture> --prx <same-build bare PRX>
  --map <same-build runtime.map> --game-symbols <sidecar>`.
- Pass criteria: (1) module list parsed, our PRX present iff it was
  resident, base extracted; (2) RIP attributed to module+offset for EVERY
  block; (3) same crash bytes → identical symbolization across re-runs
  (determinism). 3 passes = 3 distinct natural crash artifacts (across
  restarts) each meeting 1–3.
- Fail criteria: RIP inside our PRX module range but MAP-RECONSTRUCTED
  disagrees with DYNSYM-EXACT anchors where they overlap (order-assumption
  broken → the build-gap fix becomes blocking); or module list missing when
  the PRX was verifiably resident (parser gap — fix before trusting tiers).

**Verdict: needs-X** — tool works today at tier-2 accuracy; exact
function-level PRX symbolization needs the Packager symbols-sidecar build
change (small, Phase B).

---

## #7 FILE OVERLAY — pak/IoStore mod loading (never modify the game folder)

### How PC UE4SS actually handles ~mods (source-verified, vendored tree)

- **UE4SS does not mount asset paks itself.** The GAME's own
  `FPakPlatformFile` boot-scans `Content/Paks/` (incl. community `~mods`
  subfolders on stock PC engines) and mounts `*_P.pak` patch paks at higher
  priority; asset mods ride the engine's own loader.
- UE4SS's own pak involvement is logic mods: it CREATES
  `<Content>/Paks/LogicMods` and reconstructs/lists it for Lua
  (`UE4SS/src/Mod/LuaMod.cpp:2859-2866, 2997-3014` — verified in the vendored
  source). Loose Lua/INI mods bypass paks entirely (UE4SS reads its own mod
  directory directly) — which is exactly what our PS5 port already does via
  `/data/modbridge/roots/PPSA33286/`.

### How Townfall PS5 mounts paks (eboot RE, PAK-OVERLAY.md — all vaddrs cited there)

- Full stock PakFile module present: `FPakPlatformFile::Initialize`
  0x1769ee0; boot scan of `<ProjectContentDir>Paks/` (observed on-disc:
  `/mnt/sandbox/PPSA33286_000/app0/townfall/content/paks`) via
  `IterateDirectory` (0x1769a20) collecting `*.pak` (FindPakFiles 0x176b410).
- `_P.pak` patch convention recognized IN-ENGINE (mount helper 0x176ec30
  tests `Contains("_P.pak")`); folder priority ranker 0x1770140 (4/3/2/1,
  project-content highest). **Subfolder recursion (~mods) NOT settled
  statically** (BSS visitor vtable) — placing mods directly in `Paks/` with
  `_P` names is the proved-safe surface.
- Game ships `pakchunk0..12-ps5.pak` + `.utoc/.ucas` (IoStore **v8**,
  unencrypted, TOC signed flags clear) + 339-B stub paks. Classic paks hold
  LOOSE FILES ONLY (4,699 entries: .res/.png/.ini/…, zero uasset) — all
  cooked content is IoStore. Container compatibility with PC mods: same v11
  compact index, same utoc v8 geometry, unencrypted (Leon trio verified).
- Unproved: engine ACCEPTANCE of any foreign/staged pak (the frozen trial
  staged bytes + readback but mounting was never observed); the diagnostic
  pak's entry flags were 0 vs game records `0xE0000000` — the concrete
  candidate fix.

### Overlay design — additive-only, staged, with an engine-mount end-state

Hard law: **never overwrite/modify any original game file**; additive-only
staged files with full rollback (the proven staged-transaction model:
`2026-10-01-townfall-mod-trial.md`). Even additive staging writes INTO the
game's Paks dir — flagging that tension honestly: mechanism (c) is the only
one that truly never touches the game folder; (a) mounts from OUR partition;
(b) is additive-but-in-folder and is the fallback + Phase 0 instrument.

- **(a) Engine-level runtime mount via our PRX (primary design).** Call the
  engine's own mount helper 0x176ec30 (or DLC loop 0x1764de0) with a pak
  file living under `/data/modbridge/roots/PPSA33286/paks/`. Requires:
  FString marshaling through the engine allocator — the round-2/3 bridge
  (push_fstring_to_lua/from_fname family) is the proven marshaling layer;
  timing after `FPakPlatformFile::Initialize` and before package resolution
  (QuickMountingPaks deferral means "after Initialize" is insufficient as
  stated — hook event must be settled); fail-closed prologue-signature
  verification per the existing pin law (vaddrs from the game profile,
  verified against image sha). Priority: `_P` name gives engine-native
  patch priority. This is the UE4SS-proven pattern, adapted.
- **(b) File-level redirect via ftpsrv-visible overlay + admission
  (fallback/Phase 0).** Stage `pakchunk9999-<mod>_<guid>_P.pak` (and
  utoc/ucas trio for IoStore) into the game Paks dir through the existing
  admission (SIZE-EXACT loadlist law, byte-exact readback, rollback).
  Naming sorts after every base chunk. NOT "never modify the folder" —
  additive-only with rollback; kept as the Phase 0 instrument (it isolates
  engine mount acceptance from our runtime code) and as the fallback if
  (a)'s timing window cannot be met.
- **(c) Kernel virtual-file injection (A2 in PAK-OVERLAY) — Phase B dep.**
  Kernel module synthesizes pak entries when the game enumerates its Paks
  dir; nothing ever lands in the game folder. Depends on the kernel track
  (hardest; kstuffSharp is quarantined — noted, not designed here).

### Risks / unknowns (honest list)

- Engine acceptance of a foreign pak/trio: UNPROVED (Phase 0 exists to
  settle it — diagnostic pak rebuilt with game-identical `0xE0000000`
  records + an observable cvar effect, one launch, behind Claude review).
- IoStore "signing": no signing barrier OBSERVED (all captured TOCs
  Townfall v8 + Stellar Blade v3 + Leon PC trio: encrypted flags clear,
  signed flags clear, PAK index unencrypted) — but utoc validation flags
  and the `_P` numeric bump constants remain unextracted.
- `~mods` subfolder recursion unobserved at runtime; assume direct `Paks/`
  placement until one observation settles it.
- AMPR layering (app0 is AMPR-backed) may complicate (c); sandbox path
  filtering must be exact.
- Game updates re-number chunks: additive names (`9999`, guid) avoid
  collisions by construction.

### Console test plan (3-pass law) — PROPOSAL, staged behind Claude review

Stage gating: Phase 0 (acceptance observation) → Phase 1 (additive staging
overlay) → Phase 2 (engine-level mount via PRX). Each stage is its own
Claude-reviewed packet; nothing below runs otherwise.

- Phase 0, per pass: stage the rebuilt diagnostic pak (game-identical
  records) + klog tap + ONE user launch; PASS = engine-log/cvar observable
  effect (mount evidence, not a file-landing); FAIL = no effect with bytes
  verified staged (readback) → iterate on flags/geometry, never blind-replay.
- Phase 1/2, per pass: mod pak staged/mounted + ONE user launch; PASS =
  observable in-game substitution of a KNOWN original asset (duplicate-file
  counters or directory listings do NOT count — container-survey law);
  x3 across restarts with byte-identical staged bytes and identical
  outcomes; FAIL criteria: any crash (note code+when, harvest, reboot),
  any original-file hash change (readback guard), or inconsistent outcome
  across the 3 passes (stop; suspect timing window).

**Verdict: needs-X** — design is grounded in proved RE + container evidence;
blocked on Phase 0 mount-acceptance observation (one reviewed launch) before
overlay engineering is worth building; (c) additionally blocked on the
kernel track (Phase B).

---

## #8 ASSET CONVERSION — PC→PS5 textures/meshes

### Starting from what the user already did by hand (evidence)

- The user sourced + the team staged the **Leon 2K PC mod unchanged** on
  console (`zzz_Leon_Townfall_2K_P.{pak,utoc,ucas}` trio; frozen trial:
  exact readback verified, path matching proved via 68/127 package-ID
  matches; **platform-cook compatibility explicitly NOT proved** — that
  trial's pending visible check is the arbiter for whether PC-cooked bytes
  load at all).
- Three ORIGINAL PS5 files (SaveData.png, PS5Engine.ini, DefaultEngine.ini)
  were recovered from the game pak and repacked into a v11/None pak by the
  pinned `repak` — the proven loose-file roundtrip (`unreal-container-survey`,
  `compatibility-implementation-checkpoint`).
- Studio side (`/WORKSPACE/Projects/ai-game-studio/`): a Blender-based
  original-asset pipeline exists (crypt asset recipes, `engine/asset_manifest.py`)
  — different engine target (Unity), but it proves local batch Blender usage.

### Mac tooling survey (verified this pass, 2026-10-08)

| Tool | State |
|---|---|
| Blender 4.5.13 LTS CLI | PRESENT `/Applications/Blender.app/Contents/MacOS/Blender` (mesh/texture source authoring; `--background --python` batching) |
| repak (pinned upstream build, arm64) | PRESENT `.superpowers/.../unreal-library-container-survey-1/mount-research/tools/build/aarch64-apple-darwin/release/repak` |
| retoc v0.1.5 CLI (arm64) | PRESENT `.superpowers/.../retoc-v0.1.5-raw-roundtrip-1/tool/retoc_cli-aarch64-apple-darwin/retoc` |
| project C components | PAK v11 reader+writer (`mb_pak_write_v11_none`), IoStore reader v3/v8, `none_packer`/`container_header_merge`/`directory_merge` + vendored BLAKE3, DDS facts-parser contract, package-header reader — all with existing host test receipts |
| sips (Apple) | present; ImageMagick/ffmpeg NOT installed (brew exists; NO installs without user OK — Mac on battery + no-purchase/no-network laws) |
| python imaging (PIL/numpy) | ABSENT from system python 3.9.6 — pipeline code must be pure-stdlib (or vendored single-file) |
| Oodle / UnrealIoStoreTools in SDK trees | **ABSENT** — `ps5-payload-sdk/bin` = clang/lld/llvm only. NOT on the critical path: every captured Townfall/Leon/Stellar-Blade container declares None compression (4,699/4,544 records checked). A future compressed-container game would need a separate, explicitly-authorized Oodle solution — refuse silently bundling one. |

### Target formats

PS5 UE = standard cooked UE formats inside IoStore (.uasset descriptor +
.ubulk payload), with the platform deltas exactly as ASSET-CONVERT.md §3
established: **BC1–BC7 block bytes are identical PC/PS5**; what changes is
the descriptor (GNF `GnmTexture` registers, not a DDS header) and the block
ORDER (AMD GFX10 `SW_64KB_S` macro-tile swizzle at 4×4-block granularity,
per mip). Lossless header-rewrite + block-permutation, no transcode.

### Minimal TEXTURE-swap pipeline (Mac-runnable steps, mirroring ASSET-CONVERT §4)

1. Intake/inventory + wrong-game refusal (exists: download_prepare).
2. Unpack container (exists: PAK v11 reader, IoStore reader, None-block
   assembly).
3. Extract PC `.ubulk` payload (or source DDS if the mod ships one) →
   neutral BC block form (DDS-contract normalizer design exists).
4. **Per-mip GFX10 SW_64KB_S block permutation — the one new core primitive**
   (port DrSwizzler/Mesa addrlib equations; block-granularity copies; 3D
   textures fail closed).
5. Patch `Texture2D` platform data: PC→PS5 EPixelFormat map, GnmTexture
   descriptor, bulk offsets/sizes (extends the existing package-header
   reader to an export-level walker).
6. Verify offline: deswizzle(converted) == neutral blocks per-mip by SHA-256
   (CUE4Parse deswizzler as the inverse oracle); determinism byte-check.
7. Repack (exists: IoStore merge/pack + BLAKE3 rehash, or PAK v11 writer),
   preserving `zzz_…_P` priority naming + mount points.
8. Install via the §7 overlay; in-game acceptance = separately-run gate.

### Minimal MESH-swap pipeline

v1 REFUSES with a named requirement (`mesh.platform_cook missing`):
SkeletalMesh render-data re-serialization for PS5 (vertex buffers, GNM
vertex declarations, platform inline data) is un-researched; the captured
15,249-byte mesh reference package is the entry point for the Phase B
research pass. Leon 2K's full-model subset stays blocked on this; its
texture subset is pipeline-addressable. Blender's role is SOURCE authoring
(export OBJ/glTF/fbx → a PC-side UE cook step still needed for real mesh
mods — documented boundary: no PC UE cook happens on this Mac).

### Local test plan (now) + console test plan (PROPOSAL, behind Claude review + the §7 overlay)

- Local (each build step): synthetic BC1/BC7 fixtures, non-square/non-multiple-of-8
  extents, sub-8×8-block mips, per-mip roundtrip hashes; Leon texture
  packages as read-only fixtures (output = candidate, never claimed
  installed).
- Console: rides §7's staged plan — a converted texture pak/trio is just
  another overlay payload; acceptance = visible in-game texture change,
  x3 across restarts, byte-identical inputs, identical outcomes.

**Verdict: texture path ready-to-build** (primitive port is well-specified
with an inverse oracle; all container stages already exist and are tested);
**mesh path blocked-on-platform-cook research** (P3); Oodle absent but not
needed for the captured corpus (documented refusal for anything compressed).

---

## Tool inventory delivered (phasea-tools/)

| file | role | tests |
|---|---|---|
| `engine_match.py` | #2 markers classifier/diff: identity+triage+axes+tiers+anchor-pack recommendation; SELF refusal; `--diff` | 10 checks |
| `symbolize_crash.py` | #5 crash symbolizer: klog blocks, RIP/fault VA, module ranges+fingerprints, 4 symbol-source tiers | 8 checks |
| `game-symbols-townfall.json` | #5/#7 sidecar: 22 cited Townfall addresses (space-labeled) | via symbolizer |
| `run_phasea_tests.py` | suite; writes `phasea-test-receipt.json` | 18/18 PASS |

All fixtures read-only (Astro inner ELF, Townfall decrypted eboot, klog
capture, l26-A runtime.map, package-A bare PRX). No tree edits, no builds,
no console contact.
