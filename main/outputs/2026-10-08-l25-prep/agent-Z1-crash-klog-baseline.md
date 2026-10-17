# Agent Z1 — PLUGIN-RUN crash baseline from captured klog (for VANILLA comparison)

Date: 2026-10-08. READ-ONLY analysis; this file is the only artifact written.

## 0. Source files examined

| File | Role |
|---|---|
| `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/klog-20261008-173728.log` | PRIMARY baseline. 50,551 lines, 3,149,664 bytes. klogsrv capture started 2026-10-08T17:37:28Z (12:37:28 local, UTC-5), stream closed by peer 19:21:55Z (console reboot), reconnected 19:25:03Z; last write 14:27 local. One continuous boot, 12:37 -> 14:27 local. |
| `.../klog-20261008-171754.log` (4,300 B) | Session-start capture 17:17:54Z; **0 fatal blocks**. |
| `.../harvest-20261008T174239Z-25929d/klog-snapshot.log` (410,710 B, taken 12:42 local) | **0 fatal blocks** — no crashes before the l23 harvest. |
| `outputs/2026-10-08-l22-package-r2/.../klog-20261008-155359.log`, `outputs/2026-10-08-menu-complete-package/.../klog-20261008-141317.log` | Earlier sessions today; **0 fatal blocks** each. |
| `/tmp/l24-aligned.log` (47,935 lines) | modbridge-runtime-aligned-entry/v1 trace, build `f6530c7905934e5a8cc201aa9400ebb0`. NOT a klog. Zero kernel fatal content — its 3,090 "copyin" hits are the plugin's own `pool_copyin_probe_*` telemetry (`pool_copyin_probe_rc value=0xffffffff`, `errno=0x00000002` — the GATE-1 short-write oracle), not kernel `copyin: has nonsleeping lock`. |

**No klog from today's l24 runs exists as a separate file.** No `klog-*.log` under any `2026-10-08-l24-*` dir. However the primary klog's capture window (12:37–14:27 local) *contains* the l24 crash minutes: block 6 lands 13:47:16–13:47:46 local and block 8 lands 14:07:16–14:07:47 local — exact matches for the reported 13:47 / 14:07 l24 crash sessions, and their signature is byte-identical to the other blocks (see §5). There is exactly ONE plugin-run crash signature today.

Wall-clock calibration: onion_daemon uptime epoch T0 = 17:35:45.6Z, pinned by atime anchors `L231 [atime: 17:36:52.155]` (28 lines after onion `[66.504]`) and `L3474 [atime: 17:39:31.146]` (9 lines after onion `[225.491]`) — both agree to 0.1 s. Local time = UTC-5.

## 1. Fatal-signal block index (8 blocks, 7 distinct eboot processes)

Session map: 13 `launchApp(PPSA33286)` (L19, 1759, 3320, 13496, 16070, 17511, 19473, 24394, 27268, 31527, 36753, 40907, 43754); 7 processes crashed (8 blocks — pid 167 crashed twice), 6 exited cleanly. Every launch is followed by a paired `launchApp(NPXS40093)` (save-dialog system app).

| # | Banner L | Extent (incl. lib list) | pid | tid | Thread name | rip | Reason | Wall clock (local) | Session lifetime |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 15097 | 15096–15356 | 144 | 101937 | *(empty)* | `0000000080004920` | abort is called | 13:38:16–13:38:47 | ~90 s |
| 2 | 18357 | 18356–~18612 (interleaved) | 167 | 102206 | `RTHeartBeat 0` | `00000008000010ec` | SIGSEGV 11, GPU Protection fault, VA `0x0000000c80004000` | 13:40:09–13:40:16 | ~23 s |
| 3 | 18620 | 18619–18652 (truncated, 2 libs) | 167 | 102200 | `SceShareLibContentEventThread` | `000000080000224c` | exception `0xa002031f` SYSTEM_INTERNAL_SERVICE_CALL_ERROR | 13:40:09–13:40:16 (cascade of #2) | — |
| 4 | 22470 | 22469–22729 | 178 | 101881 | *(empty)* | `0000000080004920` | abort is called | 13:41:46–13:42:16 | ~90 s |
| 5 | 25979 | 25978–26238 | 193 | 100650 | *(empty)* | `0000000080004920` | abort is called | 13:45:16–13:45:46 | ~60 s |
| 6 | 30503 | 30502–30762 | 208 | 101547 | *(empty)* | `0000000080004920` | abort is called | **13:47:16–13:47:46** | ~90 s |
| 7 | 42529 | 42528–42788 | 251 | 102439 | *(empty)* | `0000000080004920` | abort is called | 14:04:46–14:05:16 | ~60 s |
| 8 | 45404 | 45403–45663 | 266 | 102053 | *(empty)* | `0000000080004920` | abort is called | **14:07:16–14:07:46** | ~60 s |

Timestamp anchors used (onion_daemon `[uptime]` lines bracketing each banner; atime lines flanking sessions):
B1 `[3750.930]`L15095 / `[3780.933]`L15972; atime L13690 18:37:02Z, L16215 18:39:46Z — B2/B3 `[3864.020]`L17728 / `[3870.940]`L18934; atime L17674 18:40:09Z, L19621 18:40:25Z — B4 `[3960.951]`L22458 / `[3990.952]`L23320 — B5 `[4170.960]`L25821 / `[4200.962]`L26781; atime L24539 18:44:22Z — B6 `[4290.969]`L30201 / `[4320.971]`L31433; atime L27419 18:46:06Z, L31674 18:52:13Z — B7 `[5341.014]`L42374 / `[5371.015]`L43085; atime L41057 19:03:55Z — B8 `[5491.020]`L45273 / `[5521.022]`L46208; atime L43908 19:06:22Z. APP_CRASH syscore events follow each block (Push/Pop at 15366/15367, 18616/18618 + 18872/18876, 22732/22733, 26247/26248, 30767/30769, 42793/42794, 45668/45670).

**Binaries identical in every block** (from each dump's own lib list):
- `/app0/eboot.bin` xotext `0x400000:0x787c000`, fingerprint `04bb4330a7d71dcefe1cc2c4d19769d500000000`
- `/app0/sce_module/libc.prx` `0x80000000:0x80118000`, fp `0d72e63fb015cb5fffbc2f5162fbe21500000000`
- `/data/plugins/modbridge_supplier_composition.prx` `0x80a8c000:0x81cb4000` (~19 MB), fp `ffad2cd5674725e2ce744b8089bf33d900000000`
- common `libkernel.sprx` fp `dc088297572186a79f13c8e227026d0d00000000`

All 8 crashes occurred 13:36–14:08 local — i.e. after l24 packaging completed (13:28) and after the l23 harvest (12:45, whose snapshot shows 0 fatals). The eboot+plugin fingerprints never change across the window, so whether a given relaunch followed an l24 upload or not, the loaded binary set is constant throughout: this file is a valid single-signature PLUGIN-RUN baseline.

### Backtrace of the 6 abort blocks (identical in all six; module attribution via the dump's own xotext map)

```
rip 0x80004920            -> libc.prx+0x4920                      (abort)
#000000000040f918         -> eboot.bin+0xf918                     (abort import thunk)
#00000000005e3ad8         -> eboot.bin+0x1e3ad8
#0000000000575197         -> eboot.bin+0x175197
#00000000004b2379         -> eboot.bin+0xb2379
#00000000004b4b75         -> eboot.bin+0xb4b75
#0000000080b2d36b         -> modbridge_supplier_composition.prx+0xa136b
#0000000080b81f80         -> modbridge_supplier_composition.prx+0xf5f80
#00000000818b6840         -> modbridge_supplier_composition.prx+0xe2a840
#0000000080ad6392         -> modbridge_supplier_composition.prx+0x4a392
#0000000080ad4a14         -> modbridge_supplier_composition.prx+0x48a14
#0000000080ad486f         -> modbridge_supplier_composition.prx+0x4886f
#0000000080b5068a         -> modbridge_supplier_composition.prx+0xc468a
#0000000080b4bdd3         -> modbridge_supplier_composition.prx+0xbfdd3
#0000000080db89fe         -> modbridge_supplier_composition.prx+0x32c9fe
#0000000080db7ded         -> modbridge_supplier_composition.prx+0x32bded
#0000000081c0b4b7         -> modbridge_supplier_composition.prx+0x117f4b7
#0000000081c0e094         -> modbridge_supplier_composition.prx+0x1182094
#0000000081c0fc52         -> modbridge_supplier_composition.prx+0x1183c52
#0000000081c0fbde         -> modbridge_supplier_composition.prx+0x1183bde
#0000000081c0f9b0         -> modbridge_supplier_composition.prx+0x11839b0
#000000080000763d         -> common libkernel.sprx+0x763d          (thread entry)
#0000000000000000         -> terminator
```

## 2. Activity correlation per block

Every crashed session ran the full save-data sequence (SaveGame + ProfileData + DisplaySettings, api_id 90 mount / 88-91 commit / 92 unmount, 2,016 libSceSaveData lines total in file) before dying:

- **B1 (pid 144)**: last save event = DisplaySettings update+unmount+`term` (L14297–14301); then ~790 lines of quiet (SYSCTL `kern.proc.pathname`/`security.jail.jailed` probe noise, BFS churn, SceShellUI FirstDraw) -> abort. Menu-idle/quiet-period pattern.
- **B2 (pid 167)**: SIGSEGV **5 lines after** DisplaySettings api_id 92 unmount + `[PFS] umount finished` (L18353) + `dbggc_push_only_protection_fault_info_to_user` (L18355) — crash **during save-data teardown**, on the render heartbeat thread. Dump interleaved with SceCdlgApp save-dialog unload and `sceApplicationSuspend` (L18438–18444).
- **B3 (pid 167)**: cascade, not an independent fault: while mDBG sends signo 0xb to the already-faulted tid 102206 and `[IPMIMGR] suspendPoint timeout` fires, `SceShareLibContentEventThread` raises 0xa002031f (L18613–18617). Two APP_CRASH events for this pid.
- **B4 (pid 178)**: SaveGame mount/unmount done at L20764; ~1,700 lines of BFS-only churn with no other game events -> abort. Menu-idle pattern.
- **B5/B7/B8 (pids 193/251/266)**: SaveGame mount+unmount **50–75 lines before** the abort — crash within seconds of save-data load/unmount completing.
- **B6 (pid 208)**: same SaveGame pattern with a longer gap (L28556 -> L30503); pre-crash lines are only BFS refcnt transitions.
- In all abort blocks the six lines before the banner are BFS refcnt churn (GPU buffer-fence transitions) — the abort is the first anomaly; no preceding error line.

Summary: 7 of 8 blocks tie to save-load/teardown timing; none coincide with app start (all sessions lived 23–90 s to crash); none with clean exit (those sessions show `onAppExit` without APP_CRASH).

## 3. Primary CE-108255-1-style block — verbatim (L15096–15155)

```
#
# A user thread receives a fatal signal
#
# reason:  abort is called
# thread ID: 101937
# thread name: 
# proc ID: 144
# proc name: eboot.bin
#
# registers:
# rax: 0000000009758a0c  rbx: 00000000097bd298
# rcx: 0000000000000000  rdx: 00000031e8f80008
# rsi: 0000000000004b42  rdi: 0000000000000000
# rbp: 00000007ed7f10b0  rsp: 00000007ed7f10b0
# r8 : 0000000000000050  r9 : 000000000000001e
# r10: 0000000009709830  r11: 00000000097089b8
# r12: 0000001000698364  r13: 00000007ed7f11f0
# r14: 00000008000751d0  r15: 0000001780509000
# rip: 0000000080004920  eflags: 00000246
# lbf: ffffffffffffffff  lbt: ffffffffffffffff
# lb0: ffffffffffffffff  lb1: ffffffffffffffff
# lb2: 0000000800001c80  lb3: 000000080002af10
# lb4: 000000008086ae80  lb5: 000000008083d770
# lb6: 0000000080836199  lb7: 0000000080836180
#
# backtrace:
copyin:  has nonsleeping lock
# 000000000040f918
# 00000000005e3ad8
# 0000000000575197
# 00000000004b2379
# 00000000004b4b75
# 0000000080b2d36b
# 0000000080b81f80
# 00000000818b6840
# 0000000080ad6392
# 0000000080ad4a14
# 0000000080ad486f
# 0000000080b5068a
# 0000000080b4bdd3
# 0000000080db89fe
# 0000000080db7ded
# 0000000081c0b4b7
# 0000000081c0e094
# 0000000081c0fc52
# 0000000081c0fbde
# 0000000081c0f9b0
# 000000080000763d
# 0000000000000000
#
# dynamic libraries:
# /app0/eboot.bin
#  xotext: 0000000000400000:000000000787c000 nsegs: 4
#  fingerprint: 04bb4330a7d71dcefe1cc2c4d19769d500000000
# /YBJS8crmSZ/common/lib/libkernel.sprx
#  xotext: 0000000800000000:0000000800048000 nsegs: 4
#  fingerprint: dc088297572186a79f13c8e227026d0d00000000
# /YBJS8crmSZ/common/lib/libSceLibcInternal.sprx
#  xotext: 00000008000a4000:00000008001cc000 nsegs: 4
#  fingerprint: acfdf1b620da82b65f2b9093d5a0a9c100000000
```
(Lib list continues to L15356; last entry is `/data/plugins/modbridge_supplier_composition.prx`, fp `ffad2cd5674725e2ce744b8089bf33d900000000`.)

**Faulting-thread identity.** The banner "A user thread receives a fatal signal" is the kernel crash-dumper header for a *user-mode* thread (not a kernel thread). The thread name field is empty and the tid (101937) is high-numbered/dynamically allocated — a spawned worker, not a named game thread and not the kernel. Reading the salvaged stack outermost-in: common `libkernel.sprx+0x763d` (thread-entry trampoline) -> 15 frames inside `modbridge_supplier_composition.prx` -> 4 eboot call frames -> eboot abort thunk -> `libc.prx abort` (rip). I.e. the aborting thread is a plugin-context worker executing game code that called `abort()`. Caveat: the dumper itself warns the walk was degraded (`copyin: has nonsleeping lock` — it could not normally copy the user stack because the thread held a spin mutex), so exact frame order is best-effort; but the eboot+modbridge+libkernel-entry module spread is identical across all six abort blocks. B2's thread (`RTHeartBeat 0`) is the game's render heartbeat thread, faulted in `libkernel.sprx+0x10ec` (`# fault address: 00000008000010ec (xotext:"libkernel.sprx"+0x10ec)`, "user read instruction, page not present", raw fault info `0x00100c30`, GPU Protection fault hub:0 vmid: game process1 client:CPG(6) access:Read permission:0x3, VA `0xc80004000`). B3's thread is a system share-lib content event thread that raised 0xa002031f while the dying process was being suspended — collateral, not an origin.

## 4. `copyin: has nonsleeping lock` occurrences (8, one per block)

All sit immediately after `# backtrace:` and before the first frame; format is `copyin: <thread name> has nonsleeping lock` (empty name -> double space). NOTE for grep: a single-space literal like `copyin: has` will MISS the unnamed-thread lines — match `copyin:` and `nonsleeping lock` separately.

| Line | Verbatim | Context (±5) |
|---|---|---|
| 15122 | `copyin:  has nonsleeping lock` | L15117 lb2/lb3 regs … L15121 `# backtrace:` -> L15122 copyin -> L15123 `# 000000000040f918` (first frame) |
| 18397 | `copyin: RTHeartBeat 0 has nonsleeping lock` | L18396 `# backtrace:` -> copyin -> L18398 `#` -> L18399 `# dynamic libraries:` (no frames salvaged) |
| 18645 | `copyin: SceShareLibContentEventThread has nonsleeping lock` | L18644 `# backtrace:` -> copyin -> L18646 `#` -> L18647 `# dynamic libraries:` (no frames) |
| 22495 | `copyin:  has nonsleeping lock` | L22494 `# backtrace:` -> copyin -> L22496 first frame `# 000000000040f918` |
| 26004 | `copyin:  has nonsleeping lock` | L26003 `# backtrace:` -> copyin -> L26005 first frame |
| 30528 | `copyin:  has nonsleeping lock` | L30527 `# backtrace:` -> copyin -> L30529 first frame |
| 42554 | `copyin:  has nonsleeping lock` | L42553 `# backtrace:` -> copyin -> L42555 first frame |
| 45429 | `copyin:  has nonsleeping lock` | L45428 `# backtrace:` -> copyin -> L45430 first frame |

## 5. COMPARISON SPEC — vanilla klog vs this baseline

Baseline hit profile (this file, 50,551 lines):

| Pattern (grep -F unless noted) | Baseline hits |
|---|---|
| `A user thread receives a fatal signal` | 8 |
| `^# proc name: eboot` (grep -E) | 8 |
| `rip: 0000000080004920` | 6 |
| `nonsleeping lock` / `^copyin:` | 8 / 8 |
| `SCE_SYSCORE_EVENT_APP_CRASH` | 16 (8 Push + 8 Pop) |
| `App Crash : PID` | 8 |
| `dbggc_push_only_protection_fault_info_to_user` | 1 |
| `GPU Protection fault` | 1 |
| `0xa002031f` | 4 |
| `modbridge` | 6 (one lib-list entry per full abort dump) |
| `launchApp(PPSA33286)` | 13 (sanity: game launched) |
| `libSceSaveData` | 2,016 (sanity: normal save activity) |

Ready-to-run commands (`V` = vanilla klog path; use -F for patterns with parens):

```bash
V=<vanilla-klog-path>
grep -nF "A user thread receives a fatal signal" "$V"        # baseline 8 ; vanilla expect 0
grep -nE "^# proc name: eboot" "$V"                           # baseline 8 ; expect 0
grep -nF "rip: 0000000080004920" "$V"                         # baseline 6 ; expect 0
grep -nE "^copyin: .* has nonsleeping lock" "$V"              # baseline 8 ; expect 0
grep -nF "nonsleeping lock" "$V"                              # baseline 8 ; expect 0
grep -nF "SCE_SYSCORE_EVENT_APP_CRASH" "$V"                   # baseline 16; expect 0
grep -nF "App Crash : PID" "$V"                               # baseline 8 ; expect 0
grep -nF "dbggc_push_only_protection_fault_info_to_user" "$V" # baseline 1 ; expect 0
grep -nF "GPU Protection fault" "$V"                          # baseline 1 ; expect 0
grep -nF "0xa002031f" "$V"                                    # baseline 4 ; expect 0
grep -nF "modbridge" "$V"                                     # baseline 6 ; expect 0
grep -nF "launchApp(PPSA33286)" "$V"                          # baseline 13; expect >=1 (session validity)
grep -cF "libSceSaveData" "$V"                                # baseline 2016; expect >0 (normal saves)
```

One-shot profile:
```bash
for p in "A user thread receives a fatal signal" "# proc name: eboot" "rip: 0000000080004920" \
         "nonsleeping lock" "SCE_SYSCORE_EVENT_APP_CRASH" "App Crash : PID" \
         "dbggc_push_only_protection_fault_info_to_user" "GPU Protection fault" \
         "0xa002031f" "modbridge"; do printf '%3d  %s\n' "$(grep -cF "$p" "$V")" "$p"; done
```

**Clean-session expectation: zero hits on all ten crash patterns** (per l13/l16/l18 precedent of clean vanilla captures), with `launchApp(PPSA33286) >= 1` and libSceSaveData activity present so the session is comparable. The vanilla capture window must span at least one full launch -> save-load -> exit: baseline plugin-run sessions died 23–90 s after launch, so a vanilla capture of a few minutes of play plus a save load is a like-for-like comparison.

Interpretation of vanilla outcomes:
1. **All ten 0, launch present** — clean; the CE-108255-1-style crash series is plugin-run-dependent. Proceed with l25 on the plugin side.
2. **Fatal blocks present, `proc name: eboot`, but rip differs and `modbridge` = 0** — game crashed *without* the plugin: game-side regression (eboot/data), not modbridge. Stop plugin iteration; audit eboot.
3. **Fatal blocks present AND `modbridge` > 0** — the session was not actually vanilla (plugin autoload still armed): wrong capture/session; do not score it as vanilla.
4. **`copyin`/`nonsleeping` hits but no eboot fatal block** — another process crashed; check the block's `proc name` before concluding anything.
5. **`SCE_SYSCORE_EVENT_APP_CRASH` present but no `A user thread` block** — abnormal termination without a user-mode fault dump (watchdog/kill path); investigate separately, not comparable to this baseline.

Caveats: the baseline file includes a post-reboot tail after the 19:21:55Z stream close (no fatal blocks there; grepping the whole file is safe). The abort backtraces are kernel-salvaged under the nonsleeping-lock condition, so frame *order* is indicative; the *module membership* (eboot + modbridge + libkernel entry) is the stable part, and it is what the `modbridge` grep keys on.
