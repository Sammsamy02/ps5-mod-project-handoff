// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// Reads the system values the kernel publishes under dotted names, such as <c>hw.ncpu</c> or
/// <c>hw.acpi.thermal.tz0.temperature</c>. Every value is a block of bytes with a name and a size; the
/// readers here cover the three shapes that block takes in practice - a fixed-width integer, a
/// NUL-terminated string, and an opaque run of bytes - and each has a <c>Try</c> form because a name
/// that is absent, or a value an unprivileged process may not read, is an ordinary outcome rather than
/// an error worth an exception.
/// </summary>
/// <remarks>
/// Which names exist depends on what the running kernel configured and what the platform firmware
/// declared, so a name that answers on one machine can be missing on another. Treat a false return as
/// "this machine does not publish that", read <see cref="LastErrorNumber"/> straight afterwards if the
/// difference between absent and refused matters, and never assume a name is present because it was
/// present once.
/// </remarks>
public static unsafe class Sysctl
{
    /// <summary>The error number reported when no value is published under the requested name.</summary>
    public const int NotPresentError = 2;

    /// <summary>The error number reported when the caller may not read the requested value.</summary>
    public const int NotPermittedError = 1;

    /// <summary>The error number reported when the supplied buffer is too small for the value.</summary>
    public const int BufferTooSmallError = 12;

    // The value can change size between asking how big it is and reading it. Re-asking once is enough
    // for the values that vary; a run of failures means something else is wrong and is reported as one.
    private const int GrowRetries = 3;

    /// <summary>
    /// The system error number the calling thread's most recent platform call left behind. Read it
    /// immediately after a <c>Try</c> method returned false; anything else the thread does in between
    /// can replace it. <see cref="NotPresentError"/> means the name is not published,
    /// <see cref="NotPermittedError"/> means the process may not read it.
    /// </summary>
    public static int LastErrorNumber => *KernelSystem.__error();

    /// <summary>
    /// Reports whether the system publishes a value under <paramref name="name"/>. This asks for the
    /// value's size, so a name the process may not read reports false in the same way an absent one
    /// does; <see cref="LastErrorNumber"/> tells the two apart.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool Exists(string name) => TryGetSize(name, out _);

    /// <summary>
    /// The size in bytes of the value published under <paramref name="name"/>.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The size could not be read.</exception>
    public static int GetSize(string name)
        => TryGetSize(name, out int size) ? size : throw Failure(name);

    /// <summary>
    /// Reads the size in bytes of the value published under <paramref name="name"/> into
    /// <paramref name="size"/>. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryGetSize(string name, out int size)
    {
        Validate(name);
        size = 0;
        nuint length = 0;
        if (KernelSystem.sysctlbyname(name, null, &length, null, 0) != 0)
            return false;
        if (length > int.MaxValue)
            return false;
        size = (int)length;
        return true;
    }

    /// <summary>
    /// Reads the 32-bit value published under <paramref name="name"/>.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static int ReadInt32(string name)
        => TryReadInt32(name, out int value) ? value : throw Failure(name);

    /// <summary>
    /// Reads the 32-bit value published under <paramref name="name"/> into <paramref name="value"/>.
    /// Returns false when the machine will not answer, or when what it publishes under that name is not
    /// four bytes wide.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryReadInt32(string name, out int value)
    {
        Validate(name);
        value = 0;
        int read = 0;
        nuint length = sizeof(int);
        if (KernelSystem.sysctlbyname(name, &read, &length, null, 0) != 0)
            return false;
        if (length != sizeof(int))
            return false;
        value = read;
        return true;
    }

    /// <summary>
    /// Reads the unsigned 32-bit value published under <paramref name="name"/>.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static uint ReadUInt32(string name)
        => TryReadUInt32(name, out uint value) ? value : throw Failure(name);

    /// <summary>
    /// Reads the unsigned 32-bit value published under <paramref name="name"/> into
    /// <paramref name="value"/>. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryReadUInt32(string name, out uint value)
    {
        bool ok = TryReadInt32(name, out int read);
        value = unchecked((uint)read);
        return ok;
    }

    /// <summary>
    /// Reads the 64-bit value published under <paramref name="name"/>.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static long ReadInt64(string name)
        => TryReadInt64(name, out long value) ? value : throw Failure(name);

    /// <summary>
    /// Reads the 64-bit value published under <paramref name="name"/> into <paramref name="value"/>.
    /// Returns false when the machine will not answer, or when what it publishes under that name is not
    /// eight bytes wide.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryReadInt64(string name, out long value)
    {
        Validate(name);
        value = 0;
        long read = 0;
        nuint length = sizeof(long);
        if (KernelSystem.sysctlbyname(name, &read, &length, null, 0) != 0)
            return false;
        if (length != sizeof(long))
            return false;
        value = read;
        return true;
    }

    /// <summary>
    /// Reads the unsigned 64-bit value published under <paramref name="name"/>.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static ulong ReadUInt64(string name)
        => TryReadUInt64(name, out ulong value) ? value : throw Failure(name);

    /// <summary>
    /// Reads the unsigned 64-bit value published under <paramref name="name"/> into
    /// <paramref name="value"/>. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryReadUInt64(string name, out ulong value)
    {
        bool ok = TryReadInt64(name, out long read);
        value = unchecked((ulong)read);
        return ok;
    }

    /// <summary>
    /// Reads the text published under <paramref name="name"/>, without the terminating NUL.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static string ReadString(string name)
        => TryReadString(name, out string value) ? value : throw Failure(name);

    /// <summary>
    /// Reads the text published under <paramref name="name"/> into <paramref name="value"/>, without
    /// the terminating NUL. Returns false when the machine will not answer.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryReadString(string name, out string value)
    {
        value = string.Empty;
        if (!TryReadRaw(name, out byte[] raw))
            return false;
        value = DecodeString(raw);
        return true;
    }

    /// <summary>
    /// Reads the raw bytes published under <paramref name="name"/>.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static byte[] ReadRaw(string name)
        => TryReadRaw(name, out byte[] value) ? value : throw Failure(name);

    /// <summary>
    /// Reads the raw bytes published under <paramref name="name"/> into <paramref name="value"/>,
    /// allocating a buffer of the size the system reports. Returns false when the machine will not
    /// answer.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="name"/> is empty or contains a NUL.</exception>
    public static bool TryReadRaw(string name, out byte[] value)
    {
        Validate(name);
        value = [];
        if (!TryGetSize(name, out int size))
            return false;
        if (size == 0)
            return true;

        for (int attempt = 0; attempt < GrowRetries; attempt++)
        {
            byte[] buffer = new byte[size];
            nuint length = (nuint)buffer.Length;
            int result;
            fixed (byte* destination = buffer)
                result = KernelSystem.sysctlbyname(name, destination, &length, null, 0);

            if (result == 0)
            {
                value = length == (nuint)buffer.Length ? buffer : buffer[..(int)length];
                return true;
            }

            // Only a value that grew between the two calls is worth another attempt.
            if (LastErrorNumber != BufferTooSmallError || !TryGetSize(name, out size) || size == 0)
                return false;
        }

        return false;
    }

    /// <summary>
    /// Reads the raw bytes published under <paramref name="name"/> into <paramref name="destination"/>
    /// and reports how many were written. Returns false when the machine will not answer or when
    /// <paramref name="destination"/> is too small, in which case <see cref="LastErrorNumber"/> is
    /// <see cref="BufferTooSmallError"/>.
    /// </summary>
    /// <exception cref="ArgumentException">
    /// <paramref name="name"/> is empty or contains a NUL, or <paramref name="destination"/> is empty.
    /// </exception>
    public static bool TryReadRaw(string name, Span<byte> destination, out int written)
    {
        Validate(name);
        written = 0;
        if (destination.IsEmpty)
            throw new ArgumentException("The destination needs room for at least one byte.", nameof(destination));

        nuint length = (nuint)destination.Length;
        int result;
        fixed (byte* buffer = destination)
            result = KernelSystem.sysctlbyname(name, buffer, &length, null, 0);
        if (result != 0)
            return false;
        written = (int)length;
        return true;
    }

    /// <summary>
    /// Returns the text held in <paramref name="raw"/> up to the first NUL, decoded as UTF-8. This is
    /// the shape a text value comes back in, split out so it can be applied to a block read on its own.
    /// </summary>
    public static string DecodeString(ReadOnlySpan<byte> raw)
    {
        int end = raw.IndexOf((byte)0);
        return Encoding.UTF8.GetString(end < 0 ? raw : raw[..end]);
    }

    private static void Validate(string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        if (name.Contains('\0', StringComparison.Ordinal))
            throw new ArgumentException("A system value name cannot contain a NUL character.", nameof(name));
    }

    private static ProsperoException Failure(string name)
        => new($"sysctlbyname(\"{name}\")", SceResult.KernelFacility | (LastErrorNumber & 0xFFFF));
}
