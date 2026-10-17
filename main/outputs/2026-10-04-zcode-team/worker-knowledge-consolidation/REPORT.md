# REPORT — worker-knowledge-consolidation (2026-10-05)

Status: **DELIVERED (read-only synthesis; no console contact, no nested subagents, nothing
executed, no frozen or wiring file modified).** Deliverable order honored: CLAIM.json first,
KNOWLEDGE-BASE.md, this report last. ~30 tool calls.

## What was delivered

1. `CLAIM.json` — scope, claims KC-1..KC-3, non-claims, deliverables (written first).
2. `KNOWLEDGE-BASE.md` — the read-first consolidation document:
   - **Domain A — Target Facts** (7 sections): pinned image/container facts; runtime memory
     model (exec-only span prot=0x4, validated single bias 0x400000, RELRO L3 read-only);
     GUObjectArray layout A + engine identity; kernel interface facts (VMMAP reliability,
     PATHNAME ENOENT, kve_path zeros, DT_DEBUG dead); mprotect import situation; the seed
     table (6 slots with per-slot status); import contracts (191-row frozen / 196-import
     composition, 4-arg runtime helper).
   - **Domain B — Methods** (9 entries): VMMAP variable-stride walker; RELA reconstruction;
     seed derivation (3 independent successes + honest-refusal standard); map-gated data-side
     reads; multi-window identity gate; write-through record discipline; fail-open runtime;
     offline native-ness oracle; ridealong law.
   - **Domain C — Engineering Patterns** (7 entries): gate+receipt pipeline (8→13→15
     fail-closed checks, negative control, determinism pair/triple, replace-receipt, ODR
     pre-flight); console safety; ledger/honesty discipline; evidence channels; worker-scope
     contract; compile-time gates over settings keys; FTP/fself channel laws.
   - **Domain D — Anti-pattern ledger**: 17 proved dead ends/traps, each with a one-line
     why-dead citation. The seven required by the task are items 1-7: header scans on
     exec-only, ps_scan structural death (G0-G4), tick witness route (B1-B4), fixed-stride
     parsing, PRX-substitution temptation, stale receipt classes, the reuse-lineage bug —
     plus 10 more earned the same night (igf5/igf6 exec reads, libc weak-resolution-to-0,
     GMalloc string idioms, settings-key gating, availability-globals trap, 4-arg/5-arg ABI,
     /tmp sinks, stale checkpoint narratives, unproven single-bias assumption).
   - **Domain E — Open questions**: E1 writability (IGW-1 armed, probe in flight for the
     return; decision tree W-works / W-fails-R-works / both-fail; residual ring-0 risk
     explicit), E2 R3 carrier (F-1, owning artifacts + permanently-dead list), E3 ProcessEvent
     family (vtable-swap R4 design, coverage-check requirement, zero console evidence),
     E4 smaller opens (FC-1 held, required-slots answered, ODR runtime proof, comp6 staging
     pending).
3. `REPORT.md` — this file (written last).

## Method and coverage

- Started from the team's own state index (`W/worker-project-state-index/CURRENT-STATE-DIGEST.md`
  + `INDEX.md`, frozen 02:31Z at 201 ledger entries) so state was not re-derived.
- Read the ledger's post-freeze growth (259 `zcode_*` keys at this read) to capture what the
  index predates: composition-6 gate 13/13 GO, the D1 reuse-lineage root cause and fix,
  GMalloc derivation + arming split (launch-9 3-slot safe vs launch-10 4-slot armed, gate
  15/15), launch-9 prediction, IGW-1 probe arming, R3 zero-syscall tier + carrier prep,
  required-slots verdict.
- Deep-read the decisive artifacts per domain: EXEC-WRITABILITY-EVIDENCE.md (exec-only
  model), SEGMENT-CORRESPONDENCE.md (vaddr semantics + PT_LOAD table), TICK-WITNESS-DESIGN.md
  (witness blockers), required-slots REPORT.md (ps_scan gates G0-G4), gmalloc-derivation
  REPORT.md (RELA reconstruction), seed-table-derivation REPORT.md (seed method),
  vtable-swap-design REPORT.md (RELRO correction + swap architecture), r3-carrier-prep
  CLAIM.json (native-ness oracle), engine-tick-fallback + diag-channel-postmortem (fail-open),
  console-safety procedure, comp6-driver checklist (stale receipts), virtual-memory REPORT
  (DLL-family VM owner, distinct from the VMMAP walker — disambiguated in the KB).
- Targeted greps located the anti-patterns not named in any single doc: the reuse-lineage
  bug (L`d1_root_cause`, `W/worker-d1-determinism-diagnosis/`), stale receipt classes
  (`W/worker-gate-evolution/` side-finding, L`fresh_r11_gate_pair`, comp6 checklist §1),
  PRX-substitution removal (L`composition5_launch8_result`).

## Honesty notes

- Every KB entry cites a worker dir plus its decisive artifact; where the underlying evidence
  is hypothesis-grade (igf5 kernel-fault attribution, FreeBSD errno predictions, D-1 RELRO
  re-protect outcomes) the KB says so explicitly rather than promoting it to fact.
- Two numbers look contradictory without context and are disambiguated in the KB: the frozen
  191-row import contract belongs to the observer/facts lineage while the composition binds
  196 imports; "worker-virtual-memory" is the DLL-family Windows VirtualMemory emulation, not
  the VMMAP variable-stride walker (that lives in the bridge r5 fix + worker-c2-probe).
- The KB is a snapshot: launch 9/10 outcomes, the IGW-1 verdict, and the R3 carrier
  measurement did not exist at write time (console unreachable at 04:56Z; launches pending
  the user). It records the owning artifacts so the returning session updates rather than
  re-derives.
- No new target facts were invented; consolidation only. No quality, completion, or parity
  claim is made.

## Non-claims

No console contact, no probe execution, no nested subagents, no ledger writes, no edits
outside this directory. Nothing here claims engine-ready, mods-started, spawner activation,
or completion.
