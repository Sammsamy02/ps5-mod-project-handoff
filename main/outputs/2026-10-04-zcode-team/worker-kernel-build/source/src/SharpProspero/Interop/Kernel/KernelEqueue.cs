// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Kernel;

/// <summary>
/// One report taken out of an event queue. The queue fills these; the accessor calls on
/// <see cref="KernelEqueue"/> read the fields rather than the caller picking them apart, because which
/// field means what depends on the filter.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 32)]
public unsafe struct SceKernelEvent
{
    /// <summary>What the report is about: a descriptor for the file filters, the caller's identifier for the rest.</summary>
    public nuint Identifier;

    /// <summary>The filter that produced the report, one of the <c>Filter*</c> values on <see cref="KernelEqueue"/>.</summary>
    public short Filter;

    /// <summary>The action and result bits, the <c>Flag*</c> values on <see cref="KernelEqueue"/>.</summary>
    public ushort Flags;

    /// <summary>The filter-specific bits. For a file event this is the set of <c>Note*</c> values that fired.</summary>
    public uint FilterFlags;

    /// <summary>The filter-specific payload: readable bytes for a read event, the error number when <see cref="Flags"/> reports one.</summary>
    public nint Data;

    /// <summary>The pointer the caller handed to the add call.</summary>
    public void* UserData;
}

/// <summary>
/// Event-queue bindings. A queue collects reports from several sources - timers, descriptor readiness,
/// file changes and events the application raises itself - so one thread can wait on all of them at
/// once instead of polling each in turn. Create a queue, add the sources to it, then block in
/// <see cref="sceKernelWaitEqueue"/> until at least one report arrives.
/// </summary>
/// <remarks>
/// Timeouts are a pointer to a microsecond count: a null pointer waits forever, and a pointer to zero
/// returns at once with whatever is already pending.
/// </remarks>
public static unsafe partial class KernelEqueue
{
    private const string Lib = "libkernel";

    /// <summary>A descriptor became readable. Value -1.</summary>
    public const short FilterRead = -1;

    /// <summary>A descriptor became writable. Value -2.</summary>
    public const short FilterWrite = -2;

    /// <summary>A watched file changed. Value -4.</summary>
    public const short FilterFile = -4;

    /// <summary>A timer elapsed. Value -7.</summary>
    public const short FilterTimer = -7;

    /// <summary>The application raised the event itself. Value -11.</summary>
    public const short FilterUser = -11;

    /// <summary>The display side reported. Value -13.</summary>
    public const short FilterVideoOut = -13;

    /// <summary>The graphics core reported. Value -14.</summary>
    public const short FilterGraphicsCore = -14;

    /// <summary>A high-resolution timer elapsed. Value -15.</summary>
    public const short FilterHighResolutionTimer = -15;

    /// <summary>The file was removed. Value 0x0001.</summary>
    public const uint NoteDelete = 0x0001;

    /// <summary>The file contents changed. Value 0x0002.</summary>
    public const uint NoteWrite = 0x0002;

    /// <summary>The file grew. Value 0x0004.</summary>
    public const uint NoteExtend = 0x0004;

    /// <summary>The file attributes changed. Value 0x0008.</summary>
    public const uint NoteAttrib = 0x0008;

    /// <summary>The file was renamed. Value 0x0020.</summary>
    public const uint NoteRename = 0x0020;

    /// <summary>Access to the file was revoked. Value 0x0040.</summary>
    public const uint NoteRevoke = 0x0040;

    /// <summary>Every file change the watch call accepts, the six <c>Note*</c> values combined.</summary>
    public const uint NoteAll = NoteDelete | NoteWrite | NoteExtend | NoteAttrib | NoteRename | NoteRevoke;

    /// <summary>Report flag: the source reached its end. Value 0x8000.</summary>
    public const ushort FlagEndOfFile = 0x8000;

    /// <summary>Report flag: the report carries an error, and <see cref="SceKernelEvent.Data"/> is its number. Value 0x4000.</summary>
    public const ushort FlagError = 0x4000;

    /// <summary>Creates a queue named <paramref name="name"/> (null-terminated UTF-8), writing its handle to <paramref name="queue"/>.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelCreateEqueue(nint* queue, byte* name);

    /// <summary>Destroys a queue. Sources still attached to it are dropped with it.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteEqueue(nint queue);

    /// <summary>
    /// Waits for at least one report on <paramref name="queue"/> and copies up to <paramref name="count"/>
    /// of them into <paramref name="events"/>, writing how many arrived to <paramref name="received"/>.
    /// <paramref name="timeoutMicroseconds"/> is null to wait forever.
    /// </summary>
    /// <returns>Zero on success, or a negative error code. A timeout is reported as an error.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelWaitEqueue(
        nint queue, SceKernelEvent* events, int count, int* received, uint* timeoutMicroseconds);

    /// <summary>
    /// Adds a repeating timer that reports under <paramref name="id"/> every
    /// <paramref name="microseconds"/>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddTimerEvent(nint queue, int id, uint microseconds, void* userData);

    /// <summary>Removes the timer added under <paramref name="id"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteTimerEvent(nint queue, int id);

    /// <summary>
    /// Adds a timer that reports under <paramref name="id"/> after the interval in
    /// <paramref name="time"/>, expressed to the nanosecond rather than the microsecond.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddHRTimerEvent(nint queue, int id, KernelTimespec* time, void* userData);

    /// <summary>Removes the high-resolution timer added under <paramref name="id"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteHRTimerEvent(nint queue, int id);

    /// <summary>
    /// Reports when <paramref name="descriptor"/> has at least <paramref name="size"/> bytes to read.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddReadEvent(nint queue, int descriptor, nuint size, void* userData);

    /// <summary>Stops reporting readability for <paramref name="descriptor"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteReadEvent(nint queue, int descriptor);

    /// <summary>Reports when <paramref name="descriptor"/> can take at least <paramref name="size"/> bytes.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddWriteEvent(nint queue, int descriptor, nuint size, void* userData);

    /// <summary>Stops reporting writability for <paramref name="descriptor"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteWriteEvent(nint queue, int descriptor);

    /// <summary>
    /// Watches the file behind <paramref name="descriptor"/> for the changes named by
    /// <paramref name="watch"/>, a combination of the <c>Note*</c> values.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddFileEvent(nint queue, int descriptor, int watch, void* userData);

    /// <summary>Stops watching the file behind <paramref name="descriptor"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteFileEvent(nint queue, int descriptor);

    /// <summary>
    /// Adds a source the application raises itself under <paramref name="id"/>. The report stays raised
    /// until the queue hands it out.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddUserEvent(nint queue, int id);

    /// <summary>
    /// Adds a source the application raises itself under <paramref name="id"/>, cleared as soon as it is
    /// reported rather than staying raised.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelAddUserEventEdge(nint queue, int id);

    /// <summary>Removes the source added under <paramref name="id"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteUserEvent(nint queue, int id);

    /// <summary>Raises the source added under <paramref name="id"/>, handing <paramref name="userData"/> to the waiter.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelTriggerUserEvent(nint queue, int id, void* userData);

    /// <summary>The filter that produced <paramref name="ev"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelGetEventFilter(SceKernelEvent* ev);

    /// <summary>What <paramref name="ev"/> is about: a descriptor, or the identifier the add call was given.</summary>
    [LibraryImport(Lib)]
    public static partial nuint sceKernelGetEventId(SceKernelEvent* ev);

    /// <summary>The filter-specific payload of <paramref name="ev"/>.</summary>
    [LibraryImport(Lib)]
    public static partial nint sceKernelGetEventData(SceKernelEvent* ev);

    /// <summary>The filter-specific bits of <paramref name="ev"/>.</summary>
    [LibraryImport(Lib)]
    public static partial uint sceKernelGetEventFflags(SceKernelEvent* ev);

    /// <summary>The error number carried by <paramref name="ev"/>, or zero when it carries none.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelGetEventError(SceKernelEvent* ev);

    /// <summary>The pointer the add call was given for <paramref name="ev"/>.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceKernelGetEventUserData(SceKernelEvent* ev);
}
