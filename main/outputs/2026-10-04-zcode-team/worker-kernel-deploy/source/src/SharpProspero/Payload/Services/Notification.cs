// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Kernel;
using SharpProspero.Interop.Notification;
using System;

namespace SharpProspero.Payload.Services;

/// <summary>
/// Sends on-screen toast notifications from a payload context. Unlike the application-module
/// notification wrapper, this class makes no assumptions about a launcher-owned dialog handle,
/// a pre-initialised user-service session, or a running sysmodule prerequisite: it constructs
/// the notification request struct itself and calls the kernel or notification SPRX directly.
/// </summary>
/// <remarks>
/// <para>Two independent notification paths are available:</para>
/// <list type="bullet">
/// <item><description><see cref="SendKernelNotification"/>: Calls
/// <c>sceKernelSendNotificationRequest</c> from <c>libkernel</c>. It constructs a 3120-byte <see cref="SceNotificationRequest"/>
/// with the message at byte offset 45 and sends it on device zero. No extra SPRX module is
/// needed beyond the default <c>libkernel</c>.</description></item>
/// <item><description><see cref="SendNotification"/>: Calls <c>sceNotificationSend</c> from
/// <c>libSceNotification</c>. It sends a JSON payload through the notification service, supporting toast templates with
/// icons, sub-messages, and deep-link actions. Requires <c>libSceNotification</c> in the
/// payload's DT_NEEDED list.</description></item>
/// </list>
/// </remarks>
public static unsafe partial class PayloadNotification
{
    /// <summary>The system user id that represents the local system rather than a signed-in user.</summary>
    public const int LocalUserIdSystem = 0xFE;

    /// <summary>
    /// Sends a kernel notification toast with the given message. This is the simplest path: no
    /// extra SPRX module is needed beyond the default <c>libkernel</c>.
    /// </summary>
    /// <param name="message">A UTF-8 message, at most 1023 bytes (excluding the NUL terminator).
    /// Longer messages are silently truncated.</param>
    /// <returns>Zero on success, or a negative error code from the kernel.</returns>
    public static int SendKernelNotification(ReadOnlySpan<byte> message)
    {
        SceNotificationRequest req = default;

        // Type 0x64 + RequestId -1 + Target all-0xFF + Unk28 -1: the standard text notification
        // header, matching every known launching notification request in the corpus.
        req.Type = 0x64;
        req.RequestId = -1;
        for (int i = 0; i < 16; i++)
            req.Target[i] = 0xFF;
        req.Unk28 = -1;

        int copyLen = message.Length < 1023 ? message.Length : 1023;
        fixed (byte* src = message)
        {
            byte* dst = req.Message;
            for (int i = 0; i < copyLen; i++)
                dst[i] = src[i];
            dst[copyLen] = 0; // NUL terminator
        }

        return KernelNotification.sceKernelSendNotificationRequest(
            KernelNotification.ToastDevice, &req, (nuint)KernelNotification.RequestSize, 0);
    }

    /// <summary>
    /// Sends a notification through the notification service with a JSON payload. This path
    /// supports toast templates with icons, sub-messages, and deep-link actions.
    /// </summary>
    /// <param name="userId">The user id to send the notification on behalf of. Use
    /// <see cref="LocalUserIdSystem"/> for a system-level notification.</param>
    /// <param name="isLogged">Whether the notification should be recorded in the notification
    /// center log.</param>
    /// <param name="jsonPayload">A NUL-terminated UTF-8 JSON string describing the toast,
    /// up to 0x1800 bytes.</param>
    /// <returns>Zero on success, or a negative error code from the notification service.</returns>
    public static int SendNotification(int userId, bool isLogged, ReadOnlySpan<byte> jsonPayload)
    {
        fixed (byte* p = jsonPayload)
            return SceNotification.sceNotificationSend(userId, isLogged ? (byte)1 : (byte)0, p);
    }

    /// <summary>
    /// Sends a notification by id with a JSON body through the notification service.
    /// </summary>
    /// <param name="userId">The user id to send the notification on behalf of.</param>
    /// <param name="isLogged">Whether the notification should be logged.</param>
    /// <param name="id">A NUL-terminated UTF-8 notification identifier, up to 0x20 bytes.</param>
    /// <param name="jsonData">A NUL-terminated UTF-8 JSON body, up to 0x1800 bytes.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int SendNotificationById(int userId, bool isLogged,
        ReadOnlySpan<byte> id, ReadOnlySpan<byte> jsonData)
    {
        fixed (byte* pId = id)
        fixed (byte* pJson = jsonData)
            return SceNotification.sceNotificationSendById(userId, isLogged ? (byte)1 : (byte)0, pId, pJson);
    }

    /// <summary>
    /// Sends a system-level notification with plain text through the system-utility service.
    /// This is a third notification path distinct from the kernel notification and the
    /// JSON-based notification service. Requires <c>libSceSysUtil</c> in the payload's
    /// DT_NEEDED list.
    /// </summary>
    /// <param name="messageType">The notification type code.</param>
    /// <param name="message">A NUL-terminated UTF-8 message string.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int SendSystemNotification(int messageType, ReadOnlySpan<byte> message)
    {
        fixed (byte* p = message)
            return sceSysUtilSendSystemNotificationWithText(messageType, p);
    }

    [System.Runtime.InteropServices.LibraryImport("libSceSysUtil")]
    private static partial int sceSysUtilSendSystemNotificationWithText(int messageType, byte* message);
}
