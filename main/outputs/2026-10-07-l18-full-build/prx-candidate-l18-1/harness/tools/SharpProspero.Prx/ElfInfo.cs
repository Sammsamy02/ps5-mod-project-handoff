// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;

namespace SharpProspero.Prx;

/// <summary>One program header of an ELF module: a segment's type, flags and placement.</summary>
public readonly record struct ElfProgramHeader(
    uint Type, uint Flags, ulong FileOffset, ulong VirtualAddress, ulong FileSize, ulong MemorySize, ulong Align)
{
    /// <summary>A readable name for the segment type.</summary>
    public string TypeName => Type switch
    {
        0 => "NULL",
        1 => "LOAD",
        2 => "DYNAMIC",
        3 => "INTERP",
        4 => "NOTE",
        6 => "PHDR",
        7 => "TLS",
        0x6474E550 => "GNU_EH_FRAME",
        0x6474E551 => "GNU_STACK",
        0x6474E552 => "GNU_RELRO",
        0x61000000 => "SCE_DYNLIBDATA",
        0x61000001 => "SCE_PROCPARAM",
        0x61000002 => "SCE_MODULEPARAM",
        0x6FFFFF00 => "SCE_COMMENT",
        0x6FFFFF01 => "SCE_VERSION",
        _ => $"0x{Type:X8}",
    };

    /// <summary>The read/write/execute flags as a three-character string.</summary>
    public string FlagsText =>
        $"{((Flags & 4) != 0 ? 'R' : '-')}{((Flags & 2) != 0 ? 'W' : '-')}{((Flags & 1) != 0 ? 'X' : '-')}";
}

/// <summary>The header and program-header table of an ELF module, read without an external tool.</summary>
public readonly record struct ElfInfo(
    bool Is64Bit, byte OsAbi, ushort Type, ushort Machine, ulong Entry, ushort ProgramHeaderCount)
{
    /// <summary>The program headers, in file order.</summary>
    public IReadOnlyList<ElfProgramHeader> ProgramHeaders { get; init; } = [];

    /// <summary>
    /// Reads the header and program headers of the module at <paramref name="path"/>. A signed
    /// container is unwrapped to its embedded ELF first.
    /// </summary>
    public static ElfInfo Read(string path) => Parse(ModuleFile.Read(path).Elf);

    /// <summary>Reads the header and program headers from bytes already in memory.</summary>
    public static ElfInfo Parse(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (data.Length < 0x40)
            throw new PrxFormatException("File is too short to be an ELF.");
        if (BinaryPrimitives.ReadUInt32LittleEndian(data) != 0x464C457F)
            throw new PrxFormatException("File is not an ELF.");

        ulong phoff = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(0x20));
        ushort phentsize = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x36));
        ushort phnum = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x38));

        var headers = new List<ElfProgramHeader>(phnum);
        for (int i = 0; i < phnum; i++)
        {
            long ph = (long)phoff + (long)i * phentsize;
            if (ph < 0 || ph + 0x38 > data.Length)
                break;
            int b = (int)ph;
            headers.Add(new ElfProgramHeader(
                Type: BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b)),
                Flags: BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b + 4)),
                FileOffset: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 8)),
                VirtualAddress: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 16)),
                FileSize: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 32)),
                MemorySize: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 40)),
                Align: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 48))));
        }

        return new ElfInfo(
            Is64Bit: data[4] == 2,
            OsAbi: data[7],
            Type: BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x10)),
            Machine: BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x12)),
            Entry: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(0x18)),
            ProgramHeaderCount: phnum)
        {
            ProgramHeaders = headers,
        };
    }

    /// <summary>A readable name for the object-file type.</summary>
    public string TypeName => Type switch
    {
        0x01 => "Relocatable object",
        0x02 => "Executable",
        0x03 => "Shared object",
        0xFE00 => "Module executable",
        0xFE04 => "Module relocatable executable",
        0xFE0C => "Module stub library",
        0xFE10 => "Dynamic executable",
        0xFE18 => "Dynamic module",
        _ => $"0x{Type:X4}",
    };
}
