// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Content;
using SharpProspero.Interop.Sysmodule;
using SharpProspero.Modules;
using System;
using System.Text;
using Native = SharpProspero.Interop.Content.ContentDelete;

namespace SharpProspero.Platform;

/// <summary>
/// Deletes content the user owns — a screenshot, a video capture, or a piece of downloaded content.
/// Open it, delete by path, dispose it. Deleting content the process is not allowed to touch raises a
/// <see cref="ProsperoException"/>; the permission depends on the console, so a call can fail even
/// when the path is right.
/// </summary>
/// <example>
/// <code>
/// using var content = ContentDeleter.Open();
/// content.DeleteByPath("/user/data/capture0.jpg");
/// </code>
/// </example>
public sealed unsafe class ContentDeleter : IDisposable
{
    // The loadable module the service lives in, owned from the moment it is loaded so that every way
    // out of the open sequence gives it back rather than leaving it mapped for the life of the process.
    private readonly SystemModule _module;
    private bool _disposed;
    private bool _initialized;

    private ContentDeleter(SystemModule module) => _module = module;

    /// <summary>
    /// The working heap the service is started with. It is not a size to choose: the service compares
    /// what it is given against this exact figure and refuses anything else, then allocates this much
    /// regardless. Asking for more was refused every time, so the service could never be started.
    /// </summary>
    public const int HeapSize = 16 * 1024;

    /// <summary>Starts the content-delete service.</summary>
    /// <exception cref="ProsperoException">The service could not be started.</exception>
    public static ContentDeleter Open()
    {
        var deleter = new ContentDeleter(SystemModule.Load(SystemModuleId.ContentDelete));
        try
        {
            var param = new SceContentDeleteInitParam { HeapSize = HeapSize };
            SceResult.ThrowIfFailed(Native.sceContentDeleteInitialize(&param), nameof(Native.sceContentDeleteInitialize));
            deleter._initialized = true;
            return deleter;
        }
        catch
        {
            deleter.Dispose();
            throw;
        }
    }

    /// <summary>Deletes the content at <paramref name="path"/>.</summary>
    /// <exception cref="ProsperoException">The content could not be deleted.</exception>
    public void DeleteByPath(string path)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentException.ThrowIfNullOrEmpty(path);

        int count = Encoding.UTF8.GetByteCount(path);
        byte* buffer = stackalloc byte[count + 1];
        Encoding.UTF8.GetBytes(path, new Span<byte>(buffer, count));
        buffer[count] = 0;
        SceResult.ThrowIfFailed(Native.sceContentDeleteByPath(buffer), nameof(Native.sceContentDeleteByPath));
    }

    /// <summary>Deletes the content with <paramref name="contentId"/>.</summary>
    /// <exception cref="ProsperoException">The content could not be deleted.</exception>
    public void DeleteById(long contentId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceResult.ThrowIfFailed(Native.sceContentDeleteById(contentId), nameof(Native.sceContentDeleteById));
    }

    /// <summary>Stops the service and unloads the module.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_initialized)
            Native.sceContentDeleteTerminate();
        _module.Dispose();
    }
}
