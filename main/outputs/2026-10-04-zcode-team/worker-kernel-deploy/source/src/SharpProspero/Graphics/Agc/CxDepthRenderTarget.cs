// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The depth-stencil render-target context register block: sixteen GPU registers describing a depth
/// and stencil buffer's formats, dimensions, clear values, HTILE acceleration, and read/write and HTILE
/// addresses. The typed setters encode each field into the right register bits, as the graphics context
/// register definitions do, so the block can be written straight into a command buffer.
/// </summary>
/// <remarks>
/// As with <see cref="CxRenderTarget"/>, the register offsets and reset values come from the graphics
/// driver at runtime through <c>sceAgcGetRegisterDefaults</c> (they depend on the graphics processor
/// configuration); construct the block, <see cref="Init"/> it from those defaults, then apply the setters.
/// </remarks>
public sealed class CxDepthRenderTarget
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 16;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index (0..15).</summary>
    public CxRegister this[int index]
    {
        get => _regs[index];
        set => _regs[index] = value;
    }

    /// <summary>Loads the register offsets and reset values from the driver-provided defaults (sixteen registers).</summary>
    /// <exception cref="ArgumentException">Fewer than sixteen defaults were provided.</exception>
    public CxDepthRenderTarget Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++)
            _regs[i] = defaults[i];
        return this;
    }

    /// <summary>DepthFormat field values for a depth-stencil target.</summary>
    public enum DepthFormat : uint
    {
        kInvalid = 0x00000000,
        k16UNorm = 0x00000001,
        k32Float = 0x00000003,
        kBitMask = 0x00000003,
    }

    /// <summary>NumFragments field values for a depth-stencil target.</summary>
    public enum NumFragments : uint
    {
        k1 = 0x00000000,
        k2 = 0x00000004,
        k4 = 0x00000008,
        k8 = 0x0000000c,
        kBitMask = 0x0000000c,
    }

    /// <summary>HtileAcceleration field values for a depth-stencil target.</summary>
    public enum HtileAcceleration : uint
    {
        kEnable = 0x20000000,
        kDisable = 0x00000000,
        kBitMask = 0x20000000,
    }

    /// <summary>ExpClearDepthAcceleration field values for a depth-stencil target.</summary>
    public enum ExpClearDepthAcceleration : uint
    {
        kEnable = 0x08000000,
        kDisable = 0x00000000,
        kBitMask = 0x08000000,
    }

    /// <summary>ZCompareBase field values for a depth-stencil target.</summary>
    public enum ZCompareBase : uint
    {
        kZMin = 0x00000000,
        kZMax = 0x80000000,
        kBitMask = 0x80000000,
    }

    /// <summary>EmbeddedSampleLocations field values for a depth-stencil target.</summary>
    public enum EmbeddedSampleLocations : uint
    {
        kEnable = 0x00100800,
        kDisable = 0x00000000,
        kBitMask = 0x00100800,
    }

    /// <summary>PartiallyResidentDepth field values for a depth-stencil target.</summary>
    public enum PartiallyResidentDepth : uint
    {
        kEnable = 0x00001000,
        kDisable = 0x00000000,
        kBitMask = 0x00001000,
    }

    /// <summary>TextureCompatiblePlaneCompression field values for a depth-stencil target.</summary>
    public enum TextureCompatiblePlaneCompression : uint
    {
        kFullCompression = 0x00000000,
        kBitMask = 0x03800000,
    }

    /// <summary>StencilFormat field values for a depth-stencil target.</summary>
    public enum StencilFormat : uint
    {
        kInvalid = 0x00000000,
        k8UInt = 0x00000001,
        kBitMask = 0x00000001,
    }

    /// <summary>TextureCompatibleStencil field values for a depth-stencil target.</summary>
    public enum TextureCompatibleStencil : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00100800,
        kBitMask = 0x00100800,
    }

    /// <summary>HtileStencil field values for a depth-stencil target.</summary>
    public enum HtileStencil : uint
    {
        kEnable = 0x00000000,
        kDisable = 0x20000000,
        kBitMask = 0x20000000,
    }

    /// <summary>PartiallyResidentStencil field values for a depth-stencil target.</summary>
    public enum PartiallyResidentStencil : uint
    {
        kEnable = 0x00001000,
        kDisable = 0x00000000,
        kBitMask = 0x00001000,
    }

    /// <summary>ExpClearStencilAcceleration field values for a depth-stencil target.</summary>
    public enum ExpClearStencilAcceleration : uint
    {
        kEnable = 0x08000000,
        kDisable = 0x00000000,
        kBitMask = 0x08000000,
    }

    /// <summary>DepthWrite field values for a depth-stencil target.</summary>
    public enum DepthWrite : uint
    {
        kEnable = 0x00000000,
        kDisable = 0x01000000,
        kBitMask = 0x01000000,
    }

    /// <summary>StencilWrite field values for a depth-stencil target.</summary>
    public enum StencilWrite : uint
    {
        kEnable = 0x00000000,
        kDisable = 0x02000000,
        kBitMask = 0x02000000,
    }

    /// <summary>Sets the NumMipLevels field.</summary>
    public CxDepthRenderTarget SetNumMipLevels(uint value) { _regs[0].Value = (_regs[0].Value & 0xfff0ffffu) | ((((value - 1) << 16) & 0x000f0000u)); return this; }
    /// <summary>Gets the NumMipLevels field.</summary>
    public uint GetNumMipLevels() => ((_regs[0].Value & 0x000f0000u) >> 16) + 1;

    /// <summary>Sets the DepthFormat field.</summary>
    public CxDepthRenderTarget SetDepthFormat(DepthFormat value) { _regs[0].Value = (_regs[0].Value & ~(uint)DepthFormat.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DepthFormat field.</summary>
    public DepthFormat GetDepthFormat() => (DepthFormat)(_regs[0].Value & (uint)DepthFormat.kBitMask);

    /// <summary>Sets the NumFragments field.</summary>
    public CxDepthRenderTarget SetNumFragments(NumFragments value) { _regs[0].Value = (_regs[0].Value & ~(uint)NumFragments.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the NumFragments field.</summary>
    public NumFragments GetNumFragments() => (NumFragments)(_regs[0].Value & (uint)NumFragments.kBitMask);

    /// <summary>Sets the HtileAcceleration field.</summary>
    public CxDepthRenderTarget SetHtileAcceleration(HtileAcceleration value) { _regs[0].Value = (_regs[0].Value & ~(uint)HtileAcceleration.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the HtileAcceleration field.</summary>
    public HtileAcceleration GetHtileAcceleration() => (HtileAcceleration)(_regs[0].Value & (uint)HtileAcceleration.kBitMask);

    /// <summary>Sets the ExpClearDepthAcceleration field.</summary>
    public CxDepthRenderTarget SetExpClearDepthAcceleration(ExpClearDepthAcceleration value) { _regs[0].Value = (_regs[0].Value & ~(uint)ExpClearDepthAcceleration.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ExpClearDepthAcceleration field.</summary>
    public ExpClearDepthAcceleration GetExpClearDepthAcceleration() => (ExpClearDepthAcceleration)(_regs[0].Value & (uint)ExpClearDepthAcceleration.kBitMask);

    /// <summary>Sets the ZCompareBase field.</summary>
    public CxDepthRenderTarget SetZCompareBase(ZCompareBase value) { _regs[0].Value = (_regs[0].Value & ~(uint)ZCompareBase.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ZCompareBase field.</summary>
    public ZCompareBase GetZCompareBase() => (ZCompareBase)(_regs[0].Value & (uint)ZCompareBase.kBitMask);

    /// <summary>Sets the EmbeddedSampleLocations field.</summary>
    public CxDepthRenderTarget SetEmbeddedSampleLocations(EmbeddedSampleLocations value) { _regs[0].Value = (_regs[0].Value & ~(uint)EmbeddedSampleLocations.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the EmbeddedSampleLocations field.</summary>
    public EmbeddedSampleLocations GetEmbeddedSampleLocations() => (EmbeddedSampleLocations)(_regs[0].Value & (uint)EmbeddedSampleLocations.kBitMask);

    /// <summary>Sets the PartiallyResidentDepth field.</summary>
    public CxDepthRenderTarget SetPartiallyResidentDepth(PartiallyResidentDepth value) { _regs[0].Value = (_regs[0].Value & ~(uint)PartiallyResidentDepth.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the PartiallyResidentDepth field.</summary>
    public PartiallyResidentDepth GetPartiallyResidentDepth() => (PartiallyResidentDepth)(_regs[0].Value & (uint)PartiallyResidentDepth.kBitMask);

    /// <summary>Sets the TextureCompatiblePlaneCompression field.</summary>
    public CxDepthRenderTarget SetTextureCompatiblePlaneCompression(TextureCompatiblePlaneCompression value) { _regs[0].Value = (_regs[0].Value & ~(uint)TextureCompatiblePlaneCompression.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the TextureCompatiblePlaneCompression field.</summary>
    public TextureCompatiblePlaneCompression GetTextureCompatiblePlaneCompression() => (TextureCompatiblePlaneCompression)(_regs[0].Value & (uint)TextureCompatiblePlaneCompression.kBitMask);

    /// <summary>Sets the StencilFormat field.</summary>
    public CxDepthRenderTarget SetStencilFormat(StencilFormat value) { _regs[1].Value = (_regs[1].Value & ~(uint)StencilFormat.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilFormat field.</summary>
    public StencilFormat GetStencilFormat() => (StencilFormat)(_regs[1].Value & (uint)StencilFormat.kBitMask);

    /// <summary>Sets the TextureCompatibleStencil field.</summary>
    public CxDepthRenderTarget SetTextureCompatibleStencil(TextureCompatibleStencil value) { _regs[1].Value = (_regs[1].Value & ~(uint)TextureCompatibleStencil.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the TextureCompatibleStencil field.</summary>
    public TextureCompatibleStencil GetTextureCompatibleStencil() => (TextureCompatibleStencil)(_regs[1].Value & (uint)TextureCompatibleStencil.kBitMask);

    /// <summary>Sets the HtileStencil field.</summary>
    public CxDepthRenderTarget SetHtileStencil(HtileStencil value) { _regs[1].Value = (_regs[1].Value & ~(uint)HtileStencil.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the HtileStencil field.</summary>
    public HtileStencil GetHtileStencil() => (HtileStencil)(_regs[1].Value & (uint)HtileStencil.kBitMask);

    /// <summary>Sets the PartiallyResidentStencil field.</summary>
    public CxDepthRenderTarget SetPartiallyResidentStencil(PartiallyResidentStencil value) { _regs[1].Value = (_regs[1].Value & ~(uint)PartiallyResidentStencil.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the PartiallyResidentStencil field.</summary>
    public PartiallyResidentStencil GetPartiallyResidentStencil() => (PartiallyResidentStencil)(_regs[1].Value & (uint)PartiallyResidentStencil.kBitMask);

    /// <summary>Sets the ExpClearStencilAcceleration field.</summary>
    public CxDepthRenderTarget SetExpClearStencilAcceleration(ExpClearStencilAcceleration value) { _regs[1].Value = (_regs[1].Value & ~(uint)ExpClearStencilAcceleration.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ExpClearStencilAcceleration field.</summary>
    public ExpClearStencilAcceleration GetExpClearStencilAcceleration() => (ExpClearStencilAcceleration)(_regs[1].Value & (uint)ExpClearStencilAcceleration.kBitMask);

    /// <summary>Sets the BaseArraySliceIndex field.</summary>
    public CxDepthRenderTarget SetBaseArraySliceIndex(uint value) { _regs[11].Value = (_regs[11].Value & 0xffffe000u) | ((value & 0x000007ffu) | (value & 0x00001800u)); return this; }
    /// <summary>Gets the BaseArraySliceIndex field.</summary>
    public uint GetBaseArraySliceIndex() => (_regs[11].Value & 0x000007ffu) | (_regs[11].Value & 0x00001800u);

    /// <summary>Sets the LastArraySliceIndex field.</summary>
    public CxDepthRenderTarget SetLastArraySliceIndex(uint value) { _regs[11].Value = (_regs[11].Value & 0x3f001fffu) | (((value << 13) & 0x00ffe000u) | ((value << 19) & 0xc0000000u)); return this; }
    /// <summary>Gets the LastArraySliceIndex field.</summary>
    public uint GetLastArraySliceIndex() => ((_regs[11].Value & 0x00ffe000u) >> 13) | ((_regs[11].Value & 0xc0000000u) >> 19);

    /// <summary>Sets the CurrentMipLevel field.</summary>
    public CxDepthRenderTarget SetCurrentMipLevel(uint value) { _regs[11].Value = (_regs[11].Value & 0xc3ffffffu) | (((value << 26) & 0x3c000000u)); return this; }
    /// <summary>Gets the CurrentMipLevel field.</summary>
    public uint GetCurrentMipLevel() => (_regs[11].Value & 0x3c000000u) >> 26;

    /// <summary>Sets the DepthWrite field.</summary>
    public CxDepthRenderTarget SetDepthWrite(DepthWrite value) { _regs[11].Value = (_regs[11].Value & ~(uint)DepthWrite.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DepthWrite field.</summary>
    public DepthWrite GetDepthWrite() => (DepthWrite)(_regs[11].Value & (uint)DepthWrite.kBitMask);

    /// <summary>Sets the StencilWrite field.</summary>
    public CxDepthRenderTarget SetStencilWrite(StencilWrite value) { _regs[11].Value = (_regs[11].Value & ~(uint)StencilWrite.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilWrite field.</summary>
    public StencilWrite GetStencilWrite() => (StencilWrite)(_regs[11].Value & (uint)StencilWrite.kBitMask);

    /// <summary>Sets the Width field.</summary>
    public CxDepthRenderTarget SetWidth(uint value) { _regs[13].Value = (_regs[13].Value & 0xffffc000u) | (((value - 1) & 0x00003fffu)); return this; }
    /// <summary>Gets the Width field.</summary>
    public uint GetWidth() => (_regs[13].Value & 0x00003fffu) + 1;

    /// <summary>Sets the Height field.</summary>
    public CxDepthRenderTarget SetHeight(uint value) { _regs[13].Value = (_regs[13].Value & 0xc000ffffu) | ((((value - 1) << 16) & 0x3fff0000u)); return this; }
    /// <summary>Gets the Height field.</summary>
    public uint GetHeight() => ((_regs[13].Value & 0x3fff0000u) >> 16) + 1;

    /// <summary>Sets the DepthClearValue (stored as its raw 32-bit floating-point value).</summary>
    public CxDepthRenderTarget SetDepthClearValue(float value) { _regs[14].Value = BitConverter.SingleToUInt32Bits(value); return this; }
    /// <summary>Gets the DepthClearValue.</summary>
    public float GetDepthClearValue() => BitConverter.UInt32BitsToSingle(_regs[14].Value);

    /// <summary>Sets the StencilClearValue field.</summary>
    public CxDepthRenderTarget SetStencilClearValue(uint value) { _regs[15].Value = (_regs[15].Value & 0xffffff00u) | ((value & 0x000000ffu)); return this; }
    /// <summary>Gets the StencilClearValue field.</summary>
    public uint GetStencilClearValue() => _regs[15].Value & 0x000000ffu;

    /// <summary>Sets the DepthReadAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxDepthRenderTarget SetDepthReadAddress(ulong address)
    {
        _regs[2].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[6].Value = (_regs[6].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the DepthReadAddress.</summary>
    public ulong GetDepthReadAddress() => ((ulong)(_regs[2].Value) << 8) | (((ulong)_regs[6].Value & 0xffUL) << 40);

    /// <summary>Sets the DepthWriteAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxDepthRenderTarget SetDepthWriteAddress(ulong address)
    {
        _regs[4].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[8].Value = (_regs[8].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the DepthWriteAddress.</summary>
    public ulong GetDepthWriteAddress() => ((ulong)(_regs[4].Value) << 8) | (((ulong)_regs[8].Value & 0xffUL) << 40);

    /// <summary>Sets the HtileAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxDepthRenderTarget SetHtileAddress(ulong address)
    {
        _regs[12].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[10].Value = (_regs[10].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the HtileAddress.</summary>
    public ulong GetHtileAddress() => ((ulong)(_regs[12].Value) << 8) | (((ulong)_regs[10].Value & 0xffUL) << 40);

    /// <summary>Sets the StencilReadAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxDepthRenderTarget SetStencilReadAddress(ulong address)
    {
        _regs[3].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[7].Value = (_regs[7].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the StencilReadAddress.</summary>
    public ulong GetStencilReadAddress() => ((ulong)(_regs[3].Value) << 8) | (((ulong)_regs[7].Value & 0xffUL) << 40);

    /// <summary>Sets the StencilWriteAddress (a 256-byte-aligned address, split across two registers).</summary>
    public CxDepthRenderTarget SetStencilWriteAddress(ulong address)
    {
        _regs[5].Value = (uint)((address >> 8) & 0xffffffffu);
        _regs[9].Value = (_regs[9].Value & 0xffffff00u) | (uint)((address >> 40) & 0x000000ffu);
        return this;
    }
    /// <summary>Gets the StencilWriteAddress.</summary>
    public ulong GetStencilWriteAddress() => ((ulong)(_regs[5].Value) << 8) | (((ulong)_regs[9].Value & 0xffUL) << 40);

}
