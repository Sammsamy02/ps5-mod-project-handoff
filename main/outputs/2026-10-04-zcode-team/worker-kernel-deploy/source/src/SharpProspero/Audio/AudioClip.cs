// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Audio;

/// <summary>
/// Offline operations on a <see cref="PcmAudio"/> clip: change the sample rate, fold to mono or spread to
/// stereo, scale or normalise the level, join two clips, and cut a range out. Each returns a new clip and
/// leaves the input untouched, so a sound can be prepared once at load time and then played.
/// </summary>
public static class AudioClip
{
    /// <summary>Resamples the clip to <paramref name="sampleRate"/> by linear interpolation.</summary>
    public static PcmAudio Resample(PcmAudio audio, int sampleRate)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        if (sampleRate <= 0)
            throw new ArgumentOutOfRangeException(nameof(sampleRate));
        if (sampleRate == audio.SampleRate || audio.SampleRate <= 0)
            return audio with { SampleRate = sampleRate };

        int channels = audio.Channels;
        int inFrames = audio.FrameCount;
        long outFrames = (long)inFrames * sampleRate / audio.SampleRate;
        var output = new short[outFrames * channels];
        double step = (double)audio.SampleRate / sampleRate;
        for (long f = 0; f < outFrames; f++)
        {
            double src = f * step;
            int i0 = (int)src;
            int i1 = Math.Min(i0 + 1, inFrames - 1);
            double frac = src - i0;
            for (int c = 0; c < channels; c++)
            {
                double a = audio.Samples[i0 * channels + c];
                double b = audio.Samples[i1 * channels + c];
                output[f * channels + c] = (short)Math.Clamp(Math.Round(a + (b - a) * frac), short.MinValue, short.MaxValue);
            }
        }
        return new PcmAudio(output, sampleRate, channels);
    }

    /// <summary>Folds a stereo clip to one channel by averaging; a mono clip is returned unchanged.</summary>
    public static PcmAudio ToMono(PcmAudio audio)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        if (audio.Channels == 1)
            return audio;
        int frames = audio.FrameCount;
        var output = new short[frames];
        for (int f = 0; f < frames; f++)
        {
            int sum = 0;
            for (int c = 0; c < audio.Channels; c++)
                sum += audio.Samples[f * audio.Channels + c];
            output[f] = (short)(sum / audio.Channels);
        }
        return new PcmAudio(output, audio.SampleRate, 1);
    }

    /// <summary>Spreads a mono clip across two channels; a stereo clip is returned unchanged.</summary>
    public static PcmAudio ToStereo(PcmAudio audio)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        if (audio.Channels == 2)
            return audio;
        if (audio.Channels != 1)
            throw new ArgumentException("Only a mono clip can be spread to stereo.", nameof(audio));
        var output = new short[audio.Samples.Length * 2];
        for (int i = 0; i < audio.Samples.Length; i++)
            output[i * 2] = output[i * 2 + 1] = audio.Samples[i];
        return new PcmAudio(output, audio.SampleRate, 2);
    }

    /// <summary>Scales every sample by <paramref name="factor"/>, clamping to the 16-bit range.</summary>
    public static PcmAudio Gain(PcmAudio audio, float factor)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        var output = new short[audio.Samples.Length];
        for (int i = 0; i < output.Length; i++)
            output[i] = (short)Math.Clamp(MathF.Round(audio.Samples[i] * factor), short.MinValue, short.MaxValue);
        return audio with { Samples = output };
    }

    /// <summary>
    /// Scales the clip so its loudest sample reaches <paramref name="peak"/> of full scale (0 to 1). A
    /// silent clip is returned unchanged.
    /// </summary>
    public static PcmAudio Normalize(PcmAudio audio, float peak = 1.0f)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        int max = 0;
        foreach (short sample in audio.Samples)
            max = Math.Max(max, Math.Abs((int)sample));
        if (max == 0)
            return audio;
        return Gain(audio, Math.Clamp(peak, 0f, 1f) * short.MaxValue / max);
    }

    /// <summary>Joins two clips end to end. They must share a sample rate and channel count.</summary>
    public static PcmAudio Concat(PcmAudio first, PcmAudio second)
    {
        ArgumentNullException.ThrowIfNull(first.Samples);
        ArgumentNullException.ThrowIfNull(second.Samples);
        if (first.SampleRate != second.SampleRate || first.Channels != second.Channels)
            throw new ArgumentException("The clips must share a sample rate and channel count.");
        var output = new short[first.Samples.Length + second.Samples.Length];
        first.Samples.CopyTo(output, 0);
        second.Samples.CopyTo(output, first.Samples.Length);
        return new PcmAudio(output, first.SampleRate, first.Channels);
    }

    /// <summary>Cuts <paramref name="frameCount"/> frames starting at <paramref name="startFrame"/>.</summary>
    public static PcmAudio Trim(PcmAudio audio, int startFrame, int frameCount)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        int frames = audio.FrameCount;
        if (startFrame < 0 || frameCount < 0 || startFrame > frames)
            throw new ArgumentOutOfRangeException(nameof(startFrame));
        int count = Math.Min(frameCount, frames - startFrame);
        var output = new short[count * audio.Channels];
        Array.Copy(audio.Samples, startFrame * audio.Channels, output, 0, output.Length);
        return new PcmAudio(output, audio.SampleRate, audio.Channels);
    }
}
