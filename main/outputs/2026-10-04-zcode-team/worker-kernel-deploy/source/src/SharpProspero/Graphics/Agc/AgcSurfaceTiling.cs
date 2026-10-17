// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Graphics.Agc;

// The per-mip fields the address computation needs, beyond the size/alignment that AgcSurfaceLayout
// surfaces: the byte offset of each mip within a slice, and - for mips packed into the mip tail - the
// element coordinate of the mip within its tail block.
internal readonly struct AgcTilingMip(uint width, uint height, uint depth, uint paddedWidth, ulong offsetInBytes, uint mipTailCoordX, uint mipTailCoordY)
{
    public uint Width { get; } = width;
    public uint Height { get; } = height;
    public uint Depth { get; } = depth;
    public uint PaddedWidth { get; } = paddedWidth;
    public ulong OffsetInBytes { get; } = offsetInBytes;
    public uint MipTailCoordX { get; } = mipTailCoordX;
    public uint MipTailCoordY { get; } = mipTailCoordY;
}

// A surface summary with the block and per-mip detail the address swizzle needs. This mirrors the
// graphics address library's SurfaceSummary; AgcSurfaceLayout is the public, size-oriented view of the
// same computation, so this keeps the extra fields internal to the tiler.
internal readonly struct AgcTilingSummary(
    AgcTileMode tileMode, uint bpeLog2, uint numColorFragmentsLog2, uint numDepthFragmentsLog2,
    uint blockSizeInBytesLog2, uint blockWidthLog2, uint blockHeightLog2, uint blockDepthLog2,
    uint numBlockSlices, ulong blockSliceSizeInBytes, ulong totalSizeInBytes,
    bool isMicro, AgcTilingMip[] mips)
{
    public AgcTileMode TileMode { get; } = tileMode;
    public uint BpeLog2 { get; } = bpeLog2;
    public uint NumColorFragmentsLog2 { get; } = numColorFragmentsLog2;
    public uint NumDepthFragmentsLog2 { get; } = numDepthFragmentsLog2;
    public uint BlockSizeInBytesLog2 { get; } = blockSizeInBytesLog2;
    public uint BlockWidthLog2 { get; } = blockWidthLog2;
    public uint BlockHeightLog2 { get; } = blockHeightLog2;
    public uint BlockDepthLog2 { get; } = blockDepthLog2;
    public uint NumBlockSlices { get; } = numBlockSlices;
    public ulong BlockSliceSizeInBytes { get; } = blockSliceSizeInBytes;
    public ulong TotalSizeInBytes { get; } = totalSizeInBytes;
    public bool IsMicro { get; } = isMicro;
    public AgcTilingMip[] Mips { get; } = mips;
}

public static partial class AgcSurface
{
    // Computes the address-oriented summary of a surface: the same layout AgcSurfaceLayout describes,
    // plus the per-mip offsets and mip-tail coordinates the swizzle address computation reads.
    internal static AgcTilingSummary ComputeTilingSummary(in AgcSurfaceDescription desc)
    {
        uint bpeLog2 = Log2PowerOfTwo(desc.BytesPerElement, 4, nameof(desc.BytesPerElement));
        uint fragLog2 = Log2PowerOfTwo(desc.NumFragments, 3, nameof(desc.NumFragments));
        uint swizzle = (uint)desc.TileMode;

        Block block = GetBlockDimensions(desc, bpeLog2, fragLog2);
        uint blockSizeLog2 = 8u + (Is4Kb(swizzle) ? 4u : 0u) + (Is64Kb(swizzle) ? 8u : 0u);
        uint blockWidth = 1u << block.WidthLog2;
        uint blockHeight = 1u << block.HeightLog2;
        uint blockDepth = 1u << block.DepthLog2;
        uint blockSizeInBytes = 1u << (int)blockSizeLog2;

        uint numColorFragmentsLog2 = desc.TileMode == AgcTileMode.RenderTarget ? fragLog2 : 0;
        uint numDepthFragmentsLog2 = desc.TileMode == AgcTileMode.Depth ? fragLog2 : 0;
        uint numBlockSlices = desc.Dimension == AgcSurfaceDimension.ThreeD
            ? ShiftCeil(desc.Depth, (uint)block.DepthLog2)
            : desc.NumSlices;

        var mips = new AgcTilingMip[desc.NumMips];
        ulong blockSlice;
        bool isMicro = desc.TileMode != AgcTileMode.Linear && Is256B(swizzle);

        if (desc.TileMode == AgcTileMode.Linear)
            blockSlice = TilingSummaryLinear(desc, bpeLog2, fragLog2, blockWidth, mips);
        else if (isMicro)
            blockSlice = TilingSummaryMicro(desc, bpeLog2, blockWidth, blockHeight, mips);
        else
            blockSlice = TilingSummaryMacro(desc, bpeLog2, fragLog2, swizzle, blockWidth, blockHeight, blockDepth, blockSizeInBytes, mips);

        ulong total = blockSlice * numBlockSlices;
        return new AgcTilingSummary(desc.TileMode, bpeLog2, numColorFragmentsLog2, numDepthFragmentsLog2,
            blockSizeLog2, (uint)block.WidthLog2, (uint)block.HeightLog2, (uint)block.DepthLog2,
            numBlockSlices, blockSlice, total, isMicro, mips);
    }

    private static uint WidthInElementsForMip(in AgcSurfaceDescription desc, uint mip)
        => ((desc.Width >> (int)mip) * desc.MultiElementMultiplier + desc.TexelsPerElementWide - 1) / desc.TexelsPerElementWide;

    private static uint HeightInElementsForMip(in AgcSurfaceDescription desc, uint mip)
        => ((desc.Height >> (int)mip) + desc.TexelsPerElementTall - 1) / desc.TexelsPerElementTall;

    private static ulong TilingSummaryLinear(in AgcSurfaceDescription desc, uint bpeLog2, uint fragLog2, uint blockWidth, AgcTilingMip[] mips)
    {
        uint widthInElements = WidthInElements(desc);
        uint heightInElements = HeightInElements(desc);
        uint depthInElements = desc.Depth;
        int shift = (int)(bpeLog2 + fragLog2);
        uint numMips = desc.NumMips;

        var sizes = new ulong[numMips];
        var recs = new AgcTilingMip[numMips];
        ulong blockSlice = 0;
        for (uint mip = 0; mip < numMips; mip++)
        {
            uint mipWidthInElements = WidthInElementsForMip(desc, mip);
            uint mipWidth = Max1(mipWidthInElements / desc.MultiElementMultiplier * desc.MultiElementMultiplier);
            uint mipHeight = Max1(HeightInElementsForMip(desc, mip));
            uint mipDepth = Max1(depthInElements >> (int)mip);
            uint wCeil = Max1(ShiftCeil(widthInElements, mip));
            uint hCeil = Max1(ShiftCeil(heightInElements, mip));
            uint paddedWidth = PowTwoAlign(wCeil, blockWidth);
            ulong size = (ulong)hCeil * paddedWidth << shift;
            sizes[mip] = size;
            recs[mip] = new AgcTilingMip(mipWidth, mipHeight, mipDepth, paddedWidth, 0, 0, 0);
            blockSlice += size;
        }
        ulong offset = 0;
        for (int mip = (int)numMips - 1; mip >= 0; mip--)
        {
            var r = recs[mip];
            mips[mip] = new AgcTilingMip(r.Width, r.Height, r.Depth, r.PaddedWidth, offset, 0, 0);
            offset += sizes[mip];
        }
        return blockSlice;
    }

    private static ulong TilingSummaryMicro(in AgcSurfaceDescription desc, uint bpeLog2, uint blockWidth, uint blockHeight, AgcTilingMip[] mips)
    {
        uint widthInElements = WidthInElements(desc);
        uint heightInElements = HeightInElements(desc);
        ulong blockSlice = 0;
        for (int mip = (int)desc.NumMips - 1; mip >= 0; mip--)
        {
            uint mipWidthInElements = WidthInElementsForMip(desc, (uint)mip);
            uint mipWidth = Max1(mipWidthInElements / desc.MultiElementMultiplier * desc.MultiElementMultiplier);
            uint mipHeight = Max1(HeightInElementsForMip(desc, (uint)mip));
            uint wCeil = Max1(ShiftCeil(widthInElements, (uint)mip));
            uint hCeil = Max1(ShiftCeil(heightInElements, (uint)mip));
            uint paddedWidth = PowTwoAlign(wCeil, blockWidth);
            uint paddedHeight = PowTwoAlign(hCeil, blockHeight);
            ulong size = (ulong)paddedWidth * paddedHeight << (int)bpeLog2;
            mips[mip] = new AgcTilingMip(mipWidth, mipHeight, 1, paddedWidth, blockSlice, 0, 0);
            blockSlice += size;
        }
        return blockSlice;
    }

    private static ulong TilingSummaryMacro(in AgcSurfaceDescription desc, uint bpeLog2, uint fragLog2, uint swizzle,
        uint blockWidth, uint blockHeight, uint blockDepth, uint blockSizeInBytes, AgcTilingMip[] mips)
    {
        uint widthInElements = WidthInElements(desc);
        uint heightInElements = HeightInElements(desc);
        uint depthInElements = desc.Depth;
        int shift = (int)(bpeLog2 + fragLog2);
        uint numMips = desc.NumMips;

        if (numMips == 1)
        {
            uint paddedWidth = PowTwoAlign(widthInElements, blockWidth);
            uint paddedHeight = PowTwoAlign(heightInElements, blockHeight);
            ulong size = (ulong)blockDepth * paddedHeight * paddedWidth << shift;
            mips[0] = new AgcTilingMip(widthInElements, heightInElements, depthInElements, paddedWidth, 0, 0, 0);
            return size;
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

        var sizes = new ulong[numMips];
        var recs = new AgcTilingMip[numMips];
        ulong blockSlice = 0;
        ulong tailSize = 0;
        uint first = numMips;
        for (uint mip = 0; mip < numMips; mip++)
        {
            uint wCeil = Max1(ShiftCeil(widthInElements, mip));
            uint hCeil = Max1(ShiftCeil(heightInElements, mip));
            if (wCeil <= tailWidthLimit && hCeil <= tailHeightLimit && numMips - mip <= maxTailLength)
            {
                first = mip;
                tailSize = blockSizeInBytes;
                blockSlice += blockSizeInBytes;
                break;
            }
            uint dCeil = Max1(ShiftCeil(depthInElements, mip));
            uint mipWidthInElements = WidthInElementsForMip(desc, mip);
            uint mipWidth = Max1(mipWidthInElements / desc.MultiElementMultiplier * desc.MultiElementMultiplier);
            uint mipHeight = Max1(HeightInElementsForMip(desc, mip));
            uint mipDepth = Max1(depthInElements >> (int)mip);
            uint paddedWidth = PowTwoAlign(wCeil, blockWidth);
            uint paddedHeight = PowTwoAlign(hCeil, blockHeight);
            ulong size = (ulong)blockDepth * paddedHeight * paddedWidth << shift;
            sizes[mip] = size;
            recs[mip] = new AgcTilingMip(mipWidth, mipHeight, mipDepth, paddedWidth, 0, 0, 0);
            blockSlice += size;
            _ = PowTwoAlign(dCeil, blockDepth);
        }

        ulong offset = tailSize;
        for (int mip = (int)first - 1; mip >= 0; mip--)
        {
            var r = recs[mip];
            mips[mip] = new AgcTilingMip(r.Width, r.Height, r.Depth, r.PaddedWidth, offset, 0, 0);
            offset += sizes[mip];
        }
        for (uint mip = first; mip < numMips; mip++)
        {
            (uint X, uint Y) loc = tailLocations[mip - first];
            uint mipWidth = Max1(WidthInElementsForMip(desc, mip));
            uint mipHeight = Max1(HeightInElementsForMip(desc, mip));
            uint mipDepth = Max1(depthInElements >> (int)mip);
            mips[mip] = new AgcTilingMip(mipWidth, mipHeight, mipDepth, blockWidth, 0, loc.X, loc.Y);
        }
        return blockSlice;
    }
}
