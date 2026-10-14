# CLAUDE — PS5-as-PC CAPABILITY STATUS (8 receipt-verified rows + #12 design-only row)

**Mac-only audit; nothing here claims console work performed outside the cited
receipts.** Agent GO, 2026-10-10 evening. Every row below was re-verified
against the primary receipts (read-only; the one command executed was
`phasea-tools/symbolize_crash.py` in stdout-only mode on a preserved crash
klog). Paths are relative to the work dir
`/WORKSPACE/.codex/worktrees/translation-core/what/`. Where the parent's
draft cited evidence that did not hold up, the row was downgraded or its
evidence corrected — flagged `[AUDIT-CORRECTED]`; details in
`agent-GO-capability-audit.md`.

**REVISION (agent GV, 2026-10-10, Claude-ordered):** rows #3/#4/#5/#7
relabelled exactly per Claude's review; #1 scope confirmed against the
source-verified v1.11 surface with the R0 first-rung spec added; #3
RegisterHook method confirmed from the port's code; #6 no-game-action
confirmation added; #7 broader-label checklist and #8 standing texture-pause
policy added as sections below. Relabels are flagged
`[RELABELED 2026-10-10 Claude order; GV]` inline; each STATUS stays one line.
Details: `agent-GV-capability-relabels.md`.

Status vocabulary: **console-proven** (receipt from a real console session) /
**Mac-tested** (tool + tests ran on the Mac only) / **design-only** (docs +
plans, nothing executed) / **declined-by-design** (route rejected with recorded
rationale). Split rows carry both halves on one line, as directed.

---

## The table

| # | Capability | STATUS | EVIDENCE (receipt + decisive line) | BLOCKER (exact) | What would upgrade it (one next action) |
|---|---|---|---|---|---|
| 1 | Kernel reads (cross-process / kernel memory) | **design-only** (in-process R0 reads are console-proven — the port's own runtime) | `outputs/2026-10-08-l25-prep/FZ-PHASEB-KSTUFF-LITE.md` (interface, evidence classes [SRC]/[LOG]/[META]); `PHASEB-PLAN.md` §2–§3.1 read ladder R0–R4 (R1 `KEKCALL_CHECK` = "design; first kernel-adjacent call allowed"); pin event verified below. No client kekcall has ever been issued: `-DMODBRIDGE_KB_PROBES` appears only inside PHASEB-PLAN.md — never defined in any build config (audited: zero non-doc references). **[GV scope verdict]** Original goal "reading memory the game can't normally see": CONFIRMED covered by the verified v1.11 surface at its level — kekcall 5 REMOTE_SYSCALL executes any stock syscall with the target pid's first thread as td, args copyin'd from the caller, retval/EFAULT/ESRCH returned to the caller (VERIFIED §2 sub-5); upstream itself reads another process (SceShellCore) this way via remote `SYS_dynlib_get_info_ex 608` (`ps5-kstuff/main.c:1253-1257`) and remote sysctl `KERN_PROC`/`KERN_PROC_VMMAP` into its own buffers is "the same channel class" (VERIFIED §3.1). kekcall 1 READ_DR reads DR0–DR3/DR6/DR7 (48 B copy-out). ONE narrowing recorded: NO generic kernel-VA content read exists in the lite surface (R4 refuted — PHASEB-PLAN GP CORRECTIONS #3; VERIFIED §3.1 item 4) — true kernel-memory reads would be L4 (new kernel-resident code, prohibited). | Source<->binary identity for kstuff-lite v1.11 unproven (PHASEB-PLAN §2 P4 byte-identical rebuild not run; source zip present, pinned binary present); lite kekcall subset + 0xdeb7 ABI unknown until pinned. | Execute the R0 FIRST-RUNG SPEC (below): firmware-version sysctl read, no game running — then R1 `KEKCALL_CHECK` — 3 passes across restarts, each under Claude review + user OK. §2 P4 rebuild stays open (advisory-source caveat). |
| 2 | Scanner (engine/version detection) | **Mac-tested**, on console-proven primitives | Tool+tests: `phasea-tools/phasea-test-receipt.json` — `"passed": 18, "failed": 0`; Astro Bot inner ELF REFUSED (0/16 triage, 0/244 markers, sha `7723d150…`), Townfall eboot `VERSION_CONFIDENT` UE 5.6 shipping, 54/244 markers, sha `3b6cfad4…` (byte-exact reproduction of the recorded console profile). Console-proven primitives: `evidence/claude2-menu-session-20261010.log` L561 `[2026-10-10 00:53:06] [Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified (ByteProperty probe id == pool id 0x3)`; pool admission `[Seeds] slot 1 fname pool 0x9765EC0: admitted via image-segments` (also l21 `prior-console-evidence/REVIEW-TABLE.md` row (b): "console-proven"); SeedTable LEA rip-relative censuses in `outputs/2026-10-08-l26-package-B/lineage/SeedTable.h` (slot 0 g_uobject_array 1565 lea/mov sites; slot 2 ctor census 10284/5/2/18/2 sites; slot 4 SCO 1918 sites — offline census over the console image, "validated in-game"). | GF #2: no UE4 anchor packs authored yet (first UE4 game would run unpinned); Stellar Blade eboot acquisition (user's copy, decrypt-off law) before its dry-run. | Author the UE4 anchor packs (Phase B item) and re-run the classifier against them on the Mac. |
| 3 | Code patching | **pointer/table hooks console-proven; instruction patching design-only** `[RELABELED 2026-10-10 Claude order; GV]` | Console-proven (pointer/table), `evidence/claude2-menu-session-20261010.log`: `[2026-10-10 00:53:08] [R3] FuncPtr-marshal event carrier armed: [n: F7BD] @0x0000003000AA8BD0 (callback id 1)` (+ callbacks 2, 3; event carrier scan armed), then `[l22] ExecuteInGameThread queued/drained/callback returned` families — the pump fired end-to-end; menu opened in game (`CLAUDE-MENU-UP-SPAWN-BLOCKED.md` §1/§7.6, F7 rerun 6/6). Instruction-level: same log — `[R3] refused: EngineTick pre-callback registrations … (detour registration compiled out under R3 marshal mode)` and `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal`. **[GV RegisterHook confirmation]** The port's `RegisterHook` IS the proven pointer-swap method, not instruction patching: `typescan-r4/candidate/src/LuaMod.cpp:5705-5775` resolves the UFunction (`StaticFindObject` :5750) and for `FUNC_Native` calls `unreal_function->RegisterPreHook/RegisterPostHook` (:5767-5768) → `UFunction::GetFunctionHookData()` (`ue4ss-unreal-native-build-1/port/unreal/src/UFunction.cpp:78-89`) which on first hook SAVES the original `Func` and swaps the UFunction object's `Func` FIELD to `&Internal::UnrealScriptFunctionHook` (`SetFuncPtr`, UFunction.cpp:86) — a data member write; the engine's own dispatch then calls the hook (`UFunctionStructs.cpp:163`), and `UnregisterAllHooks` restores the saved pointer (UFunction.cpp:121-132). Full hook-kind inventory: see the "#3 hook-kind inventory" section below. VMT pump caveat retained from GO's audit: `[EU-VMT] arming refused: non-text-ent (R3 carriers remain the pump)` — the pump never fired on console; Mac host-fixture-proven only (`agent-EU-vmt-pump-selftest.md`). | Instruction-level blocked: PLH x64Detour detours soft-refuse on this target — every live game-text touch is mediated by the owned-memory registry (prologue read returns nothing, `ZydisDisassembler.cpp:65`; `hook()` returns false at the empty-insts check, `x64Detour.cpp:355-358`, before any write — LuaMod.cpp:8533-8558) and under R3 marshal the registrations are compiled out/deferred (:8594). EU VMT pump blocked by the non-text entry in the copied table extent (loud refusal, by design). | Instruction rung: needs the Phase B L3 write channel (`SYS_mdbg_call 573`, hooked-but-passthrough in v1.11 — VERIFIED §3 #3) with write→verify→mprotect receipts. For the VMT pump: whitelist the non-text vtable entry that caused the arming refusal and re-arm in a Claude-reviewed launch (data-level fix, no new authority). |
| 4 | Early loading | **partly proven until shown against engine milestones (before first LoadMap)** `[RELABELED 2026-10-10 Claude order; GV]` | `PHASEC-EARLY-LOADING-DESIGN.md` §5 line 147: `**#4 (early loading) = SATISFIED-ALREADY.**` Timestamp chain verified: eboot-monitor (preserved at `evidence/tmp-preserved-20261010/tmp/l25-variant-A/eboot-monitor-20261010T051653Z.log` line 5): `2026-10-10T05:52:31Z TRANSITION eboot.bin [] -> [1271]` (create bounded (05:52:16Z, 05:52:31Z] = local (00:52:16, 00:52:31]); runtime `[BUILD] id=l26-20261010T043709Z` at 00:53:05; `[Seeds] slot 1 fname pool … admitted` + `slot 2 FName ctor bound` + `[Seeds][L26] engine delete-listener ops admitted: add=0x8841A0 remove=0x884240` all at 00:53:06; mod `Loaded v1.0` 00:53:08; `24 spawnable pickups found` 00:54:35; DRY ladder 00:55:14 (claude2 log). **[GV relabel rationale]** What the receipts prove is residency during engine init in-process; they do NOT timestamp the load against a specific engine milestone (before first LoadMap) — and "injected before eboot's first instruction" remains mechanism-proven only (GO's own caveat). Hence partly proven, not console-proven standalone. | Evidence gap, not a parity blocker: no receipt places module admission before first LoadMap / a named engine milestone. | The 3-pass observe-only test using existing frozen-build instrumentation (module_start_entered t_ms vs the monitor bound), no new build — upgrades the row to console-proven against milestones. |
| 5 | Debugger (crash symbolization) | **crash-log symbolization, Mac-tested** (narrowed from live debugger) `[RELABELED 2026-10-10 Claude order; GV]` | `phasea-tools/phasea-test-receipt.json` (same 18/18 receipt): `klog-blocks-found blocks=2`, `klog-eboot-base-law bases=['0x400000']`, `klog-eboot-build-id 04bb4330…`, `klog-rip-in-libkernel`, MAP-RECONSTRUCTED tier 97.1% .text. Independently re-run BY THIS AUDIT (read-only): `python3 phasea-tools/symbolize_crash.py --klog evidence/tmp-preserved-20261010/tmp/l25-variant-A/klog-20261009-021938.log` → 2 SIGSEGV blocks parsed; RIP `0x80000224c` → `/U2YeYl0w0W/common/lib/libkernel.sprx` base `0x800000000` + offset `0x224c` with module fingerprint; eboot module base `0x400000` + build-id `04bb4330…` (real console crash capture, Oct 9 session). **[GV relabel note]** the capability as evidenced is post-hoc symbolization of captured crash klogs by a Mac tool; no live/debug-attached debugging (breakpoints, stepping, live inspection) exists in the receipts — hence the narrowed label. | GF #5 verdict "needs-X": no final-link symbols sidecar — today's `runtime.map` is the `-r` partial-link map (all VMA=0, 207,852 sections); PRX carries only 2 dynsym anchors; no minidumps exist on PS5 (CE-108255-1 is a UI label only). | Have the Packager emit a final-link symbols sidecar (or unstripped ELF) per build — moves RIP resolution from MAP-RECONSTRUCTED to EXACT. |
| 6 | Controller (kernel input route / userspace input) | **kernel route declined-by-design; userspace PS5Pad+ScePad console-proven** | Decline: `PHASEB-PLAN.md` §3.3 (line 251): `Capability #6 — controller kernel route: ASSESSMENT = userspace ScePad is sufficient; kernel route DECLINED` (kekcall table has no pad/input numbers; IDT hooks are debug-register intercepts; PC UE4SS is userspace input too — parity needs no kernel route; revisit trigger documented). Userspace console proof: `evidence/tmp-preserved-20261010/tmp/zzdiag2-diag.jsonl` — `tag":"pad_l3r3_edge","value":118` → `tag":"pad_l3r3_f7_dispatched","value":118` (118 = F7 keycode; chord edge → subscribed → dispatched); `PHASEB-PLAN.md` §3.3: "console-proven across sessions (menu-up session used it)"; claude2 log: F7 keybind registration (`[l22] RegisterKeyBind returned key=118`) + live keybind callbacks (`[l22] keybind callback entry key=40/key=13` during menu navigation); menu opens via F7 (CLAUDE-MENU-UP §1, incl. F7 rerun). **[GV confirmation — mapped combos trigger no game actions]** (a) Reserved-button law: `keymap/MAPPING.md` §2 reserves game controls (`OPTIONS` = game pause, "proven tonight: l12 deliberately returned OPTIONS to the game; stay unbound forever"; face buttons/d-pad = game actions) and pins `L3+R3` as the universal mod-menu chord with "no other row may use L3 or R3 in any combination" — chords are rising-edge conjunctions dispatched on the UE4SS update thread, exactly one key event per completion (MAPPING.md §1). (b) Empirical: L3+R3 fires only mod behavior in every retained session — `zzdiag2-diag.jsonl` seq 1114-1117 (`pad_l3r3_edge` → `_allow`/`_subscribed` → key 118) and the menu-up session used it with no game-side action observed (PHASEB-PLAN §3.3 "console-proven across sessions"; CLAUDE-MENU-UP §1/§2 6/6 twice). (c) F7 is pad-mapped only: keycode 118 exists on console solely as the L3+R3 dispatch — no physical F7 key; `[l22] keybind callback entry key=118 ref=4` (claude2 log) is the pad bridge. (d) FV's generator (`keymap-gen.py` + `GENERATED-BASELINE.ini`) stays Mac-only until a generated map is actually used on console — the live 3,718 B console profile is the pinned one; LADDER-V2 §1.3.1: "GENERATED-BASELINE.ini remains an unused staging candidate". | None for parity (declined route is the design; revisit trigger recorded in §3.3). | None required. If ever revisited: only via the documented §3.3 trigger (new kernel-resident code = L4, prohibited this phase). |
| 7 | File overlay (pak/io-store mod staging) | **console-proven for ONE mod in ONE game; method changes a game index file** `[RELABELED 2026-10-10 Claude order; GV]` | `.superpowers/native-observations/leon-index-overlay-trial-2026-10-01/result.json`: `"phase": "in_game_leon_appearance_confirmed_by_user"`, `"mod": "Leon Townfall 2K v1.1"` (ONE mod), `"title_id": "PPSA33286"` / game root `Silent Hill Townfall` (ONE game — UE 5.6 per scanner row #2, NOT a UE4 title), `"gameplay_effect_confirmed": true`, `"user_observation": "Leon appears; game runs normally"`, `"base_ucas_modified": false`; staged hashes `"derived_index_sha256": "3ff1946c…"`, `"sidecar_sha256": "27c7b50a…"`; journal `console-transaction.json` + `promotion-review.json` (approved measured FTP stage, rollback attested). Method = the game's ORIGINAL `.utoc` index REPLACED by the derived index (`profile.json` original_index path `…/content/paks/pakchunk0-ps5.utoc`; backup retained; base UCAS untouched) — a game index file is changed, not an additive/runtime mount. This audit re-hashed the retained sidecar `.superpowers/native-dependency-spike/leon-iostore-adapter-trial-1/overlay/derived/pakchunk0-ps5_s1.ucas` → `27c7b50a…` — stage == receipt == retained bytes. CAVEATS (stated exactly): `"potential_material_texture_artifact": "pale checkerboard pattern on glasses", "cause_confirmed": false`; `"all_rendering_verified": false`; the screenshot path recorded in result.json is PURGED — the visual record exists only as the JSON observation fields. Broader-label needs: explicit checklist in the "#7 broader-label checklist" section below. | Generality unproven: additive/runtime engine-mount form not run (Phase 0 mount-acceptance observation; kernel virtual files = Phase B dependency, GF #7 "needs-X"); one mod / one game / index-replace only. | One Claude-reviewed Phase 0 mount-acceptance launch with game-identical `0xE0000000` pak records on the kernel-free engine-mount path (0x176ec30) per PAK-RUNTIME-MOUNT-DESIGN §4 — plus the #7 broader-label checklist items. |
| 8 | Asset conversion (PC→PS5 textures/meshes) | **design-only / components** | `PHASEA-PLANS.md` §#8 verdict (line 437): `texture path ready-to-build … mesh path blocked-on-platform-cook research`; swizzler (AMD GFX10 `SW_64KB_S`) specified but not ported; container stages exist + tested (repak loose-file roundtrip, cited inside §#8). The Leon trials prove staging, NOT conversion: `PHASEA-PLANS.md` "the team staged the **Leon 2K PC mod unchanged** on console"; sidecar sha `27c7b50a…` verified byte-stable by this audit (row 7). | Texture path: swizzler port + deswizzle roundtrip oracle not written. Mesh path: platform-cook semantics unresearched (P3); Oodle absent in captured corpus (all None-compression — refuse bundling one silently). | Port the GFX10 `SW_64KB_S` per-mip swizzler with the CUE4Parse-deswizzle roundtrip oracle (GF #8's single ready-to-build action). **[GV STANDING POLICY — texture pipeline PAUSED]**: the swizzler/oracle action is ON HOLD until a real mod demonstrably fails without it — Leon's PC textures ran UNCONVERTED and the game ran normally (result.json row 7: "Leon appears; game runs normally"; the only anomaly candidate is the glasses checkerboard, `cause_confirmed: false`, and GM's file-level verdict names PC-only SHADER platforms (materials), not textures — `shader-fallback/fallback.md` §1d). Active refocus = the PC-only-shader MATERIAL fallback (GM, `shader-fallback/fallback.md` Part 2 + `classify_materials.py`): derived overlay entries generated at INSTALL time pointing at EXISTING game shaders (Option A: keep the base store entry with its PS5 hashes; Option C: known-good base invisible material), originals untouched ("original mod files are never modified"), Option B (Sony-licensed SP_PS5 cook) REFUSED — no Sony shader tools, ever. Full policy text in the "#8 standing texture-pause policy" section below. |
| 12 | Shader translator (PC shader effects → exact PS5 rendering) | **design-only** | `outputs/2026-10-08-l25-prep/CAP12-SHADER-DESIGN.md` (HF, 2026-10-10): pipeline DXBC/DXIL → SPIR-V (dxil-spirv MIT, pinned `8434b063…`; vkd3d-shader LGPL-2.1, pinned `22307558…`) → RDNA2 code (Mesa ACO MIT raw-file pin / LLVM Apache-2.0 fallback) → the game's own PS5 shader package format, learned ONLY from shipped shaders; tool pins + sha256s in `outputs/cap12-tools/PINS.json`. Local proof substrate: base pakchunk0 utoc census = 2 type-8 ShaderCodeLibrary manifest chunks (1.8 MB at ucas offsets 0/393216) + 2,579 type-9 ShaderCode chunks (156 MB) + 5,387 store hash refs over 1,500 unique maps; ZERO store-hash/chunk-id prefix matches ⇒ IoHash-keyed v8 library whose SHA-1→code map lives in the type-8 manifests (the M1 acquisition target, ONE decrypt-off bounded fetch <10 MB, user's own game); CityHash64 port verified 6/6 against container ids (`cap12-tools/cityhash64.py`); `FShaderCodeLibrary::OpenLibrary` present in the shipping eboot. Milestones M1 decode+byte-identical rebuild (Mac) → M2 trivial-shader translate (Mac) → M3 in-game render (console, 1 gated launch, rides existing ladder) → M4 emissive then Leon set; L1/L2 fallback law until M4; install-time derived-overlay output only, originals untouched. | Base shader bytes not local (128 GB ucas absent — the M1 fetch session is the gate); type-8 manifest format undecoded; PS5 packaging delta from gfx1030-class ISA unknown until M1; engine/console verification behavior unknown pending M3's single-launch observation. | Run the one bounded manifest+sample fetch (1b), decode the type-8 manifest, and attempt the M1 byte-identical rebuild — all Mac-side; console only re-opens at M3 with Claude review + user OK. |

---

## Count summary (honest classes — after GV relabels, 2026-10-10)

- **console-proven (standalone): 1** — #7 file overlay (ONE mod in ONE game,
  index-replace method, with stated caveats).
- **partly proven: 1** — #4 early loading (residency-during-init receipts
  solid; milestone-relative proof pending).
- **split rows (one line each, both halves evidenced): 2** — #3 code patching
  (pointer/table hooks console-proven; instruction patching design-only);
  #6 controller (kernel route declined-by-design; userspace console-proven).
- **Mac-tested: 2** — #2 scanner (18/18 tool receipt; primitives
  console-receipted); #5 crash-log symbolization (18/18 incl. klog checks;
  real console crash klog parsed — tool itself Mac-only; narrowed from
  "live debugger").
- **design-only: 2** — #1 kernel reads (scope confirmed at the verified v1.11
  surface; kernel-VA content reads unavailable — R4 refuted); #8 asset
  conversion (texture path PAUSED per standing policy; material-fallback
  refocus active).
- **design-only (new cap, not receipt-audited): 1** — #12 shader translator
  (HF 2026-10-10; row appended after the GO audit — local-evidence-backed,
  no console/runtime claims; M1-M4 ladder + L1/L2 fallback law in the row).

No row claims more than its receipts. Gameplay remains a prototype (spawn
configure blocked at the ItemId reverse read — CLAUDE-MENU-UP §3); nothing in
this table asserts game-completion.

---

## R0 first-rung spec — capability #1 (agent GV, per Claude order)

The first rung of the read ladder: **no game running**, read a KNOWN value.
Everything below is design; nothing runs without Claude review + explicit
user OK for that exact step (PHASEB-PLAN §0.1).

1. **Preconditions.** Console at dashboard, no game process (`processes_list`
   eboot count 0 — LADDER-V2 §4.0 style); stock kstuff-lite v1.11 resident
   (etaHEN `state=1 readable=1 crypt=deb7/deb7`); klog tap on TCP 3232 to a
   NEW file; our payload loaded once via the Z2 pldmgr procedure; saves
   backed up; one user-authorized action per boot.
2. **The known-value read: firmware version, via the exact call kstuff
   itself uses.** `r0gdb_get_fw_version()` (VERIFIED pin;
   `prosper0gdb/r0gdb.c:1738-1745`, "kernel_get_fw_version from sb's sdk"):
   `int mib[2] = {1, 46}; sysctl(mib, 2, &version, &size, 0, 0); return
   version;` then `ver = version >> 16` (`prosper0gdb/offsets.c:107`).
   This is a PLAIN OWN-PID SYSCTL — no kekcall, no kernel mediation
   (PHASEB-PLAN §3.1 R2 class: "none (stock syscalls)") — which is why it is
   the true first rung, below even R1. **Expected known value:** a u32 whose
   `>>16` equals `0x1360` for firmware 13.60 (`offsets.c:174` `case 0x1360:
   set_offsets_1360()`), cross-checked against the console's already-known
   FW (13.60) — a read verified against an independently known answer.
3. **Crash/fault handling (the payload's error model, per VERIFIED §2).**
   - Plain syscalls (the fw-version read): errors return errno normally
     (e.g. EFAULT on a bad pointer) — a userspace error, never a kernel
     fault.
   - kekcall-class calls: success ⇒ retval written to `td_retval`;
     handled-with-error ⇒ errno in rax per the normal PS5 stub convention;
     unknown/unimplemented sub-number ⇒ SILENT fallback to real getppid
     (parent pid) — the gate cannot crash on a wrong sub-number (VERIFIED
     G5). REMOTE_SYSCALL failures surface as EFAULT/ESRCH (VERIFIED §2
     sub-5). CHECK: resident ⇒ 0 (G4 corrected polarity).
   - L0–L2 activity cannot crash the kernel if the ladder is honored
     (PHASEB-PLAN §4.5). Recovery: our payload is userspace — natural exit
     or pldmgr `/process_kill` on OUR pid only; universal rollback = reboot
     (kstuff has no unload, P8). klog watch per §4.4; any kernel
     coredump/panic ⇒ STOP.
4. **Read-only leaves nothing changed (source-cited).** The fw-version
   sysctl reads the kernel's kern table — kstuff itself calls it BEFORE any
   kernel write to fail-closed select its offset table (VERIFIED §5:
   `set_offsets()` returns -1 on unknown FW, notify + exit, zero writes).
   READ_DR copies DR0–DR3/DR6/DR7 out to a 48 B user buffer (uelf/kekcall.c
   sub 1 — reads hardware registers, writes nothing kernel-side); RDMSR
   returns the value in td_retval; CHECK sets args[RAX]=0. REMOTE_SYSCALL is
   as read-only as the syscall chosen for it: upstream's own read paths —
   `find_proc` KERN_PROC_PID enumeration (`main.c:530-545`) and
   `dynlib_get_info_ex` against SceShellCore (`main.c:1253-1257`) — touch no
   target state. The remote `mmap/mprotect` write family (`main.c:1056-1083`)
   is EXCLUDED from every read rung.
5. **DONE law (written into this rung): DONE = 3 passes across restarts,
   never one session.** PASS = fresh boot → residency verified read-only →
   payload loaded → fw-version sysctl returns the 0x1360-class value →
   payload exits at its bound → klog clean. DONE only after 3/3 identical
   passes across 3 separate boots (PHASEB-PLAN §4.6 "3 console passes across
   reboots"; §2 P5 3-pass law; LADDER-V2 §3.4). Any variance ⇒ STOP, report;
   the rung is not accepted. Only then does R1 (KEKCALL_CHECK, expect
   return 0 = resident) open, then R2 (own-pid 608 / KERN_PROC), then R3
   (REMOTE_SYSCALL — VERIFIED §6 R2 rungs (a)/(b)/(c)).

---

## #3 hook-kind inventory — what works without Phase B vs what needs instruction patching (agent GV)

From the port's code (l26 candidate tree `outputs/2026-10-08-l26-runtime/
typescan-r4/candidate/src/`, dependency port `.superpowers/
native-dependency-spike/ue4ss-unreal-native-build-1/port/unreal/src/`):

**Work WITHOUT instruction patching (pointer/table/registration methods):**

1. **UFunction `Func`-pointer swap** (native UFunction hooks; what Lua
   `RegisterHook` does for `FUNC_Native`): `LuaMod.cpp:5705-5775` →
   `UFunction::RegisterPreHook/RegisterPostHook` (`UFunction.cpp:91-101`) →
   `GetFunctionHookData()` (`UFunction.cpp:78-89`) saves the original `Func`
   and swaps the UFunction object's `Func` FIELD to
   `&Internal::UnrealScriptFunctionHook` (`SetFuncPtr`, UFunction.cpp:86);
   the engine's own dispatch calls it (`UFunctionStructs.cpp:163`);
   `UnregisterAllHooks` restores the saved pointer (`UFunction.cpp:121-132`).
   Console-proven: the R3 FuncPtr-marshal event carriers ARE RegisterPostHook
   carriers (armed 3x in the menu-up session; 145 ExecuteInGameThread
   drain-family lines).
2. **Non-native (script/BP) UFunction hooks**: `LuaMod.cpp:5776-5793` stores
   callbacks in `m_script_hook_callbacks`, dispatched from the
   ProcessInternal-replacement flow (execute site `LuaMod.cpp:8464`
   `execute_hook(LuaMod::m_script_hook_callbacks, true)`; unhook path
   :2389-2399). Rides UE4SS's internal-dispatch replacement — no code bytes
   touched.
3. **Engine listener registration** (GUObjectArray create/delete listeners):
   `UnrealInitializer.cpp:203-204, 600` (`AddUObjectCreateListener` /
   `AddUObjectDeleteListener` — the engine's own listener TArray API, pure
   data appends). Console-proven: `[Seeds][L26] engine delete-listener ops
   admitted: add=0x8841A0 remove=0x884240` (claude2 log).
4. **VMT/vtable swap** (EU FIX4 tick pump, `VmtTickPumpCore.inc`): copy the
   vtable extent, replace slot 0x310 (UEngine::Tick), ONE aligned 8-byte
   vptr store; "Data-only and XOM-safe: no code byte is ever written or
   read". MAC HOST-FIXTURE-PROVEN ONLY — console arming refused (non-text
   entry in the copied extent); loud refusal by design.

**NEED instruction patching (attempted today, soft-refused on this target):**

- **Native function detours via PLH x64Detour** — the eager detour arms:
  EngineTick/ProcessEvent game-thread pumps and the LoadMap pair
  (`RegisterLoadMapPreCallback` → `Hooks.cpp:211` → `DetourInstance
  AddCallback` → `EnsureHooked` → `InstallHook` → `PLH::x64Detour::hook()`,
  measured routing documented at `LuaMod.cpp:8533-8558`). On console: under
  R3 marshal mode the registrations are compiled out/deferred (":8594" log
  line; session log "detour registration compiled out under R3 marshal
  mode"), and even when attempted every live game-text touch is mediated by
  the owned-memory registry — prologue read returns nothing
  (`ZydisDisassembler.cpp:65`), `hook()` returns false at the empty-insts
  check (`x64Detour.cpp:355-358`) BEFORE any write attempt. The LoadMap
  deferred arm (`try_deferred_loadmap_arm`) is the one live attempted
  instruction-detour arm in the current build. Enabling instruction patching
  for real is Phase B L3 territory (`SYS_mdbg_call 573` write channel; GP:
  hooked-but-passthrough in v1.11; x86-64 needs no i-cache invalidation —
  VERIFIED §3 #3).

---

## #7 broader-label checklist — what "file overlay" still needs (agent GV)

The current label is console-proven for ONE mod in ONE game via index
replacement. For the broader "file overlay works" label, ALL of these remain:

- [ ] **3 passes across restarts** — the Leon trial is a single-session
  observation (result.json phases staged → in-game confirmed; no repeated
  relaunch receipt). The mission's 3-restart law (PHASEB-PLAN §4.6;
  LADDER-V2 §3.4) has not been applied to the overlay form.
- [ ] **Several mods at once** — co-residency of multiple overlay mods in one
  loadlist/index (the multimod question; LADDER-V2 §1.3 is the analogous
  Lua-side precedent, not an overlay one).
- [ ] **Uninstall proven on console** — rollback is ATTESTED in
  `promotion-review.json` (backup retained) but no receipt shows the
  original `.utoc` restored and the game verified unmodded on console.
- [ ] **A UE4 game** — the one proof is Townfall (UE 5.6, scanner row #2);
  a UE4 title would exercise different io_store/container generations.
- [ ] (already-tracked blocker) additive/runtime engine-mount form — the
  Phase 0 mount-acceptance launch per PAK-RUNTIME-MOUNT-DESIGN §4.

---

## #8 standing texture-pause policy (agent GV, per Claude directive)

**The texture-conversion pipeline is PAUSED until a real mod demonstrably
fails without it.** Rationale: Leon's PC textures ran UNCONVERTED and the
game ran normally ("Leon appears; game runs normally", result.json row 7;
`PHASEA-PLANS.md`: "the team staged the Leon 2K PC mod unchanged"). The one
visual anomaly candidate (glasses checkerboard) is `cause_confirmed: false`
and GM's file-level forensics attributes it to PC-only SHADER platforms
(materials), not texture formats (`shader-fallback/fallback.md` §1b/§1d —
STRONGEST-HYPOTHESIS, not confirmed).

**Active refocus: the PC-only-shader MATERIAL fallback (GM, in flight).**
Point mods at EXISTING game shaders instead of converting anything:

- Generated AT INSTALL TIME as derived overlay entries (GK's overlay builder
  carries them; the analyzer only emits directives — fallback.md Part 2 law).
- Option A: substitute the base game's equivalent export — keep the base
  store entry WITH its PS5 shader hashes (suppression, not bytes).
- Option C: hide via a known-good base invisible material (candidates
  verified present with PS5 store hashes, e.g. `M_FluxSurfaceInvisible`).
- **Originals untouched**: "original mod files are never modified" —
  substitutions are derived overlay entries only; revert = row removal.
- **No Sony shader tools**: Option B (cooking a real SP_PS5 shader map)
  is REFUSED on licensing and integrity grounds (Epic's console shader
  compiler is Sony-SDK-gated; a synthesized map would be fabrication) —
  not deferred, refused.

Resume trigger for the texture pipeline: a real mod fails with textures
unconverted AND the material fallback cannot rescue it — then the GFX10
`SW_64KB_S` swizzler + CUE4Parse-deswizzle roundtrip oracle work reopens.

---

## kstuff-lite pin event (verified BOTH sides by this audit)

- **Pinned file:** `outputs/kstuff-lite-pinned/kstuff-lite-v1.11.elf`
  (mtime 2026-10-10 16:03, alongside `kstuff-lite-v1.11-source.zip`).
  `shasum -a 256` (computed by this audit) =
  `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779`.
  File magic `7f454c46` (plain ELF — the distribution form of a GitHub-release
  payload; not a console decrypt-law artifact: this copy was not fetched from
  the console ftpsrv).
- **Console-side mirror citation (the other side):** pldmgr catalog
  `outputs/2026-10-04-zcode-team/worker-boot-protocol/probe/list_payloads.json`
  — for `/data/pldmgr/payloads/kstuff-lite/kstuff-lite_v1.11.elf`, version
  `v1.11`, `"checksum": "ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779"`,
  recorded URL `"https://github.com/itsPLK/ps5-payloads-mirror/releases/download/payloads-mirror/kstuff-lite_v1.11.elf"`
  (the PLK payloads-mirror of **EchoStretch/kstuff** releases, per
  `FZ-PHASEB-KSTUFF-LITE.md` §1 "Upstream" row, which cites the same full hash
  [META]). **MATCH: release binary sha == console pldmgr mirror sha.**
- **GD's citation:** `agent-GD-phasebc.md` gap 1 quotes the mirror hash prefix
  `ab9a6cb4…`; `PHASEB-PLAN.md` §2 P1 quotes the full hash with the same
  [META] provenance. Both check out against the catalog file.
- **Caveats (exact):** the pinned directory contains no separate pin-receipt
  document, and the fetch's own provenance (which URL the pinned copy was
  downloaded from) is not independently receipted — identity is established
  hash-vs-catalog only. The §2 P4 byte-identical REBUILD from the source zip
  has not been run, so source<->binary identity is unproven; this is why row #1
  stays design-only (interface pinned from docs + family source; binary
  inventory pending).

---

## Audit method note

Read-only throughout: file reads, `shasum`, `grep -a` (the claude2 session log
is NUL-interleaved — binary-safe grep required), and one stdout-only execution
of `symbolize_crash.py` against a preserved klog. No builds, no console
contact, no tree edits other than this document and
`agent-GO-capability-audit.md`.

**GV revision method note (2026-10-10):** equally read-only — file reads and
`grep` only (plus python used purely as a text editor for the long table
rows). Code citations verified directly in the l26 candidate tree and the
pinned v1.11 source; console facts come exclusively from the retained
receipts already cited. No builds, no console contact, no other file edited
besides this document and `agent-GV-capability-relabels.md`.
