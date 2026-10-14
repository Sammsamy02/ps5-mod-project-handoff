// Derived from the retained owned strict GateHost; only export inspection/selection added.
using System.Security.Cryptography;
using System.Text.Json;
using System.Buffers.Binary;
using System.Text;
using SharpProspero.Link;
using SharpProspero.Prx;

// Owned host entry point. No SDK stubs/Core catalog/compat/start objects are added.
// The actual retained resolver decides which strong names remain required.
static class Program
{
    static readonly JsonSerializerOptions Json = new() { WriteIndented = true };
    static readonly Dictionary<string, (byte[] Bytes, string Hash)> Sources = new(StringComparer.Ordinal);
    sealed record Requirement(string name, string binding, string symbol_type);
    static string TypeName(int type) => type switch {
        SymType.Func => "FUNC", SymType.Object => "OBJECT", SymType.NoType => "NOTYPE", _ => "UNSUPPORTED"
    };
    // Verify the exact SDK/owned declaration record, not merely a claimed hash shape.
    // Only ELF header/section/symbol-name/type metadata is read; no symbol values/code.
    static int VerifyDeclaredType(string name, JsonElement record)
    {
        string source = record.GetProperty("evidence").GetProperty("source").GetString()!;
        string expectedHash = record.GetProperty("evidence").GetProperty("sha256").GetString()!;
        if (!Sources.TryGetValue(source, out var input))
        {
            if (new FileInfo(source).Length > 256 * 1024 * 1024) throw new InvalidDataException("Type source too large.");
            byte[] bytes = File.ReadAllBytes(source);
            input = (bytes, Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant());
            Sources[source] = input;
        }
        byte[] data = input.Bytes;
        if (input.Hash != expectedHash || data.Length < 64 || data[0] != 0x7f || data[1] != 'E' ||
            data[2] != 'L' || data[3] != 'F' || data[4] != 2 || data[5] != 1 ||
            BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(18)) != 62)
            throw new InvalidDataException("Type source hash/ELF metadata mismatch.");
        ReadOnlySpan<byte> Span(ulong offset, ulong count) {
            if (offset > (ulong)data.Length || count > (ulong)data.Length - offset)
                throw new InvalidDataException("Type metadata extent is outside serialized source.");
            return data.AsSpan((int)offset, (int)count);
        }
        ulong shoff = BinaryPrimitives.ReadUInt64LittleEndian(Span(40, 8));
        ushort stride = BinaryPrimitives.ReadUInt16LittleEndian(Span(58, 2));
        int count = BinaryPrimitives.ReadUInt16LittleEndian(Span(60, 2));
        if (count == 0) count = checked((int)BinaryPrimitives.ReadUInt64LittleEndian(Span(shoff + 32, 8)));
        if (stride < 64 || count == 0) throw new InvalidDataException("Unsupported type metadata sections.");
        Span(shoff, (ulong)stride * (ulong)count);
        uint requested = record.GetProperty("symbol_table").GetString() switch {
            ".dynsym" => 11, ".symtab" => 2, _ => throw new InvalidDataException("Unsupported declared symbol table.")
        };
        int index = record.GetProperty("symbol_index").GetInt32();
        if (index < 0) throw new InvalidDataException("Invalid declared symbol index.");
        var found = new List<int>();
        for (int i = 0; i < count; i++)
        {
            var section = Span(shoff + (ulong)i * stride, 64);
            if (BinaryPrimitives.ReadUInt32LittleEndian(section[4..]) != requested) continue;
            ulong off = BinaryPrimitives.ReadUInt64LittleEndian(section[24..]);
            ulong size = BinaryPrimitives.ReadUInt64LittleEndian(section[32..]);
            uint link = BinaryPrimitives.ReadUInt32LittleEndian(section[40..]);
            if (link >= count || BinaryPrimitives.ReadUInt64LittleEndian(section[56..]) != 24 ||
                size % 24 != 0 || (ulong)index >= size / 24)
                throw new InvalidDataException("Declared symbol record outside supported table.");
            var symbol = Span(off + (ulong)index * 24, 24);
            uint nameOff = BinaryPrimitives.ReadUInt32LittleEndian(symbol);
            int kind = symbol[4] & 15, binding = symbol[4] >> 4;
            if (kind is not (SymType.Func or SymType.Object) || binding is not (SymBind.Global or SymBind.Weak) ||
                BinaryPrimitives.ReadUInt16LittleEndian(symbol[6..]) == 0)
                throw new InvalidDataException("Type declaration is not a supported defined SDK/owned symbol.");
            var strings = Span(shoff + (ulong)link * stride, 64);
            if (BinaryPrimitives.ReadUInt32LittleEndian(strings[4..]) != ShType.StrTab)
                throw new InvalidDataException("Declared symbol string table is unsupported.");
            var names = Span(BinaryPrimitives.ReadUInt64LittleEndian(strings[24..]),
                             BinaryPrimitives.ReadUInt64LittleEndian(strings[32..]));
            if (nameOff >= names.Length) throw new InvalidDataException("Declared symbol name outside string table.");
            var tail = names[(int)nameOff..]; int end = tail.IndexOf((byte)0);
            if (end < 0 || end > 512 || Encoding.ASCII.GetString(tail[..end]) != name ||
                record.GetProperty("name").GetString() != name || record.GetProperty("symbol_type").GetString() != TypeName(kind))
                throw new InvalidDataException("Exact declared symbol name/type does not match source metadata.");
            found.Add(kind);
        }
        if (found.Count != 1) throw new InvalidDataException("Declared type table is missing/ambiguous.");
        return found[0];
    }
    static (LinkResolution Resolution, List<Requirement> Requirements, string Hash) Resolve(string path)
    {
        byte[] raw = File.ReadAllBytes(path);
        var options = new LinkOptions { Kind = ModuleKind.Library };
        options.ExtraObjects.Add(ElfObjectReader.Read(raw, path));
        LinkResolution result = Linker.Resolve(options);
        if (result.Imports.Count != 0 || result.SkippedMembers.Count != 0)
            throw new InvalidDataException("Bare resolver unexpectedly imported or skipped an input.");
        var symbols = result.Included.SelectMany(o => o.Symbols)
            .Where(s => s.IsUndefined && s.IsGlobalOrWeak && s.Name.Length > 0 && !result.Defined.ContainsKey(s.Name))
            .GroupBy(s => s.Name).ToDictionary(g => g.Key, g => g.ToArray(), StringComparer.Ordinal);
        var strong = result.Unresolved.ToHashSet(StringComparer.Ordinal);
        var requirements = new List<Requirement>();
        foreach (var pair in symbols.OrderBy(p => p.Key, StringComparer.Ordinal))
        {
            bool requiredStrong = strong.Contains(pair.Key);
            if (!requiredStrong && pair.Value.Any(s => !s.IsWeak)) continue; // resolver-provided synthetic names
            var types = pair.Value.Select(s => s.Type).Where(t => t != SymType.NoType).Distinct().ToArray();
            string kind = types.Length == 0 ? "NOTYPE" : types.Length == 1 ? TypeName(types[0]) : "UNSUPPORTED";
            requirements.Add(new(pair.Key, requiredStrong ? "GLOBAL" : "WEAK", kind));
        }
        if (!strong.SetEquals(requirements.Where(r => r.binding == "GLOBAL").Select(r => r.name)))
            throw new InvalidDataException("Unresolved names do not have matching actual symbol declarations.");
        return (result, requirements, Convert.ToHexString(SHA256.HashData(raw)).ToLowerInvariant());
    }

    // Inspect our emitted module; never infer an external provider's identity.
    static int DescribeExports(string module, string namesFile, string output)
    {
        var image = PrxImage.Load(module);
        if (image.ModuleName.Length == 0 || image.ExportLibraryName.Length == 0)
            throw new InvalidDataException("Emitted module lacks its own explicit names.");
        var names = File.ReadAllLines(namesFile).Where(n => n.Length > 0).ToArray();
        if (names.Length == 0 || names.Length > 64 || names.Distinct().Count() != names.Length)
            throw new InvalidDataException("Invalid bounded export request.");
        string hash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(module))).ToLowerInvariant();
        var rows = names.Select(name => {
            var found = image.FindByName(name) ?? throw new InvalidDataException("Required export missing: " + name);
            if (found.LibraryName != image.ExportLibraryName)
                throw new InvalidDataException("Export library disagrees with emitted metadata.");
            return new { name, symbol_type = found.IsFunction ? "FUNC" : "OBJECT",
                identity = new { ModuleName = image.ModuleName, LibraryName = found.LibraryName,
                    Soname = Path.GetFileName(module), ModuleVersion = image.ModuleVersion,
                    LibraryVersion = image.LibraryVersion, PublishedName = name },
                evidence = new { source = Path.GetFullPath(module), sha256 = hash,
                    locator = "export/" + found.Nid, emitted_module_export_verified = true } };
        }).ToArray();
        File.WriteAllText(output, JsonSerializer.Serialize(new {schema = 1, declarations = rows}, Json) + "\n");
        return 0;
    }

    static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && args[0] == "extract")
            {
                if (new FileInfo(args[1]).Length > 256 * 1024 * 1024)
                    throw new InvalidDataException("SELF input too large.");
                byte[] elf = SelfContainer.ExtractElf(File.ReadAllBytes(args[1]));
                using var file = new FileStream(args[2], FileMode.CreateNew, FileAccess.Write, FileShare.None);
                file.Write(elf);
                return 0;
            }
            if (args.Length == 4 && args[0] == "exports") return DescribeExports(args[1], args[2], args[3]);
            if (args.Length < 3 || args[0] is not ("resolve" or "write"))
                throw new ArgumentException("resolve <object> <json>; write <object> <compiler-report> <output-prx-or-dash>");
            var actual = Resolve(args[1]);
            if (args[0] == "resolve")
            {
                var declarations = new Dictionary<string, JsonElement>(StringComparer.Ordinal);
                var requirements = new List<Requirement>();
                using var evidence = args.Length > 3 ? JsonDocument.Parse(File.ReadAllText(args[3])) : null;
                foreach (var requirement in actual.Requirements)
                {
                    var candidates = evidence is null ? new List<JsonElement>() : evidence.RootElement.GetProperty("records")
                        .EnumerateArray().Where(r => r.GetProperty("name").GetString() == requirement.name).ToList();
                    var kinds = candidates.Select(r => VerifyDeclaredType(requirement.name, r)).Distinct().ToArray();
                    if (kinds.Length > 1 || (kinds.Length == 1 && requirement.symbol_type != "NOTYPE" && requirement.symbol_type != TypeName(kinds[0])))
                        throw new InvalidDataException("Independent declared kinds conflict with each other or actual typed reference.");
                    if (kinds.Length == 1) declarations[requirement.name] = candidates[0];
                    requirements.Add(requirement.symbol_type == "NOTYPE" && kinds.Length == 1
                        ? requirement with { symbol_type = TypeName(kinds[0]) } : requirement);
                }
                File.WriteAllText(args[2], JsonSerializer.Serialize(new {
                    schema = 1, required = requirements, raw_required = actual.Requirements,
                    reconciled_type_evidence = declarations,
                    input_object = new { path = Path.GetFullPath(args[1]), sha256 = actual.Hash, bytes = new FileInfo(args[1]).Length },
                    resolver = "Retained Linker.Resolve, no stubs/Core/default identity inference"
                }, Json) + "\n");
                return 0;
            }
            bool childLifecycle = args.Length == 8 && args[5] == "--child-lifecycle";
            if (args.Length != 5 && !childLifecycle)
                throw new ArgumentException("write requires object, report, output and export names file, optionally --child-lifecycle init fini");
            using var document = JsonDocument.Parse(File.ReadAllText(args[2]));
            var root = document.RootElement;
            if (root.GetProperty("input_object").GetProperty("sha256").GetString() != actual.Hash)
                throw new InvalidDataException("Object bytes changed after resolving actual requirements.");
            if (!root.GetProperty("identity_complete").GetBoolean() || root.GetProperty("contract").ValueKind != JsonValueKind.Object)
                throw new InvalidDataException("Contract is incomplete: writer refused.");
            var expected = actual.Requirements.ToDictionary(r => r.name, StringComparer.Ordinal);
            var imports = new List<ImportSymbol>();
            var mapped = new HashSet<string>(StringComparer.Ordinal);
            var modules = new Dictionary<string, (string, ushort)>(StringComparer.Ordinal);
            var libraries = new Dictionary<(string, string), ushort>();
            foreach (var row in root.GetProperty("contract").GetProperty("imports").EnumerateArray())
            {
                var imp = row.GetProperty("ImportSymbol");
                string name = imp.GetProperty("Name").GetString()!;
                string kind = row.GetProperty("symbol_type").GetString()!;
                string expectedKind = expected.TryGetValue(name, out var candidate) ? candidate.symbol_type : "UNSUPPORTED";
                if (expectedKind == "NOTYPE") {
                    if (!root.GetProperty("reconciled_type_evidence").TryGetProperty(name, out var declaration))
                        throw new InvalidDataException("NOTYPE import lacks independently declared kind.");
                    expectedKind = TypeName(VerifyDeclaredType(name, declaration));
                }
                if (!expected.TryGetValue(name, out var requirement) || !mapped.Add(name) ||
                    expectedKind is not ("FUNC" or "OBJECT") || expectedKind != kind ||
                    requirement.binding != row.GetProperty("binding").GetString())
                    throw new InvalidDataException("Contract name/binding/type disagrees with actual resolved input; NOTYPE requires independent type reconciliation.");
                var value = new ImportSymbol(name, imp.GetProperty("ModuleName").GetString()!,
                    imp.GetProperty("LibraryName").GetString()!, imp.GetProperty("Soname").GetString()!,
                    imp.GetProperty("ModuleVersion").GetUInt16(), imp.GetProperty("LibraryVersion").GetUInt16(),
                    imp.GetProperty("PublishedName").GetString()!) {
                        ExplicitType = kind == "OBJECT" ? SymType.Object : SymType.Func
                    };
                var module = (value.ModuleName, value.ModuleVersion);
                var library = (value.Soname, value.LibraryName);
                if ((modules.TryGetValue(value.Soname, out var previousModule) && previousModule != module) ||
                    (libraries.TryGetValue(library, out var previousVersion) && previousVersion != value.LibraryVersion))
                    throw new InvalidDataException("Contract group identity conflicts with the writer's actual keys.");
                modules[value.Soname] = module; libraries[library] = value.LibraryVersion; imports.Add(value);
            }
            if (!actual.Resolution.Unresolved.All(mapped.Contains))
                throw new InvalidDataException("Contract does not cover every actual required strong name.");
            // Clear only after actual name/type/group checks. Preserve all included objects and metadata.
            var explicitResolution = new LinkResolution {
                Included = actual.Resolution.Included, Defined = actual.Resolution.Defined,
                Imports = imports, Unresolved = Array.Empty<string>(),
                SkippedMembers = actual.Resolution.SkippedMembers, DroppedSections = actual.Resolution.DroppedSections
            };
            var exportNames = File.ReadAllLines(args[4]).Where(n => n.Length > 0).ToArray();
            if (exportNames.Length == 0 || exportNames.Length > 64 || exportNames.Distinct().Count() != exportNames.Length)
                throw new InvalidDataException("Invalid bounded export request.");
            foreach (string name in exportNames)
            {
                if (!actual.Resolution.Defined.TryGetValue(name, out var owner) ||
                    !owner.Symbols.Any(s => s.Name == name && !s.IsUndefined && s.Type is SymType.Func or SymType.Object))
                    throw new InvalidDataException("Requested export is not a genuine defined function/data symbol: " + name);
            }
            if (args[3] != "-")
            {
                if (Path.GetFullPath(args[3]) == Path.GetFullPath(args[1]) ||
                    Path.GetFullPath(args[3]) == Path.GetFullPath(args[2]) || File.Exists(args[3]))
                    throw new InvalidDataException("Output would overwrite an existing file or input evidence.");
                byte[] bytes = DynamicWriter.Write(explicitResolution, null, ModuleKind.Library,
                    exportSymbols: exportNames, moduleFileName: Path.GetFileName(args[3]),
                    libraryInitSymbol: childLifecycle ? args[6] : null,
                    libraryFiniSymbol: childLifecycle ? args[7] : null,
                    libraryOwnsArrays: childLifecycle);
                using var output = new FileStream(args[3], FileMode.CreateNew, FileAccess.Write, FileShare.None);
                output.Write(bytes);
            }
            Console.WriteLine(JsonSerializer.Serialize(new { mapped_imports = imports,
                required_strong = actual.Resolution.Unresolved.Count, object_sha256 = actual.Hash,
                writer_invoked = args[3] != "-", runtime_verified = false }, Json));
            return 0;
        }
        catch (Exception exc) when (exc is ArgumentException or InvalidDataException or JsonException or IOException or ElfLinkException or KeyNotFoundException)
        {
            Console.Error.WriteLine("explicit packaging refused: " + exc.Message);
            return 2;
        }
    }
}
