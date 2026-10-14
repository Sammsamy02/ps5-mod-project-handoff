// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Mouse;

/// <summary>The mouse buttons, as a bitmask.</summary>
[Flags]
public enum MouseButton : uint
{
    None = 0,

    /// <summary>Normally the left button.</summary>
    Primary = 0x00000001,

    /// <summary>Normally the right button.</summary>
    Secondary = 0x00000002,

    /// <summary>Normally the wheel press.</summary>
    Optional = 0x00000004,

    /// <summary>Normally the back button.</summary>
    Optional2 = 0x00000008,

    /// <summary>Normally the forward button.</summary>
    Optional3 = 0x00000010,

    /// <summary>The system has taken the mouse; this read is not for the application.</summary>
    Intercepted = 0x80000000,
}

/// <summary>
/// One read of the mouse. The movement is relative, not an absolute position. The C bool field is one
/// byte, held here as a byte so the fields after it keep their offsets.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 40)]
public unsafe struct SceMouseData
{
    /// <summary>The system timestamp of this read, in microseconds.</summary>
    public ulong Timestamp;

    private byte _connected;

    /// <summary>The buttons held, as a bitmask.</summary>
    public MouseButton Buttons;

    /// <summary>The movement since the last read, in the x direction.</summary>
    public int XAxis;

    /// <summary>The movement since the last read, in the y direction.</summary>
    public int YAxis;

    /// <summary>The wheel movement since the last read.</summary>
    public int Wheel;

    /// <summary>The tilt-wheel movement since the last read, for a mouse that has one.</summary>
    public int Tilt;

    private fixed byte _reserve[8];

    /// <summary>True while a mouse is connected.</summary>
    public readonly bool Connected => _connected != 0;
}

/// <summary>The open parameters.</summary>
[StructLayout(LayoutKind.Sequential, Size = 8)]
public unsafe struct SceMouseOpenParam
{
    /// <summary>0 for a normal open, 1 to merge all mice into one handle.</summary>
    public byte BehaviorFlag;

    private fixed byte _reserve[7];
}

/// <summary>USB mouse bindings.</summary>
public static unsafe partial class Mouse
{
    private const string Lib = "libSceMouse";

    /// <summary>Open normally, one handle per mouse.</summary>
    public const byte OpenNormal = 0x00;

    /// <summary>Merge every mouse into one handle.</summary>
    public const byte OpenMerged = 0x01;

    /// <summary>Starts the mouse service. Call once before opening. Zero on success.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMouseInit();

    /// <summary>Opens a mouse for a user, returning a handle, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMouseOpen(int userId, int type, int index, SceMouseOpenParam* param);

    /// <summary>Closes a mouse handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceMouseClose(int handle);

    /// <summary>The largest number of history entries a single read reports.</summary>
    public const int MaxDataCount = 64;

    /// <summary>
    /// Reads up to <paramref name="count"/> history entries into <paramref name="data"/>, newest
    /// first. Returns the number read, or a negative error code. Read one entry for the latest state.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceMouseRead(int handle, SceMouseData* data, int count);
}
