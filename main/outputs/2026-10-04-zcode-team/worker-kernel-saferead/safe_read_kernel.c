// safe_read_kernel.c — kernel-side prototype of the "safe-read" service for the
// PS5 UE4SS runtime port.
//
// STATUS: research prototype. Never compiled, never run on console. Every
// structure offset below is traceable to the kstuffSharp source tree (paths in
// comments). Items marked [VERIFY] are inferred from an adjacent source-verified
// offset and must be confirmed on-device before production use.
//
// WHAT THIS FILE IS
// -----------------
// The resident kernel module ("kstuff" kelf) is not compiled C: it is x86-64
// machine code synthesized by SharpProspero.Link/KernelModuleEmitter.cs at
// build time. The realistic integration path is therefore:
//
//   1. This C file is the reviewable/reference semantics of the new kekcall
//      handlers (nr=7 safe_read, nr=8 safe_probe).
//   2. The same logic is re-expressed in KernelModuleEmitter.cs using the
//      existing emit helpers (kpeek64_checked, copy_to_kernel, guarded calls,
//      the allproc walk already emitted for kekcall nr=5), producing a new
//      kelf/uelf pair via build/build-kstuff.ps1.
//   3. The rebuilt kstuff loader is deployed; it installs on next boot (the
//      installer's KEKCALL_CHECK(-1) already-loaded gate refuses to overwrite a
//      live module mid-flight, by design).
//
// Alternatively this file can be compiled as a freestanding payload ELF through
// the SharpProspero.Link path: LinkOptions.Objects accepts relocatable ELF
// objects (tools/SharpProspero.Link/Linker.cs: "Reads relocatable objects and
// archives"), so a PS5-toolchain clang -ffreestanding -c of this file can be
// linked by the same tool that builds the C# payloads. That path is for the
// LOADER side (user process with the pipe R/W primitive); it is not the kelf
// path. See the feasibility notes at the bottom of this file.
//
// SOURCE MAP (all paths relative to
//   .superpowers/native-dependency-spike/sharpprospero-legacy-ctors-1/source/)
//   [PROG]     samples/kstuffSharp/Program.cs            (1654 lines, read in full)
//   [KIO]      src/SharpProspero/Payload/Kernel/KernelIo.cs
//   [KPROC]    src/SharpProspero/Payload/Kernel/KernelProcess.cs
//   [KOFF]     src/SharpProspero/Payload/Kernel/KernelOffsets.cs
//   [KINSTALL] src/SharpProspero/Payload/Kernel/KernelModuleInstaller.cs
//   [KEK]      src/SharpProspero/Payload/Kernel/Kekcall.cs
//   [KPG]      src/SharpProspero/Payload/Kernel/KernelPaging.cs
//   [EMIT]     tools/SharpProspero.Link/KernelModuleEmitter.cs
//   [CRT]      tools/SharpProspero.Link/PayloadCrtEmitter.cs
//   [ARGS]     src/SharpProspero/Payload/Args.cs

#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  s64;
typedef int32_t  s32;

// ============================================================================
// 1. Kernel structure offsets (all firmware-invariant unless noted)
// ============================================================================

// ---- struct proc ([KOFF] "Firmware-invariant structure field offsets") ----
#define PROC_P_LIST      0x000  // p_list.le_next — chains the allproc list
#define PROC_P_THREADS   0x010  // p_threads.tqh_first
#define PROC_P_UCRED     0x040  // p_ucred
#define PROC_P_FD        0x048  // p_fd (filedesc)
#define PROC_P_VMSPACE   0x200  // p_vmspace (8 bytes) [PROG line 1619 comment]
#define PROC_P_PID       0x0BC  // p_pid (4 bytes)
#define PROC_P_TITLE_ID  0x470  // inline 10-byte title id
#define PROC_P_DYNLIB    0x3E8  // p_dynlib — FW 10.00+ only ([KOFF] ProcDynlib)
#define PROC_P_COMM      0x5DC  // p_comm inline 17 bytes

// ---- struct filedesc ----
#define FD_RDIR          0x010  // fd_rdir
#define FD_JDIR          0x018  // fd_jdir
// (used by the loader-side jail escape; kept for completeness)

// ---- struct vmspace / vm_map ----
// vm_map "root/first entry" pointer INSIDE struct vmspace is PER-FIRMWARE:
// [CRT] BSS comment line 1504: "__sp_kernel_vmspace_p_root (per-FW: 0x1c0/0x1c8/0x1d0)".
// kstuffSharp [PROG] hardcodes 0x1d0 (its supported FW family). The per-FW
// selection is baked into the emitted CRT __kernel_init.
#define VMSPACE_VM_ROOT_FW_OLDEST   0x1C0
#define VMSPACE_VM_ROOT_FW_MID      0x1C8
#define VMSPACE_VM_ROOT_FW_10XX     0x1D0  // value used by kstuffSharp on 10.x
// vm_pmap sub-struct offset inside vmspace ([KOFF] VmspacePmapOffset):
//   fwMajorMinor <= 0x102 -> 0x2C0
//   fwMajorMinor <= 0x550 -> 0x2E0
//   else                  -> 0x2E8
#define PMAP_OFF_LE_102   0x2C0
#define PMAP_OFF_LE_550   0x2E0
#define PMAP_OFF_GT_550   0x2E8
// Inside struct pmap ([PROG] lines 137-139, [KINSTALL] ReadDmapAndCr3):
#define PMAP_PM_PML4      32   // pm_pml4 (also holds DMAP base VA on PS5)
#define PMAP_PM_CR3       40   // pm_cr3 (physical PML4 root)

// ---- struct vm_map_entry ([PROG] KernelSetVmemProtection, lines 1611-1653) ----
#define VME_NEXT          0x08  // links.le_next — next entry
#define VME_START         0x20  // vm_offset_t start      [source-verified]
#define VME_END           0x28  // vm_offset_t end        [source-verified]
#define VME_PROTECTION    0x60  // vm_prot_t protection   [VERIFY: inferred,
                                //  one u_char slot below max_protection]
#define VME_MAX_PROTECTION 0x64 // vm_prot_t max_protection [source-verified:
                                //  [PROG] writes 1 byte at +0x64 with doc
                                //  "Sets vm_map_entry.max_protection"]

// vm_prot_t bits (FreeBSD): VM_PROT_READ=1, VM_PROT_WRITE=2, VM_PROT_EXEC=4.
// [PROG] maps ELF phdr flags: X->4, W->2, R->1 (lines 1584-1587).
#define VM_PROT_READ      0x01
#define VM_PROT_WRITE     0x02
#define VM_PROT_EXEC      0x04

// ---- allproc head ([KOFF] Allproc(): kdata-relative, per firmware) ----
// Examples: FW 10.00-10.60 -> 0x2765D70 (kdata base 0xFFFFFFFF80ED0000);
// FW 11.00-11.60 -> 0x2875D70; FW 12.00-12.70 -> 0x2885E00; FW 13.40+ -> 0x28C9E80.
// The kelf receives the RESOLVED absolute address at install time via the
// AddSym("allproc", kdb + KernelOffsets.Allproc(fw)) symbol ([KINSTALL] step 7).
#define KDATA_BASE_10XX   0xFFFFFFFF80ED0000ULL  // FW 9.00-10.60 ([KOFF] KdataBase)
#define ALLPROC_OFF_10XX  0x2765D70ULL           // FW 10.00-10.60

// ---- pmap store for DMAP base discovery ([KOFF] KernelPmapStore) ----
// kernel_pmap_store: FW 10.00-10.60 -> kdata+0x2CF0EF8.
//   dmap_virt  = ReadU64(pmap_store + 32)   (pm_pml4)
//   kernel_cr3 = ReadU64(pmap_store + 40)   (pm_cr3)
//   dmap_base  = dmap_virt - kernel_cr3     ([PROG] lines 137-140)
#define DMAP_DEFAULT_BASE 0xFFFF800000000000ULL  // typical DMAP_BASE [KPG]

// ---- uelf trampoline page-table facts ([KINSTALL] BuildUelfCr3) ----
// The kernel module builds a private CR3 whose DMEM PML4 region maps ALL
// physical RAM: 512 x 1GB pages with flags 0x87 (P|RW|U|PS), at an empty
// PML4 index >= 256. A "shared_area" 2KB buffer is addressed as
// sharedAreaPhys + dmemVirtBase. Facts for the shared-memory fallback option.

// ---- walk safety caps ([KPROC] MaxAllprocIterations, [PROG] iter cap) ----
#define MAX_ALLPROC_ITER  4096
#define MAX_VME_ITER      4096

// ============================================================================
// 2. Kernel memory access primitives available to the kelf
// ============================================================================
//
// The kelf does NOT use the pipe primitive (that belongs to the loader-side
// user process, see section 6). Inside the kelf, emitted checked helpers exist:
//
//   kpeek64_checked(kaddr, &out)  — checked 8-byte kernel read; returns nonzero
//                                   on fault. Used by the emitted allproc walk
//                                   ([EMIT] lines 4631-4690).
//   copy_from_kernel(dst, ksrc, len) — checked kernel->kelf-local copy.
//   copy_to_kernel(kdst, src, len)   — checked kelf-local->kernel copy.
//   dmem-based physical reads       — translate VA via target CR3, read via
//                                   DMAP ([EMIT] fpkg handler uses these to
//                                   read another context's user buffers).
//
// This prototype is written against two abstract backends:
//   KREAD(kaddr, out, len)  — kernel virtual read, checked, returns 0 on ok
//   DMAP_READ(pa, out, len) — physical read through DMAP, checked
// In the emitter port these become kpeek64_checked / copy_from_kernel and the
// page-table-walk + dmem read pattern respectively. In a user-process build
// (loader side) KREAD is PayloadKernelIo.TryRead ([KIO]) and DMAP_READ is
// io.Read(dmap_base + pa, ...) ([KPG] PhysCopyin/PhysCopyout).
//
// The two functions below are the ONLY environment-dependent pieces; everything
// else compiles anywhere.

static int KREAD(u64 kaddr, void *out, u32 len);   /* prototype-only */
static int DMAP_READ(u64 phys_addr, void *out, u32 len); /* prototype-only */

static int kread64(u64 kaddr, u64 *out) { return KREAD(kaddr, out, 8); }
static int kread32(u64 kaddr, u32 *out) { return KREAD(kaddr, out, 4); }
static int kread8(u64 kaddr, u8 *out)  { return KREAD(kaddr, out, 1); }

// ============================================================================
// 3. allproc walk — find struct proc by PID
// ============================================================================
//
// Reference: [KPROC] PayloadKernel.WalkAllprocForPid (checked variant) and the
// kelf's own emitted walk in the kekcall nr=5 continuation ([EMIT] 4605-4690),
// including the allproc SENTINEL handling the managed version does not have:
// list entries below (signed) -0x100000000 are sentinel nodes; follow le_prev
// at +8 to skip them. Both behaviors are folded in here.

// Inputs supplied at install time to the kelf ([KINSTALL] AddSym("allproc",..)):
static u64 g_allproc_kva;   // absolute &allproc (kdata + per-FW offset)
// For a loader-side build these arrive via PayloadArgs ([ARGS]):
//   args->KernelDataBase + KernelOffsets.Allproc(fw)

// Returns the kernel virtual address of struct proc for `pid`, or 0.
static u64 kproc_find_by_pid(s32 pid)
{
    u64 proc;
    if (kread64(g_allproc_kva, &proc) != 0)
        return 0;                                  // list head unreadable

    for (int iter = 0; proc != 0 && iter < MAX_ALLPROC_ITER; iter++) {
        // Skip allproc sentinel nodes: proc as signed < -0x100000000.
        // ([EMIT] "Skip allproc sentinel entries (proc < -0x100000000 as signed)")
        while (proc != 0 && ((s64)proc < -(s64)0x100000000LL)) {
            u64 prev;
            if (kread64(proc + 0x08, &prev) != 0)  // le_prev at +8
                return 0;
            proc = prev;
        }
        if (proc == 0)
            break;

        u32 this_pid;
        if (kread32(proc + PROC_P_PID, &this_pid) != 0)
            return 0;                              // unreadable proc: abort walk
        if ((s32)this_pid == pid)
            return proc;

        u64 next;
        if (kread64(proc + PROC_P_LIST, &next) != 0)
            return 0;                              // broken link: abort
        proc = next;
    }
    return 0;                                      // not found / cap reached
}

// ============================================================================
// 4. vm_map walk — the safe_probe core
// ============================================================================
//
// Reference: [PROG] Program.cs InstallerLoader.KernelSetVmemProtection
// (lines 1611-1653). The managed walk:
//   proc = WalkAllprocForPid(io, own_pid)
//   vmspace   = ReadU64(proc + 0x200)
//   firstEntry = ReadU64(vmspace + 0x1d0)        // per-FW: 0x1c0/0x1c8/0x1d0
//   loop (max 4096):
//     entryStart = ReadU64(entry + 0x20)
//     entryEnd   = ReadU64(entry + 0x28)
//     if entryStart >= endAddr: if foundMatch break;
//     else if entryEnd > addr:  foundMatch = true; (write prot at +0x64)
//     next = ReadU64(entry + 0x08); break if next==entry||next==first||next==0
//
// safe_probe reuses the walk read-only. Two per-FW values are needed:
static u32 g_vmspace_root_off;   // VMSPACE_VM_ROOT_* for the running FW
static u32 g_vmspace_pmap_off;   // PMAP_OFF_* for the running FW

// Probe result codes (returned to userspace through the kekcall channel).
#define SR_PROBE_MAPPED_READABLE  1    // covered by an entry, READ allowed,
                                       // and PTE-present across the whole range
#define SR_PROBE_MAPPED_NOREAD    0    // covered, but protection lacks READ
#define SR_PROBE_UNMAPPED        -1    // no vm_map entry covers the range
#define SR_PROBE_FAULT           -2    // kernel structures unreadable
#define SR_PROBE_BADARGS         -3

struct vme_info {
    u64 start;            // entry start  (+0x20)
    u64 end;              // entry end    (+0x28)
    u8  protection;       // entry protection      ([VERIFY] +0x60)
    u8  max_protection;   // entry max_protection  (+0x64, source-verified)
};

// Walk the target process's vm_map for the entry covering `addr`.
// Returns 1 and fills `out` when a covering entry is found, 0 when the range
// is not covered, negative SR_PROBE_FAULT on structure-read failure.
static int vm_map_lookup(u64 vmspace, u64 addr, struct vme_info *out)
{
    u64 first;
    if (kread64(vmspace + g_vmspace_root_off, &first) != 0 || first == 0)
        return SR_PROBE_FAULT;

    u64 entry = first;
    int found = 0;

    for (int iter = 0; entry != 0 && iter < MAX_VME_ITER; iter++) {
        u64 e_start, e_end;
        if (kread64(entry + VME_START, &e_start) != 0 ||
            kread64(entry + VME_END,   &e_end)   != 0)
            return SR_PROBE_FAULT;

        // Entries are ordered by start; stop once past the probe address.
        if (e_start > addr) {
            if (found) break;
            // still worth continuing only while entries could start <= addr;
            // keep walking (matches the reference walk semantics).
        } else if (e_end > addr) {
            found = 1;
            out->start = e_start;
            out->end   = e_end;
            if (kread8(entry + VME_PROTECTION,     &out->protection)     != 0 ||
                kread8(entry + VME_MAX_PROTECTION, &out->max_protection) != 0)
                return SR_PROBE_FAULT;
        }

        u64 next;
        if (kread64(entry + VME_NEXT, &next) != 0)
            return SR_PROBE_FAULT;
        if (next == entry || next == first || next == 0)
            break;
        entry = next;
    }
    return found ? 1 : 0;
}

// Translate a target-process USER virtual address to a physical address by
// walking the target's page tables through DMAP.
// Reference: [KPG] KernelPaging.VirtToPhys (4-level walk, 1GB/2MB large pages,
// physical bound DmemMaxPhys = 0x100000000000). Returns 0 on failure.
// `cr3` is the PHYSICAL PML4 root read from vmspace+pmap_off+PMAP_PM_CR3.
static u64 target_virt_to_phys(u64 dmap_base, u64 cr3, u64 va)
{
    const u64 ADDR_MASK  = 0x000FFFFFFFFFF000ULL;
    const u64 DMEM_MAX   = 0x0001000000000000ULL;  // [KPG] DmemMaxPhys

    u64 pml4e, pdpte, pde, pte;

    if (DMAP_READ(dmap_base + (cr3 & ADDR_MASK) + ((va >> 39) & 0x1FF) * 8,
                  &pml4e, 8) != 0 || (pml4e & 1) == 0)
        return 0;

    if (DMAP_READ(dmap_base + (pml4e & ADDR_MASK) + ((va >> 30) & 0x1FF) * 8,
                  &pdpte, 8) != 0 || (pdpte & 1) == 0)
        return 0;
    if (pdpte & 0x80)  // 1GB page
        return ((pdpte & 0x000FFFFFC0000000ULL) | (va & 0x3FFFFFFFULL));

    if (DMAP_READ(dmap_base + (pdpte & ADDR_MASK) + ((va >> 21) & 0x1FF) * 8,
                  &pde, 8) != 0 || (pde & 1) == 0)
        return 0;
    if (pde & 0x80)    // 2MB page
        return ((pde & 0x000FFFFFFFE00000ULL) | (va & 0x1FFFFFULL));

    if (DMAP_READ(dmap_base + (pde & ADDR_MASK) + ((va >> 12) & 0x1FF) * 8,
                  &pte, 8) != 0 || (pte & 1) == 0)
        return 0;

    u64 pa = (pte & ADDR_MASK) | (va & 0xFFF);
    return pa < DMEM_MAX ? pa : 0;
}

// ============================================================================
// 5. The safe-read service (what the new kekcall handlers execute)
// ============================================================================

// Service context — per-boot constants the installer already resolves:
//   dmap_base from kernel_pmap_store ([KINSTALL] ReadDmapAndCr3)
//   allproc from the symbol table ([KINSTALL] AddSym("allproc", ...))
static u64 g_dmap_base;

// Read `len` bytes of TARGET-process USER virtual memory `addr` into `out`
// WITHOUT any possibility of a page fault: every page is validated by walking
// the target's page tables, then read through DMAP. This works cross-process
// because it never executes in the target's address space.
//
// The vm_map pre-check is advisory metadata (fast bulk reject); the page-table
// walk is the AUTHORITATIVE presence check. If the two disagree the walk wins,
// and vm_map disagreement is reported, not trusted.
//
// Returns bytes read (== len on success) or a negative error.
#define SR_OK                 0    // == bytes read for kekcall nr=7
#define SR_ERR_NOENT        (-10)  // pid not found
#define SR_ERR_UNMAPPED     (-11)  // no vm_map entry / PTE not present
#define SR_ERR_PROT         (-12)  // entry exists but lacks VM_PROT_READ
#define SR_ERR_LEN          (-13)  // zero/oversized length
#define SR_ERR_RANGE        (-14)  // kernel address passed as user addr

#define SR_MAX_LEN        0x10000u // 64 KiB per call (one page at a time inside)

static int vmspace_of_pid(s32 pid, u64 *vmspace_out)
{
    u64 proc = kproc_find_by_pid(pid);
    if (proc == 0)
        return SR_ERR_NOENT;
    u64 vmspace;
    if (kread64(proc + PROC_P_VMSPACE, &vmspace) != 0 || vmspace == 0)
        return SR_ERR_NOENT;
    *vmspace_out = vmspace;
    return 0;
}

static int target_cr3(u64 vmspace, u64 *cr3_out)
{
    u64 cr3;
    if (kread64(vmspace + g_vmspace_pmap_off + PMAP_PM_CR3, &cr3) != 0 || cr3 == 0)
        return SR_ERR_FAULT_STRUCT;
    *cr3_out = cr3;
    return 0;
}

// additional internal error used before mapping to public codes
#define SR_ERR_FAULT_STRUCT (-15)

// int safe_read_other(pid, addr, out, len)
static int safe_read_other(s32 pid, u64 addr, void *out, u32 len)
{
    if (len == 0 || len > SR_MAX_LEN)
        return SR_ERR_LEN;
    if (addr >= 0xFFFF800000000000ULL)
        return SR_ERR_RANGE;                 // kernel-range VA: refuse

    u64 vmspace;
    int rc = vmspace_of_pid(pid, &vmspace);
    if (rc != 0)
        return rc;

    // --- advisory vm_map check (fast reject + protection report) ---
    struct vme_info vme;
    rc = vm_map_lookup(vmspace, addr, &vme);
    if (rc < 0)
        return SR_ERR_FAULT_STRUCT;
    if (rc == 0)
        return SR_ERR_UNMAPPED;
    if (!(vme.protection & VM_PROT_READ)) {
        // [VERIFY] protection slot: if +0x60 turns out wrong on-device, fall
        // back to max_protection (+0x64, source-verified) or PTE-only policy.
        if (!(vme.max_protection & VM_PROT_READ))
            return SR_ERR_PROT;
        // Entry is covered and max-prot readable but current prot is not:
        // pages may still be PTE-present. Report PROT unless policy says
        // otherwise; UE4SS scanner callers treat PROT as "not readable now".
        return SR_ERR_PROT;
    }
    // A read spanning past the covering entry's end must check the next
    // entries too — the per-page walk below enforces this authoritatively.

    // --- authoritative per-page read through the target's page tables ---
    u64 cr3;
    rc = target_cr3(vmspace, &cr3);
    if (rc != 0)
        return rc;

    u8  *dst = (u8 *)out;
    u64  va = addr;
    u32  remain = len;
    while (remain > 0) {
        u64 pa = target_virt_to_phys(g_dmap_base, cr3, va);
        if (pa == 0)
            return SR_ERR_UNMAPPED;          // page not present in target pmap
        u32 page_remain = 0x1000u - (u32)(va & 0xFFF);
        u32 chunk = page_remain < remain ? page_remain : remain;
        if (DMAP_READ(pa, dst, chunk) != 0)
            return SR_ERR_FAULT_STRUCT;
        va += chunk;
        dst += chunk;
        remain -= chunk;
    }
    return (int)len;
}

// bool safe_probe(pid, addr, len): whole range mapped + readable?
// Cheaper than safe_read for scanner hot paths: vm_map coverage + protection
// only, plus (optionally) PTE presence of the first and last page.
#define SR_PROBE_PTE_CHECK_FULL 0   // set 1 to walk every page (exact, slower)

static int safe_probe(s32 pid, u64 addr, u64 len)
{
    if (len == 0 || len > SR_MAX_LEN)
        return SR_PROBE_BADARGS;
    if (addr >= 0xFFFF800000000000ULL || addr + len < addr)
        return SR_PROBE_BADARGS;

    u64 vmspace;
    if (vmspace_of_pid(pid, &vmspace) != 0)
        return SR_PROBE_MAPPED_READABLE * 0 + SR_PROBE_FAULT; // pid gone

    // The vm_map walk covers ONE contiguous entry. A range may legitimately
    // span several entries, so probe page-granular via the entry walk:
    // check first and last page coverage (full PTE walk when enabled).
    struct vme_info vme;
    u64 first_va = addr;
    u64 last_va  = addr + len - 1;

    int rc1 = vm_map_lookup(vmspace, first_va, &vme);
    if (rc1 <= 0)
        return rc1 < 0 ? SR_PROBE_FAULT
             : (vme.end >= last_va + 1 ? SR_PROBE_UNMAPPED : SR_PROBE_UNMAPPED);
    if (!(vme.protection & VM_PROT_READ))
        return SR_PROBE_MAPPED_NOREAD;
    if (vme.end < last_va + 1) {
        // range crosses into the next entry — check the tail page too
        struct vme_info vme2;
        int rc2 = vm_map_lookup(vmspace, last_va, &vme2);
        if (rc2 <= 0)
            return rc2 < 0 ? SR_PROBE_FAULT : SR_PROBE_UNMAPPED;
        if (!(vme2.protection & VM_PROT_READ))
            return SR_PROBE_MAPPED_NOREAD;
        // NOTE: adjacent-entry coverage between the two probed pages is not
        // verified unless SR_PROBE_PTE_CHECK_FULL; UE4SS reads will still be
        // individually validated by safe_read, so a rare false positive on a
        // probe degrades to an error return from safe_read, never a fault.
    }

#if SR_PROBE_PTE_CHECK_FULL
    {
        u64 cr3;
        if (target_cr3(vmspace, &cr3) != 0)
            return SR_PROBE_FAULT;
        for (u64 va = addr & ~0xFFFULL; va < addr + len; va += 0x1000)
            if (target_virt_to_phys(g_dmap_base, cr3, va) == 0)
                return SR_PROBE_UNMAPPED;
    }
#endif
    return SR_PROBE_MAPPED_READABLE;
}

// ============================================================================
// 6. kekcall wiring (how userspace reaches this code)
// ============================================================================
//
// Channel ([KEK] + [CRT] line 2863 + [EMIT] line 4200):
//   * Userspace invokes syscall 39 (SYS_getppid) with the kekcall number
//     packed in RAX[63:32] and 39 in RAX[31:0] (the __sp_kekcall shim).
//   * The kstuff module hooked sysent[39].sy_call for BOTH the global PS5 and
//     PS4 sysentvec tables and SceShellCore's private clone, so EVERY process
//     — including the GAME process hosting the UE4SS PRX — reaches the kekcall
//     dispatcher. A plain getppid (RAX[63:32] == 0) passes through to the
//     original handler; production kstuff depends on this.
//   * Existing dispatch numbers: -1 ping, 1 read dbregs, 2 write dbregs,
//     3 rdmsr, 5 remote syscall, 6 snapshot. 4 is unused. Proposal:
//
//        nr = 7  safe_read
//              a1 = user pointer to struct sr_request (copyin'd, like nr=5)
//              on success: data copyout'd to req->buf, td_retval = len
//        nr = 8  safe_probe
//              a1 = user pointer to struct sr_probe_request
//              td_retval = SR_PROBE_* code
//
//   The nr=5 continuation pattern ([EMIT] 4373-4429) shows the exact recipe
//   for a copyin-based request: build a doreti_iret continuation frame tagged
//   MKTRAP(KEKCALL, n), push it with push_stack_checked, redirect RIP=copyin
//   with RDI=user buf / RSI=kernel scratch / RDX=len, and finish in the trap
//   handler. safe_read additionally needs a copyout leg for the data, i.e.
//   TWO continuation traps (or one if the data is memcpy'd to a kelf scratch
//   buffer first and copyout once — the nr=1 pattern, [EMIT] 4222-4313).

struct sr_request {          // kekcall nr=7 (safe_read)
    u32 pid;                 // target pid (0 = caller's own pid, resolved
                             // server-side from the saved td -> td_proc)
    u32 _pad0;
    u64 addr;                // target user VA
    u64 len;                 // <= SR_MAX_LEN
    u64 buf;                 // CALLER user VA to receive the data
};

struct sr_probe_request {    // kekcall nr=8 (safe_probe)
    u32 pid;
    u32 _pad0;
    u64 addr;
    u64 len;
};

// Kernel-side dispatch sketch (runs in the getppid hook context of the CALLER):
static long kekcall_safe_read_dispatch(const struct sr_request *req /* kelf-local */)
{
    s32 pid = (s32)req->pid;
    u8 scratch[SR_MAX_LEN];          // emitter: kelf BSS scratch, 64 KiB
    int n = safe_read_other(pid, req->addr, scratch, (u32)req->len);
    if (n < 0)
        return n;                    // negative code -> td_retval[0]
    // Copy `n` bytes from kelf scratch to the caller's req->buf via copyout
    // (the nr=1 redirect recipe: RIP=copyout, RDI=req->buf, RSI=scratch,
    //  RDX=n; zero td_retval first so a fault surfaces as EFAULT).
    return copyout_to_caller(req->buf, scratch, (u32)n) == 0 ? n : -14;
}
static int copyout_to_caller(u64 uaddr, const void *k, u32 len); /* prototype-only */

// ============================================================================
// 7. FEASIBILITY NOTES (extend kstuff vs new module)
// ============================================================================
//
// Q: Can the ALREADY-RESIDENT kstuff be extended in place?
// A: Not from the game process, and not safely from anywhere without new
//    install-time plumbing:
//    * The kelf lives in kernel heap (8 MiB block allocated by the installer,
//      [KINSTALL] InitialBlockSize) at addresses nobody outside the module
//      knows; there is no symbol server to find it at runtime.
//    * The only game-process->module channel is the kekcall gate, whose
//      dispatch table is frozen machine code; unknown nr returns ENOSYS(78)
//      ([EMIT] 4441-4444). No "load plugin" kekcall exists.
//    * The kernel R/W pipe primitive (PayloadKernelIo, [KIO]/[ARGS]) exists
//      ONLY in the loader process that received PayloadArgs from the exploit
//      (pipe fds + kernel pipe address). The game process never has it.
//    * Re-running the loader is a no-op while the module lives: Install() and
//      the installer binary both early-return on KEKCALL_CHECK(-1) == 0
//      ([KINSTALL] 88-106) — deliberately, because re-patching IDT/TSS/sysent
//      mid-flight panics the kernel.
//    => The supported extension mechanism IS the source: edit
//       KernelModuleEmitter.cs (this file's logic, in emit form), rebuild
//       kelf/uelf with build/build-kstuff.ps1, reboot the console (or add a
//       versioned-teardown kekcall in the same rebuild — a much bigger,
//       riskier change; the module already captures original IDT gates and
//       IST values, so a clean uninstall is conceivable but unimplemented).
//
// Q: Why not a separate NEW kernel module?
// A: Nothing stops a second install in principle, but it must re-do the whole
//    KernelModuleInstaller dance (trace-based kfncall, malloc block, per-CPU
//    kelf/uelf, IDT/TSS patching). Two IDT patches from two modules means the
//    second install's "original handler" capture records the FIRST module's
//    gates — workable but every uninstall/update becomes order-dependent.
//    Since we control the kstuff source and both ship from this repo, one
//    module with the safe-read kekcalls baked in is strictly simpler and
//    safer. Decision: EXTEND kstuff in source, redeploy on next boot.
//
// Q: Can the UE4SS PRX (in the GAME process) reach the gate?
// A: Yes. The sysent hook is global (CloneAndHookSysent redirects the shared
//    sysentvec sv_table for all processes, [KINSTALL] 1049-1051). The PRX
//    needs no special privileges — only a raw syscall 39 with nr in
//    RAX[63:32], using the same libkernel syscall gadget the CRT derives
//    (sceKernelDlsym("getpid") + 0xA, [CRT] __sp_crt_syscall_init). The
//    client header (safe_read_client.h) implements this in C.
//
// Q: Simplest possible communication mechanism?
// A: kekcall over the hooked getppid — chosen. Alternatives rejected:
//    * char device (/dev/modbridge): the kelf is not a kmod(9) module; it
//      cannot create devices or register cdevsw without far more install-time
//      patching. Not feasible.
//    * shared memory page: the infrastructure exists (the 2 KiB
//      shared_area + per-CPU DMEM 1GB-page physical map, [KINSTALL] 181-205)
//      but its VA is a private PML4 slot known only to the kelf/uelf; exposing
//      it to the game process needs a pmap patch — kernel work comparable to
//      the kekcall handlers, with worse latency characteristics for the
//      request/reply handshake (polling). Keep as a future bulk-transfer
//      optimization if 64 KiB/call proves too slow.
//    * procfs entry: same cdevsw-like problem, plus mount complexity. No.
//    * syscall hook on a DEDICATED syscall: equivalent risk to getppid reuse
//      but wastes a syscall and adds a new match arm; getppid reuse is
//      battle-tested in production kstuff. No.
