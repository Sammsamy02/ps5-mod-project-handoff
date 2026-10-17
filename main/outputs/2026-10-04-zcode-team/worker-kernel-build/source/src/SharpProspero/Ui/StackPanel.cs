// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using System.Collections.Generic;

namespace SharpProspero.Ui;

/// <summary>
/// A container that stacks its children top to bottom, each as wide as the container and as tall as it
/// asks to be, with a gap between them. This is the usual root of a screen: add labels, buttons and
/// lists to it and it lays them out in order.
/// </summary>
public sealed class StackPanel : UiElement
{
    private readonly List<UiElement> _children = [];

    /// <summary>The gap between children, or -1 to use the theme's spacing (the default).</summary>
    public int Spacing { get; set; } = -1;

    /// <summary>The children, in order.</summary>
    public IReadOnlyList<UiElement> Children => _children;

    /// <summary>Adds <paramref name="child"/> to the bottom and returns this panel, so calls can chain.</summary>
    public StackPanel Add(UiElement child)
    {
        _children.Add(child);
        return this;
    }

    /// <inheritdoc />
    public override int Measure(int width, UiTheme theme)
    {
        int gap = Spacing >= 0 ? Spacing : theme.Spacing;
        int total = 0;
        int shown = 0;
        foreach (UiElement child in _children)
        {
            if (!child.Visible)
                continue;
            total += child.Measure(width, theme);
            shown++;
        }
        if (shown > 1)
            total += gap * (shown - 1);
        return total;
    }

    /// <inheritdoc />
    internal override void Arrange(UiRect bounds, UiTheme theme)
    {
        Bounds = bounds;
        int gap = Spacing >= 0 ? Spacing : theme.Spacing;
        int y = bounds.Y;
        foreach (UiElement child in _children)
        {
            if (!child.Visible)
                continue;
            int height = child.Measure(bounds.Width, theme);
            child.Arrange(new UiRect(bounds.X, y, bounds.Width, height), theme);
            y += height + gap;
        }
    }

    /// <inheritdoc />
    internal override void CollectFocusables(List<UiElement> into)
    {
        foreach (UiElement child in _children)
        {
            if (child.Visible)
                child.CollectFocusables(into);
        }
    }

    /// <inheritdoc />
    public override void Draw(Surface surface, UiTheme theme, UiElement? focused)
    {
        foreach (UiElement child in _children)
        {
            if (child.Visible)
                child.Draw(surface, theme, focused);
        }
    }
}
