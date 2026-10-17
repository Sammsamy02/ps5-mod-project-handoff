// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>One hardware register in a context register block: the register offset the GPU writes and its 32-bit value.</summary>
/// <remarks>Creates a register with an offset and value.</remarks>
public struct CxRegister(ushort offset, uint value)
{
    /// <summary>The register offset (which GPU register this value is written to).</summary>
    public ushort Offset = offset;
    /// <summary>The 32-bit register value.</summary>
    public uint Value = value;
}

/// <summary>
/// The color render-target context register block: sixteen GPU registers describing a render target's
/// format, dimensions, tiling, addresses, and blend and compression behaviour. The typed setters encode
/// each field into the right register bits, exactly as the graphics context register definitions do, so
/// the block can be written straight into a command buffer.
/// </summary>
/// <remarks>
/// The register offsets and reset values (<see cref="Init"/>) come from the graphics driver at runtime
/// through <c>sceAgcGetRegisterDefaults</c>; they depend on the graphics processor configuration and are
/// not baked in. Construct the block, <see cref="Init"/> it from those defaults, then apply the setters -
/// or let <see cref="AgcRenderTargetSetup.Initialize"/> apply the full sequence from a description.
/// </remarks>
public sealed class CxRenderTarget
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 16;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];
    private readonly ushort[] _defaultOffsets = new ushort[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index (0..15).</summary>
    public CxRegister this[int index]
    {
        get => _regs[index];
        set => _regs[index] = value;
    }

    /// <summary>
    /// Loads the register offsets and reset values from the driver-provided defaults (sixteen registers),
    /// the way the block's own initializer does. Call this before applying setters.
    /// </summary>
    /// <exception cref="ArgumentException">Fewer than sixteen defaults were provided.</exception>
    public CxRenderTarget Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++)
        {
            _regs[i] = defaults[i];
            _defaultOffsets[i] = defaults[i].Offset;
        }
        return this;
    }

    /// <summary>
    /// Selects which of the eight hardware render-target slots this block targets, by shifting the register
    /// offsets. Requires <see cref="Init"/> to have loaded the default offsets first.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">The slot is not in [0, 7].</exception>
    public CxRenderTarget SetSlot(uint slot)
    {
        if (slot >= 8)
            throw new ArgumentOutOfRangeException(nameof(slot), "Slot must be in [0, 7].");
        for (int i = 0; i < RegisterCount; i++)
            _regs[i].Offset = (ushort)(_defaultOffsets[i] + slot * (i < 10 ? 15u : 1u));
        return this;
    }

    /// <summary>Returns the slot selected by <see cref="SetSlot"/>.</summary>
    public uint GetSlot() => (uint)((_regs[0].Offset - _defaultOffsets[0]) / 15);

    /// <summary>Format field values for a render target.</summary>
    public enum Format : uint
    {
        kInvalid = 0x00000000,
        k8 = 0x00000004,
        k16 = 0x00000008,
        k8_8 = 0x0000000c,
        k32 = 0x00000010,
        k16_16 = 0x00000014,
        k11_11_10 = 0x00000018,
        k10_11_11 = 0x0000001c,
        k2_10_10_10NonFloat = 0x00000020,
        k10_10_10_2NonFloat = 0x00000024,
        k8_8_8_8 = 0x00000028,
        k32_32 = 0x0000002c,
        k16_16_16_16 = 0x00000030,
        k32_32_32_32 = 0x00000038,
        k5_6_5 = 0x00000040,
        k5_5_5_1 = 0x00000044,
        k1_5_5_5 = 0x00000048,
        k4_4_4_4 = 0x0000004c,
        k10_10_10_2Float = 0x0000007c,
        kBitMask = 0x0000007c,
    }

    /// <summary>ChannelType field values for a render target.</summary>
    public enum ChannelType : uint
    {
        kUNorm = 0x00000000,
        kSNorm = 0x00000100,
        kUInt = 0x00000400,
        kSInt = 0x00000500,
        kSrgb = 0x00000600,
        kFloat = 0x00000700,
        kBitMask = 0x00000700,
    }

    /// <summary>ChannelOrder field values for a render target.</summary>
    public enum ChannelOrder : uint
    {
        kStandard = 0x00000000,
        kAlt = 0x00000800,
        kReversed = 0x00001000,
        kAltReversed = 0x00001800,
        kBitMask = 0x00001800,
    }

    /// <summary>CmaskFastClear field values for a render target.</summary>
    public enum CmaskFastClear : uint
    {
        kEnable = 0x00002000,
        kDisable = 0x00000000,
        kBitMask = 0x00002000,
    }

    /// <summary>DccCompression field values for a render target.</summary>
    public enum DccCompression : uint
    {
        kEnable = 0x10000000,
        kDisable = 0x00000000,
        kBitMask = 0x10000000,
    }

    /// <summary>BlendBypass field values for a render target.</summary>
    public enum BlendBypass : uint
    {
        kEnable = 0x00010000,
        kDisable = 0x00000000,
        kBitMask = 0x00010000,
    }

    /// <summary>BlendClamp field values for a render target.</summary>
    public enum BlendClamp : uint
    {
        kEnable = 0x00008000,
        kDisable = 0x00000000,
        kBitMask = 0x00008000,
    }

    /// <summary>RoundMode field values for a render target.</summary>
    public enum RoundMode : uint
    {
        kRoundByHalf = 0x00000000,
        kTruncate = 0x00040000,
        kBitMask = 0x00040000,
    }

    /// <summary>FmaskCompression field values for a render target.</summary>
    public enum FmaskCompression : uint
    {
        kEnable = 0x00004000,
        kDisable = 0x00000000,
        kBitMask = 0x00004000,
    }

    /// <summary>FmaskOneFragMode field values for a render target.</summary>
    public enum FmaskOneFragMode : uint
    {
        kEnable = 0x08000000,
        kDisable = 0x00000000,
        kBitMask = 0x08000000,
    }

    /// <summary>CompressFmaskData field values for a render target.</summary>
    public enum CompressFmaskData : uint
    {
        kEnable = 0x00000000,
        kDisable = 0x04000000,
        kBitMask = 0x04000000,
    }

    /// <summary>NumSamples field values for a render target.</summary>
    public enum NumSamples : uint
    {
        k1 = 0x00000000,
        k2 = 0x00001000,
        k4 = 0x00002000,
        k8 = 0x00003000,
        k16 = 0x00004000,
        kBitMask = 0x00007000,
    }

    /// <summary>NumFragments field values for a render target.</summary>
    public enum NumFragments : uint
    {
        k1 = 0x00000000,
        k2 = 0x00008000,
        k4 = 0x00010000,
        k8 = 0x00018000,
        kBitMask = 0x00018000,
    }

    /// <summary>ForceDestAlphaToOne field values for a render target.</summary>
    public enum ForceDestAlphaToOne : uint
    {
        kEnable = 0x00020000,
        kDisable = 0x00000000,
        kBitMask = 0x00020000,
    }

    /// <summary>OverwriteCombiner field values for a render target.</summary>
    public enum OverwriteCombiner : uint
    {
        kEnable = 0x00000000,
        kDisable = 0x00000001,
        kBitMask = 0x00000001,
    }

    /// <summary>DccClearKey field values for a render target.</summary>
    public enum DccClearKey : uint
    {
        kEnable = 0x00000002,
        kDisable = 0x00000000,
        kBitMask = 0x00000002,
    }

    /// <summary>DccMaxCompressedBlockSize field values for a render target.</summary>
    public enum DccMaxCompressedBlockSize : uint
    {
        k64B = 0x00000000,
        k128B = 0x00000020,
        k256B = 0x00000040,
        kBitMask = 0x00000060,
    }

    /// <summary>DccMaxUncompressedBlockSize field values for a render target.</summary>
    public enum DccMaxUncompressedBlockSize : uint
    {
        k64B = 0x00000000,
        k128B = 0x00000004,
        k256B = 0x00000008,
        kBitMask = 0x0000000c,
    }

    /// <summary>DccColorTransform field values for a render target.</summary>
    public enum DccColorTransform : uint
    {
        kAuto = 0x00000000,
        kNone = 0x00000080,
        kAbgr = 0x00000100,
        kBgra = 0x00000180,
        kBitMask = 0x00000180,
    }

    /// <summary>DccForceIndependentBlocks field values for a render target.</summary>
    public enum DccForceIndependentBlocks : uint
    {
        kDisable = 0x00000000,
        k64B = 0x00000200,
        k128B = 0x00100000,
        kBitMask = 0x00100200,
    }

    /// <summary>DataWriteOnDccClearToRegister field values for a render target.</summary>
    public enum DataWriteOnDccClearToRegister : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00080000,
        kBitMask = 0x00080000,
    }

    /// <summary>TileMode field values for a render target.</summary>
    public enum TileMode : uint
    {
        kLinear = 0x00000000,
        kStandard256B = 0x00004000,
        kStandard4KB = 0x00014000,
        kStandard64KB = 0x00024000,
        kPrt = 0x00044000,
        kDepth = 0x00060000,
        kRenderTarget = 0x0006c000,
        kBitMask = 0x0007c000,
    }

    /// <summary>Dimension field values for a render target.</summary>
    public enum Dimension : uint
    {
        k1d = 0x00000000,
        k2d = 0x01000000,
        k3d = 0x02000000,
        kBitMask = 0x03000000,
    }

    /// <summary>MetadataPipeAlignment field values for a render target.</summary>
    public enum MetadataPipeAlignment : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x44000000,
        kBitMask = 0x44000000,
    }

    /// <summary>Sets the CurrentMipLevel field.</summary>
    public CxRenderTarget SetCurrentMipLevel(uint value) { _regs[1].Value = (_regs[1].Value & 0xc3ffffffu) | (((value << 26) & 0x3c000000u)); return this; }
    /// <summary>Gets the CurrentMipLevel field.</summary>
    public uint GetCurrentMipLevel() => (_regs[1].Value & 0x3c000000u) >> 26;

    /// <summary>Sets the LastArraySliceIndex field.</summary>
    public CxRenderTarget SetLastArraySliceIndex(uint value) { _regs[1].Value = (_regs[1].Value & 0xfc001fffu) | (((value << 13) & 0x03ffe000u)); return this; }
    /// <summary>Gets the LastArraySliceIndex field.</summary>
    public uint GetLastArraySliceIndex() => (_regs[1].Value & 0x03ffe000u) >> 13;

    /// <summary>Sets the BaseArraySliceIndex field.</summary>
    public CxRenderTarget SetBaseArraySliceIndex(uint value) { _regs[1].Value = (_regs[1].Value & 0xffffe000u) | ((value & 0x00001fffu)); return this; }
    /// <summary>Gets the BaseArraySliceIndex field.</summary>
    public uint GetBaseArraySliceIndex() => _regs[1].Value & 0x00001fffu;

    /// <summary>Sets the Format field.</summary>
    public CxRenderTarget SetFormat(Format value) { _regs[2].Value = (_regs[2].Value & ~(uint)Format.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the Format field.</summary>
    public Format GetFormat() => (Format)(_regs[2].Value & (uint)Format.kBitMask);

    /// <summary>Sets the ChannelType field.</summary>
    public CxRenderTarget SetChannelType(ChannelType value) { _regs[2].Value = (_regs[2].Value & ~(uint)ChannelType.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ChannelType field.</summary>
    public ChannelType GetChannelType() => (ChannelType)(_regs[2].Value & (uint)ChannelType.kBitMask);

    /// <summary>Sets the ChannelOrder field.</summary>
    public CxRenderTarget SetChannelOrder(ChannelOrder value) { _regs[2].Value = (_regs[2].Value & ~(uint)ChannelOrder.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ChannelOrder field.</summary>
    public ChannelOrder GetChannelOrder() => (ChannelOrder)(_regs[2].Value & (uint)ChannelOrder.kBitMask);

    /// <summary>Sets the CmaskFastClear field.</summary>
    public CxRenderTarget SetCmaskFastClear(CmaskFastClear value) { _regs[2].Value = (_regs[2].Value & ~(uint)CmaskFastClear.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the CmaskFastClear field.</summary>
    public CmaskFastClear GetCmaskFastClear() => (CmaskFastClear)(_regs[2].Value & (uint)CmaskFastClear.kBitMask);

    /// <summary>Sets the DccCompression field.</summary>
    public CxRenderTarget SetDccCompression(DccCompression value) { _regs[2].Value = (_regs[2].Value & ~(uint)DccCompression.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DccCompression field.</summary>
    public DccCompression GetDccCompression() => (DccCompression)(_regs[2].Value & (uint)DccCompression.kBitMask);

    /// <summary>Sets the BlendBypass field.</summary>
    public CxRenderTarget SetBlendBypass(BlendBypass value) { _regs[2].Value = (_regs[2].Value & ~(uint)BlendBypass.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the BlendBypass field.</summary>
    public BlendBypass GetBlendBypass() => (BlendBypass)(_regs[2].Value & (uint)BlendBypass.kBitMask);

    /// <summary>Sets the BlendClamp field.</summary>
    public CxRenderTarget SetBlendClamp(BlendClamp value) { _regs[2].Value = (_regs[2].Value & ~(uint)BlendClamp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the BlendClamp field.</summary>
    public BlendClamp GetBlendClamp() => (BlendClamp)(_regs[2].Value & (uint)BlendClamp.kBitMask);

    /// <summary>Sets the RoundMode field.</summary>
    public CxRenderTarget SetRoundMode(RoundMode value) { _regs[2].Value = (_regs[2].Value & ~(uint)RoundMode.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the RoundMode field.</summary>
    public RoundMode GetRoundMode() => (RoundMode)(_regs[2].Value & (uint)RoundMode.kBitMask);

    /// <summary>Sets the FmaskCompression field.</summary>
    public CxRenderTarget SetFmaskCompression(FmaskCompression value) { _regs[2].Value = (_regs[2].Value & ~(uint)FmaskCompression.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the FmaskCompression field.</summary>
    public FmaskCompression GetFmaskCompression() => (FmaskCompression)(_regs[2].Value & (uint)FmaskCompression.kBitMask);

    /// <summary>Sets the FmaskOneFragMode field.</summary>
    public CxRenderTarget SetFmaskOneFragMode(FmaskOneFragMode value) { _regs[2].Value = (_regs[2].Value & ~(uint)FmaskOneFragMode.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the FmaskOneFragMode field.</summary>
    public FmaskOneFragMode GetFmaskOneFragMode() => (FmaskOneFragMode)(_regs[2].Value & (uint)FmaskOneFragMode.kBitMask);

    /// <summary>Sets the CompressFmaskData field.</summary>
    public CxRenderTarget SetCompressFmaskData(CompressFmaskData value) { _regs[2].Value = (_regs[2].Value & ~(uint)CompressFmaskData.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the CompressFmaskData field.</summary>
    public CompressFmaskData GetCompressFmaskData() => (CompressFmaskData)(_regs[2].Value & (uint)CompressFmaskData.kBitMask);

    /// <summary>Sets the NumSamples field.</summary>
    public CxRenderTarget SetNumSamples(NumSamples value) { _regs[3].Value = (_regs[3].Value & ~(uint)NumSamples.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the NumSamples field.</summary>
    public NumSamples GetNumSamples() => (NumSamples)(_regs[3].Value & (uint)NumSamples.kBitMask);

    /// <summary>Sets the NumFragments field.</summary>
    public CxRenderTarget SetNumFragments(NumFragments value) { _regs[3].Value = (_regs[3].Value & ~(uint)NumFragments.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the NumFragments field.</summary>
    public NumFragments GetNumFragments() => (NumFragments)(_regs[3].Value & (uint)NumFragments.kBitMask);

    /// <summary>Sets the ForceDestAlphaToOne field.</summary>
    public CxRenderTarget SetForceDestAlphaToOne(ForceDestAlphaToOne value) { _regs[3].Value = (_regs[3].Value & ~(uint)ForceDestAlphaToOne.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ForceDestAlphaToOne field.</summary>
    public ForceDestAlphaToOne GetForceDestAlphaToOne() => (ForceDestAlphaToOne)(_regs[3].Value & (uint)ForceDestAlphaToOne.kBitMask);

    /// <summary>Sets the OverwriteCombiner field.</summary>
    public CxRenderTarget SetOverwriteCombiner(OverwriteCombiner value) { _regs[4].Value = (_regs[4].Value & ~(uint)OverwriteCombiner.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the OverwriteCombiner field.</summary>
    public OverwriteCombiner GetOverwriteCombiner() => (OverwriteCombiner)(_regs[4].Value & (uint)OverwriteCombiner.kBitMask);

    /// <summary>Sets the DccClearKey field.</summary>
    public CxRenderTarget SetDccClearKey(DccClearKey value) { _regs[4].Value = (_regs[4].Value & ~(uint)DccClearKey.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DccClearKey field.</summary>
    public DccClearKey GetDccClearKey() => (DccClearKey)(_regs[4].Value & (uint)DccClearKey.kBitMask);

    /// <summary>Sets the DccMaxCompressedBlockSize field.</summary>
    public CxRenderTarget SetDccMaxCompressedBlockSize(DccMaxCompressedBlockSize value) { _regs[4].Value = (_regs[4].Value & ~(uint)DccMaxCompressedBlockSize.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DccMaxCompressedBlockSize field.</summary>
    public DccMaxCompressedBlockSize GetDccMaxCompressedBlockSize() => (DccMaxCompressedBlockSize)(_regs[4].Value & (uint)DccMaxCompressedBlockSize.kBitMask);

    /// <summary>Sets the DccMaxUncompressedBlockSize field.</summary>
    public CxRenderTarget SetDccMaxUncompressedBlockSize(DccMaxUncompressedBlockSize value) { _regs[4].Value = (_regs[4].Value & ~(uint)DccMaxUncompressedBlockSize.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DccMaxUncompressedBlockSize field.</summary>
    public DccMaxUncompressedBlockSize GetDccMaxUncompressedBlockSize() => (DccMaxUncompressedBlockSize)(_regs[4].Value & (uint)DccMaxUncompressedBlockSize.kBitMask);

    /// <summary>Sets the DccColorTransform field.</summary>
    public CxRenderTarget SetDccColorTransform(DccColorTransform value) { _regs[4].Value = (_regs[4].Value & ~(uint)DccColorTransform.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DccColorTransform field.</summary>
    public DccColorTransform GetDccColorTransform() => (DccColorTransform)(_regs[4].Value & (uint)DccColorTransform.kBitMask);

    /// <summary>Sets the DccForceIndependentBlocks field.</summary>
    public CxRenderTarget SetDccForceIndependentBlocks(DccForceIndependentBlocks value) { _regs[4].Value = (_regs[4].Value & ~(uint)DccForceIndependentBlocks.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DccForceIndependentBlocks field.</summary>
    public DccForceIndependentBlocks GetDccForceIndependentBlocks() => (DccForceIndependentBlocks)(_regs[4].Value & (uint)DccForceIndependentBlocks.kBitMask);

    /// <summary>Sets the DataWriteOnDccClearToRegister field.</summary>
    public CxRenderTarget SetDataWriteOnDccClearToRegister(DataWriteOnDccClearToRegister value) { _regs[4].Value = (_regs[4].Value & ~(uint)DataWriteOnDccClearToRegister.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DataWriteOnDccClearToRegister field.</summary>
    public DataWriteOnDccClearToRegister GetDataWriteOnDccClearToRegister() => (DataWriteOnDccClearToRegister)(_regs[4].Value & (uint)DataWriteOnDccClearToRegister.kBitMask);

    /// <summary>Sets the ClearColorWord0 field.</summary>
    public CxRenderTarget SetClearColorWord0(uint value) { _regs[7].Value = (_regs[7].Value & 0x00000000u) | ((value & 0xffffffffu)); return this; }
    /// <summary>Gets the ClearColorWord0 field.</summary>
    public uint GetClearColorWord0() => _regs[7].Value & 0xffffffffu;

    /// <summary>Sets the ClearColorWord1 field.</summary>
    public CxRenderTarget SetClearColorWord1(uint value) { _regs[8].Value = (_regs[8].Value & 0x00000000u) | ((value & 0xffffffffu)); return this; }
    /// <summary>Gets the ClearColorWord1 field.</summary>
    public uint GetClearColorWord1() => _regs[8].Value & 0xffffffffu;

    /// <summary>Sets the Height field.</summary>
    public CxRenderTarget SetHeight(uint value) { _regs[14].Value = (_regs[14].Value & 0xffffc000u) | (((value - 1) & 0x00003fffu)); return this; }
    /// <summary>Gets the Height field.</summary>
    public uint GetHeight() => (_regs[14].Value & 0x00003fffu) + 1;

    /// <summary>Sets the Width field.</summary>
    public CxRenderTarget SetWidth(uint value) { _regs[14].Value = (_regs[14].Value & 0xf0003fffu) | ((((value - 1) << 14) & 0x0fffc000u)); return this; }
    /// <summary>Gets the Width field.</summary>
    public uint GetWidth() => ((_regs[14].Value & 0x0fffc000u) >> 14) + 1;

    /// <summary>Sets the NumMipLevels field.</summary>
    public CxRenderTarget SetNumMipLevels(uint value) { _regs[14].Value = (_regs[14].Value & 0x0fffffffu) | ((((value - 1) << 28) & 0xf0000000u)); return this; }
    /// <summary>Gets the NumMipLevels field.</summary>
    public uint GetNumMipLevels() => ((_regs[14].Value & 0xf0000000u) >> 28) + 1;

    /// <summary>Sets the Depth field.</summary>
    public CxRenderTarget SetDepth(uint value) { _regs[15].Value = (_regs[15].Value & 0xffffe000u) | (((value - 1) & 0x00001fffu)); return this; }
    /// <summary>Gets the Depth field.</summary>
    public uint GetDepth() => (_regs[15].Value & 0x00001fffu) + 1;

    /// <summary>Sets the TileMode field.</summary>
    public CxRenderTarget SetTileMode(TileMode value) { _regs[15].Value = (_regs[15].Value & ~(uint)TileMode.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the TileMode field.</summary>
    public TileMode GetTileMode() => (TileMode)(_regs[15].Value & (uint)TileMode.kBitMask);

    /// <summary>Sets the Dimension field.</summary>
    public CxRenderTarget SetDimension(Dimension value) { _regs[15].Value = (_regs[15].Value & ~(uint)Dimension.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the Dimension field.</summary>
    public Dimension GetDimension() => (Dimension)(_regs[15].Value & (uint)Dimension.kBitMask);

    /// <summary>Sets the MetadataPipeAlignment field.</summary>
    public CxRenderTarget SetMetadataPipeAlignment(MetadataPipeAlignment value) { _regs[15].Value = (_regs[15].Value & ~(uint)MetadataPipeAlignment.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the MetadataPipeAlignment field.</summary>
    public MetadataPipeAlignment GetMetadataPipeAlignment() => (MetadataPipeAlignment)(_regs[15].Value & (uint)MetadataPipeAlignment.kBitMask);

    /// <summary>Sets the DataAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxRenderTarget SetDataAddress(ulong address)
    {
        _regs[0].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[10].Value = (_regs[10].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the DataAddress.</summary>
    public ulong GetDataAddress() => ((ulong)(_regs[0].Value) << 8) | (((ulong)_regs[10].Value & 0xffUL) << 40);

    /// <summary>Sets the FmaskAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxRenderTarget SetFmaskAddress(ulong address)
    {
        _regs[6].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[12].Value = (_regs[12].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the FmaskAddress.</summary>
    public ulong GetFmaskAddress() => ((ulong)(_regs[6].Value) << 8) | (((ulong)_regs[12].Value & 0xffUL) << 40);

    /// <summary>Sets the CmaskAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxRenderTarget SetCmaskAddress(ulong address)
    {
        _regs[5].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[11].Value = (_regs[11].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the CmaskAddress.</summary>
    public ulong GetCmaskAddress() => ((ulong)(_regs[5].Value) << 8) | (((ulong)_regs[11].Value & 0xffUL) << 40);

    /// <summary>Sets the DccAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxRenderTarget SetDccAddress(ulong address)
    {
        _regs[9].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[13].Value = (_regs[13].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the DccAddress.</summary>
    public ulong GetDccAddress() => ((ulong)(_regs[9].Value) << 8) | (((ulong)_regs[13].Value & 0xffUL) << 40);

}
