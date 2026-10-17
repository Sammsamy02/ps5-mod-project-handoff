// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Numerics;
using System;

namespace SharpProspero.Graphics;

/// <summary>
/// A movable, zoomable view onto a 2D world. It converts between world coordinates (where things live)
/// and screen coordinates (where they are drawn), so a map larger than the screen can scroll and zoom
/// while game logic stays in world space. Point it at the part of the world to show, pass it to a
/// <see cref="TileMap"/> or a <see cref="ParticleSystem"/> to draw, and convert positions yourself with
/// <see cref="WorldToScreen"/>.
/// </summary>
public sealed class Camera2D
{
    private float _zoom = 1f;

    /// <summary>Creates a camera for a viewport of the given size (usually the screen).</summary>
    /// <exception cref="ArgumentOutOfRangeException">A dimension is not positive.</exception>
    public Camera2D(int viewportWidth, int viewportHeight)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(viewportWidth);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(viewportHeight);
        ViewportWidth = viewportWidth;
        ViewportHeight = viewportHeight;
    }

    /// <summary>The world point shown at the centre of the viewport.</summary>
    public Vector2 Position { get; set; }

    /// <summary>How much the view is magnified; greater than one zooms in. Kept finite and above zero.</summary>
    public float Zoom
    {
        get => _zoom;
        set => _zoom = float.IsFinite(value) ? MathF.Max(value, 1e-4f) : 1e-4f;
    }

    /// <summary>The width of the viewport in pixels.</summary>
    public int ViewportWidth { get; set; }

    /// <summary>The height of the viewport in pixels.</summary>
    public int ViewportHeight { get; set; }

    /// <summary>The world rectangle the camera currently shows, for culling what is off screen.</summary>
    public RectF VisibleWorldBounds => RectF.FromCenter(Position, ViewportWidth / _zoom, ViewportHeight / _zoom);

    /// <summary>Converts a world point to where it lands on the screen.</summary>
    public Vector2 WorldToScreen(Vector2 world) => ((world - Position) * _zoom) + Center;

    /// <summary>Converts a screen point back to the world point under it.</summary>
    public Vector2 ScreenToWorld(Vector2 screen) => ((screen - Center) / _zoom) + Position;

    /// <summary>Moves the camera by <paramref name="delta"/> in world units.</summary>
    public void Move(Vector2 delta) => Position += delta;

    /// <summary>Centres the camera on <paramref name="position"/> in world units.</summary>
    public void MoveTo(Vector2 position) => Position = position;

    /// <summary>
    /// Keeps the view inside <paramref name="worldBounds"/> so the camera never shows past the edges of a
    /// map. When the world is smaller than the view along an axis, it centres on that axis instead.
    /// </summary>
    public void ClampToBounds(RectF worldBounds)
    {
        float halfWidth = ViewportWidth / (2f * _zoom);
        float halfHeight = ViewportHeight / (2f * _zoom);

        float x = worldBounds.Width <= 2f * halfWidth
            ? worldBounds.Center.X
            : MathUtil.Clamp(Position.X, worldBounds.Left + halfWidth, worldBounds.Right - halfWidth);
        float y = worldBounds.Height <= 2f * halfHeight
            ? worldBounds.Center.Y
            : MathUtil.Clamp(Position.Y, worldBounds.Top + halfHeight, worldBounds.Bottom - halfHeight);

        Position = new Vector2(x, y);
    }

    private Vector2 Center => new(ViewportWidth / 2f, ViewportHeight / 2f);
}
