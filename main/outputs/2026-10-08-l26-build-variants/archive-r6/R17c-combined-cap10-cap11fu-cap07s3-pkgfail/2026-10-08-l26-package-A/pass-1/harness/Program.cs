using System.Security.Cryptography;
using System.Text.Json;
using SharpProspero.Link;

try
{
    if (args.Length != 3)
        throw new InvalidDataException("Usage: Packager <combined-owned-probe.o> <diagnostic-contract.json> <new-output.bare.prx>");
    string output = Path.GetFullPath(args[2]);
    if (File.Exists(output) || output == Path.GetFullPath(args[0]) || output == Path.GetFullPath(args[1]))
        throw new InvalidDataException("Refuse an existing output or input overwrite.");
    byte[] objectBytes = File.ReadAllBytes(args[0]);
    byte[] contractBytes = File.ReadAllBytes(args[1]);
    if (Convert.ToHexStringLower(SHA256.HashData(contractBytes)) !=
        "514957427bc71e16fe0ee10e9f0d7da8de6aa639e02439929f5dda9242ee3e3f")
        throw new InvalidDataException("Require the exact pinned 191-import diagnostic identity report.");
    using var document = JsonDocument.Parse(contractBytes);
    var root = document.RootElement;
    if (root.GetProperty("schema").GetInt32() != 1 || !root.GetProperty("identity_complete").GetBoolean() ||
        root.GetProperty("counts").GetProperty("required_strong").GetInt32() != 350 ||
        root.GetProperty("counts").GetProperty("required_weak").GetInt32() != 6 ||
        root.GetProperty("counts").GetProperty("identity_covered_strong").GetInt32() != 350 ||
        root.GetProperty("counts").GetProperty("covered_strong").GetInt32() != 350 ||
        root.GetProperty("counts").GetProperty("missing_strong").GetInt32() != 0 ||
        root.GetProperty("counts").GetProperty("conflicts").GetInt32() != 0 ||
        root.GetProperty("diagnostic_extension").GetProperty("original_contract_sha256").GetString() !=
            "6bfa42726ff82c005747c603c6d28bc74912cdcbaeb1b802a6262c2d2c71c4a9")
        throw new InvalidDataException("Require the complete pinned 191-strong diagnostic identity contract.");
    var options = new LinkOptions { Kind = ModuleKind.Library };
    options.ExtraObjects.Add(ElfObjectReader.Read(objectBytes, args[0]));
    var actual = Linker.Resolve(options);
    if (actual.Imports.Count != 0 || actual.SkippedMembers.Count != 0)
        throw new InvalidDataException("Bare resolver unexpectedly imported/skipped an input.");
    var imports = new List<ImportSymbol>();
    var names = new HashSet<string>(StringComparer.Ordinal);
    var types = new Dictionary<string, int>(StringComparer.Ordinal);
    var modules = new Dictionary<string, (string, ushort)>(StringComparer.Ordinal);
    var libraries = new Dictionary<(string, string), ushort>();
    foreach (var row in root.GetProperty("contract").GetProperty("imports").EnumerateArray())
    {
        var imp = row.GetProperty("ImportSymbol");
        string name = imp.GetProperty("Name").GetString()!;
        if (row.GetProperty("binding").GetString() != "GLOBAL" || !names.Add(name))
            throw new InvalidDataException("Contract contains duplicate/non-strong imports.");
        int kind = row.GetProperty("symbol_type").GetString() switch {
            "FUNC" => SymType.Func, "OBJECT" => SymType.Object,
            _ => throw new InvalidDataException("Unsupported contract import kind.") };
        var value = new ImportSymbol(name, imp.GetProperty("ModuleName").GetString()!,
            imp.GetProperty("LibraryName").GetString()!, imp.GetProperty("Soname").GetString()!,
            imp.GetProperty("ModuleVersion").GetUInt16(), imp.GetProperty("LibraryVersion").GetUInt16(),
            imp.GetProperty("PublishedName").GetString()!) { ExplicitType = kind };
        foreach (string text in new[] { value.Name, value.ModuleName, value.LibraryName, value.Soname, value.PublishedName! })
            if (string.IsNullOrEmpty(text) || text.Any(c => c < 32 || c > 126))
                throw new InvalidDataException("Import identity fields must be explicit printable ASCII.");
        var module = (value.ModuleName, value.ModuleVersion);
        var library = (value.Soname, value.LibraryName);
        if ((modules.TryGetValue(value.Soname, out var priorModule) && priorModule != module) ||
            (libraries.TryGetValue(library, out ushort priorVersion) && priorVersion != value.LibraryVersion))
            throw new InvalidDataException("Conflicting import group identities.");
        modules[value.Soname] = module; libraries[library] = value.LibraryVersion;
        types.Add(name, kind); imports.Add(value);
    }
    if (names.Count != 350 || !names.SetEquals(actual.Unresolved) ||
        imports.Count(x => x.ExplicitType == SymType.Func) != 343 ||
        imports.Count(x => x.ExplicitType == SymType.Object) != 7)
        throw new InvalidDataException("Actual unresolved strong names/types must equal the contract's 187 FUNC + 4 OBJECT imports.");
    var boundaries = new HashSet<string>(StringComparer.Ordinal) {
        "__init_array_start", "__init_array_end", "__fini_array_start", "__fini_array_end",
        "__dso_handle", "__eh_frame_start", "__eh_frame_end", "__eh_frame_hdr_start", "__eh_frame_hdr_end", "_DYNAMIC" };
    var allowedWeak = new HashSet<string>(StringComparer.Ordinal) {
        "__dladdr", "__dlclose", "__dlerror", "__dlopen", "__dlsym", "kernel_mprotect" };
    var weak = new HashSet<string>(StringComparer.Ordinal);
    var referencedKinds = new Dictionary<string, HashSet<int>>(StringComparer.Ordinal);
    foreach (var sym in actual.Included.SelectMany(o => o.Symbols).Where(s => s.IsUndefined && s.IsGlobalOrWeak && s.Name.Length > 0))
    {
        if (actual.Defined.ContainsKey(sym.Name)) continue;
        if (sym.IsWeak) {
            if (!allowedWeak.Contains(sym.Name)) throw new InvalidDataException("Unexpected unresolved weak name: " + sym.Name);
            weak.Add(sym.Name); continue;
        }
        if (boundaries.Contains(sym.Name)) continue;
        if (!types.TryGetValue(sym.Name, out int expected))
            throw new InvalidDataException("Unexpected internal/strong undefined name: " + sym.Name);
        if (sym.Type != SymType.NoType && sym.Type != expected)
            throw new InvalidDataException("Actual import reference conflicts with declared kind: " + sym.Name);
        if (!referencedKinds.TryGetValue(sym.Name, out var set)) referencedKinds[sym.Name] = set = new();
        if (sym.Type != SymType.NoType) set.Add(sym.Type);
    }
    if (!weak.SetEquals(allowedWeak))
        throw new InvalidDataException("Actual unresolved weak names must equal the exact six-name diagnostic contract set.");
    // Generated owned references must actually declare every carried kind. An
    // untyped C reference may coexist with that independently typed reference.
    foreach (var pair in types)
        if (!referencedKinds.TryGetValue(pair.Key, out var set) || set.Count != 1 || !set.Contains(pair.Value))
            throw new InvalidDataException("Missing exact typed owned import reference: " + pair.Key);
    var explicitResolution = new LinkResolution {
        Included = actual.Included, Defined = actual.Defined, Imports = imports,
        Unresolved = Array.Empty<string>(), SkippedMembers = actual.SkippedMembers,
        DroppedSections = actual.DroppedSections };
    byte[] bytes = DynamicWriter.Write(explicitResolution, null, ModuleKind.Library,
        exportSymbols: new[] { "module_start", "module_stop" }, moduleFileName: Path.GetFileName(output),
        moduleName: "modbridge_runtime_aligned_entry", exportLibraryName: "modbridge_runtime_aligned_entry",
        libraryInitSymbol: "module_start", libraryFiniSymbol: "module_stop", explicitLifecycleOwnsArrays: true);
    // Preserve object identity across resolution and final write.
    if (!objectBytes.AsSpan().SequenceEqual(File.ReadAllBytes(args[0])) ||
        !contractBytes.AsSpan().SequenceEqual(File.ReadAllBytes(args[1])))
        throw new InvalidDataException("An input changed during packaging.");
    using (var file = new FileStream(output, FileMode.CreateNew, FileAccess.Write, FileShare.None)) file.Write(bytes);
    Console.WriteLine(JsonSerializer.Serialize(new { object_sha256 = Convert.ToHexStringLower(SHA256.HashData(objectBytes)),
        contract_sha256 = Convert.ToHexStringLower(SHA256.HashData(contractBytes)), imports = imports.Count,
        functions = imports.Count(x => x.ExplicitType == SymType.Func), objects = imports.Count(x => x.ExplicitType == SymType.Object),
        unresolved_weak = weak.Order().ToArray(), output, bytes = bytes.Length,
        output_sha256 = Convert.ToHexStringLower(SHA256.HashData(bytes)),
        explicit_init = "module_start", explicit_fini = "module_stop", loader_array_ownership = false,
        runtime_or_hooks_executed = false }));
    return 0;
}
catch (Exception ex) when (ex is InvalidDataException or IOException or JsonException or ElfLinkException or ArgumentException or KeyNotFoundException)
{
    Console.Error.WriteLine("diagnostic packaging refused: " + ex.Message);
    return 2;
}
