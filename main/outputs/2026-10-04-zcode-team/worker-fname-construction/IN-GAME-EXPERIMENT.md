# IN-GAME-EXPERIMENT.md — minimal experiment ladder that closes the FName CONSTRUCTION gap

worker-fname-construction, 2026-10-04-zcode-team. **DESIGN ONLY — nothing in
this document was built, executed, or staged.** Facts cite CENSUS.md entries
(A-n/B-n/C-n/D-n/E-n).

Hard rules inherited from the siblings (E-5, E-6): classifier-only means
READ-ONLY in-process observation; **NO engine function is called until its
address is validated** (and the only experiment that calls anything is FC-3,
last in the ladder); no guessed offsets or widths (D-1); absence of a record is
a result, never a retry trigger; the game launch is USER-COORDINATED; records
are JSONL lines in the worker-ingame-probe envelope shape
`{"marker":…,"schema":1,"build_id":…,"seq":N,"exp":…,"rc":R,"errno":E,"fields":{…}}`
(new `exp` ids `fc1`/`fc2`/`fc3`, one JSONL file per probe build). Refusals use
`rc` refusal codes and PRESERVE partial state verbatim.

Vehicle: a sibling-pattern PRX (worker-ingame-probe STAGE-PLAN shape —
`/data/plugins/<module>.prx`, ploader.ini mode 2, user launches Townfall,
records retrieved from `/data/<module>.jsonl`). Fresh `build_id`; first run
ALONE (no UE4SS module loaded concurrently — the retained program's own
`VerifyFNameConstructor` hook (C-5) must never coexist with FC-3's probe; a
shared detour would corrupt both results).

Preconditions shared by ALL experiments (any unmet -> the record says
`refused`, `fields.refusal_reason` names it, and NOTHING is read beyond the
span table):

- P-i: worker-1 live-module facts produced the eboot anchor image
  `image_base` + readable spans (ingame-probe G1/G4/G2 outcomes), with
  `base_provenance` recorded. Missing -> emit `needs-base` (E-5) and stop.
- P-ii: image identity — capture sha256 `3b6cfad4…22fc` (worker-2 REPORT §2)
  matches the live mapping's module identity fact. Mismatch -> `refused`
  (a different build invalidates every vaddr in this design).
- P-iii: every read is span-bounded (`tlp::MemSpan` discipline, E-5); refused
  dereferences are COUNTED in `fields.validation_blocked_derefs` (E-6), never
  forced.

---

## FC-1 (a) — validate the `0x23d590` call-site candidate in-process (READ-ONLY, lowest risk)

**Question:** is `image_base + 0x23d590` a readable, sanely-shaped function
entry, and do the anchor call-sites that produced the candidate still encode
to exactly that target when relocated? (Candidate provenance: A-3, B-4.)

**Preconditions:** P-i, P-ii, P-iii. No game state dependency (any time after
module load; the region is engine code and already resident).

**Steps (pure classifier, no calls, no writes):**

1. Resolve `target = image_base + 0x23d590` (file-image vaddr A-3 + proven base).
2. `probe_code_shape(spans, n, image_base, 0x23d590, nullptr)` — the SAME
   vendored classifier G6 already runs for Tick/StaticConstructObject (E-5,
   E-6): executable-span containment, valid x86-64 decode from the first byte,
   bounded window, entry-prologue plausibility. Record verdict + rcode.
3. Optional strengthening (only if the controller first materializes
   `calls_by_anchor` by a bounded OFFLINE host re-run of the receipt-pinned
   resolver — census note under A-3): for each anchor call-site vaddr `c_i`,
   decode the 5 bytes at `image_base + c_i`; require `E8 rel32` whose
   relocated target equals `target`. Count matches/misses.

**Pass (exact):** FC-1 verdict `Validated` iff step 2 validates the code shape
AND (if step 3 ran) all five call-sites' rel32 targets equal `target` exactly.
Code-shape alone caps the verdict at `Inconclusive` ("readable plausible code,
call-target provenance not re-proven") — it can never `Validate` the
call-target claim by itself.

**Fail/refuse:** decode invalid or outside executable span -> `Refuted`
(candidate address is not code; the whole ladder stops and the gap stays
open with the refutation recorded). Any step-3 mismatch (a relocated
call-site encodes elsewhere) -> `Refuted` (the 0x23d590 intersection no
longer holds in the live image) and STOP — do not rescan for a "better"
candidate inside the probe. Preconditions unmet -> `refused`/`needs-base`.
No retries: one classifier pass per launch.

**Why first:** zero risk (reads only), decides whether the ladder continues,
and its evidence is reusable by every later consumer as the
`FNameConstructorAddressValidated` token input (see ADAPTER-INTERFACE.hpp).

## FC-2 (b) — FNamePool/GNames anchors via the validated GUObjectArray (READ-ONLY, medium)

**Question:** do live object headers corroborate the candidate member layouts,
and is any name/pool anchor lawfully observable from them? (A-4; D-2; E-1/E-4.)

**Preconditions:** P-i, P-ii, P-iii; AND G6's `gua_verdict` from the sibling
probe is `Validated` for exactly one of layouts A/B (E-5, worker-5 REPORT).
If G6 is `Inconclusive`/`Refuted`/absent, FC-2 runs its DEGRADED subset only
(item-shape invariants, below) and every layout-dependent sub-verdict stays
`NotProbed` — it may not guess A-vs-B.

**Steps (all span-bounded reads; names are RAW BYTES, E-4):**

1. From validated `GUObjectArray = image_base + 0x93ba110` (A-4) read
   `NumElements`/chunk table per the winning layout hypothesis (bounded:
   `MAX_CHUNK_SCAN=64`, `NUM_ELEMENTS_PER_CHUNK=64*1024`, `FUOBJECTITEM_SIZE
   0x18` — sibling constants, min_predicates.py).
2. Sample the first `K<=16` items in one validated chunk. Per item: `Object`
   pointer must be canonical-or-null (sibling predicate), else that item is
   skipped (counted, never fatal).
3. UObjectBase hypothesis check at candidate offsets (D-2: widths
   compiled-port; offsets candidate): read `ObjectFlags@0x8`(u32),
   `InternalIndex_Private@0xC`(u32), `ClassPrivate@0x10`,
   `NamePrivate@0x18` (8 raw bytes), `OuterPrivate@0x20`. Structural
   invariants ONLY: `InternalIndex_Private < NumElements`;
   `ClassPrivate`/`OuterPrivate` canonical-or-null; NO interpretation of the
   NamePrivate bytes (E-4).
4. **Pool-anchor sub-experiment (only if FC-1 `Validated`):** bounded
   disassembly of the validated ctor window at `image_base + 0x23d590`
   (e.g. first 0x400 bytes, still span-bounded): record RIP-relative
   data-section references as `pool_reference_candidates` — label, offset,
   nothing else. These are **candidate-only** anchors for a FUTURE dedicated
   predicate; FC-2 never dereferences them as a pool (P2 rule, E-1; D-3).

**Pass (exact):** FC-2 `Validated` iff G6-validated layout + >=12 of 16
sampled items pass every invariant. Then the census fact "UObjectBase offset
set corroborated in-process" becomes available (upgrades the D-2 offsets from
candidate toward staged-layout input via the normal worker-5
publish pipeline). Pool anchors can NEVER be Validated by FC-2 — best case
they enter the census as new `candidate-only` entries.

**Fail/refuse:** layout A/B both unvalidated -> degraded subset only
(`Inconclusive`); invariant violations on >4 of 16 items -> `Refuted` for the
offset hypothesis (recorded; hypothesis B consumers blocked; NO alternative
offsets are tried in-process). Preconditions unmet -> `refused`.

**Honest ceiling:** FC-2 cannot produce a GNames/FNamePool anchor because the
P2 rule forbids pool heuristics (E-1) and no width exists (D-1). Its real
deliverables are (i) layout corroboration and (ii) ctor-internal pool
REFERENCE candidates for a later, separately-designed endpoint.

## FC-3 (c) — smallest harmless construction probe (ONE guarded call; highest risk; LAST)

**Question:** does calling `image_base + 0x23d590` with the explicit-output
ABI `void fn(ptr output, ptr text, i32 mode)` (C-8; B-2; B-4) construct a
consistent FName from an existing static name?

**Preconditions (ALL, no exceptions):** P-i, P-ii, P-iii; **FC-1 `Validated`**
(code shape +, if run, five-call-site agreement); module is ALONE in ploader
(no retained-program FName hook active, C-5/C-6 interference); a
`"seq":N` pre-record `{"exp":"fc3","phase":"attempt","target":…,"mode":0}` is
FLUSHED to the JSONL before the first call (a crash then leaves an honest
marker — post-record absence IS the crash evidence, E-6's
absence-is-a-result rule).

**Inputs (nothing guessed):**
- `output`: two distinct 32-byte, 8-byte-aligned, zeroed stack buffers used
  in turn (32 bytes is a CAPTURE WINDOW instrument sized above every
  port-variant total {8,12,16} per C-1 — it is explicitly NOT a width claim;
  the changed-prefix extent is recorded as an observation only).
- `text`: process-static UTF-16 literals `"ByteProperty\0"` and `"Engine\0"`
  (both guaranteed present at startup: C-6's own verification name with its
  "findable as soon as any FName exists" note; "Engine" is a LoadPreInitModules
  anchor, B-1/B-4).
- `mode = 0` (`FNAME_Find`): NON-MUTATING find semantics per retained port
  enum (C-6 uses it for exactly this verification). The only mode value
  corroborated IN-IMAGE is 1 (Add) at the anchor call-sites (B-4); mode 0
  relies on the port's `EFindName` lineage, so any anomalous output is
  treated as REFUSAL evidence, never as license to try another mode.
  **Mode 1 / any Add-variant is REFUSED by design: it mutates the engine name
  pool and is out of scope for a probe.**

**Call schedule (exactly 4 calls, one name per record, no retries):**
1. `"ByteProperty"` into buffer 1; 2. `"ByteProperty"` into buffer 2;
3. `"Engine"` into buffer 1 (buffers re-zeroed between uses);
4. `"Engine"` into buffer 2. Each preceded by its flushed pre-record and
followed by its post-record containing the full 32-byte capture (hex),
changed-prefix extent, thread id, and monotonic timestamp delta.

**Pass (exact — ALL must hold):**
- P-a: all four calls returned (four post-records exist);
- P-b: each buffer shows a non-zero changed prefix (the function wrote
  through the output pointer — sret lowering CONFIRMED, C-8);
- P-c: same-name calls across the two buffers produced IDENTICAL changed
  windows (pool consistency: two constructions of the same name agree);
- P-d: `"ByteProperty"` and `"Engine"` produced DIFFERENT changed windows
  (the function actually reads the text argument — not a constant stub);
- P-e (optional cross-check, only if FC-2 validated offsets): scan the <=16
  sampled live objects' raw `NamePrivate@0x18` 8-byte windows for an exact
  raw match against a constructed name's first 8 bytes; a match is recorded
  as corroboration, a NON-match is NOT a failure (different name, width
  still unresolved D-1 — the check is one-directional).

Verdict `Validated` requires P-a..P-d. P-e only annotates.

**Fail/refuse semantics:**
- Any precondition unmet (including FC-1 below `Validated`) -> `refused`, the
  four calls DO NOT HAPPEN.
- All-zero buffers after a returned call (P-b fails) -> `Refuted` as an
  sret-constructor candidate: either wrong address or ABI mismatch; the
  candidate is DEMOTED to refuted-in-process, the ladder closes, and the gap
  reopens at the survey level (new anchors, controller decision — never an
  in-probe rescan).
- Same-name divergence (P-c fails) or same-output-for-different-names (P-d
  fails) -> `Refuted` (non-deterministic / text-blind), same closure.
- Console hang/reboot with pre-records but missing post-records -> recorded
  as `Refuted` (unsafe address) via the absence rule; no replay, ever.
- NO second attempt within a launch; NO Add-mode fallback; NO width inference
  from the changed-prefix extent (that number stays an observation until a
  width-facts predicate validates it — D-1).

**Threading note (lineage, not proof):** the retained port itself calls the
ctor from its init thread (`FName("ByteProperty", FNAME_Find, address)`,
Signatures.cpp:248) before any hook exists, so an init-thread probe matches
retained behavior; the probe records its thread id and, if the ingame-probe
game-thread identity fact (E-6 route) is available, preferentially schedules
the four calls after the first genuine Tick to minimize pre-init pool risk.

---

## Recommended order and dependencies

`FC-1 -> FC-2 -> FC-3`, one launch each (records stay unambiguous); FC-2's
pool-reference subset soft-depends on FC-1; FC-3 HARD-depends on FC-1
`Validated` and on running alone. FC-2 may run in the same launch as FC-1
(both read-only) — recommended to halve user-coordinated launches — but FC-3
is always its own launch. Rollback at every stage: remove the ploader.ini
line, delete the PRX, keep the retrieved JSONL + sha256 with the receipts.
