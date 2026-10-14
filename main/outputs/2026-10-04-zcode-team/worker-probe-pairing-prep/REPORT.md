# REPORT — worker-probe-pairing-prep (2026-10-05)

Status: **DELIVERED (proposal-only)** — the paired staging design, evidence
contract, and controller runbook for ONE future user-coordinated probe window
carrying BOTH staged-but-never-injected probes. LOCAL ONLY: zero console
contact, zero launches, zero ini writes. Claim written after the analysis;
this report last.

## Task 1 — fc1pool artifacts located (all constants verified on disk)

- Probe: `modbridge_fc1pool.prx`, 22733 B, sha256 `d2431bf8…`, build
  `fc1pool1-78508a2e2940` — `outputs/2026-10-04-zcode-team/worker-fc1-validator-probe/`
  (deterministic double-build run1==run2; bare-sdk 69408 B `54f8335e…`).
- Ini entry: `/data/plugins/modbridge_fc1pool.prx = 2` — canonical entry 2
  (FINAL-INI-LAYOUT), content-anchored AFTER the xwp line.
- Output channel: `/data/modbridge-fc1pool-facts.jsonl` — OWN single-writer
  channel (v3 separator-correct writer, one sceKernelWrite per <1 KiB line,
  `O_APPEND|O_CREAT|O_NOFOLLOW`), envelope `{"marker":"modbridge.ingamefacts",
  "schema":1,"build_id":"fc1pool1-78508a2e2940",…}` + exactly ONE
  `FC1POOL: …` line per launch in `/data/plugins/UE4SS.log`.
- Capture fields (driver `fc1pool_driver.c:581-590`): `blocks0` (ig_kv_hex,
  `"0x"+16 lowercase hex`), `blocks0_canonical`, `current_block`,
  `current_byte_offset`, `id_none`, `id_none_is_zero_expected`,
  `id_byteproperty`, `byteprop_id_for_find_crosscheck`; per-entry raw evidence
  (bytes16 hex, entry/blockptr addresses) in the `fc1pool_entries` record.
- Runtime chain: GUA anchor `0x97ba110` admission → base = GUA−0x93ba110
  (expected 0x400000) → pool `0x9765ec0` → CurrentBlock@+0x8 / Blocks[0]@+0x10
  / bootstrap ids@+0x14040/+0x14044 → entry chase + ASCII "None"/
  "ByteProperty". 11 checks, fail-open with bounded 8×1 s grace.
- Cross-check: readback constants (67848 B, body `7e2dce27…`) recomputed this
  session with the EXACT `console_batch.py` `sht_prefix_end` law — MATCH.

## Task 2 — paired ini designed; Option P recommended, Option A recorded fallback

- **Option P (recommended, NEW pin proposal): 3-entry probe-only ini** —
  header + xwp `= 2` + fc1pool `= 2` + path_test `= 2`; **149 B, sha256
  `d6770f44f5908e9188e981175b4797917f524c0404a8c75cb92f069312084598`**
  (host-computed TARGET). Matches the window's probes-only purpose; 2 uploads,
  3 presence proofs; keeps the path_test traversal terminator. NOT law-ready:
  the final verifier correctly rejects it today; adoption = the layout owner's
  NEW pinned-bytes decision under a NEW id (ninth-slot precedent). This worker
  did NOT pin or write anything.
- **Option A (law-ready fallback): the preserved 9-entry canonical
  `fd9c7393…`** (FINAL-INI-OK, re-verified byte-identical on disk) already
  enables BOTH probes at entries 1–2 — but it is NOT probes-only: the
  composition runs at entry 3, and 4 of its 8 enabled files are missing on
  console since launch-9 (fc1pool, qh_driver, observer, carrier — the
  incident that created files-first-config-last), so it costs 5 uploads +
  8 presence proofs + ridealong thread noise.
- Justification for the split decision is in PAIRED-STAGING.md §1 (window
  purpose, blast radius, ladder risk, measurement cleanliness); the pinned-
  bytes law is honored by PROPOSING bytes for the owner to pin, never by
  staging unpinned bytes or editing pins in place.

## Task 3 — sequencing: ONE launch is safe

No interference found, mechanism by mechanism (PAIRED-STAGING.md §2): xwp's
one-shot `module_start` fully completes (mprotect 0x7878000 X→RX, read test,
ALWAYS restore, post-restore re-proof) before fc1pool's `module_start` begins
(sequential file-order injection); the flipped page overlaps zero fc1pool
read targets; the flip is a permission WIDENING (adds R) so even a failed
restore cannot block a read; disjoint channels, no shared state, append-only
time-ordered UE4SS.log writers, worst case ≈9.6 s ≪ 25 s/120 s caps.
Ordering contract: xwp line BEFORE fc1pool line in any option. Two-launch
spec exists only as reconcile-first contingency, never a default.

## Task 4 — evidence contract delivered

EVIDENCE-CONTRACT.md pins the exact expected lines: the 19-record xwp chain
(entry/startup/row/vmmap_persist/attempt/rw_test/restore/post_restore +
5×skip pairs + summary; grammar corroborated by launch-9's REAL 15-record
`xwp-jsonl-final.jsonl`) with the W_WORKS / W_FAILS_R_WORKS / fail-closed /
ABSENT / FATAL classification table; the 6-record fc1pool chain with the
PASS `fc1pool_result` line carrying the FIRST REAL `blocks0` u64, all
hard/grace refusal codes, and the exact `FC1POOL:` UE4SS line. Downstream
consequences pinned: L11 matrix D2/D3 inputs; candidate/synthetic label
resolved by where blocks0 lands; validator v4's R1/R2/R3 predicates get
their first real values before launch-12 arms it;
`byteprop_id_for_find_crosscheck` pinned for the later behavioral crosscheck.

## Task 5 — runbook delivered

PAIRED-STAGING.md §3: gate (launch-10 window closed; Step-0 RETR expecting
`1216aab6…`; `.bak` under fresh TAG; Option P pin ON RECORD before staging)
→ files first (xwp REBUILD + fc1pool uploads, SHT-strip readback law, one
bounded re-STOR each) → config last (pre-publish presence guard over every
enabled entry; verbatim ini STOR; byte-exact RETR diff) → user launches
manually → fetch + restore. Restore paths: one bounded `.bak` restore on any
staging failure; MANDATORY end-of-window restore of the `1216aab6…` baseline
so launch-10's frozen pre/post state stays undisturbed; owned evidence jsonl
never deleted; uploaded .prx may stay (unreferenced, launch-9 precedent) with
presence recorded.

## Load-bearing finding (hazard H-1)

`console_batch.py`'s `XWP_ARMED` constant still pins the LAUNCH-9 build
(`igd1-6ebab07f2111`, fself `48949316…`). A batch flow staging "xwp" as-is
would upload the OLD bytes under the SAME console filename and self-verify
them PASS — reproducing launch-9's all-skip evidence instead of the row-0
verdict. The runbook requires either a batch-tool-owner constants update
under NEW ids or explicit controller staging from
`worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx`, with the
staged build identity checked against the seq-1 entry record's `build_id`
(must read `igd1-e030a09eb4d1`).

## Honest limits

- PROPOSAL ONLY: every console step is the controller's; the launch is
  user-coordinated and must never overlap launch-10's window.
- Zero console contact; every console constant is [TARGET] until the Step-0
  RETR proves it.
- Neither probe has ever executed on target in this configuration (the xwp
  rebuild: never executed; fc1pool: never injected). All record grammars are
  pinned machinery + host tests + launch-9's real xwp corroboration — not
  console measurements for this pairing.
- No outcome predicted: W_WORKS, W_FAILS_R_WORKS, every refusal class, and
  every blocks0 placement are specified with pre-agreed consequences; none
  is promised.
- Option P is NOT lawful until the layout owner pins it; until then Option A
  is the only lawful path (with its stated costs).
- The interference analysis is source-level; actual co-resident execution is
  a target-only fact the window itself will produce.
- No files outside this worker directory were created or modified; the
  Option P byte computation used /tmp only.
