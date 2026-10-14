// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Storage;

/// <summary>
/// Reads and writes comma-separated values, with no system module — a table exported from a tool, a list
/// the user can open in a spreadsheet, a simple data file. A field that holds the separator, a quote or a
/// line break is wrapped in quotes on writing and unwrapped on reading, so the round trip keeps the data
/// intact. Pass a tab for tab-separated values.
/// </summary>
/// <remarks>
/// Rows are separated by a line break (a plain one or a carriage-return pair); fields by the separator.
/// A quote inside a quoted field is written doubled. This is a self-contained calculation, so it works
/// the same on the device and in tests.
/// </remarks>
/// <example>
/// <code>
/// var rows = Csv.Load("/data/scores.csv");
/// foreach (string[] row in rows)
///     Log.Info($"{row[0]} = {row[1]}");
///
/// Csv.Save("/data/out.csv", new[] { new[] { "name", "score" }, new[] { "Ada", "42" } });
/// </code>
/// </example>
public static class Csv
{
    /// <summary>Parses CSV <paramref name="text"/> into rows of fields, splitting on <paramref name="separator"/>.</summary>
    public static List<string[]> Parse(string text, char separator = ',')
    {
        ArgumentNullException.ThrowIfNull(text);
        var rows = new List<string[]>();
        if (text.Length == 0)
            return rows;

        var fields = new List<string>();
        var field = new StringBuilder();
        bool inQuotes = false;
        bool anyContent = false; // whether the current row has started (so a trailing line break adds no empty row)

        for (int i = 0; i < text.Length; i++)
        {
            char c = text[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < text.Length && text[i + 1] == '"')
                    {
                        field.Append('"');
                        i++;
                    }
                    else
                    {
                        inQuotes = false;
                    }
                }
                else
                {
                    field.Append(c);
                }
                continue;
            }

            if (c == '"')
            {
                inQuotes = true;
                anyContent = true;
            }
            else if (c == separator)
            {
                fields.Add(field.ToString());
                field.Clear();
                anyContent = true;
            }
            else if (c == '\n')
            {
                fields.Add(field.ToString());
                field.Clear();
                rows.Add([.. fields]);
                fields.Clear();
                anyContent = false;
            }
            else if (c != '\r')
            {
                field.Append(c);
                anyContent = true;
            }
        }

        // Emit the last row unless the text ended right after a line break.
        if (anyContent || field.Length > 0 || fields.Count > 0)
        {
            fields.Add(field.ToString());
            rows.Add([.. fields]);
        }
        return rows;
    }

    /// <summary>Writes <paramref name="rows"/> of fields as CSV text, quoting fields that need it.</summary>
    public static string Write(IEnumerable<string[]> rows, char separator = ',')
    {
        ArgumentNullException.ThrowIfNull(rows);
        var builder = new StringBuilder();
        foreach (string[] row in rows)
        {
            for (int i = 0; i < row.Length; i++)
            {
                if (i > 0)
                    builder.Append(separator);
                AppendField(builder, row[i], separator);
            }
            builder.Append("\r\n");
        }
        return builder.ToString();
    }

    /// <summary>Reads and parses the CSV file at <paramref name="path"/>, or an empty list when it is absent.</summary>
    public static List<string[]> Load(string path, char separator = ',')
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        return FileSystem.Exists(path) ? Parse(Encoding.UTF8.GetString(FileSystem.ReadAllBytes(path)), separator) : [];
    }

    /// <summary>Writes <paramref name="rows"/> as CSV to the file at <paramref name="path"/>.</summary>
    public static void Save(string path, IEnumerable<string[]> rows, char separator = ',')
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        FileSystem.WriteAllText(path, Write(rows, separator));
    }

    private static void AppendField(StringBuilder builder, string? field, char separator)
    {
        field ??= "";
        bool needsQuotes = field.Contains('"') || field.Contains(separator) || field.Contains('\n') || field.Contains('\r');
        if (!needsQuotes)
        {
            builder.Append(field);
            return;
        }
        builder.Append('"');
        builder.Append(field.Replace("\"", "\"\""));
        builder.Append('"');
    }
}
