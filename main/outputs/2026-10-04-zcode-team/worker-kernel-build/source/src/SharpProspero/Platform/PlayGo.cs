// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.PlayGo;
using SharpProspero.Interop.Sysmodule;
using SharpProspero.Modules;
using System;
using System.Runtime.InteropServices;
using Native = SharpProspero.Interop.PlayGo.PlayGo;

namespace SharpProspero.Platform;

/// <summary>Download progress: how much has arrived, out of the total.</summary>
/// <param name="Downloaded">The bytes downloaded so far.</param>
/// <param name="Total">The total bytes.</param>
public readonly record struct DownloadProgress(ulong Downloaded, ulong Total)
{
    /// <summary>The fraction downloaded, 0 to 1.</summary>
    public double Fraction => Total == 0 ? 1.0 : (double)Downloaded / Total;
}

/// <summary>
/// Reads the install and download progress of the running application's content chunks. A launcher or
/// a title that streams its data shows this while content arrives.
/// </summary>
/// <example>
/// <code>
/// using var playGo = PlayGo.Open();
/// DownloadProgress p = playGo.GetProgress(new ushort[] { 0, 1, 2 });
/// Show(p.Fraction);
/// </code>
/// </example>
public sealed unsafe class PlayGo : IDisposable
{
    private readonly int _handle;
    private void* _workBuffer;

    // The loadable module the service lives in, owned from the moment it is loaded so that every way
    // out of the open sequence gives it back rather than leaving it mapped for the life of the process.
    private readonly SystemModule _module;
    private bool _disposed;

    private PlayGo(SystemModule module, int handle, void* workBuffer)
    {
        _module = module;
        _handle = handle;
        _workBuffer = workBuffer;
    }

    /// <summary>Starts PlayGo and opens a handle for the running application.</summary>
    /// <exception cref="ProsperoException">PlayGo could not be started.</exception>
    public static PlayGo Open()
    {
        SystemModule module = SystemModule.Load(SystemModuleId.PlayGo);
        void* buffer = NativeMemory.AllocZeroed(Native.HeapSize);
        try
        {
            var init = new ScePlayGoInitParams { BufAddr = buffer, BufSize = Native.HeapSize };
            SceResult.ThrowIfFailed(Native.scePlayGoInitialize(&init), nameof(Native.scePlayGoInitialize));

            int handle;
            int opened = Native.scePlayGoOpen(&handle, null);
            if (opened < 0)
            {
                Native.scePlayGoTerminate();
                SceResult.ThrowIfFailed(opened, nameof(Native.scePlayGoOpen));
            }
            return new PlayGo(module, handle, buffer);
        }
        catch
        {
            NativeMemory.Free(buffer);
            module.Dispose();
            throw;
        }
    }

    /// <summary>Reads the download progress of the given content chunks.</summary>
    /// <exception cref="ProsperoException">The progress could not be read.</exception>
    public DownloadProgress GetProgress(ReadOnlySpan<ushort> chunkIds)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ScePlayGoProgress progress;
        fixed (ushort* ids = chunkIds)
            SceResult.ThrowIfFailed(
                Native.scePlayGoGetProgress(_handle, ids, (uint)chunkIds.Length, &progress),
                nameof(Native.scePlayGoGetProgress));
        return new DownloadProgress(progress.ProgressSize, progress.TotalSize);
    }

    /// <summary>Reads where each of the given content chunks is.</summary>
    /// <exception cref="ProsperoException">The state could not be read.</exception>
    public PlayGoLocus[] GetLocus(ReadOnlySpan<ushort> chunkIds)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var loci = new PlayGoLocus[chunkIds.Length];
        fixed (ushort* ids = chunkIds)
        fixed (PlayGoLocus* outLoci = loci)
            SceResult.ThrowIfFailed(
                Native.scePlayGoGetLocus(_handle, ids, (uint)chunkIds.Length, (sbyte*)outLoci),
                nameof(Native.scePlayGoGetLocus));
        return loci;
    }

    /// <summary>Closes the handle, stops PlayGo, frees its buffer, and unloads its module.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        Native.scePlayGoClose(_handle);
        Native.scePlayGoTerminate();
        if (_workBuffer != null) { NativeMemory.Free(_workBuffer); _workBuffer = null; }
        _module.Dispose();
    }
}
