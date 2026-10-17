# PROMOTION-DECISION — Townfall native layout pair (FUObjectArray / TUObjectArray subset)

| Field | Value |
|---|---|
| Document | PROMOTION-DECISION.md |
| Session | 2026-10-04-zcode-team |
| Date prepared | 2026-10-04 |
| Prepared by | worker-promotion-decision (ZCode) |
| Decision owner | zcode-controller (signature required — section 4) |
| Status | PREPARED — conditional GO framework; signature deferred until the code-anchor criterion (C2) is settled by igf6 |
| Subject | Promotion of the staged native layout pair emitted by worker-staged-pair-live (`emission-live/`) from "evidence" to "stageable artifact" for the FUObjectArray/TUObjectArray subset (rung 1→2) |
| Absolute workspace | `/WORKSPACE/.codex/worktrees/translation-core/what` |

Standing discipline (binding on this decision, per the user's operating contract):
**only what completed checks establish may be promoted.** Nothing in this
document elevates a draft, a plan, a context count, or a critic score into an
engine fact. Raw reports remain unchanged; every claim below cites its source.

---

## 1. Evidence table

Every check, its result, and its exact source. Verdicts use the task's
vocabulary; where a check is not complete, the row says so and names what is
outstanding. Shared provenance used by several rows:

- Retained game image: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`, 194,241,424 bytes, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (re-verified by multiple workers; = survey capture E).
- Profile vaddrs (worker-2-layout-facts `townfall-ps5-ue56.profile.json`): GUObjectArray `0x93ba110`, UGameEngine::Tick `0x2fac8c0`, StaticConstructObject `0x490aa0`.
- In-game igf4 run (build `igf4-4891869337b4`): `outputs/2026-10-04-zcode-controller/console/anchor-validation-results.jsonl` (10 records, seq 1–10).
- Live v3 run: `outputs/2026-10-04-zcode-controller/console/ingame-v3-full-results.jsonl`, sha256 `a2a8ee0a741fa41cd2676a391deedbdb437acb0cf51d47442ccac5674b1a2d58`.

### 1.1 GUA structural validation (layout A, corrected predicate, live data) — **PASS**

- **Live capture (real game memory):** igf4 seq 6 `gua_read` — 96 bytes read at runtime address `0x97ba110` (= base `0x400000` + profile vaddr `0x93ba110`), covered by RW span `[0x970c000, 0x9a34000)`; seq 7 `gua_writeback` — same-value write-back restored=1, proving the span is genuinely writable live BSS-like data. Decoded words: `w0=0x00008a9400008a95` (ObjFirstGCIndex=35477 / 0x8A95, ObjLastNonGCIndex=35476 / 0x8A94), `w1` MaxObjectsNotConsideredByGC=35477, `w2=0x13800220a0` (Objects**), MaxElements=2162688, NumElements=71959, MaxChunks=33, NumChunks=2, PreAllocated NULL. Source: `anchor-validation-results.jsonl` seq 6–7.
- **Corrected predicate evaluation (separate from the original, see 1.6):** worker-predicate-gc-fix — host replay of the exact captured bytes (cfgL: captured header span + labeled chunk-scaffolding reconstruction) yields layout A **Validated** with the exact live values (first=35477, last=35476, num=71959, max=2162688, chunks 2/33, `FUObjectArray_Outer`), while layout B stays Refuted (`GOA_R_PTR_NONCANONICAL`) — exactly one hypothesis survives, which is the predicate's definition of Validated. Corrected host suite 24/24 (original 23 checks with corrected fixtures + T20 replaying the live bytes). Sources: `worker-predicate-gc-fix/REPORT.md` §4–5, `replay_results.txt`, `test_results_fixed_suite.txt`.
- **Consumption by the staged pair:** worker-staged-pair-live emitted the pair from exactly this single real verdict (`guobject_array = Validated, layout A`), verdict digest `97a16867…25072`; ParsePair `ok:true` (17 keys, identity round-trips); live refusal demo (null identity → exit 2, nothing emitted) and negative control (BuildId stripped → exit 3) both behave correctly. Sources: `worker-staged-pair-live/REPORT.md` §2–4, `input/verdicts-live-gua.json`, `emission-live/parsepair-result.json`.
- **Honest boundary:** the corrected-predicate PASS is a host-side replay of live-captured header bytes plus labeled reconstruction of chunk contents; the in-game corrected-predicate replay (igf5 contract seq 6 `gua_pred_replay`) was never observed because igf5 failed (1.7). Predicate provenance itself (worker-5 suite, cross-worker PS5 object-hash match vs worker-predicate-compile) is host evidence cited by `worker-anchor-validation/RECEIPT.md`.

### 1.2 Single-bias mapping (base 0x400000, L7 exact match) — **CONFIRMED**

- **Checkpoint of record:** `zcode_v3_MAPPING_BREAKTHROUGH` in `worker-composition-3/evidence/transport-ledger.composition3-mirror.json` (line ~9819), status `SINGLE_BIAS_CONFIRMED_LOAD7_PROVEN_GUA_ADDRESS_ESTABLISHED`, actor "zcode-controller (from v3 VMMAP data)", UTC `2026-10-04T19:15:31Z`, source `ingame-v3-full-results.jsonl`.
- **Static derivation backing it:** `worker-vaddr-semantics/SEGMENT-CORRESPONDENCE.md` — the observed exec span `[0x400000, 0x787c000)` prot `--X` is byte-exact LOAD0 (memsz rounded `0x747c000`), proving `bias(L0)=0x400000`; Tick → `0x33ac8c0` and SCO → `0x890aa0` land inside the observed exec span. LOAD7 (RW, vaddr `0x913c000`) is the segment containing the GUA; the v3 live map proves its placement, and the igf4 `gua_read` at `0x400000+0x93ba110=0x97ba110` returning structurally sane FUObjectArray bytes is the operational confirmation (a wrong L7 placement could not produce first==last+1 with consistent counts and a readable chunk pointer).
- **Supporting analysis:** `worker-mapping-recovery/REPORT.md` — recovered VMMAP spans argue base `0x8018c000` puts the GUA in a read-only span (against that alternative) and base `0x400000` puts it in the (dense, corrupted) gap where the live RW span was subsequently observed.

### 1.3 Axes (non_shipping / stats) — **RESOLVED** (from game evidence)

- **Result:** SHIPPING (`build_configuration=1`, margin 60 over runner-up), STATS OFF (`stats=1`, margin 12), engine fingerprint 5.6 (margin 20; evidence-only — the runtime detects version separately). Source: `worker-axes-detector/REPORT.md` §4 — faithful C port of the genuine patternsleuth cost-model classifiers (244 UTF-16LE gated markers, 30 candidate builds), host-measured on the real retained Townfall image; C detector and an independent Python reference agree exactly; 54 markers present (9/10 ShippingOnly present, all 32 NotShipping/DevOnly markers absent, all 12 WithStats markers absent).
- **Meaning for staging:** with these axes the runtime publish gate inputs are determined; the offline INI declares 0/0 pending the runtime's own in-probe resolution, and `SetResolvedNativeLayoutAxes` → `PublishStagedNativeLayoutMaps` refuses any staged-vs-resolved disagreement at runtime regardless (fail-closed). Source: `worker-staged-pair-live/REPORT.md` §6.
- **Honest boundary:** this is a host measurement of the real game image — the strongest offline evidence class — not yet an in-probe console record; the console-side axes record was igf5's `exp="axes"` slot and transfers to the igf lineage rebuild.

### 1.4 Identity (engine_revision / build_id) — **RESOLVED**

- **Result:** `engine_revision = "5.6.1-0+++UE5"`, `build_id = "04bb4330a7d71dcefe1cc2c4d19769d5"`. Source: `worker-layout-identity/IDENTITY-RESOLUTION.md`, every component byte-verified against the retained image (store-site decode at file `0x1F37DE`: Major=5, Minor=6, Patch=1; return-0 stubs → changelist 0, licensee 0; branch accessor → wide `L"UE5"` at file `0x782A782`; ELF `NT_GNU_BUILD_ID` note at file `0xB93B250`). Acceptance matrix: all binding conditions pass.
- **Two ratification items reserved to the controller (documented, not hidden):** (a) the assembled revision string is a semantic reconstruction — UE renders it at runtime; no component is invented, the joined form is not a verbatim image string; (b) `build_id` is the ELF GNU build-id (UE's FModuleManager BUILD_ID string does not exist in this image — negative searches documented in §5 of the source). Both fields are staging provenance only: the runtime publish gate never compares them (`NativeLayoutBinding.cpp` publish gate compares only major/minor/non_shipping/stats).
- These values close the GAP markers that worker-staged-pair-live deliberately baked into the pair (`GAP-NOT-A-SCAN-FACT-…`); re-emission with real identity is required before staging (see section 3).

### 1.5 Code anchors (Tick / SCO byte comparison) — **PENDING igf6**

- **Only in-game attempt so far (igf4):** seq 4 `tick_anchor` and seq 5 `sco_anchor` both returned rc=3 refusal `covered_not_readable` — the exec-only pages (prot lacking READ per the current VMMAP) were **not** dereferenced; no bytes were read; nothing crashed. Expected 16 bytes (`554889e54157415641554154534881ec` at Tick `0x33ac8c0` / SCO `0x890aa0`) are triple-sourced: retained-image decode (`worker-vaddr-semantics` §3), `worker-expected-bytes/EXPECTED-BYTES.json` (independent extraction, first-16 byte-identical, bytes 17–32 continue the prologues: `sub rsp,0x88` / `sub rsp,0x258`), and build-time injection into igf4/igf5 (`worker-anchor-validation/RECEIPT.md`). Summary record: `anchor_confirmed=0`, `promotion_gate_indicator=0` (indicator only).
- **igf5 (mprotect-first design):** built, deterministic, fully host-checked (`worker-code-anchor-reads/REPORT.md`, build `igf5-4186048a6dd6`) — staged and launched; produced **zero records** and is crash-correlated. The anchor comparison therefore never ran. See 1.7.
- **igf6 (rebuild):** worker-igf5-diagnosis-rebuild — direct read (plain dereference/memcpy), no mprotect, no syscall, igf4 packaging recipe, two-run determinism; in progress at preparation time. The anchor check stays **OPEN** until an igf6 launch returns `byte_match=1` for both anchors under the map-trust/refusal discipline — or the controller explicitly names and accepts a concrete alternative read that establishes equivalent Tick/SCO correspondence. Per `worker-staged-pair-live/REPORT.md` §7, a rung-3 vtable read may corroborate but **never substitutes** for the byte anchors.

### 1.6 Predicate original refutation (GC-order bug) — **DOCUMENTED**; corrected evaluation SEPARATE

- **The refutation is real, preserved, and never overwritten:** igf4 seq 9 `gua_pred_live` (8657 live spans, map_trusted=1) returned layout A **Refuted `GOA_R_GC_ORDER`** under the original P3; the header-span configuration (seq 8) refuted at P1 as designed. These raw records stand unchanged in `anchor-validation-results.jsonl`.
- **Root cause:** worker-5's P3 asserted `first_gc <= last_non_gc`; the real UE invariant is the opposite (permanent objects occupy `[0, Last]`; GC-visible objects begin at First; settled state `First == Last+1` — exactly what the live bytes show). Under the old rule every settled FUObjectArray state auto-refutes, so layout A could never validate against a real array. Source: `worker-predicate-gc-fix/REPORT.md` §1–2, including the honesty note that the invariant is established from UE semantics plus decisive captured-data self-consistency (first=last+1; last <= NumElements per `worker-expected-bytes`), because engine GC maintenance code is not retained in the tree.
- **Separation discipline:** the correction is a surgical diff (`townfall_layout_probes_gc_order.diff` + fixture diff + T20); the original verdict is preserved as replay row [A] and reproduced exactly; RCode `GOA_R_GC_ORDER` retained with corrected meaning; the fixed and original evaluations are separate vendored trees (`vendored-fixed/`, `vendored-original/`). No raw report was edited.

### 1.7 igf5 failure — **DOCUMENTED** (cause under diagnosis)

- **Facts:** igf5 (`modbridge_ingame_facts.prx`, fself 25,981 B, sha256 `486845b7e121d5993febdf812ac55815cc974fc1e2a062ebb4ac60fc5b8f8dfa`) launched; the log-lineage file received **zero records**; failure is crash-correlated. Last known-good is igf4 (26,077 B, sha256 `4aa624cc…`, full 10-record run). Sources: `worker-igf5-diagnosis-rebuild/CLAIM.json`; `worker-code-anchor-reads/REPORT.md`.
- **Leading hypothesis (not established):** the mprotect inline-syscall shim (`SYS_mprotect 74`, constants parsed from pinned SDK headers) — the kernel syscall number cannot be verified on-host and was the explicit residual-risk sign-off gate in `worker-code-anchor-reads/REPORT.md` §3. Binary-level diff igf5 vs igf4 and the no-syscall igf6 rebuild are the diagnosis worker's tasks, in progress at preparation time; nothing here pre-judges its result.
- **Consequence for this decision:** criterion C2 cannot be evaluated yet; the signature is deferred (section 4). No igf5 retry, no replay of the failed candidate — the rebuild is a new, differently-designed probe, consistent with the no-identical-resubmission rule.

---

## 2. Promotion criteria (what must be true to promote)

The decision is per-criterion. Promotion of the rung 1→2 subset requires **all** of C1–C5.

- **C1 — GUA validation stands (currently satisfied).** The PASS in 1.1 is the evidence of record. It may be re-opened only by new contradicting live evidence (e.g., an igf-lineage corrected-predicate record returning a refute-class code on the same bytes), never by re-arguing old evidence.
- **C2 — Code anchors settle (currently OPEN; blocks signature).** Either:
  - (a) an igf6 record shows `byte_match=1` for both Tick and SCO 16-byte expectations under the standing map-trust and refusal discipline (no force-read, every refusal recorded, no permission state left altered), **or**
  - (b) the controller names, in the sign-off block, one concrete alternative read (with its own record evidence) that establishes equivalent Tick/SCO correspondence, and accepts it explicitly as the substitute.
  Corroborating-only evidence (vtable proximity, prologue-shape classifiers) does not satisfy C2.
- **C3 — Identity and axes accepted (currently satisfied, two ratifications due).** Values per 1.3–1.4; the controller ratifies the two documented conventions (revision-string reconstruction; build_id := GNU build-id) at signature. The runtime publish gate remains fail-closed on major/minor/non_shipping/stats regardless.
- **C4 — Scope limit respected.** Promotion extends **only** to the subset the completed checks establish: FUObjectArray + TUObjectArray (the rung 1→2 emission). Anything beyond requires the reads ladder of `worker-staged-pair-live/RUNG3-REQUIRED-READS.md` — R1+R2 (chunk table + FUObjectItem samples → unlocks FUObjectItem), R3+R4 (live item → ClassPrivate → SuperStruct chain → unlocks UObjectBase/UStruct/UClass), R5 (FField-variant settlement → unlocks FField/FProperty only if UE5_FField), R6 (strengthening only, never gates) — each landing as Validated under the same refusal discipline. UFunction stays permanently refused (classifier-only); FName pool stays excluded (T19, construction gap is its own fact).
- **C5 — Artifact readiness for staging.** The pair must be re-emitted with the real identity values (replacing the `GAP-NOT-A-SCAN-FACT-*` markers) and ParsePair-verified + negative-controlled again; staging remains forbidden for any placeholder-identity artifact. (worker-staged-pair-live §5; assembler refuses incomplete identity by construction, demonstrated live.)

Per the user's discipline: if any criterion is unmet, the unmet part is recorded as unmet — no partial silent promotion, no re-rating of unchanged evidence.

---

## 3. What promotion means — and does NOT mean

### Promotion MEANS

1. The staged pair in `worker-staged-pair-live/emission-live/` (re-emitted per C5) is **real Townfall layout data** — FUObjectArray @ UEP_TotalSize 0xB8 and TUObjectArray @ 0x20 — whose emitted offsets are precisely the structure the validated GUA header established, traceable to live game bytes.
2. The pair is **stageable on the console**: the staging gate's identity requirements can now be satisfied with evidence-backed values (1.4), and the runtime publish gate's axes inputs are resolved (1.3) with fail-closed disagreement refusal.
3. The Tick/SCO address correspondence (base 0x400000) is confirmed for the anchor images, to the extent C2's evidence supports.

### Promotion does NOT mean

1. **Full engine integration** — no UE4SS runtime recomposition, no UnrealInitializer chain execution claim.
2. **Game-thread delivery** — no binding delivery, no tick-path presence, no harness-arm claims.
3. **Item Spawner working** — that milestone sits behind the full rung ladder (63-member / 8-section pair needs R1–R5; `worker-integration-checklist` steps 6–14 all remain ahead).
4. **Runtime facts for identity strings** — EngineRevision/BuildId stay staging provenance; they are never runtime-compared.
5. **Any callable-ABI or code-patching claim** — the code-shape probes are Inconclusive-classifiers by design; nothing here validates a hook target.
6. **Permanence of the mapping fact** — base 0x400000 is confirmed for the observed launches; each future launch re-verifies via the map-trust gate before any read.

### Rung ladder and per-rung acceptance bars

| Rung | Content | Acceptance bar |
|---|---|---|
| 1→2 (this decision) | GUA header → FUObjectArray + TUObjectArray sections | C1–C5 above |
| 2 completion | FUObjectItem | R1+R2 reads Validated (`uobject_item_chain`) |
| 3 | UObjectBase, UStruct, UClass | R3+R4 reads Validated (`ustruct_chain`) |
| 4 | FField, FProperty | R5 variant settlement == UE5_FField only |
| — | UFunction | Permanently refused (no Validated-class predicate exists) |
| — | FName pool | Excluded as data (T19); own construction-gap resolution path |

Each subsequent rung is a separate decision with its own evidence table; this document does not pre-approve them.

---

## 4. Sign-off block (controller)

> Signature is deferred until C2 is settled. The controller completes every
> field; an unsigned or partially completed block leaves the pair at its
> current status: **evidence, not a stageable artifact** (beyond the C5
> re-emission requirement, which applies before staging regardless).

```
DECISION (circle one):   PROMOTE (rung 1->2 subset)   /   HOLD   /   NO-GO

Timestamp (UTC):  ____________________   e.g. 2026-10-04THH:MM:SSZ

C1 GUA validation stands .......... [ ] accepted   citation: worker-predicate-gc-fix/REPORT.md §4-5;
                                     anchor-validation-results.jsonl seq 6-7, 9; verdict digest 97a16867...
C2 Code anchors settled ........... [ ] (a) igf6 byte_match=1 both anchors   build_id: ____________
                                       run: record seq(s): ______  bytes: tick ______ sco ______
                                    [ ] (b) alternative (NAMED): ____________________________________
                                       evidence record: _________________________________________
C3 Identity + axes accepted ....... [ ] ratified   revision "5.6.1-0+++UE5" (reconstruction) [ ] 
                                     build_id "04bb4330a7d71dcefe1cc2c4d19769d5" (GNU build-id) [ ]
                                     axes SHIPPING / STATS OFF (worker-axes-detector) [ ]
C4 Scope limit respected .......... [ ] promotion limited to FUObjectArray + TUObjectArray (rung 1->2)
C5 Artifact re-emission w/ real identity, ParsePair + negative control ... [ ] verified

igf5 failure acknowledged as documented, cause under diagnosis, no replay
of the failed candidate ........................................... [ ]

Evidence citations consulted (list the records actually read):
  1. ______________________________________________________________________
  2. ______________________________________________________________________
  3. ______________________________________________________________________

Conditions or reservations (if HOLD/NO-GO, name the blocking criterion):

  ________________________________________________________________________

Controller signature: ________________________   (zcode-controller, session 2026-10-04-zcode-team)
```

On PROMOTE: the integration chain proceeds per
`worker-integration-checklist/INTEGRATION-CHECKLIST.md` (STEP 0 gate opens),
with staging of the C5 re-emitted pair only. On HOLD: name the blocking
criterion and the new evidence required; no re-evaluation without new
evidence. On NO-GO: the pair remains evidence-only and the blocking fact is
recorded in the ledger.

---

## 5. Document provenance

- Prepared by worker-promotion-decision from read-only sources listed in this directory's `CLAIM.json`; every cited path is inside `/WORKSPACE/.codex/worktrees/translation-core/what`.
- The evidence statuses in section 1 mirror the task brief exactly: PASS (1.1), CONFIRMED (1.2), RESOLVED (1.3, 1.4), PENDING igf6 (1.5), DOCUMENTED with corrected evaluation separate (1.6), DOCUMENTED cause-under-diagnosis (1.7).
- No raw report, record file, or worker output was modified. This document is decision framework only; it executes nothing.
