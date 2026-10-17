// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Ui;

/// <summary>
/// Chooses which control focus moves to when the user presses a direction. It picks the nearest
/// focusable control that lies in that direction, measured from the centers, favouring one that stays
/// on the same line over one that drifts to the side. This is what makes the d-pad move around the
/// screen the way a person expects.
/// </summary>
public static class FocusNavigator
{
    /// <summary>
    /// Returns the control focus should move to from <paramref name="current"/> in
    /// <paramref name="direction"/>, or null when there is none that way. With no current control, the
    /// first focusable is returned so a fresh screen starts with something focused.
    /// </summary>
    public static UiElement? Next(IReadOnlyList<UiElement> focusables, UiElement? current, UiDirection direction)
    {
        ArgumentNullException.ThrowIfNull(focusables);

        if (current is null)
            return focusables.Count > 0 ? focusables[0] : null;

        UiRect from = current.Bounds;
        UiElement? best = null;
        long bestScore = long.MaxValue;

        foreach (UiElement candidate in focusables)
        {
            if (ReferenceEquals(candidate, current))
                continue;

            UiRect to = candidate.Bounds;
            bool inDirection = direction switch
            {
                UiDirection.Up => to.CenterY < from.CenterY,
                UiDirection.Down => to.CenterY > from.CenterY,
                UiDirection.Left => to.CenterX < from.CenterX,
                UiDirection.Right => to.CenterX > from.CenterX,
                _ => false,
            };
            if (!inDirection)
                continue;

            bool vertical = direction is UiDirection.Up or UiDirection.Down;
            long primary = vertical ? Math.Abs(to.CenterY - from.CenterY) : Math.Abs(to.CenterX - from.CenterX);
            long cross = vertical ? Math.Abs(to.CenterX - from.CenterX) : Math.Abs(to.CenterY - from.CenterY);

            // Distance along the pressed axis, plus a heavier penalty for drifting off the line, so a
            // control straight ahead wins over one that is closer but far to the side.
            long score = primary + cross * 3;
            if (score < bestScore)
            {
                bestScore = score;
                best = candidate;
            }
        }

        return best;
    }
}
