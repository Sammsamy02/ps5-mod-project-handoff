// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Storage;

/// <summary>
/// Reads files from the mounted package. Assets bundled with a module live under the package root,
/// for example <c>/app0/assets/level.bin</c>; open them by path and read their bytes. The package
/// root is read-only, so this type only reads.
/// </summary>
public static unsafe class PackageFile
{
    /// <summary>The mounted package root that a module's own files load from.</summary>
    public const string Root = "/app0";

    /// <summary>Reads the whole file at <paramref name="path"/> into a byte array.</summary>
    /// <exception cref="ProsperoException">Opening, sizing, or reading the file failed.</exception>
    public static byte[] ReadAllBytes(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        int fd = Open(path);
        try
        {
            long size = KernelFile.sceKernelLseek(fd, 0, KernelFile.SeekEnd);
            if (size < 0)
                throw new ProsperoException(nameof(KernelFile.sceKernelLseek), (int)size);
            if (size > int.MaxValue)
                throw new ProsperoException(nameof(KernelFile.sceKernelRead), unchecked((int)0x80020000));
            KernelFile.sceKernelLseek(fd, 0, KernelFile.SeekSet);

            byte[] data = new byte[size];
            long total = 0;
            fixed (byte* p = data)
            {
                while (total < size)
                {
                    long read = KernelFile.sceKernelRead(fd, p + total, (nuint)(size - total));
                    if (read < 0)
                        throw new ProsperoException(nameof(KernelFile.sceKernelRead), (int)read);
                    if (read == 0)
                        break;
                    total += read;
                }
            }
            return total == size ? data : data[..(int)total];
        }
        finally
        {
            KernelFile.sceKernelClose(fd);
        }
    }

    /// <summary>Reads the whole file at <paramref name="path"/> as UTF-8 text.</summary>
    public static string ReadAllText(string path) => Encoding.UTF8.GetString(ReadAllBytes(path));

    private static int Open(string path)
    {
        int byteCount = Encoding.UTF8.GetByteCount(path);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(path, buffer);
        buffer[byteCount] = 0;
        int fd;
        fixed (byte* p = buffer)
            fd = KernelFile.sceKernelOpen(p, KernelFile.ReadOnly, 0);
        return SceResult.ThrowIfFailed(fd, nameof(KernelFile.sceKernelOpen));
    }
}
