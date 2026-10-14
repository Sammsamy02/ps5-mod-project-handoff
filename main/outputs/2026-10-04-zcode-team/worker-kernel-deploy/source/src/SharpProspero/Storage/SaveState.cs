// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;

namespace SharpProspero.Storage;

/// <summary>
/// A versioned save document: a schema version paired with a JSON payload. Writing wraps the payload with
/// its version; reading pulls both back; and <see cref="MigrateTo"/> walks an old save up to the current
/// version through a set of per-version upgrade steps, so a build can load a save written by an earlier one
/// without special-casing every field. It layers over the JSON reader and writer.
/// </summary>
/// <example>
/// <code>
/// var save = new SaveState(2, data);
/// FileSystem.WriteAllText(path, save.Write(indented: true));
/// // later, loading a possibly-older save:
/// SaveState loaded = SaveState.Read(FileSystem.ReadAllText(path)).MigrateTo(2, Migrations);
/// </code>
/// </example>
public sealed class SaveState
{
    /// <summary>Creates a save at <paramref name="version"/> holding <paramref name="data"/>.</summary>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="version"/> is negative.</exception>
    /// <exception cref="ArgumentNullException"><paramref name="data"/> is null.</exception>
    public SaveState(int version, JsonValue data)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(version);
        ArgumentNullException.ThrowIfNull(data);
        Version = version;
        Data = data;
    }

    /// <summary>The schema version the payload was written for.</summary>
    public int Version { get; }

    /// <summary>The payload.</summary>
    public JsonValue Data { get; }

    /// <summary>Serializes the save as JSON, wrapping the payload with its version.</summary>
    public string Write(bool indented = false)
    {
        JsonValue root = JsonValue.NewObject();
        root["version"] = JsonValue.Of(Version);
        root["data"] = Data;
        return root.Write(indented);
    }

    /// <summary>Reads a save written by <see cref="Write"/>.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="json"/> is null.</exception>
    /// <exception cref="JsonException">The text is not a save document (no version field).</exception>
    public static SaveState Read(string json)
    {
        ArgumentNullException.ThrowIfNull(json);
        JsonValue root = JsonValue.Parse(json);
        if (root.Type != JsonType.Object || !root.ContainsKey("version"))
            throw new JsonException("The text is not a save document: it has no version field.");

        // Read the version explicitly rather than through AsInt, which would turn a string, boolean or
        // fractional version into a silent zero and mislead the migration.
        JsonValue versionValue = root["version"];
        if (versionValue.Type != JsonType.Number)
            throw new JsonException("The save's version is not a number.");
        double rawVersion = versionValue.AsNumber();
        if (rawVersion < 0 || rawVersion != Math.Floor(rawVersion))
            throw new JsonException("The save's version must be a whole number that is zero or greater.");

        JsonValue data = root.TryGet("data", out JsonValue payload) ? payload : JsonValue.NewObject();
        return new SaveState((int)rawVersion, data);
    }

    /// <summary>
    /// Brings the save up to <paramref name="targetVersion"/> by applying each upgrade step in turn: the
    /// entry keyed by version <c>v</c> transforms the payload written for <c>v</c> into the payload for
    /// <c>v + 1</c>. Returns a new save at the target version; the original is unchanged.
    /// </summary>
    /// <exception cref="ArgumentNullException"><paramref name="migrations"/> is null.</exception>
    /// <exception cref="ArgumentOutOfRangeException"><paramref name="targetVersion"/> is older than the current version.</exception>
    /// <exception cref="InvalidOperationException">A step is missing, or a step returned null.</exception>
    public SaveState MigrateTo(int targetVersion, IReadOnlyDictionary<int, Func<JsonValue, JsonValue>> migrations)
    {
        ArgumentNullException.ThrowIfNull(migrations);
        if (targetVersion < Version)
            throw new ArgumentOutOfRangeException(nameof(targetVersion), "Cannot migrate to an older version.");

        // Work on a copy so a step that edits its input in place cannot reach back and change this save,
        // and so the returned save never shares its payload with the original — including the no-op path.
        JsonValue data = JsonValue.Parse(Data.Write());
        int version = Version;
        while (version < targetVersion)
        {
            if (!migrations.TryGetValue(version, out Func<JsonValue, JsonValue>? step))
                throw new InvalidOperationException($"No migration step is registered from version {version}.");
            data = step(data) ?? throw new InvalidOperationException($"The migration from version {version} returned null.");
            version++;
        }

        return new SaveState(version, data);
    }
}
