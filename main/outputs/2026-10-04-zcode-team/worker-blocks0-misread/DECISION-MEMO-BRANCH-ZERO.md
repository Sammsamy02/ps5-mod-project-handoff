# DECISION-MEMO-BRANCH-ZERO — pre-staged plan if `pool_range_verdict_fresh == 0`

Worker: worker-blocks0-misread · 2026-10-05 · READ-ONLY analysis + this memo.
No source edits, no console dispatch, no game claims. Prepared BEFORE the live
launch lands its verdict, so the branch-zero decision is instant.

Abbreviations: UI = `outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp`;
SV = `.../units/SeedValidation.hpp`; RE = `outputs/2026-10-04-zcode-team/worker-re-fname-pool/FNAME-POOL-RE.md`;
FV = `outputs/2026-10-04-zcode-team/worker-fname-ctor-audit/FNAME-VERDICT.md`;
EV = `outputs/2026-10-04-zcode-controller/launch10-evidence/pid700-fullfix/EVIDENCE.md`;
PV = `outputs/2026-10-04-zcode-team/worker-blocks0-provenance/PROVENANCE.md`;
RSB = `outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2/lifecycle/runtime_start_bridge.cpp`.

---

## 1. What verdict 0 means, exactly (code, not hope)

Emission site: `bridge_kernel_range_verdict` (RSB:805-833; declared UI:699).
It runs `mincore(page_align_down(addr), page_align_up(addr+len)-start, vec)` via
the generic `syscall` trampoline with `SYS_mincore`. Diagnostic codes (RSB:830-831):
**0 = ENOMEM over the range (some page has NO VMA), 1 = fully mapped, 2 = call
class refused, 3 = invalid request**. The probe is called ONCE per boot at
UI:1029 as `bridge_kernel_range_verdict(words.blocks0, kFNameBlockCapacityBytes)`
— i.e. over **[0x22C0000000, 0x22C0200000)** (blocks0 happens to be page-aligned;
0x20000 = 32 pages; cap 16 MiB OK).

On 0, `resolve_listing_gap` returns the refusal note "kernel range verdict: NOT
mapped over the required read range (genuine unmapped proof, stronger than the
listing refusal)" (SV:460-463) and the boot terminates via `seed_refused`
(UI:1040-1046) with that note in the status line.

**Verdict 0 therefore rests on three premises, and forces exactly one of them
to be false:**
- **P1 (read correctness):** the qword at pool+0x10 (runtime 0x9765ED0) is
  0x22C0000000.
- **P2 (semantic correctness):** that qword is the raw CPU pointer to block 0.
- **P3 (probe correctness):** mincore ENOMEM over [blocks0, blocks0+0x20000)
  ⇒ the game's block is not CPU-accessible there.

The game-side contradiction: if P1 and P2 hold, the ENGINE ITSELF dereferences
Blocks[0] on every FName operation (getter @0x243281 `mov rax,[rcx+rax*8+0x10]`
then `movzx r15d, word [rax+rcx*2]`, RE §2.1; find @0x23c0b0 reads [pool+0x10],
FV §4). FNamePool::Init runs at engine startup, long before our module_start
(+831 ms into an already-running game, EV §4). A dead 0x22C0000000 would have
crashed the game seconds before our probe ever ran. The game booted, ran, and
the buffered-Lua harness went green (EV §4/§7). So under branch zero, "the
kernel itself says nothing is mapped" cannot be taken at face value as "the
value is wrong" — it means **P1, P2, or P3 is false**, and the memo below
decides which, in order of evidence cost.

## 2. Field-by-field audit of the read path (offsets, widths, provenance)

Read pass: `read_pool_words(pool_runtime)` (UI:800-810) builds `PoolWords`
(UI:751-758) with byte-wise volatile LE loops `read_u32`/`read_u64` (UI:764-782).
Base: `pool_runtime = base + kFNamePoolLabel` (UI:927); `kFNamePoolLabel =
0x9365ec0` (SeedTable.h:12); base proven per-launch (MainExe view, UI:726-741)
and **independently pinned in-boot by GMalloc V1/V2 exact-match** — the FMalloc
instance address and its vptr read via the same `read_u64` primitive equaled
link-time-derived constants exactly (UI:905-917). Any base error would have
failed V1/V2 first.

Layout constants: `kPoolLayout{0x8, 0xC, 0x10, 0x14040, 0x14044, 16, 8192}`
(SV:72-73). Provenance per field:

| Field | Runtime addr (pool=0x9765EC0) | Width | Provenance (disassembly that FIXES it) |
|---|---|---|---|
| CurrentBlock | 0x9765EC8 (+0x8) | u32 | RE §2.3: Init `memset(pool+8,0,0x10008)` @0x23a5fd zeroes +8..+0x1000F; RE §2.4: Reserve reads `[rbx+8]` @0x2460a8, writes `[rdi+8]` @0x2460ea; FV §4: insert path reads CurrentByteOffset/Blocks from the same struct |
| CurrentByteOffset | 0x9765ECC (+0xC) | u32 | RE §2.4: `mov ebx,[rbx+0xC]` @0x2460ab, `[rdi+0xC]` stores @0x2460ed/0x246128 |
| Blocks[0] | 0x9765ED0 (+0x10) | u64 | RE §2.3: `mov [rbx+0x10], rax` @0x23a625 = Malloc(0x20000,2) result; RE §2.4: `[rdi+rcx*8+0x10]` @0x2460db/0x2460f4/0x246154; RE §2.1: getter `mov rax,[rcx+rax*8+0x10]` @0x243281; FV §4: find @0x23c0b0 reads [pool+0x10] and [pool+0x18] (Blocks[1]) |
| NoneId | 0x9779F00 (+0x14040) | u32 | RE §2.3: `lea r15,[rbx+0x14040]` @0x23a656; store "None" → `[rbx+0x14040]` @0x23a727 |
| BytePropertyId | 0x9779F04 (+0x14044) | u32 | RE §2.3 line 99: "ByteProperty" → +0x14044 |
| id encoding | block = id>>16, offset unit = id&0xFFFF, entry stride 2 | — | RE §2.1: `shr eax,0x10` @0x24327e, `movzx r14w` @0x243286, `word [rax+rcx*2]` @0x24328a; RE §1 layout verdict (stride 2, 16 block bits, 8192 blocks "all confirmed against code") |

All addresses were pinned by operand-level disassembly of THIS eboot
(sha 3b6cfad4…, the running title), not inferred from UE4SS version tables —
so alternative (b) "engine version layout differs" does not reach this chain
(see §4). All five addresses sit inside the in-game RW span
[0x953c000, 0x9a34000) prot 3 (FV §2 table, §5 item 5) — the span also
independently recorded as the kernel VMMAP record [0x970c000,0x9a34000) that
contains the pool struct (PV:26-29). **Premise correction to the task brief:**
the igf4 `gua_read` "96-byte read" was of the GUObjectArray (rt_addr 0x97ba110);
the span is the containing record's metadata. The pool words themselves come
from one `read_pool_words` pass (UI:994): two reads in page 0x9765E000
(+0x8/+0xC/+0x10 — current_block and blocks0 share ONE 4 KiB page) and two
reads 82 KiB away in page 0x9779F000, same span.

Observed live values (verbatim, EV:143, pid 700, plus the same values from the
two prior boots pids 645/672, EV:151): **CurrentBlock = 68, Blocks[0] =
0x22C0000000, published spans 64, envelope 0x1100400000, walk 10,070 entries
stop=0** (EV:47-92). Class: capture-truncated (blocks0 ≥ envelope), which is a
rescuable class — the fresh range probe fires on it (UI:1023-1035; SV:455-456).

## 3. Can blocks0 be wrong while the invariant AND 8-alignment hold?

**Arithmetically: yes — blocks0 is barely constrained.** The structural gate
`check_pool_shape` (SV:351-397) applies to blocks0 only: nonzero (SV:368-371)
and 8-aligned (SV:372-375). The engine invariant `none_id>>16 <= CurrentBlock`
(SV:363-367) constrains the (none_id, CurrentBlock) pair — it does not touch
blocks0. So for a uniformly random 64-bit value, P(pass) = 1/8 exactly
(alignment; nonzero adds a negligible +2^-64). **Any nonzero 8-aligned garbage
passes; 12.5% of arbitrary u64 values would.** The invariant itself is also
weak against garbage in general: with CurrentBlock a random u32 it passes with
probability ≈ 1 − 32768/2^32 ≈ 0.999992 (it only bites when CurrentBlock is
small, as observed).

**Mechanistically: no — no misread path exists.** What would a wrong read look
like per field, vs. what was observed?

| Field | Wrong-read signature | Observed | Discriminating power |
|---|---|---|---|
| CurrentBlock | garbage u32 (P(≤1000) ≈ 2.3e-7) or 0 from zeroed/pre-init BSS | 68, a small plausible open-block index; identical across 3 boots | Strong against base/offset error |
| Blocks[0] | any nonzero 8-aligned u64 (12.5% of garbage passes) | 0x22C0000000, 8-aligned, page-aligned, identical across 3 boots | Weak alone; the anomaly (139 GiB, 71 GiB above envelope) is what triggered this branch |
| NoneId / BytePropertyId | garbage u32; invariant usually still passes; nonzero check undemanding | not printed by any line in the chain — **their values have never been emitted**; the task brief's "nonzero" is currently UNPROVEN by archived evidence | Weak |

Why an isolated blocks0 misread has no mechanism:
1. **Same primitive, same pass.** `read_u64` is byte-identical in structure to
   `read_u32` (UI:764-782). A defect in the assembly loop corrupting only the
   +0x10 window and not +0x8 cannot be constructed from this code.
2. **Same page.** +0x8..+0x17 share one 4 KiB page (0x9765E000). Any
   base/slide/mapping-class error hits both CurrentBlock and Blocks[0];
   coherent-68-with-garbage-blocks0 is not a base-level failure mode.
3. **Independent end-to-end validation of read_u64 on target.** GMalloc V1/V2
   (UI:905-917) used read_u64 to match a derived BSS instance address and
   derived vtable address EXACTLY, in the same boot, minutes earlier in the
   ladder. The primitive demonstrably returns exact link-time constants.
4. **No tearing source.** Blocks[0] is written exactly once by Init
   (RE §2.3 @0x23a625) and never mutated thereafter (Reserve writes only
   newer slots, RE §2.4); no concurrent writer exists for a torn read.
5. **Cross-distance coherence.** none_id at +0x14040 (82 KiB away, different
   page, same span) satisfied `none_id>>16 <= 68` in the same pass. Under the
   garbage model the joint probability of the observed pattern
   (CurrentBlock=68 × none_id coherent × blocks0 8-aligned) is ≈ 2^-32 ×
   (69/2^16) × (1/8) ≈ 1.3e-13 per boot — and the values repeated across three
   independent boots (EV:151). A deterministic systematic misread could in
   principle repeat identically, but it would have to survive point 3's
   exact-match validation and point 2's shared-page geometry.
6. **CurrentBlock=68 is itself plausible**: 69 blocks × 64 Ki entries/block ≈
   4.5M interned names — high but normal for a shipping UE5 title; and the GUA
   (71,959 objects, FV §3) is consistent with a heavily-named game.

**Verdict: a blocks0 misread is arithmetically unconstrained (12.5%) but
mechanically unexplained; under branch zero the misread hypothesis is ranked
BELOW the probe-semantics hypothesis (see §4), because P1+P2 combined imply the
game could not run, and P1 has the corroborations above.**

## 4. Alternatives if 0 comes back — ranked, with evidence needs

**R1 — Probe semantics gap (P3 false): mincore lies for this address class.**
The standalone payload proved only the RETURN CONVENTION (raw positive ENOMEM,
errno==0; RSB:819-825, build prospero-5c2d3055) — on ordinary addresses. Its
behavior for a never-listed direct-memory-class region at 139 GiB is UNTESTED:
the kernel could return ENOMEM for such ranges despite a live VMA, or the
generic trampoline could misbehave on them. Note this is also the only
hypothesis consistent with P1+P2+the-game-runs (§1). Evidence needs:
(a) standalone mincore ladder — positive control (a listed heap span from the
frozen capture), negative control (a canonical unmapped gap), then blocks0,
then a ±2 MiB page-stepped boundary sweep around blocks0; (b) the durable
inference that the engine's own startup-long FName traffic through
Blocks[0] (RE §2.1 getter) proves CPU accessibility — recorded as inference,
never as a read.

**R2 — Misread of the value (P1 false): decided by one bounded re-dump.**
Despite §3, this is the branch the task brief expects, and one cheap diagnostic
kills or confirms it (see §5 A2): re-read the raw qwords through the same
discipline in a NEW distinct-ID revision and print them. If 0x22C0000000
repeats, P1 is closed for practical purposes and R1 escalates. If a DIFFERENT
value appears, stop everything: that is a read-path integrity event that would
also indict the GMalloc V1/V2 exact-matches from the same boot — reconcile
against those, never replay identical requests (AGENTS contract; UI:743-749).

**R3 — task's (a): tagged/relative pointer needing transformation (P2 false,
variant 1). CLOSED by disassembly.** RE §2.1 shows the engine's own getter
dereferences the raw qword with NO transform (`mov rax,[rcx+rax*8+0x10]` then
a direct `[rax+rcx*2]` entry read); RE §2.3/§2.4 show raw Malloc results stored
unmodified. FV §4 corroborates (find @0x23c0b0 reads [pool+0x10] directly).
Whatever transformation a pointer would need, the engine would need too — it
needs none. UE5-version folklore about offset-encoded blocks does not override
this build's own code. Reopen only if the §5 A2 dump ever shows two different
values for the same qword.

**R4 — task's (b): engine-version layout differs / EngineVersionOverride
(P2 false, variant 2). CLOSED for this chain.** EngineVersionOverride governs
UE4SS generic pattern scanning; this chain uses a per-binary derived seed table
(SeedTable.h guard `GENERATED_9765ec0_`) whose layout constants were verified
against THIS eboot's Init/Reserve/getter disassembly (RE §1 "all confirmed
against code", §2.3, §2.4; FV §4). Base provenance is doubly pinned (per-launch
view + GMalloc V1/V2 exact match, §2). Reopen only with new disassembly
evidence on a different binary than sha 3b6cfad4.

**R5 — task's (c): Blocks[] in a per-block pool with a different base
(P2 false, variant 3). CLOSED.** Each block is an individual
`FMemory::Malloc(0x20000, 2)` stored raw at Blocks[i] (RE §2.3 @0x23a613-25,
§2.4 @0x24613f-54); there is no base+scale indirection anywhere in the chain.
The allocator identity itself is bounded by V1/V2 (FMallocBinned3-family
instance + vtable, UI:905-917; RE §2.5).

Watch item (NOT branch-zero-blocking, flag for the controller): the success
gate at UI:1036 requires `none_id != 0`. In stock UE5 the first stored entry is
"None" at offset 0 of block 0, i.e. NoneId would be exactly 0 (and the RE entry-0
anchor at RE §4 item 6 says entry 0 of Blocks[0] IS "None"). If this build
matches stock, a healthy pool would spin to budget expiry at UI:1040-1042 and
refuse as "interned ids still zero" — a false refusal class that has never yet
executed (shape has never passed). Verify against the A2 dump's NoneId value
before the next shape-gated launch; do not change code without that evidence.

## 5. Exact next actions when verdict == 0 (bounded; in order)

- **A0. Record, never replay in-boot.** The probe already fired once
  (UI:1021-1033: `gap_probed_this_boot`); `pool_range_verdict_fresh=0` is the
  durable fresh-evidence event (RSB:831). The refusal terminates the launch
  (UI:1040-1046). No re-probe, no identical-request resubmission.

- **A1. New distinct-ID diagnostic revision: dump the raw struct qwords.**
  WHERE: UI gap block (UI:1020-1035), guarded by `!shape.ok && failure_class ∈
  {CaptureTruncated, NotCovered}` — the pool label is already classified
  data-readable there (UI:930-933), so four extra reads via the existing
  `read_u64` at pool offsets +0x0, +0x8, +0x10, +0x14040 (+0x14044 optional)
  stay inside the established data-side volatile-byte discipline and the
  already-verified readable span. **No read at blocks0 itself** — mapped-ness
  never authorizes reads (evidence contract, SV:410-429). Emit as new
  diagnostic tags (e.g. `pool_struct_qwords_boot<n>`) through the existing
  diagnostic_event channel, and include the values in the refusal status line
  (the same channel that carried EV:143). WHAT IT PROVES: P1 in one shot —
  value repeatability across boots; CurrentByteOffset sanity (<0x20000); and
  the NEVER-PRINTED NoneId/BytePropertyId actual values (resolves the §4 watch
  item with evidence, per the user's labeling law).

- **A2. First-page sub-verdict (same new revision, separate tag).** Call
  `bridge_kernel_range_verdict(words.blocks0, 0x1000)` once and emit as e.g.
  `pool_range_verdict_fresh_page0`. Splits R1 vs R2 decisively: page0 ENOMEM ⇒
  the address is dead as a pointer (R2 rises); page0 mapped + full-range
  ENOMEM ⇒ region geometry/semantics (R1 confirmed, block possibly real but
  the 0x20000 range not fully VMA-backed in kernel terms).

- **A3. If the A1 dump reproduces 0x22C0000000:** P1 closed → escalate R1:
  standalone-payload mincore ladder with positive/negative controls and the
  boundary sweep (§4 R1 evidence needs), plus the durable game-operates
  inference. P2 variants (R3-R5) stay closed unless that evidence reopens them.

- **A4. If the A1 dump shows a different value:** freeze the branch; treat as
  read-path integrity event; reconcile against GMalloc V1/V2 from the same
  boot; distinct-ID retry only with new derivation evidence (UI:743-749).

- **A5. Regardless of branch:** the 'None' entry-0 content anchor (UI:1050-1059)
  and all content checks stay downstream; nothing at blocks0 is read on the
  basis of any verdict; the frozen VMMAP capture is never edited with fresh
  results (SV:410-429).

## 6. One-line decision rule

If `pool_range_verdict_fresh == 0`: run A1+A2 in a new distinct-ID revision —
a repeated 0x22C0000000 plus a page0 ENOMEM splits the world into "probe
semantics at DM-class addresses" (R1) vs "dead pointer" (R2) within one bounded
diagnostic, while the RE-pinned layout (R3/R4/R5) stays closed.
