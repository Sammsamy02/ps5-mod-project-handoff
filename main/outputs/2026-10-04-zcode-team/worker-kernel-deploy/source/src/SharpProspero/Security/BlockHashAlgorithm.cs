// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Security;

/// <summary>
/// The shared machinery for the digests that process data in 64-byte blocks and finish with a length
/// pad. It buffers partial blocks, feeds whole blocks to the concrete algorithm, and appends the
/// terminator, zero padding and message length at the end. Concrete algorithms supply the block
/// transform and the way they emit their state.
/// </summary>
public abstract class BlockHashAlgorithm : HashAlgorithm
{
    private readonly byte[] _block = new byte[64];
    private int _blockLength;
    private ulong _totalBytes;

    /// <inheritdoc/>
    public sealed override void Update(ReadOnlySpan<byte> data)
    {
        _totalBytes += (ulong)data.Length;

        // Top up a partial block first; only once it is full is it transformed.
        if (_blockLength > 0)
        {
            int take = Math.Min(64 - _blockLength, data.Length);
            data[..take].CopyTo(_block.AsSpan(_blockLength));
            _blockLength += take;
            data = data[take..];
            if (_blockLength < 64)
                return;
            ProcessBlock(_block);
            _blockLength = 0;
        }

        while (data.Length >= 64)
        {
            ProcessBlock(data[..64]);
            data = data[64..];
        }

        if (!data.IsEmpty)
        {
            data.CopyTo(_block);
            _blockLength = data.Length;
        }
    }

    /// <inheritdoc/>
    protected sealed override void FinishCore(Span<byte> destination)
    {
        ulong bitLength = _totalBytes * 8;

        // The terminator and length are appended through Update, which lands on exact block boundaries
        // because the pad length is chosen to fill the current block and, if needed, one more.
        Span<byte> tail = stackalloc byte[128];
        tail[0] = 0x80;
        int padLength = _blockLength < 56 ? 56 - _blockLength : 120 - _blockLength;
        if (LengthIsBigEndian)
            BinaryPrimitives.WriteUInt64BigEndian(tail.Slice(padLength, 8), bitLength);
        else
            BinaryPrimitives.WriteUInt64LittleEndian(tail.Slice(padLength, 8), bitLength);
        Update(tail[..(padLength + 8)]);

        WriteDigest(destination);
    }

    /// <summary>Whether the appended message length is big-endian (the SHA family) or little-endian (MD5).</summary>
    protected abstract bool LengthIsBigEndian { get; }

    /// <summary>Transforms one 64-byte block into the running state.</summary>
    protected abstract void ProcessBlock(ReadOnlySpan<byte> block);

    /// <summary>Writes the final state as the digest.</summary>
    protected abstract void WriteDigest(Span<byte> destination);
}
