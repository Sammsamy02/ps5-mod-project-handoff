// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>The memory layout of a linear surface: its padded row pitch, total size, and base alignment.</summary>
public readonly struct LinearSurfaceLayout
{
    internal LinearSurfaceLayout(uint paddedWidthInElements, uint rowPitchBytes, ulong sizeBytes, uint baseAlignBytes)
    {
        PaddedWidthInElements = paddedWidthInElements;
        RowPitchBytes = rowPitchBytes;
        SizeBytes = sizeBytes;
        BaseAlignBytes = baseAlignBytes;
    }

    /// <summary>The width, in elements, after padding the row up to the block width.</summary>
    public uint PaddedWidthInElements { get; }

    /// <summary>The number of bytes from one row to the next in the top mip (a multiple of 256).</summary>
    public uint RowPitchBytes { get; }

    /// <summary>The total size of the surface in bytes, across every mip and slice.</summary>
    public ulong SizeBytes { get; }

    /// <summary>The alignment, in bytes, the surface's base address must satisfy (256 for a linear surface).</summary>
    public uint BaseAlignBytes { get; }
}

/// <summary>
/// Computes the memory layout of a linear (untiled) surface - the padded row pitch, total size, and base
/// alignment - the way the graphics address library does. Linear is the storage where rows follow one
/// another in order, each padded in X to a 256-byte block; it is the layout to use for surfaces the CPU
/// reads and writes directly, for one-dimensional surfaces, and for buffers a shader addresses linearly.
/// </summary>
/// <remarks>
/// The tiled (swizzled) layouts - the ones scan-out render targets, depth targets, and optimal textures
/// use - are produced by a large generated address solver in the SDK that has no loadable module to bind
/// and is not reproduced here; only the linear layout, which is plain arithmetic, is computed. The formula
/// matches the SDK's own linear surface computation: rows are padded so a row block is 256 bytes wide, the
/// size sums padded rows over every mip level, and the base alignment is 256.
/// </remarks>
public static class LinearSurface
{
    /// <summary>The alignment a linear surface's base address must satisfy: a 256-byte block.</summary>
    public const uint BlockSizeBytes = 256;

    /// <summary>
    /// Computes the layout of a linear surface of <paramref name="width"/> by <paramref name="height"/>
    /// elements whose element is <paramref name="bytesPerElement"/> bytes (a power of two, 1 to 16).
    /// </summary>
    /// <param name="width">Surface width in texels.</param>
    /// <param name="height">Surface height in texels.</param>
    /// <param name="bytesPerElement">Bytes per element - 4 for an eight-bit-per-channel colour, for example. A power of two in [1, 16].</param>
    /// <param name="numMips">Number of mip levels (at least one).</param>
    /// <param name="numSlices">Number of array slices (at least one).</param>
    /// <param name="texelsPerElementWide">Texels per element horizontally: 1 uncompressed, 4 for block-compressed, 2 for 4:2:2.</param>
    /// <param name="texelsPerElementTall">Texels per element vertically: 1 uncompressed, 4 for block-compressed.</param>
    /// <param name="multiElementMultiplier">1 except for the three-channel 32-bit format, which uses 3.</param>
    /// <param name="numFragmentsLog2">Log2 of the fragments per pixel (0 for a single-sampled surface).</param>
    /// <exception cref="ArgumentOutOfRangeException">A dimension is zero, or <paramref name="bytesPerElement"/> is not a power of two in [1, 16].</exception>
    public static LinearSurfaceLayout Compute(
        uint width,
        uint height,
        uint bytesPerElement,
        uint numMips = 1,
        uint numSlices = 1,
        uint texelsPerElementWide = 1,
        uint texelsPerElementTall = 1,
        uint multiElementMultiplier = 1,
        uint numFragmentsLog2 = 0)
    {
        if (width == 0)
            throw new ArgumentOutOfRangeException(nameof(width));
        if (height == 0)
            throw new ArgumentOutOfRangeException(nameof(height));
        if (numMips == 0)
            throw new ArgumentOutOfRangeException(nameof(numMips));
        if (numSlices == 0)
            throw new ArgumentOutOfRangeException(nameof(numSlices));
        if (texelsPerElementWide == 0)
            throw new ArgumentOutOfRangeException(nameof(texelsPerElementWide));
        if (texelsPerElementTall == 0)
            throw new ArgumentOutOfRangeException(nameof(texelsPerElementTall));
        if (multiElementMultiplier == 0)
            throw new ArgumentOutOfRangeException(nameof(multiElementMultiplier));

        uint bytesPerElementLog2 = Log2PowerOfTwo(bytesPerElement);
        if (bytesPerElementLog2 > 4)
            throw new ArgumentOutOfRangeException(nameof(bytesPerElement), "Must be a power of two in [1, 16].");

        // Linear block width in elements: kLog2BlockSizeLinear widthLog2 is 8 - log2(bytesPerElement), so a
        // row block spans 256 bytes. Block height and depth are 1 for a linear surface.
        uint blockWidth = 1u << (int)(8 - bytesPerElementLog2);

        uint widthInElements = (width * multiElementMultiplier + texelsPerElementWide - 1) / texelsPerElementWide;
        uint heightInElements = (height + texelsPerElementTall - 1) / texelsPerElementTall;

        int shift = (int)(bytesPerElementLog2 + numFragmentsLog2);
        ulong blockSliceSize = 0;
        uint topPaddedWidth = 0;
        for (uint mip = 0; mip < numMips; mip++)
        {
            uint mipWidthCeil = Max1(ShiftCeil(widthInElements, mip));
            uint mipHeightCeil = Max1(ShiftCeil(heightInElements, mip));
            uint paddedWidth = PowTwoAlign(mipWidthCeil, blockWidth);
            if (mip == 0)
                topPaddedWidth = paddedWidth;
            blockSliceSize += (ulong)mipHeightCeil * paddedWidth << shift;
        }

        ulong totalSize = blockSliceSize * numSlices;
        uint rowPitchBytes = topPaddedWidth << shift;
        return new LinearSurfaceLayout(topPaddedWidth, rowPitchBytes, totalSize, BlockSizeBytes);
    }

    private static uint PowTwoAlign(uint x, uint align) => (x + align - 1) & ~(align - 1);

    private static uint ShiftCeil(uint a, uint b) => (a + (1u << (int)b) - 1) >> (int)b;

    private static uint Max1(uint x) => x < 1 ? 1 : x;

    private static uint Log2PowerOfTwo(uint value)
    {
        if (value == 0 || (value & (value - 1)) != 0)
            throw new ArgumentOutOfRangeException(nameof(value), "Must be a power of two.");
        uint log = 0;
        while ((value >>= 1) != 0)
            log++;
        return log;
    }
}
