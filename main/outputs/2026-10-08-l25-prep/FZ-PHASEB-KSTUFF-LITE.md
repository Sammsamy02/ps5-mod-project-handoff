# FZ — Phase B prep: kstuff-lite kernel-bridge payload (research, 2026-10-08)

Agent FZ next-track research item A (Claude round-4 item 9). Research/design only:
no builds, no console contact, no tree edits. Sources are all Mac-local; every claim
below is tagged with its evidence class.

Evidence classes used in this document:

- **[SRC]** pinned upstream kstuff FULL source on this Mac:
  `.superpowers/native-dependency-spike/ps5-pid-source-shape-1/pinned-kstuff-main.c`
  (1152 lines, pinned 2026-10-01, pre-v1.6.x lineage; the lite build is a reduced
  build of the same repo — interface ABI shared, marked where unverified).
- **[LOG]** console-side logs/receipts on this Mac (etaHEN startup logs, klog
  snapshots, l18/l23 mission receipts, pldmgr catalog).
- **[META]** pldmgr payload catalog metadata (`worker-boot-protocol/probe/list_payloads.json`).
- **[POST]** `outputs/2026-10-06-recovery-fakesign/POSTMORTEM.md` (quarantine facts).
- **[MISS]** `outputs/2026-10-06-recovery-fakesign/PS5-MODDING-MISSION.md` (Phase B rules).
- **[SP]** SharpProspero.Link l26 harness sources (project's own client-side
  kekcall implementation — corroborates the calling convention).
- **[INF]** inference; explicitly NOT verified.

**Headline finding: the kstuff-lite SOURCE is NOT on the Mac.** No tree matching
`*kstuff-lite*` exists anywhere under the worktree (searched `outputs/` and
`.superpowers/`; only `kstuffSharp*` in-house C# samples, `build-kstuff.ps1` build
scripts, and the single pinned FULL-kstuff `main.c` exist). Everything below about
lite specifically is therefore from console-side evidence plus the shared-family
source that IS pinned. Fetching/reading the actual EchoStretch/kstuff-lite v1.11
source is a prerequisite before any Phase B code touches it (see Open questions).

---

## 1. What kstuff-lite is, on this console

| Fact | Value | Evidence |
|---|---|---|
| Console path | `/data/pldmgr/payloads/kstuff-lite/kstuff-lite_v1.11.elf` | [META]/[LOG] |
| Version / release | v1.11, mirror last_update 2026-09-20 | [META] |
| Upstream | `itsPLK/ps5-payloads-mirror` mirror of **EchoStretch/kstuff** releases; sha256 `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` | [META] |
| Autoload state | FIRST line of `autoload.txt` (12 payloads; ploader last) — loaded every boot by the etaHEN/OnionHEN chain | [LOG] l23 harvest autoload.txt |
| Resident process | named `kstuff.elf`, e.g. PID 87/88 fresh boots; "initialization/remount/shell patch" logged | [LOG] klog snapshots, l18 receipts |
| Residency check | etaHEN bootstrap logs `kstuff state=1 readable=1 native=0xffffff80… compat=0xffffff80… crypt=deb7/deb7` then `resident kstuff found` — every recorded generation | [LOG] `worker-boot-protocol/probe/etahen-startup-1360-port.log` |
| Full kstuff also installed but NOT autoloaded | `/data/pldmgr/payloads/kstuff/kstuff.elf` v1.6.7 | [META] |
| Automount marker | `/data/.kstuff_noautomount` present (0 B) — lite's automount disabled | [LOG] boot-protocol §1.3 |

## 2. Exposed interface (how a client payload calls it)

There is **no /dev node, no ioctl, no TCP server, and no linking API**. The entire
userland surface is (a) a hijacked-syscall calling convention and (b) patched
kernel tables that other payloads inherit. From [SRC] (+[SP] corroboration):

### 2.1 kekcall calling convention (user → kernel call gate)

- Client discovers a syscall gadget: kstuff's own userland does
  `p_kekcall = dlsym(handle 0x1, "getpid") + 7` [SRC main.c:922]; the project's
  CRT emitter derives `ptr_syscall` from `sceKernelDlsym`/`getpid` similarly [SP
  `PayloadCrtEmitter.cs` `__sp_crt_syscall_init`].
- A kekcall is dispatched as **syscall number 0x27 (SYS_getppid) with the
  kekcall sub-number packed in RAX[63:32]**; args a1..a6 in the six syscall
  registers. Exact shim (project-side, [SP] `__sp_kekcall`):
  `rax = (nr << 32) | 0x27`, args shifted down one slot, then `call [ptr_syscall]`.
  The kernel-side handler intercepts getppid and reads `rax>>32`.
- Known kekcall numbers [SRC main.c:221-226] (FULL source; lite subset unverified):

| Number | Name | Meaning |
|---|---|---|
| `0x000000027` | KEKCALL_GETPPID | pass-through getppid |
| `0x100000027` | KEKCALL_READ_DR | read debug registers |
| `0x200000027` | KEKCALL_WRITE_DR | write debug registers |
| `0x300000027` | KEKCALL_RDMSR | read MSR |
| `0x500000027` | KEKCALL_REMOTE_SYSCALL | execute a syscall **in an arbitrary pid**: `kekcall(pid, nr, args_ptr, 0,0,0, 0x500000027)` |
| `0xffffffff00000027` | KEKCALL_CHECK | residency probe — returns nonzero → "already loaded" no-op exit |

- Underlying kernel channels kstuff itself uses (and thus proven available once
  resident) [SRC]: `SYS_mdbg_call 573` (kernel write/patch channel — used to
  patch SceShellCore in other processes), `SYS_dynlib_get_info_ex 608` (module
  base/segment info for any pid), sysctl `KERN_PROC` pid enumeration,
  kernel-memory `kmemcpy/copyin/copyout/kpoke64/kmemzero` primitives implemented
  in the loaded kelf.

### 2.2 Resident kernel patches other payloads inherit

- `sysentvec` (native) and `sysentvec_ps4` (compat) syscall tables get the
  `0xdeb7` extension marker [SRC main.c:1112-1113]; etaHEN independently reads
  both patched tables at boot (`native=… compat=… crypt=deb7/deb7`) [LOG] —
  **verified resident on this console**.
- `crypt_singleton_array` XTS + HMAC crypto singletons patched [SRC:1114-1115] —
  this is what makes ftpsrv's decrypt-on-download view work on this console [POST].
- Remount `/system_ex` RW + shell patch + automount (automount disabled here via
  marker) [LOG klog; kstuffSharp port mirrors these steps, `Program.cs` steps 5-6].
- FULL kstuff additionally: security/QA/utoken/targetid flags, debug-settings
  shellcore patches, per-FW parasite trampolines [SRC]. Which of these lite v1.11
  includes is **not verifiable without its source** [INF]; the observed log line
  "remount/shell patch" shows at least remount+shellcore patches are in lite.

### 2.3 What our PRX/payload would do for Phase B capability #1 (read-only kernel reads)

1. Zero-kernel-touch residency probe first: `kekcall(0,0,0,0,0,0, KEKCALL_CHECK)`
   through a `getpid+7`-style gadget. Nonzero ⇒ kstuff present; also proves the
   call gate end-to-end with a read-only call. (This is exactly mission rule
   "first determine whether kstuff-lite's existing kekcall interface suffices".)
2. Reads: via the `0xdeb7`-extended syscall range the patched sysentvec provides
   (exact per-number ABI must be extracted from the lite v1.11 binary/source —
   not on the Mac today) and/or `KEKCALL_REMOTE_SYSCALL` for cross-process reads.
3. Writes (later, gated): only after reads pass the three-console-pass rule, via
   `SYS_mdbg_call`/kekcall write services.

## 3. Offset / firmware-compat model

- FW detection is **dynamic** at load: `r0gdb_get_fw_version()` [SRC]; kernel
  offsets come from a per-FW table (`prosper0gdb/offset_list.txt` included into
  the symbol-fixup list) [SRC main.c:1013-1014].
- Per-FW HARDCODED tables in the pinned FULL source: shellcore patches +
  security-check "parasites" for **3.00, 3.10, 3.20, 3.21, 4.00, 4.02, 4.03,
  4.50, 4.51** — unknown FW ⇒ `notify("not supported")` + clean exit, and
  `KEKCALL_CHECK` makes re-runs a no-op [SRC].
- This console: FW **13.600.007**, etaHEN Beta 2.5 13.60-port [LOG]. The pinned
  source predates it; **kstuff-lite v1.11 (2026-09-20) demonstrably supports
  13.60** — resident at every recorded bootstrap generation [LOG].
- Kernel addresses are **per-boot randomized**: the `native=`/`compat=` values
  differ every generation (14eb3ef0/15cb3ef0/1ae03ef0/1307bef0/1b4b7ef0/1a4c7ef0/
  0e8d7ef0 suffixes) [LOG]. Nothing may pin kernel addresses across boots.
- There is **no unload path** — once resident, only a reboot replaces it; there
  is also no in-tree reload [LOG boot-protocol §1.3].

Compat matrix (evidence-graded):

| FW | kstuff FULL (pinned src) | kstuff-lite v1.11 (installed) | kstuffSharp (in-house) |
|---|---|---|---|
| 3.00–4.51 | explicit tables [SRC] | presumed (same repo lineage) [INF] | never ran |
| 10.01 | upstream-era support [INF] | unknown | offsets written for it [POST] |
| 13.60 | unknown for pinned copy | **verified resident** [LOG] | **crashed kernel** [POST] |

## 4. Differences vs quarantined kstuffSharp/InstallerApp

kstuffSharp is the in-house C# NativeAOT reimplementation (samples under
`outputs/2026-10-04-zcode-team/worker-kernel-build/source/samples/kstuffSharp*`,
emitter lineage alive in l26 `SharpProspero.Link/KernelModuleWriter.cs`). It is
QUARANTINED: `/data/modbridge-quarantine/` + one inert copy in `payloads/`, not
in autoload.txt [POST rule 7].

Why it crashed and kstuff-lite doesn't (same-FW comparison):

1. **Offset maintenance model.** kstuffSharp shipped ONE hardcoded offset set,
   `KernelOffsets.CryptSingletonArray_1001` etc. for FW 10.01; on 13.60 those
   constants point at arbitrary kernel data, and its writes there produced a
   kernel coredump (2026-10-06 20:26:46Z boot, autoload state B) [POST]. Lite
   v1.11 carries maintained 13.60-era offset tables from the upstream repo
   [META date + LOG residency].
2. **Fail-closed on unknown FW.** Upstream kstuff detects FW, and if no table
   matches it notifies and exits WITHOUT writing [SRC `get_parasites`/
   `get_shellcore_patches` default paths]. kstuffSharp only guarded the
   pmap-offset lookup; other 10.01 offsets were still applied where present
   [SRC Program.cs:130-141 + POST].
3. **Hook conflicts.** A second kernel module (kstuffSharp) installing IDT
   int1/int13/syscall hooks collides with the already-resident kstuff hooks —
   the mission explicitly bars it: "it conflicts with kstuff's own hooks" [MISS].
4. **Resident-guard.** kstuff's KEKCALL_CHECK makes double-loading a logged
   no-op [SRC main.c:924-928]; etaHEN's own inspection also detects "resident
   kstuff found" [LOG].

Residual risk that is the SAME for lite: any kernel-writing payload with wrong
per-boot addresses can panic the kernel; lite mitigates via upstream tables +
fail-closed, but a MODIFIED lite build reintroduces kstuffSharp-class risk —
hence the modified-build rules in §5.

## 5. Safety / recovery plan for Phase B (binding, from mission + postmortem)

1. **Never autoload a modified kstuff.** autoload.txt changes require explicit
   user OK [MISS standing rules]. Modified builds NEVER overwrite
   `kstuff-lite_v1.11.elf`; distinct filenames only [MISS].
2. **Per-launch manual load.** Use pldmgr `/loadpayload` for the payload under
   test, once, with receipt; never duplicate dispatch [MISS/AGENTS].
3. **Kill-switch.** Empty `ploader.ini` for unmodded play; remove the test build
   from autoload; abort criterion = any panic/coredump signature ⇒ stop, do not
   relaunch (no-rapid-launch law below) and roll back.
4. **Rollback.** Keep `kstuff-lite_v1.11.elf` byte-intact on console (checksum
   above) and a Mac-side copy; recovery = remove test build from autoload +
   reboot. **Pre-prove the recovery path before the first modified-build test:
   FTP (ftpsrv :2121) must still come up on a crashed boot so the original can
   be restored; if that cannot be established, report the test blocked** [MISS].
5. **No-rapid-launch law.** ShadowMount pauses kstuff 15–25 s after ANY launch
   and resumes only on a clean game exit; failed launches never fire the resume,
   so a burst of failed launches leaves kstuff paused and ALL games fail auth
   until a clean exit or reboot [POST rule 6]. One user-launched test per boot;
   `kstuff_pause_delay_image_seconds=25` / `direct=15` defaults stand
   (l23 `shadowmount-config.ini`).
6. **Crash-signature monitoring.** klog captures over TCP 3232 to NEW files
   (never truncate); watch for kernel coredump/panic markers after any kernel
   step; `kstuff_crash_detection=0` is already set (do not re-enable blindly);
   check `autotune.ini` after every crash [MISS l12-D, POST].
7. **Read-before-write ladder.** Read-only additions first; kernel writes only
   after reads are console-proven three times across reboots with logs [MISS].
8. **Regression launches.** After any kstuff change: Astro Bot, Stellar Blade
   and Townfall must still launch (separate user-launched tests) [MISS].
9. **Back up saves before any kernel test** [MISS].
10. Console-policy scope: Phase B uses kernel access for the mod runtime's own
    reads/patches (#1/#3/#6) and second-game profile work — NOT for bypassing
    console security policy.

## 6. Phase B use cases unlocked

- **#1 read-only kernel reads** — kekcall gate + 0xdeb7 syscall range (§2.3).
- **#3 harmless in-game patch** — KEKCALL_REMOTE_SYSCALL + SYS_mdbg_call write
  channel into a target pid, with clean revert [MISS Phase B order 2].
- **#6 kernel input path** [MISS Phase B order 3].
- **Second-game/system-level work** — cross-process reads for a second game's
  profile (live base/VMMAP discovery where static labels are not enough).
- NOT a use case: anything console-policy related (DRM/piracy/circumvention).

## 7. Open questions (must resolve before Phase B code)

1. **Obtain kstuff-lite v1.11 source** (EchoStretch/kstuff repo, lite branch) —
   not on the Mac; needed to confirm which kekcall numbers the lite kelf actually
   services and the 0xdeb7-range syscall ABI. (Web fetch allowed for research
   only if not duplicating an earlier fetch; otherwise request it in the next
   console-connected session.)
2. Whether lite on 13.60 installs the FULL set (QA/security/targetid) or a
   subset — affects nothing in §5 but should be documented from source.
3. Exact behavior of a *modified* lite re-run while stock lite is already
   resident (KEKCALL_CHECK says "already loaded" for the family; confirm lite's
   variant) — determines whether a modified build must replace stock in autoload
   (needs user OK) or can coexist per-launch.
