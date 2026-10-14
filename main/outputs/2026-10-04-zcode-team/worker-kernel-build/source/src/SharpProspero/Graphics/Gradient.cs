// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Graphics;

/// <summary>A colour placed at a position along a gradient, where position runs 0 to 1.</summary>
/// <param name="Position">Where the colour sits, from 0 (start) to 1 (end).</param>
/// <param name="Color">The colour at that position.</param>
public readonly record struct GradientStop(float Position, Color Color);

/// <summary>
/// A colour ramp defined by two or more stops that can be sampled at any position. Where the surface's
/// built-in gradient fills blend two colours, this holds as many stops as you like — a heat ramp, a UI
/// theme, a spectrum — and returns the colour at any point along it.
/// </summary>
public sealed class Gradient
{
    private readonly GradientStop[] _stops;

    /// <summary>Creates a gradient from its stops. They are sorted by position; at least one is required.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="stops"/> is null.</exception>
    /// <exception cref="ArgumentException">No stops were given.</exception>
    public Gradient(params GradientStop[] stops)
        : this((IEnumerable<GradientStop>)stops)
    {
    }

    /// <summary>Creates a gradient from its stops. They are sorted by position; at least one is required.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="stops"/> is null.</exception>
    /// <exception cref="ArgumentException">No stops were given.</exception>
    public Gradient(IEnumerable<GradientStop> stops)
    {
        ArgumentNullException.ThrowIfNull(stops);
        _stops = [.. stops];
        if (_stops.Length == 0)
            throw new ArgumentException("A gradient needs at least one stop.", nameof(stops));
        Array.Sort(_stops, static (a, b) => a.Position.CompareTo(b.Position));
    }

    /// <summary>How many stops define the gradient.</summary>
    public int StopCount => _stops.Length;

    /// <summary>
    /// Returns the colour at <paramref name="t"/>, clamped to 0..1. Before the first stop it is the first
    /// colour and after the last stop the last, and in between it blends the two surrounding stops.
    /// </summary>
    public Color Sample(float t)
    {
        if (float.IsNaN(t) || t <= _stops[0].Position || _stops.Length == 1)
            return _stops[0].Color;

        GradientStop last = _stops[^1];
        if (t >= last.Position)
            return last.Color;

        for (int i = 1; i < _stops.Length; i++)
        {
            GradientStop hi = _stops[i];
            if (t <= hi.Position)
            {
                GradientStop lo = _stops[i - 1];
                float span = hi.Position - lo.Position;
                float local = span <= 0f ? 0f : (t - lo.Position) / span;
                return Color.Lerp(lo.Color, hi.Color, local);
            }
        }

        return last.Color;
    }

    /// <summary>Samples the gradient at <paramref name="count"/> evenly spaced points into a fixed palette.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="count"/> is not positive.</exception>
    public Palette ToPalette(int count)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
        Color[] colors = new Color[count];
        for (int i = 0; i < count; i++)
            colors[i] = Sample(count == 1 ? 0f : (float)i / (count - 1));
        return new Palette(colors);
    }

    /// <summary>A two-stop gradient from <paramref name="start"/> to <paramref name="end"/>.</summary>
    public static Gradient TwoColor(Color start, Color end) =>
        new(new GradientStop(0f, start), new GradientStop(1f, end));

    /// <summary>A black-red-yellow-white heat ramp, for a level meter or a value map.</summary>
    public static Gradient Heat { get; } = new(
        new GradientStop(0f, Color.FromRgb(0, 0, 0)),
        new GradientStop(0.4f, Color.FromRgb(0xC0, 0x20, 0x00)),
        new GradientStop(0.75f, Color.FromRgb(0xFF, 0xC0, 0x00)),
        new GradientStop(1f, Color.FromRgb(0xFF, 0xFF, 0xFF)));

    /// <summary>A red-orange-yellow-green-blue-violet spectrum, for a rainbow ramp.</summary>
    public static Gradient Rainbow { get; } = new(
        new GradientStop(0f, Color.FromRgb(0xFF, 0x00, 0x00)),
        new GradientStop(0.2f, Color.FromRgb(0xFF, 0xA5, 0x00)),
        new GradientStop(0.4f, Color.FromRgb(0xFF, 0xFF, 0x00)),
        new GradientStop(0.6f, Color.FromRgb(0x00, 0xC0, 0x00)),
        new GradientStop(0.8f, Color.FromRgb(0x00, 0x40, 0xFF)),
        new GradientStop(1f, Color.FromRgb(0x80, 0x00, 0xFF)));
}

/// <summary>
/// A fixed set of colours addressed by index, for a theme, a data-series colour cycle, or a sampled
/// gradient. Build one from colours directly or from a <see cref="Gradient"/>.
/// </summary>
public sealed class Palette
{
    private readonly Color[] _colors;

    /// <summary>Creates a palette from the given colours, in order. At least one is required.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="colors"/> is null.</exception>
    /// <exception cref="ArgumentException">No colours were given.</exception>
    public Palette(params Color[] colors)
    {
        ArgumentNullException.ThrowIfNull(colors);
        if (colors.Length == 0)
            throw new ArgumentException("A palette needs at least one colour.", nameof(colors));
        _colors = (Color[])colors.Clone();
    }

    /// <summary>How many colours the palette holds.</summary>
    public int Count => _colors.Length;

    /// <summary>The colour at <paramref name="index"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="index"/> is outside the palette.</exception>
    public Color this[int index]
    {
        get
        {
            ArgumentOutOfRangeException.ThrowIfNegative(index);
            ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(index, _colors.Length);
            return _colors[index];
        }
    }

    /// <summary>
    /// Cycles through the palette so any index maps to a colour, wrapping round for a colour-per-series
    /// scheme where the count is not known in advance.
    /// </summary>
    public Color Cycle(int index)
    {
        int wrapped = index % _colors.Length;
        if (wrapped < 0)
            wrapped += _colors.Length;
        return _colors[wrapped];
    }

    /// <summary>Maps <paramref name="t"/> in 0..1 to the nearest palette entry.</summary>
    public Color Sample(float t)
    {
        if (_colors.Length == 1)
            return _colors[0];
        float clamped = float.IsNaN(t) || t < 0f ? 0f : t > 1f ? 1f : t; // match Gradient: an undefined t maps low
        int index = (int)MathF.Round(clamped * (_colors.Length - 1));
        return _colors[index];
    }
}
