// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Compression;

/// <summary>
/// Asynchronous zlib decompression bindings. A decompression is submitted, waited on, and its result
/// read back; the service holds one work buffer the caller supplies at initialization.
/// </summary>
public static unsafe partial class Zlib
{
    private const string Lib = "libSceZlib";

    /// <summary>The largest output a single decompression produces.</summary>
    public const int MaxDestinationSize = 64 * 1024;

    /// <summary>The alignment the destination buffer must have.</summary>
    public const int DestinationAlignSize = 2 * 1024;

    /// <summary>The service was already initialized. A second init returns this.</summary>
    public const int AlreadyInitialized = unchecked((int)0x81120033);

    /// <summary>A call was made before the service was initialized.</summary>
    public const int NotInitialized = unchecked((int)0x81120032);

    /// <summary>The result is not ready yet.</summary>
    public const int Again = unchecked((int)0x8112000B);

    /// <summary>
    /// Initializes the service with a work buffer. The buffer must stay valid until
    /// <see cref="sceZlibFinalize"/>. Zero on success.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceZlibInitialize(void* buffer, nuint length);

    /// <summary>Shuts the service down.</summary>
    [LibraryImport(Lib)]
    public static partial int sceZlibFinalize();

    /// <summary>
    /// Submits a decompression, returning a request id in <paramref name="requestId"/>. The
    /// destination must be aligned to <see cref="DestinationAlignSize"/> and no larger than
    /// <see cref="MaxDestinationSize"/>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceZlibInflate(void* source, uint sourceLength, void* destination, uint destinationLength, ulong* requestId);

    /// <summary>Waits for a submitted request to finish. Pass a timeout in microseconds, or null to block.</summary>
    [LibraryImport(Lib)]
    public static partial int sceZlibWaitForDone(ulong* requestId, uint* timeout);

    /// <summary>Reads the result of a finished request: the bytes produced and the status.</summary>
    [LibraryImport(Lib)]
    public static partial int sceZlibGetResult(ulong requestId, uint* destinationLength, int* status);
}
