// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Audio;

/// <summary>The shape of a <see cref="BiquadFilter"/>.</summary>
public enum BiquadType
{
    /// <summary>Passes frequencies below the cut-off and rolls off above it.</summary>
    LowPass,
    /// <summary>Passes frequencies above the cut-off and rolls off below it.</summary>
    HighPass,
    /// <summary>Passes a band around the centre frequency and rolls off either side.</summary>
    BandPass,
    /// <summary>Cuts a narrow band around the centre frequency and passes the rest.</summary>
    Notch,
}

/// <summary>
/// A two-pole audio filter - low pass, high pass, band pass or notch - for shaping a sound: soften a
/// harsh effect, isolate a frequency band, or remove a hum. It processes one sample or a whole block in
/// place, keeping its state between calls, so a stream is filtered continuously.
/// </summary>
public sealed class BiquadFilter
{
    private float _b0, _b1, _b2, _a1, _a2;
    private float _x1, _x2, _y1, _y2;

    /// <summary>Builds a filter of the given shape at <paramref name="frequency"/> hertz.</summary>
    /// <param name="type">The filter shape.</param>
    /// <param name="sampleRate">Samples per second of the audio it will process.</param>
    /// <param name="frequency">Cut-off or centre frequency in hertz.</param>
    /// <param name="q">Resonance / bandwidth. 0.707 is a flat, non-resonant response.</param>
    public BiquadFilter(BiquadType type, float sampleRate, float frequency, float q = 0.70710678f)
        => Configure(type, sampleRate, frequency, q);

    /// <summary>Recomputes the coefficients for a new shape, frequency or resonance.</summary>
    public void Configure(BiquadType type, float sampleRate, float frequency, float q = 0.70710678f)
    {
        if (sampleRate <= 0)
            throw new ArgumentOutOfRangeException(nameof(sampleRate));
        frequency = Math.Clamp(frequency, 1f, sampleRate * 0.5f - 1f);
        if (q <= 0)
            q = 0.70710678f;

        float w0 = 2f * MathF.PI * frequency / sampleRate;
        float cos = MathF.Cos(w0), sin = MathF.Sin(w0);
        float alpha = sin / (2f * q);
        float a0;
        switch (type)
        {
            case BiquadType.HighPass:
                _b0 = (1 + cos) / 2; _b1 = -(1 + cos); _b2 = (1 + cos) / 2;
                a0 = 1 + alpha; _a1 = -2 * cos; _a2 = 1 - alpha;
                break;
            case BiquadType.BandPass: // constant 0 dB peak gain
                _b0 = alpha; _b1 = 0; _b2 = -alpha;
                a0 = 1 + alpha; _a1 = -2 * cos; _a2 = 1 - alpha;
                break;
            case BiquadType.Notch:
                _b0 = 1; _b1 = -2 * cos; _b2 = 1;
                a0 = 1 + alpha; _a1 = -2 * cos; _a2 = 1 - alpha;
                break;
            default: // LowPass
                _b0 = (1 - cos) / 2; _b1 = 1 - cos; _b2 = (1 - cos) / 2;
                a0 = 1 + alpha; _a1 = -2 * cos; _a2 = 1 - alpha;
                break;
        }
        _b0 /= a0; _b1 /= a0; _b2 /= a0; _a1 /= a0; _a2 /= a0;
    }

    /// <summary>Clears the filter's memory of past samples.</summary>
    public void Reset() => _x1 = _x2 = _y1 = _y2 = 0;

    /// <summary>Filters one sample and advances the state.</summary>
    public float Process(float input)
    {
        float output = _b0 * input + _b1 * _x1 + _b2 * _x2 - _a1 * _y1 - _a2 * _y2;
        _x2 = _x1; _x1 = input;
        _y2 = _y1; _y1 = output;
        return output;
    }

    /// <summary>Filters a block of 16-bit samples in place. For stereo, run one filter per channel.</summary>
    public void ProcessBlock(Span<short> samples)
    {
        for (int i = 0; i < samples.Length; i++)
            samples[i] = (short)Math.Clamp(MathF.Round(Process(samples[i])), short.MinValue, short.MaxValue);
    }
}
