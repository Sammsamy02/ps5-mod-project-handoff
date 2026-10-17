// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Texture;

/// <summary>
/// Reads and writes QOI images - a small, fast, lossless format that packs about as tightly as PNG with a
/// fraction of the work, which suits build-time texture assets. Decodes to and encodes from the same
/// red-green-blue-alpha pixels the other decoders and <see cref="GnfWriter"/> use.
/// </summary>
public static class QoiImage
{
    private const byte OpRgb = 0xFE, OpRgba = 0xFF, OpIndex = 0x00, OpDiff = 0x40, OpLuma = 0x80, OpRun = 0xC0;
    private const byte Mask2 = 0xC0;

    /// <summary>Whether the bytes start with the QOI signature.</summary>
    public static bool IsQoi(ReadOnlySpan<byte> bytes)
        => bytes.Length >= 14 && bytes[0] == 'q' && bytes[1] == 'o' && bytes[2] == 'i' && bytes[3] == 'f';

    /// <summary>Decodes a QOI image to red-green-blue-alpha pixels.</summary>
    /// <exception cref="ImageFormatException">The bytes are not a valid QOI image.</exception>
    public static DecodedImage Decode(ReadOnlySpan<byte> qoi)
    {
        if (!IsQoi(qoi))
            throw new ImageFormatException("Not a QOI image.");
        uint width = BinaryPrimitives.ReadUInt32BigEndian(qoi[4..]);
        uint height = BinaryPrimitives.ReadUInt32BigEndian(qoi[8..]);
        if (width == 0 || height == 0 || (long)width * height * 4 > int.MaxValue)
            throw new ImageFormatException("QOI has invalid dimensions.");

        long pixelCount = (long)width * height;
        byte[] pixels = new byte[pixelCount * 4];
        Span<byte> table = stackalloc byte[64 * 4];
        byte r = 0, g = 0, b = 0, a = 255;
        int pos = 14;
        long px = 0;
        while (px < pixelCount)
        {
            if (pos >= qoi.Length)
                throw new ImageFormatException("QOI ended before all pixels were read.");
            byte op = qoi[pos++];
            int run = 0;
            if (op == OpRgb)
            {
                if (pos + 3 > qoi.Length) throw new ImageFormatException("QOI truncated.");
                r = qoi[pos++]; g = qoi[pos++]; b = qoi[pos++];
            }
            else if (op == OpRgba)
            {
                if (pos + 4 > qoi.Length) throw new ImageFormatException("QOI truncated.");
                r = qoi[pos++]; g = qoi[pos++]; b = qoi[pos++]; a = qoi[pos++];
            }
            else if ((op & Mask2) == OpIndex)
            {
                int slot = (op & 0x3F) * 4;
                r = table[slot]; g = table[slot + 1]; b = table[slot + 2]; a = table[slot + 3];
            }
            else if ((op & Mask2) == OpDiff)
            {
                r = (byte)(r + ((op >> 4) & 3) - 2);
                g = (byte)(g + ((op >> 2) & 3) - 2);
                b = (byte)(b + (op & 3) - 2);
            }
            else if ((op & Mask2) == OpLuma)
            {
                if (pos >= qoi.Length) throw new ImageFormatException("QOI truncated.");
                byte op2 = qoi[pos++];
                int dg = (op & 0x3F) - 32;
                r = (byte)(r + dg + ((op2 >> 4) & 0xF) - 8);
                g = (byte)(g + dg);
                b = (byte)(b + (op2 & 0xF) - 8);
            }
            else // OpRun
            {
                run = op & 0x3F; // the current pixel repeats run+1 times
            }

            int hash = ((r * 3 + g * 5 + b * 7 + a * 11) & 63) * 4;
            table[hash] = r; table[hash + 1] = g; table[hash + 2] = b; table[hash + 3] = a;

            for (int i = 0; i <= run && px < pixelCount; i++, px++)
            {
                int at = (int)(px * 4);
                pixels[at] = r; pixels[at + 1] = g; pixels[at + 2] = b; pixels[at + 3] = a;
            }
        }
        return new DecodedImage((int)width, (int)height, pixels);
    }

    /// <summary>Encodes red-green-blue-alpha pixels to a QOI image.</summary>
    public static byte[] Encode(DecodedImage image)
    {
        ArgumentNullException.ThrowIfNull(image);
        byte[] src = image.Rgba;
        int pixels = image.Width * image.Height;
        // Worst case is one RGBA chunk (five bytes) per pixel, plus the header and the eight-byte trailer.
        byte[] output = new byte[14 + pixels * 5 + 8];
        "qoif"u8.CopyTo(output);
        BinaryPrimitives.WriteUInt32BigEndian(output.AsSpan(4), (uint)image.Width);
        BinaryPrimitives.WriteUInt32BigEndian(output.AsSpan(8), (uint)image.Height);
        output[12] = 4;    // channels
        output[13] = 0;    // colour space: sRGB with linear alpha

        Span<byte> table = stackalloc byte[64 * 4];
        int pos = 14;
        byte pr = 0, pg = 0, pb = 0, pa = 255;
        int run = 0;
        for (int i = 0; i < pixels; i++)
        {
            int at = i * 4;
            byte r = src[at], g = src[at + 1], b = src[at + 2], a = src[at + 3];
            if (r == pr && g == pg && b == pb && a == pa)
            {
                run++;
                if (run == 62 || i == pixels - 1)
                {
                    output[pos++] = (byte)(OpRun | (run - 1));
                    run = 0;
                }
                continue;
            }
            if (run > 0)
            {
                output[pos++] = (byte)(OpRun | (run - 1));
                run = 0;
            }

            int hash = ((r * 3 + g * 5 + b * 7 + a * 11) & 63);
            int slot = hash * 4;
            if (table[slot] == r && table[slot + 1] == g && table[slot + 2] == b && table[slot + 3] == a)
            {
                output[pos++] = (byte)(OpIndex | hash);
            }
            else
            {
                table[slot] = r; table[slot + 1] = g; table[slot + 2] = b; table[slot + 3] = a;
                if (a == pa)
                {
                    int dr = SignedByte(r - pr), dg = SignedByte(g - pg), db = SignedByte(b - pb);
                    int dgr = dr - dg, dgb = db - dg;
                    if (dr is >= -2 and <= 1 && dg is >= -2 and <= 1 && db is >= -2 and <= 1)
                        output[pos++] = (byte)(OpDiff | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2));
                    else if (dg is >= -32 and <= 31 && dgr is >= -8 and <= 7 && dgb is >= -8 and <= 7)
                    {
                        output[pos++] = (byte)(OpLuma | (dg + 32));
                        output[pos++] = (byte)(((dgr + 8) << 4) | (dgb + 8));
                    }
                    else
                    {
                        output[pos++] = OpRgb; output[pos++] = r; output[pos++] = g; output[pos++] = b;
                    }
                }
                else
                {
                    output[pos++] = OpRgba; output[pos++] = r; output[pos++] = g; output[pos++] = b; output[pos++] = a;
                }
            }
            pr = r; pg = g; pb = b; pa = a;
        }

        // The eight-byte end marker.
        for (int i = 0; i < 7; i++) output[pos++] = 0;
        output[pos++] = 1;
        return output[..pos];
    }

    // The signed 8-bit interpretation of a channel difference, matching the decoder's byte wrap.
    private static int SignedByte(int value)
    {
        value &= 0xFF;
        return value < 128 ? value : value - 256;
    }
}
