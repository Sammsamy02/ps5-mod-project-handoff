// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using SharpProspero.Storage;
using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// The optical (Blu-ray) disc drive. The system mounts a recognised disc's filesystem under
/// <see cref="MountPoint"/>, which is read with the ordinary file APIs; the raw block device is opened
/// for a sector-level dump. Both need the module to run with enough privilege to reach the device, which
/// a normal application sandbox does not have, so treat the reads as best-effort and handle failure.
/// </summary>
/// <remarks>
/// There is no dedicated disc service to call: the drive is reached through the standard file system and
/// the device node. Sectors read straight from the device are as the drive returns them, which for a
/// commercial disc is protected content; the readable files of a recognised disc are the ones the system
/// has mounted under <see cref="MountPoint"/>.
/// </remarks>
public static class DiscDrive
{
    /// <summary>Where the system mounts a recognised disc's filesystem.</summary>
    public const string MountPoint = "/mnt/disc";

    /// <summary>The primary optical drive block device.</summary>
    public const string PrimaryDevice = "/dev/cd0";

    /// <summary>The secondary optical drive block device.</summary>
    public const string SecondaryDevice = "/dev/cd1";

    /// <summary>True when a disc's filesystem is mounted and readable under <see cref="MountPoint"/>.</summary>
    public static bool IsDiscMounted => FileSystem.Exists(MountPoint);

    /// <summary>
    /// Lists the entries of the mounted disc at <paramref name="subPath"/> beneath <see cref="MountPoint"/>
    /// (empty for the root), so a tool can browse a disc's contents with the ordinary file APIs.
    /// </summary>
    /// <exception cref="ProsperoException">The disc is not mounted or the path could not be read.</exception>
    public static IReadOnlyList<DirectoryEntry> EnumerateFiles(string subPath = "")
    {
        string path = string.IsNullOrEmpty(subPath) ? MountPoint : $"{MountPoint}/{subPath.TrimStart('/')}";
        return FileSystem.EnumerateDirectory(path);
    }

    /// <summary>Opens the raw disc block device for sector reads and dumping.</summary>
    /// <param name="devicePath">The device node, <see cref="PrimaryDevice"/> by default.</param>
    /// <exception cref="ProsperoException">The device could not be opened (no disc, or not enough privilege).</exception>
    public static DiscDevice OpenDevice(string devicePath = PrimaryDevice) => DiscDevice.Open(devicePath);

    /// <summary>Opens the raw disc block device, returning false instead of throwing when it cannot be opened.</summary>
    public static bool TryOpenDevice(out DiscDevice? device, string devicePath = PrimaryDevice)
    {
        try
        {
            device = DiscDevice.Open(devicePath);
            return true;
        }
        catch (ProsperoException)
        {
            device = null;
            return false;
        }
    }
}

/// <summary>
/// An open handle on the raw disc block device: read sectors sequentially or at an offset, or dump the
/// whole disc to a file. What the device returns is the drive's raw content. Dispose it when finished.
/// </summary>
public sealed unsafe class DiscDevice : IDisposable
{
    private int _fd;
    private bool _disposed;

    private DiscDevice(int fd) => _fd = fd;

    /// <summary>Opens the device node at <paramref name="devicePath"/> for reading.</summary>
    /// <exception cref="ProsperoException">The device could not be opened.</exception>
    public static DiscDevice Open(string devicePath)
    {
        ArgumentException.ThrowIfNullOrEmpty(devicePath);
        int byteCount = Encoding.UTF8.GetByteCount(devicePath);
        Span<byte> buffer = byteCount < 256 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(devicePath, buffer);
        buffer[byteCount] = 0;

        int fd;
        fixed (byte* p = buffer)
            fd = KernelFile.sceKernelOpen(p, KernelFile.ReadOnly, 0);
        SceResult.ThrowIfFailed(fd, nameof(KernelFile.sceKernelOpen));
        return new DiscDevice(fd);
    }

    /// <summary>Reads the next bytes into <paramref name="buffer"/>; returns the count read, or zero at the end.</summary>
    /// <exception cref="ProsperoException">The read failed.</exception>
    public int Read(Span<byte> buffer)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (buffer.IsEmpty)
            return 0;
        long read;
        fixed (byte* p = buffer)
            read = KernelFile.sceKernelRead(_fd, p, (nuint)buffer.Length);
        if (read < 0)
            throw new ProsperoException(nameof(KernelFile.sceKernelRead), (int)read);
        return (int)read;
    }

    /// <summary>Moves the read position to <paramref name="offset"/> bytes from the start.</summary>
    /// <exception cref="ProsperoException">The seek failed.</exception>
    public void Seek(long offset)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentOutOfRangeException.ThrowIfNegative(offset);
        long result = KernelFile.sceKernelLseek(_fd, offset, KernelFile.SeekSet);
        if (result < 0)
            throw new ProsperoException(nameof(KernelFile.sceKernelLseek), (int)result);
    }

    /// <summary>
    /// Copies the whole device to the file at <paramref name="outputPath"/>, reading until the end.
    /// <paramref name="onProgress"/>, if given, is called with the running byte total. Returns the total
    /// bytes written.
    /// </summary>
    /// <exception cref="ProsperoException">Opening the output or reading or writing failed.</exception>
    public long DumpTo(string outputPath, Action<long>? onProgress = null)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(outputPath);

        int output = OpenForWrite(outputPath);
        try
        {
            byte[] chunk = new byte[1 << 20]; // 1 MiB
            long total = 0;
            fixed (byte* p = chunk)
            {
                while (true)
                {
                    long read = KernelFile.sceKernelRead(_fd, p, (nuint)chunk.Length);
                    if (read < 0)
                        throw new ProsperoException(nameof(KernelFile.sceKernelRead), (int)read);
                    if (read == 0)
                        break;

                    long written = 0;
                    while (written < read)
                    {
                        long w = KernelFile.sceKernelWrite(output, p + written, (nuint)(read - written));
                        if (w <= 0)
                            throw new ProsperoException(nameof(KernelFile.sceKernelWrite), w < 0 ? (int)w : 0);
                        written += w;
                    }
                    total += read;
                    onProgress?.Invoke(total);
                }
            }
            return total;
        }
        finally
        {
            KernelFile.sceKernelClose(output);
        }
    }

    private static int OpenForWrite(string path)
    {
        int byteCount = Encoding.UTF8.GetByteCount(path);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(path, buffer);
        buffer[byteCount] = 0;
        int fd;
        fixed (byte* p = buffer)
            fd = KernelFile.sceKernelOpen(p, KernelFile.WriteOnly | KernelFile.Create | KernelFile.Truncate, 0x1B6);
        return SceResult.ThrowIfFailed(fd, nameof(KernelFile.sceKernelOpen));
    }

    /// <summary>Closes the device handle.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_fd >= 0)
        {
            KernelFile.sceKernelClose(_fd);
            _fd = -1;
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Closes the device handle if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~DiscDevice() => Dispose();
}
