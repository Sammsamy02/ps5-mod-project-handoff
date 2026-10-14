# agent-GD-phasebc — Phase B plan + Phase C verdict (2026-10-10)

Deliverables (design only — no builds, no console, no tree edits; Mac on
battery; nothing runs on console without Claude review + user OK, law
written into both docs):

1. `outputs/2026-10-08-l25-prep/PHASEB-PLAN.md` — kstuff-lite kernel bridge
   plan for #1/#3/#6.
2. `outputs/2026-10-08-l25-prep/PHASEC-EARLY-LOADING-DESIGN.md` — #4 early
   loading analysis + verdict.

## Phase B readiness gaps (blockers before any client code)

1. **kstuff-lite v1.11 source not on the Mac** (FZ's open blocker, restated)
   — the plan's §2 pinning procedure is the gate: decrypt-off fetch
   (`-Q "SITE SELF"`; the decrypt law DOES apply — kstuff-lite.elf is an
   executable-class payload) + magic readback + sha256 vs the pldmgr mirror
   hash `ab9a6cb4…`; version-string extraction; **byte-identical rebuild or
   bust** (bust => source advisory-only, interface pinned from the BINARY
   via static kekcall/0xdeb7 inventory, writes need Claude re-review).
2. **Lite kekcall subset + 0xdeb7 ABI unknown** — resolved by the binary
   inventory step even if the source pin busts.
3. **Gadget-offset duality** (`getpid+7` family source vs `+0xa` project
   CRT) — pinned from the fetched binary's own usage; the skeleton TU
   refuses without a verified stub signature (fail-closed).
4. **mdbg (syscall 573) write-path i-cache semantics unknown** — data-page
   patches first; instruction patches only after a proven target-side sync.
5. **FTP-up-after-crash pre-proof** required before the first write-rung
   test; read rungs don't need it.
6. **#6 kernel input route: DECLINED** — pinned kstuff exposes no pad/input
   services (kekcall table has none; IDT hooks are debug-register
   intercepts); userspace ScePad (PS5PadSource) already meets UE4SS parity
   (PC UE4SS is userspace input too). Revisit trigger documented, not
   planned.

Also in the Phase B doc: read-primitive ladder R0-R4 (actual cross-process
read = KEKCALL_REMOTE_SYSCALL 0x500000027; R0-R2 need no new authority);
patching law interaction (runtime memory only, disk never, revert-first);
risk ladder L0-L4 (L4 = any new kernel-resident code = prohibited this
phase); no-autoload law, kill-switch, Z1+kernel crash watch list, rollback,
3-restart acceptance; full `[KB]` skeleton TU code block (compile+runtime
double-gated default-OFF, P1-style fail-closed admission) + exact
PRX-integration checklist (NEW_UNITS entry, -DMODBRIDGE_KB_PROBES, CHANGES
scope id, markers-declared.json, snapshot/BRIDGE_SNAPSHOT_ID,
verify_build.sh --manifest, decrypt-off upload readback, one-dispatch arm).

## Phase C verdict

**#4 = SATISFIED-ALREADY** (no escalation; the
IMPOSSIBLE-WITHOUT-EXE-CHANGE branch is not reached):

- UE4SS "early" semantics = resident in-process DURING engine init (PC
  UE4SS itself waits for object construction — the same wait loop is in
  our tree), NOT before first instruction.
- Receipted from tonight's frozen-build session: eboot process create
  bounded (00:52:16-00:52:31] by the eboot monitor; runtime startup
  [BUILD] 00:53:05; **fname pool admission + slot-2 ctor binding + engine
  delete-listener ops admitted at 00:53:06 — while engine init continued
  for minutes** (item scan 00:54:35, DRY ladder 00:55:14, save load after);
  mod loaded 00:53:08 before gameplay.
- The original #4 technique (eboot patching) is rejected as unnecessary and
  would violate the no-exe-modification law; suspension stays permanent.
- Options assessed: autoload reordering = no-op (injection is per-game-
  launch, not boot-ordered); kstuff-mediated pre-main hook = rejected
  (kernel-write risk, zero parity benefit; process create is already the
  earliest legal in-process point).
- Caveat kept honest: "injected before eboot's first instruction" is
  mechanism-proven (pinned ploader source) but not timestamp-receipted
  tonight — and is beyond PC parity anyway. The 3-pass observe-only test
  plan receipts it using ONLY existing frozen-build instrumentation
  (module_start_entered t_ms in supplier-diag vs the monitor bound); no
  new build needed. Note: `[FFO2] … num=0` is NOT used as timing evidence
  (num = FName Number field, not an instance count).

Boundary note in the doc: exec-page first-call hooking is a Phase B #3
patching question, not a loading-timing question — do not conflate.
