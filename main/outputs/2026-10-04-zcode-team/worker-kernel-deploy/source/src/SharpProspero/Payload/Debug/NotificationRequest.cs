// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Debug;

/// <summary>
/// Full notification request structure (0xC30 bytes). Exposes all fields for constructing
/// detailed notifications with priority, icons, URIs, and target user/app identifiers.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 0xC30)]
public unsafe struct NotificationRequest
{
    /// <summary>Notification type (0x64 for standard text toast).</summary>
    public int Type;

    /// <summary>Request identifier (-1 for untracked).</summary>
    public int RequestId;

    /// <summary>Priority level.</summary>
    public int Priority;

    /// <summary>Message identifier.</summary>
    public int MsgId;

    /// <summary>Target identifier (16 bytes, typically all 0xFF for broadcast).</summary>
    public fixed byte TargetId[16];

    /// <summary>User identifier.</summary>
    public int UserId;

    /// <summary>Application identifier.</summary>
    public int AppId;

    /// <summary>Error number associated with the notification.</summary>
    public int ErrorNum;

    /// <summary>Whether to use an icon image URI.</summary>
    public int UseIconImageUri;

    /// <summary>The notification message text (up to 1024 bytes, NUL-terminated UTF-8).</summary>
    public fixed byte Message[1024];

    /// <summary>Icon image URI (up to 1024 bytes, NUL-terminated UTF-8).</summary>
    public fixed byte Uri[1024];

    /// <summary>Additional string field (up to 1024 bytes).</summary>
    public fixed byte ExtraString[1024];
}
