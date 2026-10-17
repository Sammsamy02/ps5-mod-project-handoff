// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Prx;

/// <summary>Raised when a file is not a module this reader understands.</summary>
public sealed class PrxFormatException(string message) : Exception(message)
{
}

/// <summary>
/// Reads a module (a <c>.prx</c> or <c>.sprx</c> ELF) and enumerates its exported symbols. The
/// reader parses the dynamic table and the dynamic-link metadata blob; it does not decrypt signed
/// images, so a plaintext module is required.
/// </summary>
public sealed class PrxImage
{
    // ELF and module-format constants.
    private const uint ElfMagic = 0x464C457FU; // 0x7F 'E' 'L' 'F'
    private const int EiClass64 = 2;
    private const int MachineX8664 = 0x3E;

    private const uint PtLoad = 0x00000001;
    private const uint PtDynamic = 0x00000002;
    private const uint PtSceDynlibData = 0x61000000;
    private const uint PtSceModuleParam = 0x61000002;
    private const uint PtSceProcParam = 0x61000001;

    /// <summary>The value that marks a library's parameter block as one this reader understands.</summary>
    private const uint ModuleParamMagic = 0x3C13F4BF;

    /// <summary>The same, for an application's block, which carries the version in the same place.</summary>
    private const uint ProcParamMagic = 0x4942524F;

    private const long DtNeeded = 1;
    private const long DtStrTab = 5;
    private const long DtSymTab = 6;
    private const long DtStrSz = 10;
    // What a module publishes: the library its exports belong to, and the module's own record. The
    // reader also accepts the older tag for a library, because a module built for the earlier console
    // uses it and is otherwise readable.
    private const long DtSceExportLib = 0x61000047;
    private const long DtSceExportLibLegacy = 0x61000013;
    private const long DtSceModuleInfo = 0x61000043;
    private const long DtSceStrTab = 0x61000035;
    private const long DtSceStrSz = 0x61000037;
    private const long DtSceSymTab = 0x61000039;
    private const long DtSceSymTabSz = 0x6100003F;
    private const long DtNull = 0;

    private readonly record struct LoadSegment(ulong VirtualAddress, ulong FileOffset, ulong FileSize);

    // Base64 alphabet for the numeric library and module ids in a symbol suffix.
    private const string Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

    private PrxImage(ushort type, string name, IReadOnlyList<PrxExport> exports, IReadOnlyList<string> needed,
        ushort libraryVersion, uint sdkVersion, ushort moduleVersion, string exportLibraryName)
    {
        Type = type;
        ModuleName = name;
        Exports = exports;
        NeededModules = needed;
        LibraryVersion = libraryVersion;
        SdkVersion = sdkVersion;
        ModuleVersion = moduleVersion;
        ExportLibraryName = exportLibraryName;
    }

    /// <summary>The ELF <c>e_type</c> value of the module.</summary>
    public ushort Type { get; }

    /// <summary>The module's own name from its info record, or empty when absent.</summary>
    public string ModuleName { get; }

    /// <summary>
    /// The version the module publishes itself at. An import of one of its names records the same
    /// version, so a stub built for this module carries it rather than a default.
    /// </summary>
    public ushort ModuleVersion { get; }

    /// <summary>
    /// The library the module publishes its exports under, which is not always its own name. Empty
    /// when the module publishes nothing.
    /// </summary>
    public string ExportLibraryName { get; }

    /// <summary>Every exported symbol.</summary>
    public IReadOnlyList<PrxExport> Exports { get; }

    /// <summary>The module files this module depends on (its needed records).</summary>
    public IReadOnlyList<string> NeededModules { get; }

    /// <summary>
    /// The version this module's export library publishes. An importer must record the same version,
    /// so a stub built for this module carries it rather than a default.
    /// </summary>
    public ushort LibraryVersion { get; }

    /// <summary>
    /// The version the module was built against, packed as major, minor and patch: 0x02000009 is
    /// 2.00 patch 9. Zero when the module records none. The system reads this when it loads the
    /// module, so a package that ships the module must require at least
    /// <see cref="RequiredSystemVersion"/>.
    /// </summary>
    public uint SdkVersion { get; }

    /// <summary>
    /// The lowest system this module runs on, as "MM.mm" (for example "02.00"). Empty when the module
    /// records no version.
    /// </summary>
    public string RequiredSystemVersion => FormatSystemVersion(SdkVersion);

    /// <summary>
    /// The value a package must carry in its <c>requiredSystemSoftwareVersion</c> to ship this module,
    /// as the hex string that field takes. Empty when the module records no version.
    /// </summary>
    public string RequiredSystemSoftwareVersion => FormatRequiredSystemSoftwareVersion(SdkVersion);

    /// <summary>
    /// Formats the major and minor of a packed version as "MM.mm". The digits are stored as they read:
    /// the byte 0x11 is the number 11, not 17, so the pair is printed digit for digit.
    /// </summary>
    public static string FormatSystemVersion(uint sdkVersion)
        => sdkVersion == 0 ? "" : $"{(sdkVersion >> 24) & 0xFF:X2}.{(sdkVersion >> 16) & 0xFF:X2}";

    /// <summary>
    /// Builds the package's <c>requiredSystemSoftwareVersion</c> for a module built against
    /// <paramref name="sdkVersion"/>: the major and minor pair, left-aligned in a 64-bit value.
    /// </summary>
    public static string FormatRequiredSystemSoftwareVersion(uint sdkVersion)
        => sdkVersion == 0 ? "" : $"0x{((ulong)(sdkVersion & 0xFFFF0000u)) << 32:X16}";

    /// <summary>
    /// Reads the module at <paramref name="path"/>. A signed container is unwrapped to its embedded
    /// ELF first, so a <c>.sprx</c> reads the same way as a <c>.prx</c>.
    /// </summary>
    public static PrxImage Load(string path) => Parse(ModuleFile.Read(path).Elf);

    /// <summary>
    /// Reads only the version a module was built against, from its parameter block. This needs the
    /// program headers and nothing else, so it answers for a module whose exports cannot be read.
    /// </summary>
    /// <returns>The packed version, or zero when the module records none.</returns>
    /// <exception cref="PrxFormatException"><paramref name="data"/> is not a module.</exception>
    public static uint ParseSdkVersion(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        ReadElfHeader(data, out _, out ulong phoff, out ushort phentsize, out ushort phnum);
        FindSegments(data, phoff, phentsize, phnum, out _, out _, out _, out long paramOffset, out long paramSize, out _);
        return ReadSdkVersion(data, paramOffset, paramSize);
    }

    /// <summary>Reads a module from bytes already in memory.</summary>
    public static PrxImage Parse(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        ReadElfHeader(data, out ushort eType, out ulong phoff, out ushort phentsize, out ushort phnum);
        FindSegments(data, phoff, phentsize, phnum,
            out long dynOffset, out long dynSize, out long dynlibOffset,
            out long paramOffset, out long paramSize, out List<LoadSegment> loads);

        uint sdkVersion = ReadSdkVersion(data, paramOffset, paramSize);

        if (dynOffset < 0)
            throw new PrxFormatException("Module has no dynamic segment.");

        // Symbol and string tables are located one of two ways. Newer modules point standard
        // DT_SYMTAB/DT_STRTAB at a virtual address mapped through a load segment; others place a
        // metadata blob and give offsets into it through the module-format tags. Resolve both to a
        // file offset.
        long stdSymVa = -1, stdStrVa = -1, sceSymOff = -1, sceStrOff = -1;
        long symTabSz = 0, strSz = 0;
        var exportLibRaw = new List<ulong>();
        ulong? moduleInfoRaw = null;
        var neededRaw = new List<ulong>();

        for (long d = dynOffset; d + 16 <= dynOffset + dynSize && d + 16 <= data.Length; d += 16)
        {
            long tag = BinaryPrimitives.ReadInt64LittleEndian(data.AsSpan((int)d));
            ulong val = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)d + 8));
            if (tag == DtNull) break;
            switch (tag)
            {
                case DtNeeded: neededRaw.Add(val); break;
                case DtSymTab: stdSymVa = (long)val; break;
                case DtStrTab: stdStrVa = (long)val; break;
                case DtStrSz: if (strSz == 0) strSz = (long)val; break;
                case DtSceSymTab: sceSymOff = (long)val; break;
                case DtSceStrTab: sceStrOff = (long)val; break;
                case DtSceStrSz: strSz = (long)val; break;
                case DtSceSymTabSz: symTabSz = (long)val; break;
                case DtSceExportLib:
                case DtSceExportLibLegacy: exportLibRaw.Add(val); break;
                case DtSceModuleInfo: moduleInfoRaw = val; break;
            }
        }

        long symBase = ResolveTable(sceSymOff, stdSymVa, dynlibOffset, loads);
        long strBase = ResolveTable(sceStrOff, stdStrVa, dynlibOffset, loads);
        if (symBase < 0 || strBase < 0)
            throw new PrxFormatException("Module dynamic table has no symbol or string table.");
        if (symTabSz <= 0)
            symTabSz = strBase > symBase ? strBase - symBase : data.Length - symBase;

        // One entry per library the module publishes, keyed by the id its exports name, so a module
        // publishing more than one resolves each export to the library it actually belongs to. The
        // record packs nameOffset | (version << 32) | (id << 48).
        var exportLibs = new Dictionary<int, string>();
        var exportLibVersions = new Dictionary<int, ushort>();
        ushort libraryVersion = 0x0001;
        foreach (ulong raw in exportLibRaw)
        {
            uint nameOff = (uint)(raw & 0xFFFFFFFF);
            int id = (int)((raw >> 48) & 0xFFFF);
            libraryVersion = (ushort)((raw >> 32) & 0xFFFF);
            string name = ReadCString(data, strBase + nameOff, strSz);
            exportLibs[id] = name;
            exportLibVersions[id] = libraryVersion;
        }

        // The module's own record, which is what a module is named by. It is not the same thing as the
        // library its exports belong to: a module publishing several libraries names none of them
        // after itself, and reading one where the other was meant fabricates the name.
        string publishedModuleName = "";
        ushort publishedModuleVersion = 0;
        if (moduleInfoRaw is ulong info)
        {
            publishedModuleName = ReadCString(data, strBase + (uint)(info & 0xFFFFFFFF), strSz);
            publishedModuleVersion = (ushort)((info >> 32) & 0xFFFF);
        }

        var exports = new List<PrxExport>();
        for (long s = symBase; s + 24 <= symBase + symTabSz && s + 24 <= data.Length; s += 24)
        {
            uint stName = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)s));
            byte stInfo = data[(int)s + 4];
            ushort stShndx = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan((int)s + 6));
            ulong stValue = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)s + 8));

            int bind = stInfo >> 4;
            int type = stInfo & 0xF;
            bool defined = stShndx != 0 && stValue != 0;
            bool visible = bind is 1 or 2; // GLOBAL or WEAK
            if (!defined || !visible)
                continue;

            string mangled = ReadCString(data, strBase + stName, strSz);
            if (!TrySplitName(mangled, out string nid, out int libId, out int modId))
                continue;

            exportLibs.TryGetValue(libId, out string? libName);
            exports.Add(new PrxExport(nid, libId, modId, libName ?? "", type == 2 /* FUNC */, stValue));
        }

        var needed = new List<string>();
        foreach (ulong raw in neededRaw)
        {
            // A needed record holds a string-table offset in its low 32 bits; the module info and
            // import records pack a version and id above it, which the mask drops.
            string name = ReadCString(data, strBase + (long)(uint)raw, strSz);
            if (name.Length > 0)
                needed.Add(name);
        }

        // The module's own name when it records one, and the library its exports belong to otherwise,
        // which is all an older module offers.
        string moduleName = publishedModuleName.Length > 0
            ? publishedModuleName
            : exportLibs.Count > 0 ? FirstValue(exportLibs) : "";
        return new PrxImage(eType, moduleName, exports, needed, libraryVersion, sdkVersion,
            publishedModuleVersion, exportLibs.Count > 0 ? FirstValue(exportLibs) : "");
    }

    /// <summary>Finds the export whose identifier matches the identifier of <paramref name="symbolName"/>.</summary>
    public PrxExport? FindByName(string symbolName)
    {
        string nid = SceNid.Compute(symbolName);
        foreach (PrxExport export in Exports)
        {
            if (export.Nid == nid)
                return export;
        }
        return null;
    }

    private static void ReadElfHeader(byte[] data, out ushort eType, out ulong phoff, out ushort phentsize, out ushort phnum)
    {
        if (data.Length < 0x40)
            throw new PrxFormatException("File is too short to be an ELF.");
        if (BinaryPrimitives.ReadUInt32LittleEndian(data) != ElfMagic)
            throw new PrxFormatException("File is not an ELF.");
        if (data[4] != EiClass64)
            throw new PrxFormatException("Only 64-bit modules are supported.");
        if (BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x12)) != MachineX8664)
            throw new PrxFormatException("Only x86-64 modules are supported.");

        eType = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x10));
        phoff = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(0x20));
        phentsize = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x36));
        phnum = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x38));
    }

    private static void FindSegments(
        byte[] data, ulong phoff, ushort phentsize, ushort phnum,
        out long dynOffset, out long dynSize, out long dynlibOffset,
        out long paramOffset, out long paramSize, out List<LoadSegment> loads)
    {
        dynOffset = -1; dynSize = 0; dynlibOffset = -1; paramOffset = -1; paramSize = 0;
        loads = [];
        for (int i = 0; i < phnum; i++)
        {
            long ph = (long)phoff + (long)i * phentsize;
            // data.Length - 0x38 rather than ph + 0x38 so a near-maximum offset cannot wrap the sum
            // negative and read out of range.
            if (ph < 0 || ph > data.Length - 0x38)
                break;
            uint pType = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)ph));
            ulong pOffset = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)ph + 0x08));
            ulong pVaddr = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)ph + 0x10));
            ulong pFilesz = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)ph + 0x20));
            if (pType == PtDynamic) { dynOffset = (long)pOffset; dynSize = (long)pFilesz; }
            else if (pType == PtSceDynlibData) { dynlibOffset = (long)pOffset; }
            else if (pType is PtSceModuleParam or PtSceProcParam) { paramOffset = (long)pOffset; paramSize = (long)pFilesz; }
            else if (pType == PtLoad) { loads.Add(new LoadSegment(pVaddr, pOffset, pFilesz)); }
        }
    }

    private static bool TrySplitName(string mangled, out string nid, out int libId, out int modId)
    {
        nid = "";
        libId = 0;
        modId = 0;
        int first = mangled.IndexOf('#');
        if (first <= 0)
            return false;
        int second = mangled.IndexOf('#', first + 1);
        if (second < 0)
            return false;

        nid = mangled[..first];
        libId = DecodeId(mangled.Substring(first + 1, second - first - 1));
        modId = DecodeId(mangled[(second + 1)..]);
        return nid.Length == SceNid.Length;
    }

    // Prefers a metadata-blob-relative offset when the module supplies one; otherwise maps a virtual
    // address through the load segments.
    // The module's parameter block: an 8-byte size, the magic, the block's own version, an attribute
    // word, then the version the module was built against. The system reads the same block the same
    // way, and treats a block older than version 2 as carrying no version at all.
    private static uint ReadSdkVersion(byte[] data, long offset, long size)
    {
        // Written so the length check cannot overflow: a crafted near-maximum offset would wrap
        // offset + 0x18 negative and slip past a naive comparison. Subtracting from the length keeps
        // both sides in range.
        if (offset < 0 || size < 0x18 || offset > data.Length - 0x18)
            return 0;
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)offset + 8));
        // A library's block and an application's carry a different mark and the same shape; reading
        // only the library's meant an application was reported as recording no version at all, which
        // is what a check for a minimum version and an edit of it both then acted on.
        if (magic != ModuleParamMagic && magic != ProcParamMagic)
            return 0;
        if (BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)offset + 12)) < 2)
            return 0;
        return BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)offset + 20));
    }

    private static long ResolveTable(long sceOffset, long standardVirtualAddress, long dynlibOffset, List<LoadSegment> loads)
    {
        if (sceOffset >= 0 && dynlibOffset >= 0)
            return dynlibOffset + sceOffset;
        if (standardVirtualAddress >= 0)
            return VirtualAddressToOffset((ulong)standardVirtualAddress, loads);
        return -1;
    }

    private static long VirtualAddressToOffset(ulong address, List<LoadSegment> loads)
    {
        foreach (LoadSegment load in loads)
        {
            if (address >= load.VirtualAddress && address < load.VirtualAddress + load.FileSize)
                return (long)(load.FileOffset + (address - load.VirtualAddress));
        }
        // No mapping: treat the value as a direct file offset.
        return (long)address;
    }

    private static int DecodeId(string encoded)
    {
        int value = 0;
        foreach (char c in encoded)
        {
            int index = Alphabet.IndexOf(c);
            if (index < 0)
                return value;
            value = value * 64 + index;
        }
        return value;
    }

    private static string ReadCString(byte[] data, long offset, long limit)
    {
        if (offset < 0 || offset >= data.Length)
            return "";
        long end = offset;
        long max = limit > 0 ? Math.Min(data.Length, offset + limit) : data.Length;
        while (end < max && data[end] != 0)
            end++;
        return Encoding.ASCII.GetString(data, (int)offset, (int)(end - offset));
    }

    private static string FirstValue(Dictionary<int, string> map)
    {
        foreach (KeyValuePair<int, string> pair in map)
            return pair.Value;
        return "";
    }
}
