// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Storage;
using System;
using System.Buffers.Binary;

namespace SharpProspero.Audio;

/// <summary>
/// Reads and writes VAG audio - the compact four-bit adaptive-differential form the console decodes for
/// sound effects - with no system module. A clip decodes to <see cref="PcmAudio"/> ready to play through
/// the audio port, and 16-bit PCM encodes to a VAG a fraction of the size of a WAV. Mono and stereo, with
/// per-channel blocks interleaved. The container is the 48-byte header the decoder expects; the samples
/// use the base predictor set, so a produced file plays through the console's decoder.
/// </summary>
public static class VagAudio
{
    private const int HeaderSize = 48;
    private const int BlockSize = 16;      // bytes per block
    private const int BlockSamples = 28;   // decoded samples per block
    // What the second byte of a block says about it. The block carrying the end mark is a terminator
    // and holds no sound: a file ends with one per channel, and whoever plays it stops there. The mark
    // on the last block that does carry sound says only that it is the last.
    private const byte FlagPlaybackEnd = 7;
    private const byte FlagLastDataBlock = 1;

    // What a terminator block holds after its two leading bytes. Every file measured carries this.
    private const byte TerminatorFill = 0x77;

    // Adaptive-differential filter coefficients, scaled by 64, for the base predictors 0-4. This is the
    // fixed filter set the container's decoder applies for these predictor indices.
    private static readonly int[] F0 = [0, 60, 115, 98, 122];
    private static readonly int[] F1 = [0, 0, -52, -55, -60];
    private static readonly double[] G0 = [0, 60.0 / 64, 115.0 / 64, 98.0 / 64, 122.0 / 64];
    private static readonly double[] G1 = [0, 0, -52.0 / 64, -55.0 / 64, -60.0 / 64];

    /// <summary>Loads and decodes the VAG file at <paramref name="path"/>.</summary>
    /// <exception cref="ProsperoException">The file could not be read or is not a supported VAG.</exception>
    public static PcmAudio Load(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        return Decode(FileSystem.ReadAllBytes(path));
    }

    /// <summary>Decodes a VAG clip to 16-bit PCM.</summary>
    /// <exception cref="ProsperoException">The data is not a supported VAG.</exception>
    public static PcmAudio Decode(ReadOnlySpan<byte> vag)
    {
        if (vag.Length < HeaderSize || vag[0] != (byte)'V' || vag[1] != (byte)'A' || vag[2] != (byte)'G' || vag[3] != (byte)'p')
            throw new ProsperoException("Not a VAG file.", -1);

        int sampleRate = (int)BinaryPrimitives.ReadUInt32BigEndian(vag[16..]);
        int channels = vag[30] == 0 ? 1 : vag[30];   // ucNumChannels
        if (channels != 1 && channels != 2)
            throw new ProsperoException("Only mono or stereo VAG is supported.", -1);
        if (sampleRate <= 0)
            throw new ProsperoException("The VAG has an invalid sample rate.", -1);

        ReadOnlySpan<byte> body = vag[HeaderSize..];
        int superBlock = BlockSize * channels;
        int superCount = body.Length / superBlock;

        // A conforming file leads with a silent block per channel; drop it so a round trip is clean.
        int start = superCount > 0 && IsSilentSuperBlock(body[..superBlock]) ? 1 : 0;

        // A file ends with a block per channel carrying the end mark and no sound. Decoding it emits
        // twenty-eight samples of the fill it holds, which is a full-scale step at the end of every
        // clip - so the scan stops there and the length is settled from where it stopped.
        int end = superCount;
        for (int sb = start; sb < superCount; sb++)
        {
            if (body[sb * superBlock + 1] != FlagPlaybackEnd)
                continue;
            end = sb;
            break;
        }

        int frames = Math.Max(0, end - start) * BlockSamples;
        short[] samples = new short[frames * channels];

        int[] h1 = new int[channels];
        int[] h2 = new int[channels];
        Span<short> block = stackalloc short[BlockSamples];
        for (int sb = start; sb < end; sb++)
        {
            int baseFrame = (sb - start) * BlockSamples;
            for (int c = 0; c < channels; c++)
            {
                DecodeBlock(body.Slice(sb * superBlock + c * BlockSize, BlockSize), ref h1[c], ref h2[c], block);
                for (int i = 0; i < BlockSamples; i++)
                    samples[(baseFrame + i) * channels + c] = block[i];
            }
        }
        return new PcmAudio(samples, sampleRate, channels);
    }

    /// <summary>Encodes 16-bit PCM to the bytes of a VAG file, with an optional 16-character name.</summary>
    public static byte[] Encode(PcmAudio audio, string name = "")
    {
        ArgumentNullException.ThrowIfNull(audio.Samples);
        if (audio.Channels != 1 && audio.Channels != 2)
            throw new ArgumentException("Channels must be 1 or 2.", nameof(audio));
        if (audio.SampleRate <= 0)
            throw new ArgumentException("The sample rate must be positive.", nameof(audio));

        int channels = audio.Channels;
        int frames = audio.Samples.Length / channels;
        // One leading silent block per channel, one block per 28-sample frame, and one terminator per
        // channel at the end. A file without the terminator plays on into whatever follows it, because
        // nothing in it says where the sound stops.
        int dataBlocks = (frames + BlockSamples - 1) / BlockSamples;
        int blocksPerChannel = 1 + dataBlocks + 1;
        int dataSize = blocksPerChannel * BlockSize * channels;
        byte[] output = new byte[HeaderSize + dataSize];
        Span<byte> span = output;

        // Header (big-endian multi-byte fields).
        "VAGp"u8.CopyTo(span);
        BinaryPrimitives.WriteUInt32BigEndian(span[4..], 0x20);          // version
        BinaryPrimitives.WriteUInt32BigEndian(span[12..], (uint)dataSize);
        BinaryPrimitives.WriteUInt32BigEndian(span[16..], (uint)audio.SampleRate);
        span[30] = (byte)channels;                  // ucNumChannels
        WriteName(span[32..48], name);              // ucName[16]

        int super = BlockSize * channels;
        Span<short> frame = stackalloc short[BlockSamples];
        // The leading silent block is already zero. Encode each channel independently from the second block.
        for (int c = 0; c < channels; c++)
        {
            int h1 = 0, h2 = 0;
            for (int f = 0; f < dataBlocks; f++)
            {
                for (int i = 0; i < BlockSamples; i++)
                {
                    int index = (f * BlockSamples + i) * channels + c;
                    frame[i] = index < audio.Samples.Length ? audio.Samples[index] : (short)0;
                }
                int blockOff = HeaderSize + (1 + f) * super + c * BlockSize;
                bool last = f == dataBlocks - 1;
                EncodeBlock(frame, ref h1, ref h2, span.Slice(blockOff, BlockSize), last ? FlagLastDataBlock : (byte)0);
            }

            // The terminator: no predictor, no shift, the end mark, and the fill.
            Span<byte> terminator = span.Slice(HeaderSize + (1 + dataBlocks) * super + c * BlockSize, BlockSize);
            terminator[0] = 0;
            terminator[1] = FlagPlaybackEnd;
            terminator[2..].Fill(TerminatorFill);
        }
        return output;
    }

    /// <summary>Encodes <paramref name="audio"/> and writes it to the file at <paramref name="path"/>.</summary>
    public static void Save(string path, PcmAudio audio, string name = "")
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        FileSystem.WriteAllBytes(path, Encode(audio, name));
    }

    private static void DecodeBlock(ReadOnlySpan<byte> block, ref int h1, ref int h2, Span<short> outSamples)
    {
        int predictor = block[0] >> 4;
        int shift = block[0] & 0x0F;
        if (predictor > 4) predictor = 0;   // only the base predictors decode with this filter set
        if (shift > 12) shift = 12;
        for (int b = 0; b < 14; b++)
        {
            byte pair = block[2 + b];
            for (int n = 0; n < 2; n++)
            {
                int nibble = n == 0 ? pair & 0x0F : pair >> 4;
                int residual = (short)(nibble << 12) >> shift;   // sign-extended, scaled
                int predicted = (h1 * F0[predictor] + h2 * F1[predictor]) >> 6;
                int sample = Math.Clamp(residual + predicted, short.MinValue, short.MaxValue);
                h2 = h1;
                h1 = sample;
                outSamples[b * 2 + n] = (short)sample;
            }
        }
    }

    private static void EncodeBlock(ReadOnlySpan<short> frame, ref int h1, ref int h2, Span<byte> block, byte flag)
    {
        // Choose the predictor that minimises the largest residual over the frame (measuring against the
        // source samples), then a shift fine enough to carry it.
        int best = 0;
        double bestMax = double.MaxValue;
        for (int p = 0; p < 5; p++)
        {
            double s1 = h1, s2 = h2, max = 0;
            foreach (short sample in frame)
            {
                double residual = sample - (s1 * G0[p] + s2 * G1[p]);
                if (Math.Abs(residual) > max) max = Math.Abs(residual);
                s2 = s1;
                s1 = sample;
            }
            if (max < bestMax) { bestMax = max; best = p; }
        }

        int exponent = 0;
        while (exponent < 12 && (7 << exponent) < (int)bestMax) exponent++;
        int shift = 12 - exponent;
        int step = 1 << exponent;

        block[0] = (byte)((best << 4) | shift);
        block[1] = flag;
        for (int i = 0; i < 14; i++) block[2 + i] = 0;

        // Quantise against the decoder's own running history, so the encoded file reproduces these samples.
        for (int i = 0; i < BlockSamples; i++)
        {
            int predicted = (h1 * F0[best] + h2 * F1[best]) >> 6;
            int residual = frame[i] - predicted;
            int nibble = exponent == 0 ? residual : (residual + (residual >= 0 ? step / 2 : -step / 2)) / step;
            nibble = Math.Clamp(nibble, -8, 7);
            int decoded = Math.Clamp((nibble << exponent) + predicted, short.MinValue, short.MaxValue);
            h2 = h1;
            h1 = decoded;
            int at = 2 + i / 2;
            block[at] = (byte)(i % 2 == 0 ? nibble & 0x0F : (block[at] & 0x0F) | ((nibble & 0x0F) << 4));
        }
    }

    private static bool IsSilentSuperBlock(ReadOnlySpan<byte> superBlock)
    {
        for (int b = 0; b < superBlock.Length; b += BlockSize)
            for (int i = 2; i < BlockSize; i++)
                if (superBlock[b + i] != 0)
                    return false;
        return true;
    }

    private static void WriteName(Span<byte> destination, string name)
    {
        destination.Clear();
        for (int i = 0; i < name.Length && i < destination.Length - 1; i++)
            destination[i] = (byte)(char.IsAscii(name[i]) ? name[i] : '_');
    }
}
