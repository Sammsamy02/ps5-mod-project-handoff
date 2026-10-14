// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK
//
// Builds the exception-frame search index (the .eh_frame_hdr) from the exception frames the compiler
// emits. The index lets the loader binary-search from a program address to the frame that covers it
// instead of scanning the frames linearly. Parsing is limited to the encoding the compiler uses for
// this target (call-frame records whose function pointers are program-counter-relative); anything
// outside that shape returns no index, and the frames still work through a linear scan.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;

namespace SharpProspero.Link;

/// <summary>Reads the exception frames and builds their search index.</summary>
internal static class EhFrame
{
    // Pointer-encoding nibbles.
    private const byte FormatMask = 0x0F, ApplicationMask = 0x70;
    private const byte AbsPtr = 0x00, UData4 = 0x03, UData8 = 0x04, SData4 = 0x0B, SData8 = 0x0C;
    private const byte PcRel = 0x10;
    private const byte Omit = 0xFF;

    /// <summary>One covered function: the address it starts at and the frame record that describes it.</summary>
    internal readonly record struct Entry(ulong PcBegin, ulong FrameAddress);

    /// <summary>
    /// Parses the frames in <paramref name="data"/>, whose first byte maps to <paramref name="baseAddress"/>,
    /// appending one entry per frame. Returns false when the frames use a shape the index does not
    /// cover, in which case the caller emits no index.
    /// </summary>
    internal static bool TryParse(ReadOnlySpan<byte> data, ulong baseAddress, List<Entry> entries)
    {
        var frameEncoding = new Dictionary<int, byte>();
        int pos = 0;
        while (pos + 4 <= data.Length)
        {
            uint length = BinaryPrimitives.ReadUInt32LittleEndian(data[pos..]);
            if (length == 0)
                break;                          // terminator record
            if (length == 0xFFFFFFFF)
                return false;                   // 64-bit length form is not handled
            int recordStart = pos;
            int content = pos + 4;              // content <= data.Length from the loop guard
            // The record must hold at least the id word and must not run past the buffer. Checking the
            // unsigned length before the cast keeps recordEnd from wrapping negative.
            if (length < 4 || length > (uint)(data.Length - content))
                return false;
            int recordEnd = content + (int)length;

            uint id = BinaryPrimitives.ReadUInt32LittleEndian(data[content..]);
            if (id == 0)
            {
                if (!TryReadCieEncoding(data, content, recordEnd, out byte encoding))
                    return false;
                frameEncoding[recordStart] = encoding;
            }
            else
            {
                int cieStart = content - (int)id;
                if (!frameEncoding.TryGetValue(cieStart, out byte encoding))
                    return false;
                if (!TryDecodePointer(data, content + 4, encoding, baseAddress, out ulong pcBegin))
                    return false;
                entries.Add(new Entry(pcBegin, baseAddress + (ulong)recordStart));
            }
            pos = recordEnd;
        }
        return true;
    }

    /// <summary>
    /// Builds the index bytes placed at <paramref name="headerAddress"/>, pointing at the first frame
    /// region at <paramref name="firstFrameAddress"/> and indexing <paramref name="entries"/> (sorted
    /// by start address).
    /// </summary>
    internal static byte[] Build(ulong headerAddress, ulong firstFrameAddress, List<Entry> entries)
    {
        entries.Sort((a, b) => a.PcBegin.CompareTo(b.PcBegin));
        byte[] header = new byte[12 + entries.Count * 8];
        header[0] = 1;          // version
        header[1] = 0x1B;       // frame pointer: program-counter-relative, signed 32-bit
        header[2] = UData4;     // count: unsigned 32-bit
        header[3] = 0x3B;       // table entries: relative to this header, signed 32-bit
        BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(4), (int)((long)firstFrameAddress - (long)(headerAddress + 4)));
        BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(8), (uint)entries.Count);
        for (int i = 0; i < entries.Count; i++)
        {
            int b = 12 + i * 8;
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(b), (int)((long)entries[i].PcBegin - (long)headerAddress));
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(b + 4), (int)((long)entries[i].FrameAddress - (long)headerAddress));
        }
        return header;
    }

    // Reads a call-frame-information record's augmentation to recover the frame-pointer encoding. Only
    // the 'z' augmentation (the one the compiler emits) is handled; others return false.
    private static bool TryReadCieEncoding(ReadOnlySpan<byte> data, int content, int recordEnd, out byte encoding)
    {
        encoding = 0;
        int p = content + 4;               // past the id word
        if (p >= recordEnd)
            return false;
        byte version = data[p++];

        int augStart = p;
        while (p < recordEnd && data[p] != 0)
            p++;
        if (p >= recordEnd)
            return false;
        ReadOnlySpan<byte> augmentation = data[augStart..p];
        p++;                               // the null terminator
        if (augmentation.Length == 0 || augmentation[0] != (byte)'z')
            return false;

        if (version >= 4)
            p += 2;                        // address size + segment size
        if (!TrySkipLeb(data, ref p, recordEnd))       // code alignment factor
            return false;
        if (!TrySkipLeb(data, ref p, recordEnd))       // data alignment factor
            return false;
        if (version == 1)
            p += 1;                        // return address register (one byte)
        else if (!TrySkipLeb(data, ref p, recordEnd))
            return false;
        if (!TryReadUleb(data, ref p, recordEnd, out _))   // augmentation data length
            return false;

        bool found = false;
        for (int a = 1; a < augmentation.Length; a++)
        {
            switch ((char)augmentation[a])
            {
                case 'R':
                    if (p >= recordEnd)
                        return false;
                    encoding = data[p++];
                    found = true;
                    break;
                case 'L':
                    p += 1;                // LSDA encoding byte
                    break;
                case 'P':
                    if (p >= recordEnd)
                        return false;
                    byte personalityEncoding = data[p++];
                    int size = PointerSize(personalityEncoding);
                    if (size < 0)
                        return false;
                    p += size;
                    break;
                case 'S':
                case 'B':
                case 'G':
                    break;                 // no augmentation data
                default:
                    return false;
            }
        }
        return found && p <= recordEnd;
    }

    private static bool TryDecodePointer(ReadOnlySpan<byte> data, int offset, byte encoding, ulong baseAddress, out ulong pointer)
    {
        pointer = 0;
        if (encoding == Omit)
            return false;
        if ((encoding & ApplicationMask) != PcRel)
            return false;                  // only program-counter-relative frame pointers are indexed
        long value;
        switch (encoding & FormatMask)
        {
            case SData4:
                if (offset + 4 > data.Length) return false;
                value = BinaryPrimitives.ReadInt32LittleEndian(data[offset..]);
                break;
            case UData4:
                if (offset + 4 > data.Length) return false;
                value = BinaryPrimitives.ReadUInt32LittleEndian(data[offset..]);
                break;
            case SData8:
            case UData8:
            case AbsPtr:
                if (offset + 8 > data.Length) return false;
                value = BinaryPrimitives.ReadInt64LittleEndian(data[offset..]);
                break;
            default:
                return false;
        }
        pointer = baseAddress + (ulong)offset + (ulong)value;
        return true;
    }

    private static int PointerSize(byte encoding) => (encoding & FormatMask) switch
    {
        AbsPtr => 8,
        UData4 or SData4 => 4,
        UData8 or SData8 => 8,
        0x02 or 0x0A => 2,                 // udata2 / sdata2
        _ => -1,
    };

    private static bool TrySkipLeb(ReadOnlySpan<byte> data, ref int p, int end)
        => TryReadUleb(data, ref p, end, out _);

    private static bool TryReadUleb(ReadOnlySpan<byte> data, ref int p, int end, out ulong value)
    {
        value = 0;
        int shift = 0;
        while (p < end)
        {
            byte b = data[p++];
            value |= (ulong)(b & 0x7F) << shift;
            if ((b & 0x80) == 0)
                return true;
            shift += 7;
            if (shift >= 64)
                return false;
        }
        return false;
    }
}
