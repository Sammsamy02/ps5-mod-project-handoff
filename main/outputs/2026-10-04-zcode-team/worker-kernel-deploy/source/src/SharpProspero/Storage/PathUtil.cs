// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Storage;

/// <summary>
/// Works with file paths as text: join parts, and pull out the file name, the extension or the folder.
/// Paths use a forward slash between parts, as the device filesystem does, and an absolute path starts
/// with one (for example <c>/app0/data/level.csv</c>). These are plain string operations, so they touch
/// no files.
/// </summary>
public static class PathUtil
{
    /// <summary>The character between parts of a path.</summary>
    public const char Separator = '/';

    /// <summary>
    /// Joins <paramref name="left"/> and <paramref name="right"/> with a single separator. When
    /// <paramref name="right"/> is absolute it is returned as is.
    /// </summary>
    public static string Combine(string left, string right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        if (right.Length > 0 && right[0] == Separator)
            return right;
        if (left.Length == 0)
            return right;
        if (right.Length == 0)
            return left;
        return string.Concat(left.TrimEnd(Separator), Separator.ToString(), right.TrimStart(Separator));
    }

    /// <summary>Joins several parts with separators, in order.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="parts"/> is null.</exception>
    public static string Combine(params string[] parts)
    {
        ArgumentNullException.ThrowIfNull(parts);
        if (parts.Length == 0)
            return "";
        string result = parts[0] ?? "";
        for (int i = 1; i < parts.Length; i++)
            result = Combine(result, parts[i] ?? "");
        return result;
    }

    /// <summary>The part of <paramref name="path"/> after the last separator (the file or folder name).</summary>
    public static string GetFileName(string path)
    {
        ArgumentNullException.ThrowIfNull(path);
        int slash = path.LastIndexOf(Separator);
        return slash < 0 ? path : path[(slash + 1)..];
    }

    /// <summary>The extension of <paramref name="path"/>, including the dot, or an empty string when there is none.</summary>
    public static string GetExtension(string path)
    {
        string name = GetFileName(path);
        int dot = name.LastIndexOf('.');
        return dot > 0 ? name[dot..] : ""; // a leading dot is a hidden name, not an extension
    }

    /// <summary>The file name of <paramref name="path"/> without its extension.</summary>
    public static string GetFileNameWithoutExtension(string path)
    {
        string name = GetFileName(path);
        int dot = name.LastIndexOf('.');
        return dot > 0 ? name[..dot] : name;
    }

    /// <summary>The folder part of <paramref name="path"/> (before the last separator), or an empty string when there is none.</summary>
    public static string GetDirectoryName(string path)
    {
        ArgumentNullException.ThrowIfNull(path);
        int slash = path.LastIndexOf(Separator);
        if (slash < 0)
            return "";
        return slash == 0 ? Separator.ToString() : path[..slash]; // keep the root as "/"
    }

    /// <summary>Whether <paramref name="path"/> has an extension.</summary>
    public static bool HasExtension(string path) => GetExtension(path).Length > 0;

    /// <summary>Whether <paramref name="path"/> is absolute (starts with a separator).</summary>
    public static bool IsAbsolute(string path)
    {
        ArgumentNullException.ThrowIfNull(path);
        return path.Length > 0 && path[0] == Separator;
    }

    /// <summary>
    /// Replaces the extension of <paramref name="path"/> with <paramref name="extension"/> (a leading dot
    /// is added if missing). A null <paramref name="extension"/> removes the extension.
    /// </summary>
    public static string ChangeExtension(string path, string? extension)
    {
        ArgumentNullException.ThrowIfNull(path);
        string current = GetExtension(path);
        string withoutExtension = current.Length > 0 ? path[..^current.Length] : path;
        if (extension is null || extension.Length == 0)
            return withoutExtension;
        return extension[0] == '.' ? withoutExtension + extension : withoutExtension + "." + extension;
    }
}
