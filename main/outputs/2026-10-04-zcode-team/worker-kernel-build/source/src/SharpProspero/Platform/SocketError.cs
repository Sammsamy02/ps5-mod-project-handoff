// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Net;

namespace SharpProspero.Platform;

/// <summary>
/// Reads the per-thread socket error and turns a failed socket call into an exception. A socket call
/// that fails returns a negative value; the specific reason is read from the network error location
/// rather than the process-wide error, which the socket layer does not set.
/// </summary>
internal static unsafe class SocketError
{
    /// <summary>The last network error on this thread, or zero when there is none.</summary>
    public static int Last()
    {
        int* location = Socket.sceNetErrnoLoc();
        return location != null ? *location : 0;
    }

    /// <summary>
    /// Throws when <paramref name="result"/> reports failure, reporting the network error, and returns
    /// the result unchanged on success.
    /// </summary>
    public static int Check(int result, string operation)
    {
        if (result >= 0)
            return result;
        throw Failure(result, operation);
    }

    /// <summary>
    /// Builds the exception for a failed <paramref name="result"/> without throwing, reading the network
    /// error now. Use it to capture the reason before a cleanup call (such as closing the socket) that
    /// could change the per-thread error.
    /// </summary>
    public static ProsperoException Failure(int result, string operation)
    {
        // A plain -1 means the reason is in the network error location; any other negative value is
        // already a specific code, so it is reported directly.
        int code = result == -1 ? Last() : result;
        return new ProsperoException(operation, code);
    }
}
