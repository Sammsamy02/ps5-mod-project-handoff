// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.IO;

/// <summary>
/// Low-level file descriptor I/O for a payload context. Wraps the POSIX <c>open</c>,
/// <c>close</c>, <c>read</c>, <c>write</c>, <c>lseek</c>, <c>pread</c>, <c>pwrite</c>,
/// <c>fstat</c>, <c>ioctl</c>, <c>pipe</c>, <c>dup2</c>, and <c>fcntl</c> from <c>libkernel</c>.
/// </summary>
public static unsafe partial class PayloadIo
{
    private const string Lib = "libkernel";

    /// <summary>Opens a file at <paramref name="path"/> with <paramref name="flags"/> and
    /// <paramref name="mode"/> permissions.</summary>
    /// <returns>A file descriptor on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int open(byte* path, int flags, ushort mode);

    /// <summary>Opens a file at <paramref name="path"/> with <paramref name="flags"/>.</summary>
    /// <returns>A file descriptor on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int open(byte* path, int flags);

    /// <summary>Closes a file descriptor.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int close(int fd);

    /// <summary>Reads up to <paramref name="nbytes"/> from <paramref name="fd"/>.</summary>
    /// <returns>The number of bytes read, 0 at EOF, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial long read(int fd, void* buf, nuint nbytes);

    /// <summary>Writes up to <paramref name="nbytes"/> to <paramref name="fd"/>.</summary>
    /// <returns>The number of bytes written, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial long write(int fd, void* buf, nuint nbytes);

    /// <summary>Repositions the file offset of <paramref name="fd"/>.</summary>
    /// <param name="fd">The file descriptor.</param>
    /// <param name="offset">The new offset, relative to <paramref name="whence"/>.</param>
    /// <param name="whence">One of <see cref="SeekSet"/>, <see cref="SeekCur"/>, or
    /// <see cref="SeekEnd"/>.</param>
    /// <returns>The resulting offset from the beginning of the file, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial long lseek(int fd, long offset, int whence);

    /// <summary>Reads up to <paramref name="nbytes"/> from <paramref name="fd"/> at the given
    /// <paramref name="offset"/> without changing the file position.</summary>
    /// <returns>The number of bytes read, 0 at EOF, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial long pread(int fd, void* buf, nuint nbytes, long offset);

    /// <summary>Writes up to <paramref name="nbytes"/> to <paramref name="fd"/> at the given
    /// <paramref name="offset"/> without changing the file position.</summary>
    /// <returns>The number of bytes written, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial long pwrite(int fd, void* buf, nuint nbytes, long offset);

    /// <summary>Reads file status of an open file descriptor.</summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int fstat(int fd, FreeBsdStat* buf);

    /// <summary>
    /// Performs a device-specific control operation on <paramref name="fd"/>. The
    /// <paramref name="request"/> and <paramref name="arg"/> are device-dependent.
    /// </summary>
    /// <returns>Zero on success for most requests, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int ioctl(int fd, ulong request, void* arg);

    /// <summary>
    /// Creates an unidirectional data channel (pipe). On success, <paramref name="fildes"/>[0]
    /// is the read end and <paramref name="fildes"/>[1] is the write end.
    /// </summary>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int pipe(int* fildes);

    /// <summary>
    /// Duplicates <paramref name="oldfd"/> onto <paramref name="newfd"/>, closing
    /// <paramref name="newfd"/> first if it is open.
    /// </summary>
    /// <returns>The new file descriptor on success, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int dup2(int oldfd, int newfd);

    /// <summary>
    /// Performs a file control operation on <paramref name="fd"/>.
    /// </summary>
    /// <returns>Operation-specific value, or -1 on error.</returns>
    [LibraryImport(Lib)]
    public static partial int fcntl(int fd, int cmd, int arg);

    /// <summary>Seek from the beginning of the file.</summary>
    public const int SeekSet = 0;

    /// <summary>Seek from the current position.</summary>
    public const int SeekCur = 1;

    /// <summary>Seek from the end of the file.</summary>
    public const int SeekEnd = 2;

    /// <summary>Get file descriptor flags.</summary>
    public const int F_GETFD = 1;

    /// <summary>Set file descriptor flags.</summary>
    public const int F_SETFD = 2;

    /// <summary>Get file status flags.</summary>
    public const int F_GETFL = 3;

    /// <summary>Set file status flags.</summary>
    public const int F_SETFL = 4;

    /// <summary>Close-on-exec flag for <see cref="F_SETFD"/>.</summary>
    public const int FD_CLOEXEC = 1;

    /// <summary>Non-blocking I/O flag for <see cref="F_SETFL"/>.</summary>
    public const int O_NONBLOCK = 0x0004;

    /// <summary>File-backed memory mapping with full flag support.</summary>
    [LibraryImport(Lib)]
    public static partial void* mmap(void* addr, nuint len, int prot, int flags, int fd, long offset);

    /// <summary>Unmaps a previously mapped region.</summary>
    [LibraryImport(Lib)]
    public static partial int munmap(void* addr, nuint len);

    /// <summary>PROT_READ — pages may be read.</summary>
    public const int ProtRead = 0x01;

    /// <summary>PROT_WRITE — pages may be written.</summary>
    public const int ProtWrite = 0x02;

    /// <summary>PROT_EXEC — pages may be executed.</summary>
    public const int ProtExec = 0x04;

    /// <summary>MAP_SHARED — changes are visible to other processes.</summary>
    public const int MapShared = 0x0001;

    /// <summary>MAP_PRIVATE — copy-on-write private mapping.</summary>
    public const int MapPrivate = 0x0002;

    /// <summary>MAP_ANONYMOUS — not backed by a file.</summary>
    public const int MapAnon = 0x1000;
}
