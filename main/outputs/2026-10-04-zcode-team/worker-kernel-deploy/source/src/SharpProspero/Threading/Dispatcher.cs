// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Concurrent;

namespace SharpProspero.Threading;

/// <summary>
/// A hand-off point back to one thread. Work posted from any thread is queued and run later on the
/// thread that calls <see cref="RunPending"/> — in an application that is the frame thread, which drains
/// it once per frame. Use it to apply the result of a background job to the drawing state, since a
/// drawing surface and most application state are not safe to touch from a worker thread.
/// </summary>
/// <example>
/// Load a file on a worker and show it on the frame thread:
/// <code>
/// var load = new BackgroundOperation&lt;PngImage&gt;(() =&gt; PngImage.Load(path));
/// // later, when load.IsComplete:
/// context.Dispatcher.Post(() =&gt; _texture = load.Result.AsSurface());
/// </code>
/// </example>
public sealed class Dispatcher
{
    private readonly ConcurrentQueue<Action> _queue = new();

    /// <summary>
    /// An optional handler for an exception thrown by posted work. When set, a throwing callback is
    /// reported here and the remaining queued work still runs. When null, the exception propagates out
    /// of <see cref="RunPending"/> so it is not swallowed; the work not yet run stays queued.
    /// </summary>
    public Action<Exception>? ErrorHandler { get; set; }

    /// <summary>The number of callbacks waiting to run.</summary>
    public int PendingCount => _queue.Count;

    /// <summary>
    /// Queues <paramref name="work"/> to run on the next <see cref="RunPending"/>. Safe to call from any
    /// thread. The callback runs later, not now.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="work"/> is null.</exception>
    public void Post(Action work)
    {
        ArgumentNullException.ThrowIfNull(work);
        _queue.Enqueue(work);
    }

    /// <summary>
    /// Runs the callbacks queued at the moment of the call, on the calling thread, and returns how many
    /// ran. Work a callback posts is left for the next call rather than run in the same pass, so a
    /// callback that re-posts itself cannot stall the loop.
    /// </summary>
    public int RunPending()
    {
        // Snapshot the count so callbacks that Post more work do not extend this pass.
        int budget = _queue.Count;
        int ran = 0;
        while (ran < budget && _queue.TryDequeue(out Action? work))
        {
            ran++;
            if (ErrorHandler is null)
            {
                work();
            }
            else
            {
                try { work(); }
                catch (Exception e) { ErrorHandler(e); }
            }
        }

        return ran;
    }

    /// <summary>Drops every queued callback without running it.</summary>
    public void Clear()
    {
        while (_queue.TryDequeue(out _))
        {
        }
    }
}
