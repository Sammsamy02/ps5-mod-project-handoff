// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Keyboard;
using SharpProspero.Platform;
using System;

namespace SharpProspero.Input;

/// <summary>
/// A keyboard layout, which decides the character a key produces. Read the user's own layout with
/// <see cref="KeycodeConverter.GetLayout"/>, or pass a fixed one.
/// </summary>
public enum KeyboardLayout
{
    /// <summary>Unknown layout.</summary>
    None = 0,

    /// <summary>Danish.</summary>
    Danish = 1,

    /// <summary>German (Germany).</summary>
    German = 2,

    /// <summary>German (Switzerland).</summary>
    GermanSwiss = 3,

    /// <summary>English (US).</summary>
    EnglishUs = 4,

    /// <summary>English (UK).</summary>
    EnglishGb = 5,

    /// <summary>Spanish (Spain).</summary>
    Spanish = 6,

    /// <summary>Spanish (Latin America).</summary>
    SpanishLatinAmerica = 7,

    /// <summary>Finnish.</summary>
    Finnish = 8,

    /// <summary>French (France).</summary>
    French = 9,

    /// <summary>French (Belgium).</summary>
    FrenchBelgian = 10,

    /// <summary>French (Canada).</summary>
    FrenchCanadian = 11,

    /// <summary>French (Switzerland).</summary>
    FrenchSwiss = 12,

    /// <summary>Italian.</summary>
    Italian = 13,

    /// <summary>Dutch.</summary>
    Dutch = 14,

    /// <summary>Norwegian.</summary>
    Norwegian = 15,

    /// <summary>Polish.</summary>
    Polish = 16,

    /// <summary>Portuguese (Brazil).</summary>
    PortugueseBrazil = 17,

    /// <summary>Portuguese (Portugal).</summary>
    PortuguesePortugal = 18,

    /// <summary>Russian.</summary>
    Russian = 19,

    /// <summary>Swedish.</summary>
    Swedish = 20,

    /// <summary>Turkish.</summary>
    Turkish = 21,

    /// <summary>Japanese (Latin input).</summary>
    JapaneseRoman = 22,

    /// <summary>Japanese (kana input).</summary>
    JapaneseKana = 23,

    /// <summary>Korean.</summary>
    Korean = 24,

    /// <summary>Simplified Chinese.</summary>
    SimplifiedChinese = 25,

    /// <summary>Arabic.</summary>
    Arabic = 30,

    /// <summary>Thai.</summary>
    Thai = 31,

    /// <summary>Czech.</summary>
    Czech = 32,

    /// <summary>Greek.</summary>
    Greek = 33,
}

/// <summary>
/// Turns a physical keyboard's key codes into the characters they produce. A key code from
/// <c>sceKeyboardReadState</c> is a position on the keyboard, not a letter; this applies the layout and
/// the held modifiers to get the character, so a build can read a USB keyboard directly without the
/// on-screen keyboard. It resolves its functions from a system library at run time, so open it where the
/// module is available and dispose it when done.
/// </summary>
/// <remarks>
/// Use <see cref="TryOpen"/> for a build that carries on without a physical keyboard, and <see cref="Open"/>
/// where the library is required. <see cref="ToCharacter"/> returns the character a key produces, and
/// <see cref="GetLayout"/> reads the user's chosen layout so a build honors it rather than assuming one.
/// </remarks>
public sealed unsafe class KeycodeConverter : IDisposable
{
    /// <summary>The system library the converter resolves its functions from.</summary>
    public const string ModulePath = "/system/common/lib/libSceConvertKeycode.sprx";

    private readonly SystemLibrary _library;
    private readonly delegate* unmanaged<ushort, uint, int, uint*, int> _getCharacter;
    private readonly delegate* unmanaged<ushort, int, ushort*, int> _getVirtualKeycode;
    private readonly delegate* unmanaged<int, int*, int> _getKeyboardType;
    private bool _disposed;

    private KeycodeConverter(
        SystemLibrary library,
        delegate* unmanaged<ushort, uint, int, uint*, int> getCharacter,
        delegate* unmanaged<ushort, int, ushort*, int> getVirtualKeycode,
        delegate* unmanaged<int, int*, int> getKeyboardType)
    {
        _library = library;
        _getCharacter = getCharacter;
        _getVirtualKeycode = getVirtualKeycode;
        _getKeyboardType = getKeyboardType;
    }

    /// <summary>Opens the converter, resolving its functions from the system library.</summary>
    /// <exception cref="ProsperoException">The library is not present or is missing an export.</exception>
    public static KeycodeConverter Open()
    {
        SystemLibrary library = SystemLibrary.Open(ModulePath);
        try
        {
            var getCharacter = (delegate* unmanaged<ushort, uint, int, uint*, int>)library.GetFunction("sceConvertKeycodeGetCharacter");
            var getVirtualKeycode = (delegate* unmanaged<ushort, int, ushort*, int>)library.GetFunction("sceConvertKeycodeGetVirtualKeycode");
            var getKeyboardType = (delegate* unmanaged<int, int*, int>)library.GetFunction("sceConvertKeycodeGetImeKeyboardType");
            return new KeycodeConverter(library, getCharacter, getVirtualKeycode, getKeyboardType);
        }
        catch
        {
            library.Dispose();
            throw;
        }
    }

    /// <summary>Opens the converter, or returns null when the library is not available.</summary>
    public static KeycodeConverter? TryOpen()
    {
        try
        {
            return Open();
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// The character key <paramref name="keycode"/> produces under <paramref name="layout"/> with
    /// <paramref name="modifiers"/> held and <paramref name="leds"/> on, or the null character when the
    /// key makes none (a modifier, a function key).
    /// </summary>
    /// <remarks>
    /// The two are packed into one word for the call, in the places it reads them: the modifiers eight
    /// bits up and the lock keys sixteen. Handing it the modifier byte as it stands puts every modifier
    /// where nothing looks, so shift, the right alt, control, caps lock and num lock all read as off
    /// and the answer comes back as the unshifted character with nothing reported.
    /// </remarks>
    public char ToCharacter(ushort keycode, KeyModifier modifiers, KeyboardLayout layout,
        KeyboardLed leds = KeyboardLed.None)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        uint status = ((uint)modifiers & 0xFF) << 8 | ((uint)leds & 7) << 16;
        uint character = 0;
        int result = _getCharacter(keycode, status, (int)layout, &character);
        return result == 0 ? (char)character : '\0';
    }

    /// <summary>
    /// The virtual key code for key <paramref name="keycode"/> under <paramref name="layout"/>, or -1
    /// when there is none.
    /// </summary>
    public int ToVirtualKeycode(ushort keycode, KeyboardLayout layout)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ushort virtualKeycode = 0;
        int result = _getVirtualKeycode(keycode, (int)layout, &virtualKeycode);
        return result == 0 ? virtualKeycode : -1;
    }

    /// <summary>
    /// The keyboard layout chosen for <paramref name="userId"/>, or <see cref="KeyboardLayout.None"/>
    /// when it cannot be read.
    /// </summary>
    public KeyboardLayout GetLayout(int userId = SceUser.System)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        int type = 0;
        int result = _getKeyboardType(userId, &type);
        return result == 0 ? (KeyboardLayout)type : KeyboardLayout.None;
    }

    /// <summary>Closes the system library.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        _library.Dispose();
    }
}
