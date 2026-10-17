// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Application;

/// <summary>
/// A small in-process message bus that lets parts of an application talk without holding references to
/// each other. A subscriber asks for a message type; a publisher sends one and every subscriber for that
/// type receives it. The message type is the channel, so a <c>ScoreChanged</c> record and a
/// <c>PlayerDied</c> record are delivered independently.
/// </summary>
/// <remarks>
/// Delivery is synchronous on the calling thread, in subscription order. A subscriber may subscribe or
/// unsubscribe while a message is being delivered without disturbing the one in flight. An exception a
/// handler throws propagates to the publisher and stops the remaining handlers for that message, so keep
/// handlers from throwing on the normal path.
/// </remarks>
/// <example>
/// <code>
/// var events = new EventHub();
/// using IDisposable subscription = events.Subscribe&lt;ScoreChanged&gt;(m =&gt; _hud.SetScore(m.Total));
/// events.Publish(new ScoreChanged(1200));
/// </code>
/// </example>
public sealed class EventHub
{
    private readonly Dictionary<Type, List<Subscription>> _handlers = [];

    /// <summary>
    /// Registers <paramref name="handler"/> for messages of type <typeparamref name="T"/>. Dispose the
    /// returned token to stop receiving them; disposing it more than once is harmless.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="handler"/> is null.</exception>
    public IDisposable Subscribe<T>(Action<T> handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        if (!_handlers.TryGetValue(typeof(T), out List<Subscription>? list))
        {
            list = [];
            _handlers[typeof(T)] = list;
        }

        var subscription = new Subscription(this, typeof(T), handler);
        list.Add(subscription);
        return subscription;
    }

    /// <summary>Delivers <paramref name="message"/> to every subscriber for its type.</summary>
    public void Publish<T>(T message)
    {
        if (!_handlers.TryGetValue(typeof(T), out List<Subscription>? list) || list.Count == 0)
            return;

        // Deliver against a snapshot so a handler may subscribe or unsubscribe during delivery.
        Subscription[] snapshot = [.. list];
        foreach (Subscription subscription in snapshot)
            ((Action<T>)subscription.Handler)(message);
    }

    /// <summary>How many subscribers are registered for message type <typeparamref name="T"/>.</summary>
    public int SubscriberCount<T>() => _handlers.TryGetValue(typeof(T), out List<Subscription>? list) ? list.Count : 0;

    /// <summary>Removes every subscription.</summary>
    public void Clear() => _handlers.Clear();

    private void Remove(Subscription subscription)
    {
        // Remove by identity, not by handler equality, so disposing a token can only ever remove its own
        // registration — never a later re-subscription of the same handler.
        if (_handlers.TryGetValue(subscription.Type, out List<Subscription>? list)
            && list.Remove(subscription)
            && list.Count == 0)
        {
            _handlers.Remove(subscription.Type);
        }
    }

    private sealed class Subscription(EventHub hub, Type type, Delegate handler) : IDisposable
    {
        private EventHub? _hub = hub;

        public Type Type { get; } = type;

        public Delegate Handler { get; } = handler;

        public void Dispose()
        {
            _hub?.Remove(this);
            _hub = null;
        }
    }
}
