// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Audio;
using System;

namespace SharpProspero.Audio;

/// <summary>
/// A stereo 16-bit audio-output port. Open it once, then push one block of interleaved samples per
/// call; each push blocks until there is room in the output queue for it, which paces the caller to the
/// audio clock. Fill a buffer of <see cref="SamplesPerBlock"/> shorts (left, right, left, right, …) and
/// pass it to <see cref="Output"/>.
/// </summary>
/// <remarks>
/// Because a push returns once the block is queued rather than once it has played, closing the port
/// straight after the last push cuts that block off. <see cref="Dispose"/> waits for it first; call
/// <see cref="Drain"/> directly to wait without closing.
/// </remarks>
public sealed unsafe class AudioOutDevice : IDisposable
{
    private const int Channels = 2;

    private readonly int _handle;
    private readonly uint _grain;
    private bool _disposed;

    private AudioOutDevice(int handle, uint grain)
    {
        _handle = handle;
        _grain = grain;
    }

    /// <summary>Samples in one output block: the grain times the two channels.</summary>
    public int SamplesPerBlock => (int)_grain * Channels;

    /// <summary>Frames (per-channel samples) in one output block.</summary>
    public uint Grain => _grain;

    /// <summary>
    /// Opens the main output as a stereo 16-bit port. <paramref name="grain"/> is the samples per
    /// block, in whole multiples of 256 from 256 to 2048; <paramref name="sampleRate"/> is the rate in
    /// hertz, which the main output takes at 48000 or 192000 and at nothing else.
    /// </summary>
    /// <exception cref="ProsperoException">Opening the port failed.</exception>
    public static AudioOutDevice OpenStereo(uint grain = AudioOut.MinGrain, uint sampleRate = 48000, int userId = SceUser.System)
    {
        // Both of these are refused by the output rather than adjusted, and the refusal arrives as a
        // number from a call several layers down. Checking them here says which argument was wrong.
        if (grain < AudioOut.MinGrain || grain > AudioOut.MaxGrain || grain % AudioOut.MinGrain != 0)
            throw new ArgumentOutOfRangeException(nameof(grain),
                "Grain must be a whole multiple of 256 samples, from 256 to 2048.");
        if (sampleRate is not (48000 or 192000))
            throw new ArgumentOutOfRangeException(nameof(sampleRate),
                "The main output takes 48000 or 192000 hertz. Resample before playing anything else.");

        // Initialization is tolerant: a module may find the subsystem already started, which the
        // return code reports without preventing a port from opening.
        AudioOut.sceAudioOutInit();

        int handle = AudioOut.sceAudioOutOpen(
            userId, (int)AudioOutPortType.Main, 0, grain, sampleRate, (uint)AudioOutFormat.S16Stereo);
        SceResult.ThrowIfFailed(handle, nameof(AudioOut.sceAudioOutOpen));

        var device = new AudioOutDevice(handle, grain);
        device.SetVolume(AudioOut.Volume0Db);
        return device;
    }

    /// <summary>
    /// Outputs one block of interleaved stereo samples, blocking until the queue has room for it. The
    /// span must hold at least <see cref="SamplesPerBlock"/> shorts.
    /// </summary>
    /// <exception cref="ProsperoException">The output call failed.</exception>
    public void Output(ReadOnlySpan<short> samples)
    {
        if (samples.Length < SamplesPerBlock)
            throw new ArgumentException($"A block needs at least {SamplesPerBlock} samples.", nameof(samples));
        fixed (short* p = samples)
            SceResult.ThrowIfFailed(AudioOut.sceAudioOutOutput(_handle, p), nameof(AudioOut.sceAudioOutOutput));
    }

    /// <summary>Sets both channels to <paramref name="volume"/>, from 0 (silent) to <see cref="AudioOut.Volume0Db"/>.</summary>
    /// <exception cref="ProsperoException">The volume call failed.</exception>
    public void SetVolume(int volume)
    {
        int level = Math.Clamp(volume, 0, AudioOut.Volume0Db);
        // Size the buffer to the maximum channel count so the service cannot read past it regardless of
        // how it indexes the array; only the selected (left, right) entries are applied.
        int* channels = stackalloc int[8];
        for (int i = 0; i < 8; i++)
            channels[i] = level;
        SceResult.ThrowIfFailed(
            AudioOut.sceAudioOutSetVolume(_handle, AudioOut.VolumeFlagLeft | AudioOut.VolumeFlagRight, channels),
            nameof(AudioOut.sceAudioOutSetVolume));
    }

    /// <summary>
    /// The time of the port's last output, on the audio clock. The difference between two readings says
    /// how far the output has advanced between them, which is how a caller works out whether its own
    /// mixing is keeping up.
    /// </summary>
    /// <remarks>
    /// This port publishes no way to ask how much room the queue has left, so there is no way to tell in
    /// advance whether <see cref="Output"/> will block. <see cref="AudioQueueDevice"/> is the output that
    /// does answer that, and is what an application uses when audio must never hold up a frame.
    /// </remarks>
    /// <exception cref="ProsperoException">The time could not be read.</exception>
    public ulong LastOutputTime
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            ulong time = 0;
            SceResult.ThrowIfFailed(
                AudioOut.sceAudioOutGetLastOutputTime(_handle, &time),
                nameof(AudioOut.sceAudioOutGetLastOutputTime));
            return time;
        }
    }

    /// <summary>
    /// Reads where the port's samples are going and how loud. Watch
    /// <see cref="SceAudioOutPortState.RerouteCounter"/> to notice the player moving from the television
    /// to a headset, which is when a mix built for one speaker layout has to be rebuilt for another.
    /// </summary>
    /// <exception cref="ProsperoException">The state could not be read.</exception>
    public SceAudioOutPortState GetPortState()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceAudioOutPortState state = default;
        SceResult.ThrowIfFailed(
            AudioOut.sceAudioOutGetPortState(_handle, &state),
            nameof(AudioOut.sceAudioOutGetPortState));
        return state;
    }

    /// <summary>Which outputs the port's samples are reaching.</summary>
    /// <exception cref="ProsperoException">The state could not be read.</exception>
    public AudioOutStateOutput ConnectedOutputs => (AudioOutStateOutput)GetPortState().Output;

    /// <summary>
    /// Waits for the block already queued to finish playing and leaves the port idle. Pushing again
    /// afterwards resumes output.
    /// </summary>
    /// <exception cref="ProsperoException">The output call failed.</exception>
    public void Drain()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        // A push with nothing to push is the port's end-of-output: it waits for the queued block and
        // then stops rather than submitting another. One call is enough - there is one block to wait on.
        SceResult.ThrowIfFailed(AudioOut.sceAudioOutOutput(_handle, null), nameof(AudioOut.sceAudioOutOutput));
    }

    /// <summary>Waits for the last block to play, then closes the port.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        AudioOut.sceAudioOutOutput(_handle, null);
        _disposed = true;
        AudioOut.sceAudioOutClose(_handle);
    }
}
