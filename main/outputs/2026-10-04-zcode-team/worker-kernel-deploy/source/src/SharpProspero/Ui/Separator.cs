// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;

namespace SharpProspero.Ui;

/// <summary>
/// A horizontal rule that divides one group of controls from the next. It takes no focus and needs no
/// content; put one between sections of a panel.
/// </summary>
public sealed class Separator : UiElement
{
    /// <summary>The line's thickness in pixels. Default one.</summary>
    public int Thickness { get; set; } = 1;

    /// <summary>The line color, or null to use the theme's border color.</summary>
    public Color? LineColor { get; set; }

    /// <inheritdoc/>
    public override int Measure(int width, UiTheme theme) => (theme.Spacing * 2) + Thickness;

    /// <inheritdoc/>
    public override void Draw(Surface surface, UiTheme theme, UiElement? focused)
    {
        if (!Visible)
            return;
        int thickness = Thickness < 1 ? 1 : Thickness;
        int y = Bounds.Y + ((Bounds.Height - thickness) / 2);
        surface.FillRect(Bounds.X, y, Bounds.Width, thickness, LineColor ?? theme.Border);
    }
}
