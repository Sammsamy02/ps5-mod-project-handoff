// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Sysmodule;
using System;

namespace SharpProspero.Modules;

/// <summary>
/// A loaded system module. Load the modules a title needs at startup and dispose them at shutdown;
/// disposing unloads. Load is idempotent from the service's side, so loading an already-loaded module
/// succeeds.
/// </summary>
public sealed class SystemModule : IDisposable
{
    private readonly SystemModuleId _id;
    private bool _unloaded;

    private SystemModule(SystemModuleId id) => _id = id;

    /// <summary>The module identifier.</summary>
    public SystemModuleId Id => _id;

    /// <summary>Loads the module with <paramref name="id"/>.</summary>
    /// <exception cref="ProsperoException">Loading failed.</exception>
    public static SystemModule Load(SystemModuleId id)
    {
        SceResult.ThrowIfFailed(Sysmodule.sceSysmoduleLoadModule((ushort)id), nameof(Sysmodule.sceSysmoduleLoadModule));
        return new SystemModule(id);
    }

    /// <summary>Reports whether the module with <paramref name="id"/> is currently loaded.</summary>
    public static bool IsLoaded(SystemModuleId id)
        => SceResult.Succeeded(Sysmodule.sceSysmoduleIsLoaded((ushort)id));

    /// <summary>Unloads the module.</summary>
    public void Dispose()
    {
        if (_unloaded)
            return;
        _unloaded = true;
        Sysmodule.sceSysmoduleUnloadModule((ushort)_id);
    }
}
