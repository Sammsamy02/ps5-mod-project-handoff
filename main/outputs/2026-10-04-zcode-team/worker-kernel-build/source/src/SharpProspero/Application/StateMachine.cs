// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Application;

/// <summary>
/// Runs an application or game as a set of named states, one active at a time, each with optional work to
/// do on entry, on every frame, and on exit. Configure the states, <see cref="Start"/> in one, call
/// <see cref="Update"/> each frame, and <see cref="TransitionTo"/> another when something happens — a menu
/// to a level, a level to a pause screen, a request to its result. It keeps the enter and exit work paired
/// so a state always cleans up after itself.
/// </summary>
/// <typeparam name="TState">The state identifier, usually an enum.</typeparam>
/// <example>
/// <code>
/// var game = new StateMachine&lt;Screen&gt;()
///     .Configure(Screen.Menu, onUpdate: dt =&gt; { if (start) game.TransitionTo(Screen.Play); })
///     .Configure(Screen.Play, onEnter: LoadLevel, onUpdate: Step, onExit: UnloadLevel);
/// game.Start(Screen.Menu);
/// // each frame:
/// game.Update(context.DeltaSeconds);
/// </code>
/// </example>
public sealed class StateMachine<TState> where TState : notnull
{
    private sealed class Behavior
    {
        public Action? Enter;
        public Action<double>? Update;
        public Action? Exit;
    }

    private readonly Dictionary<TState, Behavior> _states = [];
    private TState? _current;
    private bool _started;
    private bool _transitioning;

    /// <summary>Raised after a transition, with the state left and the state entered.</summary>
    public event Action<TState, TState>? Transitioned;

    /// <summary>Whether <see cref="Start"/> has been called.</summary>
    public bool IsRunning => _started;

    /// <summary>The active state.</summary>
    /// <exception cref="InvalidOperationException">The machine has not started.</exception>
    public TState Current => _started ? _current! : throw new InvalidOperationException("The state machine has not started.");

    /// <summary>
    /// Registers <paramref name="state"/> and its optional enter, per-frame, and exit callbacks. Call it
    /// once per state before <see cref="Start"/>; registering the same state again replaces its callbacks.
    /// Returns this machine so calls chain.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="state"/> is null.</exception>
    public StateMachine<TState> Configure(TState state, Action? onEnter = null, Action<double>? onUpdate = null, Action? onExit = null)
    {
        ArgumentNullException.ThrowIfNull(state);
        _states[state] = new Behavior { Enter = onEnter, Update = onUpdate, Exit = onExit };
        return this;
    }

    /// <summary>Enters <paramref name="initial"/> and runs its enter callback.</summary>
    /// <exception cref="InvalidOperationException">The machine has already started.</exception>
    /// <exception cref="ArgumentException"><paramref name="initial"/> was not configured.</exception>
    public void Start(TState initial)
    {
        if (_started)
            throw new InvalidOperationException("The state machine has already started.");
        if (!_states.TryGetValue(initial, out Behavior? behavior))
            throw new ArgumentException($"No state '{initial}' was configured.", nameof(initial));

        _started = true;
        _current = initial;
        behavior.Enter?.Invoke();
    }

    /// <summary>Runs the active state's per-frame callback. Does nothing before <see cref="Start"/>.</summary>
    public void Update(double deltaSeconds)
    {
        if (_started)
            _states[_current!].Update?.Invoke(deltaSeconds);
    }

    /// <summary>
    /// Leaves the active state and enters <paramref name="next"/>: the current state's exit callback runs,
    /// then <see cref="Transitioned"/> is raised, then the new state's enter callback runs. Transitioning
    /// to the active state does nothing. Call it from a per-frame callback, not from an exit, enter, or
    /// <see cref="Transitioned"/> callback — starting a transition from inside one would leave the machine
    /// half-moved, so it is refused.
    /// </summary>
    /// <exception cref="InvalidOperationException">The machine has not started, or a transition is already in progress.</exception>
    /// <exception cref="ArgumentException"><paramref name="next"/> was not configured.</exception>
    public void TransitionTo(TState next)
    {
        if (!_started)
            throw new InvalidOperationException("The state machine has not started.");
        if (_transitioning)
            throw new InvalidOperationException("Cannot start a transition from inside an exit, enter, or Transitioned callback.");
        if (!_states.TryGetValue(next, out Behavior? target))
            throw new ArgumentException($"No state '{next}' was configured.", nameof(next));
        if (EqualityComparer<TState>.Default.Equals(_current!, next))
            return;

        _transitioning = true;
        try
        {
            TState previous = _current!;
            _states[previous].Exit?.Invoke();
            _current = next;
            Transitioned?.Invoke(previous, next);
            target.Enter?.Invoke();
        }
        finally
        {
            _transitioning = false;
        }
    }
}
