# Agent GF — PHASE A items #2/#5/#7/#8 (designs + Mac-safe tools)

2026-10-08. DESIGN DOCS ONLY executed: no builds, no tree edits, no console
contact. Deliverables: `PHASEA-PLANS.md` (four full sections with console
test plans, each gated on the law: nothing runs on console without Claude
review + user OK) and `phasea-tools/` (two tools + sidecar + test suite,
18/18 checks PASS, receipt `phasea-tools/phasea-test-receipt.json`).

## Verdicts

| # | Item | Verdict | The one thing it needs |
|---|---|---|---|
| #2 | Scanner generalization | **ready-to-build** (tool done+tested) | UE4 anchor packs authored (Phase B) before any UE4 game; Stellar Blade eboot acquisition (user's copy, decrypt-off law) before its dry-run |
| #5 | Crash symbolizer | **needs-X** | Packager must emit a final-link symbols sidecar (or unstripped ELF) — today's runtime.map is the `-r` map (all VMA=0) and the PRX carries only 2 dynsym anchors; tool works now at MAP-RECONSTRUCTED (order-assumed) accuracy |
| #7 | File overlay | **needs-X** | Phase 0 mount-acceptance observation (one Claude-reviewed launch with game-identical `0xE0000000` pak records); kernel-free engine-mount path (0x176ec30) is the primary design, additive staging the fallback, kernel virtual files = Phase B dep |
| #8 | Asset conversion | **ready-to-build** (texture path) / **blocked-on-platform-cook research** (mesh path) | Port the GFX10 SW_64KB_S per-mip swizzler with the deswizzle roundtrip oracle; all container stages already exist+tested; Oodle absent but unneeded (captured corpus is all None-compression — refuse silently bundling one) |

## Key new facts found (beyond the brief)

1. **The marker table already spans UE 4.7–5.8** — the "Townfall-UE5 shape"
   lives in the anchor packs/layout stages, not the version classifier;
   synthetic UE4.27/4.7/5.8 images resolve with margins 24–33 (validated).
2. **runtime.map in every l26 pass dir is the `-r` partial-link map** (all
   VMA=0; 207,852 per-function sections). Full symbolization reconstructs
   85,487 functions at 97.1% .text coverage (cross-build fixture) — honest
   tier, but the build should emit final symbols.
3. **klog crash dumps carry per-module xotext base:end + the ELF build-id**;
   the eboot fingerprint equals the profile build_id
   (`04bb4330a7d71dcefe1cc2c4d19769d5`) — verified live crash capture
   (/tmp/l25-variant-A/klog-20261009-021938.log, 2 SIGSEGV blocks parsed).
   No minidumps exist on PS5; CE-108255-1 is just the UI label.
4. PC UE4SS never mounts asset paks itself (source-verified: LuaMod.cpp only
   creates/lists `Paks/LogicMods`) — the PS5 overlay should call the game's
   own mount helper (0x176ec30, `_P`-aware), exactly the PAK-OVERLAY §4
   design, now backed by our FName/FString marshaling bridge.

## Local validation summary (read-only fixtures)

- Astro Bot inner ELF → REFUSED in 0.24 s (0/16 triage, 0/244 markers;
  sha `7723d150…` matches FZ's record).
- Townfall eboot → VERSION_CONFIDENT: UE5 **5.6**, shipping (margin 60),
  stats off (margin 12), **54/244 markers**, sha `3b6cfad4…`, extent
  `0xbc602c8` — byte-exact reproduction of the recorded profile facts.
- Fake-SELF input → refused with the decrypt-off law quoted.
- Symbolizer: real klog (bases, fingerprints, RIP→libkernel+offset),
  synthetic LoadMap+0x123 via cited sidecar, module_start+0x0 EXACT via
  DT_INIT dynsym, MAP-RECONSTRUCTED tier verified.

Mac battery respected: pure-stdlib Python only, no installs, no builds.
