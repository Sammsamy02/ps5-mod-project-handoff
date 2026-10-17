// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.SystemService;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// The GPU load emulation profile. It decides how much GPU work the system lets through, which is
/// what the machine's power and heat budget follows.
/// </summary>
public enum GpuLoadEmulationMode
{
    /// <summary>Emulation off, so the GPU runs at its own pace.</summary>
    Off = 0,

    /// <summary>The standard profile.</summary>
    Normal = 1,
}

/// <summary>
/// The power surface an application module can reach: hold off the idle timer while a long operation
/// runs, let the machine idle again, and read or choose the GPU load profile that the power and heat
/// budget follows.
/// </summary>
/// <remarks>
/// <para>
/// An application cannot turn the machine off, restart it, or put it into standby. The system carries
/// routines that do all three, but none of them is offered to an application at link time, so a module
/// that reached for one would carry an import nothing binds and would never reach its first
/// instruction. The kernel side is closed the same way: the device nodes that drive the power
/// controller are created owned by the privileged account with owner-only access, and an application
/// process is neither that account nor allowed the node, so opening one is refused. There is no
/// argument, entitlement, or setting a module can pass to change either answer. A module that wants
/// the machine to shut down has to ask the person holding the controller to do it.
/// </para>
/// <para>
/// Suspend and resume are one-sided. An application is told after the fact, through
/// <see cref="SystemEventType.Resume"/> arriving from <see cref="SystemControl.TryReceiveEvent"/>,
/// that it was suspended and has started running again; it is given no warning beforehand. Treat the
/// resume event as the point to re-read the wall clock, re-check which pads are connected, and drop
/// anything timed against the frame counter, because arbitrary real time passed while the process was
/// frozen and nothing in it advanced.
/// </para>
/// </remarks>
/// <example>
/// <code>
/// while (installer.Busy)
/// {
///     installer.Pump();
///     PowerControl.KeepAwake();      // the user is not touching the pad; do not let it idle out
/// }
/// </code>
/// </example>
public static unsafe class PowerControl
{
    /// <summary>
    /// Pushes the idle timer back so the machine does not power itself down. Call it repeatedly, at
    /// least once every few seconds, for as long as a long operation runs with nobody touching the
    /// pad - a download, an install, a long copy.
    /// </summary>
    /// <remarks>
    /// There is no matching release. The timer starts counting from the last call, so a module lets
    /// the machine idle again simply by stopping: once the calls stop, the system's own idle countdown
    /// runs to its end and takes over. Nothing has to be undone at teardown.
    /// </remarks>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public static void KeepAwake() =>
        SceResult.ThrowIfFailed(
            SystemService.sceSystemServicePowerTick(),
            nameof(SystemService.sceSystemServicePowerTick));

    /// <summary>
    /// Pushes the idle timer back and reports whether that worked, for a caller that would rather
    /// carry on than be interrupted by a failure inside its own loop.
    /// </summary>
    /// <returns>True when the timer was pushed back.</returns>
    public static bool TryKeepAwake() =>
        SceResult.Succeeded(SystemService.sceSystemServicePowerTick());

    /// <summary>
    /// The GPU load emulation profile in effect. Reading it reports
    /// <see cref="GpuLoadEmulationMode.Normal"/> when the state cannot be read, so it never fails;
    /// writing it throws when the system refuses the change.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">The value written is not a defined mode.</exception>
    /// <exception cref="ProsperoException">The system refused the change.</exception>
    public static GpuLoadEmulationMode GpuLoadEmulation
    {
        get => (GpuLoadEmulationMode)SystemService.sceSystemServiceGetGpuLoadEmulationMode();
        set
        {
            if (!IsDefined(value))
                throw new ArgumentOutOfRangeException(nameof(value), value, "Not a defined GPU load emulation mode.");
            SceResult.ThrowIfFailed(
                SystemService.sceSystemServiceSetGpuLoadEmulationMode((int)value),
                nameof(SystemService.sceSystemServiceSetGpuLoadEmulationMode));
        }
    }

    /// <summary>
    /// Chooses the GPU load emulation profile and reports whether the system accepted it. The system
    /// refuses the change on a machine where the profile is fixed, which a caller offering this as an
    /// option should expect rather than treat as a fault.
    /// </summary>
    /// <param name="mode">The profile to select.</param>
    /// <returns>True when the profile was changed.</returns>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="mode"/> is not a defined mode.</exception>
    public static bool TrySetGpuLoadEmulation(GpuLoadEmulationMode mode)
    {
        if (!IsDefined(mode))
            throw new ArgumentOutOfRangeException(nameof(mode), mode, "Not a defined GPU load emulation mode.");
        return SceResult.Succeeded(SystemService.sceSystemServiceSetGpuLoadEmulationMode((int)mode));
    }

    /// <summary>True when <paramref name="mode"/> is one of the profiles the system defines.</summary>
    public static bool IsDefined(GpuLoadEmulationMode mode) =>
        mode is GpuLoadEmulationMode.Off or GpuLoadEmulationMode.Normal;
}
