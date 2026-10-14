# Agent AR — Interpretation matrix v4 (post-offset-fix), appended to LAUNCH-PLAN.md

Date: 2026-10-08 night. Deliverable: the section
`## Interpretation matrix v4 — post-offset-fix (2026-10-08 night)` appended
to `outputs/2026-10-08-l25-prep/LAUNCH-PLAN.md` (only edit; v2/v3 sections
and everything above them untouched). This report is the working record.

## 0. Inputs read (all under 2026-10-08-l25-prep/ unless noted)

- `agent-AA-offset-verify.md` — offset root cause proven from engine code:
  l24 profile assumed S(crit)=0x28, the shipping Prospero build uses 0x18;
  REAL create=0x58, delete=0x68, delete-crit=0x78 (recursive, lazy-mutex,
  0x18 bytes), serial=0x90, total=0x98. The l24 accessor read the CRIT
  STATE WORD as "Data" and mutex-pointer halves as Num/Max → non-heap Data
  → the Z11 chain's allocator abort. Engine's own Add @ image 0x4841A0
  (runtime 0x8841A0), Remove @ runtime 0x884240, this=&GUObjectArray
  runtime 0x97BA110.
- `agent-AE-variant-c.md` — variant C patch ready
  (`outputs/2026-10-08-l25-runtime/variant-c-proposal/variant-c-patch.diff`),
  locked + engine-allocator end-to-end (GMalloc@0x9340950, Realloc vtable
  +0x40), recursive mutex, refusal code 7 EngineListenerCallUnavailable,
  14 boot-time engine call sites prove the family is live.
- `agent-AC-container-audit.md` — ranked post-:528 site table; first B
  hazard = LuaMod.cpp:7995 (G1); GetFullName cluster rank 2 (G2); cache
  block X1/X2 (G3); main.lua runs BEFORE the [l22] mod-start marker; the
  "crashes one line later" hazard class.
- `agent-Z11-stack-symbolize.md` — call-graph-verified crash chain
  (Add→Append→ResizeTo→FMemory::Realloc→FMalloc::Realloc→call rax→game
  fatal formatter→abort), UE4SS-InitThread, byte-identical ×6.
- LAUNCH-PLAN.md v2 (Z7) + v3 (AF) sections — B-first order, marker
  vocabulary, spawn procedure; plus O §5 (listener markers), P §1 (phase
  tags), AD §6 (item-4 GMalloc logging + refusal 6), AO (G1–G3 exact skip
  lines + nm proofs), Z6 (mechanism B), Z10 (residual ranking #1–#6).

## 1. What was written (structure of the appended v4 section)

- **v4.0 New baseline**: the corrected profile eliminates the l24 crash
  mechanism at the source for A and C (real engine-heap Data at 0x68); moot
  for B (compiled out, nm-proven). PRECONDITION added: STEP 0 static-gate
  tables must pin create 0x58 / delete 0x68 / crit 0x78 / serial 0x90 /
  total 0x98 and show no l24-era delete=0x78 pin — otherwise A is still
  the l24 crasher and the matrix doesn't apply. Residual-risk map for A:
  unlocked-write race (Z6 mech B), foreign-thread GMalloc use (now valid
  block, accepted per the engine's own path), GetFullName cluster (G2),
  LuaMod registration (G1). B = control; C = proper fix.
- **v4.1 Outcome table**: 8 rows — B-1 full success, B-2 menu silent,
  B-3 no mod start, B-4 B crashes, A-5a survives (expected), A-5b crashes
  at registration, C-6a survives (production), C-6b refusal/in-Add/late
  crash.
- **v4.2 B-1**: 9-step proof-marker sequence (skip lines → GC print →
  event_loop_started → on_program_start_entered → G1 skip → armed [n:] →
  Lua mod started + Loaded v1.0 → first_invoke/pumped → pad chain → menu
  pair → Spawned proof → zero klog hits → Stopping mod). Honest scope note:
  B also removes G2 prints + the skipped eager detour arms; the
  registration attribution rests on Z11's frames sitting inside Add→Append
  →Realloc. B never left resident.
- **v4.3 B-2 ladder**: armed → first_invoke → pumped/eigt drained → pad
  edges (pad_available/first_state_read/key_edge 118/l3r3_edge/
  f7_dispatched) → keybind entry → mod-side failure lines (Scan failed /
  UI creation failed / WidgetBlueprintLibrary not found).
- **v4.4 B-3 ladder**: GC print → event loop started → on_program_start
  entered → G1 skip → [TypeScan] core-ready / R3 → mod start; plus the
  explicit main.lua-vs-runtime discriminator (mod prints w/o [l22] marker:
  game alive = mod hang; game dead + log tail inside lua startup + lua
  frames in klog = mod-level CRASH — distinguished from a runtime stall =
  no marker + game alive; no mod prints at all = runtime-side).
- **v4.5 B-4**: klog backtrace IS the evidence (B has NO item-4 pre-Append
  logging by design — the logging lives in the Add callee B never calls).
  First check: if the Z11 chain frames are STILL present, the B gates
  didn't ship (build error); different frames = new site (symbolize vs the
  l25 combined object, phase-tag bracket). Then crash-moment split:
  init-time → AC's remaining LOW table; save-load → Z10 #2 FName-thunk
  [[noreturn]] + #4 pump/detour + the ungated mod boundary; L3+R3/spawn →
  first live SetFuncPtr dispatch; theoretical #5/#6. STOP/reboot/escalate;
  no replays.
- **v4.6 A-5**: expected to survive registration; full success-marker
  sequence; then the item-4 anomaly enumeration (the pre-line's purpose):
  growth_required=1 + vptr=0x0 with GMalloc non-null = dead/changed
  instance; pre≠post GMallocInst/vptr = Append mutated the allocator
  chain; REFUSED(6) under the corrected profile = fix not in the build (or
  anchor moved) — and code 6 firing is the screen WORKING (clean skip, no
  Realloc); header-pre-without-post after growth_required=1 = fault inside
  Append with state recorded; RISK-realloc crash or Num ANOMALY =
  mechanism B live → straight to C; clean registration + save-load crash
  with/without dispatch markers = callback body vs mechanism-B/unrelated
  (C distinguishes).
- **v4.7 C-6**: production candidate outcomes — survives = ship (re-derive
  labels per title; AE's promotion follow-ups); REFUSED(7) = resolver, not
  engine; crash inside engine Add = foreign-thread hostility → AE/Z11
  fallback (deferred game-thread registration via the R3 pump); survives
  registration but save-load crash with dispatch markers = callback body;
  A-vs-C divergence = highly diagnostic (lock/engine-allocator delta),
  escalate with both harvests.
- **v4.8 standing rules**: B never resident; C needs packet → Claude →
  user OK → STEP 0; one launch per OK; crash → harvest → reboot; no claims
  without markers present.

## 2. Consistency checks performed

- Every marker string copied verbatim from landed vocabulary: O §5
  (`[L25][DeleteListener]` family, `engine called …`, skip line), AO §1–3
  (G1/G3 exact skip lines, G2 `[n: {:X}] @0x{:016X}` formats), AD §6
  (`header pre: … growth_required= GMallocVar= GMalloc= GMallocInst=
  vptr=`, `GMalloc post:`, `REFUSED (6)`, `l25_dl_gmalloc_pre/_post`), P
  §1 (`[l25] event loop started`, `on_program_start entered`,
  `r3_marshal_first_invoke`, `r3_marshal_pumped`, `eigt_queue_depth`), v3.3
  mod markers (`Spawned`, `Menu created.`, `Scan failed:`, `Loaded v1.0`).
  No strings invented.
- Engine addresses match AA §2/AE §1-3 (Add runtime 0x8841A0, GUObjectArray
  runtime 0x97BA110, crit +0x78 recursive, GMalloc image 0x9340950 /
  Realloc +0x40).
- Orderings follow AC's thread/handoff chain (InitThread → UpdateThread →
  per-mod thread) and AO's post-edit line numbers (G1 at tree line
  8076-8091, audit line 7995).
- The "item-4 logging is NOT in B" fact verified from AD §2 (B gates the
  call sites; the instrumented callee never runs) — stated in v4.5.
- No console contact, no builds, no other files touched. The only edit to
  LAUNCH-PLAN.md is the appended section (plus one in-section typo fix
  068→0x68).

## 3. Open items for the coordinator

- The STEP 0 precondition (corrected offsets pinned in the l25 build
  receipt / static-gate tables) is asserted, not verified — the profile-fix
  template exists under `outputs/2026-10-08-l25-runtime/profile-fix-template/`
  but the rebuilt profile's adoption must be confirmed before v4 applies to
  variant A.
- AE's variant C seed-slot promotion + admission-time prologue verification
  remain open (flagged in v4.7).
- G4 (quieting the StaticConstructObject arm) deliberately not implemented
  (AO §2) — v4.4 documents the arm's soft-refuse noise so B-3 rung 2 is not
  misread.
