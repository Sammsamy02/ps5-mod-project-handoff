// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Ui;

/// <summary>A direction a control's focus moves in, from the controller's d-pad.</summary>
public enum UiDirection
{
    /// <summary>Toward the top of the screen.</summary>
    Up,

    /// <summary>Toward the bottom of the screen.</summary>
    Down,

    /// <summary>Toward the left of the screen.</summary>
    Left,

    /// <summary>Toward the right of the screen.</summary>
    Right,
}

/// <summary>
/// An axis-aligned rectangle in pixels, the origin at the top-left. Layout gives every control a
/// rectangle to draw within, and focus navigation compares their centers.
/// </summary>
/// <param name="X">Left edge.</param>
/// <param name="Y">Top edge.</param>
/// <param name="Width">Width in pixels.</param>
/// <param name="Height">Height in pixels.</param>
public readonly record struct UiRect(int X, int Y, int Width, int Height)
{
    /// <summary>The x coordinate just past the right edge.</summary>
    public int Right => X + Width;

    /// <summary>The y coordinate just past the bottom edge.</summary>
    public int Bottom => Y + Height;

    /// <summary>The horizontal center.</summary>
    public int CenterX => X + Width / 2;

    /// <summary>The vertical center.</summary>
    public int CenterY => Y + Height / 2;

    /// <summary>True when (<paramref name="px"/>, <paramref name="py"/>) is inside the rectangle.</summary>
    public bool Contains(int px, int py) => px >= X && px < Right && py >= Y && py < Bottom;

    /// <summary>The rectangle shrunk by <paramref name="amount"/> pixels on every side (never negative).</summary>
    public UiRect Inset(int amount)
        => new(X + amount, Y + amount, Math.Max(0, Width - 2 * amount), Math.Max(0, Height - 2 * amount));
}
