// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The depth and stencil test control register: whether the depth test and write are on and its
/// comparison, and whether the stencil test is on and its comparisons for front and back faces. Record
/// it to enable the depth buffer or the stencil test for a draw.
/// </summary>
/// <remarks>
/// The register offset(s) and reset values load from the driver defaults through <see cref="Init"/>,
/// exactly as the graphics context register definitions do; construct the block, <see cref="Init"/> it,
/// then apply the setters and write <see cref="Registers"/> into a command buffer.
/// </remarks>
public sealed class CxDepthStencilControl
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 1;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];
    private readonly ushort[] _defaultOffsets = new ushort[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index.</summary>
    public CxRegister this[int index] { get => _regs[index]; set => _regs[index] = value; }

    /// <summary>Loads the register offsets and reset values from the driver-provided defaults.</summary>
    /// <exception cref="ArgumentException">Fewer than 1 defaults were provided.</exception>
    public CxDepthStencilControl Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++) { _regs[i] = defaults[i]; _defaultOffsets[i] = defaults[i].Offset; }
        return this;
    }

    /// <summary>DepthWrite field values.</summary>
    public enum DepthWrite : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00000004,
        kBitMask = 0x00000004,
    }

    /// <summary>Depth field values.</summary>
    public enum Depth : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00000002,
        kBitMask = 0x00000002,
    }

    /// <summary>Stencil field values.</summary>
    public enum Stencil : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00000001,
        kBitMask = 0x00000001,
    }

    /// <summary>DepthBounds field values.</summary>
    public enum DepthBounds : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00000008,
        kBitMask = 0x00000008,
    }

    /// <summary>SeparateStencil field values.</summary>
    public enum SeparateStencil : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x00000080,
        kBitMask = 0x00000080,
    }

    /// <summary>ColorWritesOnDepthFail field values.</summary>
    public enum ColorWritesOnDepthFail : uint
    {
        kDisable = 0x00000000,
        kEnable = 0x40000000,
        kBitMask = 0x40000000,
    }

    /// <summary>ColorWritesOnDepthPass field values.</summary>
    public enum ColorWritesOnDepthPass : uint
    {
        kDisable = 0x80000000,
        kEnable = 0x00000000,
        kBitMask = 0x80000000,
    }

    /// <summary>DepthFunction field values.</summary>
    public enum DepthFunction : uint
    {
        kNever = 0x00000000,
        kLess = 0x00000010,
        kEqual = 0x00000020,
        kLessEqual = 0x00000030,
        kGreater = 0x00000040,
        kNotEqual = 0x00000050,
        kGreaterEqual = 0x00000060,
        kAlways = 0x00000070,
        kBitMask = 0x00000070,
    }

    /// <summary>StencilFunction field values.</summary>
    public enum StencilFunction : uint
    {
        kNever = 0x00000000,
        kLess = 0x00000100,
        kEqual = 0x00000200,
        kLessEqual = 0x00000300,
        kGreater = 0x00000400,
        kNotEqual = 0x00000500,
        kGreaterEqual = 0x00000600,
        kAlways = 0x00000700,
        kBitMask = 0x00000700,
    }

    /// <summary>StencilFunctionBack field values.</summary>
    public enum StencilFunctionBack : uint
    {
        kNever = 0x00000000,
        kLess = 0x00100000,
        kEqual = 0x00200000,
        kLessEqual = 0x00300000,
        kGreater = 0x00400000,
        kNotEqual = 0x00500000,
        kGreaterEqual = 0x00600000,
        kAlways = 0x00700000,
        kBitMask = 0x00700000,
    }

    /// <summary>Sets the DepthWrite field.</summary>
    public CxDepthStencilControl SetDepthWrite(DepthWrite value) { _regs[0].Value = (_regs[0].Value & ~(uint)DepthWrite.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DepthWrite field.</summary>
    public DepthWrite GetDepthWrite() => (DepthWrite)(_regs[0].Value & (uint)DepthWrite.kBitMask);

    /// <summary>Sets the Depth field.</summary>
    public CxDepthStencilControl SetDepth(Depth value) { _regs[0].Value = (_regs[0].Value & ~(uint)Depth.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the Depth field.</summary>
    public Depth GetDepth() => (Depth)(_regs[0].Value & (uint)Depth.kBitMask);

    /// <summary>Sets the Stencil field.</summary>
    public CxDepthStencilControl SetStencil(Stencil value) { _regs[0].Value = (_regs[0].Value & ~(uint)Stencil.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the Stencil field.</summary>
    public Stencil GetStencil() => (Stencil)(_regs[0].Value & (uint)Stencil.kBitMask);

    /// <summary>Sets the DepthBounds field.</summary>
    public CxDepthStencilControl SetDepthBounds(DepthBounds value) { _regs[0].Value = (_regs[0].Value & ~(uint)DepthBounds.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DepthBounds field.</summary>
    public DepthBounds GetDepthBounds() => (DepthBounds)(_regs[0].Value & (uint)DepthBounds.kBitMask);

    /// <summary>Sets the SeparateStencil field.</summary>
    public CxDepthStencilControl SetSeparateStencil(SeparateStencil value) { _regs[0].Value = (_regs[0].Value & ~(uint)SeparateStencil.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the SeparateStencil field.</summary>
    public SeparateStencil GetSeparateStencil() => (SeparateStencil)(_regs[0].Value & (uint)SeparateStencil.kBitMask);

    /// <summary>Sets the ColorWritesOnDepthFail field.</summary>
    public CxDepthStencilControl SetColorWritesOnDepthFail(ColorWritesOnDepthFail value) { _regs[0].Value = (_regs[0].Value & ~(uint)ColorWritesOnDepthFail.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ColorWritesOnDepthFail field.</summary>
    public ColorWritesOnDepthFail GetColorWritesOnDepthFail() => (ColorWritesOnDepthFail)(_regs[0].Value & (uint)ColorWritesOnDepthFail.kBitMask);

    /// <summary>Sets the ColorWritesOnDepthPass field.</summary>
    public CxDepthStencilControl SetColorWritesOnDepthPass(ColorWritesOnDepthPass value) { _regs[0].Value = (_regs[0].Value & ~(uint)ColorWritesOnDepthPass.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ColorWritesOnDepthPass field.</summary>
    public ColorWritesOnDepthPass GetColorWritesOnDepthPass() => (ColorWritesOnDepthPass)(_regs[0].Value & (uint)ColorWritesOnDepthPass.kBitMask);

    /// <summary>Sets the DepthFunction field.</summary>
    public CxDepthStencilControl SetDepthFunction(DepthFunction value) { _regs[0].Value = (_regs[0].Value & ~(uint)DepthFunction.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the DepthFunction field.</summary>
    public DepthFunction GetDepthFunction() => (DepthFunction)(_regs[0].Value & (uint)DepthFunction.kBitMask);

    /// <summary>Sets the StencilFunction field.</summary>
    public CxDepthStencilControl SetStencilFunction(StencilFunction value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilFunction.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilFunction field.</summary>
    public StencilFunction GetStencilFunction() => (StencilFunction)(_regs[0].Value & (uint)StencilFunction.kBitMask);

    /// <summary>Sets the StencilFunctionBack field.</summary>
    public CxDepthStencilControl SetStencilFunctionBack(StencilFunctionBack value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilFunctionBack.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilFunctionBack field.</summary>
    public StencilFunctionBack GetStencilFunctionBack() => (StencilFunctionBack)(_regs[0].Value & (uint)StencilFunctionBack.kBitMask);

}

/// <summary>
/// The stencil test value, compare mask, write mask and operation value for front-facing primitives.
/// </summary>
/// <remarks>
/// The register offset(s) and reset values load from the driver defaults through <see cref="Init"/>,
/// exactly as the graphics context register definitions do; construct the block, <see cref="Init"/> it,
/// then apply the setters and write <see cref="Registers"/> into a command buffer.
/// </remarks>
public sealed class CxStencilControl
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 1;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];
    private readonly ushort[] _defaultOffsets = new ushort[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index.</summary>
    public CxRegister this[int index] { get => _regs[index]; set => _regs[index] = value; }

    /// <summary>Loads the register offsets and reset values from the driver-provided defaults.</summary>
    /// <exception cref="ArgumentException">Fewer than 1 defaults were provided.</exception>
    public CxStencilControl Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++) { _regs[i] = defaults[i]; _defaultOffsets[i] = defaults[i].Offset; }
        return this;
    }

    /// <summary>Sets the TestValue field.</summary>
    public CxStencilControl SetTestValue(uint value) { _regs[0].Value = (_regs[0].Value & 0xffffff00u) | (value & 0x000000ffu); return this; }
    /// <summary>Gets the TestValue field.</summary>
    public uint GetTestValue() => (_regs[0].Value & 0x000000ffu);

    /// <summary>Sets the Mask field.</summary>
    public CxStencilControl SetMask(uint value) { _regs[0].Value = (_regs[0].Value & 0xffff00ffu) | ((value << 8) & 0x0000ff00u); return this; }
    /// <summary>Gets the Mask field.</summary>
    public uint GetMask() => ((_regs[0].Value & 0x0000ff00u) >> 8);

    /// <summary>Sets the WriteMask field.</summary>
    public CxStencilControl SetWriteMask(uint value) { _regs[0].Value = (_regs[0].Value & 0xff00ffffu) | ((value << 16) & 0x00ff0000u); return this; }
    /// <summary>Gets the WriteMask field.</summary>
    public uint GetWriteMask() => ((_regs[0].Value & 0x00ff0000u) >> 16);

    /// <summary>Sets the OpValue field.</summary>
    public CxStencilControl SetOpValue(uint value) { _regs[0].Value = (_regs[0].Value & 0x00ffffffu) | ((value << 24) & 0xff000000u); return this; }
    /// <summary>Gets the OpValue field.</summary>
    public uint GetOpValue() => ((_regs[0].Value & 0xff000000u) >> 24);

}

/// <summary>
/// The stencil test value, compare mask, write mask and operation value for back-facing primitives.
/// </summary>
/// <remarks>
/// The register offset(s) and reset values load from the driver defaults through <see cref="Init"/>,
/// exactly as the graphics context register definitions do; construct the block, <see cref="Init"/> it,
/// then apply the setters and write <see cref="Registers"/> into a command buffer.
/// </remarks>
public sealed class CxStencilControlBackFace
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 1;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];
    private readonly ushort[] _defaultOffsets = new ushort[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index.</summary>
    public CxRegister this[int index] { get => _regs[index]; set => _regs[index] = value; }

    /// <summary>Loads the register offsets and reset values from the driver-provided defaults.</summary>
    /// <exception cref="ArgumentException">Fewer than 1 defaults were provided.</exception>
    public CxStencilControlBackFace Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++) { _regs[i] = defaults[i]; _defaultOffsets[i] = defaults[i].Offset; }
        return this;
    }

    /// <summary>Sets the TestValue field.</summary>
    public CxStencilControlBackFace SetTestValue(uint value) { _regs[0].Value = (_regs[0].Value & 0xffffff00u) | (value & 0x000000ffu); return this; }
    /// <summary>Gets the TestValue field.</summary>
    public uint GetTestValue() => (_regs[0].Value & 0x000000ffu);

    /// <summary>Sets the Mask field.</summary>
    public CxStencilControlBackFace SetMask(uint value) { _regs[0].Value = (_regs[0].Value & 0xffff00ffu) | ((value << 8) & 0x0000ff00u); return this; }
    /// <summary>Gets the Mask field.</summary>
    public uint GetMask() => ((_regs[0].Value & 0x0000ff00u) >> 8);

    /// <summary>Sets the WriteMask field.</summary>
    public CxStencilControlBackFace SetWriteMask(uint value) { _regs[0].Value = (_regs[0].Value & 0xff00ffffu) | ((value << 16) & 0x00ff0000u); return this; }
    /// <summary>Gets the WriteMask field.</summary>
    public uint GetWriteMask() => ((_regs[0].Value & 0x00ff0000u) >> 16);

    /// <summary>Sets the OpValue field.</summary>
    public CxStencilControlBackFace SetOpValue(uint value) { _regs[0].Value = (_regs[0].Value & 0x00ffffffu) | ((value << 24) & 0xff000000u); return this; }
    /// <summary>Gets the OpValue field.</summary>
    public uint GetOpValue() => ((_regs[0].Value & 0xff000000u) >> 24);

}

/// <summary>
/// The stencil operations applied on stencil fail, depth-stencil pass, and depth fail, for front and
/// back faces.
/// </summary>
/// <remarks>
/// The register offset(s) and reset values load from the driver defaults through <see cref="Init"/>,
/// exactly as the graphics context register definitions do; construct the block, <see cref="Init"/> it,
/// then apply the setters and write <see cref="Registers"/> into a command buffer.
/// </remarks>
public sealed class CxStencilOpControl
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 1;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];
    private readonly ushort[] _defaultOffsets = new ushort[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index.</summary>
    public CxRegister this[int index] { get => _regs[index]; set => _regs[index] = value; }

    /// <summary>Loads the register offsets and reset values from the driver-provided defaults.</summary>
    /// <exception cref="ArgumentException">Fewer than 1 defaults were provided.</exception>
    public CxStencilOpControl Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++) { _regs[i] = defaults[i]; _defaultOffsets[i] = defaults[i].Offset; }
        return this;
    }

    /// <summary>StencilFailOp field values.</summary>
    public enum StencilFailOp : uint
    {
        kKeep = 0x00000000,
        kZero = 0x00000001,
        kOnes = 0x00000002,
        kReplaceTest = 0x00000003,
        kReplaceOp = 0x00000004,
        kAddClamp = 0x00000005,
        kSubClamp = 0x00000006,
        kInvert = 0x00000007,
        kAddWrap = 0x00000008,
        kSubWrap = 0x00000009,
        kAnd = 0x0000000a,
        kOr = 0x0000000b,
        kXor = 0x0000000c,
        kNand = 0x0000000d,
        kNor = 0x0000000e,
        kXnor = 0x0000000f,
        kBitMask = 0x0000000f,
    }

    /// <summary>StencilZPassOp field values.</summary>
    public enum StencilZPassOp : uint
    {
        kKeep = 0x00000000,
        kZero = 0x00000010,
        kOnes = 0x00000020,
        kReplaceTest = 0x00000030,
        kReplaceOp = 0x00000040,
        kAddClamp = 0x00000050,
        kSubClamp = 0x00000060,
        kInvert = 0x00000070,
        kAddWrap = 0x00000080,
        kSubWrap = 0x00000090,
        kAnd = 0x000000a0,
        kOr = 0x000000b0,
        kXor = 0x000000c0,
        kNand = 0x000000d0,
        kNor = 0x000000e0,
        kXnor = 0x000000f0,
        kBitMask = 0x000000f0,
    }

    /// <summary>StencilZFailOp field values.</summary>
    public enum StencilZFailOp : uint
    {
        kKeep = 0x00000000,
        kZero = 0x00000100,
        kOnes = 0x00000200,
        kReplaceTest = 0x00000300,
        kReplaceOp = 0x00000400,
        kAddClamp = 0x00000500,
        kSubClamp = 0x00000600,
        kInvert = 0x00000700,
        kAddWrap = 0x00000800,
        kSubWrap = 0x00000900,
        kAnd = 0x00000a00,
        kOr = 0x00000b00,
        kXor = 0x00000c00,
        kNand = 0x00000d00,
        kNor = 0x00000e00,
        kXnor = 0x00000f00,
        kBitMask = 0x00000f00,
    }

    /// <summary>StencilFailBackOp field values.</summary>
    public enum StencilFailBackOp : uint
    {
        kKeep = 0x00000000,
        kZero = 0x00001000,
        kOnes = 0x00002000,
        kReplaceTest = 0x00003000,
        kReplaceOp = 0x00004000,
        kAddClamp = 0x00005000,
        kSubClamp = 0x00006000,
        kInvert = 0x00007000,
        kAddWrap = 0x00008000,
        kSubWrap = 0x00009000,
        kAnd = 0x0000a000,
        kOr = 0x0000b000,
        kXor = 0x0000c000,
        kNand = 0x0000d000,
        kNor = 0x0000e000,
        kXnor = 0x0000f000,
        kBitMask = 0x0000f000,
    }

    /// <summary>StencilZPassBackOp field values.</summary>
    public enum StencilZPassBackOp : uint
    {
        kKeep = 0x00000000,
        kZero = 0x00010000,
        kOnes = 0x00020000,
        kReplaceTest = 0x00030000,
        kReplaceOp = 0x00040000,
        kAddClamp = 0x00050000,
        kSubClamp = 0x00060000,
        kInvert = 0x00070000,
        kAddWrap = 0x00080000,
        kSubWrap = 0x00090000,
        kAnd = 0x000a0000,
        kOr = 0x000b0000,
        kXor = 0x000c0000,
        kNand = 0x000d0000,
        kNor = 0x000e0000,
        kXnor = 0x000f0000,
        kBitMask = 0x000f0000,
    }

    /// <summary>StencilZFailBackOp field values.</summary>
    public enum StencilZFailBackOp : uint
    {
        kKeep = 0x00000000,
        kZero = 0x00100000,
        kOnes = 0x00200000,
        kReplaceTest = 0x00300000,
        kReplaceOp = 0x00400000,
        kAddClamp = 0x00500000,
        kSubClamp = 0x00600000,
        kInvert = 0x00700000,
        kAddWrap = 0x00800000,
        kSubWrap = 0x00900000,
        kAnd = 0x00a00000,
        kOr = 0x00b00000,
        kXor = 0x00c00000,
        kNand = 0x00d00000,
        kNor = 0x00e00000,
        kXnor = 0x00f00000,
        kBitMask = 0x00f00000,
    }

    /// <summary>CreateAllocationCrawler field values.</summary>
    public enum CreateAllocationCrawler : uint
    {
        kEnable = 0x00000000,
        kDisable = 0x40000000,
        kBitMask = 0x40000000,
    }

    /// <summary>Counter field values.</summary>
    public enum Counter : uint
    {
        kEnable = 0x80000000,
        kDisable = 0x00000000,
        kBitMask = 0x80000000,
    }

    /// <summary>CrawlerType field values.</summary>
    public enum CrawlerType : uint
    {
        kPixel = 0x00000000,
        kVertex = 0x00010000,
        kCompute = 0x00020000,
        kGeometry = 0x00030000,
        kBitMask = 0x00030000,
    }

    /// <summary>Sets the StencilFailOp field.</summary>
    public CxStencilOpControl SetStencilFailOp(StencilFailOp value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilFailOp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilFailOp field.</summary>
    public StencilFailOp GetStencilFailOp() => (StencilFailOp)(_regs[0].Value & (uint)StencilFailOp.kBitMask);

    /// <summary>Sets the StencilZPassOp field.</summary>
    public CxStencilOpControl SetStencilZPassOp(StencilZPassOp value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilZPassOp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilZPassOp field.</summary>
    public StencilZPassOp GetStencilZPassOp() => (StencilZPassOp)(_regs[0].Value & (uint)StencilZPassOp.kBitMask);

    /// <summary>Sets the StencilZFailOp field.</summary>
    public CxStencilOpControl SetStencilZFailOp(StencilZFailOp value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilZFailOp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilZFailOp field.</summary>
    public StencilZFailOp GetStencilZFailOp() => (StencilZFailOp)(_regs[0].Value & (uint)StencilZFailOp.kBitMask);

    /// <summary>Sets the StencilFailBackOp field.</summary>
    public CxStencilOpControl SetStencilFailBackOp(StencilFailBackOp value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilFailBackOp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilFailBackOp field.</summary>
    public StencilFailBackOp GetStencilFailBackOp() => (StencilFailBackOp)(_regs[0].Value & (uint)StencilFailBackOp.kBitMask);

    /// <summary>Sets the StencilZPassBackOp field.</summary>
    public CxStencilOpControl SetStencilZPassBackOp(StencilZPassBackOp value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilZPassBackOp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilZPassBackOp field.</summary>
    public StencilZPassBackOp GetStencilZPassBackOp() => (StencilZPassBackOp)(_regs[0].Value & (uint)StencilZPassBackOp.kBitMask);

    /// <summary>Sets the StencilZFailBackOp field.</summary>
    public CxStencilOpControl SetStencilZFailBackOp(StencilZFailBackOp value) { _regs[0].Value = (_regs[0].Value & ~(uint)StencilZFailBackOp.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the StencilZFailBackOp field.</summary>
    public StencilZFailBackOp GetStencilZFailBackOp() => (StencilZFailBackOp)(_regs[0].Value & (uint)StencilZFailBackOp.kBitMask);

}
