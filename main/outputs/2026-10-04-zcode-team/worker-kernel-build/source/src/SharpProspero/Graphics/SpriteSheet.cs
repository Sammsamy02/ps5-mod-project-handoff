// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics;

/// <summary>
/// A single image holding a grid of equal-sized frames — a character's animation, a set of icons, a
/// tile set. It reads the frames as views onto the shared image, copying nothing, so drawing one is
/// the same as drawing any surface. Pair it with a tween or a frame counter to animate.
/// </summary>
/// <remarks>
/// The sheet is any surface: a decoded <see cref="PngImage"/>, a region of the back buffer, or a
/// drawing built at run time. Frames are numbered left to right, top to bottom, starting at zero. The
/// sheet keeps the surface it was given; it allocates nothing of its own.
/// </remarks>
public readonly unsafe struct SpriteSheet
{
    private readonly Surface _sheet;

    /// <summary>Reads <paramref name="sheet"/> as a grid of frames of the given size.</summary>
    /// <param name="sheet">The image holding the frames.</param>
    /// <param name="frameWidth">The width of one frame in pixels.</param>
    /// <param name="frameHeight">The height of one frame in pixels.</param>
    /// <exception cref="ArgumentOutOfRangeException">A frame size is not positive, or is wider or taller than the sheet.</exception>
    public SpriteSheet(Surface sheet, int frameWidth, int frameHeight)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(frameWidth);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(frameHeight);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(frameWidth, sheet.Width);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(frameHeight, sheet.Height);

        _sheet = sheet;
        FrameWidth = frameWidth;
        FrameHeight = frameHeight;
        Columns = sheet.Width / frameWidth;
        Rows = sheet.Height / frameHeight;
    }

    /// <summary>The width of one frame in pixels.</summary>
    public int FrameWidth { get; }

    /// <summary>The height of one frame in pixels.</summary>
    public int FrameHeight { get; }

    /// <summary>How many frames fit across the sheet.</summary>
    public int Columns { get; }

    /// <summary>How many rows of frames the sheet holds.</summary>
    public int Rows { get; }

    /// <summary>How many frames there are in total.</summary>
    public int Count => Columns * Rows;

    /// <summary>
    /// A view onto frame <paramref name="index"/> (numbered from zero, left to right then down). The
    /// view shares the sheet's pixels, so drawing it draws that frame.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="index"/> is outside the frames.</exception>
    public Surface Frame(int index)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(index);
        ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(index, Count);
        int column = index % Columns;
        int row = index / Columns;
        return _sheet.Region(column * FrameWidth, row * FrameHeight, FrameWidth, FrameHeight);
    }

    /// <summary>
    /// Draws frame <paramref name="index"/> onto <paramref name="destination"/> at
    /// (<paramref name="x"/>, <paramref name="y"/>), blending each pixel over what is there by its
    /// alpha so a frame with a transparent background composites as a sprite.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="index"/> is outside the frames.</exception>
    public void Draw(Surface destination, int index, int x, int y)
        => destination.BlitBlended(Frame(index), x, y);

    /// <summary>
    /// Draws frame <paramref name="index"/> scaled into the rectangle at (<paramref name="x"/>,
    /// <paramref name="y"/>), blending by alpha. Use this to show a frame larger or smaller than its
    /// own size.
    /// </summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="index"/> is outside the frames.</exception>
    public void DrawScaled(Surface destination, int index, int x, int y, int width, int height)
        => destination.BlitScaledBlended(Frame(index), x, y, width, height);
}
