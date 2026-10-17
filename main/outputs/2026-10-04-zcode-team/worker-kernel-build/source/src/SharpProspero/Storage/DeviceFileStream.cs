// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.IO;
using System.Text;

namespace SharpProspero.Storage;

/// <summary>
/// A file opened for reading or writing in pieces. This is the way to work with a file larger than the
/// heap - a disc image, a video, a save state, a download in progress - because nothing here ever holds
/// more than the caller's own buffer.
/// </summary>
/// <remarks>
/// <para>
/// Every call goes straight to the file system, so a run of small reads is a run of system calls. Wrap
/// the stream in a <see cref="BufferedStream"/> when reading or writing a few bytes at a time; read into
/// a large span when reading in bulk.
/// </para>
/// <para>
/// The stream is not safe to use from several threads at once, because the file offset is shared.
/// <see cref="ReadAt"/> and <see cref="WriteAt"/> do not use that offset and may be called concurrently
/// on one open file.
/// </para>
/// </remarks>
/// <example>
/// Copy a file of any size through a small buffer:
/// <code>
/// using var source = DeviceFileStream.OpenRead("/app0/big.bin");
/// using var target = DeviceFileStream.Create("/data/big.bin");
/// source.CopyTo(target, 1 &lt;&lt; 20);
/// </code>
/// </example>
public sealed unsafe class DeviceFileStream : Stream
{
    // Permission bits a created file is given: readable and writable by its owner and its group,
    // readable by anyone else.
    private const ushort CreateMode = 0x1B4;

    private readonly int _descriptor;
    private readonly bool _canRead;
    private readonly bool _canWrite;
    private readonly bool _append;
    private long _position;
    private bool _closed;

    private DeviceFileStream(int descriptor, bool canRead, bool canWrite, bool append, long position)
    {
        _descriptor = descriptor;
        _canRead = canRead;
        _canWrite = canWrite;
        _append = append;
        _position = position;
    }

    /// <summary>The descriptor the file system knows this file by.</summary>
    public int Descriptor => _descriptor;

    /// <summary>Whether the stream was opened for reading.</summary>
    public override bool CanRead => !_closed && _canRead;

    /// <summary>Whether the stream was opened for writing.</summary>
    public override bool CanWrite => !_closed && _canWrite;

    /// <summary>Whether the offset can be moved. True for every open file.</summary>
    public override bool CanSeek => !_closed;

    /// <summary>Opens an existing file for reading.</summary>
    /// <exception cref="ProsperoException">The file could not be opened.</exception>
    public static DeviceFileStream OpenRead(string path) => Open(path, FileMode.Open, FileAccess.Read);

    /// <summary>Creates the file, or empties it when it already exists, and opens it for writing.</summary>
    /// <exception cref="ProsperoException">The file could not be opened.</exception>
    public static DeviceFileStream Create(string path) => Open(path, FileMode.Create, FileAccess.Write);

    /// <summary>
    /// Opens the file for writing at its end, creating it when it is not there. Each write lands at the
    /// end whatever the offset was, so several writers can append without treading on each other.
    /// </summary>
    /// <exception cref="ProsperoException">The file could not be opened.</exception>
    public static DeviceFileStream OpenAppend(string path) => Open(path, FileMode.Append, FileAccess.Write);

    /// <summary>
    /// Opens <paramref name="path"/> with the given mode and access.
    /// </summary>
    /// <exception cref="ArgumentException">The mode and access do not go together.</exception>
    /// <exception cref="ProsperoException">The file could not be opened.</exception>
    public static DeviceFileStream Open(string path, FileMode mode, FileAccess access = FileAccess.ReadWrite)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        int flags = ToOpenFlags(mode, access);

        int byteCount = Encoding.UTF8.GetByteCount(path);
        Span<byte> owned = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(path, owned);
        owned[byteCount] = 0;

        int fd;
        fixed (byte* p = owned)
            fd = KernelFile.sceKernelOpen(p, flags, CreateMode);
        SceResult.ThrowIfFailed(fd, nameof(KernelFile.sceKernelOpen));

        bool append = mode == FileMode.Append;
        // An appending stream starts where the file ends, so a caller reading Position straight after
        // opening is told where its first write will land rather than nought.
        long start = 0;
        if (append || mode == FileMode.Open)
        {
            long end = KernelFile.sceKernelLseek(fd, 0, KernelFile.SeekEnd);
            if (end < 0)
            {
                KernelFile.sceKernelClose(fd);
                throw new ProsperoException(nameof(KernelFile.sceKernelLseek), (int)end);
            }
            if (append)
                start = end;
            else
                KernelFile.sceKernelLseek(fd, 0, KernelFile.SeekSet);
        }

        return new DeviceFileStream(fd, (access & FileAccess.Read) != 0, (access & FileAccess.Write) != 0, append, start);
    }

    /// <summary>
    /// The open flags that stand for <paramref name="mode"/> and <paramref name="access"/>.
    /// </summary>
    /// <remarks>
    /// The file system takes one flag word rather than a mode and an access, so the pair is folded into
    /// one here. A truncating mode also needs write access, which is why the combinations that cannot be
    /// expressed are refused rather than quietly narrowed.
    /// </remarks>
    /// <exception cref="ArgumentException">The mode and access do not go together.</exception>
    public static int ToOpenFlags(FileMode mode, FileAccess access)
    {
        int flags = access switch
        {
            FileAccess.Read => KernelFile.ReadOnly,
            FileAccess.Write => KernelFile.WriteOnly,
            FileAccess.ReadWrite => KernelFile.ReadWrite,
            _ => throw new ArgumentOutOfRangeException(nameof(access)),
        };

        bool writable = (access & FileAccess.Write) != 0;
        switch (mode)
        {
            case FileMode.Open:
                break;
            case FileMode.OpenOrCreate:
                Require(writable, mode, access);
                flags |= KernelFile.Create;
                break;
            case FileMode.Create:
                Require(writable, mode, access);
                flags |= KernelFile.Create | KernelFile.Truncate;
                break;
            case FileMode.CreateNew:
                Require(writable, mode, access);
                flags |= KernelFile.Create | KernelFile.Exclusive;
                break;
            case FileMode.Truncate:
                Require(writable, mode, access);
                flags |= KernelFile.Truncate;
                break;
            case FileMode.Append:
                if (access != FileAccess.Write)
                    throw new ArgumentException("Appending is a write-only mode.", nameof(access));
                flags |= KernelFile.Create | KernelFile.Append;
                break;
            default:
                throw new ArgumentOutOfRangeException(nameof(mode));
        }
        return flags;

        static void Require(bool writable, FileMode mode, FileAccess access)
        {
            if (!writable)
                throw new ArgumentException($"{mode} needs write access, not {access}.", nameof(access));
        }
    }

    /// <summary>The length of the file in bytes.</summary>
    /// <exception cref="ProsperoException">The length could not be read.</exception>
    public override long Length
    {
        get
        {
            ObjectDisposedException.ThrowIf(_closed, this);
            long end = KernelFile.sceKernelLseek(_descriptor, 0, KernelFile.SeekEnd);
            if (end < 0)
                throw new ProsperoException(nameof(KernelFile.sceKernelLseek), (int)end);
            // Reading the length must not move the offset the next read or write uses.
            KernelFile.sceKernelLseek(_descriptor, _position, KernelFile.SeekSet);
            return end;
        }
    }

    /// <summary>Where the next read or write starts.</summary>
    public override long Position
    {
        get => _position;
        set => Seek(value, SeekOrigin.Begin);
    }

    /// <summary>Moves the offset and returns where it landed.</summary>
    /// <exception cref="ProsperoException">The offset could not be moved.</exception>
    public override long Seek(long offset, SeekOrigin origin)
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        int whence = origin switch
        {
            SeekOrigin.Begin => KernelFile.SeekSet,
            SeekOrigin.Current => KernelFile.SeekCurrent,
            SeekOrigin.End => KernelFile.SeekEnd,
            _ => throw new ArgumentOutOfRangeException(nameof(origin)),
        };
        long landed = KernelFile.sceKernelLseek(_descriptor, offset, whence);
        if (landed < 0)
            throw new ProsperoException(nameof(KernelFile.sceKernelLseek), (int)landed);
        _position = landed;
        return landed;
    }

    /// <summary>
    /// Reads up to <paramref name="buffer"/>'s length of bytes from the current offset and returns how
    /// many arrived. A return of zero means the end of the file.
    /// </summary>
    /// <exception cref="ProsperoException">The read failed.</exception>
    public override int Read(Span<byte> buffer)
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        if (!_canRead)
            throw new NotSupportedException("The stream was not opened for reading.");
        if (buffer.Length == 0)
            return 0;

        long read;
        fixed (byte* p = buffer)
            read = KernelFile.sceKernelRead(_descriptor, p, (nuint)buffer.Length);
        if (read < 0)
            throw new ProsperoException(nameof(KernelFile.sceKernelRead), (int)read);
        _position += read;
        return (int)read;
    }

    /// <summary>
    /// Reads up to <paramref name="count"/> bytes into <paramref name="buffer"/> at
    /// <paramref name="offset"/> and returns how many arrived.
    /// </summary>
    public override int Read(byte[] buffer, int offset, int count)
    {
        ValidateBufferArguments(buffer, offset, count);
        return Read(buffer.AsSpan(offset, count));
    }

    /// <summary>Writes the whole of <paramref name="buffer"/> at the current offset.</summary>
    /// <exception cref="ProsperoException">The write failed or stopped short.</exception>
    public override void Write(ReadOnlySpan<byte> buffer)
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        if (!_canWrite)
            throw new NotSupportedException("The stream was not opened for writing.");

        int written = 0;
        fixed (byte* p = buffer)
        {
            while (written < buffer.Length)
            {
                long n = KernelFile.sceKernelWrite(_descriptor, p + written, (nuint)(buffer.Length - written));
                if (n < 0)
                    throw new ProsperoException(nameof(KernelFile.sceKernelWrite), (int)n);
                // A write that takes nothing and reports no error would spin here forever; a full device
                // shows up this way, so it is reported rather than waited on.
                if (n == 0)
                    throw new ProsperoException(nameof(KernelFile.sceKernelWrite), -1);
                written += (int)n;
                _position += n;
            }
        }
    }

    /// <summary>Writes <paramref name="count"/> bytes from <paramref name="buffer"/> at <paramref name="offset"/>.</summary>
    public override void Write(byte[] buffer, int offset, int count)
    {
        ValidateBufferArguments(buffer, offset, count);
        Write(buffer.AsSpan(offset, count));
    }

    /// <summary>
    /// Reads at <paramref name="fileOffset"/> without moving the stream's own offset, and returns how
    /// many bytes arrived. Several threads may call this on one open file at once.
    /// </summary>
    /// <exception cref="ProsperoException">The read failed.</exception>
    public int ReadAt(long fileOffset, Span<byte> buffer)
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        ArgumentOutOfRangeException.ThrowIfNegative(fileOffset);
        if (!_canRead)
            throw new NotSupportedException("The stream was not opened for reading.");
        if (buffer.Length == 0)
            return 0;

        long read;
        fixed (byte* p = buffer)
            read = KernelFile.pread(_descriptor, p, (nuint)buffer.Length, fileOffset);
        if (read < 0)
            throw new ProsperoException(nameof(KernelFile.pread), (int)read);
        return (int)read;
    }

    /// <summary>
    /// Fills <paramref name="buffer"/> from <paramref name="fileOffset"/>, reading again until it is
    /// full. Throws when the file ends first.
    /// </summary>
    /// <exception cref="EndOfStreamException">The file ended before the buffer was full.</exception>
    /// <exception cref="ProsperoException">A read failed.</exception>
    public void ReadExactlyAt(long fileOffset, Span<byte> buffer)
    {
        int filled = 0;
        while (filled < buffer.Length)
        {
            int n = ReadAt(fileOffset + filled, buffer[filled..]);
            if (n == 0)
                throw new EndOfStreamException($"The file ended {buffer.Length - filled} bytes short.");
            filled += n;
        }
    }

    /// <summary>
    /// Writes <paramref name="buffer"/> at <paramref name="fileOffset"/> without moving the stream's own
    /// offset. Not usable on a stream opened for appending, where every write lands at the end.
    /// </summary>
    /// <exception cref="ProsperoException">The write failed or stopped short.</exception>
    public void WriteAt(long fileOffset, ReadOnlySpan<byte> buffer)
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        ArgumentOutOfRangeException.ThrowIfNegative(fileOffset);
        if (!_canWrite)
            throw new NotSupportedException("The stream was not opened for writing.");
        if (_append)
            throw new NotSupportedException("An appending stream writes at the end, not at an offset.");

        int written = 0;
        fixed (byte* p = buffer)
        {
            while (written < buffer.Length)
            {
                long n = KernelFile.pwrite(_descriptor, p + written, (nuint)(buffer.Length - written), fileOffset + written);
                if (n <= 0)
                    throw new ProsperoException(nameof(KernelFile.pwrite), n < 0 ? (int)n : -1);
                written += (int)n;
            }
        }
    }

    /// <summary>
    /// Sets the file length. Growing it leaves the added bytes reading as zero without writing them,
    /// which is how an output file is sized up front.
    /// </summary>
    /// <exception cref="ProsperoException">The length could not be set.</exception>
    public override void SetLength(long value)
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        ArgumentOutOfRangeException.ThrowIfNegative(value);
        if (!_canWrite)
            throw new NotSupportedException("The stream was not opened for writing.");
        if (KernelFile.ftruncate(_descriptor, value) != 0)
            throw new ProsperoException(nameof(KernelFile.ftruncate), -1);
        if (_position > value)
            Seek(value, SeekOrigin.Begin);
    }

    /// <summary>
    /// Does nothing: a write has already reached the file system when it returns. Call
    /// <see cref="Sync"/> to make the file system commit what it is holding to the device.
    /// </summary>
    public override void Flush()
    {
    }

    /// <summary>
    /// Waits for everything written to reach the device, so a loss of power afterwards cannot lose it.
    /// This is slow; call it when a save is finished rather than after each write.
    /// </summary>
    /// <exception cref="ProsperoException">The file system refused to commit.</exception>
    public void Sync()
    {
        ObjectDisposedException.ThrowIf(_closed, this);
        if (KernelFile.fsync(_descriptor) != 0)
            throw new ProsperoException(nameof(KernelFile.fsync), -1);
    }

    /// <summary>Closes the file.</summary>
    protected override void Dispose(bool disposing)
    {
        if (!_closed)
        {
            _closed = true;
            KernelFile.sceKernelClose(_descriptor);
        }
        base.Dispose(disposing);
    }
}
