// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Security;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Compression;

/// <summary>One member of a <see cref="ZipArchive"/>.</summary>
/// <param name="Name">The path within the archive, using forward slashes.</param>
/// <param name="Crc32">The CRC-32 of the uncompressed contents.</param>
/// <param name="CompressedSize">The stored size in bytes.</param>
/// <param name="UncompressedSize">The size in bytes once decompressed.</param>
/// <param name="Method">The compression method: 0 is stored, 8 is DEFLATE.</param>
/// <param name="LastModified">The modification time recorded in the archive.</param>
/// <param name="IsDirectory">True when the entry is a folder rather than a file.</param>
public sealed record ZipEntry(
    string Name,
    uint Crc32,
    long CompressedSize,
    long UncompressedSize,
    ushort Method,
    DateTime LastModified,
    bool IsDirectory)
{
    internal long LocalHeaderOffset { get; init; }

    /// <summary>
    /// Writes the members out for <see cref="object.ToString"/>. This is written by hand rather than
    /// left to the compiler because the generated one hands the modification time to the general
    /// formatter, which carries the time-zone specifiers, which asks for the local time zone, which
    /// reads a time-zone database off the file system. That database is not there, and the cost is not
    /// a failure at run time: the reader pulls in the process layer of the run-time support library,
    /// and thirteen of the names that layer needs are published by nothing, so any module that so much
    /// as mentions this type fails to link. The failure names those thirteen and says nothing about an
    /// archive entry. Building the time from its parts keeps the whole chain out of the module.
    /// </summary>
    private bool PrintMembers(StringBuilder builder)
    {
        builder.Append("Name = ").Append(Name)
            .Append(", Crc32 = ").Append(Crc32)
            .Append(", CompressedSize = ").Append(CompressedSize)
            .Append(", UncompressedSize = ").Append(UncompressedSize)
            .Append(", Method = ").Append(Method)
            .Append(", LastModified = ").Append(Timestamp(LastModified))
            .Append(", IsDirectory = ").Append(IsDirectory);
        return true;
    }

    private static string Timestamp(DateTime when)
        => $"{when.Year:0000}-{Two(when.Month)}-{Two(when.Day)} {Two(when.Hour)}:{Two(when.Minute)}:{Two(when.Second)}";

    private static string Two(int value) => value < 10 ? "0" + value.ToString() : value.ToString();
}

/// <summary>
/// Reads a ZIP archive held in memory - a bundle of assets, a downloaded pack, a save export. It parses
/// the directory up front so you can list what is inside, then decompresses a member on demand (stored or
/// DEFLATE), checking each one against its recorded CRC-32. Everything is managed, so it works the same in
/// a tool, in a test, and on the console.
/// </summary>
public sealed class ZipArchive
{
    /// <summary>Set on an entry whose name is UTF-8. Without it the name is in the older encoding.</summary>
    private const ushort NameIsUtf8 = 0x0800;

    // The encoding the format used before it carried names as UTF-8. Its first half is plain text and
    // its second half is a fixed set of accented letters and box-drawing characters, so a byte above
    // the halfway mark maps through this table rather than being taken for UTF-8.
    private const string LegacyHighHalf =
        "ÇüéâäàåçêëèïîìÄÅ" +
        "ÉæÆôöòûùÿÖÜ¢£¥₧ƒ" +
        "áíóúñÑªº¿⌐¬½¼¡«»" +
        "░▒▓│┤╡╢╖╕╣║╗╝╜╛┐" +
        "└┴┬├─┼╞╟╚╔╩╦╠═╬╧" +
        "╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀" +
        "αßΓπΣσµτΦΘΩδ∞φε∩" +
        "≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";

    private static string DecodeLegacyName(byte[] data, int offset, int length)
    {
        return string.Create(length, (data, offset), static (chars, state) =>
        {
            for (int i = 0; i < chars.Length; i++)
            {
                byte b = state.data[state.offset + i];
                chars[i] = b < 0x80 ? (char)b : LegacyHighHalf[b - 0x80];
            }
        });
    }

    private const uint EndOfCentralDirectorySignature = 0x06054B50;
    private const uint CentralDirectorySignature = 0x02014B50;
    private const uint LocalHeaderSignature = 0x04034B50;

    private readonly byte[] _data;
    private readonly Dictionary<string, ZipEntry> _byName;

    private ZipArchive(byte[] data, List<ZipEntry> entries)
    {
        _data = data;
        Entries = entries;
        _byName = new Dictionary<string, ZipEntry>(entries.Count, StringComparer.Ordinal);
        foreach (ZipEntry entry in entries)
            _byName[entry.Name] = entry;
    }

    /// <summary>The members, in the order the central directory lists them.</summary>
    public IReadOnlyList<ZipEntry> Entries { get; }

    // The most a DEFLATE stream of this length can expand to. The format's own maximum ratio is
    // 1032:1, so a directory claiming more than this from the bytes present describes something that
    // cannot exist - and the claim would otherwise size the buffer, letting a hundred-byte archive ask
    // for two gigabytes before a single byte is decoded.
    private static long MaxPlausibleSize(long compressedSize) => compressedSize * 1032 + 64;

    /// <summary>Parses the directory of a ZIP archive already loaded into memory.</summary>
    public static ZipArchive Open(byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        int eocd = FindEndOfCentralDirectory(data);
        if (eocd < 0)
            throw new CompressionException("The end-of-central-directory record was not found; this is not a ZIP archive.");

        ushort count = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(eocd + 10));
        uint directorySize = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(eocd + 12));
        uint directoryOffset = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(eocd + 16));
        if (directoryOffset == 0xFFFFFFFF || directorySize == 0xFFFFFFFF || count == 0xFFFF)
            throw new CompressionException("ZIP64 archives are not supported.");
        if ((long)directoryOffset + directorySize > data.Length)
            throw new CompressionException("The central directory extends past the end of the archive.");

        var entries = new List<ZipEntry>(count);
        int pos = (int)directoryOffset;
        for (int i = 0; i < count; i++)
        {
            if (pos + 46 > data.Length || BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(pos)) != CentralDirectorySignature)
                throw new CompressionException("A central-directory entry is malformed.");

            ushort flags = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 8));
            ushort method = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 10));
            ushort modTime = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 12));
            ushort modDate = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 14));
            uint crc = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(pos + 16));
            uint compressed = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(pos + 20));
            uint uncompressed = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(pos + 24));
            ushort nameLength = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 28));
            ushort extraLength = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 30));
            ushort commentLength = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos + 32));
            uint localOffset = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(pos + 42));

            if (compressed == 0xFFFFFFFF || uncompressed == 0xFFFFFFFF || localOffset == 0xFFFFFFFF)
                throw new CompressionException("ZIP64 archives are not supported.");
            if (pos + 46 + nameLength > data.Length)
                throw new CompressionException("A central-directory name is truncated.");

            // A name is UTF-8 only when the entry says so. Reading every name as UTF-8 regardless
            // happens to round-trip within this SDK and turns a name written by anything else, in the
            // format's own older encoding, into the wrong characters or none at all.
            string name = (flags & NameIsUtf8) != 0
                ? Encoding.UTF8.GetString(data, pos + 46, nameLength)
                : DecodeLegacyName(data, pos + 46, nameLength);
            bool isDirectory = name.EndsWith('/') || (uncompressed == 0 && compressed == 0 && name.Length > 0 && name[^1] == '/');
            entries.Add(new ZipEntry(name, crc, compressed, uncompressed, method, DosDateTime(modDate, modTime), isDirectory)
            {
                LocalHeaderOffset = localOffset,
            });
            pos += 46 + nameLength + extraLength + commentLength;
        }

        return new ZipArchive(data, entries);
    }

    /// <summary>Finds a member by its exact path. Returns false when it is not present.</summary>
    public bool TryGetEntry(string name, out ZipEntry entry) => _byName.TryGetValue(name, out entry!);

    /// <summary>Decompresses a member by name.</summary>
    public byte[] Extract(string name)
        => TryGetEntry(name, out ZipEntry entry) ? Extract(entry) : throw new KeyNotFoundException($"No entry named '{name}'.");

    /// <summary>
    /// Decompresses a member and verifies it against its recorded CRC-32. A directory entry returns an
    /// empty array.
    /// </summary>
    public byte[] Extract(ZipEntry entry)
    {
        ArgumentNullException.ThrowIfNull(entry);
        if (entry.IsDirectory)
            return [];

        long headerPos = entry.LocalHeaderOffset;
        if (headerPos + 30 > _data.Length || BinaryPrimitives.ReadUInt32LittleEndian(_data.AsSpan((int)headerPos)) != LocalHeaderSignature)
            throw new CompressionException("The local file header is malformed.");
        ushort nameLength = BinaryPrimitives.ReadUInt16LittleEndian(_data.AsSpan((int)headerPos + 26));
        ushort extraLength = BinaryPrimitives.ReadUInt16LittleEndian(_data.AsSpan((int)headerPos + 28));
        long dataStart = headerPos + 30 + nameLength + extraLength;
        if (dataStart + entry.CompressedSize > _data.Length)
            throw new CompressionException("The entry data extends past the end of the archive.");

        ReadOnlySpan<byte> compressed = _data.AsSpan((int)dataStart, (int)entry.CompressedSize);
        byte[] result = entry.Method switch
        {
            0 => compressed.ToArray(),
            8 => Inflate.Raw(compressed, (int)Math.Min(entry.UncompressedSize, MaxPlausibleSize(entry.CompressedSize))),
            _ => throw new CompressionException($"The compression method {entry.Method} is not supported (only stored and DEFLATE)."),
        };

        if (result.Length != entry.UncompressedSize)
            throw new CompressionException("The decompressed size did not match the directory.");
        if (Crc32.Compute(result) != entry.Crc32)
            throw new CompressionException("The entry failed its CRC-32 check.");
        return result;
    }

    // The end-of-central-directory record sits near the end, before an optional comment of up to 64 KiB.
    private static int FindEndOfCentralDirectory(byte[] data)
    {
        if (data.Length < 22)
            return -1;
        int lowest = Math.Max(0, data.Length - 22 - 0xFFFF);
        for (int i = data.Length - 22; i >= lowest; i--)
            if (BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(i)) == EndOfCentralDirectorySignature)
                return i;
        return -1;
    }

    // MS-DOS date and time fields: date is day(0-4) month(5-8) year-1980(9-15); time is second/2(0-4) minute(5-10) hour(11-15).
    private static DateTime DosDateTime(ushort date, ushort time)
    {
        try
        {
            int year = 1980 + ((date >> 9) & 0x7F);
            int month = (date >> 5) & 0x0F;
            int day = date & 0x1F;
            int hour = (time >> 11) & 0x1F;
            int minute = (time >> 5) & 0x3F;
            int second = (time & 0x1F) * 2;
            if (month is < 1 or > 12 || day is < 1 or > 31)
                return default;
            return new DateTime(year, month, day, hour, Math.Min(minute, 59), Math.Min(second, 59), DateTimeKind.Unspecified);
        }
        catch (ArgumentOutOfRangeException)
        {
            return default;
        }
    }
}
