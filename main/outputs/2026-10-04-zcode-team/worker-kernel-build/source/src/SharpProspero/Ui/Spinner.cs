// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using System;

namespace SharpProspero.Ui;

/// <summary>
/// A turning ring that shows work is under way with no known end — a load, a scan, a request in flight.
/// Where <see cref="ProgressBar"/> shows a fraction that is known, this shows only that something is
/// happening. It takes no focus and no input; advance it each frame with the time since the last so it
/// keeps turning.
/// </summary>
/// <remarks>
/// Call <see cref="Advance"/> with the frame's delta (from <c>FrameContext.DeltaSeconds</c>) before the
/// screen draws, the same way a <see cref="ProgressBar"/>'s value is set each frame. Hide it (set
/// <see cref="UiElement.Visible"/> false) once the work is done.
/// </remarks>
public sealed class Spinner : UiElement
{
    private float _phase;

    /// <summary>How wide and tall the ring is drawn, in pixels. Default 40.</summary>
    public int Diameter { get; set; } = 40;

    /// <summary>How fast the bright arc turns, in radians per second. Default about two-thirds of a turn.</summary>
    public float Speed { get; set; } = 4.2f;

    /// <summary>The share of the ring the bright arc covers, from 0 to 1. Default 0.28.</summary>
    public float ArcFraction { get; set; } = 0.28f;

    /// <summary>How thick the ring is, or -1 to scale it with the diameter (the default).</summary>
    public int Thickness { get; set; } = -1;

    /// <summary>The colour of the faint full ring behind the arc, or null to use the theme's border.</summary>
    public Color? TrackColor { get; set; }

    /// <summary>The colour of the turning arc, or null to use the theme's accent.</summary>
    public Color? ArcColor { get; set; }

    /// <summary>Advances the turn by <paramref name="deltaSeconds"/>. A negative or zero delta does nothing.</summary>
    public void Advance(float deltaSeconds)
    {
        if (!float.IsFinite(deltaSeconds) || deltaSeconds <= 0f)
            return;
        _phase += Speed * deltaSeconds;
        // Keep the phase bounded so it holds its precision over a spinner left turning for a long time.
        if (_phase >= MathF.Tau)
            _phase %= MathF.Tau;
    }

    /// <inheritdoc/>
    public override int Measure(int width, UiTheme theme) => Diameter;

    /// <inheritdoc/>
    public override void Draw(Surface surface, UiTheme theme, UiElement? focused)
    {
        if (!Visible || Diameter <= 1)
            return;

        int radius = Diameter / 2;
        int thickness = Thickness >= 1 ? Thickness : Math.Max(3, Diameter / 8);
        int inner = Math.Max(0, radius - thickness);
        int cx = Bounds.X + (Bounds.Width / 2);
        int cy = Bounds.Y + radius;

        surface.FillArcRing(cx, cy, inner, radius, 0f, MathF.Tau, TrackColor ?? theme.Border);
        surface.FillArcRing(cx, cy, inner, radius, _phase, ArcFraction * MathF.Tau, ArcColor ?? theme.Accent);
    }
}
