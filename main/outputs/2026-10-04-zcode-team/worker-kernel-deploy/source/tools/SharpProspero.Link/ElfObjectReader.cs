// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Link;

/// <summary>Raised when a file is not a relocatable object this reader understands.</summary>
public sealed class ElfLinkException(string message) : Exception(message)
{
}

/// <summary>
/// Reads a relocatable object (an ELF64 x86-64 <c>ET_REL</c>) into an <see cref="ElfObject"/>: its
/// sections, its symbols, and its relocations grouped by the section they apply to.
/// </summary>
public static class ElfObjectReader
{
    private const uint ElfMagic = 0x464C457FU;
    private const int MachineX8664 = 0x3E;
    private const int TypeRel = 1;

    /// <summary>Reads the object bytes from <paramref name="origin"/>.</summary>
    public static ElfObject Read(byte[] data, string origin)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (data.Length < 0x40 || BinaryPrimitives.ReadUInt32LittleEndian(data) != ElfMagic)
            throw new ElfLinkException($"{origin}: not an ELF.");
        if (data[4] != 2)
            throw new ElfLinkException($"{origin}: not 64-bit.");
        if (BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x12)) != MachineX8664)
            throw new ElfLinkException($"{origin}: not x86-64.");
        ushort eType = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x10));
        if (eType != TypeRel)
            throw new ElfLinkException($"{origin}: expected a relocatable object (ET_REL).");

        ulong shoff = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(0x28));
        ushort shentsize = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x3A));
        ushort shnum = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x3C));
        ushort shstrndx = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x3E));

        // Validate the section-header table so a malformed object fails as an ElfLinkException (which
        // the archive-member scan skips) rather than an index or overflow exception.
        if (shentsize < 0x40)
            throw new ElfLinkException($"{origin}: section-header entry size too small.");
        if (shnum == 0)
            throw new ElfLinkException($"{origin}: no section headers.");
        if (shstrndx >= shnum)
            throw new ElfLinkException($"{origin}: section-header string-table index out of range.");
        if (shoff > (ulong)data.Length || (ulong)shnum * shentsize > (ulong)data.Length - shoff)
            throw new ElfLinkException($"{origin}: section-header table extends past the end of the file.");

        var raw = new RawSection[shnum];
        for (int i = 0; i < shnum; i++)
        {
            int b = (int)(shoff + (ulong)i * shentsize);
            raw[i] = new RawSection(
                NameOffset: BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b)),
                Type: BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b + 4)),
                Flags: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 8)),
                Address: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 16)),
                Offset: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 24)),
                Size: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 32)),
                Link: BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b + 40)),
                Info: BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b + 44)),
                AddrAlign: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 48)),
                EntSize: BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 56)));
        }

        byte[] shstr = SectionBytes(data, raw[shstrndx]);
        var sections = new ElfSection[shnum];
        for (int i = 0; i < shnum; i++)
        {
            RawSection r = raw[i];
            sections[i] = new ElfSection
            {
                Name = ReadCString(shstr, r.NameOffset),
                Type = r.Type,
                Flags = r.Flags,
                Address = r.Address,
                Size = r.Size,
                Link = r.Link,
                Info = r.Info,
                AddrAlign = r.AddrAlign == 0 ? 1 : r.AddrAlign,
                EntSize = r.EntSize,
                Data = r.Type == ShType.NoBits ? [] : SectionBytes(data, r),
            };
        }

        var symbols = ReadSymbols(data, raw, sections);
        var relocations = ReadRelocations(data, raw);
        var groups = ReadGroups(raw, sections, symbols);

        return new ElfObject
        {
            Origin = origin,
            Sections = sections,
            Symbols = symbols,
            Relocations = relocations,
            Groups = groups,
        };
    }

    private static List<ElfSymbol> ReadSymbols(byte[] data, RawSection[] raw, ElfSection[] sections)
    {
        int symIndex = Array.FindIndex(sections, s => s.Type == ShType.SymTab);
        if (symIndex < 0)
            return [];

        RawSection sym = raw[symIndex];
        if (sym.Link >= raw.Length)
            throw new ElfLinkException("Symbol table names an out-of-range string table.");
        byte[] str = SectionBytes(data, raw[sym.Link]);
        byte[] table = SectionBytes(data, sym);
        int count = table.Length / 24;
        var list = new List<ElfSymbol>(count);
        for (int i = 0; i < count; i++)
        {
            int b = i * 24;
            uint nameOff = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(b));
            list.Add(new ElfSymbol
            {
                Name = ReadCString(str, nameOff),
                Info = table[b + 4],
                Other = table[b + 5],
                SectionIndex = BinaryPrimitives.ReadUInt16LittleEndian(table.AsSpan(b + 6)),
                Value = BinaryPrimitives.ReadUInt64LittleEndian(table.AsSpan(b + 8)),
                Size = BinaryPrimitives.ReadUInt64LittleEndian(table.AsSpan(b + 16)),
            });
        }
        return list;
    }

    private static Dictionary<int, IReadOnlyList<ElfRelocation>> ReadRelocations(byte[] data, RawSection[] raw)
    {
        var result = new Dictionary<int, IReadOnlyList<ElfRelocation>>();
        for (int i = 0; i < raw.Length; i++)
        {
            if (raw[i].Type != ShType.Rela)
                continue;
            byte[] table = SectionBytes(data, raw[i]);
            int count = table.Length / 24;
            var list = new List<ElfRelocation>(count);
            for (int e = 0; e < count; e++)
            {
                int b = e * 24;
                ulong offset = BinaryPrimitives.ReadUInt64LittleEndian(table.AsSpan(b));
                ulong info = BinaryPrimitives.ReadUInt64LittleEndian(table.AsSpan(b + 8));
                long addend = BinaryPrimitives.ReadInt64LittleEndian(table.AsSpan(b + 16));
                list.Add(new ElfRelocation(offset, (uint)(info >> 32), (uint)(info & 0xFFFFFFFF), addend));
            }
            result[(int)raw[i].Info] = list; // sh_info is the target section index
        }
        return result;
    }

    /// <summary>
    /// The groups of sections this object shares with others. A compiler emits an inline function, a
    /// template body or a virtual table into every object that needs it, and names each copy under one
    /// signature so a link keeps exactly one. The first word of the section is flags; the words after it
    /// are the member section indices, and the signature is the symbol the section's info field names.
    /// </summary>
    private static IReadOnlyList<ElfSectionGroup> ReadGroups(
        RawSection[] raw, ElfSection[] sections, IReadOnlyList<ElfSymbol> symbols)
    {
        var groups = new List<ElfSectionGroup>();
        for (int i = 0; i < sections.Length; i++)
        {
            if (sections[i].Type != ShType.Group) continue;
            byte[] body = sections[i].Data;
            if (body.Length < 4) continue;
            uint flags = BinaryPrimitives.ReadUInt32LittleEndian(body);
            uint signatureIndex = raw[i].Info;
            if (signatureIndex >= (uint)symbols.Count) continue;
            string signature = symbols[(int)signatureIndex].Name;
            if (signature.Length == 0) continue;
            var members = new List<int>();
            for (int at = 4; at + 4 <= body.Length; at += 4)
            {
                int m = (int)BinaryPrimitives.ReadUInt32LittleEndian(body.AsSpan(at));
                if (m > 0 && m < sections.Length) members.Add(m);
            }
            if (members.Count > 0)
                groups.Add(new ElfSectionGroup(signature, members, (flags & GroupKeepOnlyOne) != 0));
        }
        return groups;
    }

    /// <summary>The flag saying a group asks for duplicates of itself to be dropped.</summary>
    private const uint GroupKeepOnlyOne = 1;

    private static byte[] SectionBytes(byte[] data, RawSection s)
    {
        if (s.Type == ShType.NoBits || s.Size == 0)
            return [];
        // Validate against the file length in unsigned space so a near-maximum offset or size cannot
        // wrap the bounds check; a section that overruns the file is a malformed object, not a crash.
        if (s.Offset > (ulong)data.Length || s.Size > (ulong)data.Length - s.Offset)
            throw new ElfLinkException("Section extends past the end of the file.");
        return data.AsSpan((int)s.Offset, (int)s.Size).ToArray();
    }

    private static string ReadCString(byte[] table, uint offset)
    {
        if (offset >= table.Length)
            return "";
        int end = (int)offset;
        while (end < table.Length && table[end] != 0)
            end++;
        return Encoding.ASCII.GetString(table, (int)offset, end - (int)offset);
    }

    private readonly record struct RawSection(
        uint NameOffset, uint Type, ulong Flags, ulong Address, ulong Offset,
        ulong Size, uint Link, uint Info, ulong AddrAlign, ulong EntSize);
}
