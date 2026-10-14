// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Graphics.Agc;

// Block dimension, mip-tail, and swizzle-mode tables, taken verbatim from the graphics address
// library's declarations. A Block holds log2 of the block width, height, and depth in elements.
public static partial class AgcSurface
{
    internal readonly struct Block(int widthLog2, int heightLog2, int depthLog2)
    {
        public int WidthLog2 { get; } = widthLog2; public int HeightLog2 { get; } = heightLog2; public int DepthLog2 { get; } = depthLog2;
    }

    private static readonly Block[] BlockLinear = [new Block(8, 0, 0), new Block(7, 0, 0), new Block(6, 0, 0), new Block(5, 0, 0), new Block(4, 0, 0)];
    private static readonly Block[] BlockThin256B = [new Block(4, 4, 0), new Block(4, 3, 0), new Block(3, 3, 0), new Block(3, 2, 0), new Block(2, 2, 0)];
    private static readonly Block[] BlockThin4KB = [new Block(6, 6, 0), new Block(6, 5, 0), new Block(5, 5, 0), new Block(5, 4, 0), new Block(4, 4, 0)];
    private static readonly Block[] BlockThin64KB = [new Block(8, 8, 0), new Block(8, 7, 0), new Block(7, 7, 0), new Block(7, 6, 0), new Block(6, 6, 0)];
    private static readonly Block[] BlockThick256B = [new Block(3, 2, 3), new Block(2, 2, 3), new Block(2, 2, 2), new Block(2, 1, 2), new Block(1, 1, 2)];
    private static readonly Block[] BlockThick4KB = [new Block(4, 4, 4), new Block(3, 4, 4), new Block(3, 4, 3), new Block(3, 3, 3), new Block(2, 3, 3)];
    private static readonly Block[] BlockThick64KB = [new Block(6, 5, 5), new Block(5, 5, 5), new Block(5, 5, 4), new Block(5, 4, 4), new Block(4, 4, 4)];
    private static readonly Block[][] BlockMsaa =
    [
        [ new Block(8,8,0), new Block(8,7,0), new Block(7,7,0), new Block(7,6,0), new Block(6,6,0) ],
        [ new Block(7,8,0), new Block(7,7,0), new Block(6,7,0), new Block(6,6,0), new Block(5,6,0) ],
        [ new Block(7,7,0), new Block(7,6,0), new Block(6,6,0), new Block(6,5,0), new Block(5,5,0) ],
        [ new Block(6,7,0), new Block(6,6,0), new Block(5,6,0), new Block(5,5,0), new Block(4,5,0) ],
    ];

    private static readonly (uint X, uint Y)[][] MipTailThin4KB =
    [
        [ (32u,0u), (16u,32u), (0u,48u), (0u,32u), (16u,16u), (16u,0u), (0u,16u), (0u,0u) ],
        [ (32u,0u), (16u,16u), (0u,24u), (0u,16u), (16u,8u), (16u,0u), (0u,8u), (0u,0u) ],
        [ (16u,0u), (8u,16u), (0u,24u), (0u,16u), (8u,8u), (8u,0u), (0u,8u), (0u,0u) ],
        [ (16u,0u), (8u,8u), (0u,12u), (0u,8u), (8u,4u), (8u,0u), (0u,4u), (0u,0u) ],
        [ (8u,0u), (4u,8u), (0u,12u), (0u,8u), (4u,4u), (4u,0u), (0u,4u), (0u,0u) ],
    ];
    private static readonly (uint X, uint Y)[][] MipTailThin64KB =
    [
        [ (128u,0u), (0u,128u), (64u,0u), (0u,64u), (32u,0u), (16u,32u), (0u,48u), (0u,32u), (16u,16u), (16u,0u), (0u,16u), (0u,0u) ],
        [ (128u,0u), (0u,64u), (64u,0u), (0u,32u), (32u,0u), (16u,16u), (0u,24u), (0u,16u), (16u,8u), (16u,0u), (0u,8u), (0u,0u) ],
        [ (64u,0u), (0u,64u), (32u,0u), (0u,32u), (16u,0u), (8u,16u), (0u,24u), (0u,16u), (8u,8u), (8u,0u), (0u,8u), (0u,0u) ],
        [ (64u,0u), (0u,32u), (32u,0u), (0u,16u), (16u,0u), (8u,8u), (0u,12u), (0u,8u), (8u,4u), (8u,0u), (0u,4u), (0u,0u) ],
        [ (32u,0u), (0u,32u), (16u,0u), (0u,16u), (8u,0u), (4u,8u), (0u,12u), (0u,8u), (4u,4u), (4u,0u), (0u,4u), (0u,0u) ],
    ];
    private static readonly (uint X, uint Y)[][] MipTailThick4KB =
    [
        [ (0u,8u), (8u,4u), (8u,0u), (0u,4u), (0u,0u) ],
        [ (0u,8u), (4u,4u), (4u,0u), (0u,4u), (0u,0u) ],
        [ (0u,8u), (4u,4u), (4u,0u), (0u,4u), (0u,0u) ],
        [ (0u,4u), (4u,2u), (4u,0u), (0u,2u), (0u,0u) ],
        [ (0u,4u), (2u,2u), (2u,0u), (0u,2u), (0u,0u) ],
    ];
    private static readonly (uint X, uint Y)[][] MipTailThick64KB =
    [
        [ (32u,0u), (0u,16u), (16u,0u), (8u,8u), (0u,12u), (0u,8u), (8u,4u), (8u,0u), (0u,4u), (0u,0u) ],
        [ (16u,0u), (0u,16u), (8u,0u), (4u,8u), (0u,12u), (0u,8u), (4u,4u), (4u,0u), (0u,4u), (0u,0u) ],
        [ (16u,0u), (0u,16u), (8u,0u), (4u,8u), (0u,12u), (0u,8u), (4u,4u), (4u,0u), (0u,4u), (0u,0u) ],
        [ (16u,0u), (0u,8u), (8u,0u), (4u,4u), (0u,6u), (0u,4u), (4u,2u), (4u,0u), (0u,2u), (0u,0u) ],
        [ (8u,0u), (0u,8u), (4u,0u), (2u,4u), (0u,6u), (0u,4u), (2u,2u), (2u,0u), (0u,2u), (0u,0u) ],
    ];

    // Per swizzle-mode flags packed: bit0=256B, bit1=4KB, bit2=64KB, bit3=Z-order, bit4=Std, bit5=Disp.
    private static readonly byte[] SwizzleFlags = [0, 17, 33, 0, 0, 18, 34, 0, 0, 20, 36, 0, 0, 0, 0, 0, 0, 20, 36, 0, 0, 18, 34, 0, 12, 20, 36, 4, 0, 0, 0, 0, 0];
}
