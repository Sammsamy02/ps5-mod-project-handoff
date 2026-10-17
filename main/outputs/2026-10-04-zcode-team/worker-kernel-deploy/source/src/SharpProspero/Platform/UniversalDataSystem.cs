// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Np;
using System;
using System.Text;

namespace SharpProspero.Platform;

/// <summary>
/// The named properties of an event being built. Set the values the event carries, then the event is
/// posted. Valid only inside the build callback of <see cref="UniversalDataSystem.PostEvent"/>.
/// </summary>
public readonly ref struct UdsEvent
{
    private readonly unsafe void* _properties;

    internal unsafe UdsEvent(void* properties) => _properties = properties;

    /// <summary>Sets a string property.</summary>
    public unsafe UdsEvent Set(string key, string value)
    {
        fixed (byte* k = Utf8(key))
        fixed (byte* v = Utf8(value))
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetString(_properties, k, v),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetString));
        return this;
    }

    /// <summary>Sets a 32-bit integer property.</summary>
    public unsafe UdsEvent Set(string key, int value)
    {
        fixed (byte* k = Utf8(key))
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetInt32(_properties, k, value),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetInt32));
        return this;
    }

    /// <summary>Sets a 64-bit integer property.</summary>
    public unsafe UdsEvent Set(string key, long value)
    {
        fixed (byte* k = Utf8(key))
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetInt64(_properties, k, value),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetInt64));
        return this;
    }

    /// <summary>Sets a 64-bit floating-point property.</summary>
    public unsafe UdsEvent Set(string key, double value)
    {
        fixed (byte* k = Utf8(key))
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetFloat64(_properties, k, value),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetFloat64));
        return this;
    }

    /// <summary>Sets a boolean property.</summary>
    public unsafe UdsEvent Set(string key, bool value)
    {
        fixed (byte* k = Utf8(key))
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetBool(_properties, k, value),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemEventPropertyObjectSetBool));
        return this;
    }

    private static byte[] Utf8(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        int count = Encoding.UTF8.GetByteCount(text);
        byte[] buffer = new byte[count + 1];
        Encoding.UTF8.GetBytes(text, buffer);
        return buffer;
    }
}

/// <summary>Builds the properties of an event before it is posted.</summary>
public delegate void UdsEventBuilder(UdsEvent properties);

/// <summary>
/// Reports events to the system's universal-data-system: a trophy unlock, an activity start or end, a
/// statistic. Initialize the module once, open a session for a user, then post named events with their
/// properties. This is the write side that complements <see cref="TrophySet"/>'s read side — a trophy is
/// unlocked by posting the event its trophy set defines.
/// </summary>
/// <example>
/// <code>
/// UniversalDataSystem.Initialize();
/// using var uds = UniversalDataSystem.Open(userId);
/// uds.PostEvent("_UnlockTrophy", e => e.Set("_trophy_id", trophyId));
/// </code>
/// </example>
public sealed unsafe class UniversalDataSystem : IDisposable
{
    private int _context;
    private int _handle;

    private UniversalDataSystem(int context, int handle)
    {
        _context = context;
        _handle = handle;
    }

    /// <summary>Initializes the module with a working memory pool. Call once before opening a session.</summary>
    /// <exception cref="ProsperoException">Initialization failed.</exception>
    public static void Initialize(long poolSizeBytes = 128 * 1024)
    {
        var param = new SceNpUniversalDataSystemInitParam
        {
            Size = (nuint)sizeof(SceNpUniversalDataSystemInitParam),
            PoolSize = (nuint)poolSizeBytes,
        };
        SceResult.ThrowIfFailed(
            NpUniversalDataSystem.sceNpUniversalDataSystemInitialize(&param),
            nameof(NpUniversalDataSystem.sceNpUniversalDataSystemInitialize));
    }

    /// <summary>Shuts the module down.</summary>
    /// <exception cref="ProsperoException">Termination failed.</exception>
    public static void Terminate() =>
        SceResult.ThrowIfFailed(
            NpUniversalDataSystem.sceNpUniversalDataSystemTerminate(),
            nameof(NpUniversalDataSystem.sceNpUniversalDataSystemTerminate));

    /// <summary>Opens a session for <paramref name="userId"/> (context, handle, and registration).</summary>
    /// <exception cref="ProsperoException">The session could not be opened.</exception>
    public static UniversalDataSystem Open(int userId, uint serviceLabel = 0)
    {
        int handle;
        SceResult.ThrowIfFailed(
            NpUniversalDataSystem.sceNpUniversalDataSystemCreateHandle(&handle),
            nameof(NpUniversalDataSystem.sceNpUniversalDataSystemCreateHandle));

        int context;
        try
        {
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemCreateContext(&context, userId, serviceLabel, 0),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemCreateContext));
        }
        catch
        {
            NpUniversalDataSystem.sceNpUniversalDataSystemDestroyHandle(handle);
            throw;
        }

        try
        {
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemRegisterContext(context, handle, 0),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemRegisterContext));
        }
        catch
        {
            NpUniversalDataSystem.sceNpUniversalDataSystemDestroyContext(context);
            NpUniversalDataSystem.sceNpUniversalDataSystemDestroyHandle(handle);
            throw;
        }

        return new UniversalDataSystem(context, handle);
    }

    /// <summary>
    /// Builds an event with the given name, fills in its properties through <paramref name="build"/>, and
    /// posts it.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="build"/> is null.</exception>
    /// <exception cref="ProsperoException">The event could not be built or posted.</exception>
    public void PostEvent(string eventName, UdsEventBuilder build)
    {
        ArgumentNullException.ThrowIfNull(build);

        void* @event;
        void* properties;
        fixed (byte* name = Utf8(eventName))
        {
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemCreateEvent(name, null, &@event, &properties),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemCreateEvent));
        }

        try
        {
            build(new UdsEvent(properties));
            SceResult.ThrowIfFailed(
                NpUniversalDataSystem.sceNpUniversalDataSystemPostEvent(_context, _handle, @event, 0),
                nameof(NpUniversalDataSystem.sceNpUniversalDataSystemPostEvent));
        }
        finally
        {
            NpUniversalDataSystem.sceNpUniversalDataSystemDestroyEvent(@event);
        }
    }

    /// <summary>Destroys the handle and context.</summary>
    public void Dispose()
    {
        if (_handle >= 0)
        {
            NpUniversalDataSystem.sceNpUniversalDataSystemDestroyHandle(_handle);
            _handle = -1;
        }

        if (_context >= 0)
        {
            NpUniversalDataSystem.sceNpUniversalDataSystemDestroyContext(_context);
            _context = -1;
        }

        GC.SuppressFinalize(this);
    }

    ~UniversalDataSystem() => Dispose();

    private static byte[] Utf8(string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        int count = Encoding.UTF8.GetByteCount(text);
        byte[] buffer = new byte[count + 1];
        Encoding.UTF8.GetBytes(text, buffer);
        return buffer;
    }
}
