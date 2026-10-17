// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.SystemService;
using System;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>A system event delivered to the application. Unknown values keep their raw number.</summary>
public enum SystemEventType
{
    /// <summary>No event. The value the field holds before a real event is written into it.</summary>
    Invalid = -1,

    /// <summary>
    /// The application was suspended and is running again. Arbitrary real time passed while it was
    /// frozen and nothing inside it advanced, so re-read the wall clock, re-check which pads are
    /// connected, and drop anything that was timed against the frame counter. This is the only
    /// notification an application gets about being suspended, and it arrives afterwards: there is no
    /// warning before the machine freezes the process.
    /// </summary>
    Resume = 0x10000000,

    /// <summary>An entitlement the signed-in user holds has changed.</summary>
    EntitlementUpdate = 0x10000003,

    /// <summary>Another application was launched over this one.</summary>
    AppLaunched = 0x10000007,

    /// <summary>Additional content finished installing.</summary>
    AddContentInstalled = 0x10000009,

    /// <summary>The install progress of the running application's own content moved.</summary>
    PlayGoLocusUpdate = 0x1000000C,

    /// <summary>A service entitlement the signed-in user holds has changed.</summary>
    ServiceEntitlementUpdate = 0x1000000E,

    /// <summary>The system is asking the application to act on an intent it was started with.</summary>
    GameIntent = 0x10000017,

    /// <summary>A unified entitlement the signed-in user holds has changed.</summary>
    UnifiedEntitlementUpdate = 0x10000018,

    /// <summary>A chunk of the running application's own content became available.</summary>
    PlayGoChunkAdded = 0x10000019,
}

/// <summary>A snapshot of the system service's state.</summary>
/// <param name="PendingEvents">The number of events waiting to be received.</param>
/// <param name="IsSystemUiOverlaid">Whether a system dialog is drawn over the application.</param>
/// <param name="IsInBackground">Whether the application is running in the background.</param>
public readonly record struct SystemStatus(int PendingEvents, bool IsSystemUiOverlaid, bool IsInBackground);

/// <summary>
/// Control and status for the running module: keep the console awake through a long operation, read
/// system events such as resuming from sleep, learn whether the application is in the background, and
/// take the audio output for itself. These are the pieces of app-loop plumbing a real application and a
/// system tool both need.
/// </summary>
public static unsafe class SystemControl
{
    /// <summary>
    /// Resets the idle-shutdown timer so the console stays awake. Call it periodically during a long
    /// operation such as a download or an install, when the user is not touching the controller.
    /// </summary>
    /// <seealso cref="PowerControl"/>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public static void KeepAwake() => PowerControl.KeepAwake();

    /// <summary>
    /// Reads the next pending system event, returning false when none is waiting. Poll it each frame to
    /// react to events such as <see cref="SystemEventType.Resume"/>.
    /// </summary>
    /// <remarks>
    /// One queue feeds the whole module, and each read takes an event out of it, so keep this call in
    /// one place and hand the result on. A caller watching only for a resume still has to look at every
    /// event it takes, because dropping the others silently loses them.
    /// </remarks>
    public static bool TryReceiveEvent(out SystemEventType type)
    {
        SceSystemServiceEvent native = default;
        if (SystemService.sceSystemServiceReceiveEvent(&native) < 0)
        {
            type = default;
            return false;
        }
        type = (SystemEventType)native.EventType;
        return true;
    }

    /// <summary>Reads the current service status.</summary>
    /// <exception cref="ProsperoException">The status could not be read.</exception>
    public static SystemStatus GetStatus()
    {
        SceSystemServiceStatus native = default;
        SceResult.ThrowIfFailed(SystemService.sceSystemServiceGetStatus(&native), nameof(SystemService.sceSystemServiceGetStatus));
        return new SystemStatus(native.EventNum, native.IsSystemUiOverlaid != 0, native.IsInBackgroundExecution != 0);
    }

    /// <summary>Whether the application is currently running in the background.</summary>
    public static bool IsInBackground => GetStatus().IsInBackground;

    /// <summary>
    /// The fraction of the screen, 0 to 1, that is safe to draw important content in, so a user's
    /// display-margin setting is respected. Multiply screen dimensions by it and centre the content.
    /// </summary>
    /// <exception cref="ProsperoException">The value could not be read.</exception>
    public static float DisplaySafeAreaRatio
    {
        get
        {
            SceSystemServiceDisplaySafeAreaInfo info = default;
            SceResult.ThrowIfFailed(
                SystemService.sceSystemServiceGetDisplaySafeAreaInfo(&info),
                nameof(SystemService.sceSystemServiceGetDisplaySafeAreaInfo));
            return info.Ratio;
        }
    }

    /// <summary>Stops the background media player so this module has the audio output to itself.</summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public static void SilenceBackgroundMedia() =>
        SceResult.ThrowIfFailed(SystemService.sceSystemServiceDisableMediaPlay(), nameof(SystemService.sceSystemServiceDisableMediaPlay));

    /// <summary>Allows the background media player to run again.</summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public static void RestoreBackgroundMedia() =>
        SceResult.ThrowIfFailed(SystemService.sceSystemServiceReenableMediaPlay(), nameof(SystemService.sceSystemServiceReenableMediaPlay));

    /// <summary>
    /// Replaces the running module with the executable at <paramref name="path"/>, for chain-loading
    /// another module. On success this call does not return, since the current module is gone.
    /// </summary>
    /// <exception cref="ProsperoException">The executable could not be loaded.</exception>
    public static void LoadExecutable(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        int byteCount = Encoding.UTF8.GetByteCount(path);
        Span<byte> buffer = byteCount < 512 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(path, buffer);
        buffer[byteCount] = 0;
        fixed (byte* p = buffer)
            SceResult.ThrowIfFailed(SystemService.sceSystemServiceLoadExec(p, null), nameof(SystemService.sceSystemServiceLoadExec));
    }
}
