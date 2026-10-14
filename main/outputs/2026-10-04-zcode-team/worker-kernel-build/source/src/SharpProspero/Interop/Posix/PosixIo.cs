// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Posix;

/// <summary>
/// The platform's file descriptor read and write calls under their plain names.
/// </summary>
public static unsafe partial class PosixIo
{
    private const string Lib = "libScePosix";

    /// <summary>Opens a path, returning the file descriptor or a negative error.</summary>
    [LibraryImport(Lib)]
    public static partial int open(byte* path, int flags);

    /// <summary>Reads up to <paramref name="nbytes"/> from <paramref name="fd"/> into <paramref name="buf"/>.</summary>
    [LibraryImport(Lib)]
    public static partial long read(int fd, void* buf, ulong nbytes);

    /// <summary>Writes up to <paramref name="nbytes"/> from <paramref name="buf"/> to <paramref name="fd"/>.</summary>
    [LibraryImport(Lib)]
    public static partial long write(int fd, void* buf, ulong nbytes);

    /// <summary>Returns the process identifier of the calling process.</summary>
    [LibraryImport(Lib)]
    public static partial int getpid();
}
