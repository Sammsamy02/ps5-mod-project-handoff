// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Storage;
using System;
using System.Buffers.Binary;

namespace SharpProspero.Audio;

/// <summary>
/// A block of 16-bit PCM audio: the samples, the sample rate, and the channel count. Stereo samples are
/// interleaved left, right. This is the shape the audio-output and microphone-input ports use, so a clip
/// read from a WAV plays straight through <c>AudioOutDevice</c>, and microphone samples write straight to
/// a WAV.
/// </summary>
/// <param name="Samples">The interleaved 16-bit samples.</param>
/// <param name="SampleRate">Samples per second per channel (for example 48000).</param>
/// <param name="Channels">1 for mono, 2 for stereo.</param>
public readonly record struct PcmAudio(short[] Samples, int SampleRate, int Channels)
{
    /// <summary>The number of sample frames (samples per channel).</summary>
    public int FrameCount => Channels > 0 ? Samples.Length / Channels : 0;

    /// <summary>The play length in milliseconds.</summary>
    public long DurationMilliseconds => SampleRate > 0 ? (long)FrameCount * 1000 / SampleRate : 0;
}

/// <summary>
/// Reads and writes 16-bit PCM WAV files, with no system module. WAV is uncompressed, so it is the
/// dependable way to load a sound to play through the audio port or to save a microphone recording. Read
/// a file to <see cref="PcmAudio"/> and push its samples to the output; capture microphone blocks and
/// write them back out.
/// </summary>
public static class WavAudio
{
    /// <summary>Loads and decodes the WAV file at <paramref name="path"/>.</summary>
    /// <exception cref="ProsperoException">The file could not be read or is not a supported WAV.</exception>
    public static PcmAudio Load(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        return Decode(FileSystem.ReadAllBytes(path));
    }

    /// <summary>
    /// Decodes an uncompressed 16-bit PCM WAV. Other sample widths and compressed forms are rejected, so
    /// the samples are always ready to push at the audio port.
    /// </summary>
    /// <exception cref="ProsperoException">The data is not a 16-bit PCM WAV.</exception>
    public static PcmAudio Decode(ReadOnlySpan<byte> wav)
    {
        if (wav.Length < 44
            || wav[0] != 'R' || wav[1] != 'I' || wav[2] != 'F' || wav[3] != 'F'
            || wav[8] != 'W' || wav[9] != 'A' || wav[10] != 'V' || wav[11] != 'E')
            throw new ProsperoException("Not a WAV file.", -1);

        int sampleRate = 0, channels = 0, bitsPerSample = 0, formatTag = 0;
        bool haveFormat = false;
        ReadOnlySpan<byte> data = default;

        // Walk the RIFF chunk list for "fmt " and "data".
        int offset = 12;
        while (offset + 8 <= wav.Length)
        {
            uint chunkId = BinaryPrimitives.ReadUInt32BigEndian(wav[offset..]);
            int chunkSize = BinaryPrimitives.ReadInt32LittleEndian(wav[(offset + 4)..]);
            int body = offset + 8;
            // Compare against the remaining bytes without adding, so a crafted size cannot overflow the
            // sum and slip past the clamp (body is at most wav.Length here, so the difference is safe).
            if (chunkSize < 0 || chunkSize > wav.Length - body)
                chunkSize = wav.Length - body;

            if (chunkId == 0x666D7420 && chunkSize >= 16) // "fmt "
            {
                formatTag = BinaryPrimitives.ReadUInt16LittleEndian(wav[body..]);
                channels = BinaryPrimitives.ReadUInt16LittleEndian(wav[(body + 2)..]);
                sampleRate = BinaryPrimitives.ReadInt32LittleEndian(wav[(body + 4)..]);
                bitsPerSample = BinaryPrimitives.ReadUInt16LittleEndian(wav[(body + 14)..]);
                haveFormat = true;
            }
            else if (chunkId == 0x64617461) // "data"
            {
                data = wav.Slice(body, chunkSize);
            }

            // Chunks are word-aligned: an odd size carries a pad byte.
            offset = body + chunkSize + (chunkSize & 1);
        }

        if (!haveFormat)
            throw new ProsperoException("The WAV has no format chunk.", -1);
        if (formatTag != 1)
            throw new ProsperoException("Only uncompressed PCM WAV is supported.", -1);
        if (bitsPerSample != 16)
            throw new ProsperoException("Only 16-bit WAV is supported.", -1);
        if (channels != 1 && channels != 2)
            throw new ProsperoException("Only mono or stereo WAV is supported.", -1);
        if (data.IsEmpty)
            throw new ProsperoException("The WAV has no sample data.", -1);

        int sampleCount = data.Length / 2;
        short[] samples = new short[sampleCount];
        for (int i = 0; i < sampleCount; i++)
            samples[i] = BinaryPrimitives.ReadInt16LittleEndian(data[(i * 2)..]);

        return new PcmAudio(samples, sampleRate, channels);
    }

    /// <summary>Encodes 16-bit PCM samples to the bytes of a WAV file.</summary>
    public static byte[] Encode(PcmAudio audio)
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        if (audio.Channels != 1 && audio.Channels != 2)
            throw new ArgumentException("Channels must be 1 or 2.", nameof(audio));
        if (audio.SampleRate <= 0)
            throw new ArgumentException("The sample rate must be positive.", nameof(audio));

        int dataBytes = audio.Samples.Length * 2;
        int blockAlign = audio.Channels * 2;
        int byteRate = audio.SampleRate * blockAlign;
        byte[] output = new byte[44 + dataBytes];
        Span<byte> span = output;

        // RIFF/WAVE header.
        "RIFF"u8.CopyTo(span);
        BinaryPrimitives.WriteInt32LittleEndian(span[4..], 36 + dataBytes);
        "WAVE"u8.CopyTo(span[8..]);

        // fmt  chunk.
        "fmt "u8.CopyTo(span[12..]);
        BinaryPrimitives.WriteInt32LittleEndian(span[16..], 16);
        BinaryPrimitives.WriteUInt16LittleEndian(span[20..], 1); // PCM
        BinaryPrimitives.WriteUInt16LittleEndian(span[22..], (ushort)audio.Channels);
        BinaryPrimitives.WriteInt32LittleEndian(span[24..], audio.SampleRate);
        BinaryPrimitives.WriteInt32LittleEndian(span[28..], byteRate);
        BinaryPrimitives.WriteUInt16LittleEndian(span[32..], (ushort)blockAlign);
        BinaryPrimitives.WriteUInt16LittleEndian(span[34..], 16); // bits per sample

        // data chunk.
        "data"u8.CopyTo(span[36..]);
        BinaryPrimitives.WriteInt32LittleEndian(span[40..], dataBytes);
        for (int i = 0; i < audio.Samples.Length; i++)
            BinaryPrimitives.WriteInt16LittleEndian(span[(44 + i * 2)..], audio.Samples[i]);

        return output;
    }

    /// <summary>Encodes <paramref name="audio"/> and writes it to the file at <paramref name="path"/>.</summary>
    public static void Save(string path, PcmAudio audio)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        FileSystem.WriteAllBytes(path, Encode(audio));
    }
}
