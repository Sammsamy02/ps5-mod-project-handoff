// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The colour-blend control register for one render target: whether blending is on, the source and
/// destination multipliers and the combine function for colour and alpha, and whether alpha uses its own
/// equation. Build it, select the render target with SetSlot, and record it to enable alpha blending.
/// </summary>
/// <remarks>
/// The register offset(s) and reset values load from the driver defaults through <see cref="Init"/>,
/// exactly as the graphics context register definitions do; construct the block, <see cref="Init"/> it,
/// then apply the setters and write <see cref="Registers"/> into a command buffer.
/// </remarks>
public sealed class CxBlendControl
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
    public CxBlendControl Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++) { _regs[i] = defaults[i]; _defaultOffsets[i] = defaults[i].Offset; }
        return this;
    }

    /// <summary>Blend field values.</summary>
    public enum Blend : uint
    {
        kEnable = 0x40000000,
        kDisable = 0x00000000,
        kBitMask = 0x40000000,
    }

    /// <summary>SeparateAlphaBlend field values.</summary>
    public enum SeparateAlphaBlend : uint
    {
        kEnable = 0x20000000,
        kDisable = 0x00000000,
        kBitMask = 0x20000000,
    }

    /// <summary>ColorSourceMultiplier field values.</summary>
    public enum ColorSourceMultiplier : uint
    {
        kZero = 0x00000000,
        kOne = 0x00000001,
        kSrcColor = 0x00000002,
        kOneMinusSrcColor = 0x00000003,
        kSrcAlpha = 0x00000004,
        kOneMinusSrcAlpha = 0x00000005,
        kDestAlpha = 0x00000006,
        kOneMinusDestAlpha = 0x00000007,
        kDestColor = 0x00000008,
        kOneMinusDestColor = 0x00000009,
        kSrcAlphaSaturate = 0x0000000a,
        kConstantColor = 0x0000000d,
        kOneMinusConstantColor = 0x0000000e,
        kSrc1Color = 0x0000000f,
        kInverseSrc1Color = 0x00000010,
        kSrc1Alpha = 0x00000011,
        kInverseSrc1Alpha = 0x00000012,
        kConstantAlpha = 0x00000013,
        kOneMinusConstantAlpha = 0x00000014,
        kBitMask = 0x0000001f,
    }

    /// <summary>ColorDestMultiplier field values.</summary>
    public enum ColorDestMultiplier : uint
    {
        kZero = 0x00000000,
        kOne = 0x00000100,
        kSrcColor = 0x00000200,
        kOneMinusSrcColor = 0x00000300,
        kSrcAlpha = 0x00000400,
        kOneMinusSrcAlpha = 0x00000500,
        kDestAlpha = 0x00000600,
        kOneMinusDestAlpha = 0x00000700,
        kDestColor = 0x00000800,
        kOneMinusDestColor = 0x00000900,
        kSrcAlphaSaturate = 0x00000a00,
        kConstantColor = 0x00000d00,
        kOneMinusConstantColor = 0x00000e00,
        kSrc1Color = 0x00000f00,
        kInverseSrc1Color = 0x00001000,
        kSrc1Alpha = 0x00001100,
        kInverseSrc1Alpha = 0x00001200,
        kConstantAlpha = 0x00001300,
        kOneMinusConstantAlpha = 0x00001400,
        kBitMask = 0x00001f00,
    }

    /// <summary>AlphaSourceMultiplier field values.</summary>
    public enum AlphaSourceMultiplier : uint
    {
        kZero = 0x00000000,
        kOne = 0x00010000,
        kSrcColor = 0x00020000,
        kOneMinusSrcColor = 0x00030000,
        kSrcAlpha = 0x00040000,
        kOneMinusSrcAlpha = 0x00050000,
        kDestAlpha = 0x00060000,
        kOneMinusDestAlpha = 0x00070000,
        kDestColor = 0x00080000,
        kOneMinusDestColor = 0x00090000,
        kSrcAlphaSaturate = 0x000a0000,
        kConstantColor = 0x000d0000,
        kOneMinusConstantColor = 0x000e0000,
        kSrc1Color = 0x000f0000,
        kInverseSrc1Color = 0x00100000,
        kSrc1Alpha = 0x00110000,
        kInverseSrc1Alpha = 0x00120000,
        kConstantAlpha = 0x00130000,
        kOneMinusConstantAlpha = 0x00140000,
        kBitMask = 0x001f0000,
    }

    /// <summary>AlphaDestMultiplier field values.</summary>
    public enum AlphaDestMultiplier : uint
    {
        kZero = 0x00000000,
        kOne = 0x01000000,
        kSrcColor = 0x02000000,
        kOneMinusSrcColor = 0x03000000,
        kSrcAlpha = 0x04000000,
        kOneMinusSrcAlpha = 0x05000000,
        kDestAlpha = 0x06000000,
        kOneMinusDestAlpha = 0x07000000,
        kDestColor = 0x08000000,
        kOneMinusDestColor = 0x09000000,
        kSrcAlphaSaturate = 0x0a000000,
        kConstantColor = 0x0d000000,
        kOneMinusConstantColor = 0x0e000000,
        kSrc1Color = 0x0f000000,
        kInverseSrc1Color = 0x10000000,
        kSrc1Alpha = 0x11000000,
        kInverseSrc1Alpha = 0x12000000,
        kConstantAlpha = 0x13000000,
        kOneMinusConstantAlpha = 0x14000000,
        kBitMask = 0x1f000000,
    }

    /// <summary>ColorBlendFunc field values.</summary>
    public enum ColorBlendFunc : uint
    {
        kAdd = 0x00000000,
        kSubtract = 0x00000020,
        kMin = 0x00000040,
        kMax = 0x00000060,
        kReverseSubtract = 0x00000080,
        kBitMask = 0x000000e0,
    }

    /// <summary>AlphaBlendFunc field values.</summary>
    public enum AlphaBlendFunc : uint
    {
        kAdd = 0x00000000,
        kSubtract = 0x00200000,
        kMin = 0x00400000,
        kMax = 0x00600000,
        kReverseSubtract = 0x00800000,
        kBitMask = 0x00e00000,
    }

    /// <summary>Selects which of the eight hardware slots this block targets, by shifting the register offset.</summary>
    /// <exception cref="ArgumentOutOfRangeException">The slot is not in [0, 7].</exception>
    public CxBlendControl SetSlot(uint slot)
    {
        if (slot >= 8) throw new ArgumentOutOfRangeException(nameof(slot), "Slot must be in [0, 7].");
        _regs[0].Offset = (ushort)(_defaultOffsets[0] + slot);
        return this;
    }
    /// <summary>Returns the slot selected by <see cref="SetSlot"/>.</summary>
    public uint GetSlot() => (uint)(_regs[0].Offset - _defaultOffsets[0]);

    /// <summary>Sets the Blend field.</summary>
    public CxBlendControl SetBlend(Blend value) { _regs[0].Value = (_regs[0].Value & ~(uint)Blend.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the Blend field.</summary>
    public Blend GetBlend() => (Blend)(_regs[0].Value & (uint)Blend.kBitMask);

    /// <summary>Sets the SeparateAlphaBlend field.</summary>
    public CxBlendControl SetSeparateAlphaBlend(SeparateAlphaBlend value) { _regs[0].Value = (_regs[0].Value & ~(uint)SeparateAlphaBlend.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the SeparateAlphaBlend field.</summary>
    public SeparateAlphaBlend GetSeparateAlphaBlend() => (SeparateAlphaBlend)(_regs[0].Value & (uint)SeparateAlphaBlend.kBitMask);

    /// <summary>Sets the ColorSourceMultiplier field.</summary>
    public CxBlendControl SetColorSourceMultiplier(ColorSourceMultiplier value) { _regs[0].Value = (_regs[0].Value & ~(uint)ColorSourceMultiplier.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ColorSourceMultiplier field.</summary>
    public ColorSourceMultiplier GetColorSourceMultiplier() => (ColorSourceMultiplier)(_regs[0].Value & (uint)ColorSourceMultiplier.kBitMask);

    /// <summary>Sets the ColorDestMultiplier field.</summary>
    public CxBlendControl SetColorDestMultiplier(ColorDestMultiplier value) { _regs[0].Value = (_regs[0].Value & ~(uint)ColorDestMultiplier.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ColorDestMultiplier field.</summary>
    public ColorDestMultiplier GetColorDestMultiplier() => (ColorDestMultiplier)(_regs[0].Value & (uint)ColorDestMultiplier.kBitMask);

    /// <summary>Sets the AlphaSourceMultiplier field.</summary>
    public CxBlendControl SetAlphaSourceMultiplier(AlphaSourceMultiplier value) { _regs[0].Value = (_regs[0].Value & ~(uint)AlphaSourceMultiplier.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the AlphaSourceMultiplier field.</summary>
    public AlphaSourceMultiplier GetAlphaSourceMultiplier() => (AlphaSourceMultiplier)(_regs[0].Value & (uint)AlphaSourceMultiplier.kBitMask);

    /// <summary>Sets the AlphaDestMultiplier field.</summary>
    public CxBlendControl SetAlphaDestMultiplier(AlphaDestMultiplier value) { _regs[0].Value = (_regs[0].Value & ~(uint)AlphaDestMultiplier.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the AlphaDestMultiplier field.</summary>
    public AlphaDestMultiplier GetAlphaDestMultiplier() => (AlphaDestMultiplier)(_regs[0].Value & (uint)AlphaDestMultiplier.kBitMask);

    /// <summary>Sets the ColorBlendFunc field.</summary>
    public CxBlendControl SetColorBlendFunc(ColorBlendFunc value) { _regs[0].Value = (_regs[0].Value & ~(uint)ColorBlendFunc.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the ColorBlendFunc field.</summary>
    public ColorBlendFunc GetColorBlendFunc() => (ColorBlendFunc)(_regs[0].Value & (uint)ColorBlendFunc.kBitMask);

    /// <summary>Sets the AlphaBlendFunc field.</summary>
    public CxBlendControl SetAlphaBlendFunc(AlphaBlendFunc value) { _regs[0].Value = (_regs[0].Value & ~(uint)AlphaBlendFunc.kBitMask) | (uint)value; return this; }
    /// <summary>Gets the AlphaBlendFunc field.</summary>
    public AlphaBlendFunc GetAlphaBlendFunc() => (AlphaBlendFunc)(_regs[0].Value & (uint)AlphaBlendFunc.kBitMask);

}

/// <summary>
/// The constant blend colour the blend multipliers reference (the constant-colour and constant-alpha
/// factors). Four floating-point components, red green blue alpha.
/// </summary>
/// <remarks>
/// The register offset(s) and reset values load from the driver defaults through <see cref="Init"/>,
/// exactly as the graphics context register definitions do; construct the block, <see cref="Init"/> it,
/// then apply the setters and write <see cref="Registers"/> into a command buffer.
/// </remarks>
public sealed class CxBlendColor
{
    /// <summary>The number of registers in the block.</summary>
    public const int RegisterCount = 4;

    private readonly CxRegister[] _regs = new CxRegister[RegisterCount];
    private readonly ushort[] _defaultOffsets = new ushort[RegisterCount];

    /// <summary>The register values and offsets, ready to write into a command buffer.</summary>
    public ReadOnlySpan<CxRegister> Registers => _regs;

    /// <summary>Reads or writes a raw register by index.</summary>
    public CxRegister this[int index] { get => _regs[index]; set => _regs[index] = value; }

    /// <summary>Loads the register offsets and reset values from the driver-provided defaults.</summary>
    /// <exception cref="ArgumentException">Fewer than 4 defaults were provided.</exception>
    public CxBlendColor Init(ReadOnlySpan<CxRegister> defaults)
    {
        if (defaults.Length < RegisterCount)
            throw new ArgumentException($"Expected at least {RegisterCount} register defaults.", nameof(defaults));
        for (int i = 0; i < RegisterCount; i++) { _regs[i] = defaults[i]; _defaultOffsets[i] = defaults[i].Offset; }
        return this;
    }

    // The four registers load in ascending offset order (red, green, blue, alpha at consecutive
    // offsets), matching how the block reads its defaults; each component addresses its own register
    // by that position.

    /// <summary>Sets the Red component.</summary>
    public CxBlendColor SetRed(float value) { _regs[0].Value = BitConverter.SingleToUInt32Bits(value); return this; }
    /// <summary>Gets the Red component.</summary>
    public float GetRed() => BitConverter.UInt32BitsToSingle(_regs[0].Value);

    /// <summary>Sets the Green component.</summary>
    public CxBlendColor SetGreen(float value) { _regs[1].Value = BitConverter.SingleToUInt32Bits(value); return this; }
    /// <summary>Gets the Green component.</summary>
    public float GetGreen() => BitConverter.UInt32BitsToSingle(_regs[1].Value);

    /// <summary>Sets the Blue component.</summary>
    public CxBlendColor SetBlue(float value) { _regs[2].Value = BitConverter.SingleToUInt32Bits(value); return this; }
    /// <summary>Gets the Blue component.</summary>
    public float GetBlue() => BitConverter.UInt32BitsToSingle(_regs[2].Value);

    /// <summary>Sets the Alpha component.</summary>
    public CxBlendColor SetAlpha(float value) { _regs[3].Value = BitConverter.SingleToUInt32Bits(value); return this; }
    /// <summary>Gets the Alpha component.</summary>
    public float GetAlpha() => BitConverter.UInt32BitsToSingle(_regs[3].Value);

}
