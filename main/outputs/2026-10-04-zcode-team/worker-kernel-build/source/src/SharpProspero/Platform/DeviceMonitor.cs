// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Device;
using System;

namespace SharpProspero.Platform;

/// <summary>
/// Watches for changes to the set of connected devices through the message bus. Start it, then poll
/// <see cref="Generation"/> (which advances whenever the device set changes) or read the pending
/// <see cref="PeekEvents"/> / <see cref="ConsumeEvents"/> bitmask. Dispose it to release the service.
/// </summary>
/// <remarks>
/// This wraps the device-service calls recovered from the message-bus module. The generation counter and
/// the event-state bitmask are the reliable "something changed" signals; the meaning of individual event
/// bits and the per-device record layout are device-specific and not fully documented, so this exposes the
/// change signals rather than a decoded device list.
/// </remarks>
public sealed class DeviceMonitor : IDisposable
{
    private bool _disposed;

    private DeviceMonitor() { }

    /// <summary>Starts the device service and returns a monitor for it.</summary>
    /// <exception cref="InvalidOperationException">The device service could not be started.</exception>
    public static DeviceMonitor Start()
    {
        int result;
        unsafe { result = DeviceService.sceDeviceServiceInitialize(0, null); }
        if (result < 0)
            throw new InvalidOperationException($"Could not start the device service (0x{result:X8}).");
        return new DeviceMonitor();
    }

    /// <summary>
    /// The current device-set generation. It advances whenever the connected devices change, so comparing
    /// it against the last value you saw tells you a change happened. Negative on error.
    /// </summary>
    public int Generation
    {
        get
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            return DeviceService.sceDeviceServiceGetGeneration();
        }
    }

    /// <summary>Reads the pending device-event bitmask without clearing it.</summary>
    public int PeekEvents()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        return DeviceService.sceDeviceServiceGetEventState(0);
    }

    /// <summary>Reads and clears the pending device-event bitmask.</summary>
    public int ConsumeEvents()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        return DeviceService.sceDeviceServiceGetEventState(1);
    }

    /// <summary>Stops the device service.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        DeviceService.sceDeviceServiceTerminate();
        _disposed = true;
        GC.SuppressFinalize(this);
    }

    /// <summary>Stops the device service if it was not disposed.</summary>
    ~DeviceMonitor() => Dispose();
}
