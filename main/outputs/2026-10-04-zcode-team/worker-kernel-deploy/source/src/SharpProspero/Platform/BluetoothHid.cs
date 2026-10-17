// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Bluetooth;

namespace SharpProspero.Platform;

/// <summary>
/// Entry point to the Bluetooth HID driver surface. <see cref="Initialize"/> opens the device and must
/// run before any Bluetooth HID call. The device, report, and callback calls take structures whose
/// layout is device-specific and are exposed directly on
/// <see cref="SharpProspero.Interop.Bluetooth.SceBluetoothHid"/> for advanced use; this type wraps only
/// the two calls that need no such structure.
/// </summary>
public static class BluetoothHid
{
    /// <summary>
    /// Opens the Bluetooth device node and prepares the module. Safe to call more than once; the second
    /// and later calls are no-ops. Access to the device is privileged.
    /// </summary>
    /// <exception cref="ProsperoException">Initialization failed.</exception>
    public static void Initialize() =>
        SceResult.ThrowIfFailed(
            SceBluetoothHid.sceBluetoothHidInit(),
            nameof(SceBluetoothHid.sceBluetoothHidInit));

    /// <summary>The Bluetooth HID module's build version.</summary>
    public static int Version => SceBluetoothHid.sceBluetoothHidDebugGetVersion();
}
