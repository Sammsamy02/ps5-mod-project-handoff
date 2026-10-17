// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Threading;

/// <summary>
/// How the scheduler picks between threads that share a priority. The priority decides which thread is
/// served first; the policy decides what happens between equals.
/// </summary>
public enum SchedulingPolicy
{
    /// <summary>The time-shared policy a thread runs under unless it is changed.</summary>
    Default = KernelScheduling.SchedOther,

    /// <summary>
    /// A thread runs until it blocks or yields rather than being pre-empted by an equal. Suits a fixed
    /// piece of work that must not be interrupted part-way, and starves its equals if it never yields.
    /// </summary>
    FirstInFirstOut = KernelScheduling.SchedFifo,

    /// <summary>Equal-priority threads take turns, so none of them can hold the processor.</summary>
    RoundRobin = KernelScheduling.SchedRoundRobin,
}

/// <summary>
/// Places the calling thread on the machine's processors and sets how urgently it is served. A large
/// application uses this to keep its parts apart: the frame loop on one processor, an emulated core or
/// an audio mixer on another, background loading on the rest, so none of them can take time from the
/// others.
/// </summary>
/// <remarks>
/// <para>
/// A processor set is a bit mask: bit <c>n</c> admits processor <c>n</c>. Build one with
/// <see cref="Only"/> or <see cref="Mask"/> rather than writing the number out.
/// </para>
/// <para>
/// The calls act on the thread that makes them, because a thread is addressed by a handle only it can
/// read back. To place a worker, call these from inside that worker.
/// </para>
/// </remarks>
/// <example>
/// Give an emulated core a processor of its own:
/// <code>
/// var core = new BackgroundOperation(() =&gt;
/// {
///     Processor.PlaceCurrentThread(Processor.Only(4), Processor.PriorityDefault - 32);
///     RunCore();
/// }, "core");
/// </code>
/// </example>
public static unsafe class Processor
{
    /// <summary>Every processor a mask can name.</summary>
    public const ulong All = SceKernelCpumask.All;

    /// <summary>The priority a thread is created with.</summary>
    /// <remarks>The scale runs the other way round from the number: a smaller number is served first.</remarks>
    public const int PriorityDefault = KernelThread.PriorityDefault;

    /// <summary>The most urgent priority a thread may be given.</summary>
    public const int PriorityHighest = KernelThread.PriorityHighest;

    /// <summary>The least urgent priority a thread may be given.</summary>
    public const int PriorityLowest = KernelThread.PriorityLowest;

    /// <summary>How many processors a mask can name.</summary>
    public const int Count = 13;

    /// <summary>The mask admitting <paramref name="processor"/> alone.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="processor"/> is outside the machine's range.</exception>
    public static ulong Only(int processor)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(processor);
        ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(processor, Count);
        return 1UL << processor;
    }

    /// <summary>The mask admitting every processor in <paramref name="processors"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException">A processor is outside the machine's range.</exception>
    public static ulong Mask(ReadOnlySpan<int> processors)
    {
        ulong mask = 0;
        foreach (int processor in processors)
            mask |= Only(processor);
        return mask;
    }

    /// <summary>
    /// The mask admitting the processors from <paramref name="first"/> up to but not including
    /// <paramref name="afterLast"/>, which is how a block of processors is set aside for one job.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException">The range falls outside the machine's processors.</exception>
    public static ulong Range(int first, int afterLast)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(first);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(afterLast, Count);
        ArgumentOutOfRangeException.ThrowIfLessThan(afterLast, first);
        ulong mask = 0;
        for (int i = first; i < afterLast; i++)
            mask |= 1UL << i;
        return mask;
    }

    /// <summary>Whether <paramref name="priority"/> is one the platform accepts.</summary>
    public static bool IsValidPriority(int priority)
        => priority >= PriorityHighest && priority <= PriorityLowest;

    /// <summary>Brings <paramref name="priority"/> back inside the range the platform accepts.</summary>
    /// <remarks>
    /// A priority is usually written as an offset from the default - a little more urgent, a little less
    /// - and an offset applied twice, or applied to an already-shifted value, walks out of the range.
    /// The platform refuses the whole call in that case rather than doing what was meant, so a computed
    /// priority is clamped before it is set.
    /// </remarks>
    public static int ClampPriority(int priority)
        => Math.Clamp(priority, PriorityHighest, PriorityLowest);

    /// <summary>Which processor the calling thread is on at this instant.</summary>
    /// <remarks>
    /// A thread that has not been confined can report a different number from one call to the next, so
    /// this says where the thread is now rather than where it will stay.
    /// </remarks>
    public static int Current => KernelClock.sceKernelGetCurrentCpu();

    /// <summary>The identifier the scheduler tracks the calling thread by.</summary>
    public static int CurrentThreadId => KernelThread.scePthreadGetthreadid();

    /// <summary>The processors the calling thread is allowed to run on.</summary>
    /// <exception cref="ProsperoException">The processor set could not be read.</exception>
    public static ulong CurrentThreadAffinity
    {
        get
        {
            ulong mask = 0;
            SceResult.ThrowIfFailed(
                KernelThread.scePthreadGetaffinity(KernelThread.scePthreadSelf(), &mask),
                nameof(KernelThread.scePthreadGetaffinity));
            return mask;
        }
        set => SetCurrentThreadAffinity(value);
    }

    /// <summary>Confines the calling thread to <paramref name="mask"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="mask"/> names no processor.</exception>
    /// <exception cref="ProsperoException">The processor set was refused.</exception>
    public static void SetCurrentThreadAffinity(ulong mask)
    {
        // A mask with no bit set admits nothing, which would leave the thread with nowhere to run. The
        // platform reports that as a plain argument error, so it is named here instead.
        if ((mask & All) == 0)
            throw new ArgumentOutOfRangeException(nameof(mask), "A processor set must name at least one processor.");
        SceResult.ThrowIfFailed(
            KernelThread.scePthreadSetaffinity(KernelThread.scePthreadSelf(), mask),
            nameof(KernelThread.scePthreadSetaffinity));
    }

    /// <summary>Confines the calling thread to <paramref name="mask"/>, reporting refusal rather than throwing.</summary>
    /// <returns>True when the thread was confined.</returns>
    public static bool TrySetCurrentThreadAffinity(ulong mask)
        => (mask & All) != 0
        && SceResult.Succeeded(KernelThread.scePthreadSetaffinity(KernelThread.scePthreadSelf(), mask));

    /// <summary>How urgently the calling thread is served.</summary>
    /// <exception cref="ProsperoException">The priority could not be read or set.</exception>
    public static int CurrentThreadPriority
    {
        get
        {
            int priority = 0;
            SceResult.ThrowIfFailed(
                KernelThread.scePthreadGetprio(KernelThread.scePthreadSelf(), &priority),
                nameof(KernelThread.scePthreadGetprio));
            return priority;
        }
        set
        {
            ArgumentOutOfRangeException.ThrowIfLessThan(value, PriorityHighest);
            ArgumentOutOfRangeException.ThrowIfGreaterThan(value, PriorityLowest);
            SceResult.ThrowIfFailed(
                KernelThread.scePthreadSetprio(KernelThread.scePthreadSelf(), value),
                nameof(KernelThread.scePthreadSetprio));
        }
    }

    /// <summary>
    /// Confines the calling thread to <paramref name="mask"/> and sets its priority in one call, which
    /// is what a worker does as its first act.
    /// </summary>
    /// <param name="mask">The processors the thread may run on.</param>
    /// <param name="priority">
    /// How urgently to serve it, between <see cref="PriorityHighest"/> and <see cref="PriorityLowest"/>,
    /// or null to leave it as it is.
    /// </param>
    /// <exception cref="ProsperoException">The processor set or the priority was refused.</exception>
    public static void PlaceCurrentThread(ulong mask, int? priority = null)
    {
        SetCurrentThreadAffinity(mask);
        if (priority is int value)
            CurrentThreadPriority = value;
    }

    /// <summary>
    /// Names the calling thread, which is the name a profiler and a crash report show in place of a bare
    /// thread number.
    /// </summary>
    /// <exception cref="ProsperoException">The name was refused.</exception>
    public static void NameCurrentThread(string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        int byteCount = Encoding.UTF8.GetByteCount(name);
        Span<byte> owned = byteCount < 128 ? stackalloc byte[byteCount + 1] : new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(name, owned);
        owned[byteCount] = 0;
        fixed (byte* p = owned)
            SceResult.ThrowIfFailed(
                KernelThread.scePthreadRename(KernelThread.scePthreadSelf(), p),
                nameof(KernelThread.scePthreadRename));
    }

    /// <summary>
    /// Gives up the rest of the calling thread's slice to another thread that is ready. Use this in a
    /// wait that is expected to be short; a longer wait belongs in a sleep.
    /// </summary>
    public static void Yield() => KernelThread.scePthreadYield();

    /// <summary>
    /// The policy and priority the calling thread runs under. The policy only decides what happens
    /// between threads that share a priority; the priority decides which is served first.
    /// </summary>
    /// <exception cref="ProsperoException">The settings could not be read.</exception>
    public static (SchedulingPolicy Policy, int Priority) CurrentThreadScheduling
    {
        get
        {
            int policy = 0;
            SceKernelSchedParam param = default;
            SceResult.ThrowIfFailed(
                KernelScheduling.scePthreadGetschedparam(KernelThread.scePthreadSelf(), &policy, &param),
                nameof(KernelScheduling.scePthreadGetschedparam));
            return ((SchedulingPolicy)policy, param.Priority);
        }
    }

    /// <summary>Sets the calling thread's policy and priority in one call.</summary>
    /// <param name="policy">What happens between threads that share the priority.</param>
    /// <param name="priority">
    /// How urgently to serve it, between <see cref="PriorityHighest"/> and <see cref="PriorityLowest"/>.
    /// </param>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="priority"/> is outside the range.</exception>
    /// <exception cref="ProsperoException">The settings were refused.</exception>
    public static void SetCurrentThreadScheduling(SchedulingPolicy policy, int priority)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(priority, PriorityHighest);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(priority, PriorityLowest);
        var param = new SceKernelSchedParam { Priority = priority };
        SceResult.ThrowIfFailed(
            KernelScheduling.scePthreadSetschedparam(KernelThread.scePthreadSelf(), (int)policy, &param),
            nameof(KernelScheduling.scePthreadSetschedparam));
    }

    /// <summary>
    /// The processor time the calling thread has used. This advances only while the thread runs, so it
    /// measures work done rather than time passed and is the number to compare two workers by.
    /// </summary>
    /// <exception cref="ProsperoException">The clock could not be found or read.</exception>
    public static TimeSpan CurrentThreadProcessorTime
    {
        get
        {
            int clockId = 0;
            SceResult.ThrowIfFailed(
                KernelScheduling.scePthreadGetcpuclockid(KernelThread.scePthreadSelf(), &clockId),
                nameof(KernelScheduling.scePthreadGetcpuclockid));

            KernelTimespec time = default;
            SceResult.ThrowIfFailed(
                KernelClock.sceKernelClockGettime(clockId, &time), nameof(KernelClock.sceKernelClockGettime));
            return TimeSpan.FromTicks(time.Seconds * TimeSpan.TicksPerSecond + time.Nanoseconds / 100);
        }
    }
}
