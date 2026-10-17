// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Runtime.ExceptionServices;
using System.Threading;

namespace SharpProspero.Threading;

/// <summary>
/// Runs one piece of work on a background thread and lets the frame loop check whether it has finished,
/// without waiting. Start it, then poll <see cref="IsComplete"/> each frame and act when it is done —
/// a save written, a level built — so the screen never freezes on slow work. An exception the work throws
/// is caught and reported through <see cref="Failed"/> and <see cref="Error"/>.
/// </summary>
public sealed class BackgroundOperation
{
    private readonly Thread _thread;
    private volatile bool _complete;
    private Exception? _error;

    /// <summary>Starts <paramref name="work"/> on a background thread at once.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="work"/> is null.</exception>
    public BackgroundOperation(Action work, string? name = null)
    {
        ArgumentNullException.ThrowIfNull(work);
        _thread = new Thread(() =>
        {
            try
            {
                work();
            }
            catch (Exception e)
            {
                _error = e;
            }
            finally
            {
                _complete = true;
            }
        })
        {
            IsBackground = true,
            Name = name,
        };
        _thread.Start();
    }

    /// <summary>Whether the work has finished (whether it succeeded or threw).</summary>
    public bool IsComplete => _complete;

    /// <summary>Whether the work finished by throwing.</summary>
    public bool Failed => _error is not null;

    /// <summary>The exception the work threw, or null.</summary>
    public Exception? Error => _error;

    /// <summary>Waits for the work to finish.</summary>
    public void Wait() => _thread.Join();

    /// <summary>Waits up to <paramref name="timeout"/> for the work to finish; returns whether it did.</summary>
    public bool Wait(TimeSpan timeout) => _thread.Join(timeout);
}

/// <summary>
/// Runs one piece of work that produces a result on a background thread — load and decode a file, build
/// something to show — and lets the frame loop pick up the result when it is ready. Poll
/// <see cref="IsComplete"/>, then read <see cref="Result"/>; reading the result before it is ready waits
/// for it, and if the work threw, reading the result throws that same exception.
/// </summary>
/// <typeparam name="T">The type of result the work produces.</typeparam>
public sealed class BackgroundOperation<T>
{
    private readonly Thread _thread;
    private volatile bool _complete;
    private T? _result;
    private Exception? _error;

    /// <summary>Starts <paramref name="work"/> on a background thread at once.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="work"/> is null.</exception>
    public BackgroundOperation(Func<T> work, string? name = null)
    {
        ArgumentNullException.ThrowIfNull(work);
        _thread = new Thread(() =>
        {
            try
            {
                _result = work();
            }
            catch (Exception e)
            {
                _error = e;
            }
            finally
            {
                _complete = true;
            }
        })
        {
            IsBackground = true,
            Name = name,
        };
        _thread.Start();
    }

    /// <summary>Whether the work has finished (whether it succeeded or threw).</summary>
    public bool IsComplete => _complete;

    /// <summary>Whether the work finished by throwing.</summary>
    public bool Failed => _error is not null;

    /// <summary>The exception the work threw, or null.</summary>
    public Exception? Error => _error;

    /// <summary>The result, waiting for the work if it has not finished. Rethrows the exception the work threw.</summary>
    public T Result
    {
        get
        {
            _thread.Join();
            if (_error is not null)
                ExceptionDispatchInfo.Throw(_error);
            return _result!;
        }
    }

    /// <summary>Waits for the work to finish.</summary>
    public void Wait() => _thread.Join();

    /// <summary>Waits up to <paramref name="timeout"/> for the work to finish; returns whether it did.</summary>
    public bool Wait(TimeSpan timeout) => _thread.Join(timeout);
}
