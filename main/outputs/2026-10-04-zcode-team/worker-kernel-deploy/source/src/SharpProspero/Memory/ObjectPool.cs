// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Memory;

/// <summary>
/// Keeps a set of reusable objects so a hot loop can borrow one instead of allocating, which matters on a
/// heap the garbage collector shares with the whole module. <see cref="Rent"/> hands out an idle object
/// (or makes one when none is idle); <see cref="Return"/> gives it back for the next borrower. A returned
/// object is kept up to a retained limit and dropped past it, so a burst does not grow the pool without
/// bound.
/// </summary>
/// <typeparam name="T">The pooled reference type, such as a buffer, a list, or a game entity.</typeparam>
/// <example>
/// <code>
/// var pool = new ObjectPool&lt;List&lt;int&gt;&gt;(() =&gt; new List&lt;int&gt;(), onReturn: l =&gt; l.Clear());
/// List&lt;int&gt; scratch = pool.Rent();
/// // ... use scratch ...
/// pool.Return(scratch);
/// </code>
/// </example>
public sealed class ObjectPool<T> where T : class
{
    private readonly Stack<T> _idle = new();
    private readonly Func<T> _factory;
    private readonly Action<T>? _onRent;
    private readonly Action<T>? _onReturn;
    private readonly int _maxRetained;

    /// <summary>Creates a pool.</summary>
    /// <param name="factory">Makes a new object when none is idle. Required.</param>
    /// <param name="onRent">Runs on an object as it is handed out, to prepare it. Optional.</param>
    /// <param name="onReturn">Runs on an object as it comes back, to reset it. Optional.</param>
    /// <param name="maxRetained">The most idle objects to keep; extras returned past this are dropped.</param>
    /// <param name="prewarm">How many objects to make up front, capped at <paramref name="maxRetained"/>.</param>
    /// <exception cref="ArgumentNullException"><paramref name="factory"/> is null.</exception>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="maxRetained"/> is not positive, or <paramref name="prewarm"/> is negative.</exception>
    public ObjectPool(Func<T> factory, Action<T>? onRent = null, Action<T>? onReturn = null, int maxRetained = 1024, int prewarm = 0)
    {
        ArgumentNullException.ThrowIfNull(factory);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxRetained);
        ArgumentOutOfRangeException.ThrowIfNegative(prewarm);
        _factory = factory;
        _onRent = onRent;
        _onReturn = onReturn;
        _maxRetained = maxRetained;

        for (int i = 0; i < prewarm && _idle.Count < _maxRetained; i++)
            _idle.Push(factory());
    }

    /// <summary>How many objects are idle and ready to hand out without allocating.</summary>
    public int IdleCount => _idle.Count;

    /// <summary>Hands out an idle object, or makes one when none is idle.</summary>
    public T Rent()
    {
        T item = _idle.Count > 0 ? _idle.Pop() : _factory();
        _onRent?.Invoke(item);
        return item;
    }

    /// <summary>
    /// Returns <paramref name="item"/> to the pool. Return each borrowed object once; returning one twice,
    /// or holding a reference after returning it, lets two callers share the same object.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="item"/> is null.</exception>
    public void Return(T item)
    {
        ArgumentNullException.ThrowIfNull(item);
        _onReturn?.Invoke(item);
        if (_idle.Count < _maxRetained)
            _idle.Push(item);
    }

    /// <summary>Drops every idle object. Objects currently rented out are unaffected.</summary>
    public void Clear() => _idle.Clear();
}
