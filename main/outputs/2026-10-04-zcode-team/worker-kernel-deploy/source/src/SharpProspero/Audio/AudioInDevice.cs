// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Audio;
using System;

namespace SharpProspero.Audio;

/// <summary>
/// A microphone input port that captures 16-bit samples, for a voice recorder, a level meter, or speech
/// input. Open it once, then pull one block of samples per call; each call blocks until a block has been
/// captured, which paces the caller to the audio clock. Read into a buffer of <see cref="SamplesPerBlock"/>
/// shorts.
/// </summary>
/// <example>
/// <code>
/// using var mic = AudioInDevice.OpenMicrophone(userId);
/// short[] block = new short[mic.SamplesPerBlock];
/// mic.Read(block);
/// </code>
/// </example>
public sealed unsafe class AudioInDevice : IDisposable
{
    private readonly int _handle;
    private readonly uint _grain;
    private readonly int _channels;
    private bool _disposed;

    private AudioInDevice(int handle, uint grain, int channels)
    {
        _handle = handle;
        _grain = grain;
        _channels = channels;
    }

    /// <summary>Samples in one captured block: the grain times the channel count.</summary>
    public int SamplesPerBlock => (int)_grain * _channels;

    /// <summary>Frames (per-channel samples) in one captured block.</summary>
    public uint Grain => _grain;

    /// <summary>Whether the port captures one channel or two.</summary>
    public int Channels => _channels;

    /// <summary>
    /// Opens the microphone as a 16-bit port. <paramref name="grain"/> is the samples per block (128 or
    /// 256), <paramref name="sampleRate"/> the sample rate in hertz, <paramref name="stereo"/> selects
    /// two channels, and <paramref name="type"/> the capture purpose.
    /// </summary>
    /// <exception cref="ProsperoException">Opening the port failed.</exception>
    public static AudioInDevice OpenMicrophone(
        int userId,
        uint grain = AudioIn.Grain256,
        uint sampleRate = AudioIn.Freq16k,
        bool stereo = false,
        AudioInType type = AudioInType.General)
    {
        AudioInFormat format = stereo ? AudioInFormat.S16Stereo : AudioInFormat.S16Mono;
        int handle = AudioIn.sceAudioInOpen(userId, (int)type, 0, grain, sampleRate, (uint)format);
        SceResult.ThrowIfFailed(handle, nameof(AudioIn.sceAudioInOpen));
        return new AudioInDevice(handle, grain, stereo ? 2 : 1);
    }

    /// <summary>
    /// Captures one block into <paramref name="samples"/> and blocks until it is filled. The span must
    /// hold at least <see cref="SamplesPerBlock"/> shorts.
    /// </summary>
    /// <exception cref="ProsperoException">The capture call failed.</exception>
    public void Read(Span<short> samples)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (samples.Length < SamplesPerBlock)
            throw new ArgumentException($"A block needs at least {SamplesPerBlock} samples.", nameof(samples));
        fixed (short* p = samples)
            SceResult.ThrowIfFailed(AudioIn.sceAudioInInput(_handle, p), nameof(AudioIn.sceAudioInInput));
    }

    /// <summary>True when the input is currently silent, muted at the hardware or by the system.</summary>
    public bool IsSilent
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            int state = AudioIn.sceAudioInGetSilentState(_handle);
            return state > 0;
        }
    }

    /// <summary>Closes the port.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        AudioIn.sceAudioInClose(_handle);
    }
}
