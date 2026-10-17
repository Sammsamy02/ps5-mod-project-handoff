// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace SharpProspero.Link;

/// <summary>
/// A stub library read from an <c>ET_SCE_STUBLIB</c> object: the module it imports from and the
/// function names it provides. When a link leaves a reference undefined and a stub provides that
/// name, the reference becomes an import of the stub's module.
/// </summary>
public sealed class StubLibrary
{
    private const uint ShtDynSym = 11;
    private const uint ShtDynamic = 6;
    private const long DtSoname = 0x0e;
    private const long DtSceStubModuleName = 0x6100001d;
    private const long DtSceStubModuleVersion = 0x6100001f;
    private const long DtSceStubLibraryName = 0x61000021;
    private const long DtSceStubLibraryVersion = 0x61000023;

    /// <summary>The module version a stub records when it names none. Major in the high byte.</summary>
    public const ushort DefaultModuleVersion = 0x0101;

    /// <summary>The library version a stub records when it names none.</summary>
    public const ushort DefaultLibraryVersion = 0x0001;

    private StubLibrary(string soname, string moduleName, string libraryName,
        IReadOnlyList<string> provided, ushort moduleVersion, ushort libraryVersion)
    {
        Soname = soname;
        ModuleName = moduleName;
        LibraryName = libraryName;
        Provided = provided;
        ModuleVersion = moduleVersion;
        LibraryVersion = libraryVersion;
    }

    /// <summary>The module filename the stub imports from (its <c>DT_SONAME</c>), e.g. <c>libkernel.prx</c>.</summary>
    public string Soname { get; }

    /// <summary>
    /// The module name the providing module publishes, e.g. <c>libkernel</c>. Usually the soname
    /// without its extension, but a few modules name themselves differently from their file.
    /// </summary>
    public string ModuleName { get; }

    /// <summary>
    /// The library name the providing module publishes, e.g. <c>libkernel</c>. Usually the same as
    /// the module name, but a few modules publish a library under a different name.
    /// </summary>
    public string LibraryName { get; }

    /// <summary>The function names the stub provides.</summary>
    public IReadOnlyList<string> Provided { get; }

    /// <summary>
    /// The module version the stub records. An import must carry the same version the providing module
    /// exports, so this is taken from the stub rather than assumed.
    /// </summary>
    public ushort ModuleVersion { get; }

    /// <summary>The library version the stub records, matched the same way as the module version.</summary>
    public ushort LibraryVersion { get; }

    /// <summary>Reads the stub at <paramref name="path"/>.</summary>
    public static StubLibrary Load(string path) => Parse(File.ReadAllBytes(path), Path.GetFileName(path));

    /// <summary>Reads a stub from bytes.</summary>
    public static StubLibrary Parse(byte[] data, string origin)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (data.Length < 0x40 || BinaryPrimitives.ReadUInt32LittleEndian(data) != 0x464C457F)
            throw new ElfLinkException($"{origin}: not an ELF stub.");

        ulong shoff = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(0x28));
        ushort shentsize = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x3A));
        ushort shnum = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x3C));

        // The section-header table must lie within the file before it is indexed. Comparing in unsigned
        // space keeps a near-maximum offset or size from wrapping the check, so a malformed stub fails
        // as a format error rather than throwing an index exception in the loop below.
        if (shnum > 0 && (shentsize < 0x40 || shoff > (ulong)data.Length
            || (ulong)shnum * shentsize > (ulong)data.Length - shoff))
            throw new ElfLinkException($"{origin}: malformed section header table.");

        (uint Type, ulong Off, ulong Size, uint Link)[] sh = new (uint, ulong, ulong, uint)[shnum];
        for (int i = 0; i < shnum; i++)
        {
            int b = (int)shoff + i * shentsize;
            sh[i] = (
                BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b + 4)),
                BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 24)),
                BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(b + 32)),
                BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(b + 40)));
        }

        string soname = "";
        string moduleName = "";
        string libraryName = "";
        var provided = new List<string>();
        ushort moduleVersion = DefaultModuleVersion;
        ushort libraryVersion = DefaultLibraryVersion;

        for (int i = 0; i < shnum; i++)
        {
            if (sh[i].Type == ShtDynSym)
            {
                if (sh[i].Link >= (uint)sh.Length) continue; // the linked string table is out of range
                byte[] str = Slice(data, sh[sh[i].Link]);
                byte[] table = Slice(data, sh[i]);
                for (int e = 0; e + 24 <= table.Length; e += 24)
                {
                    uint nameOff = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(e));
                    byte info = table[e + 4];
                    int bind = info >> 4;
                    if (bind is 1 or 2 && nameOff != 0)
                        provided.Add(ReadCString(str, nameOff));
                }
            }
            else if (sh[i].Type == ShtDynamic)
            {
                if (sh[i].Link >= (uint)sh.Length) continue; // the linked string table is out of range
                byte[] str = Slice(data, sh[sh[i].Link]);
                byte[] table = Slice(data, sh[i]);
                for (int e = 0; e + 16 <= table.Length; e += 16)
                {
                    long tag = BinaryPrimitives.ReadInt64LittleEndian(table.AsSpan(e));
                    ulong val = BinaryPrimitives.ReadUInt64LittleEndian(table.AsSpan(e + 8));
                    if (tag == 0) break;
                    if (tag == DtSoname) soname = ReadCString(str, (uint)val);
                    else if (tag == DtSceStubModuleName) moduleName = ReadCString(str, (uint)val);
                    else if (tag == DtSceStubLibraryName) libraryName = ReadCString(str, (uint)val);
                    else if (tag == DtSceStubModuleVersion) moduleVersion = (ushort)val;
                    else if (tag == DtSceStubLibraryVersion) libraryVersion = (ushort)val;
                }
            }
        }

        // A stub without the explicit module/library names is read the old way: the module and
        // library both take the soname without its extension.
        string bare = StripExtension(soname);
        if (moduleName.Length == 0) moduleName = bare;
        if (libraryName.Length == 0) libraryName = bare;

        return new StubLibrary(soname, moduleName, libraryName, provided, moduleVersion, libraryVersion);
    }

    private static byte[] Slice(byte[] data, (uint Type, ulong Off, ulong Size, uint Link) s)
    {
        // Compare in unsigned space so a near-maximum offset or size cannot wrap the bounds check.
        if (s.Off > (ulong)data.Length || s.Size > (ulong)data.Length - s.Off)
            return [];
        return data.AsSpan((int)s.Off, (int)s.Size).ToArray();
    }

    private static string StripExtension(string name)
        => name.EndsWith(".prx", StringComparison.Ordinal) ? name[..^4] : name;

    private static string ReadCString(byte[] table, uint offset)
    {
        if (offset >= table.Length) return "";
        int end = (int)offset;
        while (end < table.Length && table[end] != 0) end++;
        return Encoding.ASCII.GetString(table, (int)offset, end - (int)offset);
    }
}
