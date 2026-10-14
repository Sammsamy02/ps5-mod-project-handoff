// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Threading;

/// <summary>
/// A counting semaphore held by the platform rather than by the runtime. A waiter may take more than
/// one unit at a time, which suits a pool of interchangeable resources - a set of decode buffers, a
/// number of streaming slots - where a job needs several of them before it can start.
/// </summary>
/// <remarks>
/// The platform's own semaphore is the one its scheduler knows about, so a thread blocked here is
/// visible to the system's thread views and can be released in priority order. Prefer a
/// <see cref="System.Threading.SemaphoreSlim"/> for short waits that never cross into platform code.
/// </remarks>
public sealed unsafe class CountingSemaphore : IDisposable
{
    private nint _handle;

    /// <summary>Creates a semaphore holding <paramref name="initialCount"/> of <paramref name="maximumCount"/>.</summary>
    /// <param name="name">A name a memory report and a debugger show it by.</param>
    /// <param name="initialCount">The count it starts with.</param>
    /// <param name="maximumCount">The ceiling the count may reach.</param>
    /// <param name="priorityOrder">
    /// True to release the most urgent waiting thread first rather than the one that waited longest.
    /// </param>
    /// <exception cref="ProsperoException">The platform refused to create the semaphore.</exception>
    public CountingSemaphore(string name, int initialCount, int maximumCount, bool priorityOrder = false)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        ArgumentOutOfRangeException.ThrowIfNegative(initialCount);
        ArgumentOutOfRangeException.ThrowIfLessThan(maximumCount, initialCount);

        uint attributes = priorityOrder ? KernelSemaphores.AttrThreadPriority : KernelSemaphores.AttrThreadFifo;
        byte[] owned = ToNullTerminated(name);
        nint handle = 0;
        int rc;
        fixed (byte* p = owned)
            rc = KernelSemaphores.sceKernelCreateSema(&handle, p, attributes, initialCount, maximumCount, null);
        SceResult.ThrowIfFailed(rc, nameof(KernelSemaphores.sceKernelCreateSema));
        _handle = handle;
        Name = name;
        MaximumCount = maximumCount;
    }

    /// <summary>The name the semaphore was created with.</summary>
    public string Name { get; }

    /// <summary>The ceiling the count may reach.</summary>
    public int MaximumCount { get; }

    /// <summary>Takes <paramref name="count"/> from the semaphore, blocking while there is less.</summary>
    /// <param name="count">How many units to take.</param>
    /// <param name="timeout">How long to wait, or null to wait forever.</param>
    /// <exception cref="ProsperoException">The wait failed or timed out.</exception>
    public void Wait(int count = 1, TimeSpan? timeout = null)
        => SceResult.ThrowIfFailed(TryWait(count, timeout), nameof(KernelSemaphores.sceKernelWaitSema));

    /// <summary>
    /// Takes <paramref name="count"/> as <see cref="Wait"/> does but reports the outcome rather than
    /// throwing, so a timeout can be handled without an exception.
    /// </summary>
    /// <returns>Zero when the units were taken, or the platform's negative code.</returns>
    public int TryWait(int count = 1, TimeSpan? timeout = null)
    {
        ThrowIfDisposed();
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
        uint micros = timeout is { } t ? WaitTimeout.ToMicroseconds(t) : 0;
        return KernelSemaphores.sceKernelWaitSema(_handle, count, timeout is null ? null : &micros);
    }

    /// <summary>Takes <paramref name="count"/> only if that much is there, without blocking.</summary>
    /// <returns>True when the units were taken.</returns>
    public bool TryTake(int count = 1)
    {
        ThrowIfDisposed();
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
        return KernelSemaphores.sceKernelPollSema(_handle, count) >= 0;
    }

    /// <summary>Returns <paramref name="count"/> to the semaphore, releasing whatever waiters that satisfies.</summary>
    /// <exception cref="ProsperoException">The call failed, which includes exceeding the ceiling.</exception>
    public void Release(int count = 1)
    {
        ThrowIfDisposed();
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
        SceResult.ThrowIfFailed(
            KernelSemaphores.sceKernelSignalSema(_handle, count), nameof(KernelSemaphores.sceKernelSignalSema));
    }

    /// <summary>
    /// Releases every waiting thread at once with a failure and resets the count to
    /// <paramref name="count"/>. Use it to unblock a subsystem that is shutting down.
    /// </summary>
    /// <returns>How many threads were waiting.</returns>
    /// <exception cref="ProsperoException">The call failed.</exception>
    public int CancelWaiters(int count = 0)
    {
        ThrowIfDisposed();
        int waiting = 0;
        SceResult.ThrowIfFailed(
            KernelSemaphores.sceKernelCancelSema(_handle, count, &waiting), nameof(KernelSemaphores.sceKernelCancelSema));
        return waiting;
    }

    /// <summary>Destroys the semaphore, releasing any thread still waiting on it with a failure.</summary>
    public void Dispose()
    {
        if (_handle == 0)
            return;
        KernelSemaphores.sceKernelDeleteSema(_handle);
        _handle = 0;
        GC.SuppressFinalize(this);
    }

    /// <summary>Destroys the semaphore if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~CountingSemaphore() => Dispose();

    private void ThrowIfDisposed() => ObjectDisposedException.ThrowIf(_handle == 0, this);

    private static byte[] ToNullTerminated(string value)
    {
        byte[] buffer = new byte[Encoding.UTF8.GetByteCount(value) + 1];
        Encoding.UTF8.GetBytes(value, buffer);
        return buffer;
    }
}
