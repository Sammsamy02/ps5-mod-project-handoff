// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Process;

namespace SharpProspero.Payload.Elf;

/// <summary>
/// Reads and validates ELF 64-bit headers from a byte buffer or from another process's
/// memory. Works in-place without allocations, returning pointers into the provided buffer.
/// </summary>
public static unsafe class PayloadElfReader
{
    /// <summary>
    /// Validates the ELF magic at the start of <paramref name="data"/>.
    /// </summary>
    public static bool IsValidElf(byte* data, int length)
    {
        if (length < sizeof(Elf64Ehdr)) return false;
        return data[0] == 0x7F && data[1] == (byte)'E' && data[2] == (byte)'L' && data[3] == (byte)'F';
    }

    /// <summary>
    /// Returns a pointer to the ELF header at the start of <paramref name="data"/>.
    /// </summary>
    public static Elf64Ehdr* GetHeader(byte* data) => (Elf64Ehdr*)data;

    /// <summary>
    /// Returns a pointer to the program header at index <paramref name="index"/>.
    /// </summary>
    public static Elf64Phdr* GetProgramHeader(byte* data, Elf64Ehdr* ehdr, int index)
    {
        if ((uint)index >= ehdr->Phnum) return null;
        return (Elf64Phdr*)(data + ehdr->Phoff + (ulong)(index * ehdr->Phentsize));
    }

    /// <summary>
    /// Finds the first program header with the given <paramref name="type"/>.
    /// </summary>
    public static Elf64Phdr* FindProgramHeader(byte* data, Elf64Ehdr* ehdr, uint type)
    {
        for (int i = 0; i < ehdr->Phnum; i++)
        {
            Elf64Phdr* phdr = GetProgramHeader(data, ehdr, i);
            if (phdr != null && phdr->Type == type)
                return phdr;
        }
        return null;
    }

    /// <summary>
    /// Returns the base virtual address (the lowest PT_LOAD vaddr).
    /// </summary>
    public static ulong GetBaseAddress(byte* data, Elf64Ehdr* ehdr)
    {
        ulong min = ulong.MaxValue;
        for (int i = 0; i < ehdr->Phnum; i++)
        {
            Elf64Phdr* phdr = GetProgramHeader(data, ehdr, i);
            if (phdr != null && phdr->Type == ElfConstants.PtLoad && phdr->Vaddr < min)
                min = phdr->Vaddr;
        }
        return min == ulong.MaxValue ? 0 : min;
    }

    /// <summary>
    /// Returns the total memory size needed for all PT_LOAD segments.
    /// </summary>
    public static ulong GetTotalMemorySize(byte* data, Elf64Ehdr* ehdr)
    {
        ulong lo = ulong.MaxValue, hi = 0;
        for (int i = 0; i < ehdr->Phnum; i++)
        {
            Elf64Phdr* phdr = GetProgramHeader(data, ehdr, i);
            if (phdr == null || phdr->Type != ElfConstants.PtLoad) continue;
            if (phdr->Vaddr < lo) lo = phdr->Vaddr;
            ulong end = phdr->Vaddr + phdr->Memsz;
            if (end > hi) hi = end;
        }
        return lo <= hi ? hi - lo : 0;
    }

    /// <summary>
    /// Iterates the dynamic section entries. Returns the count and a pointer to the first
    /// <see cref="Elf64Dyn"/> entry.
    /// </summary>
    public static Elf64Dyn* GetDynamicTable(byte* data, Elf64Ehdr* ehdr, out int count)
    {
        Elf64Phdr* dynPhdr = FindProgramHeader(data, ehdr, ElfConstants.PtDynamic);
        if (dynPhdr == null) { count = 0; return null; }

        Elf64Dyn* dyn = (Elf64Dyn*)(data + dynPhdr->Offset);
        count = (int)(dynPhdr->Filesz / (ulong)sizeof(Elf64Dyn));
        return dyn;
    }

    /// <summary>
    /// Finds a dynamic table entry by tag.
    /// </summary>
    public static Elf64Dyn* FindDynamic(Elf64Dyn* table, int count, long tag)
    {
        for (int i = 0; i < count; i++)
        {
            if (table[i].Tag == tag) return &table[i];
            if (table[i].Tag == 0) break;
        }
        return null;
    }

    /// <summary>
    /// Reads an ELF header from another process's memory at <paramref name="addr"/>.
    /// </summary>
    public static bool ReadRemoteHeader(int pid, nint addr, Elf64Ehdr* outHdr)
    {
        if (PayloadProcessMemory.Read(pid, addr, outHdr, (nuint)sizeof(Elf64Ehdr)) != 0)
            return false;
        return outHdr->Ident[0] == 0x7F && outHdr->Ident[1] == (byte)'E'
            && outHdr->Ident[2] == (byte)'L' && outHdr->Ident[3] == (byte)'F';
    }

    /// <summary>
    /// Reads a program header from another process's memory.
    /// </summary>
    public static bool ReadRemoteProgramHeader(int pid, nint baseAddr, Elf64Ehdr* ehdr,
        int index, Elf64Phdr* outPhdr)
    {
        if ((uint)index >= ehdr->Phnum) return false;
        nint addr = baseAddr + (nint)ehdr->Phoff + (nint)(index * ehdr->Phentsize);
        return PayloadProcessMemory.Read(pid, addr, outPhdr, (nuint)sizeof(Elf64Phdr)) == 0;
    }

    /// <summary>
    /// Applies R_X86_64_RELATIVE relocations to a loaded ELF image in memory.
    /// </summary>
    public static void ApplyRelativeRelocations(byte* loadBase, Elf64Rela* rela, int count,
        ulong elfBase)
    {
        ulong baseOffset = (ulong)loadBase - elfBase;
        for (int i = 0; i < count; i++)
        {
            if (rela[i].Type == ElfConstants.RX8664Relative)
            {
                ulong* target = (ulong*)(loadBase + rela[i].Offset - elfBase);
                *target = (ulong)rela[i].Addend + baseOffset;
            }
        }
    }
}
