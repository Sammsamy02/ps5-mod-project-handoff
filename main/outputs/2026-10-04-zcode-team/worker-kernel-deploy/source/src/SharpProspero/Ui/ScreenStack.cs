// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using System;
using System.Collections.Generic;

namespace SharpProspero.Ui;

/// <summary>
/// A back-stack of screens, for an application with more than one page: a main menu that opens settings,
/// which opens a sub-page, each returning to the one before. Push a screen to go forward and pop to go
/// back; only the top screen is laid out, updated and drawn, and by default the cancel button pops it.
/// This turns the single-screen <see cref="UiScreen"/> into the frame of a whole application.
/// </summary>
/// <remarks>
/// Drive it exactly like a single screen: each frame call <see cref="Update"/> with the input, then
/// <see cref="Render"/> (or <see cref="Layout"/> then <see cref="Draw"/>). When a pushed screen has no
/// cancel handler of its own and <see cref="PopOnCancel"/> is set, pressing cancel returns to the
/// previous screen; the first screen keeps its own handler, so cancel there can leave the application.
/// </remarks>
/// <example>
/// <code>
/// var nav = new ScreenStack(mainMenu);
/// mainMenu.Cancelled = () => context.RequestExit();
/// // from a button on the main menu:
/// settingsButton.Activated = () => nav.Push(settingsScreen);
///
/// // each frame:
/// nav.Update(UiInput.From(context.Input, context.PreviousInput));
/// surface.Clear(nav.Current.Theme.Background);
/// nav.Render(surface, margin: 60);
/// </code>
/// </example>
public sealed class ScreenStack
{
    private readonly List<UiScreen> _screens = [];

    /// <summary>Creates a stack whose first screen is <paramref name="root"/>.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="root"/> is null.</exception>
    public ScreenStack(UiScreen root)
    {
        ArgumentNullException.ThrowIfNull(root);
        _screens.Add(root);
    }

    /// <summary>Whether pressing cancel on a pushed screen with no handler of its own pops it. Default true.</summary>
    public bool PopOnCancel { get; set; } = true;

    /// <summary>The screen on top, the one shown and driven.</summary>
    public UiScreen Current => _screens[^1];

    /// <summary>How many screens are on the stack.</summary>
    public int Count => _screens.Count;

    /// <summary>The screens, from the first pushed to the one on top.</summary>
    public IReadOnlyList<UiScreen> Screens => _screens;

    /// <summary>
    /// Puts <paramref name="screen"/> on top and shows it. When it has no <see cref="UiScreen.Cancelled"/>
    /// handler and <see cref="PopOnCancel"/> is set, cancel on it returns to the screen below.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="screen"/> is null.</exception>
    public void Push(UiScreen screen)
    {
        ArgumentNullException.ThrowIfNull(screen);
        if (PopOnCancel && screen.Cancelled is null)
            screen.Cancelled = () => Pop();
        _screens.Add(screen);
    }

    /// <summary>
    /// Removes the top screen and returns to the one below, unless the first screen is the only one left.
    /// </summary>
    /// <returns>True when a screen was popped; false when only the first screen remains.</returns>
    public bool Pop()
    {
        if (_screens.Count <= 1)
            return false;
        _screens.RemoveAt(_screens.Count - 1);
        return true;
    }

    /// <summary>Replaces the top screen with <paramref name="screen"/> without growing the stack.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="screen"/> is null.</exception>
    public void Replace(UiScreen screen)
    {
        ArgumentNullException.ThrowIfNull(screen);
        if (PopOnCancel && _screens.Count > 1 && screen.Cancelled is null)
            screen.Cancelled = () => Pop();
        _screens[^1] = screen;
    }

    /// <summary>Pops every screen above the first, returning to the bottom of the stack.</summary>
    public void PopToRoot()
    {
        if (_screens.Count > 1)
            _screens.RemoveRange(1, _screens.Count - 1);
    }

    /// <summary>Lays the top screen out within <paramref name="area"/>.</summary>
    public void Layout(UiRect area) => Current.Layout(area);

    /// <summary>Routes one frame of <paramref name="input"/> to the top screen.</summary>
    public void Update(UiInput input) => Current.Update(input);

    /// <summary>Draws the top screen.</summary>
    public void Draw(Surface surface) => Current.Draw(surface);

    /// <summary>Lays the top screen out to fill <paramref name="surface"/> (less <paramref name="margin"/>) and draws it.</summary>
    public void Render(Surface surface, int margin = 0) => Current.Render(surface, margin);
}
