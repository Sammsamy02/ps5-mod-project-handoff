# COPYIN-SMOKE-PLAN — one-launch evidence plan for the staged write-copyin unit

worker-copyin-smoke · 2026-10-05 · PS5 UE4SS runtime port
Status: PLANNING/SYNTHESIS ONLY. The write-copyin unit is PROPOSAL-ONLY STAGING
(worker-copyin-unit; the current lineage candidate is under a live incident
investigation). This plan pre-registers how to read the FIRST composed launch
that carries the unit — that launch is itself the in-game smoke test the design
note demands. No console dispatch, no code edits, no game claims, no quality
scores are made or implied here.

## 0. Sources read (all read-only; staged bytes, not lineage)

- `outputs/2026-10-04-zcode-team/worker-copyin-unit/ADMISSION-NOTE.md`
- `.../worker-copyin-unit/staging/SeedValidation.hpp` (sha 444f2722…)
- `.../worker-copyin-unit/staging/UnrealInitializer.cpp` (sha d0dcf411… — "the staged initializer d0dcf411")
- `.../worker-copyin-unit/staging/runtime_start_bridge_r14_diag_spanpub.cpp` (sha 4f2a1408…)
- `.../worker-copyin-unit/host-test/test_listing_gap_copyin.cpp` + `host-test-run.log` (ALL CHECKS PASSED; admission note §5: 27/27)
- `.../worker-copyin-unit/provenance/SHA256SUMS.txt`
- `.../worker-write-copyin/results/verdict-and-design-note.json` (standalone campaign evidence)
- `.../worker-blocks0-misread/DECISION-MEMO-BRANCH-ZERO.md` (branch-zero memo, R1/R2, ids-zero watch item)
- `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/generated/probe_config.h` (PROBE_LOG_PATH pin)

Abbreviations below: UI = staged UnrealInitializer.cpp (d0dcf411), BR = staged
bridge (4f2a1408), SV = staged SeedValidation.hpp (444f2722). Probed range is
always `[words.blocks0, words.blocks0 + 0x20000)` (kFNameBlockCapacityBytes,
32 pages).

---

## 1. The evidence cascade of one launch, and every combination mapped

### 1.1 The rungs, in emission order

The copyin probe can only fire inside the listing-gap branch (see §4/R-1), and
only AFTER the range verdict returned Mapped (UI:1074-1080; ordering rationale
SV:488-494 — mapped-ness is the zero-target-byte-touch oracle; the copyin probe
moves real bytes, so it must not fire on a kernel-refused range). Each failed
run of the ladder still yields durable evidence through the append-only
diagnostic record; each rung's value narrows the world:

1. **Range verdict value** — tag `pool_range_verdict_fresh` (BR:831), same
   int codes as `rc_seed::RangeMappedVerdict` (SV:478-483): `-1` unavailable
   (call class refused), `0` NotMapped (mincore ENOMEM over the range),
   `1` Mapped (0 over the range), `3` invalid request (impossible from this
   call site: len is the constant 0x20000; seeing 3 is a code-drift integrity
   event).
2. **Copyin code** — tag `pool_copyin_probe_fresh` (BR:1046): `1` READABLE /
   `0` REFUSED / `2` UNAVAILABLE / `3` invalid request. Absent entirely when
   the probe never ran (range≠Mapped, or the gap branch never executed — §3).
3. **rc/errno detail** — tags `pool_copyin_probe_rc` (raw write(2) rc of the
   LAST chunk, 0xFFFFFFFF encodes -1) and `pool_copyin_probe_errno` (errno
   captured after that write, before any further syscall; 0 under the
   raw-positive trampoline convention) (BR:1047-1048, convention law
   BR:870-878). These are descriptive only — classification never rests on rc
   shape alone (the corrected-table defect fix; sink bytes are the
   disambiguator).
4. **Sink size** — tag `pool_copyin_probe_sink` = `landed`, the write-fd
   lseek(SEEK_END) truth after the last chunk (BR:1049). Expected value on
   READABLE: `0x20000` = 131072. Separately, the anchor consumes the CAPTURED
   first page (max 0x1000, BR:1030-1041) via
   `bridge_write_copyin_sink_bytes`; the anchor's own floor is
   `2 + sizeof("None") = 6` bytes (UI:1127-1128). "sink ≥ 6" in admission
   terms refers to the capture; the file/`landed` truth on full transfer is
   0x20000. Double verification gates READABLE: write-fd lseek END == len
   after every chunk AND an independent read-fd reopen lseek END == len
   (BR:999-1024; the vacuity law — host rows R3/R3b/R10).
5. **Downstream witnesses** — either a loud refusal naming the resolution note
   (UI:1097-1101), or the success line naming real provenance
   "admitted via kernel-write-copyin-read (listing-gap owned-read probe; 'None'
   anchor verified from the sink bytes)" (UI:1174-1179).

### 1.2 Combination → conclusion map

| # | range | copyin | rc/errno detail | sink size | Conclusion (each row cites the unit's classification table) |
|---|-------|--------|-----------------|-----------|-------------------------------------------------------------|
| A | 1 | 1 | rc==len-class chunk rc, errno 0 or wrapper 0 | 0x20000 (capture ≥ 6) | **Pool ADMITTED via copyin** AND the anchor ran on sink bytes → next rungs unlock (Phase 4 ctor bind, behavioral probe, downstream conversion under bounds_source "kernel-write-copyin-read"). The in-game source-class-allowlist hypothesis is CONFIRMED for this address class as far as this probe shows (SV:526-541 admission branch; host rows R1/R1b/R2/R10). Sub-case A′ below. |
| A′ | 1 | 1 | as A | 0x20000 | **but interned ids still zero** (see the ids gate, UI:1090 — admission additionally requires `none_id != 0 && byte_property_id != 0`): the loop spins to budget and refuses with the CaptureTruncated shape failure PLUS the ADMITTED gap-note embedded (UI:1097-1101 embeds `gap_note`). That bizarre signature = the branch-zero memo §4 ids-zero watch item realized (this build's NoneId may be stock-0; its value has never been emitted). Copyin evidence still lands; admission mechanism proven; conversion blocked on the ids gate — a distinct-ID follow-up, never a replay. |
| B | 1 | 0 | -1/errno 14 (EFAULT) or raw 14; sink empty (0) | 0 | **DM-class address is NOT in the in-game copyin allowlist → the honest-refusal world persists, branch-zero memo's R1 confirmed at the read level.** Precisely: range=1 disconfirms R1's mincore-lies-ENOMEM form (the kernel answered "mapped" for the DM-class range — memo §4 R1 premise P3 holds at the mapped-ness level), while copyin=0 EFAULT confirms R1's deeper content: this address class gets different kernel treatment than ordinary regions, now demonstrated on the READ path — exactly the ADDRESS-CLASS CAVEAT (SV:454-465: EFAULT ≠ unmapped; CPU-readable mmap-anon EFAULTed twice standalone; a refused verdict is honest "not kernel-copyin-readable", never a CPU-readability claim either direction). Host row R4/R5. |
| C | 1 | 0 | any other rc (e.g. raw EIO 5 < chunk) | 0 | Copyin refusal, rc-ambiguous by design — sink-empty truth disambiguates; same refusal world as B without the EFAULT label (host row R6; BR:970-984 "otherwise → 0"). |
| D | 1 | 2 | rc==-1/errno==EPERM or raw EPERM(1) | whatever landed (all-or-nothing ⇒ 0) | **Policy abort class**: the kernel actively forbids the operation in-game (sibling of the prior ptrace EPERM wall; never observed on the write route standalone). UNAVAILABLE, honest refusal, no per-address retry by design (BR:962-977; host rows R7/R7b). New in-game policy evidence; a policy world, not an allowlist gap. |
| E | 1 | 2 | rc == full-chunk success rc, nothing retained | 0 | **Vacuous-sink oracle anomaly in-game** (/dev/null-class): never admitted, never claimed as a source refusal (BR:966-980; host rows R3/R3b). Would be a new kernel-behavior datum. |
| F | 1 | 2 | partial transfer (sink grew but != len) — never observed standalone | 0 < size < 0x20000 | **Partial honestly unavailable** — recorded, never admitted (BR:987-994; host row R8). Also catches a raw lseek-error masquerade (host row R9). |
| G | 1 | 2 | open rc<0 + errno captured (BR:932-937) | no sink created | **Sink-open failure** (/data not writable in-game): environmental, not semantics; the probe never touched target bytes. Fix is environmental in a NEW revision; no semantics conclusion. |
| H | 1 | −(no copyin tags at all) | — | — | The copyin probe never ran despite a gap boot: either the one-shot was consumed earlier by a non-Mapped first miss (timeline B, §3) or the copyin call path did not execute as staged. `pool_range_verdict_fresh` present + `pool_copyin_probe_*` absent is the discriminator. Evidence-interpretation anomaly to report, not a semantics result. |
| I | 0 | (not fired) | — | — | **Branch-zero dead-pointer branch**: mincore ENOMEM over the range = "genuine unmapped proof, stronger than the listing refusal" (SV:514-517; memo §1/§6). Per the memo: world splits into R1 (probe semantics) vs R2 (dead pointer); the memo's A1/A2 bounded diagnostics (struct-qword dump + first-page sub-verdict in a NEW distinct-ID revision) decide — never an in-boot replay (memo §5 A0). |
| J | -1 | (not fired) | — | — | Range verdict unavailable (call class refused): "listing gap unresolved — refuse honestly" (SV:518-521). mincore-semantics question stays open; branch-zero R1 untested this boot. |
| K | 3 | (not fired) | — | — | Impossible from this call site (len constant 0x20000 ≤ 16 MiB cap): code-drift integrity event; stop and reconcile against the staged bytes, never reinterpret. |
| L | (no range tag) | (no tags) | — | — | The gap branch never executed: span-membership admitted on iteration 1 (shape.ok; probe unreachable, §4/R-1), or the run refused/refused-earlier before Phase 3. A finally-working module load with ZERO copyin surface — the expected shape of every boot that does not hit the listing gap. |

Standalone-campaign anchors for these rows (design note, per_case_raw_results):
image BSS / stack / malloc-heap sources transferred byte-true (host rows R1/R2);
munmap hole, PROT_NONE, DM-class 0x22C0000000 and high unmapped all EFAULTed
byte-empty; /dev/null vacuous rc==len. In-game rows A–G are the hypothesis the
smoke exists to settle; standalone results do NOT preview them.

---

## 2. Post-launch host verification (READ-ONLY checklist)

Cleanup law (BR:880-884, 1042-1045): the sink is a fixed bridge-owned path
`/data/modbridge-copyin-sink.bin`; **unlink-first** clears any stale leaked
sink at the start of every probe (BR:928), and **close + unlink run on EVERY
path once the sink exists, including every refusal path** ("an EFAULT leaves 0
bytes but the file exists"). The only probe exit that never creates the sink is
the invalid-request class (BR:918-921: no syscalls, no sink — unreachable from
this call site per row K). Sink bytes are captured BEFORE unlink; none are
trusted after.

Checklist (all read-only; report findings, never delete/truncate/chmod — any
cleanup is a separate authorized action):

1. EXPECTED: `/data/modbridge-copyin-sink.bin` is ABSENT after the launch. Its
   absence is the compliance signal, not a failure.
2. VIOLATION to report: a sink file LINGERING on console post-launch. That is
   a cleanup-law violation and also a crash-window timestamp: the only way to
   leak is an interruption between sink-open (BR:929) and the unconditional
   cleanup (BR:1044-1045) — i.e. the boot died inside the probe. Record
   existence, size, and mtime read-only; compare size to `pool_copyin_probe_sink`:
   equal ⇒ leak at/after the final chunk; 0 bytes ⇒ leak before the first
   chunk landed. Do not remove it in the verification pass.
3. Pre-launch state SHOULD also be recorded (one read-only stat): a stale sink
   surviving from an earlier crashed probe is consumed silently by unlink-first
   at the next probe, so post-launch absence is only attributable if the
   pre-launch state is known.
4. The append-only diagnostic record is EXPECTED to persist and grow — it is
   provenance, not a leak. In the composed lineage its path is pinned by
   `PROBE_LOG_PATH` = `/data/townfall_runtime_aligned_entry.log`
   (native-full-runtime-aligned-entry-1/generated/probe_config.h; the
   controller's current composition owns the exact path). Read-only grep for:
   `pool_range_verdict_fresh`, `pool_copyin_probe_fresh`, `pool_copyin_probe_rc`,
   `pool_copyin_probe_errno`, `pool_copyin_probe_sink` — presence/absence and
   values are the raw cascade of §1.
5. Do not confuse strays: the standalone campaign's `/data/modbridge-copyin.jsonl`
   (payload provenance, earlier separate-process runs) is a DIFFERENT, expected
   file. Only `modbridge-copyin-sink.bin` is the live sink.

---

## 3. Interaction audit — the once-per-boot `gap_probed` static vs the wait loop's retry budget

Staged facts (UI d0dcf411):

- Four boot-lifetime statics: `gap_probed_this_boot`, `gap_note_cached`
  (UI:1046-1047), `gap_admitted_via_copyin`, `gap_admitted_blocks0`
  (UI:1054-1055).
- Fire condition: the FIRST loop iteration with `!shape.ok` — of ANY failure
  class. The statics do NOT gate on class (UI:1056-1058). On that iteration the
  range verdict is called unconditionally on that iteration's `words.blocks0`
  (UI:1060-1061), the copyin probe ONLY if the verdict is Mapped
  (UI:1074-1080), then `resolve_listing_gap` decides and the statics latch
  (UI:1081-1085). For Structural / SpanTableUnavailable classes the resolution
  refuses regardless of the verdicts — "structural checks gate everything"
  (SV:509-512) — but the one-shot is consumed all the same.
- After the fire, every retry (50 ms cadence, UI:1102) re-reads the pool words
  and re-runs the pure shape check, but reuses the cached note and the cached,
  address-pinned admission (`words.blocks0 == gap_admitted_blocks0`, UI:1089).
  `CheckStartupWait()` still runs each iteration (UI:1018). The loop's only
  time bound is the budget handed in at UI:1318 —
  `UnrealConfig.SecondsToScanBeforeGivingUp` (the seeds path explicitly
  "retries inside the same budget" as the PS scan it replaces).

**Expected single-fire behavior: the probe (range verdict + conditional
copyin) fires AT MOST ONCE per boot, at the first shape failure, whatever its
class; the retry budget can never re-arm it; and one fire costs one bounded
syscall burst (1 mincore over ≤ 32 pages + at most one sink open/unlink/close
cycle + ≤ 32 page-clipped writes + ≤ 33 lseeks + ≤ 1 read-back) — there is no
per-retry multiplication of fresh-evidence events or sink churn.** This is the
same discipline the range verdict already used ("the mapping is static;
re-probing every retry would multiply fresh-evidence events without new
information", UI:1042-1044 comment).

The one real interaction, for evidence reading — two boot timelines:

- **Timeline A (pool already live at the first loop iteration):** the first
  miss IS the DM-class listing gap (CaptureTruncated) → range verdict probes
  0x22C0000000-class → if Mapped, the copyin probe fires once → the full
  §1 cascade lands in the record. Tag signature: `pool_range_verdict_fresh`
  AND `pool_copyin_probe_*` both present.
- **Timeline B (pool not yet initialized at the first iteration):** pre-init
  words give `blocks0 == 0` → Structural class → the one-shot fires the range
  verdict on address 0 (expected NotMapped; `pool_range_verdict_fresh`=0), the
  copyin probe does NOT fire (only on Mapped), the resolution refuses as
  non-rescuable, and the statics latch. When the pool later goes live into the
  real DM-class gap, no re-probe occurs; the loop burns to the budget and
  refuses with the CURRENT shape failure plus the STALE cached note — so the
  message can pair "snapshot truncated …" with a resolution note about
  structural non-rescue. Tag signature: `pool_range_verdict_fresh` present,
  `pool_copyin_probe_*` ABSENT. That signature (row H) means "no copyin
  evidence this boot; the one-shot was spent pre-init" — an evidence-loss
  finding about the boot timeline, NOT a copyin refusal, and never to be read
  as one. The stale-note pairing inside one refusal line is likewise a known
  artifact of the latch, not a contradiction.

Which timeline holds in-game is unknown until the first launch; the plan's
discriminator is the tag signature above, and the refusal message's embedded
resolution note corroborates it.

---

## 4. Risk register — what could make the copyin probe destabilize a finally-working module load

- **R-1 (guard, code-confirmed): span-membership boots have ZERO probe
  surface.** The entire probe block sits inside `if (!shape.ok) { if
  (!gap_probed_this_boot) { … } }` (UI:1056-1088); a boot whose first
  iteration admits by span membership breaks out of the loop
  (UI:1089-1094) before any probe line executes — no sink, no new syscalls, no
  new diagnostics. The probe additionally cannot carry admission unless shape
  failed on every iteration (`pool_admitted_via_copyin = !shape.ok`,
  UI:1092). Confirmed from the staged code as the task asked: if the pool
  ADMITS via span membership, the probe never runs. Destabilization risk is
  therefore confined to boots that would otherwise REFUSE (evidence-only loss)
  or gap-ADMIT (new world) — never to a working span-membership load.
- **R-2 Load-time linkage risk: none added.** The probe reaches the kernel
  through the same generic `::syscall` trampoline as SYS_mincore with
  header-only SYS_*/O_* constants — expected added undefined imports = ∅
  (admission note §6.6); the fixed edition's undefined-delta gate refuses
  anything else. The risk is run-time, inside the gap branch only, not at
  module load.
- **R-3 The probe's own syscall surface.** Stock vnode write path over the
  probe's OWN file, ≤ 0x20000 bytes, once per boot, page-granular chunks; the
  target address is only ever a SOURCE pointer bit-pattern (BR:939 — "NOT
  dereferenced CPU-side: write(2) carries it"); no game memory is written and
  no game file is touched. Failure classes (sink-open fail, EFAULT, partial,
  vacuous, EPERM) all classify and clean up (BR:932-994, 1042-1045); EPERM
  aborts without retry, so no policy-escalation loop. Residual: an in-boot
  crash INSIDE the probe window leaks the sink (§2 item 2) — detectable, and
  it would already be a module-fatal event, not a silent corruption.
- **R-4 The copyin-ADMITTED path's downstream direct reads (main residual
  vector).** On admission the anchor safely reads SINK bytes (UI:1123-1135),
  but `validate_entry` → `pool_entry_bytes` then reads the 'None' /
  'ByteProperty' entry bytes DIRECTLY at the Blocks[] pointers — i.e. inside
  the DM-class block (UI:1153-1169, 834-850) — covered by the admitted
  bounds_source, and everything after Phase 3 proceeds on live reads. The
  implication "kernel-copyin-readable ⇒ CPU-faultable-readable" is
  standalone-evidenced in ONE direction only (the standalone caveat is that
  copyin is STRICTER: CPU-readable mmap-anon still EFAULTed) and is exactly
  the in-game hypothesis. If it fails in-game, the first direct entry read
  after a copyin=1 + anchor-pass signature is the crash point — which is
  itself decisive, novel evidence (a kernel-copyin-readable but not
  CPU-faultable class), at the cost of that boot. This is the accepted,
  honest reason the FIRST carrying launch is the smoke test.
- **R-5 ids-zero watch item (branch-zero memo §4).** Even a READABLE copyin
  does not convert to admission unless `none_id != 0 && byte_property_id != 0`
  (UI:1090); if this build's stored NoneId is stock-0, the boot ends in the
  A′ signature (budget refusal with the ADMITTED note embedded). Not a
  stability risk (it refuses), but it can masquerade as a copyin contradiction
  to a careless reader — the §1 A′ row and the tag values resolve it.
- **R-6 Retry-budget interaction (§3).** Worst case is Timeline B: no copyin
  evidence and a stale-note refusal. Not destabilizing (refuses loudly), but
  it can silently zero the launch's copyin yield; the pre-launch record and
  the tag signature (row H) keep it attributable.
- **R-7 /data environment.** A full or read-only /data degrades to rows G/F
  classes (honest unavailable/refusal) — never to a crash; sink growth is
  capped by the 16 MiB-class per-call budget and here by the 0x20000 range.

---

## 5. Bounded stop

This plan is the deliverable. No dispatch, no composition, no lineage edits,
no console or game actions are authorized or performed by this worker. The
next actions belong to the controller per the admission note's §6 pin cascade
and the COMPARATOR-REGEN-PROCEDURE, only after the live incident investigation
closes.
