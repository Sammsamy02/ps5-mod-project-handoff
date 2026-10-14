// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Audio;

/// <summary>
/// Plays several sounds at once through one output port — music under a run of effects, a few effects
/// overlapping. Start a clip with <see cref="Play"/>, then fill each block of the port from
/// <see cref="Mix"/>. Each sound plays until it ends (or forever, when looping); finished sounds drop out
/// on their own. A mono clip is spread to both channels, and a clip recorded at another rate is retuned
/// to the mixer's, so a sound from anywhere plays at the right pitch.
/// </summary>
/// <remarks>
/// The mixer works in the stereo format <see cref="AudioOutDevice"/> takes; make its sample rate match
/// the port's. <see cref="Mix"/> overwrites the block from silence, so pass it straight to
/// <see cref="AudioOutDevice.Output"/>.
/// </remarks>
/// <example>
/// <code>
/// using var audio = AudioOutDevice.OpenStereo();
/// var mixer = new AudioMixer();
/// mixer.Play(WavAudio.Load("/app0/music.wav"), volume: 0.6f, loop: true);
/// short[] block = new short[audio.SamplesPerBlock];
/// // each iteration of the audio loop:
/// mixer.Mix(block);
/// audio.Output(block);
/// // on an event:
/// mixer.Play(coin);
/// </code>
/// </example>
public sealed class AudioMixer
{
    private readonly int _sampleRate;
    private readonly List<Voice> _voices = [];

    /// <summary>Creates a mixer for a port running at <paramref name="sampleRate"/> samples a second.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="sampleRate"/> is not positive.</exception>
    public AudioMixer(int sampleRate = 48000)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(sampleRate);
        _sampleRate = sampleRate;
    }

    /// <summary>An overall level applied to every sound, from 0 (silent) to 1. Default 1.</summary>
    public float MasterVolume { get; set; } = 1f;

    /// <summary>The most sounds that play at once; starting one past this drops the oldest. Default 32.</summary>
    public int MaxVoices { get; set; } = 32;

    /// <summary>How many sounds are playing now.</summary>
    public int ActiveVoices => _voices.Count;

    /// <summary>The sample rate the mixer was made for.</summary>
    public int SampleRate => _sampleRate;

    /// <summary>
    /// Starts <paramref name="clip"/> playing at <paramref name="volume"/> (0 to 1). When
    /// <paramref name="loop"/> is set it repeats until <see cref="StopAll"/>; otherwise it plays once and
    /// drops out. An empty or unsupported clip is ignored.
    /// </summary>
    public void Play(PcmAudio clip, float volume = 1f, bool loop = false)
    {
        if (clip.Samples is null || clip.Samples.Length == 0 || clip.Channels is < 1 or > 2 || clip.SampleRate <= 0)
            return;
        if (_voices.Count >= Math.Max(1, MaxVoices))
            _voices.RemoveAt(0); // make room by dropping the oldest sound
        _voices.Add(new Voice(clip, Math.Clamp(volume, 0f, 1f), loop, _sampleRate));
    }

    /// <summary>Stops every sound at once.</summary>
    public void StopAll() => _voices.Clear();

    /// <summary>
    /// Fills <paramref name="stereoBlock"/> with the sum of every playing sound, overwriting it from
    /// silence. The span is interleaved stereo; pass one of <see cref="AudioOutDevice.SamplesPerBlock"/>
    /// shorts.
    /// </summary>
    public void Mix(Span<short> stereoBlock)
    {
        stereoBlock.Clear();
        if (_voices.Count == 0)
            return;

        int frames = stereoBlock.Length / 2;
        float master = Math.Clamp(MasterVolume, 0f, 1f);

        for (int f = 0; f < frames; f++)
        {
            int accLeft = 0, accRight = 0;
            foreach (Voice voice in _voices)
            {
                if (voice.Finished)
                    continue;

                int frame = (int)voice.Position;
                if (frame >= voice.FrameCount)
                {
                    if (voice.Loop && voice.FrameCount > 0)
                    {
                        voice.Position %= voice.FrameCount;
                        frame = (int)voice.Position;
                    }
                    else
                    {
                        voice.Finished = true;
                        continue;
                    }
                }

                voice.Read(frame, out int left, out int right);
                float gain = voice.Volume * master;
                accLeft += (int)(left * gain);
                accRight += (int)(right * gain);
                voice.Position += voice.Step;
            }

            stereoBlock[f * 2] = (short)Math.Clamp(accLeft, short.MinValue, short.MaxValue);
            stereoBlock[(f * 2) + 1] = (short)Math.Clamp(accRight, short.MinValue, short.MaxValue);
        }

        // Drop the sounds that ended during this block.
        for (int i = _voices.Count - 1; i >= 0; i--)
        {
            if (_voices[i].Finished)
                _voices.RemoveAt(i);
        }
    }

    // One playing sound: a clip, a level, and a position that advances by the ratio of the clip's rate to
    // the mixer's, so a clip at a different rate is resampled by nearest sample as it plays.
    private sealed class Voice(PcmAudio clip, float volume, bool loop, int mixerRate)
    {
        private readonly short[] _samples = clip.Samples;
        private readonly int _channels = clip.Channels;

        public int FrameCount { get; } = clip.FrameCount;

        public float Volume { get; } = volume;

        public bool Loop { get; } = loop;

        public double Step { get; } = (double)clip.SampleRate / mixerRate;

        public double Position { get; set; }

        public bool Finished { get; set; }

        public void Read(int frame, out int left, out int right)
        {
            if (_channels == 2)
            {
                left = _samples[frame * 2];
                right = _samples[(frame * 2) + 1];
            }
            else
            {
                left = right = _samples[frame];
            }
        }
    }
}
