// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK
//
// Reads relocatable objects and archives, and resolves the symbol graph: which object defines each
// symbol, which archive members are pulled in to satisfy a reference, and which symbols are left for
// an imported module to provide. This is the input side of producing a module. The layout,
// relocation, and file-writing phases build on the resolved graph.

using System;
using System.Collections.Generic;
using System.IO;

namespace SharpProspero.Link;

/// <summary>Whether the output is an application or a library module.</summary>
public enum ModuleKind { Executable, Library }

/// <summary>Inputs to a link.</summary>
public sealed class LinkOptions
{
    /// <summary>Objects always included in the output.</summary>
    public List<string> Objects { get; } = [];

    /// <summary>Archives whose members are pulled in only to satisfy a reference.</summary>
    public List<string> Archives { get; } = [];

    /// <summary>Stub libraries that name the modules a reference can be imported from.</summary>
    public List<string> Stubs { get; } = [];

    /// <summary>Objects the SDK supplies directly, always included (e.g. the start object).</summary>
    public List<ElfObject> ExtraObjects { get; } = [];

    /// <summary>Stub libraries the SDK supplies directly, without a file on disk.</summary>
    public List<StubLibrary> ExtraStubs { get; } = [];

    /// <summary>The output module kind.</summary>
    public ModuleKind Kind { get; set; } = ModuleKind.Executable;
}

/// <summary>A reference satisfied by an imported module rather than an included object.</summary>
/// <param name="Name">The plain symbol name.</param>
/// <param name="ModuleName">The module name the providing module publishes, e.g. <c>libkernel</c>.</param>
/// <param name="LibraryName">The library name the providing module publishes, usually the same as the module name.</param>
/// <param name="Soname">The module file name the loader loads, e.g. <c>libkernel.prx</c>.</param>
/// <param name="ModuleVersion">The module version to record, taken from the stub that provides the name.</param>
/// <param name="LibraryVersion">The library version to record, taken from the same stub.</param>
/// <param name="PublishedName">
/// The name the providing module publishes, when the reference reaches it under another one. Null
/// means the reference and the published name are the same, which is the usual case. See
/// <see cref="Linker.DeviceAliasPrefix"/> for why the two ever differ.
/// </param>
public readonly record struct ImportSymbol(
    string Name,
    string ModuleName,
    string LibraryName,
    string Soname,
    ushort ModuleVersion = StubLibrary.DefaultModuleVersion,
    ushort LibraryVersion = StubLibrary.DefaultLibraryVersion,
    string? PublishedName = null);

/// <summary>The resolved symbol graph.</summary>
public sealed class LinkResolution
{
    /// <summary>Objects included in the output, in order.</summary>
    public required IReadOnlyList<ElfObject> Included { get; init; }

    /// <summary>Global symbol name to the object that defines it.</summary>
    public required IReadOnlyDictionary<string, ElfObject> Defined { get; init; }

    /// <summary>References a stub library provides, resolved to their module.</summary>
    public required IReadOnlyList<ImportSymbol> Imports { get; init; }

    /// <summary>References neither an included object nor a stub provides.</summary>
    public required IReadOnlyList<string> Unresolved { get; init; }

    /// <summary>Archive members the reader could not parse, with the reason. An unexpected entry here
    /// hides the symbols that member would have defined, so it is surfaced rather than passed over.</summary>
    public IReadOnlyList<string> SkippedMembers { get; init; } = [];

    /// <summary>
    /// Sections left out because another included object carried the same shared group first. Anything
    /// that lays sections out has to pass over these, and a symbol defined in one is reached through the
    /// copy that was kept.
    /// </summary>
    public IReadOnlySet<(ElfObject Object, int Section)> DroppedSections { get; init; } =
        new HashSet<(ElfObject, int)>();
}

/// <summary>The input side of the linker: read objects and archives and resolve the symbol graph.</summary>
public static class Linker
{
    /// <summary>
    /// Resolves the symbol graph for <paramref name="options"/>: reads the objects, pulls the archive
    /// members that satisfy an otherwise-undefined reference, and reports what is defined and what is
    /// left for an imported module.
    /// </summary>
    public static LinkResolution Resolve(LinkOptions options)
    {
        ArgumentNullException.ThrowIfNull(options);

        var included = new List<ElfObject>();
        var defined = new Dictionary<string, ElfObject>(StringComparer.Ordinal);
        var undefined = new HashSet<string>(StringComparer.Ordinal);
        // Names referenced by at least one strong (non-weak) undefined symbol. A reference that is only
        // ever weak and stays unsatisfied is a legal binding to address zero, not a link error.
        var strongUndefined = new HashSet<string>(StringComparer.Ordinal);
        // Signatures already kept, and the sections dropped because another object carried them first.
        var groupsKept = new HashSet<string>(StringComparer.Ordinal);
        var dropped = new HashSet<(ElfObject, int)>();
        // Whether the recorded definition of each name is one that may be replaced.
        var weakDefinition = new Dictionary<string, bool>(StringComparer.Ordinal);

        foreach (string path in options.Objects)
            Include(ElfObjectReader.Read(File.ReadAllBytes(path), path), included, defined, undefined, strongUndefined, groupsKept, dropped, weakDefinition);
        foreach (ElfObject extra in options.ExtraObjects)
            Include(extra, included, defined, undefined, strongUndefined, groupsKept, dropped, weakDefinition);

        // Undefined names the objects the link is built from ask for directly. These always have to
        // resolve. Names an archive member only declares are held to the stricter test below (a
        // relocation has to reference them), so an incidental declaration in a pulled member is not an
        // error the way a genuine reference is.
        var primaryUndefined = new HashSet<string>(strongUndefined, StringComparer.Ordinal);

        // Load the archive members and index which global symbols each one defines.
        var pending = new List<(ElfObject Object, HashSet<string> Defines)>();
        var skipped = new List<string>();
        foreach (string archivePath in options.Archives)
        {
            byte[] bytes = File.ReadAllBytes(archivePath);
            foreach (ArMember member in ArReader.Read(bytes, Path.GetFileName(archivePath)))
            {
                ElfObject obj;
                try { obj = ElfObjectReader.Read(member.Data, member.Name); }
                catch (ElfLinkException ex) { skipped.Add($"{Path.GetFileName(archivePath)}({member.Name}): {ex.Message}"); continue; }
                // A file that is an object rather than an archive is named on the command line, and a
                // named object is taken whatever else is in the link. Leaving it to the rule below
                // drops an object whose only reason to be there is a constructor list, because nothing
                // asks for a name it defines.
                if (member.IsWholeFile)
                {
                    Include(obj, included, defined, undefined, strongUndefined, groupsKept, dropped, weakDefinition);
                    continue;
                }
                var defines = new HashSet<string>(StringComparer.Ordinal);
                foreach (ElfSymbol s in obj.Symbols)
                    if (s.IsGlobalOrWeak && !s.IsUndefined && s.Name.Length > 0)
                        defines.Add(s.Name);
                pending.Add((obj, defines));
            }
        }

        // Pull in members that satisfy an undefined reference, until nothing new is added.
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (int i = 0; i < pending.Count; i++)
            {
                (ElfObject obj, HashSet<string> defines) = pending[i];
                bool needed = false;
                foreach (string name in defines)
                {
                    // Only a reference that must be satisfied pulls a member in. A reference marked as
                    // one that may go unsatisfied is usually there to ask whether something is present,
                    // and pulling the member in to answer it makes the answer yes every time - the
                    // opposite of what the reference is for.
                    if (strongUndefined.Contains(name)) { needed = true; break; }
                }
                if (!needed)
                    continue;
                Include(obj, included, defined, undefined, strongUndefined, groupsKept, dropped, weakDefinition);
                pending.RemoveAt(i);
                i--;
                changed = true;
            }
        }

        // Map every name a stub provides to the stub that provides it, so the module name and the
        // versions it records travel together. Stubs the SDK supplies come first, so a project's own
        // stub cannot shadow a core module's name.
        var providedBy = new Dictionary<string, StubLibrary>(StringComparer.Ordinal);
        foreach (StubLibrary stub in options.ExtraStubs)
            foreach (string name in stub.Provided)
                providedBy.TryAdd(name, stub);
        foreach (string stubPath in options.Stubs)
        {
            StubLibrary stub = StubLibrary.Load(stubPath);
            foreach (string name in stub.Provided)
                providedBy.TryAdd(name, stub);
        }

        // A symbol only has to resolve when a relocation in an included object actually references it.
        // An object may also carry a global declaration (`.globl`) that no relocation uses — an
        // alternate entry an assembler emitted, for instance — and that is not a reference to satisfy.
        var referenced = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject obj in included)
            foreach (IReadOnlyList<ElfRelocation> relocs in obj.Relocations.Values)
                foreach (ElfRelocation r in relocs)
                    if (r.SymbolIndex < obj.Symbols.Count)
                    {
                        ElfSymbol s = obj.Symbols[(int)r.SymbolIndex];
                        if (s.IsUndefined && s.Name.Length > 0)
                            referenced.Add(s.Name);
                    }

        // Names the linker itself defines: the start and end of an allocated section named like a C
        // identifier, which code reads to walk that section. The writer fills in their addresses.
        var sectionNames = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject obj in included)
            foreach (ElfSection s in obj.Sections)
                if (s.IsAlloc && s.Name.Length > 0)
                    sectionNames.Add(s.Name);

        var imports = new List<ImportSymbol>();
        var unresolved = new List<string>();
        foreach (string name in undefined)
        {
            if (defined.ContainsKey(name) || !(referenced.Contains(name) || primaryUndefined.Contains(name)))
                continue;
            // A reference under the alias prefix reaches a published name while keeping a name of its
            // own, so an object can define a name the platform also publishes and still reach the
            // platform's. Without it, a definition shadows the published name for every reference in
            // the module including its own, and a routine that stood in front of one would call itself.
            if (name.StartsWith(DeviceAliasPrefix, StringComparison.Ordinal)
                && providedBy.TryGetValue(name[DeviceAliasPrefix.Length..], out StubLibrary? aliased))
                imports.Add(new ImportSymbol(
                    name, aliased.ModuleName, aliased.LibraryName, aliased.Soname,
                    aliased.ModuleVersion, aliased.LibraryVersion,
                    PublishedName: name[DeviceAliasPrefix.Length..]));
            else if (providedBy.TryGetValue(name, out StubLibrary? stub))
                imports.Add(new ImportSymbol(
                    name, stub.ModuleName, stub.LibraryName, stub.Soname, stub.ModuleVersion, stub.LibraryVersion));
            else if (IsEncapsulationSymbol(name, sectionNames, out _, out _))
                continue; // the linker synthesizes this at the section boundary
            else if (LinkerProvided.Contains(name))
                continue; // the linker writes this routine itself and places it with the linkage table
            else if (strongUndefined.Contains(name))
                unresolved.Add(name);
            // A weak-only reference that nothing satisfies is left out; the writer binds it to zero.
        }
        imports.Sort((a, b) => string.CompareOrdinal(a.Name, b.Name));
        unresolved.Sort(StringComparer.Ordinal);

        return new LinkResolution
        {
            Included = included,
            Defined = defined,
            Imports = imports,
            Unresolved = unresolved,
            SkippedMembers = skipped,
            DroppedSections = dropped,
        };
    }

    /// <summary>
    /// The prefix that makes a reference reach a published name under a name of its own. A compat
    /// routine standing in front of something the platform publishes has to keep the published name for
    /// itself - every reference in the module binds to the definition, its own included - so it reaches
    /// the platform's through this instead.
    /// </summary>
    public const string DeviceAliasPrefix = "__sp_device_";

    /// <summary>
    /// The routines the linker writes itself and places with the linkage table, rather than taking from
    /// an object, and the names the writer settles once the layout is fixed. An object may reach for any
    /// of them, so the resolver must not call them unresolved. Where the image starts, where its code
    /// ends, and where its frame index sits: the last two are what lets a module describe itself to
    /// whatever walks the stack, since its own header table is inside the code group and that group
    /// cannot be read. The section-boundary names below name group edges the writer draws itself
    /// (the writable image bounds, the constructor and destructor array bounds, the zero-filled data
    /// bounds, and the legacy text/data/end trio); a run-time reference to any of these succeeds
    /// against the writer's own layout addresses, so they must never be routed to the resolver.
    /// </summary>
    internal static readonly IReadOnlySet<string> LinkerProvided =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "_init", "_fini",
            CompatEmitter.ModuleBaseSymbol, CompatEmitter.TextEndSymbol, CompatEmitter.FrameIndexSymbol,
            CompatEmitter.FrameIndexEndSymbol,
            "__image_start", "__image_end",
            "__bss_start", "__bss_end",
            "__init_array_start", "__init_array_end",
            "__fini_array_start", "__fini_array_end",
            "__preinit_array_start", "__preinit_array_end",
            "_DYNAMIC",
            "edata", "end", "etext",
            "_edata", "_end", "_etext",
        };

    internal static bool IsEncapsulationSymbol(string name, ICollection<string> sectionNames, out string section, out bool isStop)
    {
        section = "";
        isStop = false;
        if (name.StartsWith("__start_", StringComparison.Ordinal)) { section = name["__start_".Length..]; isStop = false; }
        else if (name.StartsWith("__stop_", StringComparison.Ordinal)) { section = name["__stop_".Length..]; isStop = true; }
        else return false;
        return section.Length > 0 && sectionNames.Contains(section);
    }

    private static void Include(
        ElfObject obj, List<ElfObject> included,
        Dictionary<string, ElfObject> defined, HashSet<string> undefined, HashSet<string> strongUndefined,
        HashSet<string> groupsKept, HashSet<(ElfObject, int)> dropped,
        Dictionary<string, bool> weakDefinition)
    {
        included.Add(obj);

        // A compiler emits an inline function, a template body or a virtual table into every object that
        // needs it, and names each copy under one signature so a link keeps exactly one. Keeping them
        // all lays the same thing down repeatedly - which for anything with state is worse than wasteful:
        // two copies of a lock is two locks, and whichever half of the program holds the other one is
        // not excluded by it.
        foreach (ElfSectionGroup group in obj.Groups)
        {
            if (!group.KeepOnlyOne) continue;
            if (groupsKept.Add(group.Signature)) continue;   // the first copy seen is the one kept
            foreach (int member in group.Members)
                dropped.Add((obj, member));
        }

        foreach (ElfSymbol s in obj.Symbols)
        {
            if (s.Name.Length == 0 || !s.IsGlobalOrWeak)
                continue;
            if (s.IsUndefined)
            {
                if (!defined.ContainsKey(s.Name))
                {
                    undefined.Add(s.Name);
                    if (!s.IsWeak)
                        strongUndefined.Add(s.Name);
                }
            }
            else
            {
                // A definition in a dropped copy names nothing in the output; the kept copy defines it.
                if (dropped.Contains((obj, s.SectionIndex)))
                    continue;
                // Which definition wins does not depend on the order they are read in. One marked as
                // replaceable stands only until a real one is found, and never displaces one already
                // found; between two real ones the first is kept, which is what lets this toolchain's
                // own object stand in front of a name the runtime also defines. Recording whichever
                // arrived first regardless left a replaceable definition in place permanently, so a
                // name with a real body elsewhere resolved to the placeholder.
                if (!defined.TryGetValue(s.Name, out ElfObject? already))
                {
                    defined[s.Name] = obj;
                    weakDefinition[s.Name] = s.IsWeak;
                }
                else if (!s.IsWeak && weakDefinition.TryGetValue(s.Name, out bool wasWeak) && !wasWeak
                         && already != obj && !CompatEmitter.DeliberateOverrides.Contains(s.Name))
                {
                    // Two full definitions of one name. Only one can be reached, and which one is
                    // decided by the order the objects happened to be read in, so the other is laid
                    // into the module and never used. Say so rather than pick.
                    throw new ElfLinkException(
                        $"'{s.Name}' is defined in both {already.Origin} and {obj.Origin}. " +
                        "Only one of the two can be reached, so the link cannot say which was meant.");
                }
                else if (!s.IsWeak && weakDefinition.TryGetValue(s.Name, out wasWeak) && wasWeak)
                {
                    defined[s.Name] = obj;
                    weakDefinition[s.Name] = false;
                }
                undefined.Remove(s.Name);
                strongUndefined.Remove(s.Name);
            }
        }
    }
}
