// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Texture;

/// <summary>
/// Builds a GNF texture container for the console from decoded pixels. It writes a single two-dimensional
/// texture with four 8-bit channels in linear layout: the file the graphics processor loads and samples,
/// and the format the console's runtime mounts.
/// </summary>
/// <remarks>
/// The file is a header, a contents block holding one hardware texture descriptor, and the pixel data laid
/// out row by row at a padded pitch. The descriptor's address fields carry the pixel offset, the alignment
/// (as a base-two logarithm), and the pixel size until the runtime rewrites them with the real address at
/// load time. Colour channels map straight through: input byte order red, green, blue, alpha.
/// </remarks>
public static class GnfWriter
{
    private const uint Magic = 0x20464E47;        // "GNF "
    private const byte ContainerVersion = 4;       // the current console
    private const uint FormatRgba8UNorm = 56;      // four 8-bit channels, unsigned normalized
    private const uint FormatRgba8Srgb = 130;      // four 8-bit channels, sRGB colour
    private const uint ImageType2D = 9;
    private const uint TileModeLinear = 0;
    private const int HeaderSize = 8;
    private const int ContentsHeaderSize = 8;
    private const int DescriptorSize = 32;
    private const int BytesPerPixel = 4;
    private const int MaxDimension = 16384;        // the descriptor's width and height fields cap here
    private const uint BaseAlignment = 256;        // linear surfaces align to 256 bytes
    private const uint PitchTexelAlign = 64;       // 64 texels * 4 bytes = a 256-byte row

    // Channel sources for the descriptor's swizzle.
    private const uint SourceRed = 4, SourceGreen = 5, SourceBlue = 6, SourceAlpha = 7;

    /// <summary>Builds a GNF from a decoded image. The pixels are read as red-green-blue-alpha.</summary>
    /// <param name="image">The source pixels.</param>
    /// <param name="srgb">Interpret the colour channels as sRGB rather than linear (the alpha stays linear).</param>
    public static byte[] Build(DecodedImage image, bool srgb = false)
    {
        ArgumentNullException.ThrowIfNull(image);
        return BuildLinear2D(image.Rgba, image.Width, image.Height, srgb);
    }

    /// <summary>
    /// Builds a GNF for a two-dimensional texture from tightly packed red-green-blue-alpha pixels.
    /// </summary>
    /// <param name="rgba">Pixels, four bytes each, <c>width * height * 4</c> bytes, row-major top to bottom.</param>
    /// <param name="width">Width in pixels (1 to 16384).</param>
    /// <param name="height">Height in pixels (1 to 16384).</param>
    /// <param name="srgb">Interpret the colour channels as sRGB rather than linear.</param>
    public static byte[] BuildLinear2D(ReadOnlySpan<byte> rgba, int width, int height, bool srgb = false)
    {
        if (width is < 1 or > MaxDimension || height is < 1 or > MaxDimension)
            throw new ArgumentOutOfRangeException(nameof(width), $"Texture dimensions must be between 1 and {MaxDimension}.");
        if (rgba.Length < (long)width * height * BytesPerPixel)
            throw new ArgumentException("Pixel buffer is smaller than the dimensions require.", nameof(rgba));

        long pitchTexels = Align(width, (int)PitchTexelAlign);
        long pitchBytes = pitchTexels * BytesPerPixel;
        long pixelSize = pitchBytes * height;

        // The pixel data starts after the header and contents, aligned to the surface alignment.
        int contentsMin = ContentsHeaderSize + DescriptorSize;
        int pixelStart = Align(HeaderSize + contentsMin, (int)BaseAlignment);
        int contentsSize = pixelStart - HeaderSize;
        long streamSize = pixelStart + pixelSize;

        byte[] file = new byte[checked((int)streamSize)];
        Span<byte> span = file;

        // Header.
        BinaryPrimitives.WriteUInt32LittleEndian(span, Magic);
        BinaryPrimitives.WriteUInt32LittleEndian(span[4..], (uint)contentsSize);

        // Contents header.
        span[8] = ContainerVersion;
        span[9] = 1;                                   // one texture
        span[10] = (byte)Log2(BaseAlignment);          // global alignment, log2
        span[11] = 0;                                  // unused
        BinaryPrimitives.WriteUInt32LittleEndian(span[12..], (uint)streamSize);

        // The texture descriptor.
        Span<uint> descriptor = stackalloc uint[8];
        BuildDescriptor(descriptor, width, height, pixelSize, srgb);
        for (int i = 0; i < 8; i++)
            BinaryPrimitives.WriteUInt32LittleEndian(span.Slice(16 + i * 4, 4), descriptor[i]);

        // Pixel data: each row copied to its padded position; the row padding stays zero. The processor
        // derives the row pitch for a linear surface from the width, so the width must match this padding.
        int rowBytes = width * BytesPerPixel;
        for (int y = 0; y < height; y++)
            rgba.Slice(y * rowBytes, rowBytes).CopyTo(span.Slice((int)(pixelStart + y * pitchBytes), rowBytes));

        return file;
    }

    // Fills the eight-word hardware descriptor for a linear 2D four-channel texture. The address words
    // carry the file metadata (pixel offset, alignment, size) the runtime replaces at load time.
    private static void BuildDescriptor(Span<uint> w, int width, int height, long pixelSize, bool srgb)
    {
        w.Clear();
        uint format = srgb ? FormatRgba8Srgb : FormatRgba8UNorm;
        uint widthMinusOne = (uint)(width - 1);

        // Word 0: pixel offset from the pixel-stream start (a single texture starts at zero).
        w[0] = 0;

        // Word 1: alignment (log2) in the low byte, the nine-bit format, then the low two bits of width-1.
        SetBits(ref w[1], 0, 8, Log2(BaseAlignment));
        SetBits(ref w[1], 20, 9, format);
        SetBits(ref w[1], 30, 2, widthMinusOne & 0x3);

        // Word 2: the high twelve bits of width-1, then height-1.
        SetBits(ref w[2], 0, 12, widthMinusOne >> 2);
        SetBits(ref w[2], 14, 14, (uint)(height - 1));

        // Word 3: channel swizzle (identity red-green-blue-alpha), tiling, and type.
        SetBits(ref w[3], 0, 3, SourceRed);
        SetBits(ref w[3], 3, 3, SourceGreen);
        SetBits(ref w[3], 6, 3, SourceBlue);
        SetBits(ref w[3], 9, 3, SourceAlpha);
        SetBits(ref w[3], 20, 5, TileModeLinear);
        SetBits(ref w[3], 28, 4, ImageType2D);

        // Word 4 stays zero: a single-slice two-dimensional surface has no depth or array slice, and a
        // linear surface carries no pitch field. Word 7 holds the pixel size in bytes.
        w[7] = (uint)pixelSize;
    }

    private static void SetBits(ref uint word, int offset, int width, uint value)
    {
        uint mask = width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1u) << offset;
        word = (word & ~mask) | ((value << offset) & mask);
    }

    private static int Align(int value, int alignment) => (value + alignment - 1) & ~(alignment - 1);

    private static uint Log2(uint value)
    {
        uint result = 0;
        while (value > 1) { value >>= 1; result++; }
        return result;
    }
}
