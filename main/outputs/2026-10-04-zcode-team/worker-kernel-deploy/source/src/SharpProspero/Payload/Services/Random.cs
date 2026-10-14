// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Random;
using System;

namespace SharpProspero.Payload.Services;

/// <summary>
/// Fills a buffer with cryptographically random bytes from the system's entropy source in a
/// payload context. Wraps <c>sceRandomGetRandomNumber</c> from <c>libSceRandom</c>.
/// </summary>
/// <remarks>
/// The system limits a single call to <see cref="SceRandom.MaxSize"/> (64) bytes. For buffers
/// larger than that, call this method in a loop with successive slices, or use
/// <see cref="GetRandomBytesFull"/> which handles the chunking internally.
/// </remarks>
public static unsafe class PayloadRandom
{
    /// <summary>
    /// Fills <paramref name="buffer"/> with random bytes. The buffer must be at most
    /// <see cref="SceRandom.MaxSize"/> (64) bytes; larger requests return a negative error code
    /// from the SPRX without writing any bytes.
    /// </summary>
    /// <param name="buffer">The destination buffer, at most 64 bytes.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int GetRandomBytes(Span<byte> buffer)
    {
        fixed (byte* p = buffer)
            return SceRandom.sceRandomGetRandomNumber(p, (nuint)buffer.Length);
    }

    /// <summary>
    /// Fills <paramref name="buffer"/> with random bytes of any length by issuing multiple calls
    /// to the underlying SPRX when the buffer exceeds the single-call limit of 64 bytes.
    /// </summary>
    /// <param name="buffer">The destination buffer, any size.</param>
    /// <returns>Zero on success, or the first non-zero error code encountered.</returns>
    public static int GetRandomBytesFull(Span<byte> buffer)
    {
        int offset = 0;
        while (offset < buffer.Length)
        {
            int chunkSize = buffer.Length - offset;
            if (chunkSize > SceRandom.MaxSize)
                chunkSize = SceRandom.MaxSize;

            int result = GetRandomBytes(buffer.Slice(offset, chunkSize));
            if (result != 0)
                return result;

            offset += chunkSize;
        }
        return 0;
    }
}
