// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Kernel;

/// <summary>
/// Event-flag bindings. An event flag is a 64-bit pattern several threads read and one or more set:
/// a waiter names the bits it needs and blocks until they appear. It carries state a condition
/// variable does not, so a waiter that arrives after the bits were set does not miss them.
/// </summary>
public static unsafe partial class KernelEventFlags
{
    private const string Lib = "libkernel";

    /// <summary>Release waiting threads in arrival order. Value 0x01.</summary>
    public const uint AttrThreadFifo = 0x01;

    /// <summary>Release waiting threads in priority order. Value 0x02.</summary>
    public const uint AttrThreadPriority = 0x02;

    /// <summary>Only one thread may wait at a time. Value 0x10.</summary>
    public const uint AttrSingle = 0x10;

    /// <summary>Any number of threads may wait at once. Value 0x20.</summary>
    public const uint AttrMulti = 0x20;

    /// <summary>Wait until every named bit is set. Value 0x01.</summary>
    public const uint WaitModeAnd = 0x01;

    /// <summary>Wait until any named bit is set. Value 0x02.</summary>
    public const uint WaitModeOr = 0x02;

    /// <summary>Clear the whole pattern once the wait is satisfied. Value 0x10.</summary>
    public const uint WaitModeClearAll = 0x10;

    /// <summary>Clear only the named bits once the wait is satisfied. Value 0x20.</summary>
    public const uint WaitModeClearPattern = 0x20;

    /// <summary>The handle value that names no event flag. Value -1.</summary>
    public static readonly nint Invalid = -1;

    /// <summary>
    /// Creates an event flag named <paramref name="name"/> (null-terminated UTF-8) starting at
    /// <paramref name="initialPattern"/>, writing its handle to <paramref name="eventFlag"/>.
    /// <paramref name="attributes"/> combines one queueing order with one waiter count;
    /// <paramref name="options"/> is null.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelCreateEventFlag(
        nint* eventFlag, byte* name, uint attributes, ulong initialPattern, void* options);

    /// <summary>Destroys an event flag. Threads still waiting on it are released with a failure.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteEventFlag(nint eventFlag);

    /// <summary>
    /// Blocks until <paramref name="bitPattern"/> is satisfied under <paramref name="waitMode"/>,
    /// writing the pattern that satisfied it to <paramref name="resultPattern"/>.
    /// <paramref name="timeoutMicroseconds"/> is null to wait forever.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelWaitEventFlag(
        nint eventFlag, ulong bitPattern, uint waitMode, ulong* resultPattern, uint* timeoutMicroseconds);

    /// <summary>Tests <paramref name="bitPattern"/> without blocking.</summary>
    /// <returns>Zero when the pattern is already satisfied, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelPollEventFlag(
        nint eventFlag, ulong bitPattern, uint waitMode, ulong* resultPattern);

    /// <summary>Sets every bit of <paramref name="bitPattern"/>, releasing the waiters that becomes satisfied.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelSetEventFlag(nint eventFlag, ulong bitPattern);

    /// <summary>Clears every bit of <paramref name="bitPattern"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelClearEventFlag(nint eventFlag, ulong bitPattern);

    /// <summary>
    /// Releases every waiter at once with a failure and resets the pattern to
    /// <paramref name="setPattern"/>, writing how many were released to
    /// <paramref name="waitingThreads"/>. Use it to unblock a shutting-down subsystem.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelCancelEventFlag(nint eventFlag, ulong setPattern, int* waitingThreads);
}

/// <summary>
/// Counting-semaphore bindings. A semaphore holds a count between zero and a ceiling; a waiter takes
/// some of it and blocks while there is not enough. Unlike the event flag, a waiter can ask for more
/// than one unit at a time, which suits a pool of interchangeable resources.
/// </summary>
public static unsafe partial class KernelSemaphores
{
    private const string Lib = "libkernel";

    /// <summary>Release waiting threads in arrival order. Value 0x01.</summary>
    public const uint AttrThreadFifo = 0x01;

    /// <summary>Release waiting threads in priority order. Value 0x02.</summary>
    public const uint AttrThreadPriority = 0x02;

    /// <summary>The handle value that names no semaphore. Value -1.</summary>
    public static readonly nint Invalid = -1;

    /// <summary>
    /// Creates a semaphore named <paramref name="name"/> (null-terminated UTF-8) holding
    /// <paramref name="initialCount"/> and capped at <paramref name="maximumCount"/>, writing its handle
    /// to <paramref name="semaphore"/>. <paramref name="options"/> is null.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelCreateSema(
        nint* semaphore, byte* name, uint attributes, int initialCount, int maximumCount, void* options);

    /// <summary>Destroys a semaphore. Threads still waiting on it are released with a failure.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelDeleteSema(nint semaphore);

    /// <summary>
    /// Takes <paramref name="need"/> from the count, blocking while there is less.
    /// <paramref name="timeoutMicroseconds"/> is null to wait forever.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelWaitSema(nint semaphore, int need, uint* timeoutMicroseconds);

    /// <summary>Takes <paramref name="need"/> from the count only if that much is there.</summary>
    /// <returns>Zero when it was taken, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int sceKernelPollSema(nint semaphore, int need);

    /// <summary>Returns <paramref name="count"/> to the semaphore, releasing whatever waiters that satisfies.</summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelSignalSema(nint semaphore, int count);

    /// <summary>
    /// Releases every waiter at once with a failure and resets the count to <paramref name="count"/>,
    /// writing how many were released to <paramref name="waitingThreads"/>.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceKernelCancelSema(nint semaphore, int count, int* waitingThreads);
}
