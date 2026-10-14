// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Security;
using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Compression;

/// <summary>
/// Builds a ZIP archive in memory - gather a set of files under one archive to ship as a single asset, a
/// save export, or a download. Add each member, compressing it with DEFLATE or storing it as-is, then take
/// the finished bytes. A <see cref="ZipArchive"/> (or any ZIP reader) reads back what it writes.
/// </summary>
public sealed class ZipBuilder
{
    private const uint LocalHeaderSignature = 0x04034B50;
    private const uint CentralDirectorySignature = 0x02014B50;
    private const uint EndOfCentralDirectorySignature = 0x06054B50;
    private const ushort DosDate = 0x0021; // 1980-01-01, a fixed, reproducible timestamp
    private const ushort DosTime = 0x0000;

    private readonly List<byte> _output = [];
    private readonly List<Record> _records = [];

    private readonly record struct Record(
        byte[] Name, uint Crc, uint CompressedSize, uint UncompressedSize, ushort Method, uint Offset,
        bool IsDirectory, ushort Flags);

    /// <summary>
    /// Says the entry's name is UTF-8. Names are always written as UTF-8, but a reader is entitled to
    /// read one as the archive format's own old encoding unless this says otherwise, so a name outside
    /// plain text came back as different characters. It is set only where it changes anything, which
    /// keeps a plain-text name byte for byte what it was.
    /// </summary>
    private const ushort NameIsUtf8 = 0x0800;

    private static ushort FlagsFor(byte[] name)
    {
        foreach (byte b in name)
            if (b >= 0x80)
                return NameIsUtf8;
        return 0;
    }

    /// <summary>Adds a file. When <paramref name="compress"/> is true it stores whichever of DEFLATE or raw is smaller.</summary>
    public ZipBuilder Add(string name, ReadOnlySpan<byte> content, bool compress = true)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        byte[] nameBytes = Encoding.UTF8.GetBytes(name.Replace('\\', '/'));
        uint crc = Crc32.Compute(content);
        uint uncompressed = (uint)content.Length;

        byte[] stored = content.ToArray();
        byte[] payload = stored;
        ushort method = 0;
        if (compress && content.Length > 0)
        {
            byte[] deflated = Deflate.Raw(content);
            if (deflated.Length < stored.Length)
            {
                payload = deflated;
                method = 8;
            }
        }

        uint offset = (uint)_output.Count;
        ushort flags = FlagsFor(nameBytes);
        WriteLocalHeader(nameBytes, crc, (uint)payload.Length, uncompressed, method, flags);
        _output.AddRange(payload);
        _records.Add(new Record(nameBytes, crc, (uint)payload.Length, uncompressed, method, offset, false, flags));
        return this;
    }

    /// <summary>Adds a file from text (UTF-8).</summary>
    public ZipBuilder AddText(string name, string content, bool compress = true)
        => Add(name, Encoding.UTF8.GetBytes(content), compress);

    /// <summary>Adds an explicit folder entry. The name gains a trailing slash if it lacks one.</summary>
    public ZipBuilder AddDirectory(string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        string normalized = name.Replace('\\', '/');
        if (!normalized.EndsWith('/'))
            normalized += "/";
        byte[] nameBytes = Encoding.UTF8.GetBytes(normalized);

        uint offset = (uint)_output.Count;
        ushort flags = FlagsFor(nameBytes);
        WriteLocalHeader(nameBytes, 0, 0, 0, 0, flags);
        _records.Add(new Record(nameBytes, 0, 0, 0, 0, offset, true, flags));
        return this;
    }

    /// <summary>Finishes the archive and returns its bytes.</summary>
    public byte[] ToArray()
    {
        uint directoryOffset = (uint)_output.Count;
        foreach (Record record in _records)
            WriteCentralHeader(record);
        uint directorySize = (uint)_output.Count - directoryOffset;

        // The count is a sixteen-bit field. Past that the format needs its own extension, which this
        // writer does not produce, and truncating the count would write an archive whose directory says
        // it holds fewer entries than it does - readable, and quietly missing files.
        if (_records.Count > ushort.MaxValue)
            throw new CompressionException(
                $"An archive of {_records.Count} entries cannot be written: the format records the count "
                + $"in sixteen bits, so {ushort.MaxValue} is the most this writer supports.");

        WriteUInt32(EndOfCentralDirectorySignature);
        WriteUInt16(0); // this disk
        WriteUInt16(0); // disk with the central directory
        WriteUInt16((ushort)_records.Count);
        WriteUInt16((ushort)_records.Count);
        WriteUInt32(directorySize);
        WriteUInt32(directoryOffset);
        WriteUInt16(0); // comment length
        return [.. _output];
    }

    private void WriteLocalHeader(byte[] name, uint crc, uint compressed, uint uncompressed, ushort method, ushort flags)
    {
        WriteUInt32(LocalHeaderSignature);
        WriteUInt16(20);          // version needed
        WriteUInt16(flags);
        WriteUInt16(method);
        WriteUInt16(DosTime);
        WriteUInt16(DosDate);
        WriteUInt32(crc);
        WriteUInt32(compressed);
        WriteUInt32(uncompressed);
        WriteUInt16((ushort)name.Length);
        WriteUInt16(0);           // extra length
        _output.AddRange(name);
    }

    private void WriteCentralHeader(Record record)
    {
        WriteUInt32(CentralDirectorySignature);
        WriteUInt16(20);          // version made by
        WriteUInt16(20);          // version needed
        WriteUInt16(record.Flags);
        WriteUInt16(record.Method);
        WriteUInt16(DosTime);
        WriteUInt16(DosDate);
        WriteUInt32(record.Crc);
        WriteUInt32(record.CompressedSize);
        WriteUInt32(record.UncompressedSize);
        WriteUInt16((ushort)record.Name.Length);
        WriteUInt16(0);           // extra length
        WriteUInt16(0);           // comment length
        WriteUInt16(0);           // disk number start
        WriteUInt16(0);           // internal attributes
        WriteUInt32(record.IsDirectory ? 0x10u : 0u); // external attributes: the DOS directory bit
        WriteUInt32(record.Offset);
        _output.AddRange(record.Name);
    }

    private void WriteUInt16(ushort value)
    {
        _output.Add((byte)value);
        _output.Add((byte)(value >> 8));
    }

    private void WriteUInt32(uint value)
    {
        _output.Add((byte)value);
        _output.Add((byte)(value >> 8));
        _output.Add((byte)(value >> 16));
        _output.Add((byte)(value >> 24));
    }
}
