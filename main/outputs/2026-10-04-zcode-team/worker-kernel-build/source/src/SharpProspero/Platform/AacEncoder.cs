// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Audio;
using System;

namespace SharpProspero.Platform;

/// <summary>Thrown when the audio encoder rejects a call, carrying the result and internal codes it returned.</summary>
/// <remarks>Creates the exception from the codes the encoder returned.</remarks>
public sealed class AudioEncodeException(string operation, int resultCode, int internalError) : Exception($"Audio encode {operation} failed (result 0x{resultCode:X8}, internal 0x{internalError:X8}).")
{

    /// <summary>The primary result code the encoder returned.</summary>
    public int ResultCode { get; } = resultCode;
    /// <summary>The encoder's internal error code.</summary>
    public int InternalError { get; } = internalError;
}

/// <summary>
/// Encodes signed-16 or floating-point PCM into AAC-LC, one 1024-sample frame at a time. Create the encoder
/// for a channel count and bit rate, feed it frames with <see cref="Encode"/>, and <see cref="Flush"/> at
/// the end. Dispose it to release the encoder.
/// </summary>
public sealed unsafe class AacEncoder : IDisposable
{
    private int _handle;
    private bool _disposed;

    private AacEncoder(int handle) => _handle = handle;

    /// <summary>The channel count the encoder was created for.</summary>
    public int Channels { get; private init; }

    /// <summary>
    /// Creates an AAC-LC encoder. <paramref name="channels"/> is 1 or 2, <paramref name="bitRate"/> is in
    /// bits per second within the supported range, and the sample rate is 48 kHz.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">A parameter is outside its supported range.</exception>
    /// <exception cref="AudioEncodeException">The encoder could not be created.</exception>
    public static AacEncoder Create(
        int channels,
        int bitRate,
        int sampleRate = M4aacEnc.SamplingRate48K,
        M4aacEncInputFormat inputFormat = M4aacEncInputFormat.Signed16,
        M4aacEncOutputFormat outputFormat = M4aacEncOutputFormat.AacLcAdts)
    {
        if (channels is not (M4aacEnc.ChannelMono or M4aacEnc.ChannelStereo))
            throw new ArgumentOutOfRangeException(nameof(channels), "AAC encoding supports 1 or 2 channels.");
        if (bitRate < M4aacEnc.MinBitRate || bitRate > M4aacEnc.MaxBitRate)
            throw new ArgumentOutOfRangeException(nameof(bitRate), $"Bit rate must be in [{M4aacEnc.MinBitRate}, {M4aacEnc.MaxBitRate}].");
        if (sampleRate != M4aacEnc.SamplingRate48K)
            throw new ArgumentOutOfRangeException(nameof(sampleRate), "AAC encoding uses a 48 kHz sample rate.");

        int internalError = 0;
        int handle = M4aacEnc.sceM4aacEncCreateEncoder(channels, sampleRate, bitRate, (int)inputFormat, (int)outputFormat, &internalError);
        if (handle < 0)
            throw new AudioEncodeException("create", handle, internalError);
        return new AacEncoder(handle) { Channels = channels };
    }

    /// <summary>
    /// Encodes one frame - 1024 samples per channel - from <paramref name="input"/> into
    /// <paramref name="output"/>, and returns the number of encoded bytes written.
    /// </summary>
    /// <exception cref="AudioEncodeException">The frame could not be encoded.</exception>
    public int Encode(ReadOnlySpan<byte> input, Span<byte> output)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        uint outputByte = 0;
        int internalError = 0;
        int result;
        fixed (byte* pIn = input)
        fixed (byte* pOut = output)
            result = M4aacEnc.sceM4aacEncEncode(_handle, pIn, (uint)input.Length, pOut, &outputByte, &internalError);
        if (result < 0)
            throw new AudioEncodeException("encode", result, internalError);
        return (int)outputByte;
    }

    /// <summary>Emits any final buffered block into <paramref name="output"/>, returning its byte count.</summary>
    /// <exception cref="AudioEncodeException">The flush failed.</exception>
    public int Flush(Span<byte> output)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        uint outputByte = 0;
        int internalError = 0;
        int result;
        fixed (byte* pOut = output)
            result = M4aacEnc.sceM4aacEncFlush(_handle, pOut, &outputByte, &internalError);
        if (result < 0)
            throw new AudioEncodeException("flush", result, internalError);
        return (int)outputByte;
    }

    /// <summary>Drops the encoder's inter-frame state, for restarting a stream.</summary>
    public void ClearContext()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        M4aacEnc.sceM4aacEncClearContext(_handle);
    }

    /// <summary>Releases the encoder.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        int internalError = 0;
        M4aacEnc.sceM4aacEncDeleteEncoder(_handle, &internalError);
        _handle = -1;
        _disposed = true;
        GC.SuppressFinalize(this);
    }

    /// <summary>Releases the encoder if it was not disposed.</summary>
    ~AacEncoder() => Dispose();
}
