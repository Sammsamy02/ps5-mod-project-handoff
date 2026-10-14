// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;
using System.IO;
using System.IO.Compression;

namespace SharpProspero.Texture;

/// <summary>
/// Decodes a PNG image to 8-bit red-green-blue-alpha. Handles the five colour types (grayscale, RGB,
/// palette, grayscale-alpha, RGBA) at bit depths 1, 2, 4, 8, and 16, with the five scanline filters. The
/// compressed data is inflated with the runtime's deflate reader. Interlaced (Adam7) files are reported
/// as unsupported.
/// </summary>
public static class PngDecoder
{
    private static ReadOnlySpan<byte> Signature => [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];

    /// <summary>Whether the bytes begin with the PNG signature.</summary>
    public static bool IsPng(ReadOnlySpan<byte> bytes) => bytes.Length >= 8 && bytes[..8].SequenceEqual(Signature);

    /// <summary>Decodes a PNG.</summary>
    /// <exception cref="ImageFormatException">The bytes are not a valid or supported PNG.</exception>
    public static DecodedImage Decode(ReadOnlySpan<byte> bytes)
    {
        if (!IsPng(bytes))
            throw new ImageFormatException("Not a PNG file.");

        int pos = 8;
        int width = 0, height = 0;
        int bitDepth = 0, colorType = -1, interlace = 0;
        byte[]? palette = null;   // RGB triples
        byte[]? paletteAlpha = null;
        using var idat = new MemoryStream();
        bool sawHeader = false;

        while (pos + 8 <= bytes.Length)
        {
            uint length = BinaryPrimitives.ReadUInt32BigEndian(bytes.Slice(pos, 4));
            ReadOnlySpan<byte> type = bytes.Slice(pos + 4, 4);
            int dataStart = pos + 8;
            if (length > int.MaxValue || dataStart + (long)length + 4 > bytes.Length)
                throw new ImageFormatException("Truncated PNG chunk.");
            ReadOnlySpan<byte> data = bytes.Slice(dataStart, (int)length);

            if (type.SequenceEqual("IHDR"u8))
            {
                if (length < 13)
                    throw new ImageFormatException("Malformed PNG header.");
                width = (int)BinaryPrimitives.ReadUInt32BigEndian(data);
                height = (int)BinaryPrimitives.ReadUInt32BigEndian(data[4..]);
                bitDepth = data[8];
                colorType = data[9];
                if (data[10] != 0)
                    throw new ImageFormatException("Unsupported PNG compression method.");
                if (data[11] != 0)
                    throw new ImageFormatException("Unsupported PNG filter method.");
                interlace = data[12];
                sawHeader = true;
            }
            else if (type.SequenceEqual("PLTE"u8))
            {
                palette = data.ToArray();
            }
            else if (type.SequenceEqual("tRNS"u8))
            {
                paletteAlpha = data.ToArray();
            }
            else if (type.SequenceEqual("IDAT"u8))
            {
                idat.Write(data);
            }
            else if (type.SequenceEqual("IEND"u8))
            {
                break;
            }

            pos = dataStart + (int)length + 4; // skip data + CRC
        }

        if (!sawHeader)
            throw new ImageFormatException("PNG has no header chunk.");
        if (width <= 0 || height <= 0)
            throw new ImageFormatException("PNG has invalid dimensions.");
        if (interlace != 0)
            throw new ImageFormatException("Interlaced PNG is not supported.");

        int channels = colorType switch
        {
            0 => 1, // grayscale
            2 => 3, // RGB
            3 => 1, // palette index
            4 => 2, // grayscale + alpha
            6 => 4, // RGBA
            _ => throw new ImageFormatException($"Unsupported PNG colour type {colorType}."),
        };
        if (bitDepth is not (1 or 2 or 4 or 8 or 16))
            throw new ImageFormatException($"Unsupported PNG bit depth {bitDepth}.");
        if (bitDepth == 16 && colorType == 3)
            throw new ImageFormatException("Palette PNG cannot have 16-bit depth.");
        if ((bitDepth is 1 or 2 or 4) && colorType is not (0 or 3))
            throw new ImageFormatException("Sub-byte PNG depth is only valid for grayscale or palette.");
        if (colorType == 3 && palette is null)
            throw new ImageFormatException("Palette PNG has no palette.");

        // Bound the sizes so the arithmetic below stays within a 32-bit array length.
        if ((long)width * height * 4 > int.MaxValue)
            throw new ImageFormatException("PNG is too large to decode.");

        byte[] raw = Inflate(idat);

        // One filter byte per scanline, then the packed samples.
        int bitsPerPixel = channels * bitDepth;
        int stride = (int)(((long)width * bitsPerPixel + 7) / 8);
        int bpp = Math.Max(1, bitsPerPixel / 8); // filtering distance, rounded up to a whole byte
        long needed = (long)(stride + 1) * height;
        if ((long)stride * height > int.MaxValue)
            throw new ImageFormatException("PNG is too large to decode.");
        if (raw.Length < needed)
            throw new ImageFormatException("PNG pixel data is truncated.");

        byte[] unfiltered = Unfilter(raw, width, height, stride, bpp);
        return ToRgba(unfiltered, width, height, stride, bitDepth, colorType, channels, palette, paletteAlpha);
    }

    private static byte[] Inflate(MemoryStream idat)
    {
        idat.Position = 0;
        using var inflate = new ZLibStream(idat, CompressionMode.Decompress, leaveOpen: true);
        using var output = new MemoryStream();
        inflate.CopyTo(output);
        return output.ToArray();
    }

    private static byte[] Unfilter(byte[] raw, int width, int height, int stride, int bpp)
    {
        byte[] result = new byte[stride * height];
        int src = 0;
        for (int y = 0; y < height; y++)
        {
            byte filter = raw[src++];
            int row = y * stride;
            int prev = row - stride;
            for (int x = 0; x < stride; x++)
            {
                int value = raw[src + x];
                int a = x >= bpp ? result[row + x - bpp] : 0;          // reconstructed left
                int b = y > 0 ? result[prev + x] : 0;                   // reconstructed up
                int c = (y > 0 && x >= bpp) ? result[prev + x - bpp] : 0; // reconstructed up-left
                int recon = filter switch
                {
                    0 => value,
                    1 => value + a,
                    2 => value + b,
                    3 => value + ((a + b) >> 1),
                    4 => value + Paeth(a, b, c),
                    _ => throw new ImageFormatException($"Unknown PNG filter {filter}."),
                };
                result[row + x] = (byte)recon;
            }
            src += stride;
        }
        return result;
    }

    private static int Paeth(int a, int b, int c)
    {
        int p = a + b - c;
        int pa = Math.Abs(p - a);
        int pb = Math.Abs(p - b);
        int pc = Math.Abs(p - c);
        if (pa <= pb && pa <= pc) return a;
        return pb <= pc ? b : c;
    }

    private static DecodedImage ToRgba(byte[] px, int width, int height, int stride, int bitDepth,
        int colorType, int channels, byte[]? palette, byte[]? transparency)
    {
        byte[] rgba = new byte[width * height * 4];
        int outp = 0;

        // For grayscale and truecolour, tRNS names a single fully transparent sample value (the colour key).
        int sampleMask = (1 << bitDepth) - 1;
        bool hasGrayKey = colorType == 0 && transparency is { Length: >= 2 };
        bool hasRgbKey = colorType == 2 && transparency is { Length: >= 6 };
        int grayKey = hasGrayKey ? (((transparency![0] << 8) | transparency[1]) & sampleMask) : -1;
        int keyR = hasRgbKey ? (((transparency![0] << 8) | transparency[1]) & sampleMask) : -1;
        int keyG = hasRgbKey ? (((transparency![2] << 8) | transparency[3]) & sampleMask) : -1;
        int keyB = hasRgbKey ? (((transparency![4] << 8) | transparency[5]) & sampleMask) : -1;

        for (int y = 0; y < height; y++)
        {
            int row = y * stride;
            for (int x = 0; x < width; x++)
            {
                byte r, g, b, a;
                if (colorType == 3)
                {
                    int index = ReadSample(px, row, x, bitDepth);
                    int pi = index * 3;
                    if (palette is null || pi + 2 >= palette.Length)
                        throw new ImageFormatException("PNG palette index out of range.");
                    r = palette[pi]; g = palette[pi + 1]; b = palette[pi + 2];
                    a = (transparency is not null && index < transparency.Length) ? transparency[index] : (byte)0xFF;
                }
                else
                {
                    // One or more raw samples per pixel; scaled to 8 bits after the colour-key test.
                    int raw0 = ReadChannel(px, row, x * channels + 0, bitDepth);
                    switch (colorType)
                    {
                        case 0: // grayscale
                            r = g = b = (byte)ScaleSample(raw0, bitDepth);
                            a = hasGrayKey && raw0 == grayKey ? (byte)0 : (byte)0xFF;
                            break;
                        case 4: // grayscale + alpha
                            r = g = b = (byte)ScaleSample(raw0, bitDepth);
                            a = (byte)ScaleSample(ReadChannel(px, row, x * channels + 1, bitDepth), bitDepth);
                            break;
                        case 2: // RGB
                            int raw1 = ReadChannel(px, row, x * channels + 1, bitDepth);
                            int raw2 = ReadChannel(px, row, x * channels + 2, bitDepth);
                            r = (byte)ScaleSample(raw0, bitDepth);
                            g = (byte)ScaleSample(raw1, bitDepth);
                            b = (byte)ScaleSample(raw2, bitDepth);
                            a = hasRgbKey && raw0 == keyR && raw1 == keyG && raw2 == keyB ? (byte)0 : (byte)0xFF;
                            break;
                        default: // RGBA (6)
                            r = (byte)ScaleSample(raw0, bitDepth);
                            g = (byte)ScaleSample(ReadChannel(px, row, x * channels + 1, bitDepth), bitDepth);
                            b = (byte)ScaleSample(ReadChannel(px, row, x * channels + 2, bitDepth), bitDepth);
                            a = (byte)ScaleSample(ReadChannel(px, row, x * channels + 3, bitDepth), bitDepth);
                            break;
                    }
                }
                rgba[outp++] = r; rgba[outp++] = g; rgba[outp++] = b; rgba[outp++] = a;
            }
        }
        return new DecodedImage(width, height, rgba);
    }

    // Reads one sub-byte sample (bit depths 1, 2, 4) at pixel column, MSB first.
    private static int ReadSample(byte[] px, int row, int col, int bitDepth)
    {
        if (bitDepth == 8) return px[row + col];
        int bitPos = col * bitDepth;
        int bytePos = row + (bitPos >> 3);
        int shift = 8 - bitDepth - (bitPos & 7);
        int mask = (1 << bitDepth) - 1;
        return (px[bytePos] >> shift) & mask;
    }

    // Reads one channel sample by sample index within the row (handles 8-bit, 16-bit, and sub-byte).
    private static int ReadChannel(byte[] px, int row, int sampleIndex, int bitDepth)
    {
        if (bitDepth == 16)
        {
            int p = row + sampleIndex * 2;
            return (px[p] << 8) | px[p + 1];
        }
        if (bitDepth == 8)
            return px[row + sampleIndex];
        return ReadSample(px, row, sampleIndex, bitDepth);
    }

    // Scales a sample of the given bit depth up to the 0..255 range.
    private static int ScaleSample(int value, int bitDepth) => bitDepth switch
    {
        16 => value >> 8,
        8 => value,
        4 => value * 0x11,
        2 => value * 0x55,
        1 => value * 0xFF,
        _ => value,
    };
}
