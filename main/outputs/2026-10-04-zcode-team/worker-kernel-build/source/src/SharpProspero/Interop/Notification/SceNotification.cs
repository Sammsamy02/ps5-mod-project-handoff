// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Notification;

/// <summary>
/// Notification bindings (libSceNotification). This module sends a notification through the notification
/// service — a simpler path than the kernel notification request — and drives the persistent banner shown
/// next to the PS button. Signatures were recovered from the module: the send calls take a type, a flag,
/// and the text; a bad or empty message returns 0x81980001.
/// </summary>
public static unsafe partial class SceNotification
{
    private const string Lib = "libSceNotification";

    /// <summary>Sends a notification of <paramref name="type"/> carrying <paramref name="message"/> (up to 0x1800 bytes).</summary>
    [LibraryImport(Lib)]
    public static partial int sceNotificationSend(int type, byte flag, byte* message);

    /// <summary>
    /// Sends a notification identified by <paramref name="id"/> (up to 0x20 bytes) with a JSON body in
    /// <paramref name="jsonData"/> (up to 0x1800 bytes).
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceNotificationSendById(int type, byte flag, byte* id, byte* jsonData);

    /// <summary>Shows the persistent banner next to the PS button, configured by a JSON string (or "{}" when null).</summary>
    [LibraryImport(Lib)]
    public static partial int sceNotificationShowPsButtonPersistentBanner(byte* jsonParam);

    /// <summary>Hides the persistent PS-button banner.</summary>
    [LibraryImport(Lib)]
    public static partial void sceNotificationHidePsButtonPersistentBanner();
}
