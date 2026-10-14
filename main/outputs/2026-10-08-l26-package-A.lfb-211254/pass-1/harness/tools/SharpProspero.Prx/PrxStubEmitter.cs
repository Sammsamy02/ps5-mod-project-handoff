// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SharpProspero.Prx;

/// <summary>
/// Writes a link stub for a module. The stub is a small object, wrapped in an archive, that carries
/// the plain export names and their identifiers, so the linker resolves a call to the name against
/// the module that provides it. Everything in the stub is derived from the library name and the
/// exported function names.
/// </summary>
public static class PrxStubEmitter
{
    private const int ShtStrTab = 3;
    private const int ShtDynSym = 11;
    private const int ShtDynamic = 6;
    private const int ShtProgBits = 1;
    private const uint ShtSceNid = 0x61000001;

    private const long DtSoname = 0x0e;
    private const long DtSceStubModuleName = 0x6100001d;
    private const long DtSceStubModuleVersion = 0x6100001f;
    private const long DtSceStubLibraryName = 0x61000021;
    private const long DtSceStubLibraryVersion = 0x61000023;
    private const long DtSceExportLibAttr = 0x61000017;
    private const long DtNull = 0;

    /// <summary>The module version a stub records unless the caller names another. Major in the high byte.</summary>
    public const ushort DefaultModuleVersion = 0x0101;

    /// <summary>The library version a stub records unless the caller names another.</summary>
    public const ushort DefaultLibraryVersion = 0x0001;

    /// <summary>
    /// Writes the stub for <paramref name="libraryName"/> to <paramref name="outputPath"/>. The stub
    /// is a single object; the linker consumes it directly.
    /// </summary>
    public static void WriteStub(string libraryName, IReadOnlyList<string> functionNames, string outputPath,
        ushort moduleVersion = DefaultModuleVersion, ushort libraryVersion = DefaultLibraryVersion,
        string? moduleName = null, string? soname = null)
    {
        ArgumentException.ThrowIfNullOrEmpty(outputPath);
        File.WriteAllBytes(outputPath, BuildObject(libraryName, functionNames, moduleVersion, libraryVersion, moduleName, soname));
    }

    /// <summary>
    /// Builds the stub object bytes for <paramref name="libraryName"/>. The versions must be the ones
    /// the module actually exports: an import records them, and a mismatch does not bind.
    /// </summary>
    public static byte[] BuildObject(string libraryName, IReadOnlyList<string> functionNames,
        ushort moduleVersion = DefaultModuleVersion, ushort libraryVersion = DefaultLibraryVersion,
        string? moduleName = null, string? soname = null)
    {
        ArgumentException.ThrowIfNullOrEmpty(libraryName);
        ArgumentNullException.ThrowIfNull(functionNames);

        // A module usually names its file, its module, and its library the same, so these default from
        // the library name; a module that publishes them differently names each.
        moduleName ??= libraryName;
        soname ??= libraryName + ".prx";

        // .dynstr
        var dynstr = new StringTable();
        int sonameOff = dynstr.Add(soname);
        int moduleNameOff = dynstr.Add(moduleName);
        int libraryNameOff = dynstr.Add(libraryName);
        var nameOffsets = new int[functionNames.Count];
        for (int i = 0; i < functionNames.Count; i++)
            nameOffsets[i] = dynstr.Add(functionNames[i]);
        byte[] dynstrBytes = dynstr.ToBytes();

        // .dynsym: null entry then one FUNC/GLOBAL/UND per name.
        byte[] dynsym = new byte[24 * (functionNames.Count + 1)];
        for (int i = 0; i < functionNames.Count; i++)
        {
            int off = 24 * (i + 1);
            BinaryPrimitives.WriteUInt32LittleEndian(dynsym.AsSpan(off), (uint)nameOffsets[i]);
            dynsym[off + 4] = 0x12; // STB_GLOBAL | STT_FUNC
            // st_other 0, st_shndx 0 (UND), st_value 0, st_size 0 already zero.
        }

        // .scenid: one 8-byte identifier per .dynsym entry; the null entry stays zero.
        byte[] scenid = new byte[8 * (functionNames.Count + 1)];
        for (int i = 0; i < functionNames.Count; i++)
            SceNid.ComputeBytes(functionNames[i]).CopyTo(scenid.AsSpan(8 * (i + 1), 8));

        // .dynamic
        byte[] dynamic = BuildDynamic(sonameOff, moduleNameOff, libraryNameOff, moduleVersion, libraryVersion);

        // .sceversion: the stub's own name and its version records. The name carries the _stub_weak
        // suffix because the export attribute BuildDynamic records is the weak one; the two travel
        // together, and a reader that trusts one and not the other reads a contradiction.
        byte[] sceversion = BuildSceVersion(libraryName + "_stub_weak");

        // .shstrtab
        var shstr = new StringTable();
        int nDynamic = shstr.Add(".dynamic");
        int nSceNid = shstr.Add(".scenid");
        int nDynStr = shstr.Add(".dynstr");
        int nDynSym = shstr.Add(".dynsym");
        int nSceVer = shstr.Add(".sceversion");
        int nShStr = shstr.Add(".shstrtab");
        byte[] shstrBytes = shstr.ToBytes();

        // Lay out sections after the 64-byte header, each 8-byte aligned. Offsets are absolute.
        var body = new MemoryStream();
        long dynamicOff = PlaceSection(body, dynamic);
        long scenidOff = PlaceSection(body, scenid);
        long dynstrOff = PlaceSection(body, dynstrBytes);
        long dynsymOff = PlaceSection(body, dynsym);
        long sceverOff = PlaceSection(body, sceversion);
        long shstrOff = PlaceSection(body, shstrBytes);
        AlignStream(body, 8);
        long shdrOff = 64 + body.Length;

        // Section header table: [0]null, [1]dynamic, [2]scenid, [3]dynstr, [4]dynsym, [5]sceversion, [6]shstrtab.
        const int shDynamic = 1, shSceNid = 2, shDynStr = 3, shDynSym = 4, shSceVer = 5, shShStr = 6;
        var shdr = new byte[64 * 7];
        WriteShdr(shdr, shDynamic, nDynamic, ShtDynamic, dynamicOff, dynamic.Length, shDynStr, 0, 8, 16);
        WriteShdr(shdr, shSceNid, nSceNid, ShtSceNid, scenidOff, scenid.Length, shDynSym, 0, 8, 8);
        WriteShdr(shdr, shDynStr, nDynStr, ShtStrTab, dynstrOff, dynstrBytes.Length, 0, 0, 1, 0);
        WriteShdr(shdr, shDynSym, nDynSym, ShtDynSym, dynsymOff, dynsym.Length, shDynStr, 1, 8, 24);
        WriteShdr(shdr, shSceVer, nSceVer, ShtProgBits, sceverOff, sceversion.Length, 0, 0, 1, 0);
        WriteShdr(shdr, shShStr, nShStr, ShtStrTab, shstrOff, shstrBytes.Length, 0, 0, 1, 0);

        var output = new MemoryStream();
        output.Write(BuildHeader(shdrOff, shShStr));
        output.Write(body.ToArray());
        output.Write(shdr);
        return output.ToArray();
    }

    private static long PlaceSection(MemoryStream body, byte[] data)
    {
        AlignStream(body, 8);
        long offset = 64 + body.Length;
        body.Write(data);
        return offset;
    }

    private static void AlignStream(MemoryStream s, int alignment)
    {
        while (s.Length % alignment != 0)
            s.WriteByte(0);
    }

    private static byte[] BuildHeader(long shoff, int shstrndx)
    {
        byte[] e = new byte[64];
        e[0] = 0x7F; e[1] = (byte)'E'; e[2] = (byte)'L'; e[3] = (byte)'F';
        e[4] = 2;    // ELFCLASS64
        e[5] = 1;    // ELFDATA2LSB
        e[6] = 1;    // EV_CURRENT
        e[7] = 0;    // OS/ABI System V
        e[8] = 3;    // ABI version
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x10), 0xFE0C); // ET_SCE_STUBLIB
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x12), 0x3E);   // x86-64
        BinaryPrimitives.WriteUInt32LittleEndian(e.AsSpan(0x14), 1);      // e_version
        BinaryPrimitives.WriteUInt64LittleEndian(e.AsSpan(0x28), (ulong)shoff); // e_shoff
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x34), 64);     // e_ehsize
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x3A), 64);     // e_shentsize
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x3C), 7);      // e_shnum
        BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(0x3E), (ushort)shstrndx);
        return e;
    }

    private static byte[] BuildDynamic(int sonameOff, int moduleNameOff, int libraryNameOff, ushort moduleVersion, ushort libraryVersion)
    {
        (long Tag, ulong Val)[] entries =
        [
            (DtSoname, (ulong)sonameOff),
            (DtSceStubModuleName, (ulong)moduleNameOff),
            (DtSceStubLibraryName, (ulong)libraryNameOff),
            (DtSceStubModuleVersion, moduleVersion),
            (DtSceStubLibraryVersion, libraryVersion),
            // Bit 1 is what separates a weak stub from a strong one; the library id sits in the high word
            // and is 0, because a stub declares exactly one library.
            (DtSceExportLibAttr, 0x0000000000000003),
            (DtNull, 0),
        ];
        byte[] d = new byte[16 * entries.Length];
        for (int i = 0; i < entries.Length; i++)
        {
            BinaryPrimitives.WriteInt64LittleEndian(d.AsSpan(16 * i), entries[i].Tag);
            BinaryPrimitives.WriteUInt64LittleEndian(d.AsSpan(16 * i + 8), entries[i].Val);
        }
        return d;
    }

    /// <summary>The eight bytes of one version record. Every stub carries two of them, both identical.</summary>
    private static ReadOnlySpan<byte> VersionRecord => [0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01];

    private static byte[] BuildSceVersion(string name)
    {
        byte[] nameBytes = Encoding.ASCII.GetBytes(name);
        // Zero word, then the byte count of everything that follows it, a tag byte, the stub's name, a
        // colon separator, then the version records. A reader takes the count on trust, so it has to
        // cover the tag byte through the last record.
        int payload = 1 + nameBytes.Length + 1 + (2 * VersionRecord.Length);
        var s = new MemoryStream();
        s.WriteByte(0); s.WriteByte(0);
        Span<byte> count = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(count, (ushort)payload);
        s.Write(count);
        s.WriteByte(0x08);
        s.Write(nameBytes);
        s.WriteByte((byte)':');
        for (int r = 0; r < 2; r++)
            s.Write(VersionRecord);
        return s.ToArray();
    }

    private static void WriteShdr(byte[] shdr, int index, int nameOff, uint type, long offset, long size,
        int link, int info, int align, int entsize)
    {
        int b = index * 64;
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b), (uint)nameOff);
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b + 4), type);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 8), 0); // flags
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 16), 0); // addr
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 24), (ulong)offset);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 32), (ulong)size);
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b + 40), (uint)link);
        BinaryPrimitives.WriteUInt32LittleEndian(shdr.AsSpan(b + 44), (uint)info);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 48), (ulong)align);
        BinaryPrimitives.WriteUInt64LittleEndian(shdr.AsSpan(b + 56), (ulong)entsize);
    }

    private sealed class StringTable
    {
        private readonly MemoryStream _stream = new();
        private readonly Dictionary<string, int> _offsets = new(StringComparer.Ordinal);

        public StringTable() => _stream.WriteByte(0);

        public int Add(string value)
        {
            if (_offsets.TryGetValue(value, out int existing))
                return existing;
            int offset = (int)_stream.Length;
            _stream.Write(Encoding.ASCII.GetBytes(value));
            _stream.WriteByte(0);
            _offsets[value] = offset;
            return offset;
        }

        public byte[] ToBytes() => _stream.ToArray();
    }
}
