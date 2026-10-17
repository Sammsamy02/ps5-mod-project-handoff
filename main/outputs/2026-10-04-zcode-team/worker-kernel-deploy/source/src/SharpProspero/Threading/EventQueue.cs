// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Threading;

/// <summary>What produced a report taken out of an <see cref="EventQueue"/>.</summary>
public enum EventSource
{
    /// <summary>A filter this SDK does not name. Read <see cref="QueuedEvent.RawFilter"/> for the number.</summary>
    Other = 0,

    /// <summary>A descriptor has data to read.</summary>
    Readable,

    /// <summary>A descriptor can take more data.</summary>
    Writable,

    /// <summary>A watched file changed.</summary>
    FileChanged,

    /// <summary>A timer elapsed.</summary>
    Timer,

    /// <summary>The application raised the event itself.</summary>
    User,

    /// <summary>The display side reported.</summary>
    VideoOut,

    /// <summary>The graphics core reported.</summary>
    GraphicsCore,
}

/// <summary>One report taken out of an <see cref="EventQueue"/>, decoded.</summary>
public readonly struct QueuedEvent
{
    /// <summary>What produced the report.</summary>
    public EventSource Source { get; init; }

    /// <summary>The filter number as the platform reported it, for a source this SDK does not name.</summary>
    public short RawFilter { get; init; }

    /// <summary>
    /// The descriptor for a file source, or the identifier the add call was given for a timer or a
    /// user source.
    /// </summary>
    public nuint Identifier { get; init; }

    /// <summary>
    /// The payload: readable or writable bytes for a descriptor source, and the error number when
    /// <see cref="IsError"/> is set.
    /// </summary>
    public nint Data { get; init; }

    /// <summary>The filter's own bits. For a file source this is which of the changes fired.</summary>
    public uint FilterFlags { get; init; }

    /// <summary>The source has reached its end and will report nothing further.</summary>
    public bool IsEndOfFile { get; init; }

    /// <summary>The report carries an error, and <see cref="Data"/> is its number.</summary>
    public bool IsError { get; init; }

    /// <summary>Decodes one platform report.</summary>
    public static QueuedEvent From(SceKernelEvent ev) => new()
    {
        Source = FromFilter(ev.Filter),
        RawFilter = ev.Filter,
        Identifier = ev.Identifier,
        Data = ev.Data,
        FilterFlags = ev.FilterFlags,
        IsEndOfFile = (ev.Flags & KernelEqueue.FlagEndOfFile) != 0,
        IsError = (ev.Flags & KernelEqueue.FlagError) != 0,
    };

    /// <summary>The source a filter number names.</summary>
    public static EventSource FromFilter(short filter) => filter switch
    {
        KernelEqueue.FilterRead => EventSource.Readable,
        KernelEqueue.FilterWrite => EventSource.Writable,
        KernelEqueue.FilterFile => EventSource.FileChanged,
        KernelEqueue.FilterTimer or KernelEqueue.FilterHighResolutionTimer => EventSource.Timer,
        KernelEqueue.FilterUser => EventSource.User,
        KernelEqueue.FilterVideoOut => EventSource.VideoOut,
        KernelEqueue.FilterGraphicsCore => EventSource.GraphicsCore,
        _ => EventSource.Other,
    };
}

/// <summary>
/// One place to wait for several unrelated things at once: timers, a descriptor becoming readable or
/// writable, a file changing on disk, and events the application raises itself. A service thread that
/// would otherwise poll each source in turn blocks here instead and wakes only when something happens.
/// </summary>
/// <remarks>
/// A source is added under an identifier the caller chooses, and each report names the identifier it
/// came from, so one queue can carry many timers and many user events without confusion. Every add and
/// remove call may be made from any thread, including while another thread is blocked in
/// <see cref="Wait"/>.
/// </remarks>
public sealed unsafe class EventQueue : IDisposable
{
    private nint _handle;

    /// <summary>Creates an empty queue.</summary>
    /// <param name="name">A name a debugger and a memory report show it by.</param>
    /// <exception cref="ProsperoException">The platform refused to create the queue.</exception>
    public EventQueue(string name)
    {
        ArgumentException.ThrowIfNullOrEmpty(name);
        byte[] owned = ToNullTerminated(name);
        nint handle = 0;
        int rc;
        fixed (byte* p = owned)
            rc = KernelEqueue.sceKernelCreateEqueue(&handle, p);
        SceResult.ThrowIfFailed(rc, nameof(KernelEqueue.sceKernelCreateEqueue));
        _handle = handle;
        Name = name;
    }

    /// <summary>The name the queue was created with.</summary>
    public string Name { get; }

    /// <summary>The raw handle, for a call this wrapper does not cover.</summary>
    public nint Handle => _handle;

    /// <summary>
    /// Adds a timer that reports under <paramref name="id"/> every <paramref name="period"/>.
    /// </summary>
    /// <exception cref="ProsperoException">The platform refused the timer.</exception>
    public void AddTimer(int id, TimeSpan period)
    {
        ThrowIfDisposed();
        uint micros = WaitTimeout.ToMicroseconds(period);
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelAddTimerEvent(_handle, id, micros, null),
            nameof(KernelEqueue.sceKernelAddTimerEvent));
    }

    /// <summary>Removes the timer added under <paramref name="id"/>.</summary>
    /// <exception cref="ProsperoException">No such timer.</exception>
    public void RemoveTimer(int id)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelDeleteTimerEvent(_handle, id), nameof(KernelEqueue.sceKernelDeleteTimerEvent));
    }

    /// <summary>
    /// Reports when <paramref name="descriptor"/> has at least <paramref name="minimumBytes"/> to read.
    /// The report names the descriptor as its identifier.
    /// </summary>
    /// <exception cref="ProsperoException">The platform refused the source.</exception>
    public void AddReadable(int descriptor, nuint minimumBytes = 1)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelAddReadEvent(_handle, descriptor, minimumBytes, null),
            nameof(KernelEqueue.sceKernelAddReadEvent));
    }

    /// <summary>Stops reporting readability for <paramref name="descriptor"/>.</summary>
    /// <exception cref="ProsperoException">No such source.</exception>
    public void RemoveReadable(int descriptor)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelDeleteReadEvent(_handle, descriptor), nameof(KernelEqueue.sceKernelDeleteReadEvent));
    }

    /// <summary>Reports when <paramref name="descriptor"/> can take at least <paramref name="minimumBytes"/>.</summary>
    /// <exception cref="ProsperoException">The platform refused the source.</exception>
    public void AddWritable(int descriptor, nuint minimumBytes = 1)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelAddWriteEvent(_handle, descriptor, minimumBytes, null),
            nameof(KernelEqueue.sceKernelAddWriteEvent));
    }

    /// <summary>Stops reporting writability for <paramref name="descriptor"/>.</summary>
    /// <exception cref="ProsperoException">No such source.</exception>
    public void RemoveWritable(int descriptor)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelDeleteWriteEvent(_handle, descriptor), nameof(KernelEqueue.sceKernelDeleteWriteEvent));
    }

    /// <summary>
    /// Watches the file behind <paramref name="descriptor"/> for the changes named by
    /// <paramref name="watch"/>, a combination of the <c>Note*</c> values on <see cref="KernelEqueue"/>.
    /// </summary>
    /// <exception cref="ProsperoException">The platform refused the watch.</exception>
    public void AddFileWatch(int descriptor, uint watch = KernelEqueue.NoteAll)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelAddFileEvent(_handle, descriptor, (int)watch, null),
            nameof(KernelEqueue.sceKernelAddFileEvent));
    }

    /// <summary>Stops watching the file behind <paramref name="descriptor"/>.</summary>
    /// <exception cref="ProsperoException">No such watch.</exception>
    public void RemoveFileWatch(int descriptor)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelDeleteFileEvent(_handle, descriptor), nameof(KernelEqueue.sceKernelDeleteFileEvent));
    }

    /// <summary>
    /// Adds a source the application raises itself under <paramref name="id"/>. A level source stays
    /// raised until it is reported and reported again on every wait; a one-shot source clears itself as
    /// soon as it is reported.
    /// </summary>
    /// <exception cref="ProsperoException">The platform refused the source.</exception>
    public void AddUserEvent(int id, bool oneShot = false)
    {
        ThrowIfDisposed();
        int rc = oneShot
            ? KernelEqueue.sceKernelAddUserEventEdge(_handle, id)
            : KernelEqueue.sceKernelAddUserEvent(_handle, id);
        SceResult.ThrowIfFailed(rc, nameof(KernelEqueue.sceKernelAddUserEvent));
    }

    /// <summary>Removes the source added under <paramref name="id"/>.</summary>
    /// <exception cref="ProsperoException">No such source.</exception>
    public void RemoveUserEvent(int id)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelDeleteUserEvent(_handle, id), nameof(KernelEqueue.sceKernelDeleteUserEvent));
    }

    /// <summary>
    /// Raises the source added under <paramref name="id"/>, waking whatever thread is waiting. Safe to
    /// call from any thread, which makes it the way to break another thread out of <see cref="Wait"/>.
    /// </summary>
    /// <exception cref="ProsperoException">No such source.</exception>
    public void TriggerUserEvent(int id)
    {
        ThrowIfDisposed();
        SceResult.ThrowIfFailed(
            KernelEqueue.sceKernelTriggerUserEvent(_handle, id, null), nameof(KernelEqueue.sceKernelTriggerUserEvent));
    }

    /// <summary>
    /// Blocks until at least one report arrives and writes the reports into <paramref name="reports"/>.
    /// </summary>
    /// <param name="reports">Where the decoded reports are written; its length caps how many are taken.</param>
    /// <param name="timeout">How long to wait, or null to wait forever.</param>
    /// <returns>How many reports were written, which is zero when the wait timed out.</returns>
    /// <exception cref="ProsperoException">The wait failed for a reason other than a timeout.</exception>
    public int Wait(Span<QueuedEvent> reports, TimeSpan? timeout = null)
    {
        ThrowIfDisposed();
        if (reports.Length == 0)
            return 0;

        Span<SceKernelEvent> raw = reports.Length <= 16
            ? stackalloc SceKernelEvent[reports.Length]
            : new SceKernelEvent[reports.Length];

        int received = 0;
        uint micros = timeout is { } t ? WaitTimeout.ToMicroseconds(t) : 0;
        int rc;
        fixed (SceKernelEvent* p = raw)
            rc = KernelEqueue.sceKernelWaitEqueue(
                _handle, p, reports.Length, &received, timeout is null ? null : &micros);

        // A wait that runs out of time reports a failure with nothing received; that is the ordinary
        // outcome of a bounded wait rather than something the caller has to handle as an error.
        if (rc < 0 && received == 0)
        {
            if (timeout is not null)
                return 0;
            SceResult.ThrowIfFailed(rc, nameof(KernelEqueue.sceKernelWaitEqueue));
        }

        for (int i = 0; i < received; i++)
            reports[i] = QueuedEvent.From(raw[i]);
        return received;
    }

    /// <summary>Destroys the queue and every source still attached to it.</summary>
    public void Dispose()
    {
        if (_handle == 0)
            return;
        KernelEqueue.sceKernelDeleteEqueue(_handle);
        _handle = 0;
        GC.SuppressFinalize(this);
    }

    /// <summary>Destroys the queue if it was dropped without a <see cref="Dispose"/> call.</summary>
    ~EventQueue() => Dispose();

    private void ThrowIfDisposed() => ObjectDisposedException.ThrowIf(_handle == 0, this);

    private static byte[] ToNullTerminated(string value)
    {
        byte[] buffer = new byte[Encoding.UTF8.GetByteCount(value) + 1];
        Encoding.UTF8.GetBytes(value, buffer);
        return buffer;
    }
}
