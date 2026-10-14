// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using System;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// Reads and writes the system's stored settings by identifier. The settings service keeps the values
/// the system itself uses, so a tool can report or change one that no other service exposes.
/// </summary>
/// <remarks>
/// The service is loaded at run time and each entry is addressed by a numeric identifier the system
/// defines; this type does not name them, because the meaning of an identifier belongs to the system
/// version in use. Reaching the service needs the running process to be permitted to; where it is not,
/// <see cref="TryOpen"/> reports that instead of throwing, and each read has a Try form that reports a
/// refusal rather than raising. Open it once, use it, and dispose it to unload the service.
/// </remarks>
/// <example>
/// <code>
/// if (SystemSettings.TryOpen(out SystemSettings? settings))
/// {
///     using (settings)
///     {
///         if (settings!.TryGetInt32(id, out int value))
///             Log.Info($"setting {id} = {value}");
///     }
/// }
/// </code>
/// </example>
public sealed unsafe class SystemSettings : IDisposable
{
    /// <summary>Where the settings service is loaded from.</summary>
    public const string ModulePath = "/system/common/lib/libSceRegMgr.sprx";

    /// <summary>The longest string a read returns unless a larger size is asked for.</summary>
    public const int DefaultStringLength = 256;

    private readonly SystemLibrary _library;
    private readonly delegate* unmanaged<uint, int*, int> _getInt;
    private readonly delegate* unmanaged<uint, int, int> _setInt;
    private readonly delegate* unmanaged<uint, byte*, ulong, int> _getStr;
    private readonly delegate* unmanaged<uint, byte*, ulong, int> _setStr;
    private bool _disposed;

    private SystemSettings(
        SystemLibrary library,
        delegate* unmanaged<uint, int*, int> getInt,
        delegate* unmanaged<uint, int, int> setInt,
        delegate* unmanaged<uint, byte*, ulong, int> getStr,
        delegate* unmanaged<uint, byte*, ulong, int> setStr)
    {
        _library = library;
        _getInt = getInt;
        _setInt = setInt;
        _getStr = getStr;
        _setStr = setStr;
    }

    /// <summary>Loads the settings service and resolves the entry points.</summary>
    /// <exception cref="ProsperoException">The service could not be loaded or an entry point is missing.</exception>
    public static SystemSettings Open()
    {
        SystemLibrary library = SystemLibrary.Open(ModulePath);
        try
        {
            return new SystemSettings(
                library,
                (delegate* unmanaged<uint, int*, int>)library.GetFunction("sceRegMgrGetInt"),
                (delegate* unmanaged<uint, int, int>)library.GetFunction("sceRegMgrSetInt"),
                (delegate* unmanaged<uint, byte*, ulong, int>)library.GetFunction("sceRegMgrGetStr"),
                (delegate* unmanaged<uint, byte*, ulong, int>)library.GetFunction("sceRegMgrSetStr"));
        }
        catch
        {
            library.Dispose();
            throw;
        }
    }

    /// <summary>
    /// Loads the settings service, reporting whether it could be reached rather than raising. Use this
    /// where the process may not be permitted to reach it.
    /// </summary>
    public static bool TryOpen(out SystemSettings? settings)
    {
        try
        {
            settings = Open();
            return true;
        }
        catch (ProsperoException)
        {
            settings = null;
            return false;
        }
    }

    /// <summary>Reads the whole-number setting <paramref name="settingId"/>.</summary>
    /// <exception cref="ProsperoException">The setting could not be read.</exception>
    public int GetInt32(uint settingId)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        int value = 0;
        SceResult.ThrowIfFailed(_getInt(settingId, &value), "sceRegMgrGetInt");
        return value;
    }

    /// <summary>Reads the whole-number setting <paramref name="settingId"/>, reporting failure instead of raising.</summary>
    public bool TryGetInt32(uint settingId, out int value)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        int read = 0;
        bool ok = SceResult.Succeeded(_getInt(settingId, &read));
        value = ok ? read : 0;
        return ok;
    }

    /// <summary>Writes <paramref name="value"/> to the whole-number setting <paramref name="settingId"/>.</summary>
    /// <exception cref="ProsperoException">The setting could not be written.</exception>
    public void SetInt32(uint settingId, int value)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        SceResult.ThrowIfFailed(_setInt(settingId, value), "sceRegMgrSetInt");
    }

    /// <summary>Reads the text setting <paramref name="settingId"/>.</summary>
    /// <exception cref="ProsperoException">The setting could not be read.</exception>
    public string GetString(uint settingId, int maxLength = DefaultStringLength)
    {
        if (!TryGetString(settingId, out string? value, maxLength))
            throw new ProsperoException("sceRegMgrGetStr", -1);
        return value!;
    }

    /// <summary>Reads the text setting <paramref name="settingId"/>, reporting failure instead of raising.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="maxLength"/> is not positive.</exception>
    public bool TryGetString(uint settingId, out string? value, int maxLength = DefaultStringLength)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxLength);

        byte[] buffer = new byte[maxLength];
        fixed (byte* p = buffer)
        {
            if (!SceResult.Succeeded(_getStr(settingId, p, (ulong)maxLength)))
            {
                value = null;
                return false;
            }
        }

        // The service writes a null-terminated string into the buffer; anything after the terminator is
        // whatever the buffer already held.
        int length = Array.IndexOf(buffer, (byte)0);
        if (length < 0)
            length = buffer.Length;
        value = Encoding.UTF8.GetString(buffer, 0, length);
        return true;
    }

    /// <summary>Writes <paramref name="value"/> to the text setting <paramref name="settingId"/>.</summary>
    /// <exception cref="ProsperoException">The setting could not be written.</exception>
    public void SetString(uint settingId, string value)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentNullException.ThrowIfNull(value);

        int count = Encoding.UTF8.GetByteCount(value);
        byte[] buffer = new byte[count + 1];
        Encoding.UTF8.GetBytes(value, buffer);
        fixed (byte* p = buffer)
            SceResult.ThrowIfFailed(_setStr(settingId, p, (ulong)buffer.Length), "sceRegMgrSetStr");
    }

    /// <summary>Unloads the settings service.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        _library.Dispose();
    }
}
