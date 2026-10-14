// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Ai;

/// <summary>
/// Finds the shortest path across a grid — an enemy routing around walls, a cursor stepping to a target
/// on a board. It runs A* over cells you say are walkable, so it works with a <c>TileMap</c> (pass a
/// test that reads the tiles) or any grid of your own. Reuse one instance: it keeps its working buffers,
/// so repeated searches on the same grid allocate only the path they return.
/// </summary>
/// <remarks>
/// Set <see cref="AllowDiagonal"/> to let paths cut across corners diagonally; by default it moves only
/// up, down, left and right. A diagonal step is only taken when both cells beside it are walkable, so a
/// path never slips through the corner of a wall.
/// </remarks>
/// <example>
/// <code>
/// var finder = new GridPathfinder(map.Columns, map.Rows);
/// var path = finder.FindPath((startCol, startRow), (goalCol, goalRow),
///     (col, row) => map.GetTile(col, row) &lt; 16);   // tiles below 16 are floor
/// foreach ((int col, int row) in path)
///     StepEnemyTo(col, row);
/// </code>
/// </example>
public sealed class GridPathfinder
{
    private const float DiagonalCost = 1.41421356f;

    private readonly int _columns;
    private readonly int _rows;
    private readonly float[] _gScore;
    private readonly int[] _cameFrom;
    private readonly bool[] _closed;
    private readonly PriorityQueue<int, float> _open = new();

    /// <summary>Creates a pathfinder for a grid of the given size.</summary>
    /// <exception cref="ArgumentOutOfRangeException">A dimension is not positive.</exception>
    public GridPathfinder(int columns, int rows)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(columns);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(rows);
        _columns = columns;
        _rows = rows;
        int cells = columns * rows;
        _gScore = new float[cells];
        _cameFrom = new int[cells];
        _closed = new bool[cells];
    }

    /// <summary>How many columns the grid has.</summary>
    public int Columns => _columns;

    /// <summary>How many rows the grid has.</summary>
    public int Rows => _rows;

    /// <summary>Whether diagonal steps are allowed. Default false.</summary>
    public bool AllowDiagonal { get; set; }

    /// <summary>
    /// Finds the shortest path from <paramref name="start"/> to <paramref name="goal"/> across cells that
    /// <paramref name="isWalkable"/> reports as open. The returned list runs from the start cell to the
    /// goal cell inclusive, or is empty when there is no path (or an end is off the grid or blocked).
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="isWalkable"/> is null.</exception>
    public List<(int Column, int Row)> FindPath((int Column, int Row) start, (int Column, int Row) goal, Func<int, int, bool> isWalkable)
    {
        ArgumentNullException.ThrowIfNull(isWalkable);
        return FindPath(start, goal, (column, row) => isWalkable(column, row) ? 1f : 0f);
    }

    /// <summary>
    /// Finds the cheapest path from <paramref name="start"/> to <paramref name="goal"/> when cells cost
    /// different amounts to cross, such as mud that is slower than a road. <paramref name="cost"/> returns
    /// the cost to enter a cell; a value of one is normal ground, higher is slower, and zero or less means
    /// the cell is blocked. The returned list runs from the start cell to the goal cell inclusive, or is
    /// empty when there is no path. Keep costs at one or above so the search stays optimal.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="cost"/> is null.</exception>
    public List<(int Column, int Row)> FindPath((int Column, int Row) start, (int Column, int Row) goal, Func<int, int, float> cost)
    {
        ArgumentNullException.ThrowIfNull(cost);
        var path = new List<(int, int)>();

        if (!InBounds(start.Column, start.Row) || !InBounds(goal.Column, goal.Row))
            return path;
        if (cost(start.Column, start.Row) <= 0f || cost(goal.Column, goal.Row) <= 0f)
            return path;

        Array.Fill(_gScore, float.PositiveInfinity);
        Array.Fill(_cameFrom, -1);
        Array.Clear(_closed);
        _open.Clear();

        int startIndex = (start.Row * _columns) + start.Column;
        int goalIndex = (goal.Row * _columns) + goal.Column;
        _gScore[startIndex] = 0f;
        _open.Enqueue(startIndex, Heuristic(start.Column, start.Row, goal.Column, goal.Row));

        while (_open.TryDequeue(out int current, out _))
        {
            if (current == goalIndex)
            {
                Reconstruct(current, path);
                return path;
            }
            if (_closed[current])
                continue;
            _closed[current] = true;

            int cx = current % _columns;
            int cy = current / _columns;
            for (int direction = 0; direction < 8; direction++)
            {
                bool diagonal = direction >= 4;
                if (diagonal && !AllowDiagonal)
                    break;

                (int dx, int dy) = Offsets[direction];
                int nx = cx + dx, ny = cy + dy;
                if (!InBounds(nx, ny))
                    continue;
                float cellCost = cost(nx, ny);
                if (cellCost <= 0f)
                    continue;

                // A diagonal step is only taken when both orthogonal cells beside it are open, so a path
                // never slips through the corner of a wall.
                if (diagonal && (cost(cx + dx, cy) <= 0f || cost(cx, cy + dy) <= 0f))
                    continue;

                int neighbor = (ny * _columns) + nx;
                if (_closed[neighbor])
                    continue;

                float tentative = _gScore[current] + (cellCost * (diagonal ? DiagonalCost : 1f));
                if (tentative < _gScore[neighbor])
                {
                    _gScore[neighbor] = tentative;
                    _cameFrom[neighbor] = current;
                    _open.Enqueue(neighbor, tentative + Heuristic(nx, ny, goal.Column, goal.Row));
                }
            }
        }

        return path; // no path was found
    }

    private static readonly (int Dx, int Dy)[] Offsets =
    [
        (0, -1), (0, 1), (-1, 0), (1, 0),   // orthogonal
        (-1, -1), (1, -1), (-1, 1), (1, 1), // diagonal
    ];

    private void Reconstruct(int goalIndex, List<(int, int)> path)
    {
        for (int node = goalIndex; node != -1; node = _cameFrom[node])
            path.Add((node % _columns, node / _columns));
        path.Reverse();
    }

    private float Heuristic(int ax, int ay, int bx, int by)
    {
        int dx = Math.Abs(ax - bx);
        int dy = Math.Abs(ay - by);
        if (!AllowDiagonal)
            return dx + dy; // Manhattan distance for four-way movement

        // Octile distance for eight-way movement: straight runs plus diagonal shortcuts.
        int min = Math.Min(dx, dy);
        int max = Math.Max(dx, dy);
        return (max - min) + (DiagonalCost * min);
    }

    private bool InBounds(int column, int row) => (uint)column < (uint)_columns && (uint)row < (uint)_rows;
}
