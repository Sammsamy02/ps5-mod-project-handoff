// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.FeatureFlag;

namespace SharpProspero.Platform;

/// <summary>
/// Reads the console's feature flags. A feature is identified by a number; <see cref="IsOn"/> reports
/// whether it is enabled, and <see cref="IsWaitingReboot"/> whether a change is pending a reboot.
/// </summary>
public static class FeatureFlag
{
    /// <summary>Reports whether the feature identified by <paramref name="featureId"/> is enabled.</summary>
    public static bool IsOn(uint featureId) =>
        SceCffMgr.sceConsoleFeatureFlagManagerIsOn(featureId) != 0;

    /// <summary>Reports whether the feature identified by <paramref name="featureId"/> is waiting for a reboot to take effect.</summary>
    public static bool IsWaitingReboot(uint featureId) =>
        SceCffMgr.sceConsoleFeatureFlagManagerIsWaitingReboot(featureId) != 0;
}
