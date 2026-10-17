// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Kernel;

/// <summary>
/// A request to show a system notification (the toast that slides in at the top of the screen). The
/// block is a fixed 3120 bytes; the message is written into it and the header fields are set to the
/// values a working request uses.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 3120)]
public unsafe struct SceNotificationRequest
{
    /// <summary>The request type. A text notification uses 0x64.</summary>
    public int Type;

    /// <summary>A request id. A text notification uses -1.</summary>
    public int RequestId;

    private int _unk8;

    /// <summary>A target descriptor. A text notification sets every byte to 0xFF.</summary>
    public fixed byte Target[16];

    /// <summary>A field a text notification sets to -1.</summary>
    public int Unk28;

    private fixed byte _gap[13];

    /// <summary>The message text (UTF-8, NUL-terminated), up to 1023 characters.</summary>
    public fixed byte Message[1024];

    private fixed byte _reserved[2051];
}

/// <summary>System notification bindings.</summary>
public static unsafe partial class KernelNotification
{
    private const string Lib = "libkernel";

    /// <summary>The notification device that shows the on-screen toast.</summary>
    public const uint ToastDevice = 0;

    /// <summary>The size of a notification request, in bytes.</summary>
    public const int RequestSize = 3120;

    /// <summary>
    /// Sends a notification request. For the on-screen toast, <paramref name="device"/> is
    /// <see cref="ToastDevice"/>, <paramref name="request"/> points at a <see cref="SceNotificationRequest"/>,
    /// and <paramref name="size"/> is <see cref="RequestSize"/>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelSendNotificationRequest(uint device, void* request, nuint size, int blocking);
}
