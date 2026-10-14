// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Interop;

/// <summary>
/// The system user identifier passed to services that act on behalf of a signed-in user. Use
/// <see cref="System"/> for background work that is not tied to a specific profile.
/// </summary>
public static class SceUser
{
    /// <summary>The system profile. Value 0xFF.</summary>
    public const int System = 0xFF;

    /// <summary>Every signed-in user, for a service that acts on all of them. Value 0xFE.</summary>
    public const int Everyone = 0xFE;

    /// <summary>No user. The value a user id holds before a real user is chosen.</summary>
    public const int Invalid = -1;
}

/// <summary>
/// Helpers for interpreting the 32-bit result codes returned by the interop bindings. A value of
/// zero (<see cref="Ok"/>) or any non-negative value is success; a negative value is an error whose
/// high half identifies the reporting module.
/// </summary>
public static class SceResult
{
    /// <summary>The success code shared by the services.</summary>
    public const int Ok = 0;

    /// <summary>True when <paramref name="code"/> reports failure.</summary>
    public static bool Failed(int code) => code < 0;

    /// <summary>True when <paramref name="code"/> reports success.</summary>
    public static bool Succeeded(int code) => code >= 0;

    /// <summary>
    /// Throws a <see cref="ProsperoException"/> when <paramref name="code"/> reports failure.
    /// Returns <paramref name="code"/> unchanged on success so calls can be chained.
    /// </summary>
    public static int ThrowIfFailed(int code, string operation)
    {
        if (code < 0)
            throw new ProsperoException(operation, code);
        return code;
    }

    /// <summary>
    /// The high half that the kernel's own calls stamp on a failure. The low half of such a code is the
    /// system error number the underlying call set.
    /// </summary>
    public const int KernelFacility = unchecked((int)0x80020000);

    /// <summary>
    /// True when <paramref name="code"/> came from a kernel call, so <see cref="ErrorNumber"/> can read
    /// the system error number out of it.
    /// </summary>
    public static bool IsKernelError(int code) => (code & unchecked((int)0xFFFF0000)) == KernelFacility;

    /// <summary>
    /// The system error number carried by <paramref name="code"/>, or zero when the code does not carry
    /// one. A kernel call reports failure by adding its error number to a fixed high half.
    /// </summary>
    public static int ErrorNumber(int code) => IsKernelError(code) ? code & 0xFFFF : 0;

    /// <summary>
    /// A short reason for <paramref name="code"/>, suitable for showing beside whatever the call was
    /// working on. Falls back to the raw value when the code carries no system error number.
    /// </summary>
    public static string Describe(int code)
    {
        if (code >= 0)
            return "succeeded";
        int number = ErrorNumber(code);
        string? reason = ErrorText(number);
        return reason is null
            ? $"failed (0x{unchecked((uint)code):X8})"
            : $"{reason} ({number})";
    }

    // Only the numbers a call can plausibly come back with are named; anything else is reported as a raw
    // code rather than guessed at.
    private static string? ErrorText(int number) => number switch
    {
        1 => "not permitted",
        2 => "no such path",
        5 => "device error",
        6 => "device not configured",
        9 => "bad descriptor",
        12 => "out of memory",
        13 => "permission denied",
        14 => "bad address",
        16 => "device busy",
        19 => "not supported by the device",
        20 => "not a directory",
        21 => "is a directory",
        22 => "invalid argument",
        23 => "too many open files on the system",
        24 => "too many open files",
        28 => "no space left",
        30 => "read-only",
        45 => "operation not supported",
        62 => "too many links to follow",
        63 => "name too long",
        66 => "directory not empty",
        70 => "handle no longer valid",
        78 => "not implemented",
        79 => "wrong kind of file",
        _ => null,
    };
}

/// <summary>
/// Raised when a service call reports a failure code. <see cref="Code"/> carries the raw value so
/// callers can branch on specific results.
/// </summary>
/// <remarks>Creates an exception describing a failed <paramref name="operation"/>.</remarks>
public sealed class ProsperoException(string operation, int code) : Exception($"{operation} failed (0x{unchecked((uint)code):X8}).")
{

    /// <summary>The name of the operation that failed.</summary>
    public string Operation { get; } = operation;

    /// <summary>The raw result code.</summary>
    public int Code { get; } = code;
}
