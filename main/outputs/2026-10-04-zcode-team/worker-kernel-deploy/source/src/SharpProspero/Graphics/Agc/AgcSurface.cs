// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>How a surface's memory is laid out: linear, or one of the hardware-tiled modes.</summary>
public enum AgcTileMode : uint
{
    /// <summary>Row-major storage. Recommended for one-dimensional surfaces and CPU-visible data.</summary>
    Linear = 0,

    /// <summary>The tiling required for depth targets (64 KB blocks, Z order).</summary>
    Depth = 24,

    /// <summary>The tiling recommended for render targets; the only mode video-out accepts (64 KB blocks).</summary>
    RenderTarget = 27,
}

/// <summary>The dimensionality of a surface.</summary>
public enum AgcSurfaceDimension : uint
{
    /// <summary>A one-dimensional surface.</summary>
    OneD = 0,
    /// <summary>A two-dimensional surface.</summary>
    TwoD = 1,
    /// <summary>A three-dimensional (volume) surface.</summary>
    ThreeD = 2,
}

/// <summary>Describes a surface to lay out: its tiling, dimensions, format element size, mips, and slices.</summary>
/// <remarks>Creates a description. Uncompressed single-sampled defaults; override for arrays, mips, compression, or MSAA.</remarks>
public readonly struct AgcSurfaceDescription(
    AgcTileMode tileMode,
    AgcSurfaceDimension dimension,
    uint width,
    uint height,
    uint bytesPerElement,
    uint depth = 1,
    uint numMips = 1,
    uint numSlices = 1,
    uint numFragments = 1,
    uint texelsPerElementWide = 1,
    uint texelsPerElementTall = 1,
    uint multiElementMultiplier = 1)
{

    /// <summary>The tiling mode.</summary>
    public AgcTileMode TileMode { get; } = tileMode;
    /// <summary>The dimensionality.</summary>
    public AgcSurfaceDimension Dimension { get; } = dimension;
    /// <summary>Width in texels.</summary>
    public uint Width { get; } = width;
    /// <summary>Height in texels.</summary>
    public uint Height { get; } = height;
    /// <summary>Depth in texels (for a volume surface).</summary>
    public uint Depth { get; } = depth;
    /// <summary>Bytes per element - per texel uncompressed, per block for compressed. A power of two in [1, 16].</summary>
    public uint BytesPerElement { get; } = bytesPerElement;
    /// <summary>Number of mip levels.</summary>
    public uint NumMips { get; } = numMips;
    /// <summary>Number of array slices.</summary>
    public uint NumSlices { get; } = numSlices;
    /// <summary>Fragments per pixel (1, 2, 4, or 8).</summary>
    public uint NumFragments { get; } = numFragments;
    /// <summary>Texels per element horizontally (1 uncompressed, 4 block-compressed, 2 for 4:2:2).</summary>
    public uint TexelsPerElementWide { get; } = texelsPerElementWide;
    /// <summary>Texels per element vertically (1 uncompressed, 4 block-compressed).</summary>
    public uint TexelsPerElementTall { get; } = texelsPerElementTall;
    /// <summary>1 except for the three-channel 32-bit format, which uses 3.</summary>
    public uint MultiElementMultiplier { get; } = multiElementMultiplier;
}

/// <summary>The layout of one mip level: its byte offset within a slice, its size, and its padded extent.</summary>
public readonly struct AgcMipInfo
{
    internal AgcMipInfo(ulong offsetBytes, ulong sizeBytes, uint paddedWidth, uint paddedHeight, uint paddedDepth)
    {
        OffsetBytes = offsetBytes;
        SizeBytes = sizeBytes;
        PaddedWidth = paddedWidth;
        PaddedHeight = paddedHeight;
        PaddedDepth = paddedDepth;
    }

    /// <summary>Byte offset of this mip within one array slice.</summary>
    public ulong OffsetBytes { get; }
    /// <summary>Size of this mip in bytes.</summary>
    public ulong SizeBytes { get; }
    /// <summary>Padded width in elements.</summary>
    public uint PaddedWidth { get; }
    /// <summary>Padded height in elements.</summary>
    public uint PaddedHeight { get; }
    /// <summary>Padded depth in elements.</summary>
    public uint PaddedDepth { get; }
}

/// <summary>The computed memory layout of a surface: total size, base alignment, block dimensions, and per-mip info.</summary>
public readonly struct AgcSurfaceLayout
{
    internal AgcSurfaceLayout(ulong totalSizeBytes, uint baseAlignBytes, uint blockWidth, uint blockHeight, uint blockDepth, uint numBlockSlices, uint firstMipLevelInTail, AgcMipInfo[] mips)
    {
        TotalSizeBytes = totalSizeBytes;
        BaseAlignBytes = baseAlignBytes;
        BlockWidth = blockWidth;
        BlockHeight = blockHeight;
        BlockDepth = blockDepth;
        NumBlockSlices = numBlockSlices;
        FirstMipLevelInTail = firstMipLevelInTail;
        Mips = mips;
    }

    /// <summary>Total size in bytes, across every mip and slice.</summary>
    public ulong TotalSizeBytes { get; }
    /// <summary>Alignment the base address must satisfy (256 linear, 4 KB or 64 KB tiled).</summary>
    public uint BaseAlignBytes { get; }
    /// <summary>Block width in elements.</summary>
    public uint BlockWidth { get; }
    /// <summary>Block height in elements.</summary>
    public uint BlockHeight { get; }
    /// <summary>Block depth in elements.</summary>
    public uint BlockDepth { get; }
    /// <summary>Number of 2D block rasters (depth for a volume, else slice count).</summary>
    public uint NumBlockSlices { get; }
    /// <summary>First mip level packed into the mip tail (equals the mip count when there is no tail).</summary>
    public uint FirstMipLevelInTail { get; }
    /// <summary>Per-mip layout, one entry per level. The top mip's padded width is the surface's pitch in elements.</summary>
    public AgcMipInfo[] Mips { get; }
}

/// <summary>
/// Computes the memory layout of a surface - its size, alignment, block dimensions, and per-mip
/// offsets - for any tile mode, the way the graphics address library computes it. This is the layout a
/// render target, depth target, or texture must be allocated and described with. The tiled layouts use
/// block dimensions and mip-tail placement tables; none of it needs the hardware address swizzle
/// equations, which are only used to place pixel bytes into tiled order on the processor.
/// </summary>
public static partial class AgcSurface
{
    /// <summary>Computes the layout of the surface described by <paramref name="desc"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException">A dimension or count is out of range, or the element size is not a power of two in [1, 16].</exception>
    public static AgcSurfaceLayout Compute(in AgcSurfaceDescription desc)
    {
        if (desc.Width == 0)
            throw new ArgumentOutOfRangeException(nameof(desc), "Width must be positive.");
        if (desc.Height == 0)
            throw new ArgumentOutOfRangeException(nameof(desc), "Height must be positive.");
        if (desc.Depth == 0 || desc.NumMips == 0 || desc.NumSlices == 0)
            throw new ArgumentOutOfRangeException(nameof(desc), "Depth, mip count, and slice count must be positive.");
        if (desc.NumMips > 16)
            throw new ArgumentOutOfRangeException(nameof(desc), "At most 16 mip levels.");
        if (desc.TexelsPerElementWide == 0 || desc.TexelsPerElementTall == 0 || desc.MultiElementMultiplier == 0)
            throw new ArgumentOutOfRangeException(nameof(desc), "Texels-per-element and multiplier must be positive.");

        uint bpeLog2 = Log2PowerOfTwo(desc.BytesPerElement, 4, nameof(desc.BytesPerElement));
        uint fragLog2 = Log2PowerOfTwo(desc.NumFragments, 3, nameof(desc.NumFragments));

        uint swizzle = (uint)desc.TileMode;
        Block block = GetBlockDimensions(desc, bpeLog2, fragLog2);
        uint blockSizeLog2 = 8u + (Is4Kb(swizzle) ? 4u : 0u) + (Is64Kb(swizzle) ? 8u : 0u);
        uint baseAlign = 1u << (int)blockSizeLog2;
        uint blockSizeInBytes = baseAlign;
        uint blockWidth = 1u << block.WidthLog2;
        uint blockHeight = 1u << block.HeightLog2;
        uint blockDepth = 1u << block.DepthLog2;
        uint numBlockSlices = desc.Dimension == AgcSurfaceDimension.ThreeD
            ? ShiftCeil(desc.Depth, (uint)block.DepthLog2)
            : desc.NumSlices;

        var mips = new AgcMipInfo[desc.NumMips];
        ulong total;
        uint firstMipLevelInTail = desc.NumMips;

        if (desc.TileMode == AgcTileMode.Linear)
            total = ComputeLinear(desc, bpeLog2, fragLog2, blockWidth, numBlockSlices, mips);
        else if (Is256B(swizzle))
            total = ComputeMicro(desc, bpeLog2, blockWidth, blockHeight, numBlockSlices, mips);
        else
            total = ComputeMacro(desc, bpeLog2, fragLog2, swizzle, blockWidth, blockHeight, blockDepth, blockSizeInBytes, numBlockSlices, mips, ref firstMipLevelInTail);

        return new AgcSurfaceLayout(total, baseAlign, blockWidth, blockHeight, blockDepth, numBlockSlices, firstMipLevelInTail, mips);
    }

    private static Block GetBlockDimensions(in AgcSurfaceDescription desc, uint bpeLog2, uint fragLog2)
    {
        if (desc.TileMode == AgcTileMode.Linear)
            return BlockLinear[bpeLog2];
        if (fragLog2 != 0)
            return BlockMsaa[fragLog2][bpeLog2];
        uint sw = (uint)desc.TileMode;
        bool thick = IsThick(desc.Dimension, sw);
        if (Is4Kb(sw))
            return thick ? BlockThick4KB[bpeLog2] : BlockThin4KB[bpeLog2];
        if (Is64Kb(sw))
            return thick ? BlockThick64KB[bpeLog2] : BlockThin64KB[bpeLog2];
        return thick ? BlockThick256B[bpeLog2] : BlockThin256B[bpeLog2];
    }

    private static ulong ComputeLinear(in AgcSurfaceDescription desc, uint bpeLog2, uint fragLog2, uint blockWidth, uint numBlockSlices, AgcMipInfo[] mips)
    {
        uint widthInElements = WidthInElements(desc);
        uint heightInElements = HeightInElements(desc);
        int shift = (int)(bpeLog2 + fragLog2);

        ulong blockSlice = 0;
        var sizes = new ulong[desc.NumMips];
        var pw = new uint[desc.NumMips];
        for (uint mip = 0; mip < desc.NumMips; mip++)
        {
            uint wCeil = Max1(ShiftCeil(widthInElements, mip));
            uint hCeil = Max1(ShiftCeil(heightInElements, mip));
            uint paddedWidth = PowTwoAlign(wCeil, blockWidth);
            ulong size = (ulong)hCeil * paddedWidth << shift;
            sizes[mip] = size;
            pw[mip] = paddedWidth;
            blockSlice += size;
        }
        ulong offset = 0;
        for (int mip = (int)desc.NumMips - 1; mip >= 0; mip--)
        {
            mips[mip] = new AgcMipInfo(offset, sizes[mip], pw[mip], Max1(ShiftCeil(heightInElements, (uint)mip)), 1);
            offset += sizes[mip];
        }
        return blockSlice * numBlockSlices;
    }

    private static ulong ComputeMicro(in AgcSurfaceDescription desc, uint bpeLog2, uint blockWidth, uint blockHeight, uint numBlockSlices, AgcMipInfo[] mips)
    {
        uint widthInElements = WidthInElements(desc);
        uint heightInElements = HeightInElements(desc);
        ulong blockSlice = 0;
        for (int mip = (int)desc.NumMips - 1; mip >= 0; mip--)
        {
            uint wCeil = Max1(ShiftCeil(widthInElements, (uint)mip));
            uint hCeil = Max1(ShiftCeil(heightInElements, (uint)mip));
            uint paddedWidth = PowTwoAlign(wCeil, blockWidth);
            uint paddedHeight = PowTwoAlign(hCeil, blockHeight);
            ulong size = (ulong)paddedWidth * paddedHeight << (int)bpeLog2;
            mips[mip] = new AgcMipInfo(blockSlice, size, paddedWidth, paddedHeight, 1);
            blockSlice += size;
        }
        return blockSlice * numBlockSlices;
    }

    private static ulong ComputeMacro(in AgcSurfaceDescription desc, uint bpeLog2, uint fragLog2, uint swizzle, uint blockWidth, uint blockHeight, uint blockDepth, uint blockSizeInBytes, uint numBlockSlices, AgcMipInfo[] mips, ref uint firstMipLevelInTail)
    {
        uint widthInElements = WidthInElements(desc);
        uint heightInElements = HeightInElements(desc);
        uint depthInElements = desc.Depth;
        int shift = (int)(bpeLog2 + fragLog2);
        uint numMips = desc.NumMips;

        if (numMips == 1)
        {
            var (pw, ph, pd, size) = ComputeMip(widthInElements, heightInElements, depthInElements, blockWidth, blockHeight, blockDepth, shift);
            mips[0] = new AgcMipInfo(0, size, pw, ph, pd);
            firstMipLevelInTail = 1;
            return size * numBlockSlices;
        }

        bool thick = IsThick(desc.Dimension, swizzle);
        bool block64Kb = Is64Kb(swizzle);

        uint tailWidthLimit = blockWidth;
        uint tailHeightLimit = blockHeight;
        if (IsZ(swizzle) && bpeLog2 < 2)
        {
            var tailDesc = new AgcSurfaceDescription(desc.TileMode, desc.Dimension, desc.Width, desc.Height, 4,
                desc.Depth, desc.NumMips, desc.NumSlices, desc.NumFragments, desc.TexelsPerElementWide, desc.TexelsPerElementTall, desc.MultiElementMultiplier);
            Block tailBlock = GetBlockDimensions(tailDesc, 2, fragLog2);
            tailWidthLimit = 1u << tailBlock.WidthLog2;
            tailHeightLimit = 1u << tailBlock.HeightLog2;
        }
        if (thick && !block64Kb)
            tailHeightLimit >>= 1;
        else
            tailWidthLimit >>= 1;

        (uint X, uint Y)[] tailLocations = thick
            ? (block64Kb ? MipTailThick64KB[bpeLog2] : MipTailThick4KB[bpeLog2])
            : (block64Kb ? MipTailThin64KB[bpeLog2] : MipTailThin4KB[bpeLog2]);
        uint maxTailLength = (uint)tailLocations.Length;

        ulong blockSlice = 0;
        ulong tailSize = 0;
        uint first = numMips;
        var sizes = new ulong[numMips];
        var pws = new uint[numMips];
        var phs = new uint[numMips];
        var pds = new uint[numMips];
        for (uint mip = 0; mip < numMips; mip++)
        {
            uint wCeil = Max1(ShiftCeil(widthInElements, mip));
            uint hCeil = Max1(ShiftCeil(heightInElements, mip));
            if (wCeil <= tailWidthLimit && hCeil <= tailHeightLimit)
            {
                uint potentialTailLength = numMips - mip;
                if (potentialTailLength <= maxTailLength)
                {
                    first = mip;
                    tailSize = blockSizeInBytes;
                    blockSlice += blockSizeInBytes;
                    break;
                }
            }
            uint dCeil = Max1(ShiftCeil(depthInElements, mip));
            var (pw, ph, pd, size) = ComputeMip(wCeil, hCeil, dCeil, blockWidth, blockHeight, blockDepth, shift);
            sizes[mip] = size;
            pws[mip] = pw;
            phs[mip] = ph;
            pds[mip] = pd;
            blockSlice += size;
        }

        firstMipLevelInTail = first;
        ulong offset = tailSize;
        for (int mip = (int)first - 1; mip >= 0; mip--)
        {
            mips[mip] = new AgcMipInfo(offset, sizes[mip], pws[mip], phs[mip], pds[mip]);
            offset += sizes[mip];
        }
        for (uint mip = first; mip < numMips; mip++)
        {
            (uint X, uint Y) loc = tailLocations[mip - first];
            mips[mip] = new AgcMipInfo(0, blockSizeInBytes, blockWidth, blockHeight, blockDepth);
            _ = loc; // mip-tail coordinates are part of the per-mip data but not surfaced here
        }

        return blockSlice * numBlockSlices;
    }

    private static (uint PaddedWidth, uint PaddedHeight, uint PaddedDepth, ulong Size) ComputeMip(
        uint wCeil, uint hCeil, uint dCeil, uint blockWidth, uint blockHeight, uint blockDepth, int shift)
    {
        uint paddedWidth = PowTwoAlign(wCeil, blockWidth);
        uint paddedHeight = PowTwoAlign(hCeil, blockHeight);
        uint paddedDepth = PowTwoAlign(dCeil, blockDepth);
        ulong size = (ulong)blockDepth * paddedHeight * paddedWidth << shift;
        return (paddedWidth, paddedHeight, paddedDepth, size);
    }

    private static uint WidthInElements(in AgcSurfaceDescription desc)
        => (desc.Width * desc.MultiElementMultiplier + desc.TexelsPerElementWide - 1) / desc.TexelsPerElementWide;

    private static uint HeightInElements(in AgcSurfaceDescription desc)
        => (desc.Height + desc.TexelsPerElementTall - 1) / desc.TexelsPerElementTall;

    private static bool IsThick(AgcSurfaceDimension dim, uint sw)
        => dim == AgcSurfaceDimension.ThreeD && (IsStd(sw) || IsDisp(sw));

    private static bool Is256B(uint sw) => (SwizzleFlags[sw] & 0x01) != 0;
    private static bool Is4Kb(uint sw) => (SwizzleFlags[sw] & 0x02) != 0;
    private static bool Is64Kb(uint sw) => (SwizzleFlags[sw] & 0x04) != 0;
    private static bool IsZ(uint sw) => (SwizzleFlags[sw] & 0x08) != 0;
    private static bool IsStd(uint sw) => (SwizzleFlags[sw] & 0x10) != 0;
    private static bool IsDisp(uint sw) => (SwizzleFlags[sw] & 0x20) != 0;

    private static uint PowTwoAlign(uint x, uint align) => (x + align - 1) & ~(align - 1);
    private static uint ShiftCeil(uint a, uint b) => (a + (1u << (int)b) - 1) >> (int)b;
    private static uint Max1(uint x) => x < 1 ? 1 : x;

    private static uint Log2PowerOfTwo(uint value, uint maxLog, string name)
    {
        if (value == 0 || (value & (value - 1)) != 0)
            throw new ArgumentOutOfRangeException(name, "Must be a power of two.");
        uint log = 0;
        while ((value >>= 1) != 0)
            log++;
        if (log > maxLog)
            throw new ArgumentOutOfRangeException(name, "Value is too large.");
        return log;
    }
}
