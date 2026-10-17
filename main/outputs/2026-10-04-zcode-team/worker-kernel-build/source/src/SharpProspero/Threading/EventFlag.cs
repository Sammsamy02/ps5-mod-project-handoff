// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Threading;

/// <summary>How a wait treats the bits it was given.</summary>
public enum EventFlagWait
{
    /// <summary>Wait until every named bit is set.</summary>
    All,

    /// <summary>Wait until any named bit is set.</summary>
    Any,
}

/// <summary>
/// What a satisfied wait does to the pattern afterwards.
/// </summary>
public enum EventFlagClear
{
    /// <summary>Leave the pattern as it is, so a later waiter sees the same bits.</summary>
    None,

    /// <summary>Clear the bits the waiter asked for.</summary>
    Requested,

    /// <summary>Clear the whole pattern.</summary>
    All,
}

/// <summary>
/// A 64-bit pattern of bits several threads wait on and one or more set. Unlike a condition variable it
/// keeps state, so a thread that arrives after the bits were set is satisfied at once rather than
/// missing the signal. Use it to let one thread wait for several unrelated things to finish - one bit
/// each - without a lock around them.
/// </summary>
/// <remarks>
/// A wait can also clear what satisfied it, which turns a bit into a one-shot token: see
/// <see cref="EventFlagClear"/>. Waiting threads are released in arrival order unless the flag was
/// created with <c>priorityOrder</c>.
/// </remarks>
public sealed unsafe class EventFlag : IDisposable
{
    private nint _handle;

    /// <summary>
    /// Creates an event flag whose pattern starts at <paramref name="initialPattern"/>.
    /// </summary>
    /// <param name="name">A name a memory report and a debugger show it by.</param>
    /// <param name="initialPattern">The bits set to begin with.</param>
    /// <param name="singleWaiter">
    /// True when only one thread will ever wait on it, which is cheaper. A second waiter on a
    /// single-waiter flag is refused.
    /// </param>
    /// <param name="priorityOrder">
    /// True to release the most urgent waiting thread first rather than the one that waited longest.
    /// </param>
    /// <exception cref="ProsperoException">The platform refused to create the flag.</exception>
    public EventFlag(string name, ulong initialPattern = 0, bool singleWaiter = false, bool priorityOrder = false)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        uint attributes =
            (priorityOrder ? KernelEventFlags.AttrThreadPriority : KernelEventFlags.AttrThreadFifo) |
            (singleWaiter ? KernelEventFlags.AttrSingle : KernelEventFlags.AttrMulti);

        byte[] owned = ToNullTerminated(name);
        nint handle = 0;
        int rc;
        fixed (byte* p = owned)
            rc = KernelEventFlags.sceKernelCreateEventFlag(&handle, p, attributes, initialPattern, null);
        SceResult.ThrowIfFailed(rc, nameof(KernelEventFlags.sceKernelCreateEventFlag));
        _handle = handle;
        Name = name;
    }

    /// <summary>The name the flag was created with.</summary>
    public string Name { get; }

    /// <summary>Sets every bit of <paramref name="bits"/>, releasing whatever waiters that satisfies.</summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public void Set(ulong bits)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEventFlags.sceKernelSetEventFlag(_handle, bits), nameof(KernelEventFlags.sceKernelSetEventFlag));
    }

    /// <summary>Clears every bit of <paramref name="bits"/>.</summary>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public void Clear(ulong bits)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEventFlags.sceKernelClearEventFlag(_handle, bits), nameof(KernelEventFlags.sceKernelClearEventFlag));
    }

    /// <summary>
    /// Blocks until <paramref name="bits"/> is satisfied and returns the pattern that satisfied it.
    /// </summary>
    /// <param name="bits">The bits to wait for; must not be zero.</param>
    /// <param name="mode">Whether every bit is needed or any one of them.</param>
    /// <param name="clear">What to do to the pattern once the wait is satisfied.</param>
    /// <param name="timeout">How long to wait, or null to wait forever.</param>
    /// <exception cref="ProsperoException">The wait failed or timed out.</exception>
    public ulong Wait(ulong bits, EventFlagWait mode = EventFlagWait.All,
        EventFlagClear clear = EventFlagClear.None, TimeSpan? timeout = null)
    {
        int rc = TryWait(bits, out ulong pattern, mode, clear, timeout);
        SceResult.ThrowIfFailed(rc, nameof(KernelEventFlags.sceKernelWaitEventFlag));
        return pattern;
    }

    /// <summary>
    /// Blocks as <see cref="Wait"/> does but reports the outcome rather than throwing, so a timeout can
    /// be handled without an exception.
    /// </summary>
    /// <returns>Zero when the wait was satisfied, or the platform's negative code.</returns>
    public int TryWait(ulong bits, out ulong pattern, EventFlagWait mode = EventFlagWait.All,
        EventFlagClear clear = EventFlagClear.None, TimeSpan? timeout = null)
    {
        ThrowIfDisposed();
        ArgumentOutOfRangeException.ThrowIfZero(bits);
        ulong result = 0;
        uint micros = timeout is { } t ? WaitTimeout.ToMicroseconds(t) : 0;
        int rc = KernelEventFlags.sceKernelWaitEventFlag(
            _handle, bits, WaitMode(mode, clear), &result, timeout is null ? null : &micros);
        pattern = result;
        return rc;
    }

    /// <summary>
    /// Tests <paramref name="bits"/> without blocking, reporting the pattern in
    /// <paramref name="pattern"/>.
    /// </summary>
    /// <returns>True when the bits were already satisfied.</returns>
    public bool Poll(ulong bits, out ulong pattern, EventFlagWait mode = EventFlagWait.All,
        EventFlagClear clear = EventFlagClear.None)
    {
        ThrowIfDisposed();
        ArgumentOutOfRangeException.ThrowIfZero(bits);
        ulong result = 0;
        int rc = KernelEventFlags.sceKernelPollEventFlag(_handle, bits, WaitMode(mode, clear), &result);
        pattern = result;
        return rc >= 0;
    }

    /// <summary>
    /// Releases every waiting thread at once with a failure and resets the pattern to
    /// <paramref name="pattern"/>. Use it to unblock a subsystem that is shutting down.
    /// </summary>
    /// <returns>How many threads were waiting.</returns>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public int CancelWaiters(ulong pattern = 0)
    {
        ThrowIfDisposed();
        int waiting = 0;
        SceResult.ThrowIfFailed(
            KernelEventFlags.sceKernelCancelEventFlag(_handle, pattern, &waiting),
            nameof(KernelEventFlags.sceKernelCancelEventFlag));
        return waiting;
    }

    /// <summary>The wait-mode bits for a mode and a clear rule.</summary>
    public static uint WaitMode(EventFlagWait mode, EventFlagClear clear)
    {
        uint value = mode == EventFlagWait.Any ? KernelEventFlags.WaitModeOr : KernelEventFlags.WaitModeAnd;
        return value | clear switch
        {
            EventFlagClear.Requested => KernelEventFlags.WaitModeClearPattern,
            EventFlagClear.All => KernelEventFlags.WaitModeClearAll,
            _ => 0u,
        };
    }

    /// <summary>Destroys the flag, releasing any thread still waiting on it with a failure.</summary>
    public void Dispose()
    {
        if (_handle == 0)
            return;
        KernelEventFlags.sceKernelDeleteEventFlag(_handle);
        _handle = 0;
        GC.SuppressFinalize(this);
    }

    /// <summary>Destroys the flag if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~EventFlag() => Dispose();

    private void ThrowIfDisposed() => ObjectDisposedException.ThrowIf(_handle == 0, this);

    private static byte[] ToNullTerminated(string value)
    {
        byte[] buffer = new byte[Encoding.UTF8.GetByteCount(value) + 1];
        Encoding.UTF8.GetBytes(value, buffer);
        return buffer;
    }
}
