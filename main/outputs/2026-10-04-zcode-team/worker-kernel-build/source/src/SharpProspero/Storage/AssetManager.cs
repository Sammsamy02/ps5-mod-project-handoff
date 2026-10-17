// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.IO;

namespace SharpProspero.Storage;

/// <summary>
/// One path space over several sources, so a build asks for an asset by a logical name and does not care
/// whether it comes from a folder in the package, a tar archive bundled with the title, or bytes built at
/// runtime. Mount the sources once, then read by name: the bytes are read on first use and kept, and a
/// decoded asset (an image, a font, a level) is decoded once and kept too, so asking again is free. Later
/// mounts cover earlier ones for the same name, which lets a patch or a user folder override the base
/// content.
/// </summary>
/// <remarks>
/// It builds on the existing readers rather than replacing them: a folder mount reads through
/// <see cref="FileSystem"/> (so it reaches the package and the writable folders), and an archive mount
/// reads a tar through <see cref="TarArchive"/>. It is not thread-safe; load from one thread, or guard it
/// yourself.
/// </remarks>
/// <example>
/// <code>
/// var assets = new AssetManager();
/// assets.MountDirectory("/app0/assets");            // the package's assets folder, at the root
/// assets.MountArchive(File.bytes, prefix: "levels"); // a tar, reachable as levels/...
/// Image title = assets.Load("ui/title.bmp", BmpImage.Load);
/// byte[] level = assets.ReadBytes("levels/world1.dat");
/// </code>
/// </example>
public sealed class AssetManager
{
    private readonly List<Mount> _mounts = [];
    private readonly Dictionary<string, byte[]> _bytes = new(StringComparer.Ordinal);
    private readonly Dictionary<(string Path, Type Type), object> _decoded = [];
    private Mount? _memory;

    /// <summary>The logical names of the assets read so far and still held.</summary>
    public IReadOnlyCollection<string> CachedPaths => _bytes.Keys;

    /// <summary>
    /// Mounts a device folder, so a name under <paramref name="prefix"/> reads the matching file beneath
    /// <paramref name="devicePath"/>. With the default empty prefix the folder sits at the root of the
    /// path space.
    /// </summary>
    public void MountDirectory(string devicePath, string prefix = "")
    {
        ArgumentNullException.ThrowIfNull(devicePath);
        _mounts.Add(new Mount(Normalize(prefix)) { DirectoryRoot = devicePath.TrimEnd('/') });
    }

    /// <summary>
    /// Mounts the files inside a tar archive, so each is reachable by its path within the archive under
    /// <paramref name="prefix"/>. The archive is read once, here.
    /// </summary>
    public void MountArchive(ReadOnlySpan<byte> tarData, string prefix = "")
    {
        var entries = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        foreach (TarEntry entry in TarArchive.Read(tarData))
        {
            if (!entry.IsDirectory)
                entries[Normalize(entry.Name)] = entry.Data;
        }
        _mounts.Add(new Mount(Normalize(prefix)) { Entries = entries });
    }

    /// <summary>Adds a single asset held in memory, reachable by <paramref name="path"/>. Overwrites one already added.</summary>
    public void AddFile(string path, byte[] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (_memory is null)
        {
            _memory = new Mount("") { Entries = new Dictionary<string, byte[]>(StringComparer.Ordinal) };
            _mounts.Add(_memory);
        }
        _memory.Entries![Normalize(path)] = data;
        // A replaced asset invalidates anything cached under that name.
        Unload(path);
    }

    /// <summary>Whether an asset named <paramref name="path"/> can be read from any mount.</summary>
    public bool Exists(string path)
    {
        string key = Normalize(path);
        if (_bytes.ContainsKey(key))
            return true;
        for (int i = _mounts.Count - 1; i >= 0; i--)
            if (Under(_mounts[i].Prefix, key, out string relative) && _mounts[i].Has(relative))
                return true;
        return false;
    }

    /// <summary>Reads the raw bytes of <paramref name="path"/>, from the cache after the first read.</summary>
    /// <exception cref="FileNotFoundException">No mount provides the asset.</exception>
    public byte[] ReadBytes(string path)
    {
        if (TryReadBytes(path, out byte[] data))
            return data;
        throw new FileNotFoundException($"No mounted source provides the asset '{path}'.", path);
    }

    /// <summary>Reads the raw bytes of <paramref name="path"/> if a mount provides it; caches them on success.</summary>
    public bool TryReadBytes(string path, out byte[] data)
    {
        string key = Normalize(path);
        if (_bytes.TryGetValue(key, out byte[]? cached))
        {
            data = cached;
            return true;
        }
        // Newest mount first, so a later mount covers an earlier one for the same name.
        for (int i = _mounts.Count - 1; i >= 0; i--)
        {
            if (Under(_mounts[i].Prefix, key, out string relative) && _mounts[i].TryRead(relative, out byte[] raw))
            {
                _bytes[key] = raw;
                data = raw;
                return true;
            }
        }
        data = [];
        return false;
    }

    /// <summary>
    /// Reads and decodes <paramref name="path"/> with <paramref name="decode"/>, keeping the decoded asset
    /// so a later call for the same name and type returns it without decoding again. Pass a decoder such
    /// as <c>BmpImage.Load</c> or your own.
    /// </summary>
    /// <exception cref="FileNotFoundException">No mount provides the asset.</exception>
    public T Load<T>(string path, Func<byte[], T> decode)
    {
        ArgumentNullException.ThrowIfNull(decode);
        string key = Normalize(path);
        (string, Type) typed = (key, typeof(T));
        if (_decoded.TryGetValue(typed, out object? already))
            return (T)already;

        T asset = decode(ReadBytes(key));
        _decoded[typed] = asset!;
        return asset;
    }

    /// <summary>Forgets the cached bytes and decoded assets for <paramref name="path"/>, so the next read loads again.</summary>
    public void Unload(string path)
    {
        string key = Normalize(path);
        _bytes.Remove(key);
        // Drop every decoded type held under this name.
        var stale = new List<(string, Type)>();
        foreach ((string p, Type t) in _decoded.Keys)
            if (p == key)
                stale.Add((p, t));
        foreach ((string, Type) entry in stale)
            _decoded.Remove(entry);
    }

    /// <summary>Forgets every cached byte buffer and decoded asset, keeping the mounts.</summary>
    public void ClearCache()
    {
        _bytes.Clear();
        _decoded.Clear();
    }

    // Normalizes a logical path: forward slashes, no leading slash, no trailing slash.
    private static string Normalize(string path)
    {
        if (string.IsNullOrEmpty(path))
            return "";
        return path.Replace('\\', '/').Trim('/');
    }

    // Whether `path` sits under `prefix`, and if so the part of it below the prefix.
    private static bool Under(string prefix, string path, out string relative)
    {
        if (prefix.Length == 0)
        {
            relative = path;
            return true;
        }
        if (path == prefix)
        {
            relative = "";
            return true;
        }
        if (path.StartsWith(prefix + "/", StringComparison.Ordinal))
        {
            relative = path[(prefix.Length + 1)..];
            return true;
        }
        relative = "";
        return false;
    }

    // One mounted source: either a device folder or a set of in-memory entries keyed by relative path.
    private sealed class Mount(string prefix)
    {
        public string Prefix { get; } = prefix;
        public string? DirectoryRoot { get; init; }
        public Dictionary<string, byte[]>? Entries { get; init; }

        public bool Has(string relative)
            => DirectoryRoot is not null ? FileSystem.Exists(DevicePath(relative)) : Entries!.ContainsKey(relative);

        public bool TryRead(string relative, out byte[] data)
        {
            if (DirectoryRoot is not null)
            {
                string devicePath = DevicePath(relative);
                if (FileSystem.Exists(devicePath))
                {
                    data = FileSystem.ReadAllBytes(devicePath);
                    return true;
                }
                data = [];
                return false;
            }
            if (Entries!.TryGetValue(relative, out byte[]? entry))
            {
                data = entry;
                return true;
            }
            data = [];
            return false;
        }

        private string DevicePath(string relative)
            => relative.Length == 0 ? DirectoryRoot! : DirectoryRoot + "/" + relative;
    }
}
