// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Diagnostics;

/// <summary>
/// A log sink that appends lines to a file, for a log a user can read back after a run (for example
/// <c>/data/app.log</c>). Open it, add it to <see cref="Log"/>, and dispose it at shutdown. Each line is
/// appended, so a log survives across runs until the file is removed.
/// </summary>
public sealed unsafe class FileLogSink : ILogSink, IDisposable
{
    private int _fd;
    private bool _disposed;

    private FileLogSink(int fd) => _fd = fd;

    /// <summary>Opens (creating if needed) the log file at <paramref name="path"/> for appending.</summary>
    /// <exception cref="ProsperoException">The file could not be opened.</exception>
    public static FileLogSink Open(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        int byteCount = Encoding.UTF8.GetByteCount(path);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(path, buffer);
        buffer[byteCount] = 0;

        int fd;
        fixed (byte* p = buffer)
            fd = KernelFile.sceKernelOpen(p, KernelFile.WriteOnly | KernelFile.Create | KernelFile.Append, 0x1B6);
        SceResult.ThrowIfFailed(fd, nameof(KernelFile.sceKernelOpen));
        return new FileLogSink(fd);
    }

    /// <inheritdoc/>
    public void Write(LogLevel level, string message)
    {
        if (_disposed)
            return;
        string line = LogFormat.Line(level, message);
        byte[] bytes = Encoding.UTF8.GetBytes(line + "\n");
        fixed (byte* p = bytes)
            KernelFile.sceKernelWrite(_fd, p, (nuint)bytes.Length);
    }

    /// <summary>Closes the log file.</summary>
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
    }
}
