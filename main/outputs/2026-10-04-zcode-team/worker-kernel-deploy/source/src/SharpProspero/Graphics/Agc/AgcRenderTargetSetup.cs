// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>What a render target is: its pixel format, extent, tiling, and data address.</summary>
/// <remarks>
/// Format, channel type, and channel order are the render-target register enums. Use
/// <see cref="AgcRenderTargetSetup.Initialize"/> to encode a description into a register block; that mirrors
/// the graphics core's own render-target setup, including the blend and rounding rules it derives from the
/// channel type. This describes a plain, uncompressed color target - the common scan-out case.
/// </remarks>
/// <remarks>Creates a description of a 2D color render target.</remarks>
public readonly struct RenderTargetSpec(
    CxRenderTarget.Format format,
    CxRenderTarget.ChannelType channelType,
    CxRenderTarget.ChannelOrder channelOrder,
    uint width,
    uint height,
    ulong dataAddress,
    CxRenderTarget.TileMode tileMode = CxRenderTarget.TileMode.kRenderTarget,
    CxRenderTarget.Dimension dimension = CxRenderTarget.Dimension.k2d,
    uint depth = 1,
    uint numMips = 1,
    uint numSlices = 1,
    CxRenderTarget.NumSamples numSamples = CxRenderTarget.NumSamples.k1,
    CxRenderTarget.NumFragments numFragments = CxRenderTarget.NumFragments.k1)
{

    /// <summary>The pixel format.</summary>
    public CxRenderTarget.Format Format { get; } = format;
    /// <summary>How each channel's bits are interpreted.</summary>
    public CxRenderTarget.ChannelType ChannelType { get; } = channelType;
    /// <summary>How shader export channels map to format components.</summary>
    public CxRenderTarget.ChannelOrder ChannelOrder { get; } = channelOrder;
    /// <summary>Width in pixels.</summary>
    public uint Width { get; } = width;
    /// <summary>Height in pixels.</summary>
    public uint Height { get; } = height;
    /// <summary>Base address of the color data (a multiple of 256).</summary>
    public ulong DataAddress { get; } = dataAddress;
    /// <summary>The memory layout.</summary>
    public CxRenderTarget.TileMode TileMode { get; } = tileMode;
    /// <summary>The dimensionality.</summary>
    public CxRenderTarget.Dimension Dimension { get; } = dimension;
    /// <summary>Depth in pixels for a 3D target.</summary>
    public uint Depth { get; } = depth;
    /// <summary>Number of mip levels.</summary>
    public uint NumMips { get; } = numMips;
    /// <summary>Number of array slices.</summary>
    public uint NumSlices { get; } = numSlices;
    /// <summary>Number of samples.</summary>
    public CxRenderTarget.NumSamples NumSamples { get; } = numSamples;
    /// <summary>Number of fragments.</summary>
    public CxRenderTarget.NumFragments NumFragments { get; } = numFragments;
}

/// <summary>Encodes a <see cref="RenderTargetSpec"/> into a <see cref="CxRenderTarget"/> register block.</summary>
public static class AgcRenderTargetSetup
{
    /// <summary>
    /// Applies the full render-target setup sequence to <paramref name="rt"/>: the blend, clamp, and rounding
    /// modes derived from the channel type, then every field of the description. This is the graphics core's
    /// own render-target initialization, for an uncompressed color target. The block must already have been
    /// initialized from the driver defaults with <see cref="CxRenderTarget.Init"/>.
    /// </summary>
    public static CxRenderTarget Initialize(CxRenderTarget rt, in RenderTargetSpec spec)
    {
        ArgumentNullException.ThrowIfNull(rt);

        rt.SetNumMipLevels(spec.NumMips);
        if (spec.Dimension == CxRenderTarget.Dimension.k3d)
        {
            rt.SetLastArraySliceIndex(spec.Depth - 1);
            rt.SetBaseArraySliceIndex(0);
        }
        else
        {
            rt.SetLastArraySliceIndex(spec.NumSlices - 1);
        }

        bool wantsHalfRound =
            spec.ChannelType is CxRenderTarget.ChannelType.kUNorm or CxRenderTarget.ChannelType.kSNorm or CxRenderTarget.ChannelType.kSrgb;
        bool blendBypass =
            spec.ChannelType is CxRenderTarget.ChannelType.kUInt or CxRenderTarget.ChannelType.kSInt;
        bool blendClamp =
            spec.ChannelType is CxRenderTarget.ChannelType.kUNorm or CxRenderTarget.ChannelType.kSNorm or CxRenderTarget.ChannelType.kSrgb;
        if (blendBypass)
            blendClamp = false;

        rt.SetFormat(spec.Format);
        rt.SetChannelType(spec.ChannelType);
        rt.SetChannelOrder(spec.ChannelOrder);
        rt.SetBlendBypass(blendBypass ? CxRenderTarget.BlendBypass.kEnable : CxRenderTarget.BlendBypass.kDisable);
        rt.SetBlendClamp(blendClamp ? CxRenderTarget.BlendClamp.kEnable : CxRenderTarget.BlendClamp.kDisable);
        rt.SetRoundMode(wantsHalfRound ? CxRenderTarget.RoundMode.kRoundByHalf : CxRenderTarget.RoundMode.kTruncate);
        rt.SetNumSamples(spec.NumSamples);
        rt.SetNumFragments(spec.NumFragments);
        rt.SetWidth(spec.Width);
        rt.SetHeight(spec.Height);
        rt.SetDepth(spec.Depth);
        rt.SetTileMode(spec.TileMode);
        rt.SetDimension(spec.Dimension);
        rt.SetMetadataPipeAlignment(CxRenderTarget.MetadataPipeAlignment.kEnable);

        rt.SetDataAddress(spec.DataAddress);
        rt.SetCmaskAddress(0);
        rt.SetFmaskAddress(0);
        rt.SetDccAddress(0);

        // No compression: disable DCC and FMask, matching the core's kNone metadata-compression path.
        rt.SetDccMaxUncompressedBlockSize(CxRenderTarget.DccMaxUncompressedBlockSize.k256B);
        rt.SetDccMaxCompressedBlockSize(CxRenderTarget.DccMaxCompressedBlockSize.k256B);
        rt.SetDccForceIndependentBlocks(CxRenderTarget.DccForceIndependentBlocks.kDisable);
        rt.SetDataWriteOnDccClearToRegister(CxRenderTarget.DataWriteOnDccClearToRegister.kDisable);
        rt.SetDccCompression(CxRenderTarget.DccCompression.kDisable);
        rt.SetFmaskCompression(CxRenderTarget.FmaskCompression.kDisable);
        rt.SetCompressFmaskData(CxRenderTarget.CompressFmaskData.kEnable);

        return rt;
    }
}
