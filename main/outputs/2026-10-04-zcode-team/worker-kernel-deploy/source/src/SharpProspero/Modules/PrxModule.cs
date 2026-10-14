// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Modules;

/// <summary>
/// A module loaded at run time from the application's own package. Drop a <c>.prx</c> in the
/// package's <c>sce_module</c> folder, load it by name, resolve the exported functions to call, and
/// dispose it to unload. The application depends only on the supplied module; no other library is
/// required to reach it.
/// </summary>
public sealed unsafe class PrxModule : IDisposable
{
    /// <summary>The path prefix of the mounted application root's module folder.</summary>
    public const string AppModuleFolder = "/app0/sce_module/";

    private int _handle;
    private bool _disposed;

    private PrxModule(int handle) => _handle = handle;

    /// <summary>The loader handle.</summary>
    public int Handle => _handle;

    /// <summary>Loads and starts the module at an absolute <paramref name="path"/>.</summary>
    /// <exception cref="ProsperoException">The module failed to load or start.</exception>
    public static PrxModule Load(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        int handle = WithUtf8(path, p => KernelModule.sceKernelLoadStartModule(p, 0, null, 0, null, null));
        SceResult.ThrowIfFailed(handle, nameof(KernelModule.sceKernelLoadStartModule));
        return new PrxModule(handle);
    }

    /// <summary>
    /// Loads a module by file name from the package's module folder, for example
    /// <c>LoadFromPackage("mylib.prx")</c> for <c>/app0/sce_module/mylib.prx</c>.
    /// </summary>
    public static PrxModule LoadFromPackage(string fileName)
    {
        ArgumentException.ThrowIfNullOrEmpty(fileName);
        return Load(AppModuleFolder + fileName);
    }

    /// <summary>Resolves an exported symbol to its address, or <see cref="IntPtr.Zero"/> when absent.</summary>
    public IntPtr GetExport(string symbolName)
    {
        ArgumentException.ThrowIfNullOrEmpty(symbolName);
        ObjectDisposedException.ThrowIf(_disposed, this);

        int byteCount = Encoding.UTF8.GetByteCount(symbolName);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        int written = Encoding.UTF8.GetBytes(symbolName, buffer);
        buffer[written] = 0;

        void* address = null;
        int rc;
        fixed (byte* s = buffer)
            rc = KernelModule.sceKernelDlsym(_handle, s, &address);
        return SceResult.Failed(rc) ? IntPtr.Zero : (IntPtr)address;
    }

    /// <summary>
    /// Resolves an exported function to an unmanaged function pointer. Call it through a
    /// <c>delegate* unmanaged&lt;...&gt;</c> cast of the returned pointer.
    /// </summary>
    /// <exception cref="ProsperoException">The symbol was not found.</exception>
    public void* GetFunctionPointer(string symbolName)
    {
        IntPtr address = GetExport(symbolName);
        if (address == IntPtr.Zero)
            throw new ProsperoException($"Export '{symbolName}'", -1);
        return (void*)address;
    }

    /// <summary>
    /// Resolves an exported function to an unmanaged function pointer, or reports that it is absent.
    /// Use this when a symbol may not be present on every system version: try it, and fall back when
    /// it is missing rather than failing.
    /// </summary>
    /// <returns>True when the symbol was found; false leaves <paramref name="function"/> null.</returns>
    public bool TryGetFunctionPointer(string symbolName, out void* function)
    {
        IntPtr address = GetExport(symbolName);
        function = (void*)address;
        return address != IntPtr.Zero;
    }

    /// <summary>Stops and unloads the module.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_handle >= 0)
        {
            KernelModule.sceKernelStopUnloadModule(_handle, 0, null, 0, null, null);
            _handle = -1;
        }
    }

    // Encodes a UTF-8, null-terminated copy of the string on the stack for the duration of the call.
    private static int WithUtf8(string value, Utf8Call call)
    {
        int byteCount = Encoding.UTF8.GetByteCount(value);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        int written = Encoding.UTF8.GetBytes(value, buffer);
        buffer[written] = 0;
        fixed (byte* p = buffer)
            return call(p);
    }

    private delegate int Utf8Call(byte* value);
}
