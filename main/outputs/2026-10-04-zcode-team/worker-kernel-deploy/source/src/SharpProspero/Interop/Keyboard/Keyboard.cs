// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Keyboard;

/// <summary>The modifier keys held with a keypress.</summary>
[Flags]
public enum KeyModifier : uint
{
    None = 0,
    LeftControl = 1u << 0,
    LeftShift = 1u << 1,
    LeftAlt = 1u << 2,
    LeftGui = 1u << 3,
    RightControl = 1u << 4,
    RightShift = 1u << 5,
    RightAlt = 1u << 6,
    RightGui = 1u << 7,
}

/// <summary>The lock keys that are on.</summary>
[Flags]
public enum KeyboardLed : uint
{
    /// <summary>None of them.</summary>
    None = 0,

    /// <summary>The number pad types digits.</summary>
    NumLock = 1u << 0,

    /// <summary>Letters come out capital.</summary>
    CapsLock = 1u << 1,

    /// <summary>Scroll lock is on.</summary>
    ScrollLock = 1u << 2,
}

/// <summary>
/// One read of the keyboard. The C bool fields are one byte each, so they are held as bytes here and
/// read through the accessor properties; a four-byte managed bool would shift every field after it.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 96)]
public unsafe struct SceKeyboardData
{
    /// <summary>The system timestamp of this read, in microseconds.</summary>
    public ulong Timestamp;

    private byte _intercepted;
    private fixed byte _reserve1[7];
    private byte _connected;

    /// <summary>The number of valid entries in <see cref="KeyCode"/>.</summary>
    public int Length;

    /// <summary>The lock keys that are on.</summary>
    public KeyboardLed Led;

    /// <summary>The modifier keys held, as a bitmask.</summary>
    public KeyModifier ModifierKey;

    /// <summary>The raw USB HID usage codes of the keys held, the first <see cref="Length"/> valid.</summary>
    public fixed ushort KeyCode[16];

    private fixed byte _reserve2[32];

    /// <summary>True when the system has taken keyboard focus and this read is not for the application.</summary>
    public readonly bool Intercepted => _intercepted != 0;

    /// <summary>True while a keyboard is connected.</summary>
    public readonly bool Connected => _connected != 0;
}

/// <summary>The open parameters. Reserved; pass a zeroed value.</summary>
[StructLayout(LayoutKind.Sequential, Size = 8)]
public unsafe struct SceKeyboardOpenParam
{
    private fixed byte _reserve[8];
}

/// <summary>USB keyboard bindings.</summary>
public static unsafe partial class Keyboard
{
    private const string Lib = "libSceKeyboard";

    /// <summary>The standard keyboard port type.</summary>
    public const int PortTypeStandard = 0;

    /// <summary>The largest number of keycodes a single read reports.</summary>
    public const int MaxKeyCodes = 16;

    /// <summary>Starts the keyboard service. Call once before opening. Zero on success.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKeyboardInit();

    /// <summary>Opens a keyboard for a user, returning a handle, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKeyboardOpen(int userId, int type, int index, SceKeyboardOpenParam* param);

    /// <summary>Closes a keyboard handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKeyboardClose(int handle);

    /// <summary>Reads the current keyboard state into <paramref name="data"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKeyboardReadState(int handle, SceKeyboardData* data);
}
