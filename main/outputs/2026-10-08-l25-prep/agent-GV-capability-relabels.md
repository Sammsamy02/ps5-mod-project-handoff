# agent-GV — capability relabels + confirmations (Claude-ordered)

2026-10-10. Docs + code reading only: no builds, no console contact, no
network. Files edited: `CLAUDE-CAPABILITY-STATUS.md` (only) + this report.
Inputs: GO's audited table, `kstuff-lite-v1.11-VERIFIED.md` (GP),
`PHASEB-PLAN.md` (GD + GP CORRECTIONS), `keymap/MAPPING.md` (FV),
`mod-ladder/LADDER-V2.md` (GC), the l26 candidate source tree, the pinned
v1.11 source (GP's /tmp extraction), retained console receipts.

---

## 1. Relabels applied (one line each, flagged inline)

| # | New STATUS (verbatim) |
|---|---|
| 3 | **pointer/table hooks console-proven; instruction patching design-only** |
| 4 | **partly proven until shown against engine milestones (before first LoadMap)** |
| 5 | **crash-log symbolization, Mac-tested** (narrowed from live debugger) |
| 7 | **console-proven for ONE mod in ONE game; method changes a game index file** |

Rationale notes added inline per row:

- **#4**: the receipts prove residency during engine init in-process; no
  receipt timestamps admission against a named engine milestone (before
  first LoadMap), and "injected before eboot's first instruction" stays
  mechanism-proven only (GO's own caveat). Count summary updated
  (standalone console-proven: 2 → 1; new "partly proven" class for #4).
- **#5**: no live/debug-attached evidence exists (no breakpoints, stepping,
  live inspection) — the receipts are post-hoc klog symbolization by a Mac
  tool. Capability cell already read "(crash symbolization)"; the label now
  says exactly that.
- **#7**: result.json names ONE mod (`"mod": "Leon Townfall 2K v1.1"`) in
  ONE game (Townfall, `PPSA33286` — UE 5.6 per scanner row #2, so NOT the
  UE4 proof the broader label wants); the method REPLACES the game's
  `pakchunk0-ps5.utoc` index (profile.json original_index; backup retained;
  base UCAS untouched).

## 2. #1 SCOPE VERDICT — CONFIRMED, with one recorded narrowing

Original goal: reading memory the game can't normally see.

- **Covered, from the VERIFIED v1.11 ABI (GP's source read):**
  - kekcall **5 REMOTE_SYSCALL** (`0x500000027`): "Executes
    `sysents[nr].sy_call` with the target pid's first thread as `td`; the 6
    args are `copyin`'d from a3 (caller address space). Return: target
    syscall's retval copied back into the caller's retval …; failures
    surface as `EFAULT`/`ESRCH`" (VERIFIED §2 sub-5). Upstream reads
    another process exactly this way: `remote_syscall(SceShellCore, 608,
    …, &buf, …)` (`ps5-kstuff/main.c:1253-1257`), and remote
    `sysctl KERN_PROC/KERN_PROC_VMMAP` + `dynlib_get_info_ex` into our
    buffers is "the same channel class" (VERIFIED §3.1). So the game's
    process can be interrogated from outside — kernel-mediated reads of its
    VM maps, module tables, and process records — which the in-process
    runtime cannot do for OTHER processes.
  - kekcall **1 READ_DR** reads DR0–DR3/DR6/DR7 (48 B copy-out) and
    **3 RDMSR** returns MSR values — hardware-register visibility.
- **The narrowing (recorded in the row):** NO generic kernel-VA content
  read exists in the lite surface — R4 is dead (PHASEB-PLAN GP CORRECTIONS
  #3; VERIFIED §3.1 item 4: "no kmemcpy/kpeek service"); a true kernel
  read would be L4 = new kernel-resident code = prohibited this phase.
  Also noted: REMOTE_SYSCALL buffers resolve in the CALLER's address space
  (VERIFIED §3.1), so the channel reads what the kernel discloses about
  the target via syscalls — not arbitrary byte-peeks of target userspace.

### R0 FIRST-RUNG SPEC (written into the status doc, per Claude)

- **No game running**; stock kstuff resident; klog tap TCP 3232 NEW file;
  payload via the Z2 pldmgr procedure; one user-authorized action per boot.
- **Known value = firmware version 13.60, read via the exact call kstuff
  itself uses:** `r0gdb_get_fw_version()` = plain own-pid sysctl
  `int mib[2]={1,46}; sysctl(mib,2,&version,&size,0,0)` (pinned source
  `prosper0gdb/r0gdb.c:1738-1745`), value used as `version>>16` →
  `case 0x1360` (`prosper0gdb/offsets.c:107,174`). NO kekcall needed —
  this is below R1 on purpose (R2-class "plain syscalls in OWN pid…
  none (stock syscalls)", PHASEB-PLAN §3.1). Cross-checked against the
  console's independently known FW (13.60).
- **Crash/fault model (per VERIFIED §2):** plain syscalls error via errno;
  kekcalls: success ⇒ td_retval; handled-error ⇒ errno in rax; unknown
  sub-number ⇒ silent real-getppid fallback (cannot crash, G5);
  REMOTE_SYSCALL failures = EFAULT/ESRCH; CHECK resident ⇒ 0 (G4). L0–L2
  cannot crash the kernel if the ladder is honored (PHASEB-PLAN §4.5);
  recovery = natural payload exit / pldmgr process_kill OWN pid only;
  universal rollback = reboot (kstuff no unload, P8).
- **Read-only leaves nothing changed (source-cited):** kstuff calls the
  fw-version sysctl BEFORE any kernel write for fail-closed table selection
  (VERIFIED §5); READ_DR = 48 B copy-out of debug registers; RDMSR returns
  in td_retval; CHECK sets args[RAX]=0; upstream's remote READ paths
  (KERN_PROC_PID `main.c:530-545`; 608 vs SceShellCore `main.c:1253-1257`)
  touch no target state; the remote mmap/mprotect write family
  (`main.c:1056-1083`) is excluded from read rungs.
- **DONE LAW written into the rung:** DONE = 3 passes across RESTARTS,
  never one session (PHASEB-PLAN §4.6 + §2 P5; LADDER-V2 §3.4): fresh boot
  → residency read-only-verified → fw-version read returns the 0x1360-class
  value → payload exits at its bound → klog clean; 3/3 identical passes
  across 3 boots or the rung is NOT accepted. Then R1 (CHECK), R2 (own-pid
  608/KERN_PROC), R3 (REMOTE_SYSCALL per VERIFIED §6 R2 (a)(b)(c)).

## 3. #3 RegisterHook ANSWER — pointer-swap (VMT-class) method, CONFIRMED

The port's Lua `RegisterHook` (`outputs/2026-10-08-l26-runtime/typescan-r4/
candidate/src/LuaMod.cpp:5705-5809`) uses the PROVEN pointer-swap method,
not instruction patching:

- Resolves the UFunction by name via `StaticFindObject` (:5750).
- `FUNC_Native` branch (:5761-5775): `unreal_function->RegisterPreHook/
  RegisterPostHook` (:5767-5768) → `UFunction::GetFunctionHookData()`
  (`ue4ss-unreal-native-build-1/port/unreal/src/UFunction.cpp:78-89`) which
  on FIRST hook saves the original `Func` and swaps the UFunction object's
  `Func` FIELD to `&Internal::UnrealScriptFunctionHook` (`SetFuncPtr`,
  UFunction.cpp:86). The engine's own dispatch then calls the hook
  (`UFunctionStructs.cpp:163`); `UnregisterAllHooks` restores the saved
  pointer (UFunction.cpp:121-132). Pure data-member write inside a UObject.
- Non-native branch (:5776-5793): callbacks stored in
  `m_script_hook_callbacks`, dispatched from the ProcessInternal replacement
  flow (`execute_hook(LuaMod::m_script_hook_callbacks, true)` :8464) —
  again no code bytes.

**Hook kinds that work WITHOUT Phase B** (inventory in the status doc):
(1) UFunction Func-pointer swap — console-proven (the R3 FuncPtr-marshal
carriers are RegisterPostHook carriers; 3 armed in the menu-up session;
ExecuteInGameThread drained end-to-end); (2) non-native script hooks via
the ProcessInternal replacement; (3) engine listener registration
(GUObjectArray add/remove listeners, `UnrealInitializer.cpp:203-204,600` —
console-proven admitted ops `add=0x8841A0 remove=0x884240`); (4) VMT
vtable-swap tick pump (`VmtTickPumpCore.inc`, data-only, "no code byte is
ever written or read") — Mac host-fixture-proven only; console arming
refused (non-text entry in the copied extent).

**Hook kinds that NEED instruction patching:** native function detours via
PLH x64Detour (EngineTick/ProcessEvent pumps; the LoadMap pair including
its one-shot deferred arm — measured routing at LuaMod.cpp:8533-8558:
`Hooks.cpp:211` → `DetourInstance AddCallback` → `EnsureHooked` →
`InstallHook` → `PLH::x64Detour::hook()`). The code ATTEMPTS them today,
but on this target: R3 marshal compiles the registrations out/defers them
(":8594"; session log "detour registration compiled out under R3 marshal
mode"), and even attempted arms soft-refuse at the FIRST step — the
owned-memory registry returns nothing for the 100-byte prologue read
(`ZydisDisassembler.cpp:65`) and `hook()` returns false at the empty-insts
check (`x64Detour.cpp:355-358`) before any write. Making instruction
patching real = Phase B L3 (`SYS_mdbg_call 573`; GP: hooked-but-passthrough
in v1.11; x86-64 coherent, no i-cache invalidation needed).

## 4. #6 CONFIRMATION — mapped combos trigger no game actions

- **Reserved-button law (FV, `keymap/MAPPING.md` §2):** game controls are
  NEVER auto-assigned — `OPTIONS` = game pause ("proven tonight: l12
  deliberately returned OPTIONS to the game; stay unbound forever"); face
  buttons/d-pad = game actions; `L3+R3` = universal mod-menu chord, "no
  other row may use L3 or R3 in any combination". Chords are rising-edge
  conjunctions on the UE4SS update thread, one key event per completion
  (§1). The generator hard-refuses any reserved-button use (`--check`).
- **Empirical:** L3+R3 fires only mod behavior in every retained session —
  `zzdiag2-diag.jsonl` seq 1114-1117 (`pad_l3r3_edge` value 118 →
  `_allow`/`_subscribed` → dispatch) ; the menu-up session used it with no
  game-side action observed (PHASEB-PLAN §3.3 "console-proven across
  sessions"; CLAUDE-MENU-UP §1/§2, 6/6 twice incl. F7 rerun).
- **F7 = pad-mapped only:** keycode 118 exists on console solely as the
  L3+R3 dispatch (no physical F7 key); `[l22] keybind callback entry
  key=118 ref=4` (claude2 log) is the pad bridge.
- **Generator stays Mac-only:** FV's `keymap-gen.py` +
  `GENERATED-BASELINE.ini` have never been used on console; the live
  3,718 B profile is the pinned console-proven one; LADDER-V2 §1.3.1:
  "GENERATED-BASELINE.ini remains an unused staging candidate".

## 5. #7 broader-label checklist (written into the status doc)

Still needed, explicitly: [ ] 3 passes across restarts (the Leon trial is
single-session); [ ] several mods at once (overlay co-residency); [ ]
uninstall proven on console (rollback attested, restoration never
receipted); [ ] a UE4 game (current proof is Townfall, UE 5.6); plus the
already-tracked additive/runtime mount form (Phase 0 launch per
PAK-RUNTIME-MOUNT-DESIGN §4).

## 6. #8 STANDING POLICY — texture pipeline PAUSED (written into the status doc)

Texture conversion is ON HOLD until a real mod demonstrably fails without
it: Leon's PC textures ran unconverted and the game ran normally; the only
anomaly candidate (glasses checkerboard) is `cause_confirmed: false` and
GM's file-level verdict names PC-only SHADER platforms, not textures
(`shader-fallback/fallback.md` §1b/§1d). Refocus = GM's PC-only-shader
MATERIAL fallback: derived overlay entries generated at install pointing
at EXISTING game shaders (Option A keep-base-entry-with-PS5-hashes;
Option C known-good base invisible material), originals untouched
("original mod files are never modified"), Option B (Sony-licensed
SP_PS5 cook) REFUSED — no Sony shader tools, ever. Resume trigger: a real
mod fails with textures unconverted AND the material fallback cannot
rescue it.

---

Provenance: every claim above is cited in `CLAUDE-CAPABILITY-STATUS.md`
(file:line where applicable). Nothing here authorizes console work; all
rungs remain gated per PHASEB-PLAN §0 (Claude review + explicit user OK
per step).

— agent GV (ZCode), 2026-10-10. Battery-safe session: docs + code reading
only; no builds; no console contact; no other files modified.
