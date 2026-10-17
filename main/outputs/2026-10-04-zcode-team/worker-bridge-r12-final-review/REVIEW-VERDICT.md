# REVIEW-VERDICT — runtime_start_bridge_r12.cpp (final pre-composition-6 review)

Worker: worker-bridge-r12-final-review · 2026-10-04 · independent, read-only; proposed fixes as diffs only.
Reviewed bytes: `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp`
sha256 `feed52544023360326c17f7015336f90c4906789b344a805cfb6debb67bdd7a9` (702 lines, 35,775 bytes — hash verified against the comp6 driver's `BRIDGE_MEASURED_SHA`, `build_runtime_composition6.py:82`, and ONE-PASS-CHECKLIST.md P6).
All paths relative to workspace root `/WORKSPACE/.codex/worktrees/translation-core/what` (below: `bridge:NNN` = lines of the reviewed file).

## VERDICT: GO-WITH-FIXES

No P0/P1. The r12 bridge is memory-safe, its production anchor table is byte-verified against the retained image (including the C second-occurrence arithmetic), the r11 page-quantized lease registration is correct, the Resolution-B configured-roots attach is exactly the corrected §2.7 mechanics, the import surface SHRANK vs r11, and no tick address is referenced anywhere. Nothing found blocks composition-6. The fixes below are a carried-over interface-contract hole (unreachable from the shipped entry), two diagnostic-hardening nits, and test/ledger staleness — all P2/P3, all shippable as-is if the batch closes before they land.

---

## Checklist verdicts

### 1. Memory safety — PASS

- **19 MB bss vmmap**: `kBridgeVmmapEntries = 16384` (bridge:162) × pinned `sizeof(kinfo_vmentry) = 1160` = 19,005,440 bytes zero-fill bss ("~19 MiB" comment accurate). Object-level confirmation: my compile of the production source shows `g_bridge_vmmap` plus the two function-local `mappings[16384]` statics (`window_anchor_verify` bridge:337, `bridge_register_readable_ranges` bridge:464; 16384 × 24 B = 384 KiB each) and `g_image_regions[128]` — total ≈ 19.8 MiB bss, all zero-init, no heap, no VLA. Serialized: `module_start`/`module_stop` are mutually exclusive via the entry's `entry_guard` (bridge:59-60 comment; entry-review §5), and both `mappings[]` statics are used only inside that single thread, never concurrently.
- **Walk bounds, loop 1 (r6 walker, bridge:194-225)**: `while (off + kKveHdr <= len)`; `ksz` sanity `[64, sizeof(kinfo_vmentry)]`, 4-aligned, else `stop=1` (fail loudly); `off + ksz > len` → truncation `stop=2`; writes to `out[]` guarded by `entries < capacity` (bridge:212). `entries` counts beyond capacity but is only *counted*, never written — safe.
- **Loop 2 (r7 kve_path capture, bridge:272-289)** and **loop 3 (r9 discovery scan, bridge:298-319)**: identical bound predicates (`< 64`, `> 1160`, misaligned, overruns → break). Consistency: both only run after loop 1 returned `stop != 1`, so their silent `break` can never see a record loop 1 would have failed on. One latent duplication: loops 2/3 use the constant `kve_sizeof_ = 1160` (bridge:174) while loop 1 uses `sizeof(kinfo_vmentry)` (bridge:198). Equal today (`KINFO_VMENTRY_SIZE 1160`, SDK `sys/user.h:463`); a `static_assert` would pin it (P3 diff below).
- **Field offsets independently verified** against the pinned SDK header (`ps5-payload-sdk/target/include/sys/user.h:466-488`): `kve_structsize`@0, `kve_start`@8, `kve_end`@16, `kve_protection`@56, `kve_path`@136, tail `kve_path[PATH_MAX=1024]` → 1160. Bridge reads 8/16/56/136 — all correct.
- **All buffers bounded**: `g_observed_exe_path[1040]` copy bounded by `n < sizeof-1 && n < path_cap` and requires NUL-in-record (bridge:280-285); the discovery prefix loops are index-bounded (`k+5 <= cap3`, bridge:309; `k < 4 && k < cap3`, bridge:305); `copy_detail` always NUL-terminates within capacity (bridge:69-85); the ladder copy loop (bridge:438-441) cannot overflow (`i + 1 < capacity`).
- No overflow path in any of the three walk loops; `count = min(total, kBridgeVmmapEntries)` clamps (bridge:342, 470) so the possibly-capacity-exceeding `total` (and the truncated `entries + 1` lower bound, bridge:267) can never index past a `mappings[]` array.

### 2. The anchor gate — PASS (production table independently re-verified)

- I recomputed the vaddr arithmetic and extracted the bytes from the retained image (`decrypted-eboot.bin`, sha `3b6cfad4…`) using the seed table's translation rule (LOAD0/LOAD1: `file_off = linked + 0x4000`; phdr8: `file_off = linked − 0x324000`):
  - B `0x7826782` → file `0x782a782` MATCH; C `0x75cffda` → `0x75d3fda` MATCH; **C2 `0x76c8e58` → `0x76cce58` MATCH**; D `0x7b8845d` MATCH; E `0x7dd2a40` MATCH; A `0xbc5f250` → file `0xb93b250` MATCH.
  - Whole-file occurrence scan: B, D, E each occur **exactly once** (at the table addresses); C occurs **exactly twice — at exactly `0x75cffda` and `0x76c8e58`**, the two table entries. The "2 legitimate occurrences" premise and the second-occurrence address are both correct; the either-site rule (bridge:374-376, `ok[3] || ok[4]`) covers precisely the two real sites.
  - `kImageExtentVaddr = 0xbc602c8` (bridge:150) = max(p_vaddr+p_memsz) = phdr8 `0x9633420 + 0x262cea8 = 0xBC602C8` (independently hex-added; matches the seed table's pt_loads). `kCandidateBase 0x400000` = the proven single-load-bias.
- Either-site logic: table order is E(0), D(1), B(2), C(3), C2(4), A(5) (bridge:134-139); C/C2 correctly carry `required=false` (participate only via the either-site clause, r8 semantics); A stays refused-but-logged and is correctly absent from the return expression (bridge:376).
- Per-window diagnostics complete: `anchor_cov` (coverage incl. read-bit + `rt+length <= end` bound, bridge:355-360), `anchor_match` (bridge:366), `anchor_mm` with first-mismatch offset (bridge:363-367), plus `anchor_c_either` (bridge:375) — sufficient to triage any single-window failure from one log read.

### 3. Lease registration — PASS (cap safe; two P3 hardening notes)

- **Page quantization is round-outward and correct**: `b = b_raw & ~(kPage-1)` (down), `e = (e_raw + kPage - 1) & ~(kPage-1)` (up), `kPage = 0x4000` PS5 page (bridge:473, 486-487). The r11 root cause is genuinely fixed: extent end `0x400000+0xbc602c8 = 0xfc602c8` is not page-aligned (`& 0x3fff = 0xc8`), the quantized end `0xfc64000` now covers the final partial page the per-page lease walk would otherwise hit ("Owned read range includes unregistered bytes", launch-8).
- **128-region cap vs reality**: launch-7 in-game evidence (`bridge_lease_reg=6`, transport-ledger composition6-mirror) shows 6 readable∩extent spans against a 128 cap — ~21× headroom; even a fully fragmented 22,828-entry vmmap cannot plausibly yield >128 readable spans inside the 197 MB image extent on this single-image target. The cap cannot overflow the array (`registered < 128` in the loop condition, bridge:475). Gap: saturation is *silent* — `bridge_lease_reg` cannot distinguish "exactly 128" from "capped at 128". Downstream failure mode is still fail-closed (the lease throws on the unregistered span), so P3, with a counting diff below.
- **Raw `new` failure**: `new PLH::OwnedMemory::ScopedRegion(...)` (bridge:488) throws `std::bad_alloc` → propagates out of `bridge_register_readable_ranges` → outer catch (bridge:692-696) → `MRS_ERR_CONVERSION` with `error.what()` in detail. Honest abort; earlier regions stay registered process-lifetime (intentional per bridge:455-457) and `g_image_region_count` is set only after the loop, so no stale count is published. One P3: the throw path skips the `bridge_lease_reg` diagnostic entirely (the `0xffffffff` event only covers query failure, bridge:466-469).
- **Round-outward over-claim** (start rounded below the observed span start; end rounded past the extent): registers a small superset. Consumers only walk adapter-derived ranges (exact span bounds), so nothing reads the over-claimed pages; noted honestly because the OwnedMemory contract says "genuinely owned, accessible bytes" — the alternative (exact-end) is the r11 bug. Correct trade; record-only.

### 4. The path ladder — PASS (no substitution)

- Rung order: (1) kernel-observed `g_observed_exe_path` (source 1, bridge:398) → (2) base `executable_path` op probe — 8-byte probe call, then the real call must also return 0 with non-empty `out` (bridge:401-408), also labeled source 1 since it is the kernel sysctl route → (3) configured `/app0/eboot.bin` gated by `std::filesystem::is_regular_file` (bridge:409-412, source 2) → none (source 0 + the r10 stat fork, bridge:413-437). Matches the ladder comment (bridge:382-391) exactly.
- **NO substitution**: the r9 own-module rung (source 3) is gone; grep confirms zero `source = 3` in r12; the only configured constant is `/app0/eboot.bin` (bridge:395) — never the PRX. The r10 stat block is diagnose-and-refuse only (emits `stat_app0/stat_ownmod/stat_rootdir` + errno values, returns −1, bridge:421-436). Hardcoded diagnostic paths match the staged layout constants (title PPSA33286, PRX path — same as entry-review item 2 verified).
- Per-capture cache clearing: `g_observed_exe_path[0]='\0'; g_observed_exe_path_valid=false;` at the top of EVERY `bridge_query_vmentry` (bridge:246-247), and the supplier's documented capture order (VMMAP query before executable_path) guarantees same-capture state only. The wrapper's own `ops.query_vmentry = bridge_query_vmentry` (bridge:559) means every supplier capture re-clears. Correct r8 semantics preserved.
- Labels: `bridge_exepath_source` emitted as 1 (bridge:405), 0 (bridge:435), or the selected source (bridge:444); `bridge_exepath_sig` on the success path (bridge:445). Matches the design's labeling requirement (Resolution design §2.7 note).

### 5. Configured-roots attach — PASS

- **Thread-safety of the non-const mutation**: single-threaded at that point. `modbridge_runtime_start_with_activation` runs on the loader thread inside `module_start`; `module_start`/`module_stop` are serialized by the entry's `entry_guard` (bridge:59-60; entry-review §5 verified the atomic test-and-set), and the only other thread — the UE4SS init thread — does not exist until `owner->start()` inside Stage 3 (bridge:658), strictly after the mutation at bridge:636-637. `converted` is a by-value sret local (`LmfConversionResult` by value per ODR-SAFETY §3) owned by this frame; the supplier borrows `ops` by const-ref, untouched by the facts mutation. Safe.
- The mechanics match the CORRECTED §2.7 hunks exactly (flag set before conversion at bridge:611; roots attached after the `!converted` gate at bridge:636-637; the design's original post-conversion-flag mistake is not present). Distinct fields, no positional init, no ODR contact beyond what the ODR audit already owns.
- **Distinctness from `module_file_path`**: `configured_game_directory = "/app0"` (constant, bridge:149), `configured_working_directory = mod_root` (entry parameter), vs `module_file_path` = the PRX (conversion input, unchanged). Design invariants 1-4 hold in the bridge: configured values never feed the scanner/identity, and the PRX is never served as the executable path.
- Diagnostics: `bridge_cfg_gamedir` / `bridge_cfg_workdir` = 1 emitted immediately after attach (bridge:638-639) with the exact §27-corrected spelling; only on the success path (a failed conversion emits the `lmf_*` capability set instead, bridge:621-625 — correct, nothing was attached).

### 6. Cross-checks — PASS

- **No wrong tick**: grep of r12 for `0x33ac8c0` / `0x37ac8c0` / `0x2fac8c0` / any tick reference — zero hits (the only match is the unrelated include path string "tick-file-owner-1", bridge:14). The corrected tick slot (runtime `0x33ac8c0` = base + linked `0x2fac8c0`; the WRONG value being `0x37AC8C0` or linked-`0x33ac8c0`) appears nowhere; the bridge contains no tick constant at all.
- **Import set vs r11 (nm)**: compiled both revisions from the production sources against the refreshed packaging overlay (see REPORT) — r12's undefined set is a **strict subset** of r11's (59 → 57 symbols; r11's two extras `std::__next_prime` and `basic_string` copy-ctor are weak-instantiation codegen noise from the const→non-const `converted` change; r12 adds ZERO new symbols). My r12 object matches the controller's `/tmp/r12-preflight/r12.o` 1:1 on the entire lmf/RC/modbridge/PLH set modulo host-toolchain artifacts (macOS leading-underscore mangling, stack-protector, host memset/memcpy idioms — absent under the target packaging argv). No import-contract regression.
- **Tool-expected events**: `post_launch_check.py` `INTERESTING_OBSERVER_TAGS` (14 tags, :190-193), `rung_check.py`, and `console_batch.py`'s CP classifier (`classify_d1_log`, :1140-1149) all key on ENTRY-emitted tags (`module_start_entered/return`, `probe_lifecycle_result`, `runtime_wiring_begin`, `mfa_*`, `runtime_start_ok/failed/skipped`, `runtime_stop_*`) — none removed or renamed by r12 (bridge emits none of them; the entry does, unchanged since r3). The r12-specific tags the launch docs/design expect — `bridge_exepath_source`, `bridge_lease_reg`, `stat_app0/ownmod/rootdir(+_ec)`, `bridge_cfg_gamedir`, `bridge_cfg_workdir` — are all emitted with exact spellings (bridge:405/435/444, :493, :422-434, :638-639). No tool greps the bridge tags yet, so no tag-consumer breakage is possible.

### 7. Residue from the r8→r12 velocity — three items, all non-blocking

1. **host_test_r8 staleness** (test-side, evidence hygiene): the harness correctly compiles the r12 source (`#include "../runtime_start_bridge_r12.cpp"`, test_r8.cpp:92) and the ledger records `PRODUCTION_TEST_ALL_PASS_12_12_SECTIONS_1_4` (2026-10-05T01:25Z) — but the test still carries r9-era debris: the `source=3` own-module comment block (:157-175, rung removed in r10), and a **vacuous** check `last_ev("bridge_kve_prefix") == 0x61617061u || last_ev(...) != 0xffffffffu` (:135 — the tag is never emitted by r9+ code, `last_ev` returns `0xdeadbeef ≠ 0xffffffff`, so the check cannot fail). The r12 delta (opt-in flag, `bridge_cfg_*`, non-const attach) has no host assertions; the local capture files (`run-out.txt` etc., 0 bytes, mtimes 20:23-20:25) predate the 21:18 rebuild — the 12/12 receipt lives only in the ledger mirror. Section 5 (lease) remains honestly PENDING per the ledger.
2. **Stale rehearsal overlay cannot compile r12** (integration note, already owned by ODR FINDING-1): `worker-comp6-driver/staging/lineage-overlay-2-rehearsal/` holds OLD-vintage headers — compiling r12 against it fails loudly (`no member named 'allow_configured_roots_for_missing_executable_path'`; `no member named 'configured_game_directory'`) while r11 compiles clean. This is fail-closed, and the real driver restages the three Resolution-B headers unpinned + the ODR-gated Platform header (`DEFAULT_STAGE_HEADERS`, `build_runtime_composition6.py:119-120,144`), after which both revisions compile clean (verified). Do not hand-compile the bridge against the on-disk rehearsal overlay.
3. **Carried-over entry-review items, status**: P2-2 (activation leak on early returns) — **still present** (bridge:526-543; adoption at :543 after both early returns). Unreachable from the shipped entry (never NULL, single-shot), but the header contract (`runtime_start_bridge.h:53-57` "OWNERSHIP IS ALWAYS TRANSFERRED on non-NULL input") is still violated; diff below. P2-3 (compile evidence) — now RESOLVED for r12: `/tmp/r12-preflight/r12.o` exists and my independent compile reproduces its symbol set against the new-vintage headers. P3-2 (`MRS_ERR_STOP_EXN == MRS_ERR_CONTEXT == 2`, `runtime_start_bridge.h:38,41`) — still present; renumber diff below. P3-4 (`#include "ue4ss_backend.hpp"` unused by the bridge, bridge:36) — still present, harmless. P1-1/P2-1 are entry/gate-side, out of this file's scope.

---

## Findings (ranked) with proposed diffs

### P2-1 (carry-over, interface contract) — activation leak on the bridge's own early returns

Unreachable from the shipped entry, but `runtime_start_bridge.h:53-57` promises unconditional transfer on non-NULL input, and a future caller trusting it leaks the activation on `MRS_ERR_INTERNAL`. Fix by adopting first (same fix entry-review proposed; re-based to r12:525-543):

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
@@ -525,20 +525,6 @@ extern "C" int modbridge_runtime_start_with_activation(const char *module_file_path,
 {
     auto &slot = owner_slot();
-    if (module_file_path == nullptr || mod_root == nullptr || title_id == nullptr)
-    {
-        copy_detail(detail, detail_capacity, "bad arguments");
-        return MRS_ERR_INTERNAL;
-    }
-    if (slot)
-    {
-        copy_detail(detail, detail_capacity, "runtime already started");
-        return MRS_ERR_INTERNAL;
-    }
-
-    // Adopt the activation unconditionally: the ActivatedMods deleter
+    // Adopt the activation unconditionally and FIRST: the ActivatedMods deleter
     // (ActivationDeleter -> mfa_release) runs on EVERY exit path below, so
     // ownership always lands in exactly one place and nothing leaks on
     // failure. A null activation (not reachable from the C entry, which only
@@ -543,6 +529,17 @@ extern "C" int modbridge_runtime_start_with_activation(const char *module_file_path,
     // calls after a successful mfa_activate) is consumed and refused by the
     // prepare call's own activation validation.
     modbridge::native::ActivatedMods mods{activation};
+    if (module_file_path == nullptr || mod_root == nullptr || title_id == nullptr)
+    {
+        copy_detail(detail, detail_capacity, "bad arguments");
+        return MRS_ERR_INTERNAL;
+    }
+    if (slot)
+    {
+        copy_detail(detail, detail_capacity, "runtime already started");
+        return MRS_ERR_INTERNAL;
+    }
```

### P3-1 — lease-cap saturation is silent; add span-count evidence

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
@@ -473,10 +473,14 @@ static void bridge_register_readable_ranges()
     constexpr uint64_t kPage = 0x4000;  // PS5 page size
     unsigned registered = 0;
-    for (size_t i = 0; i < count && registered < 128; ++i) {
+    unsigned readable_spans = 0;  // readable spans inside the extent (cap input)
+    for (size_t i = 0; i < count; ++i) {
         if ((mappings[i].protection & kVmProtRead) == 0) continue;
-        const uint64_t b_raw = mappings[i].start < extent_begin ? extent_begin : mappings[i].start;
-        const uint64_t e_raw = mappings[i].end > extent_end ? extent_end : mappings[i].end;
-        if (b_raw >= e_raw) continue;
+        const uint64_t b_raw = mappings[i].start < extent_begin ? extent_begin : mappings[i].start;
+        const uint64_t e_raw = mappings[i].end > extent_end ? extent_end : mappings[i].end;
+        if (b_raw >= e_raw) continue;
+        ++readable_spans;
+        if (registered >= 128) continue;  // cap: keep counting, stop registering
         // r11 (launch-8 root cause): ... (unchanged)
```
and after the loop:
```diff
@@ -492,6 +496,7 @@
     }
     g_image_region_count = registered;
+    ::diagnostic_event("bridge_lease_reg_spans", (uint32_t)readable_spans);
     ::diagnostic_event("bridge_lease_reg", (uint32_t)registered);
 }
```
(`spans > 128 && reg == 128` then reads unambiguously as "capped"; launch-7 evidence predicts spans=6.)

### P3-2 — `kve_sizeof_` duplication: pin it to the type

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r12.cpp
@@ -173,6 +173,8 @@
 static constexpr uint32_t kKveHdr = 64;   // pre-path header bytes
 static constexpr uint32_t kve_sizeof_ = 1160;  // pinned SDK sizeof(kinfo_vmentry)
+static_assert(sizeof(kinfo_vmentry) == kve_sizeof_,
+              "kve_sizeof_ must equal the pinned SDK sizeof(kinfo_vmentry)");
```

### P3-3 — ladder probe reads potentially uninitialized `probe[8]`

If the base op ever violated its contract (return 0 without writing), `probe[0]` is uninitialized (bridge:401-403). Defense is free:
```diff
@@ -400,7 +400,7 @@ static int bridge_executable_path(void *ctx, char *out, size_t capacity)
     const char *configured = "/app0/eboot.bin";
     const char *value = nullptr;
     uint32_t source = 0;
     if (g_observed_exe_path_valid) { value = g_observed_exe_path; source = 1; }
     const lmf::LiveModuleOsOps &base =
         *static_cast<const lmf::LiveModuleOsOps *>(ctx);
-    char probe[8];
+    char probe[8] = {0};
```

### P3-4 (carry-over entry-review P3-2) — `MRS_ERR_STOP_EXN == MRS_ERR_CONTEXT == 2`

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.h
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.h
@@ -38,7 +38,7 @@
 #define MRS_ERR_CONTEXT    2  /* NativeProgramContext build/validation threw */
 #define MRS_ERR_START      3  /* prepare/start threw or returned null */
 #define MRS_ERR_INTERNAL   4  /* defensive: bad arguments / already started */
-#define MRS_ERR_STOP_EXN   2  /* modbridge_runtime_stop: exception escaped stop */
+#define MRS_ERR_STOP_EXN   5  /* modbridge_runtime_stop: exception escaped stop */
```
(Entry only distinguishes zero/nonzero today; re-baseline any future log reader.)

### P3-5 (test-side) — host_test_r8 debris

```diff
--- a/outputs/2026-10-04-zcode-team/worker-module-start-wiring/host_test_r8/test_r8.cpp
+++ b/outputs/2026-10-04-zcode-team/worker-module-start-wiring/host_test_r8/test_r8.cpp
@@ -133,7 +133,7 @@
     int rc = bridge_query_vmentry(nullptr, m, 16, &n);
     chk("144B walk ok (rc=0 n=2)", rc == 0 && n == 2);
     chk("144B record REFUSES full path (valid=false)", g_observed_exe_path_valid == false);
-    chk("kve 8B prefix logged", last_ev("bridge_kve_prefix") == 0x61617061u /* LE "/app" = 61 70 70 61 */ || last_ev("bridge_kve_prefix") != 0xffffffffu);
+    chk("kve prefix events emitted", last_ev("bridge_kve_distinct0") != 0xdeadbeefu && last_ev("bridge_kve_eboot") != 0xdeadbeefu);
```
plus delete/replace the r9-era `source=3` comment block (:157-175) and add one assertion for the r12 delta, e.g. after a successful `window_anchor_verify()`: assert `bridge_cfg_gamedir`/`bridge_cfg_workdir` emission via a mocked conversion — or, minimally, a comment recording that the r12 attach surface is target-observable only (`bridge_cfg_*` + `bridge_exepath_source=0`).

---

## Non-findings explicitly checked and cleared

- Anchor bytes/occurrences: all six windows byte-match the retained image; B/D/E unique; C exactly 2 sites = the table (item 2).
- `kImageExtentVaddr` and page arithmetic re-derived independently (0x9633420+0x262cea8=0xBC602C8; 0xfc602c8→0xfc64000).
- SDK field offsets 8/16/56/136/sizeof-1160 verified in `sys/user.h` (item 1).
- Walk safety with `total > capacity` and the truncated `entries+1` contract (item 1).
- No tick constant anywhere; no `source=3`; PRX never served as game executable (items 4, 6).
- Import set: r12 ⊆ r11; controller preflight object reproduced (item 6).
- All tool-consumed tags are entry-side and unchanged; r12 emits its design-specified tags exactly (item 6).
- comp6 driver default = r12 with the exact reviewed sha (`build_runtime_composition6.py:79-82`); ONE-PASS-CHECKLIST P6 pins the same sha.
- r10 stat block and r11 quantization carried into r12 untouched (r11→r12 diff is exactly the three §2.7 hunks).
- `modbridge_runtime_stop` path unchanged from the entry-reviewed lineage (owner-slot guard, catch → `slot.reset()` + `MRS_ERR_STOP_EXN`).
