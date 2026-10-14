// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.AvCapture;

/// <summary>
/// Video-capture configuration passed to open a capture channel. The service reads a small number of
/// fields and treats the rest as reserved; leave the reserved space zeroed. Only the named fields are
/// interpreted here, so build one with <see cref="Create"/> and set what you need.
/// </summary>
/// <remarks>
/// This is an advanced surface. The full field set is service-internal, and the whole of it is sent to
/// the service as configuration - all 0x2B8 bytes - so a block declared shorter than that ships
/// whatever happens to follow it. Only the fields named below are read on this side of the call.
/// </remarks>
[StructLayout(LayoutKind.Explicit, Size = 0x2B8)]
public struct Avcap2VideoConfig
{
    /// <summary>The channel kind the open path selects on (a non-zero value opens the primary channel).</summary>
    [FieldOffset(0x10)]
    public int Kind;

    /// <summary>A sub-mode the open path branches on; zero for the default.</summary>
    [FieldOffset(0x188)]
    public int Mode;

    /// <summary>
    /// The first area's length. Required together with <see cref="AreaAddress"/> when <see cref="Mode"/>
    /// is one, and required to be either both set or both clear when it is two; the open path refuses
    /// the call otherwise. Not read for the default mode.
    /// </summary>
    [FieldOffset(0x1A0)]
    public ulong AreaLength;

    /// <summary>The first area's address. See <see cref="AreaLength"/>.</summary>
    [FieldOffset(0x1A8)]
    public ulong AreaAddress;

    /// <summary>
    /// How many bytes of the frame area below are wanted. The open path measures the mapping at
    /// <see cref="FrameAreaAddress"/> and refuses the call when it holds less than this. Required
    /// whenever <see cref="Mode"/> is one or two.
    /// </summary>
    [FieldOffset(0x1C0)]
    public ulong FrameAreaLength;

    /// <summary>
    /// The address of the memory the frames are written into. It has to be mapped, and mapped as
    /// direct memory, before the call. See <see cref="FrameAreaLength"/>.
    /// </summary>
    [FieldOffset(0x1C8)]
    public ulong FrameAreaAddress;

    /// <summary>A configuration with the reserved space zeroed.</summary>
    public static Avcap2VideoConfig Create() => default;
}

/// <summary>
/// A captured video frame descriptor filled by a read. The leading words carry the frame's plane
/// addresses in the capture shared memory, its presentation time, and its geometry; those are an
/// advanced, service-internal layout. The status fields below are the ones a caller checks: a frame is
/// valid when <see cref="Result"/> is non-negative and both status bytes are zero.
/// </summary>
[StructLayout(LayoutKind.Explicit, Size = 0x68)]
public struct Avcap2VideoFrameInfo
{
    /// <summary>Descriptor word 0 (frame plane address or timestamp; advanced).</summary>
    [FieldOffset(0x00)] public ulong Word0;

    /// <summary>Descriptor word 1 (advanced).</summary>
    [FieldOffset(0x08)] public ulong Word1;

    /// <summary>Descriptor word 2 (advanced).</summary>
    [FieldOffset(0x10)] public ulong Word2;

    /// <summary>Descriptor word 3 (advanced).</summary>
    [FieldOffset(0x18)] public ulong Word3;

    /// <summary>Descriptor word 4 (advanced).</summary>
    [FieldOffset(0x20)] public ulong Word4;

    /// <summary>Descriptor word 5 (advanced).</summary>
    [FieldOffset(0x28)] public ulong Word5;

    /// <summary>Descriptor word 6 (advanced).</summary>
    [FieldOffset(0x30)] public ulong Word6;

    /// <summary>Descriptor word 7 (advanced).</summary>
    [FieldOffset(0x38)] public ulong Word7;

    /// <summary>Descriptor word 8 (advanced).</summary>
    [FieldOffset(0x40)] public ulong Word8;

    /// <summary>Descriptor word 9 (advanced).</summary>
    [FieldOffset(0x48)] public ulong Word9;

    /// <summary>Descriptor word 10 (advanced).</summary>
    [FieldOffset(0x50)] public ulong Word10;

    /// <summary>A per-frame status byte the read checks; a valid frame has this zero.</summary>
    [FieldOffset(0x59)] public byte StatusA;

    /// <summary>The per-frame result code the read stores; non-negative for a delivered frame.</summary>
    [FieldOffset(0x5C)] public int Result;

    /// <summary>A second per-frame status byte the read checks; a valid frame has this zero.</summary>
    [FieldOffset(0x64)] public byte StatusB;

    /// <summary>Whether the descriptor names a delivered, usable frame.</summary>
    public readonly bool IsValid => Result >= 0 && StatusA == 0 && StatusB == 0;
}

/// <summary>
/// Error codes the AV-capture service returns. The values share the <c>0x8195_xxxx</c> facility. Only
/// the argument error is named specifically; other negative results are service states a caller reports
/// through the exception message.
/// </summary>
public static class Avcap2Error
{
    /// <summary>The facility base for every code the service returns.</summary>
    public const int FacilityBase = unchecked((int)0x81950000);

    /// <summary>An argument was null or otherwise rejected before the request was sent.</summary>
    public const int InvalidArgument = unchecked((int)0x81950001);
}
