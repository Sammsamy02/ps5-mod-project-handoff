// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;

namespace SharpProspero.Security;

/// <summary>
/// A message digest built up from a stream of bytes. Feed it data with <see cref="Update"/> as many
/// times as needed, in any chunk sizes, then read the digest with <see cref="Finish()"/>. The concrete
/// types are <see cref="Sha256"/>, <see cref="Sha1"/> and <see cref="Md5"/>; each also offers one-shot
/// and file helpers so a caller rarely constructs one directly.
/// </summary>
/// <remarks>
/// These are self-contained calculations that need no system module, so a digest can be computed the
/// same way on the device and in tests. A hasher accumulates state, so use a fresh one per digest.
/// </remarks>
public abstract unsafe class HashAlgorithm
{
    /// <summary>The size of the digest in bytes.</summary>
    public abstract int HashSize { get; }

    /// <summary>Adds <paramref name="data"/> to the running digest.</summary>
    public abstract void Update(ReadOnlySpan<byte> data);

    /// <summary>Writes the final digest into <paramref name="destination"/>, which must hold <see cref="HashSize"/> bytes.</summary>
    /// <exception cref="ArgumentException"><paramref name="destination"/> is too small.</exception>
    public void Finish(Span<byte> destination)
    {
        if (destination.Length < HashSize)
            throw new ArgumentException($"The destination needs at least {HashSize} bytes.", nameof(destination));
        FinishCore(destination);
    }

    /// <summary>Returns the final digest as a new array.</summary>
    public byte[] Finish()
    {
        byte[] digest = new byte[HashSize];
        FinishCore(digest);
        return digest;
    }

    /// <summary>Returns the final digest as a lowercase hexadecimal string.</summary>
    public string FinishHex() => Convert.ToHexStringLower(Finish());

    /// <summary>Streams the file at <paramref name="path"/> through the digest and returns the result.</summary>
    /// <exception cref="ProsperoException">Opening or reading the file failed.</exception>
    public byte[] ComputeFile(string path)
    {
        ReadFileInto(path, this);
        return Finish();
    }

    /// <summary>Writes the final digest and resets no state; concrete types implement the finalization.</summary>
    protected abstract void FinishCore(Span<byte> destination);

    // Opens the file and feeds it to the digest in fixed-size blocks, so a file of any size is hashed
    // without holding all of it in memory at once.
    private static void ReadFileInto(string path, HashAlgorithm hash)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        int fd = OpenRead(path);
        try
        {
            byte[] buffer = new byte[64 * 1024];
            fixed (byte* p = buffer)
            {
                while (true)
                {
                    long read = KernelFile.sceKernelRead(fd, p, (nuint)buffer.Length);
                    if (read < 0)
                        throw new ProsperoException(nameof(KernelFile.sceKernelRead), (int)read);
                    if (read == 0)
                        break;
                    hash.Update(new ReadOnlySpan<byte>(p, (int)read));
                }
            }
        }
        finally
        {
            KernelFile.sceKernelClose(fd);
        }
    }

    private static int OpenRead(string path)
    {
        int byteCount = System.Text.Encoding.UTF8.GetByteCount(path);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        System.Text.Encoding.UTF8.GetBytes(path, buffer);
        buffer[byteCount] = 0;
        int fd;
        fixed (byte* p = buffer)
            fd = KernelFile.sceKernelOpen(p, KernelFile.ReadOnly, 0);
        return SceResult.ThrowIfFailed(fd, nameof(KernelFile.sceKernelOpen));
    }
}
