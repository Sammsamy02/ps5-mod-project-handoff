// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Numerics;

/// <summary>A rectangle placed by a <see cref="RectPacker"/>, in whole pixels.</summary>
public readonly record struct PackedRect(int Id, int X, int Y, int Width, int Height);

/// <summary>
/// Packs many small rectangles into one larger area without overlap - the job behind building a sprite
/// sheet or a glyph atlas from a set of images. It fills bottom-left along a running skyline, which keeps
/// the result compact. Work in whole pixels: give each piece a size and an id, and read back where it
/// landed.
/// </summary>
public sealed class RectPacker
{
    private readonly List<(int X, int Y, int Width)> _skyline = [];
    private long _usedArea;

    /// <summary>Creates a packer that fills an area <paramref name="width"/> by <paramref name="height"/> pixels.</summary>
    public RectPacker(int width, int height)
    {
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width), "The packing area must be positive.");
        Width = width;
        Height = height;
        _skyline.Add((0, 0, width));
    }

    /// <summary>Width of the packing area in pixels.</summary>
    public int Width { get; }

    /// <summary>Height of the packing area in pixels.</summary>
    public int Height { get; }

    /// <summary>The fraction of the area filled so far, 0 to 1.</summary>
    public float Occupancy => (float)((double)_usedArea / ((long)Width * Height));

    /// <summary>Clears every placement and returns the packer to empty.</summary>
    public void Reset()
    {
        _skyline.Clear();
        _skyline.Add((0, 0, Width));
        _usedArea = 0;
    }

    /// <summary>
    /// Places one rectangle and returns where it landed, or null when it does not fit in the space left.
    /// </summary>
    public PackedRect? Insert(int width, int height, int id = 0)
    {
        if (width <= 0 || height <= 0 || width > Width || height > Height)
            return null;

        int bestY = int.MaxValue, bestX = 0, bestIndex = -1, bestWidth = int.MaxValue;
        for (int i = 0; i < _skyline.Count; i++)
        {
            if (Fits(i, width, height, out int y) && (y < bestY || (y == bestY && _skyline[i].Width < bestWidth)))
            {
                bestY = y;
                bestX = _skyline[i].X;
                bestWidth = _skyline[i].Width;
                bestIndex = i;
            }
        }
        if (bestIndex < 0)
            return null;

        AddLevel(bestIndex, bestX, bestY, width, height);
        _usedArea += (long)width * height;
        return new PackedRect(id, bestX, bestY, width, height);
    }

    /// <summary>
    /// Packs a batch, largest first for a tighter fit, and returns the pieces that fit. Compare the count
    /// with the number given to see whether any were too large for the remaining space.
    /// </summary>
    public IReadOnlyList<PackedRect> Pack(IEnumerable<(int Id, int Width, int Height)> items)
    {
        ArgumentNullException.ThrowIfNull(items);
        var ordered = new List<(int Id, int Width, int Height)>(items);
        ordered.Sort((a, b) => Math.Max(b.Width, b.Height).CompareTo(Math.Max(a.Width, a.Height)));
        var placed = new List<PackedRect>(ordered.Count);
        foreach ((int id, int w, int h) in ordered)
        {
            PackedRect? slot = Insert(w, h, id);
            if (slot is { } rect)
                placed.Add(rect);
        }
        return placed;
    }

    // Whether a width-by-height rectangle starting at skyline node i fits, and at what top edge y.
    private bool Fits(int index, int width, int height, out int y)
    {
        int x = _skyline[index].X;
        if (x + width > Width)
        {
            y = 0;
            return false;
        }
        int remaining = width;
        int i = index;
        y = _skyline[index].Y;
        while (remaining > 0)
        {
            y = Math.Max(y, _skyline[i].Y);
            if (y + height > Height)
                return false;
            remaining -= _skyline[i].Width;
            i++;
            if (i == _skyline.Count && remaining > 0)
                return false;
        }
        return true;
    }

    // Raises the skyline where the rectangle was placed and trims the segments it now covers.
    private void AddLevel(int index, int x, int y, int width, int height)
    {
        _skyline.Insert(index, (x, y + height, width));
        for (int i = index + 1; i < _skyline.Count;)
        {
            (int nx, int ny, int nw) = _skyline[i];
            if (nx < x + width)
            {
                int overlap = x + width - nx;
                if (overlap >= nw)
                {
                    _skyline.RemoveAt(i);
                }
                else
                {
                    _skyline[i] = (nx + overlap, ny, nw - overlap);
                    break;
                }
            }
            else
            {
                break;
            }
        }
        Merge();
    }

    // Joins neighbouring skyline segments that sit at the same height.
    private void Merge()
    {
        for (int i = 0; i < _skyline.Count - 1;)
        {
            if (_skyline[i].Y == _skyline[i + 1].Y)
            {
                _skyline[i] = (_skyline[i].X, _skyline[i].Y, _skyline[i].Width + _skyline[i + 1].Width);
                _skyline.RemoveAt(i + 1);
            }
            else
            {
                i++;
            }
        }
    }
}
