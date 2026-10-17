// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

// Small inspection utilities over a module image: the loadable size by kind, the printable strings, the
// dynamic symbol table, and a size-reducing rewrite that drops the section headers a dynamic module does
// not need. Each reads the ELF directly, so no external tool is involved.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;

namespace SharpProspero.Prx;

/// <summary>The loadable footprint of a module, split by segment kind, in bytes.</summary>
public readonly record struct ElfSegmentSizes(ulong Code, ulong ReadOnly, ulong Data, ulong Bss)
{
    /// <summary>The bytes occupied in the file (everything but the zero-filled tail).</summary>
    public ulong File => Code + ReadOnly + Data;

    /// <summary>The bytes occupied in memory once loaded (the file image plus the zero-filled tail).</summary>
    public ulong Memory => Code + ReadOnly + Data + Bss;
}

/// <summary>One entry of a module's dynamic symbol table.</summary>
public readonly record struct ElfSymbolEntry(string Name, ulong Value, ulong Size, int Type, int Bind, ushort SectionIndex)
{
    /// <summary>Whether the symbol is imported (the module does not define it).</summary>
    public bool IsImport => SectionIndex == 0;

    /// <summary>A readable name for the symbol type.</summary>
    public string TypeName => Type switch
    {
        1 => "object",
        2 => "func",
        3 => "section",
        4 => "file",
        6 => "tls",
        10 => "ifunc",
        _ => "notype",
    };

    /// <summary>A readable name for the symbol binding.</summary>
    public string BindName => Bind switch { 0 => "local", 1 => "global", 2 => "weak", _ => Bind.ToString(System.Globalization.CultureInfo.InvariantCulture) };
}

/// <summary>Reads a module image for its size, strings, symbols, and a stripped rewrite.</summary>
public static class ElfTools
{
    private const uint PtLoad = 1, PtDynamic = 2;
    private const long DtNull = 0, DtStrTab = 5, DtSymTab = 6, DtStrSz = 10;
    private const long DtSceStrTab = 0x61000035, DtSceStrSz = 0x61000037, DtSceSymTab = 0x61000039, DtSceSymTabSz = 0x6100003F;

    /// <summary>The loadable size of the module, split into code, read-only, writable and zero-filled.</summary>
    public static ElfSegmentSizes SegmentSizes(byte[] elf)
    {
        ElfInfo info = ElfInfo.Parse(elf);
        ulong code = 0, ro = 0, data = 0, bss = 0;
        foreach (ElfProgramHeader ph in info.ProgramHeaders)
        {
            if (ph.Type != PtLoad)
                continue;
            bool exec = (ph.Flags & 1) != 0;
            bool write = (ph.Flags & 2) != 0;
            if (exec)
                code += ph.FileSize;
            else if (write)
            {
                data += ph.FileSize;
                bss += ph.MemorySize > ph.FileSize ? ph.MemorySize - ph.FileSize : 0;
            }
            else
                ro += ph.FileSize;
        }
        return new ElfSegmentSizes(code, ro, data, bss);
    }

    /// <summary>
    /// The printable ASCII runs of at least <paramref name="minLength"/> characters in the file, each with
    /// the offset it starts at. A run ends at any byte outside the printable range or a terminating zero.
    /// </summary>
    public static IReadOnlyList<(long Offset, string Text)> Strings(byte[] data, int minLength = 4)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (minLength < 1)
            minLength = 1;
        var result = new List<(long, string)>();
        int start = -1;
        for (int i = 0; i <= data.Length; i++)
        {
            byte b = i < data.Length ? data[i] : (byte)0;
            bool printable = i < data.Length && b >= 0x20 && b < 0x7F;
            if (printable)
            {
                if (start < 0)
                    start = i;
            }
            else if (start >= 0)
            {
                int length = i - start;
                if (length >= minLength)
                    result.Add((start, System.Text.Encoding.ASCII.GetString(data, start, length)));
                start = -1;
            }
        }
        return result;
    }

    /// <summary>The module's dynamic symbol table, or an empty list when it has no dynamic segment.</summary>
    public static IReadOnlyList<ElfSymbolEntry> DynamicSymbols(byte[] elf)
    {
        ArgumentNullException.ThrowIfNull(elf);
        if (!TryReadDynamic(elf, out long dynOffset, out long dynSize, out List<(ulong Va, ulong Off, ulong Size)> loads, out long dynlibOffset))
            return [];

        long symVaOrOff = -1, strVaOrOff = -1, sceSym = -1, sceStr = -1, symSz = 0, strSz = 0;
        for (long d = dynOffset; d + 16 <= dynOffset + dynSize && d + 16 <= elf.Length; d += 16)
        {
            long tag = BinaryPrimitives.ReadInt64LittleEndian(elf.AsSpan((int)d));
            ulong val = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan((int)d + 8));
            if (tag == DtNull)
                break;
            switch (tag)
            {
                case DtSymTab: symVaOrOff = (long)val; break;
                case DtStrTab: strVaOrOff = (long)val; break;
                case DtStrSz: if (strSz == 0) strSz = (long)val; break;
                case DtSceSymTab: sceSym = (long)val; break;
                case DtSceStrTab: sceStr = (long)val; break;
                case DtSceSymTabSz: symSz = (long)val; break;
                case DtSceStrSz: strSz = (long)val; break;
            }
        }

        long symBase = ResolveTable(sceSym, symVaOrOff, dynlibOffset, loads);
        long strBase = ResolveTable(sceStr, strVaOrOff, dynlibOffset, loads);
        if (symBase < 0 || strBase < 0)
            return [];
        if (symSz <= 0)
            symSz = strBase > symBase ? strBase - symBase : elf.Length - symBase;

        var symbols = new List<ElfSymbolEntry>();
        for (long s = symBase; s + 24 <= symBase + symSz && s + 24 <= elf.Length; s += 24)
        {
            uint stName = BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan((int)s));
            byte stInfo = elf[(int)s + 4];
            ushort stShndx = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan((int)s + 6));
            ulong stValue = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan((int)s + 8));
            ulong stSize = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan((int)s + 16));
            string name = ReadCString(elf, strBase + stName, strSz);
            symbols.Add(new ElfSymbolEntry(name, stValue, stSize, stInfo & 0xF, stInfo >> 4, stShndx));
        }
        return symbols;
    }

    /// <summary>
    /// A rewrite of the module with the section-header table and the non-loadable data past the last
    /// segment removed, keeping everything the dynamic loader reads. The module must carry a dynamic
    /// segment; a plain object or a payload, whose section headers are load-bearing, is left to the caller.
    /// </summary>
    public static byte[] Strip(byte[] elf)
    {
        ArgumentNullException.ThrowIfNull(elf);
        if (elf.Length < 0x40 || BinaryPrimitives.ReadUInt32LittleEndian(elf) != 0x464C457F)
            throw new PrxFormatException("File is not an ELF.");

        ulong phoff = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(0x20));
        ushort phentsize = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(0x36));
        ushort phnum = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(0x38));

        bool hasDynamic = false;
        long extent = 0x40;
        long phTableEnd = (long)phoff + (long)phnum * phentsize;
        if (phTableEnd > extent)
            extent = phTableEnd;
        for (int i = 0; i < phnum; i++)
        {
            long ph = (long)phoff + (long)i * phentsize;
            if (ph < 0 || ph + 0x38 > elf.Length)
                break;
            int b = (int)ph;
            uint type = BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan(b));
            ulong offset = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(b + 8));
            ulong filesz = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(b + 32));
            if (type == PtDynamic)
                hasDynamic = true;
            long end = (long)offset + (long)filesz;
            if (end > extent)
                extent = end;
        }
        if (!hasDynamic)
            throw new PrxFormatException("Only a dynamic module can be stripped this way; the file has no dynamic segment.");
        if (extent > elf.Length)
            extent = elf.Length;

        byte[] stripped = elf[..(int)extent];
        BinaryPrimitives.WriteUInt64LittleEndian(stripped.AsSpan(0x28), 0);  // e_shoff
        BinaryPrimitives.WriteUInt16LittleEndian(stripped.AsSpan(0x3A), 0);  // e_shentsize
        BinaryPrimitives.WriteUInt16LittleEndian(stripped.AsSpan(0x3C), 0);  // e_shnum
        BinaryPrimitives.WriteUInt16LittleEndian(stripped.AsSpan(0x3E), 0);  // e_shstrndx
        return stripped;
    }

    private static bool TryReadDynamic(byte[] elf, out long dynOffset, out long dynSize,
        out List<(ulong Va, ulong Off, ulong Size)> loads, out long dynlibOffset)
    {
        dynOffset = -1; dynSize = 0; loads = []; dynlibOffset = -1;
        if (elf.Length < 0x40 || BinaryPrimitives.ReadUInt32LittleEndian(elf) != 0x464C457F || elf[4] != 2)
            return false;
        ulong phoff = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(0x20));
        ushort phentsize = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(0x36));
        ushort phnum = BinaryPrimitives.ReadUInt16LittleEndian(elf.AsSpan(0x38));
        for (int i = 0; i < phnum; i++)
        {
            long ph = (long)phoff + (long)i * phentsize;
            if (ph < 0 || ph + 0x38 > elf.Length)
                break;
            int b = (int)ph;
            uint type = BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan(b));
            ulong offset = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(b + 8));
            ulong vaddr = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(b + 16));
            ulong filesz = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan(b + 32));
            if (type == PtLoad)
                loads.Add((vaddr, offset, filesz));
            else if (type == PtDynamic)
            {
                dynOffset = (long)offset;
                dynSize = (long)filesz;
            }
            else if (type == 0x61000000) // SCE_DYNLIBDATA
                dynlibOffset = (long)offset;
        }
        return dynOffset >= 0;
    }

    // A table address is either a virtual address mapped through a load segment, or an offset into the
    // dynamic-library data blob; resolve whichever the module uses to a file offset.
    private static long ResolveTable(long sceOffset, long stdVa, long dynlibOffset, List<(ulong Va, ulong Off, ulong Size)> loads)
    {
        if (sceOffset >= 0 && dynlibOffset >= 0)
            return dynlibOffset + sceOffset;
        if (stdVa >= 0)
        {
            foreach ((ulong va, ulong off, ulong size) in loads)
                if ((ulong)stdVa >= va && (ulong)stdVa < va + size)
                    return (long)off + (stdVa - (long)va);
        }
        return -1;
    }

    private static string ReadCString(byte[] data, long offset, long limit)
    {
        if (offset < 0 || offset >= data.Length)
            return "";
        long end = offset;
        long stop = limit > 0 ? Math.Min(data.Length, offset + limit) : data.Length;
        while (end < stop && data[(int)end] != 0)
            end++;
        return System.Text.Encoding.ASCII.GetString(data, (int)offset, (int)(end - offset));
    }
}
