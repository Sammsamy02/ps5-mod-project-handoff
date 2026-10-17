// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Prx;

/// <summary>One versioned library tag a module records: its own module info and export library, and,
/// per library it imports from, the module and library versions it was built to bind against.</summary>
/// <param name="Name">The library or module name.</param>
/// <param name="Version">The recorded version, major in the high byte.</param>
/// <param name="Kind">What the tag is: module info, export library, needed module, or import library.</param>
public readonly record struct LibraryTag(string Name, ushort Version, string Kind);

/// <summary>What a module records about the system it targets and the libraries it binds against.</summary>
/// <param name="SdkVersion">The version the module was built against (the load-time gate), or zero when it records none.</param>
/// <param name="Libraries">The versioned library tags in the module's dynamic table.</param>
public sealed record ModuleTargetInfo(uint SdkVersion, IReadOnlyList<LibraryTag> Libraries);

/// <summary>
/// Adjusts what a module records about the system it targets, so a module built for one system can be
/// retargeted to another. The system rejects at load a module built against a newer system than the one
/// it runs on; rewriting the module's recorded version to the target lets it load. A per-library version
/// tag can be rewritten too, for the case where a library publishes a different version on the target.
/// The edits are in place on the module's ELF bytes; a signed container is unwrapped and re-signed by
/// the caller.
/// </summary>
public sealed class ModuleEditor
{
    private const uint ElfMagic = 0x464C457FU;
    private const int EiClass64 = 2;

    private const uint PtLoad = 0x00000001;
    private const uint PtDynamic = 0x00000002;
    private const uint PtSceDynlibData = 0x61000000;
    private const uint PtSceModuleParam = 0x61000002;
    private const uint PtSceProcParam = 0x61000001;

    // What marks the block as one this reader understands: the first for a library, the second for an
    // application. Both carry the version in the same place.
    private const uint ModuleParamMagic = 0x3C13F4BF;
    private const uint ProcParamMagic = 0x4942524F;

    private const long DtNull = 0;
    private const long DtStrTab = 5;
    private const long DtStrSz = 10;
    private const long DtSceModuleInfo = 0x61000043;
    private const long DtSceExportLib = 0x61000047;
    private const long DtSceExportLibLegacy = 0x61000013;
    private const long DtSceNeededModule = 0x61000045;
    private const long DtSceImportLib = 0x61000049;
    private const long DtSceStrTab = 0x61000035;
    private const long DtSceStrSz = 0x61000037;

    private readonly record struct LoadSegment(ulong VirtualAddress, ulong FileOffset, ulong FileSize);

    // One version-bearing dynamic record, with the file offset of its 16-byte entry so it can be rewritten.
    private readonly record struct VersionRecord(long EntryOffset, string Name, ushort Version, string Kind);

    /// <summary>Reads the version a module targets and the versioned library tags it records.</summary>
    /// <exception cref="PrxFormatException"><paramref name="elf"/> is not a module this reader understands.</exception>
    public static ModuleTargetInfo Read(byte[] elf)
    {
        ArgumentNullException.ThrowIfNull(elf);
        Layout layout = ParseLayout(elf);
        uint sdkVersion = ReadSdkVersion(elf, layout.ParamOffset, layout.ParamSize);
        var tags = new List<LibraryTag>();
        foreach (VersionRecord record in ScanVersionRecords(elf, layout))
            tags.Add(new LibraryTag(record.Name, record.Version, record.Kind));
        return new ModuleTargetInfo(sdkVersion, tags);
    }

    /// <summary>
    /// Rewrites the version the module records it was built against, in place. Only the major and minor
    /// gate the load, so the patch is preserved and the major and minor of <paramref name="targetPacked"/>
    /// are written. Returns false when the module records no version block (nothing to rewrite, and the
    /// load is not gated).
    /// </summary>
    /// <param name="elf">The module ELF bytes, edited in place.</param>
    /// <param name="targetPacked">The target version, major and minor packed (0x0900 for 9.00).</param>
    /// <exception cref="PrxFormatException"><paramref name="elf"/> is not a module this reader understands.</exception>
    public static bool SetSdkVersion(byte[] elf, ushort targetPacked)
    {
        ArgumentNullException.ThrowIfNull(elf);
        Layout layout = ParseLayout(elf);
        long offset = layout.ParamOffset;
        long size = layout.ParamSize;
        if (offset < 0 || size < 0x18 || offset > elf.Length - 0x18)
            return false;
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan((int)offset + 8));
        if (magic != ModuleParamMagic && magic != ProcParamMagic)
            return false;
        // A block older than version 2 carries no version field at +0x14; ReadSdkVersion reports such a
        // block as recording no version, so rewriting +0x14 here would corrupt an unrelated field and
        // report a change the load never sees. Match the reader and leave it alone.
        if (BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan((int)offset + 12)) < 2)
            return false;

        uint current = BinaryPrimitives.ReadUInt32LittleEndian(elf.AsSpan((int)offset + 20));
        // The version packs major.minor in the high 16 bits and the patch in the low 16. Keep the patch;
        // set the major and minor, which are all the load-time check reads.
        uint updated = ((uint)targetPacked << 16) | (current & 0xFFFFu);
        BinaryPrimitives.WriteUInt32LittleEndian(elf.AsSpan((int)offset + 20), updated);
        return true;
    }

    /// <summary>
    /// Rewrites the recorded module version of the needed-module tag naming <paramref name="name"/>, in
    /// place, so the module binds against the version the target system's library publishes. The module
    /// version is the one the loader matches; the separate import-library version is left alone. Returns
    /// the number of tags rewritten.
    /// </summary>
    /// <param name="elf">The module ELF bytes, edited in place.</param>
    /// <param name="name">The module name to match.</param>
    /// <param name="version">The version to record, major in the high byte (0x0101 for 1.1).</param>
    /// <exception cref="PrxFormatException"><paramref name="elf"/> is not a module this reader understands.</exception>
    public static int SetLibraryVersion(byte[] elf, string name, ushort version)
    {
        ArgumentNullException.ThrowIfNull(elf);
        ArgumentException.ThrowIfNullOrEmpty(name);
        Layout layout = ParseLayout(elf);

        int rewritten = 0;
        foreach (VersionRecord record in ScanVersionRecords(elf, layout))
        {
            if (record.Kind != "needed module"
                || !string.Equals(record.Name, name, StringComparison.Ordinal))
                continue;

            // The value packs nameOffset | (version << 32) | (id << 48); replace only the version bits.
            ulong value = BinaryPrimitives.ReadUInt64LittleEndian(elf.AsSpan((int)record.EntryOffset + 8));
            value = (value & ~(0xFFFFUL << 32)) | ((ulong)version << 32);
            BinaryPrimitives.WriteUInt64LittleEndian(elf.AsSpan((int)record.EntryOffset + 8), value);
            rewritten++;
        }
        return rewritten;
    }

    private readonly record struct Layout(
        long DynOffset, long DynSize, long DynlibOffset, long ParamOffset, long ParamSize, List<LoadSegment> Loads);

    private static Layout ParseLayout(byte[] data)
    {
        if (data.Length < 0x40)
            throw new PrxFormatException("File is too short to be an ELF.");
        if (BinaryPrimitives.ReadUInt32LittleEndian(data) != ElfMagic)
            throw new PrxFormatException("File is not an ELF.");
        if (data[4] != EiClass64)
            throw new PrxFormatException("Only 64-bit modules are supported.");

        ulong phoff = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(0x20));
        ushort phentsize = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x36));
        ushort phnum = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x38));

        long dynOffset = -1, dynSize = 0, dynlibOffset = -1, paramOffset = -1, paramSize = 0;
        var loads = new List<LoadSegment>();
        for (int i = 0; i < phnum; i++)
        {
            long ph = (long)phoff + (long)i * phentsize;
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
        return new Layout(dynOffset, dynSize, dynlibOffset, paramOffset, paramSize, loads);
    }

    private static uint ReadSdkVersion(byte[] data, long offset, long size)
    {
        if (offset < 0 || size < 0x18 || offset > data.Length - 0x18)
            return 0;
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)offset + 8));
        if (magic != ModuleParamMagic && magic != ProcParamMagic)
            return 0;
        if (BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)offset + 12)) < 2)
            return 0;
        return BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan((int)offset + 20));
    }

    private static List<VersionRecord> ScanVersionRecords(byte[] data, Layout layout)
    {
        var records = new List<VersionRecord>();
        // Reject a dynamic segment whose offset or size falls outside the file before looping. The
        // subtraction keeps the check from wrapping: a crafted near-maximum offset would overflow
        // offset + size and slip past a naive comparison, then index out of range.
        if (layout.DynOffset < 0 || layout.DynSize < 0
            || layout.DynOffset > data.Length || layout.DynSize > data.Length - layout.DynOffset)
            return records;

        // Locate the string table the record names index into.
        long sceStrOff = -1, stdStrVa = -1, strSz = 0;
        for (long d = layout.DynOffset; d + 16 <= layout.DynOffset + layout.DynSize && d + 16 <= data.Length; d += 16)
        {
            long tag = BinaryPrimitives.ReadInt64LittleEndian(data.AsSpan((int)d));
            ulong val = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)d + 8));
            if (tag == DtNull) break;
            switch (tag)
            {
                case DtSceStrTab: sceStrOff = (long)val; break;
                case DtStrTab: stdStrVa = (long)val; break;
                case DtSceStrSz: strSz = (long)val; break;
                case DtStrSz: if (strSz == 0) strSz = (long)val; break;
            }
        }

        long strBase = -1;
        if (sceStrOff >= 0 && layout.DynlibOffset >= 0)
            strBase = layout.DynlibOffset + sceStrOff;
        else if (stdStrVa >= 0)
            strBase = VirtualAddressToOffset((ulong)stdStrVa, layout.Loads);
        if (strBase < 0)
            return records;

        for (long d = layout.DynOffset; d + 16 <= layout.DynOffset + layout.DynSize && d + 16 <= data.Length; d += 16)
        {
            long tag = BinaryPrimitives.ReadInt64LittleEndian(data.AsSpan((int)d));
            ulong val = BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan((int)d + 8));
            if (tag == DtNull) break;

            string? kind = tag switch
            {
                DtSceModuleInfo => "module info",
                DtSceExportLib or DtSceExportLibLegacy => "export library",
                DtSceNeededModule => "needed module",
                DtSceImportLib => "import library",
                _ => null,
            };
            if (kind is null)
                continue;

            uint nameOff = (uint)(val & 0xFFFFFFFF);
            ushort version = (ushort)((val >> 32) & 0xFFFF);
            string name = ReadCString(data, strBase + nameOff, strSz);
            records.Add(new VersionRecord(d, name, version, kind));
        }
        return records;
    }

    private static long VirtualAddressToOffset(ulong address, List<LoadSegment> loads)
    {
        foreach (LoadSegment load in loads)
        {
            if (address >= load.VirtualAddress && address < load.VirtualAddress + load.FileSize)
                return (long)(load.FileOffset + (address - load.VirtualAddress));
        }
        return (long)address;
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
}
