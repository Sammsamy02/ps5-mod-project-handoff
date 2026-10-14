// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Device;

/// <summary>
/// The device-type ids the device-service query accepts. The module carries no names or symbols for these
/// values, so they are exposed by their raw id; the notes are inferred from the query code and are not an
/// official contract.
/// </summary>
public enum DeviceType : uint
{
    /// <summary>0x1001 - a parent/hub-class device that lists child ports.</summary>
    Type1001 = 0x1001,
    /// <summary>0x2001 - same profile as <see cref="Type1001"/>.</summary>
    Type2001 = 0x2001,
    /// <summary>0x3001.</summary>
    Type3001 = 0x3001,
    /// <summary>0x4001.</summary>
    Type4001 = 0x4001,
    /// <summary>0x4002.</summary>
    Type4002 = 0x4002,
    /// <summary>0x5001.</summary>
    Type5001 = 0x5001,
    /// <summary>0x6001 - a minimal record.</summary>
    Type6001 = 0x6001,
    /// <summary>0x7001 - a minimal record.</summary>
    Type7001 = 0x7001,
    /// <summary>0x8001 - the only type that takes an explicit condition filter.</summary>
    Type8001 = 0x8001,
    /// <summary>0x9001.</summary>
    Type9001 = 0x9001,
}

/// <summary>
/// The device-service surface of the message bus (<c>libSceMbus</c>): initialize the service, read a
/// generation counter and an event-state bitmask to notice when connected devices change, and query the
/// device records. The signatures are recovered by decompiling the module (it ships no header), so the
/// primitive shapes are faithful; the per-device-type record layout is device-specific and not fully
/// documented, so records are returned as fixed-size raw blocks.
/// </summary>
public static unsafe partial class DeviceService
{
    private const string Lib = "libSceMbus";

    /// <summary>The size in bytes of one device-info record returned by <see cref="sceDeviceServiceQueryDeviceInfo"/>.</summary>
    public const int DeviceInfoRecordSize = 112;

    /// <summary>
    /// Initializes the device service. <paramref name="flags"/> is 0 or a mask within 0x33;
    /// <paramref name="pParam"/> is an optional 16-byte parameter block (may be null). Returns 0 or a
    /// negative error.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceDeviceServiceInitialize(uint flags, void* pParam);

    /// <summary>Tears the device service down.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDeviceServiceTerminate();

    /// <summary>Returns the generation counter of the device service; it advances when the device set changes.</summary>
    [LibraryImport(Lib)]
    public static partial int sceDeviceServiceGetGeneration();

    /// <summary>
    /// Reads (and, when <paramref name="clearAfterRead"/> is non-zero, clears) the pending device event-state
    /// bitmask. Returns the state, or a negative error.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceDeviceServiceGetEventState(int clearAfterRead);

    /// <summary>
    /// Writes up to <paramref name="maxCount"/> device-info records of a device type into
    /// <paramref name="outEntries"/> (each <see cref="DeviceInfoRecordSize"/> bytes). <paramref name="condition"/>
    /// is a three-<c>ushort</c> filter <c>{value, min, max}</c> used only for <see cref="DeviceType.Type8001"/>
    /// (null otherwise); <paramref name="entrySize"/> must equal <see cref="DeviceInfoRecordSize"/>. The two
    /// returned-count pointers may be null.
    /// </summary>
    [LibraryImport(Lib, EntryPoint = "sceDeviceServiceQueryDeviceInfo_")]
    public static partial int sceDeviceServiceQueryDeviceInfo(uint deviceType, ushort* condition, uint serviceHandle, void* outEntries, uint maxCount, uint* returnedCount1, uint* returnedCount2, int entrySize);
}
