// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Elf;
using SharpProspero.Payload.Process;
using System;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// Process hijacker. Attaches to a running process, walks its loaded module list from
/// kernel memory, resolves symbols by NID, and provides read/write access for hooking
/// PLT/GOT entries.
/// </summary>
public static unsafe class PayloadHijacker
{
    /// <summary>
    /// Finds the base address of a loaded module in the target process by walking the
    /// kernel's <c>SharedObject</c> linked list from the process's <c>p_dynlib</c> field.
    /// </summary>
    /// <param name="io">Kernel I/O for reading process structures.</param>
    /// <param name="proc">Kernel address of the target process.</param>
    /// <param name="soname">NUL-terminated module soname to find.</param>
    /// <param name="dynlibOffset">Offset of <c>p_dynlib</c> in <c>struct proc</c>.</param>
    /// <returns>The base virtual address of the module in the target process, or zero.</returns>
    public static ulong FindModuleBase(PayloadKernelIo io, ulong proc,
        ReadOnlySpan<byte> soname, int dynlibOffset)
    {
        ulong dynlib = io.ReadU64(proc + (ulong)dynlibOffset);
        if (dynlib == 0) return 0;

        ulong obj = io.ReadU64(dynlib); // first SharedObject
        byte* nameBuf = stackalloc byte[256];

        while (obj != 0)
        {
            ulong namePtr = io.ReadU64(obj + 8); // so_name pointer
            if (namePtr != 0)
            {
                io.Read(namePtr, nameBuf, 256);
                if (MatchName(nameBuf, soname))
                    return io.ReadU64(obj + 0x10); // so_base
            }
            obj = io.ReadU64(obj); // next in list
        }
        return 0;
    }

    /// <summary>
    /// Resolves a function address in a remote process's loaded module by NID comparison.
    /// Reads the module's symbol table from kernel memory and finds the symbol whose
    /// NID matches.
    /// </summary>
    /// <returns>The function's virtual address in the target process, or zero.</returns>
    public static ulong ResolveByNid(PayloadKernelIo io, ulong moduleBase,
        ulong nidRaw, int pid)
    {
        // Read the ELF header to find the dynamic section.
        Elf64Ehdr ehdr;
        if (PayloadProcessMemory.Read(pid, (nint)moduleBase, &ehdr, (nuint)sizeof(Elf64Ehdr)) != 0)
            return 0;

        // Find PT_DYNAMIC.
        for (int i = 0; i < ehdr.Phnum; i++)
        {
            Elf64Phdr phdr;
            nint phdrAddr = (nint)(moduleBase + ehdr.Phoff + (ulong)(i * ehdr.Phentsize));
            if (PayloadProcessMemory.Read(pid, phdrAddr, &phdr, (nuint)sizeof(Elf64Phdr)) != 0)
                continue;

            if (phdr.Type == ElfConstants.PtDynamic)
            {
                return SearchDynamicForNid(io, pid, moduleBase, phdr.Vaddr + moduleBase,
                    (int)(phdr.Filesz / (ulong)sizeof(Elf64Dyn)), nidRaw);
            }
        }
        return 0;
    }

    private static ulong SearchDynamicForNid(PayloadKernelIo io, int pid,
        ulong moduleBase, ulong dynAddr, int dynCount, ulong targetNid)
    {
        ulong symtab = 0, strtab = 0;
        uint strsz = 0;
        uint symCount = 0;

        for (int i = 0; i < dynCount; i++)
        {
            Elf64Dyn dyn;
            if (PayloadProcessMemory.Read(pid, (nint)(dynAddr + (ulong)(i * sizeof(Elf64Dyn))),
                &dyn, (nuint)sizeof(Elf64Dyn)) != 0) continue;

            if (dyn.Tag == ElfConstants.DtSymtab) symtab = dyn.Val;
            else if (dyn.Tag == ElfConstants.DtStrtab) strtab = dyn.Val;
            else if (dyn.Tag == 10) strsz = (uint)dyn.Val; // DT_STRSZ
            else if (dyn.Tag == ElfConstants.DtHash)
            {
                // DT_HASH table: [nbucket, nchain, ...]. nchain == symbol count.
                uint nchain;
                if (PayloadProcessMemory.Read(pid, (nint)(dyn.Val + 4), &nchain, (nuint)4) == 0)
                    symCount = nchain;
            }
            else if (dyn.Tag == 0) break;
        }

        if (symtab == 0 || strtab == 0 || symCount == 0) return 0;

        // Encode the target NID to its 11-byte base64 form for direct comparison.
        byte* targetNidStr = stackalloc byte[12];
        PayloadNid.EncodeRawToBytes(targetNid, new Span<byte>(targetNidStr, 11));

        // Walk the symbol table comparing NIDs.
        byte* nidStr = stackalloc byte[12];
        for (uint j = 0; j < symCount; j++)
        {
            Elf64Sym sym;
            nint symAddr = (nint)(symtab + j * (ulong)sizeof(Elf64Sym));
            if (PayloadProcessMemory.Read(pid, symAddr, &sym, (nuint)sizeof(Elf64Sym)) != 0)
                break;

            if (sym.Value != 0 && sym.Name < strsz)
            {
                // Read the NID string (11 chars) from the string table and compare directly.
                if (PayloadProcessMemory.Read(pid, (nint)(strtab + sym.Name), nidStr, 12) == 0)
                {
                    bool match = true;
                    for (int k = 0; k < 11; k++)
                    {
                        if (nidStr[k] != targetNidStr[k]) { match = false; break; }
                    }
                    if (match)
                        return moduleBase + sym.Value;
                }
            }
        }
        return 0;
    }

    /// <summary>
    /// Patches a GOT entry in the target process to redirect a function call.
    /// </summary>
    public static bool PatchGot(int pid, nint gotEntry, ulong newTarget)
    {
        return PayloadProcessMemory.WriteU64(pid, gotEntry, newTarget) == 0;
    }

    private static bool MatchName(byte* a, ReadOnlySpan<byte> b)
    {
        for (int i = 0; i < b.Length; i++)
        {
            if (b[i] == 0) return a[i] == 0;
            if (a[i] != b[i]) return false;
        }
        return true;
    }
}
