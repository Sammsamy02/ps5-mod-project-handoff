// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Audio;

/// <summary>The sample form the Advanced Audio Coding encoder reads.</summary>
public enum M4aacEncInputFormat
{
    /// <summary>Signed 16-bit samples.</summary>
    Signed16 = 0,
    /// <summary>32-bit floating-point samples.</summary>
    Float = 1,
}

/// <summary>The stream form the Advanced Audio Coding encoder writes.</summary>
public enum M4aacEncOutputFormat : uint
{
    /// <summary>Raw AAC-LC blocks with no per-frame header.</summary>
    AacLcRaw = 0x00000000,
    /// <summary>AAC-LC in an ADTS stream (each frame carries its own header).</summary>
    AacLcAdts = 0x00000001,
    /// <summary>Raw AAC-LC dual-mono blocks.</summary>
    AacLcRawDualMono = 0x00010000,
    /// <summary>AAC-LC dual-mono in an ADTS stream.</summary>
    AacLcAdtsDualMono = 0x00010001,
}

/// <summary>
/// Advanced Audio Coding (AAC-LC) encoding. An encoder is created for a channel count, sample rate, and
/// bit rate, then each call encodes one frame of samples into a compressed block. A frame is 1024 samples
/// per channel.
/// </summary>
public static unsafe partial class M4aacEnc
{
    private const string Lib = "libSceM4aacEnc";

    /// <summary>One mono channel.</summary>
    public const int ChannelMono = 1;
    /// <summary>Two channels (stereo).</summary>
    public const int ChannelStereo = 2;
    /// <summary>The sample rate the encoder accepts, in hertz.</summary>
    public const int SamplingRate48K = 48000;
    /// <summary>The lowest supported bit rate, in bits per second.</summary>
    public const int MinBitRate = 28000;
    /// <summary>The highest supported bit rate, in bits per second.</summary>
    public const int MaxBitRate = 320000;
    /// <summary>Samples per frame per channel.</summary>
    public const int FrameSamples = 1024;
    /// <summary>The largest a single encoded block can be, in bytes.</summary>
    public const int MaxOutputBufferSize = 1536;

    /// <summary>Creates an encoder. Returns a non-negative handle, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceM4aacEncCreateEncoder(int channels, int samplingRate, int bitRate, int inputFormat, int outputFormat, int* pInternalError);

    /// <summary>Destroys an encoder.</summary>
    [LibraryImport(Lib)]
    public static partial int sceM4aacEncDeleteEncoder(int m4aacEncHandle, int* pInternalError);

    /// <summary>Encodes one frame of samples into <paramref name="pOutput"/>; writes the block size to <paramref name="pOutputByte"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceM4aacEncEncode(int m4aacEncHandle, void* pInput, uint inputByte, void* pOutput, uint* pOutputByte, int* pInternalError);

    /// <summary>Emits any buffered trailing block at the end of the stream.</summary>
    [LibraryImport(Lib)]
    public static partial int sceM4aacEncFlush(int m4aacEncHandle, void* pOutput, uint* pOutputByte, int* pInternalError);

    /// <summary>Drops the encoder's inter-frame state.</summary>
    [LibraryImport(Lib)]
    public static partial int sceM4aacEncClearContext(int m4aacEncHandle);
}
