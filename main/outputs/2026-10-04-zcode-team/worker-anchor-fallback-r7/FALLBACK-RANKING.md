# FALLBACK-RANKING.md — anchor window ladder r7 (worker-anchor-fallback-r7)

Purpose: if launch-5 shows `anchor_E_match=0` in-game (rodata patched, COW, or
base anomaly), the bridge must already carry an ordered fallback ladder so the
SAME launch still anchors — and the diagnostics localize WHY E failed. This
document is the evidence ranking; the proposal diff implements exactly it.

## 0. Inputs

- Retained image: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
  194,241,424 B, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
  (re-verified this session; matches worker-c2-probe/EXPECTED-VALUES-OFFLINE.json).
- Windows: `/tmp/anchor_windows.json` (== the offline expected values).
- In-game facts: `outputs/2026-10-04-zcode-controller/console/c2-alternative-results.jsonl`,
  build `igf7-3dd949879a0a` — the launched igf7 probe (B,C,D,E,A reads with
  same-invocation mapspans gating).
- Mapping recovery: `worker-mapping-recovery/vmmap-v3-records.jsonl` +
  `recovered-spans.json`; checkpoint context: `worker-checkpoint-reconcile/CURRENT-STATE.md`.

## 1. Uniqueness — whole-file recount (this session, brute scan, all 5 windows)

| win | bytes | len | occurrences | file offsets | vaddr (canonical) | rt @ base 0x400000 | segment (phdr, flags) |
|-----|-------|-----|-------------|--------------|--------------------|--------------------|------------------------|
| E | AES S-box head | 64 | **1** | 0x7dd6a40 | 0x7dd2a40 | 0x81d2a40 | phdr1, flags 4 (rodata) |
| D | "Current Changelist\0" | 19 | **1** | 0x7b8c45d | 0x7b8845d | 0x7f8845d | phdr1, flags 4 |
| B | L"UE5" UTF-16 | 8 | **1** | 0x782a782 | 0x7826782 | 0x7c26782 | phdr1, flags 4 |
| C | "BuildIdOverride" UTF-16 | 32 | **2** | 0x75d3fda, 0x76cce58 | 0x75cffda (+2nd 0x76c8e58) | 0x79cffda (+2nd 0x7ac8e58) | phdr1, flags 4 (both copies) |
| A | GNU build-id note | 36 | **1** | 0xb93b250 | 0xbc5f250 | 0xc05f250 | phdr8, flags **0** |

The recount matches `EXPECTED-VALUES-OFFLINE.json` `occurrences_in_file`
exactly (independent re-derivation, not a copy). C's second copy sits at
p_vaddr 0x76c8e58 (rt 0x7ac8e58), inside the SAME rodata LOAD segment.

## 2. In-game igf7 verification status (cited, not re-run)

From `igf7-3dd949879a0a` `cand_read` records (all vs expected bytes, offline
comparison of live_hex):

| win | in-game result | live vs expected |
|-----|----------------|------------------|
| B | cand_read OK (gated_map_covered_data_read) | match (`5500450035000000`) |
| C | cand_read OK at canonical rt 0x79cffda | match (second site NEVER read in-game) |
| D | cand_read OK | match (`43757272656e74204368616e67656c69737400`) |
| E | cand_read OK | match (64B S-box) |
| A | **cand_refusal "uncovered"** — no dereference attempted | n/a |

## 3. Read-risk — which span covered D at the menu-state records?

Answer: **ONE span** — `[0x000000000787c000, 0x00000000082bc000) prot=1`
(read-only, no write, no exec) — the page-rounded rodata LOAD (phdr1). The
igf7 mapspans page 0 records it as `s0`; B (0x7c26782), C1 (0x79cffda),
**D (0x7f8845d)**, E (0x81d2a40) — and C's second site (0x7ac8e58) — all fall
inside that single span, so one covered read each, no partial-span risk, and
the 19-byte D read can never straddle a span boundary.

Corroboration and limits:
- `worker-mapping-recovery` (igf2-era v1/v2 recovery) independently confirms
  the eboot's FIRST span `[0x400000, 0x787c000) prot=4` (exec-only — the
  reason the ELF-header scan cannot anchor) but its recovered set contains NO
  readable span above 0x787c000; the data-span fact rests on the igf7
  same-invocation mapspans, which is the same process-state class the bridge
  runs in (module_start, menu state, pre-save).
- A (0xc05f250) is covered by NO readable span at this state — the in-game
  refusal proves it; any ladder that gated on A would deadlock the anchor.
  Reading A without the coverage gate would be a fault risk (phdr8 flags 0).

## 4. Mutation likelihood

- B/C/D/E: read-only rodata (phdr1 flags 4). The kernel cannot legitimately
  write these; a live mismatch implies image patch, COW anomaly, or wrong
  base — exactly the failure class the ladder exists for. Among them: E is a
  constant crypto table (mutation implausible), D/B are engine build metadata
  (a game UPDATE changes them — but then ALL windows change and the anchor
  must refuse; the ladder is for per-window anomaly survival, not for
  accepting a different image). C is a config-key string with a duplicate copy.
- A: flags-0 note segment, not even mapped readable at menu state — highest
  read-risk, zero gating value (never verified in-game).

## 5. The r7 ladder (what the proposal diff implements)

Ordered rungs, ordered evaluation, ALL tried every launch (r6 diagnostic
parity, ~200 B total gated reads, decision independent of evaluation order):

1. **E** (gate) — 64 B unique, igf7-verified, covered prot=1, constant table.
2. **D** (gate) — 19 B unique, igf7-verified, same span.
3. **B** (gate) — 8 B unique, igf7-verified, same span; shortest window, so
   ranked below D on identity strength despite equal uniqueness.
4. **C** (gate, BOTH sites required) — canonical 0x75cffda AND second site
   0x76c8e58 must each be covered+matching; only then does C count. This
   resolves its 2-occurrence ambiguity structurally (the bridge never scans;
   it reads fixed vaddrs, so "which copy" is decided by requiring both).
   Weakest gate rung: if C2 ever diverges, C simply never gates and D/B still
   hold the ladder up.
5. **A** (logged, NEVER gates) — unchanged r6 semantics; kept for the
   diagnostic trail and to observe future coverage changes.

**Gate rule: ONE full gate-rung match (E, or D, or B, or C-with-both-sites)
verifies base 0x400000.** r6's E-only requirement becomes "E first among
equals" — if E matches, the decision is identical to r6; if E is corrupted,
D/B/C independently hold the anchor while `anchor_E_mm_off`/`anchor_E_live_w0`
localize the corruption.

### Diagnostics per window tried (tag scheme, r6 names preserved)

| window | events |
|--------|--------|
| E | `anchor_E_covered` / `anchor_E_match` / `anchor_E_mm_off` / `anchor_E_live_w0` (unchanged) |
| D | `anchor_D_covered` / `anchor_D_match` / `anchor_D_mm_off` / `anchor_D_live_w0` |
| B | `anchor_B_covered` / `anchor_B_match` / `anchor_B_mm_off` / `anchor_B_live_w0` |
| C (site 1) | `anchor_C1_covered` / `anchor_C1_match` / `anchor_C1_mm_off` / `anchor_C1_live_w0` |
| C (site 2) | `anchor_C2_covered` / `anchor_C2_match` / `anchor_C2_mm_off` / `anchor_C2_live_w0` |
| C aggregate | `anchor_C_match` = 1 only if BOTH sites covered+matched |
| A | `anchor_A_covered` / `anchor_A_match` / `anchor_A_mm_off` / `anchor_A_live_w0` (unchanged) |

Launch-5's imminent `anchor_E_covered`/`anchor_E_match` lines keep their exact
names and order (E remains rung 0, evaluated first).

## 6. Honest limits

- C2 (second C site) has never been read in-game; the host test covers its
  logic, the first real launch that runs the r7 bridge supplies the fact.
- Menu-state span facts come from one igf7 invocation; a save-loaded state
  (launch-2 ENOMEM class) is handled by the unchanged r4 16384-entry snapshot.
- No claim is made that the ladder will pass under a game update (all windows
  change together — refusal is the correct outcome there).
