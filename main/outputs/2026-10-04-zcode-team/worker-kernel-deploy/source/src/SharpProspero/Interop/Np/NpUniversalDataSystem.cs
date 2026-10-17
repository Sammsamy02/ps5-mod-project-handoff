// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Np;

/// <summary>Start-up sizes for the universal-data-system.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpUniversalDataSystemInitParam
{
    /// <summary>The size of this struct.</summary>
    public nuint Size;

    /// <summary>The size of the working memory pool.</summary>
    public nuint PoolSize;
}

/// <summary>Memory-pool usage for the universal-data-system.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpUniversalDataSystemMemoryStat
{
    /// <summary>The pool size.</summary>
    public nuint PoolSize;

    /// <summary>The high-water mark of in-use memory.</summary>
    public nuint MaxInuseSize;

    /// <summary>The memory in use now.</summary>
    public nuint CurrentInuseSize;
}

/// <summary>Event-storage statistics for the universal-data-system.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceNpUniversalDataSystemStorageStat
{
    /// <summary>Events received.</summary>
    public nuint InEvents;

    /// <summary>Events sent.</summary>
    public nuint OutEvents;

    /// <summary>Events lost.</summary>
    public nuint LostEvents;

    /// <summary>The high-water mark of in-use storage.</summary>
    public nuint MaxInuseSize;

    /// <summary>Events held now.</summary>
    public nuint CurrentEvents;

    /// <summary>Storage in use now.</summary>
    public nuint CurrentInuseSize;

    /// <summary>Storage free now.</summary>
    public nuint CurrentFreeSize;
}

/// <summary>
/// Universal-data-system bindings (libSceNpUniversalDataSystem). This is how a title reports events —
/// posting a trophy unlock, an activity start or end, or a statistic — by building an event with named
/// properties and posting it. Events, property objects and property arrays are opaque and are created and
/// destroyed through these calls.
/// </summary>
public static unsafe partial class NpUniversalDataSystem
{
    private const string Lib = "libSceNpUniversalDataSystem";

    /// <summary>Initializes the module with a working pool.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemInitialize(SceNpUniversalDataSystemInitParam* param);

    /// <summary>Shuts the module down.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemTerminate();

    /// <summary>Reads the memory-pool usage.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemGetMemoryStat(SceNpUniversalDataSystemMemoryStat* stat);

    /// <summary>Creates a context for a user and service label.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemCreateContext(int* context, int userId, uint serviceLabel, ulong options);

    /// <summary>Destroys a context.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemDestroyContext(int context);

    /// <summary>Registers a context against a handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemRegisterContext(int context, int handle, ulong options);

    /// <summary>Creates a work handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemCreateHandle(int* handle);

    /// <summary>Destroys a work handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemDestroyHandle(int handle);

    /// <summary>Aborts the operation on a handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemAbortHandle(int handle);

    /// <summary>Posts a built event.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemPostEvent(int context, int handle, void* @event, ulong options);

    /// <summary>Builds an event with the given name and property object.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemCreateEvent(byte* eventName, void* prop, void** newEvent, void** propPtr);

    /// <summary>Destroys an event.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemDestroyEvent(void* @event);

    /// <summary>Estimates the serialized size of an event.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventEstimateSize(void* @event, nuint* size);

    /// <summary>Serializes an event to a string.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventToString(void* @event, byte* buffer, nuint bufferSize, nuint* stringSize);

    /// <summary>Creates a property object.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemCreateEventPropertyObject(void** newObject);

    /// <summary>Destroys a property object.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemDestroyEventPropertyObject(void* @object);

    /// <summary>Sets a string property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetString(void* @object, byte* key, byte* value);

    /// <summary>Sets a 32-bit signed property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetInt32(void* @object, byte* key, int value);

    /// <summary>Sets a 32-bit unsigned property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetUInt32(void* @object, byte* key, uint value);

    /// <summary>Sets a 64-bit signed property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetInt64(void* @object, byte* key, long value);

    /// <summary>Sets a 64-bit unsigned property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetUInt64(void* @object, byte* key, ulong value);

    /// <summary>Sets a 32-bit float property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetFloat32(void* @object, byte* key, float value);

    /// <summary>Sets a 64-bit float property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetFloat64(void* @object, byte* key, double value);

    /// <summary>Sets a boolean property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetBool(void* @object, byte* key, [MarshalAs(UnmanagedType.U1)] bool value);

    /// <summary>Sets a binary property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetBinary(void* @object, byte* key, void* value, nuint valueSize);

    /// <summary>Sets a nested object property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetObject(void* @object, byte* key, void* value, void** valuePtr);

    /// <summary>Sets a nested array property.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyObjectSetArray(void* @object, byte* key, void* value, void** valuePtr);

    /// <summary>Creates a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemCreateEventPropertyArray(void** newArray);

    /// <summary>Destroys a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemDestroyEventPropertyArray(void* array);

    /// <summary>Appends a string to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetString(void* array, byte* value);

    /// <summary>Appends a 32-bit signed value to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetInt32(void* array, int value);

    /// <summary>Appends a 32-bit unsigned value to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetUInt32(void* array, uint value);

    /// <summary>Appends a 64-bit signed value to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetInt64(void* array, long value);

    /// <summary>Appends a 64-bit unsigned value to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetUInt64(void* array, ulong value);

    /// <summary>Appends a 32-bit float to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetFloat32(void* array, float value);

    /// <summary>Appends a 64-bit float to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetFloat64(void* array, double value);

    /// <summary>Appends a boolean to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetBool(void* array, [MarshalAs(UnmanagedType.U1)] bool value);

    /// <summary>Appends binary data to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetBinary(void* array, void* value, nuint valueSize);

    /// <summary>Appends a nested object to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetObject(void* array, void* value, void** valuePtr);

    /// <summary>Appends a nested array to a property array.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemEventPropertyArraySetArray(void* array, void* value, void** valuePtr);

    /// <summary>Reads the event-storage statistics.</summary>
    [LibraryImport(Lib)]
    public static partial int sceNpUniversalDataSystemGetStorageStat(int context, SceNpUniversalDataSystemStorageStat* stat);
}
