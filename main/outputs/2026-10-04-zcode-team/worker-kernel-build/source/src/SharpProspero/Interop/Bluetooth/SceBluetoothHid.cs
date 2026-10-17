// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Bluetooth;

/// <summary>
/// Bluetooth HID bindings (libSceBluetoothHid). This is the low-level driver surface for Bluetooth
/// human-interface devices: it opens the <c>/dev/bt</c> and <c>/dev/bluetooth_hid</c> device nodes and
/// forwards each call to the kernel as an ioctl. Every function returns a status code (0 on success,
/// a negative 0x8154xxxx value on failure); <see cref="SceBluetoothHid.sceBluetoothHidInit"/> must run first.
/// </summary>
/// <remarks>
/// The module has no public header, so these signatures were recovered from the module itself: the
/// argument shapes (counts, pointer-versus-value, scalar widths) match the binary. The layout of the
/// structures behind the pointer arguments (device info, report buffers, the callback and its option
/// block) is not documented here, so this class stays at the entry-point level and does not offer a
/// higher-level wrapper. Access to the device nodes is privileged.
/// </remarks>
/// <summary>
/// The parameter block the module fills. Twelve bytes: the value the caller asked for, then a word the
/// module clears. It is declared whole so a caller reserves what the module writes.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 16)]
public struct SceBluetoothHidParam
{
    /// <summary>The value the caller asked for. Offset 0.</summary>
    public ulong Value;

    /// <summary>Cleared by the module. Offset 8.</summary>
    public uint Reserved;
}

public static unsafe partial class SceBluetoothHid
{
    private const string Lib = "libSceBluetoothHid";

    /// <summary>Opens the Bluetooth device node and prepares the module. Call once before anything else.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidInit();

    /// <summary>
    /// Fills the caller's parameter block with <paramref name="value"/>. The block is twelve bytes -
    /// the value and a word the device clears after it - so it is named as a type of its own rather
    /// than a single word, which the device would write four bytes past.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidParamInitialize(SceBluetoothHidParam* param, ulong value);

    /// <summary>Fills the caller's thread-parameter block <paramref name="param"/> from the given values.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidThreadParamInitialize(uint* param, uint value1, ulong value2);

    /// <summary>
    /// Registers <paramref name="callback"/> (a function pointer) with user argument <paramref name="arg"/>
    /// and an optional option block <paramref name="options"/>. Establishes the module's event thread.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidRegisterCallback(void* callback, ulong arg, void* options);

    /// <summary>Removes the callback registered by <see cref="sceBluetoothHidRegisterCallback"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidUnregisterCallback();

    /// <summary>Registers a device identified by the two 16-bit selectors (ioctl 0x8004622b).</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidRegisterDevice(ushort value1, ushort value2);

    /// <summary>Unregisters a device previously registered with <see cref="sceBluetoothHidRegisterDevice"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidUnregisterDevice(ushort value1, ushort value2);

    /// <summary>Reads the HID report descriptor for <paramref name="handle"/> into <paramref name="buffer"/> (up to <paramref name="length"/> bytes).</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidGetReportDescriptor(ulong handle, void* buffer, ulong length);

    /// <summary>Reads the device name for <paramref name="handle"/> into <paramref name="nameOut"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidGetDeviceName(ulong handle, void* nameOut);

    /// <summary>Reads information for device <paramref name="deviceId"/> into <paramref name="info"/> (ioctl 0x8010a40b).</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidGetDeviceInfo(uint deviceId, void* info);

    /// <summary>Reads an input report (<paramref name="reportId"/>) into the caller's <paramref name="report"/> block.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidGetInputReport(void* report, uint reportId);

    /// <summary>Reads a feature report (<paramref name="reportId"/>) into the caller's <paramref name="report"/> block.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidGetFeatureReport(void* report, uint reportId);

    /// <summary>Writes an output report: <paramref name="data"/> (<paramref name="length"/> bytes) with <paramref name="reportId"/> to <paramref name="handle"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidSetOutputReport(ulong handle, uint reportId, void* data, ulong length);

    /// <summary>Writes a feature report: <paramref name="data"/> (<paramref name="length"/> bytes) with <paramref name="reportId"/> to <paramref name="handle"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidSetFeatureReport(ulong handle, uint reportId, void* data, ulong length);

    /// <summary>Sends data on the interrupt-output channel: <paramref name="data"/> (<paramref name="length"/> bytes) with <paramref name="reportId"/> to <paramref name="handle"/> (ioctl 0x80206232).</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidInterruptOutput(ulong handle, uint reportId, void* data, ulong length);

    /// <summary>Disconnects the device identified by <paramref name="deviceId"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidDisconnectDevice(uint deviceId);

    /// <summary>Returns the module's build version.</summary>
    [LibraryImport(Lib)]
    public static partial int sceBluetoothHidDebugGetVersion();
}
