// SharpProspero.Packager - packs a built module folder into an installable package.
// Copyright (C) 2026 SvenGDK
//
// Thin command-line front end over the package builder. Point it at a folder that holds the
// compiled eboot.bin and an optional sce_sys metadata tree, and it writes the finished *.pkg.

using LibProsperoPkg;
using LibProsperoPkg.Content;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;

namespace SharpProspero.Packager;

internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length == 0 || HasFlag(args, "--help") || HasFlag(args, "-h"))
        {
            PrintUsage();
            return args.Length == 0 ? 1 : 0;
        }

        string? input = GetOption(args, "--in");
        string? output = GetOption(args, "--out");

        if (string.IsNullOrWhiteSpace(input) || string.IsNullOrWhiteSpace(output))
        {
            Console.Error.WriteLine("Both --in and --out are required.");
            PrintUsage();
            return 1;
        }

        var options = new ProsperoHomebrewPackageOptions
        {
            HomebrewFolder = input,
            OutputFolder = output,
            ModuleName = GetOption(args, "--module") ?? "eboot.bin",
            ContentId = GetOption(args, "--content-id") ?? "",
            Passcode = GetOption(args, "--passcode") ?? new string('0', 32),
            Title = GetOption(args, "--title") ?? "",
            Version = GetOption(args, "--version") ?? "",
            KeepStaging = HasFlag(args, "--keep-staging"),
        };

        try
        {
            Directory.CreateDirectory(output);
            ProsperoHomebrewPackageResult result = ProsperoHomebrewPackager.Package(
                options, message => Console.WriteLine($"  {message}"));

            Console.WriteLine();
            Console.WriteLine($"Package: {result.OutputPath}");
            Console.WriteLine($"Module:  {result.ModulePath}");
            // The result names the module; the copy to inspect is the one gathered in the input folder.
            // The module to inspect is the one gathered in the input folder. The packer answers with a
            // path that may be relative and may be only a file name, and the name is whatever --module
            // asked for, so the two are joined rather than assuming the default name sits at the root.
            string gathered = Path.IsPathRooted(result.ModulePath)
                ? result.ModulePath
                : Path.Combine(input, result.ModulePath);
            if (!File.Exists(gathered))
                gathered = Path.Combine(input, Path.GetFileName(result.ModulePath));
            PrintReadiness(result.LaunchReadiness, gathered);
            PrintWarnings(result.Warnings);
            return 0;
        }
        catch (ArgumentException ex)
        {
            Console.Error.WriteLine($"Invalid input: {ex.Message}");
            return 2;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            Console.Error.WriteLine($"File error: {ex.Message}");
            return 3;
        }
        catch (Exception ex)
        {
            // The packing library builds its tables from the compressed shape of the whole staged tree,
            // and a shape it does not handle surfaces here as whatever it threw. Reported raw that is a
            // managed stack trace and nothing an author can act on, so it is named and turned into an
            // exit code of its own; the folder output is unaffected and remains the way to get a module
            // onto a console meanwhile.
            Console.Error.WriteLine($"The package could not be built: {ex.GetType().Name}: {ex.Message}");
            Console.Error.WriteLine("  This is a limit of the packing step, not of the module. The module");
            Console.Error.WriteLine("  folder itself is complete and can be copied to a console as it is.");
            Console.Error.WriteLine("  Building through build-app.ps1 with -Output Folder skips this step.");
            return 4;
        }
    }

    // Reports whether the package will start, so the author knows before they copy it to a console.
    // The module's own form is checked here rather than taken from the packer: the packer recognizes an
    // older container form, so it reads a module wrapped for this platform as unreadable. Everything
    // else in the report - the metadata the launch service needs - is the packer's to judge.
    private static void PrintReadiness(ProsperoLaunchReadinessReport report, string modulePath)
    {
        ModuleForm form = ReadModuleForm(modulePath);
        bool moduleReady = form == ModuleForm.Wrapped;
        bool ready = moduleReady && report.HasEboot && report.HasParamJson && !report.HasParamSfo;

        Console.WriteLine();
        Console.WriteLine($"Launch readiness: {(ready ? "ready" : "not ready")}");
        Console.WriteLine($"  eboot.bin:   {(report.HasEboot ? "present" : "missing")}");
        Console.WriteLine($"  param.json:  {(report.HasParamJson ? "present" : "missing")}");
        if (report.HasParamSfo)
            Console.WriteLine("  param.sfo:   present (the launch service refuses this metadata form)");
        Console.WriteLine($"  module:      {DescribeForm(form)}");
        if (!moduleReady)
        {
            Console.WriteLine("    A module has to be wrapped for the loader to accept it. Build through");
            Console.WriteLine("    build-app.ps1, or wrap it with: sharpprospero-bindgen self --sign --in <module> --out <module>");
        }
        if (report.Issues.Count > 0)
        {
            Console.WriteLine("  Packer notes:");
            foreach (string issue in report.Issues)
                Console.WriteLine($"    - {issue}");
        }
    }

    /// <summary>How a gathered module is stored, read from its first bytes.</summary>
    private enum ModuleForm { Unknown, PlainElf, Wrapped, WrappedSealed }

    // The two container marks, then the ELF magic. Both marks are wrapped modules - titles that run
    // ship each - and whether a wrapped module can be read is a per-segment property (bit 1 of each
    // segment's flags word), not a different header.
    private const uint ContainerMagic = 0xEEF51454;
    private const uint AlternateContainerMagic = 0x1D3D154F;
    private const uint ElfMagic = 0x464C457F;

    private static ModuleForm ReadModuleForm(string modulePath)
    {
        byte[] data;
        try { data = File.ReadAllBytes(modulePath); }
        catch (IOException) { return ModuleForm.Unknown; }
        catch (UnauthorizedAccessException) { return ModuleForm.Unknown; }

        if (data.Length < 0x20)
            return ModuleForm.Unknown;
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(data);
        if (magic == ElfMagic)
            return ModuleForm.PlainElf;
        if (magic != ContainerMagic && magic != AlternateContainerMagic)
            return ModuleForm.Unknown;

        int segCount = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(0x18));
        for (int i = 0; i < segCount; i++)
        {
            int entry = 0x20 + i * 0x20;
            if (entry + 0x20 > data.Length)
                break;
            if ((BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(entry)) & 0x2) != 0)
                return ModuleForm.WrappedSealed;
        }
        return ModuleForm.Wrapped;
    }

    private static string DescribeForm(ModuleForm form) => form switch
    {
        ModuleForm.Wrapped => "wrapped for the loader, contents readable",
        ModuleForm.WrappedSealed => "wrapped, contents sealed (it needs the key path)",
        ModuleForm.PlainElf => "a plain ELF - the loader turns this away before it runs",
        _ => "not a form this reads",
    };

    private static void PrintWarnings(IReadOnlyList<string> warnings)
    {
        if (warnings.Count == 0)
            return;
        Console.WriteLine();
        Console.WriteLine("Warnings:");
        foreach (string warning in warnings)
            Console.WriteLine($"  - {warning}");
    }

    private static string? GetOption(string[] args, string name)
    {
        for (int i = 0; i < args.Length - 1; i++)
        {
            if (string.Equals(args[i], name, StringComparison.Ordinal))
                return args[i + 1];
        }
        return null;
    }

    private static bool HasFlag(string[] args, string name)
        => Array.Exists(args, a => string.Equals(a, name, StringComparison.Ordinal));

    private static void PrintUsage()
    {
        Console.WriteLine("Usage: sharpprospero-pack --in <module-folder> --out <output-folder> [options]");
        Console.WriteLine();
        Console.WriteLine("  --in <folder>          Folder with the compiled eboot.bin and optional sce_sys/.");
        Console.WriteLine("  --out <folder>         Folder the finished *.pkg is written to.");
        Console.WriteLine("  --module <name>        Module file name. Default: eboot.bin.");
        Console.WriteLine("  --content-id <id>      36-character content id. Default: read from param.json.");
        Console.WriteLine("  --passcode <32 chars>  Package passcode. Default: all zeros.");
        Console.WriteLine("  --title <name>         Display title. Default: read from param.json.");
        Console.WriteLine("  --version <NN.NN>      Content version. Default: read from param.json or 01.00.");
        Console.WriteLine("  --keep-staging         Keep the assembled source tree after the build.");
    }
}
