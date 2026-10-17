// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.IO;

namespace SharpProspero.Prx;

/// <summary>How the system version an application requires is settled.</summary>
public enum SystemVersionPolicy
{
    /// <summary>
    /// Require what the shipped modules need. The requirement rises to the highest any module was
    /// built against and is never lowered. This is the safe default.
    /// </summary>
    Match,

    /// <summary>Raise the requirement to a version you name. Refuses to lower it.</summary>
    Upgrade,

    /// <summary>
    /// Lower the requirement to a version you name. Reports every module that was built against
    /// something newer, because those modules will not load on the system you are now naming.
    /// </summary>
    Downgrade,

    /// <summary>Leave the requirement alone. Still reports a module that needs more than it.</summary>
    Keep,
}

/// <summary>What one module was built against.</summary>
/// <param name="FileName">The module's file name.</param>
/// <param name="Version">The system it needs, or the absent version when it records none.</param>
public readonly record struct ModuleRequirement(string FileName, SystemVersion Version);

/// <summary>Every module found under a folder, and the ones that could not be read.</summary>
/// <param name="Modules">Each module and what it needs, ordered by file name.</param>
/// <param name="Unreadable">
/// Files that look like modules but could not be read. These carry a requirement that cannot be
/// honoured, so they are reported rather than passed over.
/// </param>
public sealed record ModuleScan(IReadOnlyList<ModuleRequirement> Modules, IReadOnlyList<string> Unreadable);

/// <summary>
/// The system version an application ends up requiring, and how it got there. Produced by
/// <see cref="SystemVersionPlanner.Plan"/>.
/// </summary>
public sealed class SystemVersionPlan
{
    /// <summary>The requirement the application carries now.</summary>
    public required SystemVersion Current { get; init; }

    /// <summary>The highest version any shipped module was built against.</summary>
    public required SystemVersion Needed { get; init; }

    /// <summary>The requirement to write.</summary>
    public required SystemVersion Result { get; init; }

    /// <summary>Every module that was read, and what it needs.</summary>
    public required IReadOnlyList<ModuleRequirement> Modules { get; init; }

    /// <summary>Files sitting where a module goes that could not be read, so did not take part.</summary>
    public required IReadOnlyList<string> Unreadable { get; init; }

    /// <summary>Modules that will not load under <see cref="Result"/>, because they need more.</summary>
    public required IReadOnlyList<ModuleRequirement> Unloadable { get; init; }

    /// <summary>Notes worth showing the user.</summary>
    public required IReadOnlyList<string> Messages { get; init; }

    /// <summary>True when <see cref="Result"/> differs from <see cref="Current"/>.</summary>
    public bool Changed => Result != Current;
}

/// <summary>
/// Settles the system version an application requires against the modules it ships. A module records
/// the system it was built against; the application has to require at least as much, or the system
/// installs the application and then fails to load the module.
/// </summary>
public static class SystemVersionPlanner
{
    private static readonly string[] ModulePatterns = ["*.prx", "*.sprx"];

    /// <summary>
    /// Reads every module under <paramref name="folder"/> and reports what each was built against.
    /// Reading a requirement needs only the module's program headers, so a module whose exports
    /// cannot be read still answers. Results are ordered by file name.
    /// </summary>
    /// <exception cref="ArgumentException"><paramref name="folder"/> is null or empty.</exception>
    public static ModuleScan ScanModules(string folder)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(folder);
        var found = new List<ModuleRequirement>();
        var unreadable = new List<string>();
        if (!Directory.Exists(folder))
            return new ModuleScan(found, unreadable);

        foreach (string pattern in ModulePatterns)
        {
            foreach (string path in Directory.EnumerateFiles(folder, pattern, SearchOption.AllDirectories))
                Add(path, named: true);
        }

        // The application's own module carries a requirement the same way a library does.
        string eboot = Path.Combine(folder, "eboot.bin");
        if (File.Exists(eboot))
            Add(eboot, named: true);

        found.Sort((a, b) => string.CompareOrdinal(a.FileName, b.FileName));
        unreadable.Sort(StringComparer.Ordinal);
        return new ModuleScan(found, unreadable);

        void Add(string path, bool named)
        {
            uint sdkVersion;
            try
            {
                // A signed container (.sprx) is unwrapped to its embedded ELF; a plain module reads
                // directly. Either way the requirement comes from the module's program headers.
                sdkVersion = PrxImage.ParseSdkVersion(ModuleFile.Read(path).Elf);
            }
            catch (Exception ex) when (ex is PrxFormatException or IOException or UnauthorizedAccessException)
            {
                // The file sits where a module goes but cannot be read as one. Its requirement is
                // unknown rather than absent, so it is reported instead of passed over: passing over
                // it would let the application require less than the module needs, which is the exact
                // failure this check exists to catch.
                if (named)
                    unreadable.Add(Path.GetFileName(path));
                return;
            }
            found.Add(new ModuleRequirement(Path.GetFileName(path), SystemVersion.FromModuleSdkVersion(sdkVersion)));
        }
    }

    /// <summary>
    /// Works out the requirement to write, from the modules under <paramref name="folder"/>, the
    /// requirement the application carries now, and the <paramref name="policy"/>.
    /// </summary>
    /// <param name="folder">The folder holding the built module and its libraries.</param>
    /// <param name="current">The requirement the application carries now, in either form. May be empty.</param>
    /// <param name="policy">How to settle it.</param>
    /// <param name="target">
    /// The version to move to. Required by <see cref="SystemVersionPolicy.Upgrade"/> and
    /// <see cref="SystemVersionPolicy.Downgrade"/>; ignored by the others.
    /// </param>
    /// <exception cref="ArgumentException">
    /// <paramref name="policy"/> names a version to move to but <paramref name="target"/> is absent,
    /// or the move goes the wrong way.
    /// </exception>
    public static SystemVersionPlan Plan(
        string folder, string? current, SystemVersionPolicy policy, SystemVersion target = default)
    {
        ModuleScan scan = ScanModules(folder);
        IReadOnlyList<ModuleRequirement> modules = scan.Modules;
        SystemVersion.TryParse(current, out SystemVersion currentVersion);

        SystemVersion needed = SystemVersion.None;
        foreach (ModuleRequirement module in modules)
        {
            if (module.Version > needed)
                needed = module.Version;
        }

        var messages = new List<string>();
        SystemVersion result;

        switch (policy)
        {
            case SystemVersionPolicy.Match:
                result = needed > currentVersion ? needed : currentVersion;
                if (result > currentVersion)
                    messages.Add($"Raised to {result} for {NameOf(modules, needed)}.");
                break;

            case SystemVersionPolicy.Upgrade:
                if (!target.HasValue)
                    throw new ArgumentException("Upgrading needs the version to move to.", nameof(target));
                if (target < currentVersion)
                    throw new ArgumentException(
                        $"{target} is below the current requirement of {currentVersion}, so it is not an upgrade. Use the downgrade policy to lower it.",
                        nameof(target));
                result = target;
                messages.Add($"Raised to {result}.");
                break;

            case SystemVersionPolicy.Downgrade:
                if (!target.HasValue)
                    throw new ArgumentException("Downgrading needs the version to move to.", nameof(target));
                if (currentVersion.HasValue && target > currentVersion)
                    throw new ArgumentException(
                        $"{target} is above the current requirement of {currentVersion}, so it is not a downgrade. Use the upgrade policy to raise it.",
                        nameof(target));
                result = target;
                messages.Add($"Lowered to {result}.");
                break;

            case SystemVersionPolicy.Keep:
                result = currentVersion;
                break;

            default:
                throw new ArgumentOutOfRangeException(nameof(policy), policy, "Unknown policy.");
        }

        // Whatever the policy decided, a module built against more than the result will not load. This
        // is the whole point of the check, so it is reported for every policy including the one that
        // caused it.
        var unloadable = new List<ModuleRequirement>();
        foreach (ModuleRequirement module in modules)
        {
            if (module.Version > result)
                unloadable.Add(module);
        }

        foreach (ModuleRequirement module in unloadable)
            messages.Add($"{module.FileName} needs {module.Version} and will not load under {result}.");

        foreach (string name in scan.Unreadable)
            messages.Add($"{name} could not be read, so what it needs is unknown and was not taken into account.");

        return new SystemVersionPlan
        {
            Current = currentVersion,
            Needed = needed,
            Result = result,
            Modules = modules,
            Unreadable = scan.Unreadable,
            Unloadable = unloadable,
            Messages = messages,
        };
    }

    private static string NameOf(IReadOnlyList<ModuleRequirement> modules, SystemVersion version)
    {
        foreach (ModuleRequirement module in modules)
        {
            if (module.Version == version)
                return module.FileName;
        }
        return "the modules it ships";
    }
}
