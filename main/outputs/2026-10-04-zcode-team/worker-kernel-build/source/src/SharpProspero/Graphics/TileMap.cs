// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Numerics;
using SharpProspero.Storage;
using System;
using System.Collections.Generic;

namespace SharpProspero.Graphics;

/// <summary>
/// A grid of tiles for a level or a background — each cell names a frame of a <see cref="SpriteSheet"/>,
/// or is empty. Draw it through a <see cref="Camera2D"/> and only the tiles in view are drawn; ask
/// whether a rectangle meets any solid tile for movement and collision. Build one in code, or load it
/// from a CSV of tile numbers exported by a level editor.
/// </summary>
/// <remarks>
/// A cell holds the frame index to draw from the sheet, or <see cref="Empty"/> (-1) for nothing. What
/// counts as solid is up to the caller, passed to <see cref="Collides"/>, so the same map serves both
/// the look and the collision.
/// </remarks>
public sealed class TileMap
{
    /// <summary>The value of a cell with no tile.</summary>
    public const int Empty = -1;

    private readonly int[] _tiles;

    /// <summary>Creates an empty map of the given size, with the given tile size in pixels.</summary>
    /// <exception cref="ArgumentOutOfRangeException">A dimension is not positive.</exception>
    public TileMap(int columns, int rows, int tileWidth, int tileHeight)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(columns);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(rows);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(tileWidth);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(tileHeight);

        Columns = columns;
        Rows = rows;
        TileWidth = tileWidth;
        TileHeight = tileHeight;
        _tiles = new int[columns * rows];
        Array.Fill(_tiles, Empty);
    }

    /// <summary>How many tiles across.</summary>
    public int Columns { get; }

    /// <summary>How many tiles down.</summary>
    public int Rows { get; }

    /// <summary>The width of one tile in pixels.</summary>
    public int TileWidth { get; }

    /// <summary>The height of one tile in pixels.</summary>
    public int TileHeight { get; }

    /// <summary>The whole map's width in world pixels.</summary>
    public int WidthInPixels => Columns * TileWidth;

    /// <summary>The whole map's height in world pixels.</summary>
    public int HeightInPixels => Rows * TileHeight;

    /// <summary>The world rectangle the whole map covers.</summary>
    public RectF WorldBounds => new(0f, 0f, WidthInPixels, HeightInPixels);

    /// <summary>The tile at (<paramref name="column"/>, <paramref name="row"/>), or <see cref="Empty"/> when out of range.</summary>
    public int GetTile(int column, int row)
        => (uint)column < (uint)Columns && (uint)row < (uint)Rows ? _tiles[(row * Columns) + column] : Empty;

    /// <summary>Sets the tile at (<paramref name="column"/>, <paramref name="row"/>); a position out of range is ignored.</summary>
    public void SetTile(int column, int row, int tileIndex)
    {
        if ((uint)column < (uint)Columns && (uint)row < (uint)Rows)
            _tiles[(row * Columns) + column] = tileIndex;
    }

    /// <summary>Sets every cell to <paramref name="tileIndex"/>.</summary>
    public void Fill(int tileIndex) => Array.Fill(_tiles, tileIndex);

    /// <summary>The world rectangle a tile covers.</summary>
    public RectF TileBounds(int column, int row) => new(column * TileWidth, row * TileHeight, TileWidth, TileHeight);

    /// <summary>The tile column and row a world point falls in (may be outside the map).</summary>
    public (int Column, int Row) WorldToTile(Vector2 world)
        => ((int)MathF.Floor(world.X / TileWidth), (int)MathF.Floor(world.Y / TileHeight));

    /// <summary>
    /// Draws the tiles the camera can see, taking each cell's value as a frame of <paramref name="sheet"/>.
    /// Empty cells and cells whose value is outside the sheet are skipped. Only the visible tiles are
    /// drawn, so the cost follows the screen, not the map.
    /// </summary>
    public void Draw(Surface surface, SpriteSheet sheet, Camera2D camera)
    {
        ArgumentNullException.ThrowIfNull(camera);

        RectF view = camera.VisibleWorldBounds;
        int firstColumn = Math.Max(0, (int)MathF.Floor(view.Left / TileWidth));
        int lastColumn = Math.Min(Columns - 1, (int)MathF.Floor(view.Right / TileWidth));
        int firstRow = Math.Max(0, (int)MathF.Floor(view.Top / TileHeight));
        int lastRow = Math.Min(Rows - 1, (int)MathF.Floor(view.Bottom / TileHeight));

        int drawWidth = Math.Max(1, (int)MathF.Ceiling(TileWidth * camera.Zoom));
        int drawHeight = Math.Max(1, (int)MathF.Ceiling(TileHeight * camera.Zoom));

        for (int row = firstRow; row <= lastRow; row++)
        {
            for (int column = firstColumn; column <= lastColumn; column++)
            {
                int tile = _tiles[(row * Columns) + column];
                if (tile < 0 || tile >= sheet.Count)
                    continue;
                Vector2 screen = camera.WorldToScreen(new Vector2(column * TileWidth, row * TileHeight));
                sheet.DrawScaled(surface, tile, (int)MathF.Round(screen.X), (int)MathF.Round(screen.Y), drawWidth, drawHeight);
            }
        }
    }

    /// <summary>
    /// Whether <paramref name="worldRect"/> overlaps any tile that <paramref name="isSolid"/> reports as
    /// solid — the test a moving body makes against the level.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="isSolid"/> is null.</exception>
    public bool Collides(RectF worldRect, Func<int, bool> isSolid)
    {
        ArgumentNullException.ThrowIfNull(isSolid);
        if (worldRect.IsEmpty)
            return false;

        int firstColumn = Math.Max(0, (int)MathF.Floor(worldRect.Left / TileWidth));
        int lastColumn = Math.Min(Columns - 1, (int)MathF.Floor((worldRect.Right - 1e-4f) / TileWidth));
        int firstRow = Math.Max(0, (int)MathF.Floor(worldRect.Top / TileHeight));
        int lastRow = Math.Min(Rows - 1, (int)MathF.Floor((worldRect.Bottom - 1e-4f) / TileHeight));

        for (int row = firstRow; row <= lastRow; row++)
        {
            for (int column = firstColumn; column <= lastColumn; column++)
            {
                int tile = _tiles[(row * Columns) + column];
                if (tile != Empty && isSolid(tile))
                    return true;
            }
        }
        return false;
    }

    /// <summary>
    /// Builds a map from a CSV of tile numbers, one row of the map per line. A blank cell, or one that is
    /// not a whole number, is left empty. The map is as wide as the longest row.
    /// </summary>
    /// <exception cref="ArgumentException">The CSV holds no cells.</exception>
    /// <exception cref="ArgumentOutOfRangeException">A tile size is not positive.</exception>
    public static TileMap FromCsv(string csv, int tileWidth, int tileHeight, char separator = ',')
    {
        ArgumentNullException.ThrowIfNull(csv);
        List<string[]> rows = Csv.Parse(csv, separator);

        int columns = 0;
        foreach (string[] row in rows)
            columns = Math.Max(columns, row.Length);
        if (rows.Count == 0 || columns == 0)
            throw new ArgumentException("The CSV holds no cells.", nameof(csv));

        var map = new TileMap(columns, rows.Count, tileWidth, tileHeight);
        for (int row = 0; row < rows.Count; row++)
        {
            string[] cells = rows[row];
            for (int column = 0; column < cells.Length; column++)
            {
                ReadOnlySpan<char> cell = cells[column].AsSpan().Trim();
                map._tiles[(row * columns) + column] = int.TryParse(cell, out int value) ? value : Empty;
            }
        }
        return map;
    }
}
