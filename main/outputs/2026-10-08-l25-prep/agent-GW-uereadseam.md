# agent-GW — UE read-seam answers (Claude round-6 #2)

2026-10-10. Mac-only analysis + local tool runs; battery-safe, no builds, no
console. Full detail: `CODE-READ-SEAM.md` (same directory). Tree edits
confined to phasea-tools (+fixtures) per task rules.

## (a) Verdict + test 19

**UE4 coverage in the 18 tests: ONE synthetic-only check, zero UE4 bytes.**
`run_phasea_tests.py:84-99` builds presence vectors from the marker table
itself (`covers()`), not from any binary — it validates cost-model
self-consistency for 4.27/4.7/5.8 (margins 24-33, 3-flip noise ok), nothing
more. GF's "synthetic UE4 images resolve" wording overstated (no images
involved); GJ's UE4 table rows are seed bands, confidence 0.0.

**Modified Unreal build: the tool did NOT fail closed — real hole, found and
fixed.** Tiers keyed only on the RELATIVE margin: simulated tampers kept
margin 20 (CONFIDENT-grade) while absolute misfit exploded (best_cost 43 ->
103 with 12 foreign old-version markers spliced). Fix (small + documented):
`engine_match.py` gains `VERSION_CONFIDENT_MAX_MISFIT = 1.0` —
`misfit_ratio = best_cost/present_count` must be <= 1.0 for CONFIDENT, else
demote to VERSION_TENTATIVE with an explanatory tier_meaning. Baseline
Townfall = 0.796 (passes); tampered = 1.146 (demoted).

**Test 19 (real tampered bytes):** `fixtures/make_gw19_fixture.py`
deterministically derives `fixtures/townfall-modified-build.elf` from the
Townfall positive control — 12 native markers corrupted in EVERY PT_LOAD
occurrence (16 sites) + 6 old-version-only foreign markers spliced into
data-segment zero slack (manifest: `fixtures/tamper-manifest.json`).
Fixed tool on the fixture: `VERSION_TENTATIVE, UE5, present=48, misfit=1.146,
margin=20` — downgraded, never CONFIDENT. Suite: **25/25 PASS** (18 original
+ 4 sidecar checks a parallel session added + 3 gw19 checks); receipt
refreshed in `phasea-tools/phasea-test-receipt.json`. Honest limits: sha/pin
image-identity remains the deeper modified-build gate (unchanged,
console-proven); a code-only patch that touches no marker string is caught
downstream by sha/pins, not by the classifier; UE4 real-image validation
remains impossible on this Mac.

## (b) The seam + recommendation

**The runtime never reads running code.** The 0x63D590 prologue verification
reads a **user-staged DECRYPTED FILE COPY** once at module_start:
`runtime_start_bridge.cpp:1932` (module_start -> `bridge_code_cache_prepare`)
-> `:1213-1222` Ops = `sceKernelOpen(O_RDONLY|O_NOFOLLOW)` / `sceKernelPread`
/ `sceKernelClose` on `/data/homebrew/<title>/decrypted/eboot.bin`
(`CodeSignatureProfile.hpp:36-39`, NOT /app0 — that is the encrypted SELF) ->
`CodeImageCache.hpp:86-127 Cache::prepare`: whole-file 16 KiB streaming,
whole-image SHA-256 compare (:119), <=8x64 B pin-prefix lift+compare (:120),
60 s budget, live-base/extent agreement (:89). Admission then gates on the
CACHE: `UnrealInitializer.cpp:1149` -> `bridge_code_cache_context`
(:1265-1271) -> `named_entry` (`CodeImageCache.hpp:34-41`: registered pin +
file-backed X-not-W PT_LOAD + live exec-mapping cross-check) ->
`bridge_code_cache_signature` (:1272-1278) -> `Cache::prefix` (:81-84, memcmp
vs disk bytes). Live text is excluded by policy AND hardware
(`ImageCoverage.hpp` R-and-not-X rule; "The live exec page may never be
read" — shadow_module_source.hpp:384); behavioral probes CALL the function
instead. **Measured on console: 194,241,424 bytes in 2130 ms (~91 MB/s)**
(evidence/l24-diag.jsonl).

**Reuse for full-image generation: yes, trivially** — the seam already
streams the whole image; the generator just runs marker scans / xref census /
prefix consensus on the same chunks (coverage: all file-backed PT_LOADs;
base is fixed 0x400000 so linked labels == file view). The ONLY gap for a new
game's first launch is acquiring the decrypted copy: in-process options are
nil (/app0 gives the encrypted SELF; live text unreadable).

**Recommendation — option 1 (existing disk seam, path/sha generalized to
runtime inputs) + option 5 (refuse-and-degrade to seed bands + behavioral
verify) as the built-in degradation; option 4 (Phase B kstuff-lite
REMOTE_SYSCALL kekcall 0x500000027 cross-process read — the only
zero-staging path) documented as the Phase B upgrade.** Options 1/5 (+3,
live readable-span DETECT-only pre-check) need no Phase B; option 2 (host
FTP dump) is the unavoidable acquisition step feeding 1.

**Correction for the doc owner (GI):** FIRST-LAUNCH-AUTOMATION.md:100-109
"scan the loaded image in memory / readable spans" is true only for the
DETECT string scan; the GENERATE arms need text bytes that readable-span
policy excludes — the disk seam must be named as the text source. GW did not
edit that file (tree-edit rules).

## Artifacts

- `outputs/2026-10-08-l25-prep/CODE-READ-SEAM.md` (this task's deliverable)
- `phasea-tools/engine_match.py` (misfit-ratio guard)
- `phasea-tools/run_phasea_tests.py` (test 19 block, 25 checks)
- `phasea-tools/fixtures/{make_gw19_fixture.py, townfall-modified-build.elf,
  tamper-manifest.json}`
- `phasea-tools/phasea-test-receipt.json` (25 passed / 0 failed)
