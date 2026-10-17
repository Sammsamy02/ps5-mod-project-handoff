// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Numerics;

/// <summary>
/// An axis-aligned rectangle of single-precision numbers, for a bounding box, a hit area, or a region in
/// game and drawing code. It pairs with <see cref="Vector2"/>: a rectangle has a position and a size, and
/// answers whether it holds a point or meets another rectangle. It follows the usual half-open rule — the
/// top and left edges are inside, the bottom and right edges are not — so rectangles that share an edge do
/// not both claim it.
/// </summary>
/// <remarks>Creates a rectangle at (<paramref name="x"/>, <paramref name="y"/>) of the given size.</remarks>
public readonly struct RectF(float x, float y, float width, float height) : IEquatable<RectF>
{
    /// <summary>The left edge.</summary>
    public float X { get; } = x;

    /// <summary>The top edge.</summary>
    public float Y { get; } = y;

    /// <summary>The width.</summary>
    public float Width { get; } = width;

    /// <summary>The height.</summary>
    public float Height { get; } = height;

    /// <summary>The left edge (the same as <see cref="X"/>).</summary>
    public float Left => X;

    /// <summary>The top edge (the same as <see cref="Y"/>).</summary>
    public float Top => Y;

    /// <summary>The right edge.</summary>
    public float Right => X + Width;

    /// <summary>The bottom edge.</summary>
    public float Bottom => Y + Height;

    /// <summary>The top-left corner.</summary>
    public Vector2 Position => new(X, Y);

    /// <summary>The width and height as a vector.</summary>
    public Vector2 Size => new(Width, Height);

    /// <summary>The point at the middle.</summary>
    public Vector2 Center => new(X + (Width / 2f), Y + (Height / 2f));

    /// <summary>Whether the rectangle has no area (a width or height of zero or less).</summary>
    public bool IsEmpty => Width <= 0f || Height <= 0f;

    /// <summary>An empty rectangle at the origin.</summary>
    public static RectF Empty => default;

    /// <summary>Creates a rectangle from its left, top, right and bottom edges.</summary>
    public static RectF FromEdges(float left, float top, float right, float bottom)
        => new(left, top, right - left, bottom - top);

    /// <summary>Creates a rectangle of the given size centred on <paramref name="center"/>.</summary>
    public static RectF FromCenter(Vector2 center, float width, float height)
        => new(center.X - (width / 2f), center.Y - (height / 2f), width, height);

    /// <summary>Whether the point (<paramref name="px"/>, <paramref name="py"/>) is inside the rectangle.</summary>
    public bool Contains(float px, float py) => px >= X && px < Right && py >= Y && py < Bottom;

    /// <summary>Whether <paramref name="point"/> is inside the rectangle.</summary>
    public bool Contains(Vector2 point) => Contains(point.X, point.Y);

    /// <summary>Whether <paramref name="other"/> lies wholly inside this rectangle.</summary>
    public bool Contains(RectF other)
        => !IsEmpty && other.X >= X && other.Right <= Right && other.Y >= Y && other.Bottom <= Bottom;

    /// <summary>Whether this rectangle and <paramref name="other"/> overlap (a shared edge alone does not count).</summary>
    public bool Intersects(RectF other)
        => !IsEmpty && !other.IsEmpty && X < other.Right && other.X < Right && Y < other.Bottom && other.Y < Bottom;

    /// <summary>The overlapping region of this rectangle and <paramref name="other"/>, or <see cref="Empty"/> when they do not meet.</summary>
    public RectF Intersection(RectF other)
    {
        float left = MathF.Max(X, other.X);
        float top = MathF.Max(Y, other.Y);
        float right = MathF.Min(Right, other.Right);
        float bottom = MathF.Min(Bottom, other.Bottom);
        return right > left && bottom > top ? FromEdges(left, top, right, bottom) : Empty;
    }

    /// <summary>The smallest rectangle that holds both this one and <paramref name="other"/>.</summary>
    public RectF Union(RectF other)
    {
        if (IsEmpty)
            return other;
        if (other.IsEmpty)
            return this;
        float left = MathF.Min(X, other.X);
        float top = MathF.Min(Y, other.Y);
        float right = MathF.Max(Right, other.Right);
        float bottom = MathF.Max(Bottom, other.Bottom);
        return FromEdges(left, top, right, bottom);
    }

    /// <summary>The rectangle grown by <paramref name="dx"/> on the left and right and <paramref name="dy"/> on the top and bottom (negative to shrink).</summary>
    public RectF Inflate(float dx, float dy) => new(X - dx, Y - dy, Width + (2f * dx), Height + (2f * dy));

    /// <summary>The rectangle moved by (<paramref name="dx"/>, <paramref name="dy"/>).</summary>
    public RectF Offset(float dx, float dy) => new(X + dx, Y + dy, Width, Height);

    /// <summary>The rectangle moved by <paramref name="delta"/>.</summary>
    public RectF Offset(Vector2 delta) => Offset(delta.X, delta.Y);

    /// <summary>The point of <paramref name="point"/> pulled to the nearest spot inside the rectangle.</summary>
    public Vector2 Clamp(Vector2 point)
    {
        float cx = point.X < X ? X : (point.X > Right ? Right : point.X);
        float cy = point.Y < Y ? Y : (point.Y > Bottom ? Bottom : point.Y);
        return new Vector2(cx, cy);
    }

    /// <inheritdoc/>
    public bool Equals(RectF other) => X == other.X && Y == other.Y && Width == other.Width && Height == other.Height;

    /// <inheritdoc/>
    public override bool Equals(object? obj) => obj is RectF other && Equals(other);

    /// <inheritdoc/>
    public override int GetHashCode() => HashCode.Combine(X, Y, Width, Height);

    /// <summary>Whether two rectangles are equal.</summary>
    public static bool operator ==(RectF a, RectF b) => a.Equals(b);

    /// <summary>Whether two rectangles differ.</summary>
    public static bool operator !=(RectF a, RectF b) => !a.Equals(b);

    /// <inheritdoc/>
    public override string ToString() => $"({X}, {Y}, {Width}, {Height})";
}
