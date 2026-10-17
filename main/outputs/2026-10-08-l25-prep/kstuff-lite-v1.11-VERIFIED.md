# kstuff-lite v1.11 — SOURCE-VERIFIED INTERFACE (agent GP, 2026-10-10)

Analysis only. Battery-safe session: source reading + docs, no builds, no console,
no network. Pinned artifacts read-only; source extracted to `/tmp/gp-kstuff-lite-src/`
(zip never modified).

## 0. Pin verification (pre-read)

| Artifact | sha256 | Status |
|---|---|---|
| `outputs/kstuff-lite-pinned/kstuff-lite-v1.11.elf` | `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` | MATCHES the pldmgr mirror hash recorded from the console |
| `outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip` | `b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6` | pinned zip intact |

Binary format facts (host `file`/`xxd`): **plain ELF64 x86-64** (PS5 = Zen 2
x86-64 FreeBSD), `LSB pie executable, dynamically linked, stripped`, magic
`7f454c46`. The console copy is therefore genuinely a plain-ELF payload (the
PHASEB-PLAN P1 "if 7f454c46 appears, document" case resolves as: expected —
this is how the mirror distributes it; identity is carried by the matching
mirror sha256, not a SELF container).

Version strings visible directly in the pinned binary (P2 equivalent, done
Mac-local): `Welcome To Kstuff Lite 1.11`, `By sleirsgoevy`,
`ps5-kstuff is already loaded`, `your firmware is not supported (prosper0gdb)`,
`your firmware is not supported (shellcore)`, `kstuff.elf`, plus
`Ubuntu clang version 18.1.3 (1ubuntu1)` and build path
`/home/echo/drakmor/kstuff-lite/ps5-kstuff/` (see §7).

Source layout (what is what):

- `ps5-kstuff-ldr/` — **the pinned .elf itself** (`make` target `kstuff.elf`;
  CI artifact). A normal PS5 userland process (SDK `prospero-clang`), embeds
  `payload.bin` via `.incbin`, maps it at fixed base `0x0000000926100000`,
  jumps into it, then runs automount/shellui/app.db work forever.
- `ps5-kstuff/main.c` — kernel-loader payload (runs inside the kstuff.elf
  process): r0gdb bootstrap, loads KELF+UELF, installs IDT/sysent/crypt patches.
- `ps5-kstuff/kelf.asm` — KELF: pure ROP-frame data (no native instructions),
  per-CPU IDT/IST entry points for int1/int3/int13.
- `ps5-kstuff/uelf/` — UELF: the C logic (kekcall dispatcher, crypto, fself/
  fpkg emulation) executing in a private CR3 with a user-accessible direct map.
- `prosper0gdb/` — kernel R/W bootstrap (victim `pktopts` pipe primitive) +
  per-FW offset tables. `lib/` — freestanding crt/dl/syscall stubs.

## 1. CLAIM-BY-CLAIM VERIFICATION vs PHASEB-PLAN.md §interface + FZ-PHASEB-KSTUFF-LITE.md

Legend: CONFIRMED / CORRECTED / REFUTED / NEW (not in either doc).

### 1.1 The kekcall gate

| # | Claim (source doc) | Verdict | Evidence (real v1.11 source) |
|---|---|---|---|
| G1 | Dispatch = syscall `0x27` (SYS_getppid), kekcall sub-number in `RAX[63:32]` | **CONFIRMED** | `ps5-kstuff/main.c:301-306` defines `KEKCALL_*` as `0x<sub>00000027`; `ps5-kstuff/uelf/main.c:65-85` reads the user trap frame and passes `args[RAX]>>32` as `nr` to `handle_kekcall` (`uelf/kekcall.c:19`). |
| G2 | Gadget = `dlsym(handle 0x1,"getpid") + 7` (family) / "+7 vs +0xa open" (GD §6.3) | **CONFIRMED + RESOLVED** | `ps5-kstuff/main.c:1334`: `p_kekcall = (char*)dlsym((void*)0x1, "getpid") + 7;`. The `+7` is the `syscall` instruction itself (libkernel stubs begin `mov eax, imm32` = 7 bytes). The 8cc demo uses the equivalent `$getppid_addr + 7` (`8cc-kekcalls.c:43`) — same stub shape. **Offset duality dissolved**: any reachable raw `syscall` instruction works; a client with its own inline syscall stub needs no dlsym at all (see §3 ABI note). Caveat: `dlsym(0x1,...)` only works because the ldr patches its own eboot dynlib segments first (`ps5-kstuff-ldr/src/main.c:577-583`). |
| G3 | Args a1..a6 in the six argument registers, "shifted down one slot" ([SP] shim) | **CONFIRMED** | Client-side ROP sets `rdi..r9 = a1..a6, rax = packed` (`8cc-kekcalls.c:29-43`). Kernel side copies the 7-qword syscall frame prefix (rdi,rsi,rdx,rcx,r8,r9,rax; `_Static_assert(RAX == 6)` `uelf/main.c:68`) and passes it as `args[]`. |
| G4 | `KEKCALL_CHECK` "returns nonzero when resident" (PHASEB-PLAN §3.0) / "returns nonzero → already loaded" (FZ §2.1) | **CORRECTED — polarity is INVERTED** | `uelf/kekcall.c:102-106`: CHECK sets `args[RAX]=0`, returns 0 → **resident ⇒ kekcall() returns 0**; not-resident ⇒ the real getppid executes and returns the parent pid (nonzero). Upstream's own guard: `if(!kekcall(0,...,0xffffffff00000027)){ notify("ps5-kstuff is already loaded"); return 1; }` (`ps5-kstuff/main.c:1336-1340`). GD's `[KB]` skeleton `check_result = (check != 0)` would read residency **backwards** and must be fixed before any build. |
| G5 | Sub-number unknown ⇒ error | **CORRECTED (new nuance)** | `handle_kekcall` returns `ENOSYS` for unknown sub-numbers; the dispatcher then does **not** intercept (`err != ENOSYS` guard, `uelf/main.c:75`), so the **real getppid runs** and the caller receives the parent pid — silently indistinguishable from a no-op. You cannot probe sub-number support by return value; only CHECK (returns 0) and side effects are observable. |

### 1.2 The kekcall numbers

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| K1 | Table = GETPPID, READ_DR `0x100000027`, WRITE_DR `0x200000027`, RDMSR `0x300000027`, REMOTE_SYSCALL `0x500000027`, CHECK `0xffffffff00000027` | **CONFIRMED as the loader-side define list** (`ps5-kstuff/main.c:301-306`) — but the **actually serviced** set is the UELF dispatcher's, see K2. | |
| K2 | "lite subset unverified" | **NOW VERIFIED — full lite v1.11 table in §2** | `uelf/kekcall.c:19-107` implements sub-numbers 1,2,3,5,7,0xffffffff (+6 only in `KSTUFF_OBS=1` builds); sub 4 (WRMSR) is explicitly reserved and NOT implemented. Two numbers the docs lacked: **6 (OBS snapshot)** and **7 (PPR plaintext control, magic `0x505052504c41496e` "PPRPLAIN")**. |
| K3 | WRMSR exists (implied by READ_DR/WRITE_DR/RDMSR symmetry in FZ's table? not claimed — noted) | **REFUTED for v1.11** | `uelf/kekcall.c:63`: `//nr 4 reserved for wrmsr` — falls through to ENOSYS → real getppid. |

### 1.3 The channels

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| C1 | `SYS_mdbg_call 573` | **CONFIRMED** (number + availability) — **CORRECTED** (role) | `ps5-kstuff/main.c:557` `#define SYS_mdbg_call 573`. v1.11 does NOT use mdbg_call to patch ShellCore; it patches via its own kernel-context `phys_copyin` page-table walk (`main.c:1280`, `main.c:1069`). mdbg_call is *hooked* (`main.c:1589`, `uelf/main.c:87`) but the hook (handle_syscall_fix) still executes the original syscall — mdbg_call remains callable by any process, now flowing through kstuff's int3 fixup path. FZ's "used to patch SceShellCore" attribution belongs to older kstuff lineage (`r0gdb.h:74` `mdbg_call_20`), not v1.11's patch path. |
| C2 | `SYS_dynlib_get_info_ex 608` | **CONFIRMED** | `main.c:558`; used via remote syscall against SceShellCore at `main.c:1255`. |
| C3 | sysctl `KERN_PROC` pid enumeration | **CONFIRMED** | `find_proc()` `main.c:530-545` (KERN_PROC_PID 1..1024, name at buf+447). |
| C4 | kmemcpy/copyin/copyout/kpoke64/kmemzero kernel primitives | **CONFIRMED** — but **kstuff-private** | Implemented over the r0gdb victim-pktopts pipe (`prosper0gdb/r0gdb.c:50-167`) and, inside the UELF, over its private direct map (`uelf/utils.h` DMEM). **None of this is exposed to client payloads** — the client surface is exactly the kekcall table + stock syscalls. |

### 1.4 Resident patches

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| P1 | "sysentvec (native) and sysentvec_ps4 get the `0xdeb7` extension marker; etaHEN reads both" (FZ §2.2) | **CORRECTED** | v1.11 builds with `USE_INT3_SYSCALL_HOOK 1` (`main.c:1313`); the `#else` branch that writes `0xdeb7` into `sysentvec+14` (`main.c:1674-1675`) is **compiled out**. Instead the `sv_table` pointers are swapped to kmalloc'd **fake sysent tables** (`main.c:1604-1662`, incl. a third per-proc table installed only into SceShellCore's `p_sysent`, `main.c:1672`). The per-boot-varying `native=/compat=` addresses etaHEN logs are these fake tables (16 KiB-aligned kmalloc — matching the observed `...ef0/...7ef0` suffixes), not stock kernel data. |
| P2 | "`0xdeb7` syscall-range ABI" (PHASEB-PLAN §1 table, R4 rung; GD §6.2 open gap) | **REFUTED for v1.11** | There is **no 0xdeb7-extended syscall range**. `0xdeb7` appears exactly as a 2-byte **poison tag written into pointer high halves**: crypt XTS + HMAC singletons (`main.c:1677-1678`, unconditional) and (in old builds) `sv_table`. A poisoned pointer is non-canonical; the call traps, the UELF recognizes `reg>>48 == 0xdeb7`, restores `0xffff<<48`, and services the operation (`uelf/main.c:173-282` `handle_generic_decrypt_trap`). R4 as designed does not exist; cross-process reads go through REMOTE_SYSCALL only. |
| P3 | crypt XTS + HMAC singletons patched (making ftpsrv decrypt-on-download work) | **CONFIRMED** | `main.c:1677-1678` writes `0xdeb7` at `crypt_singleton_array + 11*8 + 2*8 + 6` (XTS) and `+ 9*8 + 6` (HMAC); UELF fake-key XTS/HMAC handlers in `uelf/pfs_crypto.c`, fake keys in `uelf/fakekeys.c`. |
| P4 | Remount `/system_ex` RW + shell patch; automount disabled via marker | **CONFIRMED** | `ps5-kstuff-ldr/src/mount_helpers.c:14` (`nmount` fspath `/system_ex` from `/dev/ssd0.system_ex`); automount gated by `/data/.kstuff_noautomount` (`ldr main.c:56,421`). ShellCore patches applied from per-FW tables (below). |
| P5 | "Which of FULL's QA/security/utoken/targetid flags lite includes unknown" (FZ open q.2) | **ANSWERED** | v1.11-lite carries: fake sysent tables, IDT int1/int3/int13 + IST patches, EFER bit (`main.c:1534`), crypt XTS/HMAC poison+emulation, fself/fpkg/npdrm emulation (execve/dynlib_load_prx/get_self_auth_info/get_sdk_compiled_version/get_ppr_sdk_compiled_version/ioctl/nmount/unmount hooks), SceShellCore byte patches + PPR/GOT wrapper (retail only), ShellUI patches (disable-able), `/system_ex` remount, automount, app.db patch. It does **not** carry FULL's security/QA/utoken/targetid flag flips (no such code in this tree). |
| P6 | Per-boot kernel ASLR; nothing may pin kernel addresses across boots | **CONFIRMED** | All offsets are `kdata_base`-relative (anchor supplied at load by elfldr; `offsets.c:7-9`); fake tables/UELF CR3/KELF are kmalloc'd fresh each boot (`main.c:1407-1533`); `find_empty_pml4_index` picks the UELF VA window dynamically (`main.c:442-452,1424-1425`). |
| P7 | CHECK-guarded re-run no-op | **CONFIRMED (with corrected polarity, see G4)** | `main.c:1336-1340`. A second launch notifies "ps5-kstuff is already loaded" and exits 1 without touching the kernel again. This also answers FZ open q.3: a *modified* lite build would see the STOCK module's CHECK hit and bail — coexistence per-launch does not work without changing the CHECK sub-number; modified builds stay prohibited anyway (L4). |
| P8 | No unload path | **CONFIRMED** | No code anywhere restores the IDT entries, fake sysentvecs, crypt markers, or ShellCore patches. `r0gdb_cleanup()` (`r0gdb.c:428`) only touches r0gdb's own debug-hook IDT/TSS slots and is only called from `die()` pre-install. Reboot is the only reset. |

### 1.5 Architecture note (PHASEB-PLAN P3.1 said "llvm-objdump … aarch64")

**CORRECTED**: the payload is x86-64 (PS5 = AMD Zen 2). Disassembly plans and
GD's `[KB] skeleton` inline asm (written with aarch64 `x0-x8/blr` constraints,
PHASEB-PLAN §5) are the wrong ISA; the client shim must be x86-64 (`syscall`
insn, `rax/rdi..r9`). The project's own CRT shim `[SP] __sp_kekcall`
(`mov r9,[rsp+8]; shl rax,32; or rax,0x27; call [rip+ptr_syscall]`) is
x86-64 and matches the verified ABI.

## 2. THE EXACT LITE v1.11 KEKCALL TABLE (from `ps5-kstuff/uelf/kekcall.c:19-107`)

Client dispatch (all): raw `syscall` with `RAX = (sub << 32) | 0x27`, args
`RDI,RSI,RDX,RCX,R8,R9` = a1..a6. Kernel reads the packed rax from the user
trap frame; sub-number = `frame_rax >> 32`.

| sub | packed constant | name | Implemented | Arguments (client view) | Behavior / return |
|---|---|---|---|---|---|
| 0 | `0x000000027` | GETPPID passthrough | n/a | — | Not intercepted: real getppid. Parent pid returned. |
| 1 | `0x100000027` | READ_DR | YES | a1 = user ptr to 6×u64 buffer | DR0,DR1,DR2,DR3,DR6,DR7 copied out (48 B). Returns 0/EFAULT-style errno. |
| 2 | `0x200000027` | WRITE_DR | YES | a1 = user ptr to 6×u64 | Debug registers installed (watchpoints). Same error model. |
| 3 | `0x300000027` | RDMSR | YES | a1 = MSR index | MSR value returned as the kekcall return value (written to `td_retval`). |
| 4 | `0x400000027` | WRMSR | **NO** (reserved) | — | ENOSYS ⇒ degrades to real getppid (parent pid). |
| 5 | `0x500000027` | REMOTE_SYSCALL | YES | a1 = target pid, a2 = syscall number, a3 = user ptr to 6×u64 args | Executes `sysents[nr].sy_call` with the target pid's first thread as `td`; the 6 args are `copyin`'d from a3 (caller address space). Return: target syscall's retval copied back into the caller's retval (trap-3 path); failures surface as `EFAULT`/`ESRCH` (and the remote errno when the target syscall fails). Special case: `SYS_sysarch`+`AMD64_GET_FSBASE` writes the target's FS base to the caller buffer (a2 of the remote syscall). Upstream helper: `main.c:547-554` `remote_syscall(pid,nr,a0..a5)`. |
| 6 | `0x600000027` | shared-area snapshot | OBS builds only | a1,a2 | `#if KSTUFF_OBS` — release build (CI default `KSTUFF_OBS=0`) returns ENOSYS ⇒ real getppid. |
| 7 | `0x700000027` | PPR plaintext control | YES | a1 = magic `0x505052504c41496e` ("PPRPLAIN"), a2 = mode, a3..a6 per mode (`uelf/fpkg.c:721+`) | fpkg-scope/trace control used by the ShellCore PPR wrapper. Returns 0/EINVAL/EFAULT. Not useful to us. |
| 0xffffffff | `0xffffffff00000027` | CHECK | YES | none | **Returns 0 when resident** (retval 0, success). Not resident ⇒ real getppid ⇒ parent pid. |

Error model (client side): success ⇒ C-level return = retval written to
`td_retval` (CHECK: 0; RDMSR: the value); handled-with-error ⇒ errno in rax per
the normal PS5 stub convention (`jc` path); unimplemented/unknown ⇒ silent
fallback to real getppid (see G5).

## 3. Capability mapping for OUR payload (#1 reads / #3 patching / #6 input)

### #1 — kernel/cross-process READ primitives actually available to a client

1. **kekcall 5 REMOTE_SYSCALL** — the only cross-process primitive. Proven
   upstream usage = exactly our read ladder: `remote_syscall(pid,
   SYS_dynlib_get_info_ex 608, 0, 0, &buf, 0,0,0)` against SceShellCore
   (`main.c:1253-1257`), plus remote `mmap/mlock/mprotect/munmap`
   (`main.c:1056-1083`). Buffers passed as syscall args resolve in the
   **caller's** address space (kernel copyio runs under the caller's CR3 in the
   trap context) — upstream reads its results into its own buffers through this
   exact path, so remote `sysctl KERN_PROC/KERN_PROC_VMMAP` and
   `dynlib_get_info_ex` into OUR buffers is the same channel class.
   Caveat: the syscall number is dispatched from the UELF's embedded ORIGINAL
   sysent copy (`uelf/kekcall.c:204`) — Sony-private numbers (573/608/etc.) are
   reachable; numbers kstuff hooks are intercepted identically for the remote
   caller (fixups still execute the original).
2. **kekcall 3 RDMSR / 1 READ_DR** — hardware registers only; diagnostic value,
   not a memory-read path.
3. **Plain syscalls in our own pid** — unchanged by residency except the hooked
   list (execve, dynlib_load_prx 594, get_self_auth_info 607,
   get_sdk_compiled_version 647, get_ppr_sdk_compiled_version 713, getppid 39,
   mprotect 74, mdbg_call 573; + ioctl/nmount/unmount for ShellCore only)
   which still execute originals behind fixups.
4. **NO generic kernel-memory read kekcall exists** (no kmemcpy/kpeek service;
   R4 refuted). If a true kernel-VA read is ever needed, that is L4 territory
   (new kernel-resident code) — out of scope, consistent with the plan's bias.

### #3 — code patching support in lite's surface

- **No kekcall writes kernel or other-process memory.** WRITE_DR is watchpoints.
- **Cross-process write channel for us = `SYS_mdbg_call 573`**, still callable
  (hook passes through with fixups) — v1.11 itself instead patches ShellCore
  from its own kernel context via `phys_copyin` (page-walk + direct map),
  which we do not have. So #3 via mdbg_call remains OUR ladder's L3 candidate,
  now source-confirmed callable (573 in the PS5 hook list, `main.c:1589`;
  dispatcher `uelf/main.c:87` runs `handle_syscall_fix` then the original).
- **i-cache question RESOLVED (architecture)**: PS5 is x86-64 — instruction
  cache is coherent; **no i-cache invalidation exists anywhere in kstuff's
  source** (grep for icache/clflush/wbinvd/invd: zero hits) and none is needed.
  The upstream patch pattern is: remote `mmap RW` → write blob → verify
  readback → remote `mprotect R+X` (`main.c:1056-1083`) — the target-side
  mprotect handles TLB permission coherence. Data-page (GOT/vtable) patches
  need nothing at all. GD §3.2's restriction ladder can be relaxed to "follow
  the upstream write→verify→mprotect pattern", though revert-first receipts
  remain mandatory.
- Fake-sign law interaction unchanged: runtime memory only, never files.

### #6 — input/pad surface

**CONFIRMED DECLINED.** Full-tree grep for pad/controller/keyboard/mouse/input
code: zero relevant hits (only crypto padding). No pad syscall is hooked, no
input path exists in the kekcall table or UELF. GD's verdict stands: userspace
ScePad (PS5PadSource) is the route; a kernel input route would be new
kernel-resident code (L4, prohibited).

## 4. Rebuild procedure (PREPARED ONLY — not executed; battery law)

Goal: byte-identical `kstuff.elf` vs the pin, or documented bust.

### 4.1 Inputs required

1. The pinned source zip (have) — **INSUFFICIENT ALONE**: `ps5-kstuff/BearSSL/`,
   `ps5-kstuff/isa-l_crypto/`, `ps5-kstuff/libtomcrypt/` are **empty** in the
   zip (GitHub source zips exclude submodules). The exact submodule commits
   pinned by the `1.11` tag (git tree, not in the zip) must be fetched before
   any build. `.gitmodules` URLs: bearssl.org/git/BearSSL,
   github.com/libtom/libtomcrypt, github.com/intel/isa-l_crypto.
2. Host toolchain: Ubuntu 24.04-class box with `gcc`, `yasm`, `nasm`,
   `python3`, `clang-18` + the ps5-payload-dev SDK tarball (CI recipe,
   `.github/workflows/ci.yml:29-39`). The pinned binary's `.comment` records
   **`Ubuntu clang version 18.1.3 (1ubuntu1)`** — the ldr link is SDK-wrapped
   clang; the exact SDK release used upstream is unknown and **our pinned
   sdk-v0.43 may differ** (its `bin/*` are wrapper scripts only; the bundled
   LLVM version could not be read out on this Mac — wrappers reference a
   relocated `llvm-clang` binary not present in the v0.43 copy). First rebuild
   step on the 5090PC: `prospero-clang --version` must print exactly
   `Ubuntu clang version 18.1.3 (1ubuntu1)` (or the build is advisory-only).
3. Rebuild path must be `/home/echo/drakmor/kstuff-lite/` — an absolute build
   path from the upstream build is **embedded in the binary**
   (`/home/echo/drakmor/kstuff-lite/ps5-kstuff/`, from `__FILE__`-class strings
   in the freestanding libs). Without reproducing it, byte-identity fails.

### 4.2 Commands (from the repo's own files — do NOT run on battery)

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk     # must be the clang-18.1.3-matching release
git submodule update --init --recursive          # restore BearSSL/isa-l_crypto/libtomcrypt at tag pins
cd /home/echo/drakmor/kstuff-lite                # reproduce embedded build path
make -C lib clean; make -C prosper0gdb clean
make -C ps5-kstuff clean; make -C ps5-kstuff-ldr clean
bash ci-ps5-kstuff-ldr.sh                        # = make ps5-kstuff KSTUFF_OBS=0; make -C ps5-kstuff-ldr
# artifact: ps5-kstuff-ldr/kstuff.elf
shasum -a 256 ps5-kstuff-ldr/kstuff.elf          # compare to ab9a6cb4...
```

Per-Makefile specifics captured: `PROSPER0GDB_OPT=-O0 PAYLOAD_OPT=-O0
UELF_OPT=-O3 UELF_ARCH=-march=znver2` (defaults); payload.elf = host `gcc
-O0 -nostdinc -isystem ../freebsd-headers -nostdlib -static -fno-stack-protector
-fPIE -ffreestanding -no-pie -Wl,-gc-sections -Wl,-z,max-page-size=16384
-DKSTUFF_OBS=0 -DPS5KEK`; uelf = `gcc -O3 -march=znver2 -mgeneral-regs-only
-fPIE -fPIC -shared -nostdlib -Wl,-Bsymbolic -Wl,-z,max-page-size=4096` +
`libbearssl.a` + `isa-l_crypto.a`, then `objcopy --strip-all` → `uelf.bin`;
`payload.elf` → `objcopy -O binary` → `frankenelf.py` (rewrites magic
`\xeb\x0bELF`→`\xeb\x0bPLD`, fixes 2 phdr sizes, pads to 4 — deterministic);
ldr = SDK `$(CC) -O2 -Wall -Werror` + `-lz -lSceSystemService -lSceUserService
-lSceAppInstUtil -lSceFsInternalForVsh -lsqlite3 -Wl,-s`, `.incbin` of
`payload.bin`. CI also runs `make clean` on all four dirs first (ci script).

### 4.3 Nondeterminism inventory (every known source)

1. **Missing submodule pins** (blocker #1 — content unknown from zip).
2. **Compiler versions**: host gcc (for payload.elf/uelf/libs), nasm/yasm
   versions, SDK clang for the ldr. Pinned binary records Ubuntu clang 18.1.3;
   gcc/nasm versions unrecorded → must be bisected from section diffs on bust.
3. **Absolute build path** `/home/echo/drakmor/kstuff-lite/...` embedded
   (must recreate exactly).
4. SDK **static libs** (libc, libSceSystemService, sqlite3, crt objects) —
   version-dependent bytes, linked into kstuff.elf.
5. `.comment`/metadata sections — deterministic given identical toolchains;
   `-Wl,-s` strips symbols from the ldr but not `.comment`.
6. **No timestamps found**: no `__DATE__/__TIME__` in tree; frankenelf and
   Makefiles inject no dates; GNU build-id not observable in the pinned file
   (checked via strings; absence re-check on rebuild). Parallelism (`-j`) is
   safe for Make but not the blocker it usually is — no archives embed order.
7. `syscalls-ps5.py`/`syscalls.py` **fetch syscall tables from the web at
   generation time** (FreeBSD syscalls.master + archived psdevwiki) — the
   generated `syscalls-ps5.asm` is a build input; if absent, a rebuild would
   hit the network AND possibly drift. Must pin the generated `.asm` from the
   release (another zip gap if not regenerated identically).

**Verdict (pre-registered)**: byte-identity is *plausibly achievable* only
after solving (1)(2)(4) and recreating (3); the zip alone cannot do it. If the
rebuild busts, per PHASEB-PLAN P4 the binary remains ground truth; define the
fallback identity pin as: SHA256 over `kstuff.elf` `.text` + the embedded
`payload.bin` blob (extract via the `.incbin` symbol bounds) + section
layout/vaddr table — i.e. hash-of-content-sections, ignoring `.comment` and
link-order-only artifacts. Any divergence beyond that = advisory-only source.

## 5. Safety: lite vs quarantined kstuffSharp

- **Does lite hardcode FW offsets? Yes — per-FW tables, fail-closed.**
  `prosper0gdb/offsets/<fw>.h` for 1.00…13.60 (63 firmwares), all deltas
  relative to the runtime `kdata_base` anchor; `set_offsets()` returns -1 on
  unknown FW (`offsets.c:181`) → `notify("your firmware is not supported
  (prosper0gdb)")` + clean exit before any kernel write (`main.c:1322-1328`).
  Shellcore patches likewise per-FW (1.00…13.60) × retail/testkit/devkit with
  unknown-FW default ⇒ notify + exit, no writes (`main.c:1234-1237`,
  `main.c:1345-1353`). Zero-delta sentinels are explicitly rejected
  (`offsets.c:184-194`).
- **13.60 is a first-class supported table** (`offsets/13_60.h`,
  `shellcore_patches/13_60.h`) — consistent with residency on our console.
- **Why kstuffSharp crashed and lite doesn't**: kstuffSharp shipped one FW-10.01
  hardcoded offset set with no table selection/fail-close and applied writes
  anyway; lite (a) anchors everything to the per-boot `kdata_base`,
  (b) selects a per-FW verified table, (c) refuses to write on unknown FW,
  (d) verifies shellcore text-segment sanity + readback before/after every
  patch (`verify_shellcore_blob`, GOT rollback, `main.c:1086-1141`).
  Residual truth unchanged: lite on a FUTURE unknown FW fails closed
  (notification + exit) rather than crashing — firmware updates are safe-ish
  but kstuff functionality dies until upstream adds the table. The
  `TODO(FW_PORT)` comments mark exactly which fields need re-derivation.
- The INT3/CFI hook approach also means lite does not keep the IDT syscall
  gate patched the old way; still, a second kernel module (kstuffSharp class)
  installing its own IDT/sysent hooks remains prohibited (conflicts).

## 6. Console test plan, updated (observe-only R0–R2, per PHASEB-PLAN §4; 3-restart law)

Unchanged laws: nothing below runs without Claude review + explicit user OK per
step; one user-launched test per boot; klog TCP 3232 to NEW files; no
rapid-fire launches; saves backed up; autoload.txt untouched.

- **R0 (pin acceptance, 3 boots)**: fresh boot → verify etaHEN line
  `kstuff state=1 readable=1 native=… compat=… crypt=deb7/deb7` +
  `resident kstuff found`; decrypt-off fetch of
  `/data/pldmgr/payloads/kstuff-lite/kstuff-lite_v1.11.elf` → magic
  `7f454c46` (expected now — plain ELF, documented above) + sha256
  `ab9a6cb4…`; strings show `Welcome To Kstuff Lite 1.11`. Update to
  PHASEB-PLAN P1: the "SELF container magic expected" branch is dead — record
  the plain-ELF case as the expected outcome, identity via hash.
  Interpretation update: `native=/compat=` values are kstuff's fake sysent
  tables (16 KiB-aligned, per-boot), not stock kernel tables.
- **R1 (CHECK probe)**: our payload (existing l26 CRT shim — already the right
  x86-64 shape) calls `kekcall(0,…,0xffffffff00000027)` once. **Expected:
  return 0 = resident** (corrected polarity). Nonzero = not resident ⇒ refuse.
  Also observable: process pid of `kstuff.elf` via klog (automount-disabled
  message or process name). No other kekcall at R1.
- **R2 (read rungs)**: (a) own-pid `SYS_dynlib_get_info_ex 608` plain call
  (baseline), then (b) kekcall 5 REMOTE_SYSCALL in our OWN pid first
  (e.g. remote getpid — a1=our pid, a2=20, a3=args{0,0,0,0,0,0}), then
  (c) remote 608 against a benign target (our second process if any, else
  SceShellCore read-only like upstream) into our buffer. Success criterion:
  retval 0 + plausible module_info; failure surfaces EFAULT/ESRCH. Do NOT use
  sub-numbers 1/2/3 (hardware registers) in R2 — unnecessary risk; do not use
  6/7 (OBS/fpkg internals). 3 passes across restarts with logs before any L3
  (mdbg_call write) discussion; L3 pattern per §3: write→readback-verify→
  revert receipts (i-cache concerns are void on x86-64; TLB handled by
  target-side mprotect).

## 7. Misc facts worth keeping

- The pinned ELF embeds the upstream build username/path (`echo`/`drakmor`) —
  useful provenance, also a rebuild determinism input (§4.3.3).
- Release build = `KSTUFF_OBS=0` (CI default): kekcall 6 absent, shared area
  8192 B (16 KiB if OBS).
- kstuff's own client stub (`main.c:297`): `kekcall: mov 8(%rsp),%rax; jmp
  *p_kekcall(%rip)` — 7th C arg (the packed number, on stack) into rax, then
  the raw syscall gadget. This is the minimal x86-64 client shape; our CRT
  shim is equivalent.
- The ldr patches its own eboot dynlib segment table to make `dlsym(0x1,…)`
  work on FW ≥ 5.00 (`ldr main.c:577-583`) — irrelevant for our own CRT-based
  stub, relevant if anyone tries the dlsym route.
- Fake sysent tables also hook `SYS_ioctl/nmount/unmount` **only for the
  SceShellCore process** (per-proc `p_sysent` swap) — other processes' ioctls
  are untouched.
- `find_proc` scans pids 1..1024 with KERN_PROC_PID — process-name discovery
  recipe for our tooling (name at offset 447 of the kinfo_proc buffer).

— agent GP (ZCode), 2026-10-10. Analysis only: no builds run, no console
contact, no network, pinned artifacts untouched, extraction in /tmp only.
