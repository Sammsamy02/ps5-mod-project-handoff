// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Kernel;

/// <summary>
/// Dynamic module bindings. A running module brings in another module from its own package, resolves
/// the exported entry points it needs, and unloads it at shutdown. Strings are UTF-8 with a trailing
/// null; the higher-level wrapper encodes them.
/// </summary>
public static unsafe partial class KernelModule
{
    private const string Lib = "libkernel";

    /// <summary>
    /// Maps and starts the module at <paramref name="moduleFileName"/>, running its initializer.
    /// </summary>
    /// <returns>A non-negative module handle on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelLoadStartModule(
        byte* moduleFileName, nuint args, void* argp, uint flags, void* pOpt, int* pRes);

    /// <summary>Resolves an exported symbol in a loaded module to its address.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDlsym(int handle, byte* symbol, void** addressOut);

    /// <summary>Runs a module's finalizer and unmaps it.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelStopUnloadModule(
        int handle, nuint args, void* argp, uint flags, void* pOpt, int* pRes);
}
