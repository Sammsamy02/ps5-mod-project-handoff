// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Audio;

/// <summary>The shape of the wave a <see cref="ToneGenerator"/> produces.</summary>
public enum Waveform
{
    /// <summary>A smooth sine wave — a clean tone.</summary>
    Sine,

    /// <summary>A square wave — a hollow, buzzy tone.</summary>
    Square,

    /// <summary>A triangle wave — softer than a square.</summary>
    Triangle,

    /// <summary>A sawtooth wave — bright and harsh.</summary>
    Sawtooth,

    /// <summary>White noise — for a hiss, a hit, or an explosion.</summary>
    Noise,
}

/// <summary>
/// Makes simple tones and sound effects as 16-bit samples an <see cref="AudioOutDevice"/> plays, with no
/// audio file — a beep, an alert, a coin, a hit. Set the wave, the pitch and the loudness, then fill each
/// block of the audio port from it; the phase carries across blocks, so a held tone is continuous.
/// </summary>
/// <remarks>
/// The samples are interleaved stereo (the same value in both channels), which is the layout
/// <see cref="AudioOutDevice.Output"/> takes. Match <see cref="ToneGenerator(int)"/>'s sample rate to the
/// port's.
/// </remarks>
/// <example>
/// <code>
/// using var audio = AudioOutDevice.OpenStereo();
/// var tone = new ToneGenerator { Frequency = 880, Amplitude = 0.3f };
/// short[] block = new short[audio.SamplesPerBlock];
/// // for as long as the beep should sound:
/// tone.Fill(block);
/// audio.Output(block);
/// </code>
/// </example>
public sealed class ToneGenerator
{
    private readonly int _sampleRate;
    private double _phase;
    private uint _noise = 0x9E3779B9;

    /// <summary>Creates a generator for a port running at <paramref name="sampleRate"/> samples a second.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="sampleRate"/> is not positive.</exception>
    public ToneGenerator(int sampleRate = 48000)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(sampleRate);
        _sampleRate = sampleRate;
    }

    /// <summary>The shape of the wave. Default <see cref="Waveform.Sine"/>.</summary>
    public Waveform Waveform { get; set; } = Waveform.Sine;

    /// <summary>The pitch in cycles a second (hertz). Default 440.</summary>
    public double Frequency { get; set; } = 440.0;

    /// <summary>The loudness from 0 (silent) to 1 (full scale). Default 0.3.</summary>
    public float Amplitude { get; set; } = 0.3f;

    /// <summary>The sample rate the generator was made for.</summary>
    public int SampleRate => _sampleRate;

    /// <summary>Starts the wave again from the beginning of its cycle.</summary>
    public void Reset() => _phase = 0.0;

    /// <summary>
    /// Fills <paramref name="interleavedStereo"/> with the next samples, the same value in the left and
    /// right of each frame. The phase carries on from the previous call, so consecutive fills join without
    /// a click.
    /// </summary>
    public void Fill(Span<short> interleavedStereo)
    {
        float amplitude = Math.Clamp(Amplitude, 0f, 1f);
        double increment = Frequency / _sampleRate;
        int frames = interleavedStereo.Length / 2;

        for (int i = 0; i < frames; i++)
        {
            float value = Sample(_phase) * amplitude;
            short sample = (short)Math.Clamp((int)(value * 32767f), short.MinValue, short.MaxValue);
            interleavedStereo[i * 2] = sample;
            interleavedStereo[(i * 2) + 1] = sample;

            _phase += increment;
            _phase -= Math.Floor(_phase); // keep the phase in [0, 1)
        }
    }

    /// <summary>
    /// Renders a fixed length of the current tone as one interleaved-stereo buffer, for a short effect
    /// played in one call. Advances the phase as if the samples had been filled block by block.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="seconds"/> is negative.</exception>
    public short[] Render(double seconds)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(seconds);
        int frames = (int)(seconds * _sampleRate);
        short[] buffer = new short[frames * 2];
        Fill(buffer);
        return buffer;
    }

    /// <summary>
    /// Renders a fixed length of the current tone as a <see cref="PcmAudio"/> clip, ready to hand to an
    /// <see cref="AudioMixer"/> as a sound effect.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="seconds"/> is negative.</exception>
    public PcmAudio RenderClip(double seconds) => new(Render(seconds), _sampleRate, 2);

    private float Sample(double phase)
    {
        return Waveform switch
        {
            Waveform.Square => phase < 0.5 ? 1f : -1f,
            Waveform.Sawtooth => (float)((2.0 * phase) - 1.0),
            Waveform.Triangle => phase < 0.5 ? (float)((4.0 * phase) - 1.0) : (float)(3.0 - (4.0 * phase)),
            Waveform.Noise => NextNoise(),
            _ => MathF.Sin((float)(phase * 2.0 * Math.PI)),
        };
    }

    // A xorshift step turned into a value in [-1, 1]; independent of the pitch, so noise ignores it.
    private float NextNoise()
    {
        _noise ^= _noise << 13;
        _noise ^= _noise >> 17;
        _noise ^= _noise << 5;
        return ((_noise / (float)uint.MaxValue) * 2f) - 1f;
    }
}
