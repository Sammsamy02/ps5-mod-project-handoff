// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace SharpProspero.Link;

/// <summary>
/// One object member of an archive. <paramref name="IsWholeFile"/> is set when the file handed over was
/// a bare object rather than an archive, because an object is always taken while a member of an archive
/// is taken only when something still wants a name it defines.
/// </summary>
public readonly record struct ArMember(string Name, byte[] Data, bool IsWholeFile = false);

/// <summary>
/// Reads a System V <c>ar</c> archive into its object members. It skips the symbol index and resolves
/// long member names through the extended-name table. A file that is a bare ELF object rather than an
/// archive is returned as a single member.
/// </summary>
public static class ArReader
{
    private const string Magic = "!<arch>\n";
    private const uint ElfMagic = 0x464C457FU;

    /// <summary>Reads the members of <paramref name="data"/>.</summary>
    public static IReadOnlyList<ArMember> Read(byte[] data, string origin)
    {
        ArgumentNullException.ThrowIfNull(data);

        // A bare ELF object (some stub libraries) is a single member, and is the whole file.
        if (data.Length >= 4 && System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(data) == ElfMagic)
            return [new ArMember(origin, data, IsWholeFile: true)];

        if (data.Length < 8 || Encoding.ASCII.GetString(data, 0, 8) != Magic)
            throw new ElfLinkException($"{origin}: not an archive.");

        var members = new List<ArMember>();
        byte[]? longNames = null;
        int pos = 8;
        while (pos + 60 <= data.Length)
        {
            string rawName = Encoding.ASCII.GetString(data, pos, 16).TrimEnd();
            string sizeField = Encoding.ASCII.GetString(data, pos + 48, 10).Trim();
            // A member size is an unsigned decimal; a negative or non-numeric field marks the end of
            // the usable members. Rejecting it here keeps a malformed archive from advancing `pos`
            // backward (a hang) or slicing a negative length.
            if (!int.TryParse(sizeField, NumberStyles.None, CultureInfo.InvariantCulture, out int size) || size < 0)
                break;
            int content = pos + 60;
            // Widen to long so a near-maximum size field cannot wrap the bounds check negative.
            if ((long)content + size > data.Length)
                break;

            if (rawName is "/" or "/SYM64/")
            {
                // Symbol index member: not needed; the reader resolves symbols itself.
            }
            else if (rawName == "//")
            {
                longNames = data.AsSpan(content, size).ToArray();
            }
            else
            {
                string name = ResolveName(rawName, longNames);
                members.Add(new ArMember(name, data.AsSpan(content, size).ToArray()));
            }

            pos = content + size + (size & 1); // members are padded to even length
        }
        return members;
    }

    private static string ResolveName(string rawName, byte[]? longNames)
    {
        if (rawName.StartsWith('/') && longNames is not null &&
            int.TryParse(rawName.AsSpan(1), NumberStyles.Integer, CultureInfo.InvariantCulture, out int offset)
            && offset >= 0 && offset <= longNames.Length)
        {
            int end = offset;
            while (end < longNames.Length && longNames[end] is not ((byte)'\n' or (byte)'/'))
                end++;
            return Encoding.ASCII.GetString(longNames, offset, end - offset);
        }
        return rawName.TrimEnd('/');
    }
}
