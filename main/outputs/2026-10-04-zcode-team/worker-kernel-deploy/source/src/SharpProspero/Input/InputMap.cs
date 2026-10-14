// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Pad;
using System;
using System.Collections.Generic;

namespace SharpProspero.Input;

/// <summary>
/// Maps controller buttons to named actions, so game code asks "did the player jump?" instead of "is
/// cross pressed?". Bind an action to one or more buttons, feed it this frame's controller sample, then
/// ask whether the action was just pressed, is held, or was just released. This keeps the buttons in one
/// place, ready to rebind, and reads clearly at the call site.
/// </summary>
/// <remarks>
/// A binding can be a single button, a chord (several buttons combined with <c>|</c>, all of which must
/// be held), or several alternatives (bind the same action more than once). Call <see cref="Update"/>
/// once per frame with the latest controller sample before the queries; it keeps the previous sample
/// itself so it can tell a press from a hold.
/// </remarks>
/// <example>
/// <code>
/// var input = new InputMap()
///     .Bind("Jump", ScePadButton.Cross)
///     .Bind("Fire", ScePadButton.R2)
///     .Bind("Pause", ScePadButton.Options);
///
/// // each frame:
/// input.Update(context.Input);
/// if (input.WasPressed("Jump")) Jump();
/// if (input.IsHeld("Fire")) Fire();
/// </code>
/// </example>
public sealed class InputMap
{
    private readonly Dictionary<string, List<ScePadButton>> _bindings = new(StringComparer.Ordinal);
    private GamePadState _current = GamePadState.Neutral;
    private GamePadState _previous = GamePadState.Neutral;

    /// <summary>
    /// Binds <paramref name="action"/> to <paramref name="button"/> (which may combine several buttons
    /// with <c>|</c> for a chord). Binding an action more than once adds an alternative that also triggers
    /// it. Returns this map so calls can chain.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="action"/> is null or empty.</exception>
    public InputMap Bind(string action, ScePadButton button)
    {
        ArgumentException.ThrowIfNullOrEmpty(action);
        if (!_bindings.TryGetValue(action, out List<ScePadButton>? buttons))
        {
            buttons = [];
            _bindings[action] = buttons;
        }
        buttons.Add(button);
        return this;
    }

    /// <summary>Removes every binding for <paramref name="action"/>.</summary>
    public void Unbind(string action) => _bindings.Remove(action);

    /// <summary>Whether <paramref name="action"/> has at least one binding.</summary>
    public bool IsBound(string action) => _bindings.ContainsKey(action);

    /// <summary>Takes this frame's controller sample, keeping the previous one to tell a press from a hold.</summary>
    public void Update(GamePadState current)
    {
        _previous = _current;
        _current = current;
    }

    /// <summary>Whether <paramref name="action"/> is held this frame.</summary>
    public bool IsHeld(string action) => ActiveIn(action, _current);

    /// <summary>Whether <paramref name="action"/> became held this frame (a press, counted once).</summary>
    public bool WasPressed(string action) => ActiveIn(action, _current) && !ActiveIn(action, _previous);

    /// <summary>Whether <paramref name="action"/> stopped being held this frame (a release, counted once).</summary>
    public bool WasReleased(string action) => !ActiveIn(action, _current) && ActiveIn(action, _previous);

    // An action is active when any of its bindings is fully held (every button of a chord is down).
    private bool ActiveIn(string action, GamePadState state)
    {
        if (!_bindings.TryGetValue(action, out List<ScePadButton>? buttons))
            return false;
        foreach (ScePadButton button in buttons)
        {
            if (state.IsPressed(button))
                return true;
        }
        return false;
    }
}
