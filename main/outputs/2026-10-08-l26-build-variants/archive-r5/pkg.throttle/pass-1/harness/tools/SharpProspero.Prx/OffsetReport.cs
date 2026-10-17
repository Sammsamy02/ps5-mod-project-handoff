// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

using System;
using System.Collections.Generic;
using System.Text;

namespace SharpProspero.Prx;

/// <summary>One symbol the SDK needs from a module, and how it appears in the module read.</summary>
/// <param name="Name">The plain export name the SDK links or resolves.</param>
/// <param name="Nid">The identifier the name maps to (the key the module stores it under).</param>
/// <param name="Present">Whether the module exports it.</param>
/// <param name="IsFunction">True when the export is a function; only meaningful when present.</param>
/// <param name="Address">The export's address within the module; only meaningful when present.</param>
public readonly record struct OffsetSymbol(string Name, string Nid, bool Present, bool IsFunction, ulong Address);

/// <summary>
/// How a module covers what the SDK needs from it: the SDK's catalog library that best matches the
/// module, and, for each name the SDK links or resolves from it, whether the module exports it and at
/// what identifier and address. A non-empty <see cref="Missing"/> on a real system module means that
/// firmware moved or dropped a name the SDK depends on — the fact worth contributing.
/// </summary>
/// <param name="MatchedLibrary">The SDK catalog library the module was matched to.</param>
/// <param name="RequiredCount">How many names the SDK needs from that library.</param>
/// <param name="PresentCount">How many of them the module exports.</param>
/// <param name="Missing">The names the SDK needs that the module does not export.</param>
/// <param name="Symbols">Every needed name, present or not, with its identifier and address.</param>
public sealed record OffsetCoverage(
    string MatchedLibrary,
    int RequiredCount,
    int PresentCount,
    IReadOnlyList<string> Missing,
    IReadOnlyList<OffsetSymbol> Symbols);

/// <summary>
/// What a supplied module contributes to the SDK's per-firmware knowledge: which form it is, the
/// firmware it came from, the version it was built against, its dependencies, its full export table
/// (each identifier and address), and — when asked — how it covers the names the SDK needs. Read a
/// real module off a system and this is the record to contribute so the SDK's registry can be
/// extended to that firmware.
/// </summary>
public sealed class OffsetReport
{
    /// <summary>The file name the report was read from.</summary>
    public required string File { get; init; }

    /// <summary>The on-disk form: <c>unsigned</c>, <c>signed</c>, or <c>signed-encrypted</c>.</summary>
    public required string Container { get; init; }

    /// <summary>The firmware the module was taken from, as the contributor stated it, or null.</summary>
    public string? Firmware { get; init; }

    /// <summary>The system version the module was built against ("MM.mm"), or null when it records none.</summary>
    public string? BuiltAgainst { get; init; }

    /// <summary>The module's own name from its info record, or empty.</summary>
    public string ModuleName { get; init; } = "";

    /// <summary>The version the module's export library publishes.</summary>
    public ushort LibraryVersion { get; init; }

    /// <summary>The module files this module depends on.</summary>
    public IReadOnlyList<string> NeededModules { get; init; } = [];

    /// <summary>Every exported symbol (identifier and address); empty when the export table cannot be read.</summary>
    public IReadOnlyList<PrxExport> Exports { get; init; } = [];

    /// <summary>True when the export table was read; false for an encrypted or non-dynamic file.</summary>
    public bool ExportsReadable { get; init; }

    /// <summary>Why the export table could not be read, when it could not; otherwise null.</summary>
    public string? Note { get; init; }

    /// <summary>Coverage of the SDK's needed names by this module, when it was asked for and readable.</summary>
    public OffsetCoverage? Coverage { get; init; }

    /// <summary>
    /// Reads a module from bytes and builds the report. A signed container is unwrapped first; an
    /// encrypted one is reported without an export table rather than failing.
    /// </summary>
    /// <param name="fileName">The name to record in the report.</param>
    /// <param name="data">The file bytes.</param>
    /// <param name="firmware">The firmware the module came from, as the contributor states it, or null.</param>
    /// <param name="includeCoverage">Whether to compute coverage against the SDK's catalog.</param>
    /// <param name="preferredLibrary">A catalog library to match against, or null to match by best overlap.</param>
    /// <exception cref="PrxFormatException">The bytes are neither an ELF nor a signed container.</exception>
    public static OffsetReport Create(
        string fileName, byte[] data, string? firmware, bool includeCoverage, string? preferredLibrary = null)
    {
        ArgumentNullException.ThrowIfNull(data);

        ModuleForm form = SelfContainer.Classify(data);
        if (form == ModuleForm.Unknown)
            throw new PrxFormatException("File is neither an ELF nor a signed container.");

        if (form == ModuleForm.SignedEncrypted)
        {
            return new OffsetReport
            {
                File = fileName,
                Container = "signed-encrypted",
                Firmware = firmware,
                ExportsReadable = false,
                Note = "encrypted; its exports cannot be read without its key",
            };
        }

        string container = form == ModuleForm.SignedPlaintext ? "signed" : "unsigned";
        byte[] elf = form == ModuleForm.SignedPlaintext ? SelfContainer.ExtractElf(data) : data;

        string? builtAgainst = null;
        try
        {
            uint sdkVersion = PrxImage.ParseSdkVersion(elf);
            if (sdkVersion != 0)
                builtAgainst = PrxImage.FormatSystemVersion(sdkVersion);
        }
        catch (PrxFormatException)
        {
            // The parameter block could not be read; the requirement stays unknown.
        }

        PrxImage? image = null;
        try { image = PrxImage.Parse(elf); }
        catch (PrxFormatException) { /* not a dynamic module: no export table */ }

        if (image is null)
        {
            return new OffsetReport
            {
                File = fileName,
                Container = container,
                Firmware = firmware,
                BuiltAgainst = builtAgainst,
                ExportsReadable = false,
                Note = "not a dynamic module; it carries no export table",
            };
        }

        return new OffsetReport
        {
            File = fileName,
            Container = container,
            Firmware = firmware,
            BuiltAgainst = builtAgainst,
            ModuleName = image.ModuleName,
            LibraryVersion = image.LibraryVersion,
            NeededModules = image.NeededModules,
            Exports = image.Exports,
            ExportsReadable = true,
            Coverage = includeCoverage ? BuildCoverage(image, preferredLibrary) : null,
        };
    }

    /// <summary>
    /// Matches the module to the SDK catalog library it best covers and reports, for each name the SDK
    /// needs from that library, whether the module exports it. Returns null when no catalog library
    /// shares a single export with the module.
    /// </summary>
    private static OffsetCoverage? BuildCoverage(PrxImage image, string? preferredLibrary)
    {
        // The libraries the SDK links against, plus the ones it resolves by name at run time (the
        // installer and USB storage), so a supplied module can be matched against either.
        var candidates = new List<StubCatalog.Entry>(StubCatalog.Core);
        candidates.AddRange(StubCatalog.RuntimeResolved);

        StubCatalog.Entry match = default;
        bool found = false;

        if (!string.IsNullOrEmpty(preferredLibrary))
        {
            foreach (StubCatalog.Entry entry in candidates)
            {
                if (string.Equals(entry.Library, preferredLibrary, StringComparison.OrdinalIgnoreCase))
                {
                    match = entry;
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            int best = 0;
            foreach (StubCatalog.Entry entry in candidates)
            {
                int count = 0;
                foreach (string name in entry.Exports)
                {
                    if (image.FindByName(name) is not null)
                        count++;
                }
                if (count > best)
                {
                    best = count;
                    match = entry;
                    found = true;
                }
            }
            if (best == 0)
                found = false;
        }

        if (!found)
            return null;

        var symbols = new List<OffsetSymbol>(match.Exports.Count);
        var missing = new List<string>();
        int present = 0;
        foreach (string name in match.Exports)
        {
            PrxExport? export = image.FindByName(name);
            if (export is PrxExport e)
            {
                present++;
                symbols.Add(new OffsetSymbol(name, e.Nid, Present: true, e.IsFunction, e.Value));
            }
            else
            {
                missing.Add(name);
                symbols.Add(new OffsetSymbol(name, SceNid.Compute(name), Present: false, IsFunction: false, Address: 0));
            }
        }

        return new OffsetCoverage(match.Library, match.Exports.Count, present, missing, symbols);
    }

    /// <summary>Renders the report for a person to read.</summary>
    public string ToText()
    {
        var sb = new StringBuilder();
        sb.AppendLine($"File:       {File}");
        sb.AppendLine($"Container:  {ContainerText(Container)}");
        sb.AppendLine($"Firmware:   {Firmware ?? "(not stated; pass --firmware NN.NN)"}");
        if (BuiltAgainst is not null)
            sb.AppendLine($"Built for:  {BuiltAgainst}");
        if (!ExportsReadable)
        {
            sb.AppendLine($"Exports:    unavailable - {Note}");
            return sb.ToString();
        }

        if (ModuleName.Length > 0)
            sb.AppendLine($"Module:     {ModuleName} (library version 0x{LibraryVersion:X4})");
        if (NeededModules.Count > 0)
            sb.AppendLine($"Needs:      {string.Join(", ", NeededModules)}");
        sb.AppendLine($"Exports:    {Exports.Count}");
        foreach (PrxExport export in Exports)
            sb.AppendLine($"  {export.Nid}  0x{export.Value:X}  {(export.IsFunction ? "func" : "data")}  {export.LibraryName}");

        if (Coverage is OffsetCoverage coverage)
        {
            sb.AppendLine();
            sb.AppendLine($"Coverage of the SDK's needs from '{coverage.MatchedLibrary}': "
                + $"{coverage.PresentCount}/{coverage.RequiredCount} present.");
            foreach (OffsetSymbol symbol in coverage.Symbols)
            {
                sb.AppendLine(symbol.Present
                    ? $"  [x] {symbol.Name}  {symbol.Nid}  0x{symbol.Address:X}"
                    : $"  [ ] {symbol.Name}  {symbol.Nid}  MISSING");
            }
        }
        return sb.ToString();
    }

    private static string ContainerText(string container) => container switch
    {
        "unsigned" => "unsigned ELF (.elf / .prx)",
        "signed" => "signed (.self / .sprx)",
        "signed-encrypted" => "signed and encrypted (.self / .sprx)",
        _ => container,
    };
}
