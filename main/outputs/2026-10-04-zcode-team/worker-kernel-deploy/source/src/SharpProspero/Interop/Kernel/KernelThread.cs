// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Kernel;

/// <summary>
/// Processor masks a thread's affinity is set from. Bit <c>n</c> of a mask admits processor <c>n</c>.
/// </summary>
public static class SceKernelCpumask
{
    /// <summary>Every processor a mask can name. Value 0x1fff.</summary>
    public const ulong All = 0x1fff;

    /// <summary>The mask that admits processor <paramref name="processor"/> alone.</summary>
    public static ulong Only(int processor) => 1UL << processor;
}

/// <summary>
/// Thread bindings. A thread handle is the value the thread itself reads back from
/// <see cref="scePthreadSelf"/>; the affinity calls take that handle, so any thread holding it can
/// read or change another thread's processor set.
/// </summary>
public static unsafe partial class KernelThread
{
    private const string Lib = "libkernel";

    /// <summary>The calling thread's own handle.</summary>
    [LibraryImport(Lib)]
    public static partial nint scePthreadSelf();

    /// <summary>
    /// Confines <paramref name="thread"/> to the processors named by <paramref name="mask"/>.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePthreadSetaffinity(nint thread, ulong mask);

    /// <summary>Reads the processors <paramref name="thread"/> is allowed to run on.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePthreadGetaffinity(nint thread, ulong* mask);

    /// <summary>The scheduling priority a thread is created with. Value 700.</summary>
    public const int PriorityDefault = 700;

    /// <summary>The most urgent priority a thread may be given. Value 256.</summary>
    /// <remarks>
    /// The scale runs the other way round from the number: a smaller number is served first.
    /// </remarks>
    public const int PriorityHighest = 256;

    /// <summary>The least urgent priority a thread may be given. Value 767.</summary>
    public const int PriorityLowest = 767;

    /// <summary>Sets <paramref name="thread"/>'s scheduling priority.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePthreadSetprio(nint thread, int priority);

    /// <summary>Reads <paramref name="thread"/>'s scheduling priority.</summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePthreadGetprio(nint thread, int* priority);

    /// <summary>
    /// Names <paramref name="thread"/> from a null-terminated UTF-8 string, which is what a profiler and
    /// a crash report show in place of a bare thread number.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(Lib)]
    public static partial int scePthreadRename(nint thread, byte* name);

    /// <summary>The identifier the scheduler tracks the calling thread by.</summary>
    [LibraryImport(Lib)]
    public static partial int scePthreadGetthreadid();

    /// <summary>Gives up the rest of the calling thread's slice to another thread that is ready.</summary>
    [LibraryImport(Lib)]
    public static partial void scePthreadYield();
}
