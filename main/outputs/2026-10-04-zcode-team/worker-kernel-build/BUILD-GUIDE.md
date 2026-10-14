# BUILD-GUIDE — kstuffSharp extended kernel module (kekcall nr=7 safe_read, nr=8 safe_probe)

Worker: `worker-kernel-build` (build engineering only; no console contact, no game claims).
Pinned source: `.superpowers/native-dependency-spike/sharpprospero-legacy-ctors-1/source/`
(SharpProspero, public pin per the host-tool spike REPORT.md).
Verified on this Mac (darwin 24.6.0, arm64, 16 GB) on 2026-10-06 with the **unmodified**
tree copied to `outputs/2026-10-04-zcode-team/worker-kernel-build/source/`.

---

## 1. What the build produces

kstuffSharp is a two-part payload built end-to-end by `build/build-kstuff.ps1`:

```
samples/kstuffSharp-installer/InstallerApp.csproj   (installer payload, kernel context)
        │  1. dotnet publish -c Release -r linux-x64  (NativeAOT → InstallerApp.o)
        │     └─ PreBuildKernelModule target runs the generator's `kmod` command,
        │        which calls KernelModuleWriter.Build() → KernelModuleEmitter.cs
        │        and writes obj/.../KernelModuleBlobs.g.cs  (kelf + uelf byte arrays)
        │  2. payload link → out/InstallerApp.elf
        ▼
samples/kstuffSharp/SampleApp.csproj                (loader payload)
        │  3. InstallerBlob.g.cs regenerated from the fresh InstallerApp.elf
        │  4. dotnet publish + payload link → out/SampleApp.elf  (embeds installer)
        ▼
  deployable payload ELF (sent with `payload --send` to a listening loader)
```

Any change to `tools/SharpProspero.Link/KernelModuleEmitter.cs` (the 8349-line machine-code
emitter) flows through the `kmod` step automatically. Building only the loader would embed
stale installer bytes — always run the full pipeline.

Verified artifact sizes (unmodified tree): `InstallerApp.elf` 2,808,672 bytes;
`SampleApp.elf` 5,661,152 bytes. Both are `ELF 64-bit LSB pie executable, x86-64`.
Verified end-to-end: the kelf (1,432 B) and uelf (33,320 B) byte arrays emitted by
`KernelModuleEmitter.cs` are present verbatim inside both ELFs (check script in §5).

## 2. Prerequisites

| Need | Version verified | Notes |
|---|---|---|
| .NET SDK | **10.0.401** (`net10.0`, LangVersion 14) | Local pinned install reused from `.superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/` (osx-arm64, SHA-verified there). No global install or PATH change needed. |
| NuGet feed | `https://api.nuget.org/v3/index.json` | Restores `Microsoft.NETCore.App.Runtime.NativeAOT.linux-x64` 10.0.12, `Microsoft.DotNet.ILCompiler` 10.0.12 (+ illink). ~353 MB package cache after first restore. |
| llvm-objcopy | NOT required | Add `-p:StripSymbols=false`; the Unix targets otherwise abort before the ilc step. The pipeline only consumes the emitted `.o`. |
| PowerShell | NOT required on macOS | `build-kstuff.ps1` hardcodes `& powershell` (Windows PS 5.1); use `build-mac.sh` (§3.2) instead. On Windows the script auto-routes the compile through WSL. |
| Native toolchain (clang etc.) | NOT required | The SharpProspero payload linker is managed C# (`SharpProspero.Link`); it writes the ELF itself. |

macOS caveat verified: NativeAOT cross-compilation **to linux-x64 works from this Mac**
(`dotnet publish -r linux-x64` emits an `ELF 64-bit LSB relocatable, x86-64` object).
The publish's final host steps do not fail here (OutputType=Library + NativeLib=Static),
so exit code 0 is normal — unlike the Windows/WSL flow described in `build-app.ps1`,
which expects a failed host link. A benign `ranlib: the table of contents is empty`
warning from Xcode's ranlib appears; the `.a` it touches is not consumed by this pipeline.

## 3. Exact build commands

### 3.1 Authoritative route (Windows with WSL, or Linux)

```powershell
pwsh build/build-kstuff.ps1              # or powershell -ExecutionPolicy Bypass -File build\build-kstuff.ps1
# Add -DiagnosticBreadcrumbs for instrumented ELFs (off by default).
```

### 3.2 Verified Mac route (this session)

```sh
WORK=outputs/2026-10-04-zcode-team/worker-kernel-build
cp -R .superpowers/native-dependency-spike/sharpprospero-legacy-ctors-1/source $WORK/source
. $WORK/build-env/env.sh        # pins DOTNET_ROOT/NUGET_PACKAGES/DOTNET_CLI_HOME (isolated)
bash $WORK/build-mac.sh         # 3-stage equivalent of build-kstuff.ps1
```

`build-mac.sh` mirrors `build-kstuff.ps1` + `build-app.ps1 -Payload` stage by stage:

1. `dotnet publish samples/kstuffSharp-installer/InstallerApp.csproj -c Release -r linux-x64 \
   -p:SharpProsperoRoot=$SRC -p:StripSymbols=false --nologo` → `obj/Release/net10.0/linux-x64/native/InstallerApp.o`
   (the `kmod` target regenerates `KernelModuleBlobs.g.cs` during this publish).
2. Gather runtime support archives from the restored pack
   `microsoft.netcore.app.runtime.nativeaot.linux-x64/<ver>/runtimes/linux-x64/native`
   (libbootstrapper.o + 12 `.a` archives; libbootstrapperdll.o excluded).
3. `dotnet run --project tools/SharpProspero.Bindings.Generator -c Release -- \
   link --kind payload --self-contained --obj InstallerApp.o \
   --sprx libSceLibcInternal --kernel-sprx libkernel_sys.sprx \
   --obj libbootstrapper.o --lib <each .a> --out out/InstallerApp.elf`
4. Regenerate `samples/kstuffSharp/InstallerBlob.g.cs` (byte array of the installer ELF).
5. Repeat 1–3 for `SampleApp.csproj` (`--sprx libSceLibcInternal --sprx libSceSysmodule
   --kernel-sprx libkernel_sys.sprx`) → `out/SampleApp.elf`.

### 3.3 Measured build times (this Mac)

| Phase | Time |
|---|---|
| First installer publish (cold: SDK sentinel + pack downloads + ilc) | ~37 s |
| Full 3-stage pipeline, warm (packs cached, generator prebuilt) | **~42 s** wall |
| Payload links (each, after compile) | < 2 s |

## 4. Extension point in KernelModuleEmitter.cs (nr=7 safe_read, nr=8 safe_probe)

All line numbers refer to the pinned tree's `tools/SharpProspero.Link/KernelModuleEmitter.cs`.

### 4.1 How a kekcall reaches the handler

Userspace calls `__sp_kekcall` (CRT shim, `LibraryImport("libScePosix")` in
`src/SharpProspero/Payload/Kernel/Kekcall.cs:119`), packing the kekcall number into
SYS_getppid(39). The module hooks sysent[39]; `EmitSyscallHandler` routes to the handler
registered as internal target `kekcall` (`KernelModuleWriter.cs:75,83`).

### 4.2 The dispatch site to extend — `EmitKekcallHandler()` (lines 4200–4478)

- nr is loaded from the trap frame's `args[RSI]` into `r12d` (lines 4214–4220).
- Dispatch is a sequential `cmp r12d, N / jne` chain, NOT a jump table:
  - nr=1 read debug regs — line 4222
  - nr=2 write debug regs — line 4315 (continuation trap 1)
  - nr=3 read MSR — line 4360
  - nr=5 remote syscall — line 4373 (continuation traps 2/3/4)
  - nr=0xFFFFFFFF ping — line 4431
  - ENOSYS fallthrough (returns 78) — lines 4441–4444
  (nr=4 is unused; nr=6 Snapshot exists only as a userspace constant, `Kekcall.cs:62`,
  and retail modules return 0 — it is not emitted here.)

**Where nr=7/nr=8 go:** insert two new cases between the end of the nr=5 block
(line 4429, after `jmpEpilogueNr5 = EmitJmpRel32Forward()`) and the ping case
(line 4431). Mechanically:

1. Repoint the existing `jneNrPing` forward jump (emitted at line 4376) to the new
   nr=7 case start instead of the ping block.
2. nr=7 case: `_code.AddRange([0x41, 0x83, 0xFC, 0x07]); // cmp r12d, 7` + forward
   `jne` to the nr=8 case; nr=8 case likewise (`0x08`) with its `jne` repointed to the
   existing ping block (patch the old `jneEnosys` chain position accordingly).
3. Every success path ends `xor eax,eax` + a `EmitJmpRel32Forward()` epilogue jump —
   add the new jump labels to the epilogue patch group at lines 4464–4469.
4. Every checked helper failure path needs a forward `jnz` label — add them to the
   EFAULT group at lines 4453–4461 (pattern: `mov eax, 14`).
5. Stack budget: the prologue allocates 0xC0 (192) bytes at line 4209 for local
   frames (nr=5 uses a 128-byte continuation frame in it). The 32-byte
   `sr_request`/`sr_probe_request` copyin scratch fits; if a larger frame is needed,
   raise `EmitSubRspImm32` — it is handler-local and safe to grow.

### 4.3 The continuation recipe (copy what nr=5 does, lines 4378–4429)

For a copyin-based request (both nr=7 and nr=8 take `a1 = user pointer` per
worker-kernel-saferead's `safe_read_kernel.c` §6):

1. Build a continuation frame on the local stack: `[0]=doreti_iret`,
   `[1]=MKTRAP(KEKCALL,n)` = `((ulong)0xDEAD0001 << 32) | n` with **n = 5 for
   safe_read, 6 for safe_read's copyout leg, 7 for safe_probe** (1–4 are taken,
   see `EmitKekcallTrapHandler` lines 4525/4883–4888), remaining slots carry saved
   state (caller td, user buf, len, resolved pid).
2. `push_stack_checked(regs, frame, size)` (kernel stack of the calling thread).
3. Redirect the trap frame: `RIP=copyin` (`CopierIn`), `RDI=user req ptr`,
   `RSI=kernel scratch (RSP+off)`, `RDX=len`; return 0.
4. When copyin completes, the main doreti dispatcher decodes
   `TRAP_KIND=0xDEAD0001` and calls `kekcall_trap_handler(regs, TRAP_IDX)`
   (dispatch at KernelModuleEmitter.cs lines 866–905).
5. Add the new sub-index cases to `EmitKekcallTrapHandler()` (lines 4507+)
   following the trap=2 pid-walk body:
   - trap=5 (safe_read): resolve pid → proc, read target memory via the page-table
     path into a kelf-local scratch buffer, then redirect a copyout to the caller's
     `req->buf` (nr=1's redirect pattern, lines 4300–4309).
   - trap=6 (safe_read copyout done): `copy_to_kernel(caller_td + 0x408, &n, 8)`
     writes `td_retval[0] = bytes read` (td_retval = 0x408, as at lines 4912–4927),
     pop frame, restore RIP.
   - trap=7 (safe_probe): compute the probe verdict, write it via
     `copy_to_kernel(caller_td + 0x408, &code, 8)`, pop frame, restore RIP.
   Failure paths in the trap handler mirror the T2/T3 groups (pop residual bytes,
   restore RIP, set EFAULT), lines 4949–4963.

### 4.4 Helper functions available to the new cases

Internal (emitted into the kelf; registered in `KernelModuleWriter.Build()`):

| Helper | Emitter | ABI |
|---|---|---|
| `copy_from_kernel` | `EmitCopyFromKernel` (5508) | RDI=dst local, RSI=src **kernel** VA, RDX=len; 0/EFAULT; walks pages via virt2phys + DMEM |
| `copy_to_kernel` | `EmitCopyToKernel` (5621) | RDI=dst kernel VA, RSI=src local, RDX=len |
| `virt2phys` | `EmitVirt2Phys` (5353) | RDI=VA, RSI=&phys, RDX=&page limit; EAX=1 ok / 0 fault |
| `push/pop/peek_stack_checked` | 6420/6477/6528 | thread kernel-stack ops with bounds checks |
| `rdmsr_fn`, `read/write_dbgregs_checked`, `run_gadget_checked` | – | existing subsystem helpers |

External (installer patches firmware addresses at deploy; see
`src/SharpProspero/Payload/Kernel/KernelOffsets.cs`):
`kpeek64_checked`, `kpoke64`, `copyin`/`copyout` (`CopierIn`/`CopierOut` redirect
targets), `doreti_iret`, `nop_ret`, `dmem_base`, `cr3_phys_addr`.

pid → proc resolution (copy from trap 2): start at `saved_td + td_proc`, skip
allproc sentinels (`proc < -0x100000000` signed → follow `proc+8` le_prev), walk
`p_list.le_next` at `proc+0`, compare `p_pid` at `proc+0xBC` (4 bytes), first thread
at `proc+0x10` — lines 4631–4712. `p_vmspace` is at `proc+0x200` (saferead design;
confirm against KernelOffsets.cs when implementing).

Cross-process translation caveat: `EmitVirt2Phys` reads the CR3 baked at its
`cr3_phys_addr` patch site (kernel context). safe_read must translate through the
**target** process's `vmspace→pmap→pm_cr3`; that needs either a new emitted helper
(e.g. `virt2phys_cr3(cr3, va, &phys, &limit)` placed beside `EmitVirt2Phys` and
registered like `CopyFromKernel` in `KernelModuleWriter` §8) or an inline walk in
the trap-5 body. Reusing only existing external targets avoids touching the
installer's firmware offset tables; any NEW external target must be added there.

Userspace: add `public const int SafeRead = 7;` / `SafeProbe = 8;` plus wrappers to
`src/SharpProspero/Payload/Kernel/Kekcall.cs` (constants at lines 42–62; the
`__sp_kekcall` shim itself is generic — no CRT change). The 64 KiB read scratch
(`SR_MAX_LEN`) must be kelf BSS/data (emit a data area like `EmitCryptoRequestCache`,
`KernelModuleWriter.cs:185`), not handler stack.

### 4.5 Build-after-edit checklist

1. Edit `KernelModuleEmitter.cs` (and `Kekcall.cs` constants; helper emitters if added).
2. Re-run the full pipeline (§3) — `kmod` regenerates `KernelModuleBlobs.g.cs`; the
   printed `kmod: kelf N bytes, uelf M bytes` sizes MUST change if the emitter changed.
3. Confirm both ELFs embed the fresh blobs (§5).
4. Deploy `SampleApp.elf` with
   `dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send --host <address> --file <SampleApp.elf>`.
   Note: the already-loaded gate (`KEKCALL_CHECK(-1)`) refuses to overwrite a live
   module; a new kelf takes effect after the next boot install (per saferead design §0).

## 5. Verification recipes (used in this session)

```sh
file source/samples/kstuffSharp*/out/*.elf
# → ELF 64-bit LSB pie executable, x86-64 ... (both)

# prove the emitter's blobs are embedded end-to-end:
python3 - <<'PY'
import re
W="outputs/2026-10-04-zcode-team/worker-kernel-build"
src=open(f"{W}/source/samples/kstuffSharp-installer/obj/Release/net10.0/linux-x64/KernelModuleBlobs.g.cs").read()
kelf=bytes(int(b,16) for b in re.findall(r"0x([0-9A-F]{2})", src.split("Kelf => [")[1].split("];")[0]))
uelf=bytes(int(b,16) for b in re.findall(r"0x([0-9A-F]{2})", src.split("Uelf => [")[1].split("];")[0]))
inst=open(f"{W}/source/samples/kstuffSharp-installer/out/InstallerApp.elf","rb").read()
load=open(f"{W}/source/samples/kstuffSharp/out/SampleApp.elf","rb").read()
assert kelf in inst and uelf in inst and kelf in load and uelf in load
print("embedded OK", len(kelf), len(uelf))
PY
```

Build logs retained: `build-env/installer-publish.log`, `installer-publish2.log`,
`full-build.log`.

## 6. Environment / reproducibility notes

- All work stayed under `outputs/2026-10-04-zcode-team/worker-kernel-build/`
  (source copy, isolated `DOTNET_CLI_HOME`, isolated `NUGET_PACKAGES`). The pinned
  `.superpowers` tree was read-only for this task; no global installs, no PATH edits.
- The unmodified build was verified only: nr=7/nr=8 emission itself is owned by the
  saferead/emitter authors; this guide is the build-engineering contract for it.
- No console contact, no payload send, no game claims. On-device behavior (trap tags,
  firmware offsets, target CR3 walk) is design-level and marked as such in §4.
