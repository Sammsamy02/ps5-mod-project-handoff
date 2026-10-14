// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Storage;

/// <summary>A read-only view of one row of a <see cref="DataTable"/>, addressed by column name or index.</summary>
public readonly struct DataRow
{
    private readonly DataTable _table;

    internal DataRow(DataTable table, int index)
    {
        _table = table;
        Index = index;
    }

    /// <summary>The row's position in the table.</summary>
    public int Index { get; }

    /// <summary>The cell in the column at <paramref name="column"/>.</summary>
    /// <exception cref="InvalidOperationException">This is a default row, not attached to a table.</exception>
    public string this[int column]
    {
        get
        {
            if (_table is null)
                throw new InvalidOperationException("This row is not attached to a table.");
            return _table[Index, column];
        }
    }

    /// <summary>The cell in the named <paramref name="column"/>.</summary>
    /// <exception cref="InvalidOperationException">This is a default row, not attached to a table.</exception>
    public string this[string column]
    {
        get
        {
            if (_table is null)
                throw new InvalidOperationException("This row is not attached to a table.");
            return _table[Index, column];
        }
    }
}

/// <summary>
/// A small in-memory table of text cells with named columns, that a list or grid interface can bind to.
/// It turns the raw rows the CSV and JSON readers produce into something you can sort, filter and group,
/// each returning a new table so the original is untouched.
/// </summary>
public sealed class DataTable
{
    private readonly string[] _columns;
    private readonly Dictionary<string, int> _columnIndex;
    private readonly List<string[]> _rows = [];

    /// <summary>Creates a table with the given column names.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="columns"/> is null.</exception>
    /// <exception cref="ArgumentException">No columns, a null name, or a duplicate name.</exception>
    public DataTable(params string[] columns)
        : this((IEnumerable<string>)columns)
    {
    }

    /// <summary>Creates a table with the given column names.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="columns"/> is null.</exception>
    /// <exception cref="ArgumentException">No columns, a null name, or a duplicate name.</exception>
    public DataTable(IEnumerable<string> columns)
    {
        ArgumentNullException.ThrowIfNull(columns);
        _columns = [.. columns];
        if (_columns.Length == 0)
            throw new ArgumentException("A data table needs at least one column.", nameof(columns));

        _columnIndex = new Dictionary<string, int>(StringComparer.Ordinal);
        for (int i = 0; i < _columns.Length; i++)
        {
            if (_columns[i] is null)
                throw new ArgumentException("A column name cannot be null.", nameof(columns));
            if (!_columnIndex.TryAdd(_columns[i], i))
                throw new ArgumentException($"The column '{_columns[i]}' is named more than once.", nameof(columns));
        }
    }

    /// <summary>The column names, in order.</summary>
    public IReadOnlyList<string> Columns => _columns;

    /// <summary>How many columns the table has.</summary>
    public int ColumnCount => _columns.Length;

    /// <summary>How many rows the table holds.</summary>
    public int RowCount => _rows.Count;

    /// <summary>The index of the named column, or -1 when there is no such column.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="column"/> is null.</exception>
    public int IndexOfColumn(string column)
    {
        ArgumentNullException.ThrowIfNull(column);
        return _columnIndex.TryGetValue(column, out int index) ? index : -1;
    }

    /// <summary>
    /// Adds a row. Fewer cells than columns are padded with empty strings; more cells than columns is an
    /// error.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="cells"/> is null.</exception>
    /// <exception cref="ArgumentException">More cells were given than the table has columns.</exception>
    public void AddRow(params string[] cells)
    {
        ArgumentNullException.ThrowIfNull(cells);
        if (cells.Length > _columns.Length)
            throw new ArgumentException($"The row has {cells.Length} cells but the table has {_columns.Length} columns.", nameof(cells));

        string[] row = new string[_columns.Length];
        for (int i = 0; i < _columns.Length; i++)
            row[i] = i < cells.Length ? cells[i] ?? string.Empty : string.Empty;
        _rows.Add(row);
    }

    /// <summary>The cell at <paramref name="row"/> and <paramref name="column"/> index.</summary>
    /// <exception cref="ArgumentOutOfRangeException">A coordinate is outside the table.</exception>
    public string this[int row, int column]
    {
        get
        {
            ArgumentOutOfRangeException.ThrowIfNegative(row);
            ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(row, _rows.Count);
            ArgumentOutOfRangeException.ThrowIfNegative(column);
            ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(column, _columns.Length);
            return _rows[row][column];
        }
    }

    /// <summary>The cell at <paramref name="row"/> in the named <paramref name="column"/>.</summary>
    /// <exception cref="ArgumentException">There is no such column.</exception>
    public string this[int row, string column]
    {
        get
        {
            int index = IndexOfColumn(column);
            if (index < 0)
                throw new ArgumentException($"There is no column '{column}'.", nameof(column));
            return this[row, index];
        }
    }

    /// <summary>A view of the row at <paramref name="index"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="index"/> is outside the table.</exception>
    public DataRow Row(int index)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(index);
        ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual(index, _rows.Count);
        return new DataRow(this, index);
    }

    /// <summary>The rows as views, in order.</summary>
    public IEnumerable<DataRow> Rows
    {
        get
        {
            for (int i = 0; i < _rows.Count; i++)
                yield return new DataRow(this, i);
        }
    }

    /// <summary>
    /// Returns a new table with the rows ordered by the named column. The order is stable, so rows with an
    /// equal key keep their original order. Pass a comparer (for example a natural-order one) to change how
    /// values compare; the default is ordinal.
    /// </summary>
    /// <exception cref="ArgumentException">There is no such column.</exception>
    public DataTable SortBy(string column, bool descending = false, IComparer<string>? comparer = null)
    {
        int index = ColumnOrThrow(column);
        comparer ??= StringComparer.Ordinal;

        int[] order = new int[_rows.Count];
        for (int i = 0; i < order.Length; i++)
            order[i] = i;

        Array.Sort(order, (a, b) =>
        {
            // Reverse by swapping the operands, not by negating, so a comparer that returns int.MinValue
            // cannot produce an inconsistent order. The tiebreak stays ascending to keep equal keys in
            // their original order in both directions.
            int compare = descending
                ? comparer.Compare(_rows[b][index], _rows[a][index])
                : comparer.Compare(_rows[a][index], _rows[b][index]);
            return compare != 0 ? compare : a.CompareTo(b);
        });

        var result = new DataTable(_columns);
        foreach (int i in order)
            result._rows.Add((string[])_rows[i].Clone());
        return result;
    }

    /// <summary>Returns a new table with only the rows for which <paramref name="predicate"/> is true.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="predicate"/> is null.</exception>
    public DataTable Where(Func<DataRow, bool> predicate)
    {
        ArgumentNullException.ThrowIfNull(predicate);
        var result = new DataTable(_columns);
        for (int i = 0; i < _rows.Count; i++)
        {
            if (predicate(new DataRow(this, i)))
                result._rows.Add((string[])_rows[i].Clone());
        }

        return result;
    }

    /// <summary>Splits the rows into a table per distinct value of the named column, keyed by that value.</summary>
    /// <exception cref="ArgumentException">There is no such column.</exception>
    public Dictionary<string, DataTable> GroupBy(string column)
    {
        int index = ColumnOrThrow(column);
        var groups = new Dictionary<string, DataTable>(StringComparer.Ordinal);
        for (int i = 0; i < _rows.Count; i++)
        {
            string key = _rows[i][index];
            if (!groups.TryGetValue(key, out DataTable? table))
            {
                table = new DataTable(_columns);
                groups[key] = table;
            }

            table._rows.Add((string[])_rows[i].Clone());
        }

        return groups;
    }

    /// <summary>
    /// Builds a table from CSV text. With <paramref name="hasHeader"/> the first row names the columns
    /// (blanks and missing names become col0, col1, …); otherwise the columns are named col0, col1, ….
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="text"/> is null.</exception>
    /// <exception cref="ArgumentException">The CSV has no rows.</exception>
    public static DataTable FromCsv(string text, bool hasHeader = true, char separator = ',')
    {
        ArgumentNullException.ThrowIfNull(text);
        List<string[]> rows = Csv.Parse(text, separator);
        if (rows.Count == 0)
            throw new ArgumentException("The CSV has no rows.", nameof(text));

        int width = 0;
        foreach (string[] row in rows)
            width = Math.Max(width, row.Length);
        if (width == 0)
            throw new ArgumentException("The CSV has no columns.", nameof(text));

        string[] columns = new string[width];
        string[]? header = hasHeader ? rows[0] : null;
        for (int i = 0; i < width; i++)
            columns[i] = header is not null && i < header.Length && header[i].Length > 0 ? header[i] : $"col{i}";

        // A CSV may repeat a header name, or a generated name may collide with one; make them unique so a
        // perfectly loadable file does not fail the table's no-duplicates rule.
        var seen = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < columns.Length; i++)
        {
            if (seen.Add(columns[i]))
                continue;
            int suffix = 2;
            string candidate;
            do
            {
                candidate = $"{columns[i]}_{suffix}";
                suffix++;
            }
            while (!seen.Add(candidate));
            columns[i] = candidate;
        }

        var table = new DataTable(columns);
        for (int r = hasHeader ? 1 : 0; r < rows.Count; r++)
            table.AddRow(rows[r]);
        return table;
    }

    /// <summary>Writes the table as CSV text, with a header row unless <paramref name="includeHeader"/> is false.</summary>
    public string ToCsv(bool includeHeader = true, char separator = ',')
    {
        var output = new List<string[]>(_rows.Count + 1);
        if (includeHeader)
            output.Add(_columns);
        output.AddRange(_rows);
        return Csv.Write(output, separator);
    }

    private int ColumnOrThrow(string column)
    {
        int index = IndexOfColumn(column);
        if (index < 0)
            throw new ArgumentException($"There is no column '{column}'.", nameof(column));
        return index;
    }
}
