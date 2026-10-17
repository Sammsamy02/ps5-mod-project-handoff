// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Compression;
using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Storage;

/// <summary>
/// Decompresses zlib-compressed data through the system service, for reading compressed assets or
/// archive members in a file explorer. Create one, decompress as many blocks as needed, dispose it.
/// </summary>
/// <remarks>
/// Each call produces at most 64 KiB. The service holds one work buffer for the life of the object;
/// the SDK does not state a required size for it, so <see cref="Create"/> takes it as a parameter with
/// a usable default. There is one service instance for the whole module and it counts no holders, so
/// only one decompressor can exist at a time: while one is alive, <see cref="Create"/> fails.
/// </remarks>
/// <example>
/// <code>
/// using var zlib = ZlibDecompressor.Create();
/// byte[] plain = zlib.Decompress(compressed);
/// </code>
/// </example>
public sealed unsafe class ZlibDecompressor : IDisposable
{
    private void* _workBuffer;
    private void* _destination;
    private bool _disposed;

    private ZlibDecompressor(void* workBuffer, void* destination)
    {
        _workBuffer = workBuffer;
        _destination = destination;
    }

    /// <summary>
    /// Initializes the decompression service with a work buffer of <paramref name="workBufferSize"/>
    /// bytes.
    /// </summary>
    /// <exception cref="ProsperoException">
    /// The service could not be initialized. A decompressor already holds it, which the service reports
    /// as <see cref="Zlib.AlreadyInitialized"/>; dispose that one first.
    /// </exception>
    public static ZlibDecompressor Create(int workBufferSize = 64 * 1024)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(workBufferSize, 1);

        void* work = NativeMemory.AlignedAlloc((nuint)workBufferSize, Zlib.DestinationAlignSize);
        void* destination = null;
        try
        {
            // The destination the service writes into, aligned and sized as the service requires.
            destination = NativeMemory.AlignedAlloc(Zlib.MaxDestinationSize, Zlib.DestinationAlignSize);

            // The service is one process-wide instance that counts no holders: a second initialization
            // registers nothing and reports that it is already up, and a shutdown from either holder
            // closes the device under the other. Refusing the second holder is what keeps the pairing of
            // one initialization to one shutdown true, so this failure is passed on rather than absorbed.
            SceResult.ThrowIfFailed(
                Zlib.sceZlibInitialize(work, (nuint)workBufferSize), nameof(Zlib.sceZlibInitialize));

            return new ZlibDecompressor(work, destination);
        }
        catch
        {
            // Free whatever was allocated before the failure so nothing leaks on the error path.
            NativeMemory.AlignedFree(work);
            if (destination != null)
                NativeMemory.AlignedFree(destination);
            throw;
        }
    }

    /// <summary>
    /// Decompresses <paramref name="source"/>, returning the plain bytes (up to 64 KiB).
    /// </summary>
    /// <exception cref="ProsperoException">The decompression failed.</exception>
    public byte[] Decompress(ReadOnlySpan<byte> source)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (source.IsEmpty)
            return [];

        ulong requestId;
        // The inflate is asynchronous: it submits the request and returns, and the service reads the
        // source while the wait runs. The source must therefore stay pinned across both the submit and
        // the wait, not only the submit - otherwise the collector could move it while the service reads.
        fixed (byte* src = source)
        {
            SceResult.ThrowIfFailed(
                Zlib.sceZlibInflate(src, (uint)source.Length, _destination, Zlib.MaxDestinationSize, &requestId),
                nameof(Zlib.sceZlibInflate));
            SceResult.ThrowIfFailed(Zlib.sceZlibWaitForDone(&requestId, null), nameof(Zlib.sceZlibWaitForDone));
        }

        uint produced = 0;
        int status = 0;
        SceResult.ThrowIfFailed(Zlib.sceZlibGetResult(requestId, &produced, &status), nameof(Zlib.sceZlibGetResult));
        SceResult.ThrowIfFailed(status, "zlib inflate");

        var output = new byte[produced];
        new ReadOnlySpan<byte>(_destination, (int)produced).CopyTo(output);
        return output;
    }

    /// <summary>Shuts the service down and releases its buffers.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        Zlib.sceZlibFinalize();
        if (_workBuffer != null) { NativeMemory.AlignedFree(_workBuffer); _workBuffer = null; }
        if (_destination != null) { NativeMemory.AlignedFree(_destination); _destination = null; }
    }
}
