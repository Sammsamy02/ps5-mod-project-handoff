# CODE-READ-SEAM.md — how the runtime reads code bytes, and what first-launch automation can reuse

Agent GW, 2026-10-10. Mac-only analysis + local tool runs (battery-safe; no
builds, no console). Answers Claude's round-6 #2 review questions:

- **(a)** Do the 18 phasea tests cover UE4, and does detection fail closed on
  an unrecognized or MODIFIED Unreal build?
- **(b)** For first-launch automation: how does the on-console runtime get a
  readable copy of the game executable when running code is execute-only?

Tree edits made (within the authorized phasea-tools scope only):
`engine_match.py` (misfit-ratio tier guard), `run_phasea_tests.py` (test 19),
`fixtures/make_gw19_fixture.py` + `fixtures/townfall-modified-build.elf` +
`fixtures/tamper-manifest.json`, refreshed `phasea-test-receipt.json`.

---

## (a) What the 18 tests actually covered — and the UE4 truth

The 18 checks in `phasea-tools/phasea-test-receipt.json` (pre-GW state):

| # | check | fixture | what it proves |
|---|---|---|---|
| 1-2 | astro-refused, astro-sha | Astro Bot inner ELF (non-Unreal) | REFUSED tier + image identity |
| 3-7 | townfall-tier / -family-version / -known-axes / -present-54 / -image-id | Townfall eboot (UE 5.6.1 shipping) | UE5 positive: VERSION_CONFIDENT 5.6, 54/244 markers, sha/extent |
| 8 | fakeself-refused | fake-SELF container | container refusal (decrypt-law message) |
| 9 | ue4-synthetic-cost-model | **NO BINARY — synthetic presence vectors** | cost model self-consistency only |
| 10 | diff-townfall-astro | Townfall vs Astro | presence diff = the 54 |
| 11-18 | klog/sidecar/prx/map checks | crash log, PRX, runtime.map | symbolizer (`#5` tool), unrelated to engine detection |

**UE4 answer: exactly ONE test touches UE4, and it is synthetic-only.**
`run_phasea_tests.py:84-99` builds the "presence" vector from the marker table
itself (`em.covers(m, target_idx, 2, False)`), never from a UE4 binary's
bytes; ideal vectors for 4.27 / 4.7 / 5.8 plus 3 random flips resolve back to
their versions with margins 24-33. It validates that the cost solver is
internally consistent for UE4-shaped evidence — it validates NOTHING about
real UE4 images (no UE4 eboot exists on this Mac). GF's report wording
"snythetic UE4.27/4.7/5.8 images resolve" (`agent-GF-phasea.md` key fact 1)
overstates: no images were involved. GJ's per-version table carries UE4 rows
as seed bands, confidence 0.0, TRAINING-KNOWLEDGE-marked — consistent, but
also not validation.

**Modified-build answer before GW: NO test, and the tool did NOT fail
closed.** Simulated tampering of the real Townfall presence vector (GW,
`/tmp/gw19_explore.py`, seed-free analytic recipes):

| tamper | best_cost | margin | tier (old logic) |
|---|---|---|---|
| baseline Townfall | 43 | 20 | VERSION_CONFIDENT |
| corrupt 1/3 of present markers | 49 | 20 | VERSION_CONFIDENT |
| corrupt 2/3 | 43 | 14 | VERSION_CONFIDENT |
| splice 6 old-version-only markers | 73 | 20 | VERSION_CONFIDENT |
| splice 12 | **103** | **20** | **VERSION_CONFIDENT** |

The tier ladder keyed ONLY on the relative margin, so a modified build with
garbage-consistent evidence still got CONFIDENT-grade pins — precisely
Claude's suspicion.

### The fix (small, obvious, documented)

`engine_match.py` now carries an absolute-fit guard:

```python
VERSION_CONFIDENT_MAX_MISFIT = 1.0   # best_cost <= present_count for CONFIDENT
```

`classify()` computes `misfit_ratio = best_cost / present_count`
(present_count >= REFUSED_MAX_PRESENT = 8 is guaranteed on this path). A
CONFIDENT-grade margin with `misfit_ratio > 1.0` demotes to
VERSION_TENTATIVE with a tier_meaning that names the demotion. Rationale for
the 1.0 constant: the known-good baseline Townfall misfit is 43/54 = 0.796
(the table's own imperfection: 8 present-but-uncovered, 3 covered-but-absent
for 5.6/shipping/nostats); a real modified build measured 1.146-1.65.
FAMILY_ONLY / REFUSED logic unchanged; `--diff` unaffected structurally.

### Test 19 (real tampered bytes, not a simulation)

`fixtures/make_gw19_fixture.py` deterministically derives
`fixtures/townfall-modified-build.elf` (194,241,424 B) from the Townfall
positive control:

- **corrupt** the first 12 present markers in marker-table order — byte at
  needle+6 XOR 0x01 in EVERY PT_LOAD occurrence (16 occurrence sites; 8
  markers occur multiple times, max 14, all flipped so presence truly drops);
- **splice** the first 6 absent old-version-only markers (table order,
  last <= 4.20, shipping-compatible gate) utf-16le+NUL into the largest
  all-zero run of any non-text PT_LOAD (0x8ff23dd, LOAD3 slice) — simulates
  strings merged in from an old branch.

Full recipe + per-offset manifest: `fixtures/tamper-manifest.json`.
Result (real run of the fixed tool):

```
tier: VERSION_TENTATIVE   family: UE5   version: 5.6
present: 48   best_cost: 55   misfit_ratio: 1.146   margin: 20
```

The suite now runs 25 checks — 25/25 PASS (was 18/18; four sidecar-pairing
checks were added to `#5` by a parallel session after the 18-check receipt;
test 19 adds three):

```
gw19-fixture-integrity            PASS  sha=081b907056703290 corrupt=12 splice=6
gw19-modified-build-not-confident PASS  tier=VERSION_TENTATIVE family=UE5 present=48 misfit=1.146
gw19-demotion-cause-is-misfit-guard PASS margin=20 (CONFIDENT-grade) but misfit=1.146 > 1.0 -> demoted
```

Regression guards: `townfall-tier` (untampered) still VERSION_CONFIDENT
(0.796 <= 1.0); astro/fakeself refusals unchanged; the UE4 synthetic
cost-model check is tier-independent and still passes. Receipt:
`phasea-tools/phasea-test-receipt.json` (25 passed, 0 failed).

**Honest limits of test 19**: (i) it proves the CLASSIFIER's tier honesty on
degraded evidence; the deeper fails-closed layer for modified builds remains
image-identity binding at pin time (CodeImageCache whole-sha refusal) plus
anchor multi-arm verification — unchanged and already console-proven; (ii)
the tamper recipe is one shape (string corruption + old-string splice); a
modified build that only patches CODE bytes without touching any marker
string legitimately still classifies as its base version at this layer and is
caught downstream by the sha/pin layers; (iii) UE4 real-image validation
remains impossible on this Mac — still synthetic-only, now stated plainly.

---

## (b) The code-read seam — exact mechanism, then the reuse answer

### How today's runtime reads code bytes (it does NOT read running code)

The runtime NEVER reads live text pages. The prologue-signature verification
of the 0x63D590 FName ctor (and SCO/ToString/listener pins) works off a
**disk-file stream of a user-staged DECRYPTED COPY of the eboot**, read once
at `module_start` and authenticated by whole-image SHA-256:

1. **Trigger** — `outputs/2026-10-08-l26-package-B/lineage/lifecycle/runtime_start_bridge.cpp:1929-1936`:
   `module_start` calls `bridge_code_cache_prepare(observed_base, resolved_base, resolved_image_end, ...)`.
   The comment (1929-1931): "all code-signature bytes and their full-image
   identity are acquired through one opened file stream NOW. The absolute 60s
   deadline starts here and remains the initializer's admission budget."
2. **Syscalls** — `runtime_start_bridge.cpp:1213-1222`: concrete `Ops`:
   `open = sceKernelOpen(path, O_RDONLY|O_NOFOLLOW, 0)` (:1218),
   `pread = sceKernelPread(fd, bytes, length, offset)` (:1219-1220),
   `close = sceKernelClose(fd)` (:1221). Plain kernel file I/O — no
   `sceKernelVirtualQuery`+`mprotect` dance, no alias mapping, no live-text
   dereference anywhere on this path.
3. **What file** — `lineage/lifecycle/CodeSignatureProfile.hpp:36-39`:
   `kProfile.path = "/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin"`
   — a DECRYPTED copy the user staged on /data (NOT `/app0/eboot.bin`, which
   is the encrypted SELF container), pinned sha
   `3b6cfad47dd17fea...`, size 194,241,424, extent 0xbc602c8, base 0x400000,
   6 pins (:28-35).
4. **Authentication + prefix extraction** — `lineage/CodeImageCache.hpp`
   `Cache::prepare` (:86-127): profile sanity (:88); **live module base and
   extent must agree with the profile** (:89 — the ASLR/geometry gate); open
   (:92-93); stream the WHOLE file in 16 KiB chunks (`kChunk=16384` :21)
   hashing SHA-256 while parsing phdrs from the first chunk and collecting
   each pin's <=64 prefix bytes at its file offset (:98-112,
   `memcpy(e->bytes,...)` at :110-ish per the l18 original); EOF probe;
   **whole-image SHA-256 compare** (:119, "whole code-image SHA256
   mismatch"); **per-pin byte compare** (:120, "authenticated function
   prefix mismatches profile"); publish (:126). Budget: `kBudgetMs=60000`
   (:22); pins/loads/phdrs caps `kMaxPins=8, kMaxLoads=16, kMaxPhdrs=32`
   (:21, EQ bumped 4->8 on 2026-10-09).
5. **Console-proven throughput** — `outputs/2026-10-08-l25-prep/evidence/l24-diag.jsonl`:
   `code_image_bytes_read_low = 194,241,424`,
   `code_image_elapsed_ms = 2130` → **~91 MB/s; the entire 194 MB image
   streams in ~2.1 s inside a 60 s budget** (two pids in the log, both full
   reads).
6. **Admission consumption (no live read)** — candidate
   `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`:
   Phase 1 `consume_required_slot_seeds` (:1149)
   `admission_ops.code_identity(base + kFNameCtorWchar.label, sizeof(kPrefix))`
   → `L13AdmissionOps::code_identity` (:924-937) →
   `bridge_code_cache_context` (`runtime_start_bridge.cpp:1265-1271`) →
   `Cache::named_entry` (`CodeImageCache.hpp:34-41`): the (address,size) must
   match a REGISTERED pin exactly; the pin must sit fully inside one
   file-backed PT_LOAD with ELF flags X-and-not-W (:38); when module-info
   exists, the LIVE mapping cross-check requires the reported segment
   protection to be exec-and-not-writable (:39, `(r.protection&4u)&&!(r.protection&2u)`).
   The actual bytes check: `bridge_code_cache_signature`
   (`runtime_start_bridge.cpp:1272-1278`) → `Cache::prefix`
   (`CodeImageCache.hpp:81-84`) — a **memcmp against the cached DISK bytes**.
   The 0x63D590 gate is therefore "file bytes at linked 0x23d590
   (+ base 0x400000), whole-image-sha-authenticated, live-mapping-geometry-
   confirmed" — never a read of the execute-only page. Live execution (the
   behavioral probes, e.g. the slot-2 FNAME_Find ByteProperty roundtrip) then
   CALLS the function, which is the permitted operation on XOM.
7. **Why not live reads** — the policy is explicit:
   `typescan-r4/include/ImageCoverage.hpp` `from_profile` requires PF_R set
   AND PF_X clear (`if(!(s.flags&4u)||(s.flags&1u)) return hit;`) — readable
   spans are data/BSS only; `from_module` marks readable = R-and-not-X.
   `NativePropertyReadBackend.inl:20-27 PropertyReadBytes` = budgeted
   VQ-coverage proof + memcpy, data addresses only;
   `UnrealInitializer.cpp:1044-1050 read_direct` is the "sole direct
   heap-read site", VQ-proven. `shadow_arm.hpp` (full-image disk cross-check
   arm) states the law via `shadow_module_source.hpp:384`: "The live exec
   page may never be read — that is the whole point." klog crash dumps
   listing "xotext" module segments corroborate exec-only text.
   `UnrealInitializer.cpp:1032-1043` declines running shadow_arm inside the
   admission deadline (no cancellation seam), but the arm exists and reads
   the same disk image.

### Can the first-launch generator reuse this seam for full-image scanning?

**Yes — it is the ONLY console-proven full-image code-byte source, and it
already does the hard part.** `Cache::prepare` streams the complete file
(today only to hash it and lift <=8x64 pin bytes); a generator would keep
streaming the same fd/chunks and run marker scanning / xref census / prefix
consensus on the chunks. Numbers: 194 MB in 2.13 s measured; a full
scan_markers pass is a handful of `memmem`-class passes over the same stream
(well within budget); coverage = every file-backed PT_LOAD byte (text,
rodata, data-file-part). NOT covered: BSS (memsz>filesz tail) and
relocation-applied values — irrelevant for static anchor derivation, and the
main eboot base is fixed 0x400000 (klog-eboot-base-law test 12), so linked
labels == file view.

**The catch (Claude's actual question): the seam needs a decrypted file at a
known path at module_start time.** Today that is a compiled-in per-game
constant pointing at a USER-STAGED dump. For a NEW game's first launch, that
copy does not exist yet, and in-process options to create it are nil:
`/app0/eboot.bin` reads back the encrypted SELF container (userland has no
decrypt path; the kernel decrypts straight into the XOM mapping), and live
text is unreadable by policy and by hardware.

### Options ladder + recommendation

| # | option | what | depends on | verdict |
|---|---|---|---|---|
| 1 | **Existing disk seam generalized** | stream the staged decrypted eboot exactly as `Cache::prepare` does; run DETECT + GENERATE arms on the chunks; record sha as the generated profile's image identity | user stages the dump (option 2) BEFORE first launch; code: make `kProfile.path`/sha runtime inputs (title-id-keyed discovery) instead of compile-time constants — same direction FIRST-LAUNCH-AUTOMATION.md already takes for profiles | **RECOMMENDED** — every primitive console-proven; fails closed naturally (missing file -> "code-image startup open failed" -> refusal) |
| 2 | FTP/host-side acquisition (feeds 1) | the user's documented dump flow (decrypt-on download, provenance cross-check per PREREQ-CHECKLIST / PS5 rules); NOT on-console at first launch | none (already documented) | the unavoidable acquisition step for 1; one host round-trip per game |
| 3 | Live readable-span scan | in-memory marker scan over data/BSS (and R rodata) spans via `bridge_image_range`/PropertyReadBytes | nothing new; UNVERIFIED assumption that every marker-bearing segment maps R on real PS5 eboots | DETECT-only; cannot do xref census (needs text). Useful as a pre-check before demanding a staged copy |
| 4 | Phase B REMOTE_SYSCALL read | kstuff-lite kekcall `0x500000027` executes a syscall in an arbitrary pid (FZ-PHASEB-KUSTUFF-LITE.md:70, 95-104; interface source-verified in kstuff-lite-v1.11-VERIFIED.md): read the game pid's mapped pages cross-process — live decrypted AND relocation-applied bytes | Phase B (kernel kekcall integration, policy gates, console-verified; quarantine law still applies to other modules) | the only path to TRUE zero-staging first launch; later upgrade, not a blocker |
| 5 | Refuse-and-degrade | seed-band pins (confidence 0, never armed as pins) + behavioral verification only (slot-2 ByteProperty probe, GMalloc V1/V2, pool words, SCO empty-callback) + TENTATIVE profile with image_sha unbound | nothing | the built-in degradation when no staged copy exists; weakest pins, self-test criticals still gate |

**Recommendation: option 1, with option 5 as its in-seam degradation and
option 4 documented as the Phase B upgrade.** Dependency graph:

- Work TODAY (no Phase B): 1 + 2 + 5 (+ optional 3 as a pre-check). All
  required primitives are in the shipped, console-verified runtime; the only
  new code is profile-input generalization plus the generator arms already
  specified in FIRST-LAUNCH-AUTOMATION.md section 2.
- Phase B only: 4 (removes the staging dependency entirely, at the cost of
  the kernel dependency and its policy gates).

**Correction needed in FIRST-LAUNCH-AUTOMATION.md** (owned by GI/next agent;
GW did not edit that file per tree-edit rules): section 2a's "Scan target:
the loaded image in memory ... admission-owned readable spans" (:100-109) is
correct only for the DETECT string scan and is WRONG as a full-generator
premise — readable-span policy excludes PF_X segments by design
(ImageCoverage.hpp), so the xref-census/prefix-consensus arms cannot run
in-memory. The generator's text-byte source must be named: the CodeImageCache
disk seam (option 1), with the staged-copy prerequisite stated honestly.
Law 1's "prologue-VERIFIED through the existing owned-copyin signature path"
conflates VERIFICATION (which the seam does, against pinned bytes) with
DISCOVERY (which needs scanning text bytes the seam can stream but readable
spans cannot).

---

## Sources

[LOCAL-SOURCE] phasea-tools/{engine_match.py, run_phasea_tests.py,
phasea-test-receipt.json, fixtures/*}; outputs/2026-10-08-l26-package-B/
lineage/{lifecycle/runtime_start_bridge.cpp, lifecycle/CodeSignatureProfile.hpp,
lifecycle/SeedTable.h, CodeImageCache.hpp}; outputs/2026-10-08-l26-runtime/
typescan-r4/{candidate/src/UnrealInitializer.cpp,
include/NativePropertyReadBackend.inl}; outputs/2026-10-07-l18-local-xom/units/
{CodeImageCache.hpp, shadow_arm.hpp, shadow_module_source.hpp,
ImageCoverage.hpp}; evidence/l24-diag.jsonl (code_image diagnostics);
FIRST-LAUNCH-AUTOMATION.md; agent-{GF,GJ,GI}-*.md; FZ-PHASEB-KUSTUFF-LITE.md;
kstuff-lite-v1.11-VERIFIED.md; outputs/2026-10-08-l26-runtime/parallel-candidate/
04-console-signature-interpretation.md (0x63d590 marker expectation).
[MEASURED] all engine_match runs + the 25/25 suite receipt, this session,
this Mac. [INFERENCE] rodata-R mapping assumption flagged under option 3.
