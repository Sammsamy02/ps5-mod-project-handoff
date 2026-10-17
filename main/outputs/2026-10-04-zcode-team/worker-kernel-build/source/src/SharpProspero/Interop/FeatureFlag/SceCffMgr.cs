// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.FeatureFlag;

/// <summary>
/// Console feature-flag bindings (libSceCffMgr). The module reads the console's feature flags: whether a
/// numbered feature is enabled, and whether it is waiting for a reboot to take effect. Each call takes a
/// feature identifier and returns the flag state. The module has no public header; these two named
/// exports were recovered from the module (the rest carry no recoverable name and are not bound).
/// </summary>
public static partial class SceCffMgr
{
    private const string Lib = "libSceCffMgr";

    /// <summary>Returns non-zero when the feature <paramref name="featureId"/> is enabled (0 when off or on error).</summary>
    [LibraryImport(Lib)]
    public static partial int sceConsoleFeatureFlagManagerIsOn(uint featureId);

    /// <summary>Returns non-zero when the feature <paramref name="featureId"/> is waiting for a reboot to take effect.</summary>
    [LibraryImport(Lib)]
    public static partial int sceConsoleFeatureFlagManagerIsWaitingReboot(uint featureId);
}
