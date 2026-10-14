// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The viewport transform and scissor for a draw: it maps the clip-space cube the shader outputs onto a
/// rectangle of the render target and clips anything outside a pixel rectangle. Set the viewport rectangle
/// and depth range and the scissor, then record the register writes it produces into a
/// <see cref="DrawCommandBuffer"/> with <see cref="DrawCommandBuffer.SetContextRegister"/> before the draw.
/// Without this nothing maps clip space to pixels and nothing is clipped.
/// </summary>
/// <remarks>
/// The register offsets, defaults, and bit layout are the graphics-processor primitive-assembler context
/// registers for viewport zero. The vertical scale is negated so clip space with y upward lands on a
/// target whose rows count downward from the top; pass a positive height and the transform handles it. The
/// guardband adjustments default to one (no adjustment). This covers the single-viewport case that scan-out
/// rendering uses.
/// </remarks>
public struct AgcViewport
{
    // Context-register offsets for viewport 0, read from the module's default-context init table and
    // matching the graphics-processor primitive-assembler register map.
    private const ushort RegXScale = 0x10F, RegXOffset = 0x110, RegYScale = 0x111, RegYOffset = 0x112, RegZScale = 0x113, RegZOffset = 0x114;
    private const ushort RegZMin = 0x0B4, RegZMax = 0x0B5;
    private const ushort RegGbVertClip = 0x2FA, RegGbVertDisc = 0x2FB, RegGbHorzClip = 0x2FC, RegGbHorzDisc = 0x2FD;
    private const ushort RegScissorTL = 0x090, RegScissorBR = 0x091;
    private const uint WindowOffsetDisable = 0x80000000u;

    /// <summary>The number of context registers the block writes.</summary>
    public const int RegisterCount = 14;

    private float _x, _y, _width, _height, _minDepth, _maxDepth;
    private float _gbVertClip, _gbVertDisc, _gbHorzClip, _gbHorzDisc;
    private int _scissorLeft, _scissorTop, _scissorRight, _scissorBottom;

    /// <summary>Creates a viewport covering a unit rectangle with the full depth range and no guardband adjustment.</summary>
    public AgcViewport()
    {
        _width = 1f;
        _height = 1f;
        _maxDepth = 1f;
        _gbVertClip = _gbVertDisc = _gbHorzClip = _gbHorzDisc = 1f;
        _scissorRight = 0x4000;
        _scissorBottom = 0x4000;
    }

    /// <summary>Sets the viewport rectangle in pixels and the depth range (0 to 1 by default).</summary>
    public void SetViewport(float x, float y, float width, float height, float minDepth = 0f, float maxDepth = 1f)
    {
        _x = x; _y = y; _width = width; _height = height; _minDepth = minDepth; _maxDepth = maxDepth;
    }

    /// <summary>Sets the scissor rectangle in pixels; pixels outside it are not drawn.</summary>
    public void SetScissor(int left, int top, int right, int bottom)
    {
        _scissorLeft = left; _scissorTop = top; _scissorRight = right; _scissorBottom = bottom;
    }

    /// <summary>Sets the guardband clip and discard adjustments (one is no adjustment).</summary>
    public void SetGuardband(float verticalClip, float verticalDiscard, float horizontalClip, float horizontalDiscard)
    {
        _gbVertClip = verticalClip; _gbVertDisc = verticalDiscard; _gbHorzClip = horizontalClip; _gbHorzDisc = horizontalDiscard;
    }

    /// <summary>Writes the context registers into <paramref name="destination"/>, which must hold at least <see cref="RegisterCount"/>.</summary>
    /// <returns>The number of registers written.</returns>
    public readonly int WriteTo(Span<CxRegister> destination)
    {
        if (destination.Length < RegisterCount)
            throw new ArgumentException($"A viewport block needs {RegisterCount} registers.", nameof(destination));

        // The transform maps clip space [-1, 1] onto the pixel rectangle; the vertical scale is negated so
        // a clip-space y that points up lands on a top-down target.
        float xScale = _width * 0.5f;
        float xOffset = _x + xScale;
        float yScale = -_height * 0.5f;
        float yOffset = _y + (_height * 0.5f);
        float zScale = _maxDepth - _minDepth;
        float zOffset = _minDepth;

        int i = 0;
        destination[i++] = Float(RegXScale, xScale);
        destination[i++] = Float(RegXOffset, xOffset);
        destination[i++] = Float(RegYScale, yScale);
        destination[i++] = Float(RegYOffset, yOffset);
        destination[i++] = Float(RegZScale, zScale);
        destination[i++] = Float(RegZOffset, zOffset);
        destination[i++] = Float(RegZMin, _minDepth);
        destination[i++] = Float(RegZMax, _maxDepth);
        destination[i++] = Float(RegGbVertClip, _gbVertClip);
        destination[i++] = Float(RegGbVertDisc, _gbVertDisc);
        destination[i++] = Float(RegGbHorzClip, _gbHorzClip);
        destination[i++] = Float(RegGbHorzDisc, _gbHorzDisc);
        destination[i++] = new CxRegister(RegScissorTL, Corner(_scissorLeft, _scissorTop) | WindowOffsetDisable);
        destination[i++] = new CxRegister(RegScissorBR, Corner(_scissorRight, _scissorBottom));
        return i;
    }

    /// <summary>The context registers the block writes, as a new array.</summary>
    public readonly CxRegister[] ToRegisters()
    {
        var registers = new CxRegister[RegisterCount];
        WriteTo(registers);
        return registers;
    }

    // A scissor corner packs an unsigned 15-bit x in the low half and a 15-bit y in the high half.
    private static uint Corner(int x, int y)
        => ((uint)Math.Clamp(x, 0, 0x7FFF)) | (((uint)Math.Clamp(y, 0, 0x7FFF)) << 16);

    private static CxRegister Float(ushort offset, float value) => new(offset, BitConverter.SingleToUInt32Bits(value));
}
