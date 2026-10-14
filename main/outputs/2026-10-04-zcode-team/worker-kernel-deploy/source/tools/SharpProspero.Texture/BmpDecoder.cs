// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Texture;

/// <summary>
/// Decodes an uncompressed BMP image to 8-bit red-green-blue-alpha. Handles 24-bit (blue-green-red) and
/// 32-bit (blue-green-red-alpha) pixels, top-down or bottom-up.
/// </summary>
public static class BmpDecoder
{
    /// <summary>Whether the bytes begin with the BMP signature.</summary>
    public static bool IsBmp(ReadOnlySpan<byte> bytes) => bytes.Length >= 2 && bytes[0] == (byte)'B' && bytes[1] == (byte)'M';

    /// <summary>Decodes a BMP.</summary>
    /// <exception cref="ImageFormatException">The bytes are not a valid or supported BMP.</exception>
    public static DecodedImage Decode(ReadOnlySpan<byte> bytes)
    {
        if (!IsBmp(bytes) || bytes.Length < 54)
            throw new ImageFormatException("Not a BMP file.");

        int pixelOffset = (int)BinaryPrimitives.ReadUInt32LittleEndian(bytes[10..]);
        int headerSize = (int)BinaryPrimitives.ReadUInt32LittleEndian(bytes[14..]);
        if (headerSize < 40)
            throw new ImageFormatException("Unsupported BMP header.");

        int width = BinaryPrimitives.ReadInt32LittleEndian(bytes[18..]);
        int rawHeight = BinaryPrimitives.ReadInt32LittleEndian(bytes[22..]);
        int bitCount = BinaryPrimitives.ReadUInt16LittleEndian(bytes[28..]);
        int compression = (int)BinaryPrimitives.ReadUInt32LittleEndian(bytes[30..]);

        if (width <= 0 || rawHeight == 0 || rawHeight == int.MinValue)
            throw new ImageFormatException("BMP has invalid dimensions.");
        bool topDown = rawHeight < 0;
        int height = Math.Abs(rawHeight);

        // compression 0 (BI_RGB) for 24/32 bit; 3 (BI_BITFIELDS) for 32-bit is accepted as B,G,R,A.
        if (bitCount is not (24 or 32))
            throw new ImageFormatException("Only 24-bit and 32-bit BMP are supported.");
        if (compression != 0 && !(compression == 3 && bitCount == 32))
            throw new ImageFormatException("Compressed BMP is not supported.");

        if ((long)width * height * 4 > int.MaxValue)
            throw new ImageFormatException("BMP is too large to decode.");

        int bpp = bitCount / 8;
        int stride = (int)((((long)width * bpp) + 3) & ~3L); // rows are padded to four bytes
        long needed = (long)pixelOffset + (long)stride * height;
        if (pixelOffset < 0 || needed > bytes.Length)
            throw new ImageFormatException("Truncated BMP pixel data.");

        byte[] rgba = new byte[width * height * 4];
        bool sawAlpha = false;
        for (int y = 0; y < height; y++)
        {
            int srcRow = topDown ? y : height - 1 - y;
            int s = pixelOffset + srcRow * stride;
            int d = y * width * 4;
            for (int x = 0; x < width; x++)
            {
                int p = s + x * bpp;
                rgba[d + 0] = bytes[p + 2]; // red
                rgba[d + 1] = bytes[p + 1]; // green
                rgba[d + 2] = bytes[p + 0]; // blue
                byte a = bpp == 4 ? bytes[p + 3] : (byte)0xFF;
                if (a != 0) sawAlpha = true;
                rgba[d + 3] = a;
                d += 4;
            }
        }

        // In a plain (uncompressed) 32-bit BMP the fourth byte is a reserved field that many encoders
        // leave zero. Reading it as alpha would make the whole image transparent, so when it is uniformly
        // zero treat the image as opaque. A bit-field BMP always carries a real alpha channel.
        if (bpp == 4 && compression == 0 && !sawAlpha)
            for (int i = 3; i < rgba.Length; i += 4)
                rgba[i] = 0xFF;

        return new DecodedImage(width, height, rgba);
    }
}
