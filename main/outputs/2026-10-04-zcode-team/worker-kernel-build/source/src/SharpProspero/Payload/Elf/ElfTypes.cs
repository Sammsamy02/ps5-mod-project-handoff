// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Elf;

/// <summary>
/// ELF 64-bit executable header.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct Elf64Ehdr
{
    /// <summary>ELF identification bytes (16 bytes: magic, class, data, version, OS/ABI, padding).</summary>
    public fixed byte Ident[16];

    /// <summary>Object file type (ET_EXEC=2, ET_DYN=3).</summary>
    public ushort Type;

    /// <summary>Architecture (EM_X86_64=62).</summary>
    public ushort Machine;

    /// <summary>ELF version.</summary>
    public uint Version;

    /// <summary>Entry point virtual address.</summary>
    public ulong Entry;

    /// <summary>Program header table file offset.</summary>
    public ulong Phoff;

    /// <summary>Section header table file offset.</summary>
    public ulong Shoff;

    /// <summary>Processor-specific flags.</summary>
    public uint Flags;

    /// <summary>ELF header size.</summary>
    public ushort Ehsize;

    /// <summary>Program header table entry size.</summary>
    public ushort Phentsize;

    /// <summary>Program header table entry count.</summary>
    public ushort Phnum;

    /// <summary>Section header table entry size.</summary>
    public ushort Shentsize;

    /// <summary>Section header table entry count.</summary>
    public ushort Shnum;

    /// <summary>Section name string table index.</summary>
    public ushort Shstrndx;
}

/// <summary>
/// ELF 64-bit program header.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Elf64Phdr
{
    /// <summary>Segment type (PT_LOAD=1, PT_DYNAMIC=2, PT_INTERP=3, PT_NOTE=4, PT_GNU_EH_FRAME=0x6474e550).</summary>
    public uint Type;

    /// <summary>Segment flags (PF_X=1, PF_W=2, PF_R=4).</summary>
    public uint Flags;

    /// <summary>Segment file offset.</summary>
    public ulong Offset;

    /// <summary>Segment virtual address.</summary>
    public ulong Vaddr;

    /// <summary>Segment physical address.</summary>
    public ulong Paddr;

    /// <summary>Segment size in file.</summary>
    public ulong Filesz;

    /// <summary>Segment size in memory.</summary>
    public ulong Memsz;

    /// <summary>Segment alignment.</summary>
    public ulong Align;
}

/// <summary>
/// ELF 64-bit dynamic section entry.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Elf64Dyn
{
    /// <summary>Dynamic tag (DT_NEEDED=1, DT_STRTAB=5, DT_SYMTAB=6, DT_RELA=7, etc.).</summary>
    public long Tag;

    /// <summary>Tag value (union of d_val and d_ptr).</summary>
    public ulong Val;
}

/// <summary>
/// ELF 64-bit symbol table entry.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Elf64Sym
{
    /// <summary>Symbol name (index into string table).</summary>
    public uint Name;

    /// <summary>Symbol type and binding (use ELF64_ST_TYPE/ELF64_ST_BIND macros).</summary>
    public byte Info;

    /// <summary>Symbol visibility.</summary>
    public byte Other;

    /// <summary>Section index.</summary>
    public ushort Shndx;

    /// <summary>Symbol value (address).</summary>
    public ulong Value;

    /// <summary>Symbol size.</summary>
    public ulong Size;
}

/// <summary>
/// ELF 64-bit relocation entry with addend.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Elf64Rela
{
    /// <summary>Location to apply the relocation.</summary>
    public ulong Offset;

    /// <summary>Relocation type and symbol index.</summary>
    public ulong Info;

    /// <summary>Addend.</summary>
    public long Addend;

    /// <summary>Extracts the symbol index from <see cref="Info"/>.</summary>
    public uint Sym => (uint)(Info >> 32);

    /// <summary>Extracts the relocation type from <see cref="Info"/>.</summary>
    public uint Type => (uint)(Info & 0xFFFFFFFF);
}

/// <summary>
/// ELF constants for segment types, dynamic tags, and relocation types.
/// </summary>
public static class ElfConstants
{
    /// <summary>ELF magic bytes.</summary>
    public const uint ElfMagic = 0x464C457F; // "\x7fELF" in little-endian

    /// <summary>Loadable segment.</summary>
    public const uint PtLoad = 1;

    /// <summary>Dynamic linking information.</summary>
    public const uint PtDynamic = 2;

    /// <summary>Program interpreter path.</summary>
    public const uint PtInterp = 3;

    /// <summary>Auxiliary information.</summary>
    public const uint PtNote = 4;

    /// <summary>GNU exception frame.</summary>
    public const uint PtGnuEhFrame = 0x6474E550;

    /// <summary>SCE dynamic linking information.</summary>
    public const uint PtSceDynlibdata = 0x61000000;

    /// <summary>SCE process parameter.</summary>
    public const uint PtSceProcparam = 0x61000001;

    /// <summary>SCE relocation table.</summary>
    public const uint PtSceRelro = 0x61000010;

    /// <summary>Executable permission.</summary>
    public const uint PfX = 1;

    /// <summary>Write permission.</summary>
    public const uint PfW = 2;

    /// <summary>Read permission.</summary>
    public const uint PfR = 4;

    /// <summary>Needed library name.</summary>
    public const long DtNeeded = 1;

    /// <summary>String table address.</summary>
    public const long DtStrtab = 5;

    /// <summary>Symbol table address.</summary>
    public const long DtSymtab = 6;

    /// <summary>Relocation table address.</summary>
    public const long DtRela = 7;

    /// <summary>Relocation table size.</summary>
    public const long DtRelasz = 8;

    /// <summary>PLT relocation table address.</summary>
    public const long DtJmprel = 23;

    /// <summary>PLT relocation table size.</summary>
    public const long DtPltrelsz = 2;

    /// <summary>Hash table address.</summary>
    public const long DtHash = 4;

    /// <summary>GNU hash table address.</summary>
    public const long DtGnuHash = 0x6FFFFEF5;

    /// <summary>Init function array address.</summary>
    public const long DtInitArray = 25;

    /// <summary>Init function array size.</summary>
    public const long DtInitArraysz = 27;

    /// <summary>R_X86_64_64 — direct 64-bit.</summary>
    public const uint RX8664_64 = 1;

    /// <summary>R_X86_64_GLOB_DAT — create GOT entry.</summary>
    public const uint RX8664GlobDat = 6;

    /// <summary>R_X86_64_JMP_SLOT — create PLT entry.</summary>
    public const uint RX8664JmpSlot = 7;

    /// <summary>R_X86_64_RELATIVE — adjust by base.</summary>
    public const uint RX8664Relative = 8;
}
