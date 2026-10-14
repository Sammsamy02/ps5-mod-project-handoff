// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.IO;
using System;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// SELF (Signed ELF) container parser and kernel-assisted decryptor. Parses the SELF
/// header to extract segment information, then uses <c>mmap</c> with the <c>0x80000</c>
/// flag to trigger kernel-side decryption of each segment.
/// </summary>
public static unsafe class PayloadSelfDecryptor
{
    /// <summary>SELF magic for the current platform (0x1D3D154F).</summary>
    public const uint SelfMagicOrbis = 0x1D3D154F;

    /// <summary>SELF magic for the next-gen platform (0xEEF51454).</summary>
    public const uint SelfMagicProspero = 0xEEF51454;

    /// <summary>The mmap flag that triggers kernel-side SELF segment decryption.</summary>
    public const int MmapSelfDecrypt = 0x80000;

    /// <summary>
    /// Checks whether a file begins with a SELF magic value.
    /// </summary>
    public static bool IsSelf(byte* data) =>
        *(uint*)data == SelfMagicOrbis || *(uint*)data == SelfMagicProspero;

    /// <summary>
    /// Reads the segment count from a SELF header.
    /// </summary>
    public static int GetSegmentCount(byte* header) => *(ushort*)(header + 0x18);

    /// <summary>
    /// Decrypts a SELF file's segments by opening the file with <c>mmap</c> using the
    /// <c>0x80000</c> flag, which causes the kernel to decrypt each segment in place.
    /// </summary>
    /// <param name="path">NUL-terminated path to the SELF file.</param>
    /// <param name="outBuf">Buffer to receive the decrypted ELF data.</param>
    /// <param name="outSize">Maximum size of the output buffer.</param>
    /// <returns>The actual size of the decrypted data, or -1 on failure.</returns>
    public static long Decrypt(byte* path, byte* outBuf, long outSize)
    {
        int fd = PayloadIo.open(path, PayloadFileSystem.O_RDONLY);
        if (fd < 0) return -1;

        FreeBsdStat stat;
        if (PayloadIo.fstat(fd, &stat) != 0) { PayloadIo.close(fd); return -1; }

        void* mapped = PayloadIo.mmap(null, (nuint)stat.st_size,
            PayloadIo.ProtRead, PayloadIo.MapPrivate | MmapSelfDecrypt, fd, 0);
        PayloadIo.close(fd);

        if (mapped == (void*)-1 || mapped == null) return -1;

        long copySize = stat.st_size < outSize ? stat.st_size : outSize;
        Buffer.MemoryCopy(mapped, outBuf, outSize, copySize);

        PayloadIo.munmap(mapped, (nuint)stat.st_size);
        return copySize;
    }
}
