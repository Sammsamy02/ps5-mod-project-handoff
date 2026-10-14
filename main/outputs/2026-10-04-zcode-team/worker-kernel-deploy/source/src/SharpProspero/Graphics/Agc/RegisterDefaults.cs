// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Agc;
using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The graphics driver's register reset values. A register block takes its reset values from the driver
/// rather than from a baked-in table, because they depend on the graphics processor configuration. This
/// reads that table once, looks a value up by its register offset, and assembles the blocks the context
/// register types in this namespace initialise from.
/// </summary>
public static unsafe class RegisterDefaults
{
    // The driver answers with a descriptor. Its first three words are the three register spaces
    // (context, shader, user config); each is a table of pointers, one per block, into a single flat
    // array of eight-byte {offset, value} records. The word at +0x20 is how many records the context
    // array holds - not how many blocks the table holds, which is far fewer. The first block starts at
    // the array, so the first table entry is where the array begins.
    private const int ContextSpaceOffset = 0x00;
    private const int ContextRecordCountOffset = 0x20;

    private static byte* _descriptor;

    // The sixteen context registers of a colour render-target block, in the order the block expects.
    private static readonly ushort[] RenderTargetOffsets =
    [
        0x0318, 0x031B, 0x031C, 0x031D, 0x031E, 0x031F, 0x0321, 0x0323,
        0x0324, 0x0325, 0x0390, 0x0398, 0x03A0, 0x03A8, 0x03B0, 0x03B8,
    ];

    // The four blend-colour registers, in red, green, blue, alpha order.
    private static readonly ushort[] BlendColorOffsets = [0x0105, 0x0106, 0x0107, 0x0108];

    // Each of these blocks is a single register: the blend equation of colour target zero (the seven
    // other targets follow it, which is what the block's slot selector shifts to), the depth and
    // stencil test controls, the front and back stencil masks, and the stencil operations.
    private static readonly ushort[] BlendControlOffsets = [0x01E0];
    private static readonly ushort[] DepthStencilControlOffsets = [0x0200];
    private static readonly ushort[] StencilOpControlOffsets = [0x010B];
    private static readonly ushort[] StencilControlOffsets = [0x010C];
    private static readonly ushort[] StencilControlBackFaceOffsets = [0x010D];

    private static void Ensure()
    {
        if (_descriptor is null) _descriptor = (byte*)SceAgc.sceAgcGetRegisterDefaults();
    }

    /// <summary>The reset value the driver holds for a context register, or zero if it lists none.</summary>
    public static uint GetContextValue(ushort offset)
    {
        Ensure();
        // The driver answers with nothing at all when it holds no defaults, so this can be absent.
        if (_descriptor is null) return 0;

        CxRegister** blocks = *(CxRegister***)(_descriptor + ContextSpaceOffset);
        uint count = *(uint*)(_descriptor + ContextRecordCountOffset);
        if (blocks is null || count == 0) return 0;

        // Walk the record array rather than the block table: the count belongs to the array, so
        // stepping the table by it reads far past the table's end, and a block pointer reaches only
        // that block's first register, which hides every other register in the block. Each offset
        // appears once in the array, so a match is the answer.
        CxRegister* records = blocks[0];
        for (uint i = 0; i < count; i++)
            if (records[i].Offset == offset) return records[i].Value;
        return 0;
    }

    /// <summary>
    /// The sixteen colour render-target registers, each carrying its offset and the driver's reset value,
    /// ready to hand to <see cref="CxRenderTarget.Init"/>.
    /// </summary>
    public static CxRegister[] RenderTargetBlock() => Block(RenderTargetOffsets);

    /// <summary>The blend-equation register of colour target zero, ready for <see cref="CxBlendControl.Init"/>.</summary>
    public static CxRegister[] BlendControlBlock() => Block(BlendControlOffsets);

    /// <summary>The four constant blend-colour registers, ready for <see cref="CxBlendColor.Init"/>.</summary>
    public static CxRegister[] BlendColorBlock() => Block(BlendColorOffsets);

    /// <summary>The depth and stencil test control register, ready for <see cref="CxDepthStencilControl.Init"/>.</summary>
    public static CxRegister[] DepthStencilControlBlock() => Block(DepthStencilControlOffsets);

    /// <summary>The front-face stencil mask register, ready for <see cref="CxStencilControl.Init"/>.</summary>
    public static CxRegister[] StencilControlBlock() => Block(StencilControlOffsets);

    /// <summary>The back-face stencil mask register, ready for <see cref="CxStencilControlBackFace.Init"/>.</summary>
    public static CxRegister[] StencilControlBackFaceBlock() => Block(StencilControlBackFaceOffsets);

    /// <summary>The stencil operation register, ready for <see cref="CxStencilOpControl.Init"/>.</summary>
    public static CxRegister[] StencilOpControlBlock() => Block(StencilOpControlOffsets);

    private static CxRegister[] Block(ReadOnlySpan<ushort> offsets)
    {
        var block = new CxRegister[offsets.Length];
        for (int i = 0; i < block.Length; i++)
            block[i] = new CxRegister(offsets[i], GetContextValue(offsets[i]));
        return block;
    }
}
