// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.SystemService;

/// <summary>
/// System state manager bindings. Controls the console's power state (standby, reboot).
/// </summary>
public static unsafe partial class SystemStateMgr
{
    // Published by the libSceSystemService module under a library name of its own.
    private const string Lib = "libSceSystemStateMgr";

    /// <summary>
    /// Puts the console into standby (rest) mode.
    /// </summary>
    /// <returns>Zero on success, or a negative error code. On success the call does not
    /// return because the system enters standby.</returns>
    [LibraryImport(Lib)]
    public static partial int sceSystemStateMgrEnterStandby();
}
