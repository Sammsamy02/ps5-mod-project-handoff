// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Graphics;
using System;

namespace SharpProspero.Diagnostics;

/// <summary>
/// Tracks how long recent frames took and reports the frame rate and the frame time, so a build can show
/// whether it is holding its pace. Feed it the time since the last frame each frame; read
/// <see cref="Fps"/> and the millisecond figures, or draw the readout and a small graph over the screen.
/// </summary>
/// <remarks>
/// It keeps a rolling window of the most recent frames, so the figures follow the recent past rather than
/// the whole run. Drive it from the frame loop with <c>FrameContext.DeltaSeconds</c>.
/// </remarks>
/// <example>
/// <code>
/// stats.Record((float)context.DeltaSeconds);
/// // after drawing the screen:
/// stats.Draw(surface, 20, 20, scale: 2, Color.White);
/// </code>
/// </example>
public sealed class FrameStats
{
    private readonly float[] _samples;
    private readonly float[] _sorted;
    private readonly (int X, int Y)[] _points;
    private int _count;
    private int _head;

    /// <summary>Creates a tracker that averages over the most recent <paramref name="window"/> frames.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="window"/> is less than two.</exception>
    public FrameStats(int window = 120)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(window, 2);
        _samples = new float[window];
        _sorted = new float[window];
        _points = new (int, int)[window];
    }

    /// <summary>The most recent frame's time, in milliseconds.</summary>
    public float LastMs { get; private set; }

    /// <summary>The mean frame time over the window, in milliseconds.</summary>
    public float AvgMs { get; private set; }

    /// <summary>The shortest frame time in the window, in milliseconds.</summary>
    public float MinMs { get; private set; }

    /// <summary>The longest frame time in the window, in milliseconds.</summary>
    public float MaxMs { get; private set; }

    /// <summary>The frame rate from the mean frame time, in frames per second.</summary>
    public float Fps => AvgMs > 0f ? 1000f / AvgMs : 0f;

    /// <summary>
    /// The frame rate of the one-percent-low frames: the slowest frames the window holds, reported as the
    /// rate a player feels during a stutter rather than the smooth average. Zero until the window fills a
    /// little. This is the ninety-ninth-percentile frame time turned into a rate.
    /// </summary>
    public float OnePercentLowFps
    {
        get
        {
            float ms = PercentileMs(99f);
            return ms > 0f ? 1000f / ms : 0f;
        }
    }

    /// <summary>How many frames the window currently holds.</summary>
    public int SampleCount => _count;

    /// <summary>
    /// The frame time at the given <paramref name="percentile"/> (0 to 100) of the window, in
    /// milliseconds: the ninety-fifth percentile, say, is the time all but the slowest five percent of
    /// frames came in under. Returns zero when the window is empty. A high percentile exposes the slow
    /// frames that a mean hides.
    /// </summary>
    public float PercentileMs(float percentile)
    {
        if (_count == 0)
            return 0f;
        Array.Copy(_samples, _sorted, _count);
        Array.Sort(_sorted, 0, _count);
        float clamped = Math.Clamp(percentile, 0f, 100f);
        // Nearest-rank: the smallest sample at or above the requested share of the window.
        int rank = (int)MathF.Ceiling(clamped / 100f * _count);
        int index = Math.Clamp(rank - 1, 0, _count - 1);
        return _sorted[index];
    }

    /// <summary>Adds a frame of <paramref name="deltaSeconds"/>. A negative or zero delta is ignored.</summary>
    public void Record(float deltaSeconds)
    {
        if (!float.IsFinite(deltaSeconds) || deltaSeconds <= 0f)
            return;

        float ms = deltaSeconds * 1000f;
        _samples[_head] = ms;
        _head = (_head + 1) % _samples.Length;
        if (_count < _samples.Length)
            _count++;
        LastMs = ms;

        // The window is small, so the figures are recomputed from it each frame rather than kept as a
        // running sum, which keeps the minimum and maximum exact as old frames leave the window.
        float sum = 0f, min = float.MaxValue, max = 0f;
        for (int i = 0; i < _count; i++)
        {
            float s = _samples[i];
            sum += s;
            if (s < min) min = s;
            if (s > max) max = s;
        }
        AvgMs = sum / _count;
        MinMs = min;
        MaxMs = max;
    }

    /// <summary>Clears the window back to empty.</summary>
    public void Reset()
    {
        _count = 0;
        _head = 0;
        LastMs = AvgMs = MinMs = MaxMs = 0f;
    }

    /// <summary>Draws a one-line readout of the rate and the frame time at (<paramref name="x"/>, <paramref name="y"/>).</summary>
    public void Draw(Surface surface, int x, int y, int scale, Color color)
    {
        string line = $"fps {Fps,3:0}  {AvgMs:0.0}ms avg  {MaxMs:0.0}ms max";
        surface.DrawText(line, x, y, scale, color);
    }

    /// <summary>
    /// Draws a sparkline of the recent frame times in the rectangle at (<paramref name="x"/>,
    /// <paramref name="y"/>), oldest at the left. The scale keeps a 30-frames-a-second spike visible, so
    /// a flat low line is a build holding its pace and a peak is a slow frame.
    /// </summary>
    public void DrawGraph(Surface surface, int x, int y, int width, int height, Color lineColor, Color? border = null)
    {
        if (width <= 1 || height <= 1)
            return;
        if (border is Color frame)
            surface.DrawRect(x, y, width, height, frame);
        if (_count < 2)
            return;

        // A slow frame (about 33ms) reaches near the top, so the common 16ms frames sit around the middle.
        float scaleMax = Math.Max(MaxMs, 33.34f);
        Span<(int X, int Y)> points = _points.AsSpan(0, _count);
        for (int i = 0; i < _count; i++)
        {
            int index = _count == _samples.Length ? (_head + i) % _samples.Length : i;
            float t = Math.Clamp(_samples[index] / scaleMax, 0f, 1f);
            int px = x + (i * (width - 1) / (_count - 1));
            int py = y + (height - 1) - (int)(t * (height - 1));
            points[i] = (px, py);
        }
        surface.DrawPolyline(points, lineColor);
    }
}
