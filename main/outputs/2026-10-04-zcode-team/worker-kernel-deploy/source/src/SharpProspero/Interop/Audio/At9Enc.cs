// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Audio;

/// <summary>The parameters an ATRAC9 encoder is created from.</summary>
/// <remarks>Set <c>Size</c> to the size of this structure. The encoder returns its ATRAC9 config bytes in
/// <c>ConfigData</c>, which a container writes so a decoder can be set up to match.</remarks>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceAt9EncParam
{
    /// <summary>The size of this structure in bytes.</summary>
    public uint Size;
    /// <summary>Number of channels.</summary>
    public uint Channels;
    /// <summary>Input sample rate in hertz.</summary>
    public uint SamplingRate;
    /// <summary>Bit rate in bits per second.</summary>
    public uint BitRate;
    /// <summary>The input sample form (see <see cref="At9Enc.InputFormatSigned16"/>).</summary>
    public uint InputFormat;
    /// <summary>Super-frame setting.</summary>
    public uint SuperFrame;
    /// <summary>Dual-channel setting.</summary>
    public uint Dual;
    /// <summary>Low-cut of the low-frequency-effects channel.</summary>
    public uint Slc;
    /// <summary>Encode bands.</summary>
    public int NBands;
    /// <summary>Intensity band.</summary>
    public int IsBand;
    /// <summary>Encoding mode.</summary>
    public int GradMode;
    /// <summary>Encode-zone priority.</summary>
    public uint WBand;
    /// <summary>The ATRAC9 config bytes the encoder fills in.</summary>
    public fixed byte ConfigData[4];
}

/// <summary>
/// ATRAC9 encoding - the device's own compressed audio form. The caller sizes and owns the encoder work
/// area: query its size, allocate it (8-byte aligned), create the encoder into it, then encode frame by
/// frame. The encoder reports its ATRAC9 config bytes so a container can record them.
/// </summary>
public static unsafe partial class At9Enc
{
    private const string Lib = "libSceAt9Enc";

    /// <summary>Signed 16-bit input samples.</summary>
    public const int InputFormatSigned16 = 2;
    /// <summary>32-bit floating-point input samples.</summary>
    public const int InputFormatFloat = 4;
    /// <summary>Super-frame off.</summary>
    public const int SuperFrameOff = 1;
    /// <summary>Super-frame on.</summary>
    public const int SuperFrameOn = 4;
    /// <summary>The required alignment of the work area, in bytes.</summary>
    public const int WorkAreaAlignment = 8;

    /// <summary>Reports the work-area size a given parameter set needs.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAt9EncQueryMemSize(SceAt9EncParam* pAt9EncParam, int* pMemSize, int* pInternalResult);

    /// <summary>Creates an encoder inside the caller's work area.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAt9EncCreateEncoder(void* pAt9EncWorkArea, SceAt9EncParam* pAt9EncParam, int memSize, int* pInternalResult);

    /// <summary>Encodes a frame; reports the encoded byte count and how much input it consumed.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAt9EncEncode(void* pAt9EncWorkArea, void* pInputBuf, void* pOutputBuf, uint inputBufSize, uint outputBufSize, uint* pOutputDataByte, uint* pUsedBufSize, int* pInternalResult);

    /// <summary>Emits any buffered trailing data at the end of the stream.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAt9EncFlush(void* pAt9EncWorkArea, void* pOutputBuf, uint outputBufSize, uint* pOutputDataByte, int* pInternalResult);

    /// <summary>Drops the encoder's inter-frame state.</summary>
    [LibraryImport(Lib)]
    public static partial int sceAt9EncClearContext(void* pAt9EncWorkArea, int* pInternalResult);
}
