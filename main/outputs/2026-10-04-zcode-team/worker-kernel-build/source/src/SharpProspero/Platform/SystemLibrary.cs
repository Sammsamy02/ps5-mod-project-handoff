// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Modules;
using System;
using System.Collections.Generic;

namespace SharpProspero.Platform;

/// <summary>
/// A system library loaded at run time and resolved by symbol name. This is the front door for every
/// service a title does not link against: the loader binds each name against whatever the running
/// system exports, so the same module reaches the service across system versions without an absolute
/// address. Resolve a name and it succeeds when the export is present; try a name and fall back when
/// it is not, so a version that renames or drops an export is handled instead of crashing.
/// </summary>
/// <remarks>
/// This wraps a run-time module load (<see cref="PrxModule"/>) with the resolution and diagnostics the
/// firmware-support layer builds on. Open it once, resolve the functions the service needs, and dispose
/// it to unload. It is not thread-safe; open one per user.
/// </remarks>
/// <example>
/// <code>
/// using var library = SystemLibrary.Open("/system/common/lib/libSceExample.sprx");
/// var doThing = (delegate* unmanaged&lt;int&gt;)library.GetFunction("sceExampleDoThing");
/// </code>
/// </example>
public sealed unsafe class SystemLibrary : IDisposable
{
    private readonly PrxModule _module;
    private bool _disposed;

    private SystemLibrary(string path, PrxModule module)
    {
        Path = path;
        _module = module;
    }

    /// <summary>The path the library was loaded from.</summary>
    public string Path { get; }

    /// <summary>The loader handle of the loaded module.</summary>
    public int Handle => _module.Handle;

    /// <summary>Loads the library at an absolute <paramref name="path"/> and starts it.</summary>
    /// <exception cref="ProsperoException">The library failed to load or start.</exception>
    public static SystemLibrary Open(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        return new SystemLibrary(path, PrxModule.Load(path));
    }

    /// <summary>Reports whether the library exports <paramref name="symbolName"/> on this system.</summary>
    public bool HasExport(string symbolName)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        return _module.GetExport(symbolName) != IntPtr.Zero;
    }

    /// <summary>
    /// Resolves an exported function, or reports that it is absent. Cast the result through a
    /// <c>delegate* unmanaged&lt;...&gt;</c> to call it.
    /// </summary>
    /// <returns>True when the symbol was found; false leaves <paramref name="function"/> null.</returns>
    public bool TryGetFunction(string symbolName, out void* function)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        return _module.TryGetFunctionPointer(symbolName, out function);
    }

    /// <summary>
    /// Resolves an exported function, or throws a clear diagnostic naming the library and the system
    /// version when it is absent. Cast the result through a <c>delegate* unmanaged&lt;...&gt;</c> to
    /// call it.
    /// </summary>
    /// <exception cref="ProsperoException">The library does not export the symbol on this system.</exception>
    public void* GetFunction(string symbolName)
    {
        if (TryGetFunction(symbolName, out void* function))
            return function;
        throw new ProsperoException($"'{symbolName}' is not exported by '{Path}'{FirmwareSuffix()}.", -1);
    }

    /// <summary>
    /// Reports which of <paramref name="requiredSymbols"/> the library does not export on this system,
    /// in the order given. An empty result means every one resolved, so the feature that needs them can
    /// run. A non-empty result names exactly what is missing, so the caller can refuse the feature with
    /// a specific reason rather than failing on the first call into it.
    /// </summary>
    public IReadOnlyList<string> FindMissingExports(IEnumerable<string> requiredSymbols)
    {
        ArgumentNullException.ThrowIfNull(requiredSymbols);
        ObjectDisposedException.ThrowIf(_disposed, this);

        var missing = new List<string>();
        foreach (string symbol in requiredSymbols)
        {
            if (!string.IsNullOrEmpty(symbol) && _module.GetExport(symbol) == IntPtr.Zero)
                missing.Add(symbol);
        }
        return missing;
    }

    /// <summary>
    /// Throws unless the library exports every one of <paramref name="requiredSymbols"/> on this
    /// system. The message names all the missing symbols and the system version, so a version that
    /// dropped a needed export is reported as one clear failure before the feature is used.
    /// </summary>
    /// <exception cref="ProsperoException">One or more required symbols are absent.</exception>
    public void RequireExports(IEnumerable<string> requiredSymbols)
    {
        IReadOnlyList<string> missing = FindMissingExports(requiredSymbols);
        if (missing.Count == 0)
            return;
        throw new ProsperoException(
            $"'{Path}' does not export {string.Join(", ", missing)}{FirmwareSuffix()}.", -1);
    }

    /// <summary>Stops and unloads the library.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        _module.Dispose();
    }

    // The running firmware, when it can be read, added to a diagnostic so an export gap on a specific
    // version reads plainly. Reading it must not itself throw and mask the real error.
    private static string FirmwareSuffix()
        => FirmwareVersion.TryGetCurrent(out FirmwareVersion version) ? $" on firmware {version}" : "";
}
