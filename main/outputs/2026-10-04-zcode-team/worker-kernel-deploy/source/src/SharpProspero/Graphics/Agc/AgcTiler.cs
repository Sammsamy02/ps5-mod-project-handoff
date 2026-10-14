// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Numerics;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// Rearranges pixel bytes between plain row-major (linear) order and the hardware-tiled order the GPU
/// reads. Building a texture in memory at runtime, or reading a rendered surface back into a linear
/// image, needs this. The address equations are the graphics address library's own, so the byte layout
/// matches what the GPU samples and what the display scans out.
/// </summary>
/// <remarks>
/// The offset math (<see cref="ComputeElementByteOffset"/>) is exact for every tile mode. <see cref="Tile"/>
/// and <see cref="Detile"/> move one mip level of one array slice at a time - pass the whole tiled surface
/// (sized by <see cref="AgcSurface.Compute"/>) and a tightly packed linear image for that mip.
/// </remarks>
public static partial class AgcTiler
{
    /// <summary>
    /// Computes the byte offset, within the tiled surface, of the element at <paramref name="x"/>,
    /// <paramref name="y"/>, <paramref name="z"/> (fragment <paramref name="fragmentIndex"/>) of mip
    /// <paramref name="mipLevel"/> and array slice <paramref name="arraySlice"/>.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">A coordinate or index is outside the surface.</exception>
    /// <exception cref="NotSupportedException">The tile mode, element size, and fragment count have no address equation.</exception>
    public static ulong ComputeElementByteOffset(in AgcSurfaceDescription desc, uint x, uint y, uint z = 0, uint fragmentIndex = 0, uint mipLevel = 0, uint arraySlice = 0)
    {
        AgcTilingSummary summary = AgcSurface.ComputeTilingSummary(desc);
        int equation = ResolveEquation(desc, summary.BpeLog2);
        ValidateElement(desc, summary, x, y, z, fragmentIndex, mipLevel, arraySlice);
        return ElementOffset(summary, equation, x, y, z, fragmentIndex, mipLevel, arraySlice);
    }

    /// <summary>
    /// The number of bytes of a tightly packed, row-major (linear) image of mip <paramref name="mipLevel"/> -
    /// the size the <c>linear</c> span must be for <see cref="Tile"/> and <see cref="Detile"/>. Multisampled
    /// surfaces include every fragment of every element.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">The mip level is outside the surface.</exception>
    public static ulong LinearSizeBytes(in AgcSurfaceDescription desc, uint mipLevel = 0)
    {
        AgcTilingSummary summary = AgcSurface.ComputeTilingSummary(desc);
        if (mipLevel >= (uint)summary.Mips.Length)
            throw new ArgumentOutOfRangeException(nameof(mipLevel));
        AgcTilingMip mip = summary.Mips[mipLevel];
        uint zBack = summary.NumColorFragmentsLog2 > 0 ? 1u << (int)summary.NumColorFragmentsLog2 : mip.Depth;
        int rowBytesLog2 = (int)(summary.NumDepthFragmentsLog2 + summary.BpeLog2);
        return ((ulong)mip.Width * mip.Height * zBack) << rowBytesLog2;
    }

    /// <summary>
    /// Writes the tiled form of a linear image into <paramref name="tiled"/>. <paramref name="linear"/> is
    /// the tightly packed row-major image of mip <paramref name="mipLevel"/> (multisampled surfaces store
    /// every fragment of an element together); <paramref name="tiled"/> is the whole surface, sized by
    /// <see cref="AgcSurface.Compute"/>.
    /// </summary>
    /// <exception cref="ArgumentException">A span is smaller than the surface or the mip requires.</exception>
    /// <exception cref="NotSupportedException">The tile mode, element size, and fragment count have no address equation.</exception>
    public static void Tile(Span<byte> tiled, ReadOnlySpan<byte> linear, in AgcSurfaceDescription desc, uint mipLevel = 0, uint arraySlice = 0)
        => Move(tiled, linear, desc, mipLevel, arraySlice, destIsTiled: true);

    /// <summary>
    /// Writes the linear form of a tiled surface into <paramref name="linear"/>. The inverse of
    /// <see cref="Tile"/>: <paramref name="tiled"/> is the whole surface, <paramref name="linear"/>
    /// receives the tightly packed row-major image of mip <paramref name="mipLevel"/>.
    /// </summary>
    /// <exception cref="ArgumentException">A span is smaller than the surface or the mip requires.</exception>
    /// <exception cref="NotSupportedException">The tile mode, element size, and fragment count have no address equation.</exception>
    public static void Detile(Span<byte> linear, ReadOnlySpan<byte> tiled, in AgcSurfaceDescription desc, uint mipLevel = 0, uint arraySlice = 0)
        => Move(linear, tiled, desc, mipLevel, arraySlice, destIsTiled: false);

    // Walks the element grid of one mip and one slice, copying each element between its linear position
    // and its tiled position. Tile writes the tiled side; Detile writes the linear side. dest is always
    // the written span, src the read span, and destIsTiled says which role dest plays.
    private static void Move(Span<byte> dest, ReadOnlySpan<byte> src, in AgcSurfaceDescription desc, uint mipLevel, uint arraySlice, bool destIsTiled)
    {
        AgcTilingSummary summary = AgcSurface.ComputeTilingSummary(desc);
        int equation = ResolveEquation(desc, summary.BpeLog2);
        if (mipLevel >= (uint)summary.Mips.Length)
            throw new ArgumentOutOfRangeException(nameof(mipLevel));
        if (arraySlice >= desc.NumSlices)
            throw new ArgumentOutOfRangeException(nameof(arraySlice));

        AgcTilingMip mip = summary.Mips[mipLevel];
        bool colorMsaa = summary.NumColorFragmentsLog2 > 0;
        bool depthMsaa = summary.NumDepthFragmentsLog2 > 0;
        uint elemBytesLog2 = summary.BpeLog2;
        int elemBytes = 1 << (int)elemBytesLog2;
        uint fragCount = depthMsaa ? 1u << (int)summary.NumDepthFragmentsLog2 : 1u;
        uint zBack = colorMsaa ? 1u << (int)summary.NumColorFragmentsLog2 : mip.Depth;
        int rowBytesLog2 = (int)(summary.NumDepthFragmentsLog2 + elemBytesLog2);

        ulong untiledSlicePitch = (ulong)mip.Width * mip.Height;
        ulong linearNeeded = (untiledSlicePitch * zBack) << rowBytesLog2;
        int tiledLen = destIsTiled ? dest.Length : src.Length;
        int linearLen = destIsTiled ? src.Length : dest.Length;
        if ((ulong)tiledLen < summary.TotalSizeInBytes)
            throw new ArgumentException("The tiled span is smaller than the surface.", "tiled");
        if ((ulong)linearLen < linearNeeded)
            throw new ArgumentException("The linear span is smaller than this mip requires.", "linear");

        for (uint zc = 0; zc < zBack; zc++)
        {
            for (uint y = 0; y < mip.Height; y++)
            {
                ulong rowBase = (zc * untiledSlicePitch + (ulong)y * mip.Width) << rowBytesLog2;
                for (uint x = 0; x < mip.Width; x++)
                {
                    ulong untiledElem = rowBase + ((ulong)x << rowBytesLog2);
                    for (uint f = 0; f < fragCount; f++)
                    {
                        uint fragmentIndex = colorMsaa ? zc : f;
                        uint zCoord = colorMsaa ? 0 : zc;
                        ulong tiledOffset = ElementOffset(summary, equation, x, y, zCoord, fragmentIndex, mipLevel, arraySlice);
                        ulong linearOffset = untiledElem + ((ulong)f << (int)elemBytesLog2);
                        if (destIsTiled)
                            src.Slice((int)linearOffset, elemBytes).CopyTo(dest.Slice((int)tiledOffset, elemBytes));
                        else
                            src.Slice((int)tiledOffset, elemBytes).CopyTo(dest.Slice((int)linearOffset, elemBytes));
                    }
                }
            }
        }
    }

    // The exact per-element address computation, following the graphics address library's
    // computeTiledElementByteOffset: block index in a raster of blocks, plus the swizzled element offset
    // inside the block. Linear surfaces are a padded row-major raster with no swizzle.
    private static ulong ElementOffset(in AgcTilingSummary s, int equation, uint x, uint y, uint z, uint fragmentIndex, uint mipLevel, uint arraySlice)
    {
        AgcTilingMip mip = s.Mips[mipLevel];
        uint bpeLog2 = s.BpeLog2;

        if (s.TileMode == AgcTileMode.Linear)
        {
            int widthInBytesXLog2 = (int)(s.NumDepthFragmentsLog2 + bpeLog2);
            ulong linearWidthInBlocks = mip.PaddedWidth >> (int)s.BlockWidthLog2;
            ulong tiledRowSize = linearWidthInBlocks << (int)s.BlockSizeInBytesLog2;
            ulong arrayMipOffset = s.BlockSliceSizeInBytes * arraySlice + mip.OffsetInBytes;
            ulong sliceOffset = (ulong)y * tiledRowSize + ((ulong)x << widthInBytesXLog2);
            return sliceOffset + arrayMipOffset + s.BlockSliceSizeInBytes * z;
        }

        bool isMicro = s.IsMicro;
        bool colorMsaa = s.NumColorFragmentsLog2 > 0;
        bool depthMsaa = s.NumDepthFragmentsLog2 > 0;
        uint zc = colorMsaa ? fragmentIndex : z;

        uint widthInBlocks = mip.PaddedWidth >> (int)s.BlockWidthLog2;
        uint blockSlice = arraySlice + (isMicro || colorMsaa ? 0 : zc >> (int)s.BlockDepthLog2);
        ulong tiledOffset = s.BlockSliceSizeInBytes * blockSlice + mip.OffsetInBytes;

        uint adjustedY = isMicro ? y : y + mip.MipTailCoordY;
        uint offsetBase = ComputeOffsetBase(equation, adjustedY, zc, arraySlice);
        uint blockIndex = (y >> (int)s.BlockHeightLog2) * widthInBlocks + (x >> (int)s.BlockWidthLog2);

        uint columnX = isMicro ? x : x + mip.MipTailCoordX;
        uint elementInBlock = ComputeFinalOffset(equation, offsetBase, columnX);
        ulong variableOffset = ((ulong)blockIndex << (int)(isMicro ? 8u : s.BlockSizeInBytesLog2)) + elementInBlock;

        ulong total = tiledOffset + variableOffset;
        if (depthMsaa)
            total += (ulong)fragmentIndex << (int)bpeLog2;
        return total;
    }

    private static uint ComputeOffsetBase(int equation, uint y, uint z, uint slice)
    {
        uint offset = 0;
        foreach ((byte coord, sbyte shift, uint mask) in BaseTerms[equation])
        {
            uint v = coord == 1 ? y : coord == 2 ? z : slice;
            uint term = shift > 0 ? v << shift : shift < 0 ? v >> -shift : v;
            offset ^= term & mask;
        }
        return offset;
    }

    private static uint ComputeFinalOffset(int equation, uint offsetBase, uint x)
    {
        uint offset = offsetBase;
        foreach ((sbyte shift, uint mask) in ColumnTerms[equation])
        {
            uint term = shift > 0 ? x << shift : shift < 0 ? x >> -shift : x;
            offset ^= term & mask;
        }
        return offset;
    }

    private static int ResolveEquation(in AgcSurfaceDescription desc, uint bpeLog2)
    {
        if (desc.TileMode == AgcTileMode.Linear)
            return -1;
        uint fragLog2 = (uint)BitOperations.Log2(desc.NumFragments);
        int dim = desc.Dimension == AgcSurfaceDimension.ThreeD ? 1 : 0;
        int[][][] table = desc.TileMode == AgcTileMode.Depth ? DepthEquations : RenderTargetEquations;
        int equation = table[dim][fragLog2][bpeLog2];
        if (equation < 0)
            throw new NotSupportedException("This tile mode, element size, and fragment count have no address equation.");
        return equation;
    }

    private static void ValidateElement(in AgcSurfaceDescription desc, in AgcTilingSummary summary, uint x, uint y, uint z, uint fragmentIndex, uint mipLevel, uint arraySlice)
    {
        if (mipLevel >= (uint)summary.Mips.Length)
            throw new ArgumentOutOfRangeException(nameof(mipLevel));
        if (arraySlice >= desc.NumSlices)
            throw new ArgumentOutOfRangeException(nameof(arraySlice));
        AgcTilingMip mip = summary.Mips[mipLevel];
        if (x >= mip.Width)
            throw new ArgumentOutOfRangeException(nameof(x));
        if (y >= mip.Height)
            throw new ArgumentOutOfRangeException(nameof(y));
        uint fragments = 1u << (int)Math.Max(summary.NumColorFragmentsLog2, summary.NumDepthFragmentsLog2);
        if (desc.Dimension == AgcSurfaceDimension.ThreeD)
        {
            if (z >= mip.Depth)
                throw new ArgumentOutOfRangeException(nameof(z));
        }
        else if (z > 0)
            throw new ArgumentOutOfRangeException(nameof(z));
        if (fragmentIndex >= fragments)
            throw new ArgumentOutOfRangeException(nameof(fragmentIndex));
    }
}
