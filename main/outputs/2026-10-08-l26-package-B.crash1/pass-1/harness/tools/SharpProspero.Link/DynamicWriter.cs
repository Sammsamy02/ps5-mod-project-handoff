// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK
//
// Writes a dynamic module: an application that imports functions from other modules. It builds the
// dynamic symbol and string tables with the mangled import names, a procedure-linkage table and its
// global-offset entries, the import relocations, the dynamic table with one needed record per
// imported module, the process parameters, and the module note, and lays them out with the load,
// dynamic, and process-parameter program headers.
//
// Four load segments: executable code, read-only data, writable data, and a fourth that requests no
// memory protection and holds the dynamic-linking tables. That fourth segment is what tells the
// loader where the linking data is; a module that names a dynamic table without carrying one is
// turned away while its program headers are scanned, so it is part of the required shape rather than
// a convenience.
//
// Structure follows the required dynamic section format. Loading on the device is the final step.

using SharpProspero.Prx;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace SharpProspero.Link;

/// <summary>Writes a dynamic module from a resolved graph that has imports.</summary>
public static class DynamicWriter
{
    private const ulong SegAlign = 0x4000;
    private const ushort TypeSceDynExec = 0xFE10;
    private const ushort TypeSceDynamic = 0xFE18;
    private const uint PfX = 1, PfW = 2, PfR = 4;
    private const int EntryStubSize = 47; // the constructor walker the entry calls
    /// <summary>Bytes reserved in front of the code so no address the module publishes is zero.</summary>
    public const ulong ImageHeadReserve = 16;
    // The section holding the call-frame records, which are laid end to end and closed by a terminator.
    private const string FrameSectionName = ".eh_frame";

    // The order a built module carries its sections in, per group, read off a link map the toolchain the
    // format comes from prints for itself. A name not listed follows the ones that are, in the order the
    // objects carry it.
    // A built module aligns the thread-local template to this before placing it, whatever its own
    // sections ask for.
    private const ulong TlsBlockAlign = 32;

    private static readonly string[] CodeOrder = [".init", ".text", ".fini"];
    private static readonly string[] ReadOnlyOrder = [".rodata", FrameSectionName];
    private static readonly string[] RelroOrder =
        [".data.rel.ro", ".got", ".got.plt", ".ctors", ".dtors", ".init_array", ".fini_array",
         ".sce_process_param", "__modules"];
    private static readonly string[] DataOrder = [".shader_header", ".data", ".bss"];

    // The prefixes a compiler splits one output section across. `.text.f` belongs to `.text`, so the
    // pieces every object contributes land together instead of being scattered among whatever else that
    // object carries. **Longest first, and the order is what makes this correct rather than merely
    // tidy**: a name is now matched whole as well as as a prefix, so `.data.rel.ro` reaches its own
    // entry before it can be taken for a piece of `.data`.
    private static readonly string[] SectionPrefixes =
        [".data.rel.ro", ".text", ".rodata", ".data", ".bss", ".init_array", ".fini_array",
         ".tdata", ".tbss", ".gcc_except_table", FrameSectionName];

    /// <summary>The name a section ends up under: its own, or the prefix it is a piece of.</summary>
    internal static string OutputSectionName(string name)
    {
        // The SDK linker script collects compiler-emitted legacy pointers into
        // the corresponding array. Only output placement changes: the object's
        // section indices, relocation targets and COMDAT membership stay intact.
        if (name == ".ctors" || name.StartsWith(".ctors.", StringComparison.Ordinal))
            return ".init_array";
        if (name == ".dtors" || name.StartsWith(".dtors.", StringComparison.Ordinal))
            return ".fini_array";
        // A name matches whole as well as split. Matching only the split form sent a section named
        // exactly `.data.rel.ro` to `.data`, so content the loader is meant to seal once it has finished
        // binding stayed writable for as long as the module ran.
        foreach (string prefix in SectionPrefixes)
            if (name.Length >= prefix.Length
                && (name.Length == prefix.Length || name[prefix.Length] == '.')
                && name.StartsWith(prefix, StringComparison.Ordinal))
                return prefix;
        return name;
    }

    /// <summary>
    /// The guard value the runtime stamps into itself while starting up. Its own object marks it
    /// read-only, and the runtime widens the page holding it to writable, writes the value, and narrows
    /// it again - refusing to start at all if either protection change is turned down.
    /// <para>
    /// That works where a protection change may widen any mapping. Here it cannot: the loader settles a
    /// range's <em>greatest</em> allowed protection when it maps the segment, taking it from the
    /// protection that segment asks for, and a segment asking for read alone is given a ceiling with no
    /// write in it. No later call can raise it. So an object the runtime writes cannot be placed in the
    /// read-only group, however read-only it is meant to be at rest - it goes in the writable group,
    /// whose ceiling admits write, and the runtime's own narrowing gives it back its read-only rest
    /// state. It is given a page of its own because that narrowing covers a whole page.
    /// </para>
    /// </summary>
    private const string GuardObjectSymbol = "__security_cookie";

    /// <summary>
    /// The name the guard object is placed under. It is named apart from the group it came from so the
    /// section table does not report one name spanning two groups.
    /// </summary>
    private const string GuardSectionName = ".sce_guard";

    /// <summary>The two arrays whose order within a group is set by the priority in each section name.</summary>
    private static readonly string[] ConstructorArrays = [".init_array", ".fini_array"];

    /// <summary>The older, relocated function-pointer form emitted by the PS5 compiler.</summary>
    private static readonly string[] OlderConstructorArrays = [".ctors", ".dtors"];

    /// <summary>
    /// The priority a constructor-array section carries in its own name. A section named for one runs
    /// before a section named for a higher one; a plain name carries none and runs last.
    /// </summary>
    internal static int ArrayPriority(string name)
    {
        int dot = name.LastIndexOf('.');
        if (dot <= 0 || !int.TryParse(name.AsSpan(dot + 1), out int priority))
            return int.MaxValue;
        // LLVM spells legacy priority p as .ctors/.dtors.(65535-p).
        return OlderConstructorArrays.Any(n => name.StartsWith(n + ".", StringComparison.Ordinal))
            ? 65535 - priority : priority;
    }

    // Mirror the SDK's separate SORT_BY_INIT_PRIORITY patterns, followed by
    // KEEP(*(.init_array .ctors)) / KEEP(*(.fini_array .dtors)). Equal-priority
    // and plain entries retain the original object/section order.
    private static int ArrayPatternOrder(string name)
    {
        if (ConstructorArrays.Any(n => name.StartsWith(n + ".", StringComparison.Ordinal)))
            return 0;
        if (OlderConstructorArrays.Any(n => name.StartsWith(n + ".", StringComparison.Ordinal)))
            return 1;
        return 2;
    }

    private static void ValidateLegacyArray(LinkResolution resolution, ElfObject obj, int index)
    {
        ElfSection section = obj.Sections[index];
        string? array = OlderConstructorArrays.FirstOrDefault(n => section.Name == n
            || section.Name.StartsWith(n + ".", StringComparison.Ordinal));
        if (array is null || section.Size == 0)
            return;
        void Reject(string reason) => throw new ElfLinkException(
            $"Unsupported legacy constructor array '{section.Name}' in {obj.Origin}: {reason}");
        if (section.Name.Length > array.Length)
        {
            ReadOnlySpan<char> suffix = section.Name.AsSpan(array.Length + 1);
            if (suffix.IsEmpty || suffix.IndexOfAnyExceptInRange('0', '9') >= 0
                || !int.TryParse(suffix, out int priority) || priority > 65535)
                Reject("priority must be a decimal value from 0 through 65535");
        }
        // This supports pointer arrays, not older CRT lists containing -1/0
        // sentinels or arbitrary bytes. One R64 relocation owns every slot.
        if (!section.IsWritable || section.IsNoBits || section.IsTls || section.IsExecutable
            || section.Size % 8 != 0 || (ulong)section.Data.Length != section.Size
            || section.AddrAlign > 8 || (section.AddrAlign != 0 && 8 % section.AddrAlign != 0)
            || section.Data.Any(b => b != 0)
            || !obj.Relocations.TryGetValue(index, out IReadOnlyList<ElfRelocation>? relocs)
            || (ulong)relocs.Count != section.Size / 8)
        {
            Reject("expected aligned, zero-filled 64-bit relocated function-pointer slots; sentinel lists are unsupported");
            return;
        }
        var slots = new HashSet<ulong>();
        foreach (ElfRelocation reloc in relocs)
        {
            if (reloc.Type != RelType.R64 || reloc.Offset % 8 != 0 || reloc.Offset >= section.Size
                || !slots.Add(reloc.Offset) || reloc.SymbolIndex >= obj.Symbols.Count || reloc.Addend < 0)
            {
                Reject("each pointer slot requires one R_X86_64_64 function relocation");
                return;
            }
            ElfObject targetObj = obj;
            ElfSymbol target = obj.Symbols[(int)reloc.SymbolIndex];
            // Match SymbolValue: a kept defined symbol resolves within its own
            // object; an undefined or discarded definition resolves through the
            // selected definition. A section symbol cannot name another copy.
            bool discarded = resolution.DroppedSections.Contains((obj, target.SectionIndex));
            if (target.Type == SymType.Section && discarded)
            {
                Reject("constructor section-symbol target was discarded");
                return;
            }
            if (target.IsUndefined || discarded)
            {
                if (!TryFindDefinedSymbol(resolution, target.Name, out ElfObject? definingObj, out ElfSymbol? definingSymbol))
                {
                    Reject("constructor target must be a defined function");
                    return;
                }
                targetObj = definingObj!;
                target = definingSymbol!;
            }
            if ((uint)target.SectionIndex >= (uint)targetObj.Sections.Count
                || !targetObj.Sections[target.SectionIndex].IsExecutable
                || resolution.DroppedSections.Contains((targetObj, target.SectionIndex))
                || (ulong)reloc.Addend > ulong.MaxValue - target.Value)
            {
                Reject("constructor target must be a defined function in an executable section");
                return;
            }
            ulong targetOffset = target.Value + (ulong)reloc.Addend;
            if (targetOffset >= targetObj.Sections[target.SectionIndex].Size
                || !targetObj.Symbols.Any(s => !s.IsUndefined && s.Type == SymType.Func
                    && s.SectionIndex == target.SectionIndex && s.Value == targetOffset))
            {
                Reject("constructor pointer must identify the start of a defined function");
                return;
            }
        }
    }

    // The linking group starts where the writable group's memory ends rather than on the next page, and
    // its file offset carries the same page offset as its address. The loader reads the group by that
    // relation; it never sits on a page of its own.
    private const ulong DynlibAlign = 16;
    // The constructor walker and the teardown routine that follows it, which is a single return.
    private const int InitFiniSize = EntryStubSize + 8;
    // The note a linked module carries in its file tail: a four-byte name and an eight-byte identifier,
    // twenty-four bytes in all, which is the shape a built module carries there. It lies outside every
    // segment the container stores, so it comes back zero-filled from a round trip - but what the module
    // *declares* about it is carried, and a length that does not describe a whole note is one a reader
    // walking notes cannot step through.
    private const int TailNoteLen = 0x18;
    private const string TailNoteName = "SIE\0";
    // The revision the modules an application links against are built at. Reported back by the system
    // as the process starts, and recorded again in the version segment; the two must not drift.
    private const uint ModuleSdkVersion = 0x02000009;
    // The version that travels with the one above. The pair is fixed: every module measured that
    // carries one carries this alongside it, and neither is chosen without the other.
    private const uint CompanionSdkVersion = 0x08050001;
    // The record a library carries in place of an executable's process parameters, and its marker.
    private const uint ModuleParamMagic = 0x3C13F4BF;
    private const int ModuleParamSize = 0x20;
    private const uint PtLoad = 1, PtDynamic = 2, PtNote = 4, PtTls = 7, PtSceProcParam = 0x61000001, PtSceModuleParam = 0x61000002, PtGnuEhFrame = 0x6474E550;
    private const uint PtGnuRelro = 0x6474E552, PtSceComment = 0x6FFFFF00, PtSceVersion = 0x6FFFFF01;

    private const long DtNeeded = 1, DtHash = 4, DtStrTab = 5, DtSymTab = 6, DtStrSz = 10, DtSymEnt = 11;
    private const long DtPltGot = 3, DtPltRelSz = 2, DtPltRel = 20, DtJmpRel = 23;
    private const long DtSceModuleInfo = 0x61000043, DtSceNeededModule = 0x61000045, DtSceImportLib = 0x61000049, DtSceImportLibAttr = 0x61000019;
    private const long DtSceOrigFilename = 0x61000041, DtSceExportLib = 0x61000047, DtSceExportLibAttr = 0x61000017;
    private const long DtSceModuleAttr = 0x61000011, DtSceSymTabSz = 0x6100003f, DtSceHashSz = 0x6100003d, DtNull = 0;
    private const long DtRela = 7, DtRelaSz = 8, DtRelaEnt = 9, DtRelaCount = 0x6ffffff9;
    private const long DtInitArray = 25, DtFiniArray = 26, DtInitArraySz = 27, DtFiniArraySz = 28;
    private const long DtInit = 12, DtFini = 13, DtDebug = 21, DtPreInitArray = 32, DtPreInitArraySz = 33;
    private const uint RJumpSlot = 7, RGlobDat = 6, RRelative = 8, RAbs64 = 1;
    // Which module owns a thread-local block. A library's descriptor pairs carry one each.
    private const uint RDtpMod64 = 16;
    private const int ShnAbs = ElfSymbol.AbsoluteSectionIndex; // not relative to a section
    private const int ShnCommon = ElfSymbol.CommonSectionIndex; // tentative storage
    private const string Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

    // One entry in the dynamic relocation table: the slot to patch, the x86-64 relocation type, the
    // dynamic-symbol index (zero for base-relative records), and the addend.
    private readonly record struct DynReloc(ulong Offset, uint Type, uint Sym, ulong Addend);

    private sealed class Import
    {
        public required string PlainName { get; init; }
        /// <summary>The name the providing module publishes; the identifier is computed from this.</summary>
        public required string PublishedName { get; init; }
        public required string ModuleName { get; init; }
        /// <summary>The library that publishes the name, which the long form is built from.</summary>
        public required string LibraryName { get; init; }
        /// <summary>The name the providing module publishes itself under, for the same reason.</summary>
        public required string PublishedModuleName { get; init; }
        public int ModuleId { get; set; }
        public int LibraryId { get; set; }
        public string MangledName { get; set; } = "";
        public int DynSymIndex { get; set; }
        public ulong GotAddress { get; set; }
        public ulong PltAddress { get; set; }
    }

    private sealed class Block
    {
        public required string Name { get; init; }
        public required byte[] Data { get; set; }
        public required uint Flags { get; init; }
        public ulong Address { get; set; }
        public ulong FileOffset { get; set; }
    }

    /// <summary>Writes the dynamic module for <paramref name="resolution"/>.</summary>
    /// <param name="resolution">The resolved symbol graph.</param>
    /// <param name="entrySymbol">The entry symbol for an executable, or null for a library.</param>
    /// <param name="kind">Whether the output is an application or a library module.</param>
    /// <param name="exportSymbols">Defined symbols the module exports for other modules to import.</param>
    /// <param name="moduleFileName">The module's own file name, recorded in the module for self-reference.</param>
    /// <param name="moduleName">
    /// The name the module publishes itself under, which the loader and any importer resolve against.
    /// Null takes the file name without its extension, which is what a module built from one source
    /// file is expected to be called.
    /// </param>
    /// <param name="exportLibraryName">
    /// The name of the library the module publishes its exports under. Null takes the module name.
    /// The two are separate because a module publishing more than one library names each apart from
    /// itself, which is how every module on the console is arranged.
    /// </param>
    /// <param name="libraryInitSymbol">A defined library function to publish as DT_INIT; null keeps the returning stub.</param>
    /// <param name="libraryFiniSymbol">A defined library function to publish as DT_FINI; null keeps the returning stub.</param>
    /// <param name="explicitLifecycleOwnsArrays">An opt-in library lifecycle owns both arrays; retain their content and bounds but advertise empty loader arrays.</param>
    public static byte[] Write(LinkResolution resolution, string? entrySymbol, ModuleKind kind = ModuleKind.Executable,
        IReadOnlyList<string>? exportSymbols = null, string? moduleFileName = null,
        string? moduleName = null, string? exportLibraryName = null, string? libraryInitSymbol = null,
        string? libraryFiniSymbol = null, bool explicitLifecycleOwnsArrays = false)
    {
        ArgumentNullException.ThrowIfNull(resolution);
        if (resolution.Unresolved.Count > 0)
            throw new ElfLinkException($"{resolution.Unresolved.Count} symbol(s) are unresolved (e.g. {resolution.Unresolved[0]}).");
        // The explicit path carries independently evidenced kinds even when input references are
        // NOTYPE. Validate before layout/emission and never silently replace conflicting evidence.
        var explicitImportTypes = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (ImportSymbol imp in resolution.Imports)
        {
            if (imp.ExplicitType is not int importType) continue;
            if (importType is not (SymType.Func or SymType.Object))
                throw new ElfLinkException($"Import '{imp.Name}' has unsupported explicit kind {importType}; expected FUNC or OBJECT.");
            if (explicitImportTypes.TryGetValue(imp.Name, out int previousType) && previousType != importType)
                throw new ElfLinkException($"Import '{imp.Name}' has conflicting explicit kinds.");
            foreach (ElfObject obj in resolution.Included)
                foreach (ElfSymbol sym in obj.Symbols)
                    if (sym.IsUndefined && sym.Name == imp.Name && sym.Type != SymType.NoType && sym.Type != importType)
                        throw new ElfLinkException($"Import '{imp.Name}' explicit kind {importType} conflicts with reference kind {sym.Type} in {obj.Origin}.");
            explicitImportTypes[imp.Name] = importType;
        }
        if (explicitLifecycleOwnsArrays &&
            (kind != ModuleKind.Library || libraryInitSymbol is null || libraryFiniSymbol is null))
            throw new ElfLinkException("Explicit array ownership requires a library with explicit init and fini functions.");
        (ElfObject Obj, ElfSymbol Sym)? libraryFini = null;
        if (libraryFiniSymbol is not null)
        {
            if (kind != ModuleKind.Library)
                throw new ElfLinkException("A library finalizer requires a PRX library.");
            if (!TryFindDefinedSymbol(resolution, libraryFiniSymbol, out ElfObject? finiObj, out ElfSymbol? finiSym)
                || finiSym!.Type != SymType.Func || finiSym.SectionIndex < 0 || finiSym.SectionIndex >= finiObj!.Sections.Count
                || !finiObj.Sections[finiSym.SectionIndex].IsExecutable)
                throw new ElfLinkException($"Library fini symbol '{libraryFiniSymbol}' must be a defined function in an executable section.");
            libraryFini = (finiObj!, finiSym!);
        }
        (ElfObject Obj, ElfSymbol Sym)? libraryInit = null;
        if (libraryInitSymbol is not null)
        {
            if (kind != ModuleKind.Library)
                throw new ElfLinkException("A library initializer requires a PRX library.");
            if (!TryFindDefinedSymbol(resolution, libraryInitSymbol, out ElfObject? initObj, out ElfSymbol? initSym)
                || initSym!.Type != SymType.Func || initSym.SectionIndex < 0 || initSym.SectionIndex >= initObj!.Sections.Count
                || !initObj.Sections[initSym.SectionIndex].IsExecutable)
                throw new ElfLinkException($"Library init symbol '{libraryInitSymbol}' must be a defined function in an executable section.");
            libraryInit = (initObj!, initSym!);
        }

        // Module ids and mangled names for the imports. A module and a library are not the same thing:
        // one module publishes several libraries, and an import names both, so the two are numbered
        // apart. Modules number from 1 (0 is the module itself) and are keyed by the module file, since
        // that is what the loader loads once. Libraries number from 0 and are keyed by the module file
        // together with the library name, so two libraries published by one module keep separate ids
        // and an import resolves against the library that actually publishes it rather than against
        // whichever one happened to be seen first. The versions come from the stub that provided the
        // name, since an import must record the version the module actually exports.
        var moduleIndex = new Dictionary<string, int>(StringComparer.Ordinal);
        var moduleData = new Dictionary<string, (string Module, ushort ModuleVersion)>(StringComparer.Ordinal);
        var libraryIndex = new Dictionary<(string Soname, string Library), int>();
        var libraryData = new Dictionary<(string Soname, string Library), (string Soname, string Library, ushort LibraryVersion, int ModuleId)>();
        var imports = new List<Import>();
        foreach (ImportSymbol imp in resolution.Imports)
        {
            if (!moduleIndex.TryGetValue(imp.Soname, out int n)) { n = moduleIndex.Count; moduleIndex[imp.Soname] = n; }
            moduleData.TryAdd(imp.Soname, (imp.ModuleName, imp.ModuleVersion));
            (string, string) libraryKey = (imp.Soname, imp.LibraryName);
            if (!libraryIndex.TryGetValue(libraryKey, out int l)) { l = libraryIndex.Count; libraryIndex[libraryKey] = l; }
            libraryData.TryAdd(libraryKey, (imp.Soname, imp.LibraryName, imp.LibraryVersion, n + 1));
            // The identifier is computed from the name the providing module publishes, which is not
            // always the name the reference carries: a routine standing in front of a published name
            // reaches it under one of its own.
            imports.Add(new Import
            {
                PlainName = imp.Name,
                PublishedName = imp.PublishedName ?? imp.Name,
                ModuleName = imp.Soname,
                LibraryName = imp.LibraryName,
                // The name the providing module publishes itself under, which is what its own long
                // forms carry - not the file it lives in, which is often spelled differently.
                PublishedModuleName = imp.ModuleName,
                ModuleId = n + 1,
                LibraryId = l
            });
        }
        foreach (Import imp in imports)
            imp.MangledName = $"{SceNid.Compute(imp.PublishedName)}#{Encode(imp.LibraryId)}#{Encode(imp.ModuleId)}";
        var importByName = new Dictionary<string, Import>(StringComparer.Ordinal);
        foreach (Import imp in imports)
            importByName[imp.PlainName] = imp;

        // Exported symbols: the module's own functions and data other modules can import. Each is a
        // defined symbol given a mangled export name under the module's own export library, numbered
        // after the import libraries. Their addresses are filled in once the layout is fixed.
        //
        // The number has to follow the **libraries**, not the modules. A module publishes more than one
        // library often enough that the two counts differ, and when they do, counting modules names a
        // library that is already an import library - so an export and an import share an id, and a
        // reader resolving one gets the other. Libraries are numbered from zero without gaps, so their
        // count is the first free id.
        int exportLibId = libraryIndex.Count;
        var exports = new List<(string Mangled, ElfObject Obj, ElfSymbol Sym, bool IsFunc)>();
        if (exportSymbols is not null)
        {
            foreach (string name in exportSymbols)
            {
                if (!TryFindDefinedSymbol(resolution, name, out ElfObject? defObj, out ElfSymbol? defSym))
                    throw new ElfLinkException($"Export symbol '{name}' is not defined by any included object.");
                bool isFunc = defSym!.Type != SymType.Object;
                exports.Add(($"{SceNid.Compute(name)}#{Encode(exportLibId)}#{Encode(0)}", defObj!, defSym, isFunc));
            }
        }
        bool hasExports = exports.Count > 0;

        // Assign each application alloc section to a segment class and record its offset. Thread-local
        // sections are held out; they form a separate template laid out below.
        var sectionOffsetInGroup = new Dictionary<(ElfObject, int), ulong>();
        // Sixteen bytes are reserved in front of the code so nothing the module publishes sits at address
        // zero. Every module the toolchain the format comes from builds opens this way. An address of
        // zero reads back as "none" everywhere a routine or a table is optional, so a real one placed
        // there cannot be told apart from an absent one.
        ulong textLen = ImageHeadReserve, roLen = 0, dataLen = 0, dataMem = 0, frameLen = 0, frameOffset = 0;

        // Sections written while the module is bound and never again go in the relocation-read-only
        // group, which the loader turns read-only once binding is done. Those are the constructor and
        // destructor arrays and anything the compiler marked as relocated-then-constant. Leaving them
        // in the plain writable group would keep them writable for the life of the process.
        //
        // The arrays are laid out first and contiguously, so a combined address and size can name them
        // for the loader; nothing else is placed between them.
        var relroSections = new HashSet<(ElfObject, int)>();
        ulong relroDataLen = 0, relroDataAlign = 8;
        ulong initArrayOff = 0, initArrayEnd = 0, finiArrayOff = 0, finiArrayEnd = 0;
        bool haveInit = false, haveFini = false;

        // Sections are gathered by the name they end up under and placed one output section at a time,
        // in the order a built module carries them. A section named `.text.f` belongs to `.text`, so
        // every object's contribution to a name lands together rather than being scattered among the
        // other sections that object happens to carry. That is what makes a name a contiguous run - the
        // frame records need it to be read as one chain, and the constructor array needs it to be named
        // by one address and one length.
        // Sections another object carried first are not laid out at all: marking them placed before
        // anything is gathered keeps every group from claiming them.
        var placed = new HashSet<(ElfObject, int)>(resolution.DroppedSections.Select(d => (d.Object, d.Section)));

        // The one object the runtime writes to through a protection change, held out of the read-only
        // group before anything is gathered so no group claims it first. See <see cref="GuardObjectSymbol"/>
        // for why it cannot live there. It is placed at the head of the writable group below.
        (ElfObject Obj, int Index)? guard = null;
        foreach (ElfObject obj in resolution.Included)
        {
            foreach (ElfSymbol sym in obj.Symbols)
            {
                if (sym.Name != GuardObjectSymbol || sym.IsUndefined)
                    continue;
                int gi = sym.SectionIndex;
                if ((uint)gi < (uint)obj.Sections.Count
                    && obj.Sections[gi] is { IsAlloc: true, IsWritable: false, IsTls: false, IsExecutable: false })
                    guard = (obj, gi);
                break;
            }
            if (guard is not null)
                break;
        }
        if (guard is { } heldOut)
            placed.Add(heldOut);

        // Keep the SDK compiler's pointer arrays, but do not silently treat a CRT
        // sentinel list or non-pointer data as loader-invoked function pointers.
        foreach (ElfObject obj in resolution.Included)
            for (int i = 0; i < obj.Sections.Count; i++)
                if (obj.Sections[i].IsAlloc && !placed.Contains((obj, i)))
                    ValidateLegacyArray(resolution, obj, i);

        List<(ElfObject Obj, int Index)> Gather(Func<ElfSection, bool> inGroup, string[] order)
        {
            var byName = new Dictionary<string, List<(ElfObject, int)>>(StringComparer.Ordinal);
            var seen = new List<string>();
            foreach (ElfObject obj in resolution.Included)
                for (int i = 0; i < obj.Sections.Count; i++)
                {
                    ElfSection sec = obj.Sections[i];
                    if (!sec.IsAlloc || sec.IsTls || placed.Contains((obj, i)) || !inGroup(sec))
                        continue;
                    string name = OutputSectionName(sec.Name);
                    if (!byName.TryGetValue(name, out List<(ElfObject, int)>? list))
                    {
                        byName[name] = list = [];
                        seen.Add(name);
                    }
                    list.Add((obj, i));
                }
            // Known names in the recorded order, then anything else in the order it was first seen.
            // The tie-break reads a snapshot rather than the list being sorted: asking the list where a
            // name currently sits, while the sort is moving names around inside it, answers about the
            // arrangement so far rather than the one the names arrived in.
            var firstSeen = new Dictionary<string, int>(seen.Count, StringComparer.Ordinal);
            for (int i = 0; i < seen.Count; i++)
                firstSeen[seen[i]] = i;
            seen.Sort((a, b) =>
            {
                int ra = Array.IndexOf(order, a), rb = Array.IndexOf(order, b);
                if (ra < 0) ra = order.Length;
                if (rb < 0) rb = order.Length;
                return ra != rb ? ra.CompareTo(rb) : firstSeen[a].CompareTo(firstSeen[b]);
            });
            // Within either constructor array, the order is the one each section's own name asks for:
            // a section named for a priority runs before one named for a higher priority, and the
            // plain name runs last. Grouping alone keeps them together but in the order the objects
            // happened to be read, which is not the order the priority asks for.
            foreach (string arrayName in ConstructorArrays)
                if (byName.TryGetValue(arrayName, out List<(ElfObject, int)>? array))
                    byName[arrayName] = [.. array.OrderBy(e => ArrayPatternOrder(e.Item1.Sections[e.Item2].Name))
                        .ThenBy(e => ArrayPriority(e.Item1.Sections[e.Item2].Name))];

            var result = new List<(ElfObject, int)>();
            foreach (string name in seen)
                result.AddRange(byName[name]);
            foreach ((ElfObject, int) s in result)
                placed.Add(s);
            return result;
        }

        // The relocated-then-constant group, in the order a built module carries it. The two arrays are
        // contiguous within it so a single address and length names each.
        foreach ((ElfObject obj, int i) in Gather(
            s => s is { IsWritable: true, IsNoBits: false } && RelroOrder.Contains(OutputSectionName(s.Name)),
            RelroOrder))
        {
            ElfSection sec = obj.Sections[i];
            ulong o = Align(relroDataLen, sec.AddrAlign);
            sectionOffsetInGroup[(obj, i)] = o;
            relroSections.Add((obj, i));
            relroDataLen = o + sec.Size;
            if (sec.AddrAlign > relroDataAlign) relroDataAlign = sec.AddrAlign;
            // The bounds are taken from the name the section is placed under, not the name it carries.
            // A constructor array given a priority carries that priority in its name, so matching the
            // raw name missed it entirely: the array was laid out, and the walker was then told it
            // spanned nothing, so those constructors were never run and nothing said so.
            string placedAs = OutputSectionName(sec.Name);
            if (placedAs == ".init_array") { if (!haveInit) { initArrayOff = o; haveInit = true; } initArrayEnd = relroDataLen; }
            if (placedAs == ".fini_array") { if (!haveFini) { finiArrayOff = o; haveFini = true; } finiArrayEnd = relroDataLen; }
        }

        // An executable runs its own global constructors: the loader runs the init array of a library it
        // loads, but not of the main executable - that is the start code's job. Without this the
        // module's initializers, the runtime's own registration among them, never run, so it starts and
        // then fails on the first managed call. Which of the two names the array, and which is left
        // doing nothing, is settled where the setup routine is written.

        // The thread-local template: the initialized sections first so their file image is contiguous,
        // then the zero-filled sections. Each thread receives a copy of this template at run time. It is
        // measured before the writable group is laid out, because its initialized bytes ride in that
        // group and have to land among the stored bytes rather than after them.
        var tlsOffset = new Dictionary<(ElfObject, int), ulong>();
        ulong tlsMemLen = 0, tlsFileLen = 0, tlsAlign = 1;
        for (int pass = 0; pass < 2; pass++)
            foreach (ElfObject obj in resolution.Included)
                for (int i = 0; i < obj.Sections.Count; i++)
                {
                    ElfSection sec = obj.Sections[i];
                    if (!sec.IsAlloc || !sec.IsTls) continue;
                    if ((pass == 0) == sec.IsNoBits) continue;          // initialized sections in pass 0
                    ulong o = Align(tlsMemLen, sec.AddrAlign);
                    tlsOffset[(obj, i)] = o; tlsMemLen = o + sec.Size;
                    if (!sec.IsNoBits) tlsFileLen = tlsMemLen;
                    if (sec.AddrAlign > tlsAlign) tlsAlign = sec.AddrAlign;
                }
        bool hasTls = tlsMemLen > 0;
        ulong tlsAlignedMem = Align(tlsMemLen, tlsAlign);

        // The code group. A built module opens it with the start-up and shutdown routines and the code,
        // and closes it with whatever else the objects mark executable.
        foreach ((ElfObject obj, int i) in Gather(s => s.IsExecutable, CodeOrder))
        {
            ElfSection sec = obj.Sections[i];
            sectionOffsetInGroup[(obj, i)] = textLen = Align(textLen, sec.AddrAlign);
            textLen += sec.Size;
        }

        // The read-only group. The frame records form one chain read from the first record to a
        // terminator, so every object's contribution has to sit in one run with nothing between: a gap
        // ends the chain early and leaves every record after it unreachable to anything reading the
        // chain rather than the index. Gathering by name is what puts them in one run.
        bool frameRunClosed = false;
        foreach ((ElfObject obj, int i) in Gather(s => !s.IsWritable && !s.IsExecutable, ReadOnlyOrder))
        {
            ElfSection sec = obj.Sections[i];
            bool frames = OutputSectionName(sec.Name) == FrameSectionName;
            // The chain's terminating zero closes the run, before whatever follows it. Nothing is
            // written there: the image starts out zeroed, so reserving the four bytes is what puts the
            // terminator in. Without it a reader walking the chain runs off the last record into
            // whatever follows and keeps going.
            if (!frames && frameLen > 0 && !frameRunClosed) { roLen = Align(roLen, 4) + 4; frameRunClosed = true; }
            sectionOffsetInGroup[(obj, i)] = roLen = frames && frameLen > 0 ? roLen : Align(roLen, sec.AddrAlign);
            if (frames && frameLen == 0) frameOffset = roLen;
            roLen += sec.Size;
            if (frames) frameLen += sec.Size;
        }
        if (frameLen > 0 && !frameRunClosed) roLen = Align(roLen, 4) + 4;

        // The guard object leads the writable group and keeps its page to itself. The runtime narrows
        // that whole page back to read-only the moment it has written the value, so anything sharing the
        // page would be frozen with it for as long as the module runs.
        if (guard is { } guardSection)
        {
            sectionOffsetInGroup[guardSection] = 0;
            dataLen = dataMem = SegAlign;
        }

        // The writable group: what it stores first, then what it only reserves. Placing a reserved
        // section between two stored ones would force the group to store the whole span and write out as
        // zeros what it could have reserved.
        var writable = Gather(s => s.IsWritable, DataOrder);
        for (int pass = 0; pass < 2; pass++)
            foreach ((ElfObject obj, int i) in writable)
            {
                ElfSection sec = obj.Sections[i];
                if ((pass == 0) == sec.IsNoBits) continue;
                ulong o = Align(dataMem, sec.AddrAlign);
                sectionOffsetInGroup[(obj, i)] = o; dataMem = o + sec.Size;
                if (!sec.IsNoBits) dataLen = dataMem;
            }

        // The exception-frame index is built from the frame sections when the compiler emitted them in
        // a form the index covers; otherwise it is omitted and the frames still resolve by linear scan.
        var ehFrames = new List<(ElfObject Obj, int Index)>();
        foreach (ElfObject obj in resolution.Included)
            for (int i = 0; i < obj.Sections.Count; i++)
            {
                ElfSection sec = obj.Sections[i];
                if (sec is { IsAlloc: true, IsExecutable: false, IsWritable: false, IsNoBits: false } && sec.Name == ".eh_frame")
                    ehFrames.Add((obj, i));
            }
        int ehFrameCount = 0;
        bool ehFrameOk = ehFrames.Count > 0;
        foreach ((ElfObject obj, int i) in ehFrames)
        {
            var probe = new List<EhFrame.Entry>();
            if (!EhFrame.TryParse(obj.Sections[i].Data, 0, probe)) { ehFrameOk = false; break; }
            ehFrameCount += probe.Count;
        }
        int ehFrameHdrSize = ehFrameOk && ehFrameCount > 0 ? 12 + ehFrameCount * 8 : 0;

        // Build dynamic-metadata bytes. The needed record uses the module file name; the module and
        // library records use the bare name (no extension), matching the reference layout.
        string fileName = moduleFileName ?? (kind == ModuleKind.Library ? "prospero_module.prx" : "eboot.bin");
        string publishedModuleName = moduleName ?? System.IO.Path.GetFileNameWithoutExtension(fileName);
        string exportLibraryPublishedName = exportLibraryName ?? publishedModuleName;
        var dynstr = new StringTable();
        int moduleInfoName = dynstr.Add(publishedModuleName);
        int exportLibNameOffset = dynstr.Add(exportLibraryPublishedName);
        int origFileNameOff = dynstr.Add(fileName);
        var moduleRecords = new (int SonameOff, int ModuleNameOff, int ModuleId, ushort ModuleVersion)[moduleIndex.Count];
        foreach ((string soname, int n) in moduleIndex)
        {
            (string neededModuleName, ushort moduleVersion) = moduleData[soname];
            moduleRecords[n] = (dynstr.Add(soname), dynstr.Add(neededModuleName), n + 1, moduleVersion);
        }
        // One record per library, carrying the id of the module that publishes it. A module publishing
        // two libraries produces one needed record and two library records.
        var libraryRecords = new (int LibraryNameOff, int LibraryId, ushort LibraryVersion, int ModuleId)[libraryIndex.Count];
        foreach (((string Soname, string Library) key, int l) in libraryIndex)
        {
            (_, string libraryName, ushort libraryVersion, int moduleId) = libraryData[key];
            libraryRecords[l] = (dynstr.Add(libraryName), l, libraryVersion, moduleId);
        }

        // Symbols reached through the global-offset table (data or position-independent access) and
        // the count of absolute 64-bit references. Both feed the dynamic relocation table; a module
        // that only calls imported functions has neither, so the table stays empty.
        var gotDataOrder = new List<string>();
        var gotDataSym = new List<(ElfObject Obj, int SymIndex)>();
        var gotDataIndex = new Dictionary<string, int>(StringComparer.Ordinal);
        // The thread-local descriptor pairs, keyed by where in the module's block they point.
        var tlsPairOrder = new List<ulong>();
        var tlsPairIndex = new Dictionary<ulong, int>();
        bool needsTlsModulePair = false;
        int abs64Count = 0;
        // An import gets a linkage-table entry only when something calls it. One reached only by taking
        // its address - which is what a data import is - is bound by a relocation on the reference
        // itself, so an entry for it would be a stub nothing jumps to and a binding record that asks
        // the loader to treat a variable the way it treats a function.
        var calledImports = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject o in resolution.Included)
            foreach (KeyValuePair<int, IReadOnlyList<ElfRelocation>> kv in o.Relocations)
            {
                if (kv.Key >= o.Sections.Count || !o.Sections[kv.Key].IsAlloc)
                    continue;
                // A section another object carried first is not in the output, so its relocations are
                // not applied either. Counting them anyway made the table longer than what goes into
                // it, and the difference stayed as records of nothing at the end of it - which the
                // pass that applies them already knew to skip, so the two disagreed.
                if (resolution.DroppedSections.Contains((o, kv.Key)))
                    continue;
                HashSet<ulong>? folded = FoldedTlsCalls(kv.Value, kind);
                foreach (ElfRelocation r in kv.Value)
                {
                    if (r.SymbolIndex >= (uint)o.Symbols.Count)
                        continue;
                    if (folded is not null && folded.Contains(r.Offset))
                        continue;
                    // A library reaches a thread-local through a pair of table slots holding which
                    // module owns the variable and where in that module's block it sits. Which module
                    // is only settled as the module loads, so the pair is what the load-time record
                    // fills. One pair serves every reference resolving to the same place in the block,
                    // which is what the offset keys the collection on: two names at one place are one
                    // pair, and the same name reached from two objects is not two.
                    if (kind == ModuleKind.Library && r.Type == RelType.TlsGd)
                    {
                        ElfSymbol ts = o.Symbols[(int)r.SymbolIndex];
                        if (!TryTlsTemplateOffset(resolution, tlsOffset, o, ts, out ulong blockOff))
                            throw new ElfLinkException($"Thread-local symbol '{ts.Name}' has no template section.");
                        tlsPairIndex.TryAdd(blockOff, tlsPairOrder.Count);
                        if (tlsPairIndex[blockOff] == tlsPairOrder.Count)
                            tlsPairOrder.Add(blockOff);
                        continue;
                    }
                    if (kind == ModuleKind.Library && r.Type == RelType.TlsLd)
                    {
                        // The module-base lookup needs one pair for the module, with no offset in it;
                        // each member is then reached by an offset written straight into the code.
                        needsTlsModulePair = true;
                        continue;
                    }
                    // A GOT-relative data load and an initial-exec thread-local load both need a GOT slot;
                    // the thread-local slot holds a link-time offset rather than an address (filled below).
                    if (RelType.IsGotPcRel(r.Type) || r.Type == RelType.GotTpOff)
                    {
                        ElfSymbol gs = o.Symbols[(int)r.SymbolIndex];
                        // A relaxable load of a symbol settled here becomes a direct reference, so it
                        // needs no slot at all.
                        if (ResolvesLocally(resolution, importByName, gs)
                            && CanRelaxGot(o.Sections[kv.Key], r))
                            continue;
                        string n = gs.Name;
                        if (n.Length > 0 && gotDataIndex.TryAdd(n, gotDataOrder.Count))
                        {
                            gotDataOrder.Add(n);
                            gotDataSym.Add((o, (int)r.SymbolIndex));
                        }
                    }
                    else if (r.Type is RelType.Plt32 or RelType.Pc32)
                    {
                        string n = o.Symbols[(int)r.SymbolIndex].Name;
                        if (n.Length > 0)
                            calledImports.Add(n);
                    }
                    else if (r.Type == RelType.R64 && ProducesDynReloc(resolution, importByName, o.Symbols[(int)r.SymbolIndex]))
                        abs64Count++;
                }
            }

        // An import is data when an object that references it says so. The distinction matters: a
        // variable declared as a function invites the loader to bind it the way a function is bound,
        // and the address read out of it would then be a call stub rather than the object itself.
        var dataImports = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject obj in resolution.Included)
            foreach (ElfSymbol sym in obj.Symbols)
                if (sym.IsUndefined && sym.Type == SymType.Object && sym.Name.Length > 0)
                    dataImports.Add(sym.Name);

        var dynsym = new List<byte>(new byte[24]);
        int di = 1;
        foreach (Import imp in imports)
        {
            byte[] e = new byte[24];
            BinaryPrimitives.WriteUInt32LittleEndian(e, (uint)dynstr.Add(imp.MangledName));
            // Bound globally, the way a module the platform's own linker builds binds its imports: one
            // that cannot be resolved is then reported rather than quietly left null, which is the
            // difference between a named failure and a call through a null pointer.
            int importType = explicitImportTypes.TryGetValue(imp.PlainName, out int explicitType)
                ? explicitType : (dataImports.Contains(imp.PlainName) ? SymType.Object : SymType.Func);
            e[4] = (byte)((1 << 4) | importType); // GLOBAL, explicitly evidenced or legacy-inferred kind
            dynsym.AddRange(e);
            imp.DynSymIndex = di++;
        }
        // Export entries follow the imports; the placeholder section index marks them defined, and
        // their addresses are patched in once the layout assigns section addresses.
        var exportDynIndex = new List<(int Index, ElfObject Obj, ElfSymbol Sym)>();
        foreach ((string mangled, ElfObject obj, ElfSymbol sym, bool isFunc) in exports)
        {
            byte[] e = new byte[24];
            BinaryPrimitives.WriteUInt32LittleEndian(e, (uint)dynstr.Add(mangled));
            e[4] = (byte)((1 << 4) | (isFunc ? 2 : 1)); // GLOBAL, FUNC or OBJECT
            BinaryPrimitives.WriteUInt16LittleEndian(e.AsSpan(6), 1);
            BinaryPrimitives.WriteUInt64LittleEndian(e.AsSpan(16), sym.Size);
            dynsym.AddRange(e);
            exportDynIndex.Add((di, obj, sym));
            di++;
        }
        byte[] dynstrBytes = dynstr.ToBytes();
        byte[] dynsymBytes = [.. dynsym];
        // The bucket a symbol sits in is the hash of its long form - the identifier, the library that
        // publishes it and the module that carries it, joined by hashes - and not of the shortened
        // name the string table holds nor of the plain name it was computed from. That is what the
        // loader hashes when it searches a module for a name, so an export filed under anything else
        // sits in a bucket no lookup reaches and cannot be found at all. Measured on two modules the
        // console ships: twenty-six of twenty-six exports land where the long form puts them and none
        // where either other form does.
        //
        // A module's own imports are never searched for in its own table - the loader looks them up in
        // the module that publishes them - so what they hash to cannot be observed. They are given the
        // same long form for consistency rather than because anything reads it.
        var dynsymNames = new List<string>(imports.Count + exports.Count + 1) { "" };
        foreach (Import imp in imports)
            dynsymNames.Add($"{SceNid.Compute(imp.PublishedName)}#{imp.LibraryName}#{imp.PublishedModuleName}");
        foreach ((string mangled, ElfObject _, ElfSymbol _, bool _) in exports)
            dynsymNames.Add($"{mangled.Split('#')[0]}#{exportLibraryPublishedName}#{publishedModuleName}");
        byte[] hashBytes = BuildSysVHash(dynsymNames);
        // The imports that something calls, in the order they were collected. Only these take a slot in
        // the linkage table and a binding record; the rest are reached through a relocation on the
        // reference itself.
        var boundImports = new List<Import>(imports.Count);
        foreach (Import im in imports)
            if (calledImports.Contains(im.PlainName))
                boundImports.Add(im);
        byte[] relaBytes = new byte[boundImports.Count * 24];
        // A GOT-data slot carries a load-time relocation unless it targets an unresolved weak symbol,
        // which stays null with no fixup; count the ones that actually emit a record so the table is
        // sized exactly rather than padded with ignored entries.
        int gotDataRelocCount = 0;
        for (int i = 0; i < gotDataOrder.Count; i++)
        {
            ElfSymbol gs = gotDataSym[i].Obj.Symbols[gotDataSym[i].SymIndex];
            // A thread-local slot holds a fixed link-time offset with no load-time fixup.
            if (gs.Type != SymType.Tls && ProducesDynReloc(resolution, importByName, gs))
                gotDataRelocCount++;
        }
        // Five pointers belong to the C-library backing blocks; an executable's process parameters
        // add three more. A library has a shorter module-parameter record with none of those fields.
        // The optional marker names the C library the module was linked against. Count each under
        // the same condition that writes its records, so the declared extent has no empty entries.
        int paramBlockPointers = ParamBlockPointers
            + (kind == ModuleKind.Executable ? ProcParamPointers : 0)
            + (importByName.ContainsKey(CompatEmitter.LibcMarkerName) ? 1 : 0);
        // One pair per thread-local descriptor, plus one for the module's own block when a
        // local-dynamic base asks for it. Each pair takes one load-time record, for its module word.
        int tlsPairCount = tlsPairOrder.Count + (needsTlsModulePair ? 1 : 0);
        byte[] relaDynBytes = new byte[(gotDataRelocCount + abs64Count + paramBlockPointers + tlsPairCount) * 24];
        byte[] pltBytes = new byte[16 + boundImports.Count * 16 + InitFiniSize];
        byte[] gotBytes = new byte[24 + boundImports.Count * 8 + gotDataOrder.Count * 8 + tlsPairCount * 16];
        byte[] procParam = kind == ModuleKind.Library ? BuildModuleParam() : BuildProcParam();
        byte[] paramBlocks = BuildParamBlocks();
        byte[] note = BuildNote();
        byte[] comment = BuildComment(fileName);
        // One record per component the link consumed: the start object this linker writes, then each
        // module the result binds against. A built module names its own start files and every library it
        // was given, in that order.
        byte[] versionBlob = BuildVersion([CrtEmitter.StartComponentName, .. moduleIndex.Keys]);

        // Assign segment base addresses and file offsets on the grid; the header occupies the first page.
        // The code group ends after the procedure-linkage table, which is aligned past the code, so the
        // group that follows starts from that end rather than from the unaligned sum. Starting from the
        // sum can place the next group below the end of this one when the code length is not already
        // aligned, which would overlap the two.
        ulong textAddr = 0;
        ulong pltAddr = Align(textAddr + textLen, 16);
        // The two routines the linker writes itself sit after the linkage table, so their addresses are
        // known as soon as the table's size is - which is before anything is relocated, and has to be,
        // because the entry calls them by name.
        int initOffset = 16 + boundImports.Count * 16;
        ulong initAddr = pltAddr + (ulong)initOffset;
        ulong finiAddr = initAddr + EntryStubSize;
        var linkerDefined = new Dictionary<string, ulong>(StringComparer.Ordinal)
        {
            [CrtEmitter.InitSymbol] = initAddr,
            ["_fini"] = finiAddr,
            // The start of the image, at link-time address zero. Reached instruction-relative, it reads
            // back as the address the module was placed at - which is the only way to learn that
            // address here, since nothing published tells a module where it landed.
            [CompatEmitter.ModuleBaseSymbol] = textAddr,
            // LLD gives __dso_handle the ELF-header address: its relocated address identifies this
            // module to __cxa_atexit/__cxa_finalize, even though its link-time address is zero.
            ["__dso_handle"] = textAddr,
        };
        ulong textSegEndAddr = pltAddr + (ulong)pltBytes.Length;

        // Read-only group: rodata then the exception-frame index.
        ulong roAddr = Align(textSegEndAddr, SegAlign);
        // Eight, which is what a module built by the toolchain the format comes from declares for the
        // frame index, rather than the four its contents alone would need.
        ulong ehFrameHdrAddr = Align(roAddr + roLen, 8);
        ulong roEndAddr = ehFrameHdrSize > 0 ? ehFrameHdrAddr + (ulong)ehFrameHdrSize : roAddr + roLen;
        // The SDK linker script names the boundaries of the emitted frame sections. The frame run
        // excludes the unnamed terminating word reserved above; an omitted section has an empty span.
        ulong ehFrameStart = frameLen > 0 ? roAddr + frameOffset : roAddr + roLen;
        linkerDefined["__eh_frame_start"] = ehFrameStart;
        linkerDefined["__eh_frame_end"] = ehFrameStart + frameLen;
        linkerDefined["__eh_frame_hdr_start"] = ehFrameHdrAddr;
        linkerDefined["__eh_frame_hdr_end"] = ehFrameHdrAddr + (ulong)ehFrameHdrSize;
        // Where the code ends and where the frame index sits. A module describing itself to the
        // unwinder needs both, and neither can be read back out of the image at runtime: they live in
        // the image header, which is inside the code group, and that group is mapped to execute without
        // read. Naming them here is what lets the description be built from instruction-relative
        // addresses instead. A module carrying no frame index names the image start for it, which the
        // description reads as having none.
        linkerDefined[CompatEmitter.TextEndSymbol] = textSegEndAddr;
        linkerDefined[CompatEmitter.FrameIndexSymbol] = ehFrameHdrSize > 0 ? ehFrameHdrAddr : textAddr;
        // The far end of that index. The reader is handed the index as a range and measures it from
        // here rather than from anything inside the index, so the two names have to be given together.
        linkerDefined[CompatEmitter.FrameIndexEndSymbol] =
            ehFrameHdrSize > 0 ? ehFrameHdrAddr + (ulong)ehFrameHdrSize : textAddr;
        // First writable group: the global-offset table and the process parameters. The
        // relocation-read-only header covers this group, so the loader turns it read-only once it has
        // finished binding the module. That is what the table is for, and where the parameters belong -
        // both are written during binding and never again. Nothing the module itself writes to goes
        // here, which is why the data below is a group of its own.
        ulong relroAddr = Align(roEndAddr, SegAlign);
        ulong gotAddr = relroAddr;
        ulong gotDataAddr = gotAddr + 24 + (ulong)boundImports.Count * 8;
        // The descriptor pairs close the table. A pair is two words and is addressed as a unit, so the
        // code points at the first of the two and the helper reads both.
        ulong tlsPairAddr = gotDataAddr + (ulong)gotDataOrder.Count * 8;
        ulong relroDataAddr = Align(gotAddr + (ulong)gotBytes.Length, relroDataAlign);
        // These names are already recognized by Linker.Resolve. Supply their actual
        // placed ranges before relocating references to them, including empty spans.
        linkerDefined["__init_array_start"] = relroDataAddr + initArrayOff;
        linkerDefined["__init_array_end"] = relroDataAddr + initArrayEnd;
        linkerDefined["__fini_array_start"] = relroDataAddr + finiArrayOff;
        linkerDefined["__fini_array_end"] = relroDataAddr + finiArrayEnd;
        ulong procAddr = Align(relroDataAddr + relroDataLen, 8);
        // The thread-local template closes the group. Every module that carries one keeps it here
        // rather than in the plain writable group: it is a template each thread copies, written while
        // the module is bound and read-only afterwards. Only its stored bytes live in the image - the
        // rest of the template is per-thread and reserved when a thread is made.
        // The template starts on a 32-byte boundary whatever its own sections ask for, which is what a
        // built module does: the address is aligned to 32 before the first thread-local section is
        // placed, so a template whose sections only need eight still begins at 32.
        ulong paramBlocksAddr = Align(procAddr + (ulong)procParam.Length, 8);
        ulong tlsAddr = Align(paramBlocksAddr + (ulong)paramBlocks.Length, Math.Max(TlsBlockAlign, tlsAlign));
        ulong relroEndAddr = hasTls ? tlsAddr + tlsFileLen : paramBlocksAddr + (ulong)paramBlocks.Length;

        // Second writable group: the data the module writes to, and whatever it reserves past what it
        // stores. This one stays writable for the life of the process, so it has to sit outside the
        // group above - data covered by the relocation-read-only header faults on its first write.
        ulong dataAddr = Align(relroEndAddr, SegAlign);
        // A module with no writable data of its own still carries the group, and it has to *store*
        // something. A mapped segment that stores nothing is carried by the container as a pair of
        // zero-length segments sharing one file offset, which the loader turns away. It also has to
        // reserve less than a whole page: reserving a page would end the group flush on a page boundary,
        // and the linking group - which starts where this group's memory ends - would then begin on a
        // page of its own at a page-aligned file offset, which is refused as well.
        if (dataLen == 0) dataLen = 8;
        ulong dataSegMem = Math.Max(dataMem, dataLen);
        ulong dataEndAddr = dataAddr + dataSegMem;

        // The dynamic-linking group holds every table the loader reads to bind the module: the symbol
        // and string tables, the hash, both relocation tables, the note, and the dynamic table itself.
        // It is a load segment carrying no memory protection, which is what marks it as linking data
        // rather than image content. A module that names a dynamic table without also carrying this
        // segment is rejected while its program headers are scanned, before any of its code runs, so
        // the group is not an optional nicety - the module does not start without it.
        // The tables come in one order and one only: the string table at the very base of the group,
        // then the symbol table, the two relocation tables with the binding records first, the hash, the
        // note, and the dynamic table last. Any other order leaves the module unable to start. Each
        // table begins on an 8-aligned address, the note on a 4-aligned one, with no padding beyond
        // what that alignment asks for.
        ulong dynlibAddr = Align(dataEndAddr, DynlibAlign);
        ulong dynstrAddr = dynlibAddr;
        ulong dynsymAddr = Align(dynstrAddr + (ulong)dynstrBytes.Length, 8);
        ulong relaAddr = Align(dynsymAddr + (ulong)dynsymBytes.Length, 8);
        ulong relaDynAddr = Align(relaAddr + (ulong)relaBytes.Length, 8);
        ulong hashAddr = Align(relaDynAddr + (ulong)relaDynBytes.Length, 8);
        ulong noteAddr = Align(hashAddr + (ulong)hashBytes.Length, 4);
        ulong dynamicAddr = Align(noteAddr + (ulong)note.Length, 8);
        // worker-prx-packaging: provide the dynamic-section address for _DYNAMIC
        // relocations, mirroring PayloadWriter's ["_DYNAMIC"] = dynamicAddr.
        linkerDefined["_DYNAMIC"] = dynamicAddr;

        // Table slot and stub for each import something calls. An import nobody calls keeps both at
        // zero: every reference to it carries its own relocation, so nothing reads these.
        for (int i = 0; i < boundImports.Count; i++)
        {
            boundImports[i].GotAddress = gotAddr + 24 + (ulong)i * 8;
            boundImports[i].PltAddress = pltAddr + 16 + (ulong)i * 16;
        }
        // Binding records and the stubs that jump through them.
        for (int i = 0; i < boundImports.Count; i++)
        {
            int b = i * 24;
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b), boundImports[i].GotAddress);
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b + 8), ((ulong)boundImports[i].DynSymIndex << 32) | RJumpSlot);
            int p = 16 + i * 16;
            pltBytes[p] = 0xFF; pltBytes[p + 1] = 0x25;
            long disp = (long)boundImports[i].GotAddress - (long)(boundImports[i].PltAddress + 6);
            BinaryPrimitives.WriteInt32LittleEndian(pltBytes.AsSpan(p + 2), unchecked((int)disp));
        }

        // Section address resolver and application relocation (imports through their PLT entry,
        // GOT-data references through the GOT). Absolute references collect a dynamic relocation.
        ulong SectionAddr(ElfObject o, int i)
        {
            if (tlsOffset.TryGetValue((o, i), out ulong tlsO)) return tlsAddr + tlsO;
            // A common (tentative) symbol carries no section; modern compilers place these in .bss by
            // default, so a clear message beats indexing past the sections with a bare index number.
            if (i == ShnCommon)
                throw new ElfLinkException($"{o.Origin}: a common (uninitialized global) symbol has no storage. Compile the object with -fno-common, the default on current compilers.");
            // A reserved or out-of-range section index has no section address; report it as a link
            // error rather than indexing past the sections.
            if ((uint)i >= (uint)o.Sections.Count)
                throw new ElfLinkException($"{o.Origin}: a symbol refers to section index {i}, which the object does not define.");
            ElfSection s = o.Sections[i];
            if (relroSections.Contains((o, i))) return relroDataAddr + sectionOffsetInGroup[(o, i)];
            // The guard object sits in the writable group although its own section asks to be read-only,
            // so its address cannot be settled from the section's flags the way every other one is.
            if (guard == (o, i)) return dataAddr + sectionOffsetInGroup[(o, i)];
            ulong bas = s.IsExecutable ? textAddr : s.IsWritable ? dataAddr : roAddr;
            return bas + sectionOffsetInGroup[(o, i)];
        }
        RefuseThreadLocalsInALibrary(resolution, kind, tlsAlignedMem);
        var dynRelocs = new List<DynReloc>(gotDataOrder.Count + abs64Count);
        var sectionData = RelocateApp(resolution, importByName, SectionAddr, gotDataAddr, gotDataIndex, dynRelocs, tlsOffset, tlsAlignedMem, linkerDefined,
            kind, tlsPairAddr, tlsPairIndex, tlsPairOrder.Count);

        // Add the global-offset-table slots to the dynamic relocations: an imported symbol resolves
        // through the dynamic symbol table (GLOB_DAT); a defined symbol is relative to the load base.
        for (int i = 0; i < gotDataOrder.Count; i++)
        {
            ulong slot = gotDataAddr + (ulong)i * 8;
            (ElfObject tobj, int tsi) = gotDataSym[i];
            ElfSymbol tsym = tobj.Symbols[tsi];
            if (tsym.Type == SymType.Tls)
            {
                // Initial-exec: the slot holds a fixed thread-pointer offset (template offset minus the
                // aligned template size, since the block sits below the thread pointer on this target), so
                // it is written into the slot at link time and needs no load-time relocation.
                if (!TryTlsTemplateOffset(resolution, tlsOffset, tobj, tsym, out ulong templateOffset))
                    throw new ElfLinkException($"Thread-local symbol '{tsym.Name}' referenced through the GOT has no template section.");
                int slotByte = 24 + boundImports.Count * 8 + i * 8;
                BinaryPrimitives.WriteUInt64LittleEndian(gotBytes.AsSpan(slotByte), (ulong)((long)templateOffset - (long)tlsAlignedMem));
                continue;
            }
            if (importByName.TryGetValue(gotDataOrder[i], out Import? imp))
                dynRelocs.Add(new DynReloc(slot, RGlobDat, (uint)imp.DynSymIndex, 0));
            else
            {
                // Resolve with the defining object's context so a file-local symbol reaches its true
                // address, not the global table (which holds only global and weak names). An unresolved
                // weak target keeps its zero-initialized slot, with no base-relative fixup, so that the
                // address-taken idiom sees null rather than the load base.
                (ElfObject o, int si) = gotDataSym[i];
                if (ProducesDynReloc(resolution, importByName, o.Symbols[si]))
                    dynRelocs.Add(new DynReloc(slot, RRelative, 0, SymbolValue(resolution, importByName, SectionAddr, o, o.Symbols[si], linkerDefined)));
            }
        }

        // The descriptor pairs: which module owns the block is written by a load-time record, and where
        // in that block the variable sits is a distance settled here. The record names no symbol, so the
        // module it answers with is the one carrying the record - this one.
        for (int i = 0; i < tlsPairCount; i++)
        {
            ulong pair = tlsPairAddr + (ulong)i * 16;
            dynRelocs.Add(new DynReloc(pair, RDtpMod64, 0, 0));
            int pairByte = 24 + boundImports.Count * 8 + gotDataOrder.Count * 8 + i * 16;
            // The last pair, when a local-dynamic base asked for one, names the block itself and so
            // carries no distance; the rest carry the distance to the variable they were made for.
            ulong within = i < tlsPairOrder.Count ? tlsPairOrder[i] : 0;
            BinaryPrimitives.WriteUInt64LittleEndian(gotBytes.AsSpan(pairByte + 8), within);
        }

        // The process parameters name the three blocks. A built module leaves the pointers zero in the
        // image and fills them in at load time, because the addresses are only known once the module is
        // placed - which is why the block reads as all zeros in a finished module and is still not the
        // same as one that has no pointers at all. A library's module parameters have no such fields.
        if (kind == ModuleKind.Executable)
        {
            dynRelocs.Add(new DynReloc(procAddr + LibcParamOffset, RRelative, 0, paramBlocksAddr + (ulong)ParamBlockOffsets[0]));
            dynRelocs.Add(new DynReloc(procAddr + KernelMemParamOffset, RRelative, 0, paramBlocksAddr + (ulong)ParamBlockOffsets[1]));
            dynRelocs.Add(new DynReloc(procAddr + KernelFsParamOffset, RRelative, 0, paramBlocksAddr + (ulong)ParamBlockOffsets[2]));
        }
        // The C library's block names the three replacement tables, and the marker recording which
        // library the module was linked against - that one is imported, so it is bound by name.
        dynRelocs.Add(new DynReloc(paramBlocksAddr + MallocReplacePointer, RRelative, 0, paramBlocksAddr + (ulong)ParamBlockOffsets[3]));
        dynRelocs.Add(new DynReloc(paramBlocksAddr + NewReplacePointer, RRelative, 0, paramBlocksAddr + (ulong)ParamBlockOffsets[4]));
        dynRelocs.Add(new DynReloc(paramBlocksAddr + TlsMallocReplacePointer, RRelative, 0, paramBlocksAddr + (ulong)ParamBlockOffsets[5]));
        // How large the C library may let its heap grow, and the flag that lifts its own limit. Left
        // unnamed, the library uses a built-in quarter of a megabyte for the whole process, and every
        // allocation past that simply fails - which reads as the runtime running out of memory while
        // the machine has hundreds of megabytes free.
        dynRelocs.Add(new DynReloc(paramBlocksAddr + LibcHeapSizePointer, RRelative, 0, paramBlocksAddr + (ulong)HeapSizeValueOffset));
        dynRelocs.Add(new DynReloc(paramBlocksAddr + LibcHeapExtendedPointer, RRelative, 0, paramBlocksAddr + (ulong)HeapExtendedValueOffset));
        if (importByName.TryGetValue(CompatEmitter.LibcMarkerName, out Import? marker))
            dynRelocs.Add(new DynReloc(paramBlocksAddr + LibcMarkerPointer, RAbs64, (uint)marker.DynSymIndex, 0));

        // Order the table so every base-relative record leads it: the loader treats the first
        // relative-count records as relative and fast-paths them.
        int relativeCount = 0, w = 0;
        foreach (DynReloc d in dynRelocs)
            if (d.Type == RRelative) { WriteRela(relaDynBytes, w++, d); relativeCount++; }
        foreach (DynReloc d in dynRelocs)
            if (d.Type != RRelative) WriteRela(relaDynBytes, w++, d);

        // Build the exception-frame index from the relocated frame bytes: their function pointers are
        // now resolved to runtime addresses.
        byte[] ehFrameHdr = [];
        if (ehFrameHdrSize > 0)
        {
            var entries = new List<EhFrame.Entry>();
            bool ok = true;
            foreach ((ElfObject obj, int i) in ehFrames)
            {
                byte[] bytes = sectionData.TryGetValue((obj, i), out byte[]? d) ? d : obj.Sections[i].Data;
                if (!EhFrame.TryParse(bytes, SectionAddr(obj, i), entries)) { ok = false; break; }
            }
            if (ok && entries.Count > 0)
                ehFrameHdr = EhFrame.Build(ehFrameHdrAddr, SectionAddr(ehFrames[0].Obj, ehFrames[0].Index), entries);
        }
        // Section boundaries and the following segment were settled from this size before relocation.
        // Refuse an inconsistent index instead of publishing addresses for bytes that were not emitted.
        if (ehFrameHdr.Length != ehFrameHdrSize)
            throw new ElfLinkException("The relocated exception-frame index does not match its planned section span.");

        // Fill each export's address now that the layout is fixed.
        foreach ((int index, ElfObject obj, ElfSymbol sym) in exportDynIndex)
            BinaryPrimitives.WriteUInt64LittleEndian(dynsymBytes.AsSpan(index * 24 + 8),
                SymbolValue(resolution, importByName, SectionAddr, obj, sym, linkerDefined));

        // The init/fini arrays sit in the data segment. A library advertises its init array so the loader
        // runs its constructors; an executable runs its own from the entry below, so its init-array tag is
        // left off.
        (ulong Address, ulong Size) initArray = haveInit ? (relroDataAddr + initArrayOff, initArrayEnd - initArrayOff) : (0, 0);
        (ulong Address, ulong Size) finiArray = haveFini ? (relroDataAddr + finiArrayOff, finiArrayEnd - finiArrayOff) : (0, 0);

        // The entry is the start routine itself: it sets the C library up, runs the constructors, and
        // calls main, in that order. Nothing is placed in front of it.
        ulong entry = 0;
        if (!string.IsNullOrEmpty(entrySymbol) && resolution.Defined.TryGetValue(entrySymbol, out ElfObject? eo))
            foreach (ElfSymbol s in eo.Symbols)
                if (!s.IsUndefined && s.Name == entrySymbol)
                    entry = SymbolValue(resolution, importByName, SectionAddr, eo, s, linkerDefined);

        // Whatever runs the constructors has to run them once. The two ways of naming them are the
        // setup routine and the array, and a module that names both under one array has them run twice
        // - which for the runtime means registering itself twice and for anything else means whatever
        // running a constructor twice means. A module built by the reference toolchain names both, and
        // they cover different sets: its setup routine walks the older constructor lists and never the
        // array. So the sets are kept disjoint here too, by naming only one of them per kind.
        //
        // An executable has an entry of its own and the loader runs no array for it, so its setup
        // routine is a walker over the array, called from the entry once the C library is up. A library
        // has no entry, so the loader runs its array directly and the setup routine has nothing left to
        // do. The teardown routine returns at once either way, having nothing to undo.
        if (kind == ModuleKind.Library)
            pltBytes[initOffset] = 0xC3; // ret
        else
            BuildInitWalker(initAddr, initArray.Address, initArray.Address + initArray.Size)
                .CopyTo(pltBytes.AsSpan(initOffset));
        pltBytes[initOffset + EntryStubSize] = 0xC3; // ret

        ulong dynamicInitAddr = libraryInit is { } selectedInit
            ? SymbolValue(resolution, importByName, SectionAddr, selectedInit.Obj, selectedInit.Sym, linkerDefined)
            : initAddr;
        ulong dynamicFiniAddr = libraryFini is { } selectedFini
            ? SymbolValue(resolution, importByName, SectionAddr, selectedFini.Obj, selectedFini.Sym, linkerDefined)
            : finiAddr;
        byte[] dynamicBytes = BuildDynamic(moduleRecords, libraryRecords, moduleInfoName,
            dynsymAddr, dynstrAddr, (ulong)dynstrBytes.Length, hashAddr, (ulong)hashBytes.Length,
            relaAddr, (ulong)relaBytes.Length, gotAddr, dynsymBytes.Length,
            relaDynAddr, (ulong)relaDynBytes.Length, relativeCount,
            hasExports, origFileNameOff, exportLibNameOffset, exportLibId,
            // An executable declares both arrays empty and runs its own constructors from the entry;
            // declaring a real one would have the loader run them again before the entry is reached.
            // A library has no entry of its own, so the loader is what runs its constructors and it
            // names the array it actually carries.
            kind == ModuleKind.Library && !explicitLifecycleOwnsArrays ? initArray : (0, 0),
            kind == ModuleKind.Library && !explicitLifecycleOwnsArrays ? finiArray : (0, 0),
            dynamicInitAddr, dynamicFiniAddr);

        // The first reserved word of the linkage table holds the address of the dynamic table. The two
        // words after it are the loader's own, and it fills them; the module leaves them clear.
        BinaryPrimitives.WriteUInt64LittleEndian(gotBytes, dynamicAddr);

        // Assemble the file.
        // The extent of every output section, for the table the module names its regions with. Sections
        // are already grouped by the name they end up under, so each name is one run.
        var outputSections = new List<(string Name, ulong Addr, ulong Size, ulong Align, bool Exec, bool Writable, bool NoBits)>();
        var outputIndex = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (ElfObject o in resolution.Included)
            for (int i = 0; i < o.Sections.Count; i++)
            {
                ElfSection sec = o.Sections[i];
                if (!sec.IsAlloc || sec.IsTls || !sectionOffsetInGroup.ContainsKey((o, i)) || sec.Size == 0)
                    continue;
                // The guard object is named apart from the group it came from: merged back under that
                // name, the name's run would stretch from the read-only group across to this one and
                // read as a single section spanning both.
                bool isGuard = guard == (o, i);
                string name = isGuard ? GuardSectionName : OutputSectionName(sec.Name);
                ulong at = SectionAddr(o, i);
                if (outputIndex.TryGetValue(name, out int at2))
                {
                    var e = outputSections[at2];
                    ulong lo = Math.Min(e.Addr, at), hi = Math.Max(e.Addr + e.Size, at + sec.Size);
                    outputSections[at2] = (name, lo, hi - lo, Math.Max(e.Align, sec.AddrAlign),
                        e.Exec, e.Writable, e.NoBits && sec.IsNoBits);
                }
                else
                {
                    outputIndex[name] = outputSections.Count;
                    outputSections.Add((name, at, sec.Size, isGuard ? SegAlign : sec.AddrAlign,
                        sec.IsExecutable, sec.IsWritable || isGuard, sec.IsNoBits));
                }
            }
        outputSections.Sort((a, b) => a.Addr.CompareTo(b.Addr));

        return WriteFile(resolution, kind, entry, sectionData, SectionAddr, outputSections,
            text: (textAddr, textLen), pltAddr, pltBytes,
            roAddr, roLen, dynlibAddr, dynsymAddr, dynsymBytes, dynstrAddr, dynstrBytes, hashAddr, hashBytes,
            relaAddr, relaBytes, relaDynAddr, relaDynBytes, procAddr, procParam, paramBlocksAddr, paramBlocks, noteAddr, note,
            ehFrameHdrAddr, ehFrameHdr, hasTls, tlsAddr, tlsFileLen, tlsMemLen, tlsAlign,
            relroAddr, relroEndAddr, dataAddr, dataLen, dataSegMem, gotAddr, gotBytes,
            dynamicAddr, dynamicBytes, comment, versionBlob);
    }

    // Thread-local access is settled at link time by working out where a variable sits relative to the
    // thread pointer and writing that distance into the code. That distance is only knowable for the
    // main executable, whose block is placed first and whose offset the system therefore knows in
    // advance. Every other module has its block placed after the ones already loaded, at a position
    // that is not known until it is loaded, so a distance written in advance points into some other
    // module's storage - it reads and writes another module's variables, and nothing reports a fault.
    //
    // A library therefore leaves the sequences alone: the pair of table slots is filled as the module
    // loads and the helper reads them. The two forms that ask for a distance from the thread pointer
    // have no such indirection to hide behind, so an object using one of them is refused rather than
    // written, because what would be written is wrong in a way nothing later detects.
    private static void RefuseThreadLocalsInALibrary(LinkResolution resolution, ModuleKind kind, ulong tlsSize)
    {
        if (kind != ModuleKind.Library || tlsSize == 0)
            return;
        foreach (ElfObject obj in resolution.Included)
            foreach (KeyValuePair<int, IReadOnlyList<ElfRelocation>> kv in obj.Relocations)
            {
                if (kv.Key >= obj.Sections.Count || !obj.Sections[kv.Key].IsAlloc)
                    continue;
                foreach (ElfRelocation r in kv.Value)
                    if (r.Type is RelType.GotTpOff or RelType.TpOff32 or RelType.TpOff64)
                        throw new ElfLinkException(
                            $"{obj.Origin}: a thread-local reference of the form this object uses cannot " +
                            "be written into a library. It asks for the distance from the thread pointer, " +
                            "which is settled only once every module carrying such a block has been " +
                            "placed; written in advance it points into another module's storage.");
            }
    }

    private static Dictionary<(ElfObject, int), byte[]> RelocateApp(
        LinkResolution resolution, Dictionary<string, Import> importByName, Func<ElfObject, int, ulong> sectionAddr,
        ulong gotDataAddr, Dictionary<string, int> gotDataIndex, List<DynReloc> dynRelocs,
        Dictionary<(ElfObject, int), ulong> tlsOffset, ulong tlsAlignedMem,
        IReadOnlyDictionary<string, ulong> linkerDefined,
        ModuleKind kind, ulong tlsPairAddr, Dictionary<ulong, int> tlsPairIndex, int tlsPairCount)
    {
        var result = new Dictionary<(ElfObject, int), byte[]>();
        foreach (ElfObject obj in resolution.Included)
        {
            foreach (KeyValuePair<int, IReadOnlyList<ElfRelocation>> kv in obj.Relocations)
            {
                int idx = kv.Key;
                if (idx >= obj.Sections.Count) continue;
                ElfSection sec = obj.Sections[idx];
                if (!sec.IsAlloc) continue;
                // A section another object carried first is not in the output, so it has no address to
                // relocate against and nothing reads what it would have held. Its references are to the
                // rest of its own group, which went with it.
                if (resolution.DroppedSections.Contains((obj, idx))) continue;
                byte[] bytes = sec.IsNoBits ? new byte[sec.Size] : (byte[])sec.Data.Clone();
                ulong secAddr = sectionAddr(obj, idx);

                // A general- or local-dynamic thread-local sequence is a lea followed by a call
                // __tls_get_addr. Relaxing the lea rewrites both instructions, so the call's relocation is
                // folded away rather than applied on its own. The call sits eight bytes past a general-
                // dynamic lea's relocation and five past a local-dynamic one.
                HashSet<ulong>? foldedTlsCall = FoldedTlsCalls(kv.Value, kind);

                foreach (ElfRelocation r in kv.Value)
                {
                    // R_X86_64_NONE uses no symbol and changes no bytes (including null-symbol records).
                    if (r.Type == RelType.None) continue;
                    if (r.SymbolIndex >= (uint)obj.Symbols.Count) continue;
                    ElfSymbol sym = obj.Symbols[(int)r.SymbolIndex];
                    ulong place = secAddr + r.Offset;
                    int at = (int)r.Offset;

                    if (foldedTlsCall is not null && foldedTlsCall.Contains(r.Offset))
                        continue; // the __tls_get_addr call, folded into the local-exec load below

                    if (r.Type == RelType.TlsGd && kind == ModuleKind.Library)
                    {
                        // Leave the pair alone and point the lea at this variable's descriptor. Which
                        // module owns the block is filled as the module loads, so nothing here can say
                        // where the variable ends up - only which pair to ask.
                        if (!TryTlsTemplateOffset(resolution, tlsOffset, obj, sym, out ulong gdBlockOff))
                            throw new ElfLinkException($"Thread-local symbol '{sym.Name}' has no template section.");
                        ulong pair = tlsPairAddr + (ulong)tlsPairIndex[gdBlockOff] * 16;
                        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(pair + (ulong)r.Addend - place));
                        continue;
                    }

                    if (r.Type == RelType.TlsLd && kind == ModuleKind.Library)
                    {
                        // The same, for the module's own block: the pair carries no distance, and each
                        // member is reached by the distance written into the code below.
                        ulong pair = tlsPairAddr + (ulong)tlsPairCount * 16;
                        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(pair + (ulong)r.Addend - place));
                        continue;
                    }

                    if (r.Type == RelType.TlsGd)
                    {
                        // Relax to local-exec: replace the 16-byte lea/call pair with a read of the thread
                        // pointer and a fixed offset load, leaving the address in rax as the call did.
                        if (at - 4 < 0 || at + 12 > bytes.Length)
                            throw new ElfLinkException($"{obj.Origin}: a thread-local sequence on '{sym.Name}' runs past the section.");
                        if (!TryTlsTemplateOffset(resolution, tlsOffset, obj, sym, out ulong gdTemplateOff))
                            throw new ElfLinkException($"Thread-local symbol '{sym.Name}' has no template section.");
                        long le = (long)gdTemplateOff - (long)tlsAlignedMem;
                        ReadOnlySpan<byte> localExec = [0x64, 0x48, 0x8B, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x80];
                        localExec.CopyTo(bytes.AsSpan(at - 4)); // mov %fs:0,%rax ; lea ...(%rax),%rax
                        BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(at + 8), checked((int)le));
                        continue;
                    }

                    if (r.Type == RelType.TlsLd)
                    {
                        // Relax the module-base lookup to a thread-pointer read: the block is this module's,
                        // so each member (its DTPOFF relocations below) becomes a local-exec offset from the
                        // thread pointer. The nop prefixes keep the replacement the same size as the pair.
                        if (at - 3 < 0 || at + 9 > bytes.Length)
                            throw new ElfLinkException($"{obj.Origin}: a thread-local base sequence on '{sym.Name}' runs past the section.");
                        ReadOnlySpan<byte> threadPointer = [0x66, 0x66, 0x66, 0x64, 0x48, 0x8B, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00];
                        threadPointer.CopyTo(bytes.AsSpan(at - 3)); // (nop) mov %fs:0,%rax
                        continue;
                    }

                    int width = r.Type is RelType.R64 or RelType.TpOff64 or RelType.Pc64 or RelType.DtpOff64 ? 8 : 4;
                    if (at < 0 || at + width > bytes.Length) continue;

                    if (RelType.IsGotPcRel(r.Type) || r.Type == RelType.GotTpOff)
                    {
                        // A relaxable load of a symbol settled here is rewritten into a direct
                        // reference, which is what removes the table slot the collection pass declined
                        // to allocate. The two decisions are made by the same pair of checks.
                        if (ResolvesLocally(resolution, importByName, sym)
                            && CanRelaxGot(obj.Sections[idx], r))
                        {
                            ulong target = SymbolValue(
                                resolution, importByName, sectionAddr, obj, sym, linkerDefined);
                            RelaxGot(bytes, at, (long)(target + (ulong)r.Addend) - (long)place);
                            continue;
                        }
                        // The reference is fixed to point at the symbol's GOT entry, not the symbol. The
                        // relaxable data variants (GOTPCRELX/REX_GOTPCRELX) and the initial-exec
                        // thread-local variant (GOTTPOFF) all resolve to the same slot address; what the
                        // slot holds (an address versus a thread-pointer offset) is filled separately.
                        // Only named symbols get a GOT slot (the collection pass skips empty names), so a
                        // GOT reference to an unnamed section symbol is unsupported rather than a crash.
                        if (!gotDataIndex.TryGetValue(sym.Name, out int gotDataSlot))
                            throw new ElfLinkException("GOT-relative relocation against an unnamed symbol is not supported.");
                        ulong gotSlot = gotDataAddr + (ulong)gotDataSlot * 8;
                        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(gotSlot + (ulong)r.Addend - place));
                        continue;
                    }

                    if (r.Type is RelType.TpOff32 or RelType.TpOff64 or RelType.DtpOff32 or RelType.DtpOff64)
                    {
                        // Local-exec thread-local reference: the value is the symbol's offset within the
                        // template minus the aligned template size, since the block sits below the thread
                        // pointer on this target. A module-block offset (DTPOFF, once its base is relaxed to
                        // the thread pointer above) resolves to the same value.
                        if (!TryTlsTemplateOffset(resolution, tlsOffset, obj, sym, out ulong templateOff))
                            throw new ElfLinkException($"Thread-local symbol '{sym.Name}' has no template section.");
                        // A distance from the module's own block is what a library writes, since its
                        // block is found through a descriptor rather than from the thread pointer. An
                        // executable measures from the thread pointer, below which its block sits.
                        long tp = kind == ModuleKind.Library
                            ? (long)templateOff + r.Addend
                            : (long)templateOff - (long)tlsAlignedMem + r.Addend;
                        if (r.Type is RelType.TpOff64 or RelType.DtpOff64)
                            BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at), (ulong)tp);
                        else
                            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), unchecked((uint)(int)tp));
                        continue;
                    }

                    ulong s = SymbolValue(resolution, importByName, sectionAddr, obj, sym, linkerDefined);
                    switch (r.Type)
                    {
                        case RelType.None:
                            break;
                        case RelType.R64:
                            BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at), s + (ulong)r.Addend);
                            // An absolute 64-bit reference needs a load-time fixup: a symbol record for
                            // an imported target, a base-relative record for a defined one. An
                            // unresolved weak reference resolves to absolute zero and needs no fixup, so
                            // leave it out rather than read the load base.
                            //
                            // A function whose address is settled by calling a routine that chooses one
                            // cannot be expressed at all: the record that would say so is of a kind the
                            // loader refuses outright, and it refuses the whole module rather than that
                            // one record. Writing a base-relative record instead would leave the address
                            // of the chooser in the slot and call it as though it were the function. So
                            // it is refused here, where it can still be read.
                            if (sym.Type == SymType.GnuIfunc && !sym.IsUndefined)
                                throw new ElfLinkException(
                                    $"'{sym.Name}' has its address settled by a routine that chooses one, " +
                                    "and this platform's loader has no record for that.");
                            else if (sym.IsUndefined && importByName.TryGetValue(sym.Name, out Import? imp))
                                dynRelocs.Add(new DynReloc(place, RAbs64, (uint)imp.DynSymIndex, (ulong)r.Addend));
                            else if (ProducesDynReloc(resolution, importByName, sym))
                                dynRelocs.Add(new DynReloc(place, RRelative, 0, s + (ulong)r.Addend));
                            break;
                        case RelType.Pc32:
                        case RelType.Plt32:
                            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(s + (ulong)r.Addend - place)); break;
                        case RelType.Pc64:
                            // The distance from here to the symbol, which only means anything when the
                            // symbol is in this module. For one that comes from elsewhere there is no
                            // distance to write: its address is not known until the module is loaded,
                            // and nothing here records that it needs filling in, so the reference was
                            // quietly left pointing at whatever the arithmetic produced from an address
                            // of zero. Refuse it rather than write that.
                            if (sym.IsUndefined && importByName.ContainsKey(sym.Name))
                                throw new ElfLinkException(
                                    $"{obj.Origin}: '{sym.Name}' comes from another module, and a " +
                                    "reference measured as a distance to it cannot be settled here.");
                            BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at), (ulong)((long)(s + (ulong)r.Addend) - (long)place)); break;
                        case RelType.R32:
                        case RelType.R32S:
                            throw new ElfLinkException(
                                $"A 32-bit absolute relocation on '{sym.Name}' cannot be fixed up in a relocatable module; compile position-independent code.");
                        default:
                            // Fail loudly on an unhandled type rather than silently leaving the target
                            // bytes at their compiler placeholder, which would miscompile without warning.
                            throw new ElfLinkException(
                                $"{obj.Origin}: unsupported relocation type {r.Type} on '{sym.Name}'. The linker resolves absolute, PC-relative, PLT, GOT-relative and local-exec thread-local references.");
                    }
                }
                result[(obj, idx)] = bytes;
            }
            for (int i = 0; i < obj.Sections.Count; i++)
                if (obj.Sections[i].IsAlloc && !result.ContainsKey((obj, i)))
                    result[(obj, i)] = obj.Sections[i].IsNoBits ? new byte[obj.Sections[i].Size] : (byte[])obj.Sections[i].Data.Clone();
        }
        return result;
    }

    private static ulong SymbolValue(
        LinkResolution resolution, Dictionary<string, Import> importByName,
        Func<ElfObject, int, ulong> sectionAddr, ElfObject obj, ElfSymbol sym,
        IReadOnlyDictionary<string, ulong>? linkerDefined = null)
    {
        // An absolute symbol's value is its final address; it is not relative to any section.
        if (sym.SectionIndex == ShnAbs) return sym.Value;
        if (sym.Type == SymType.Section) return sectionAddr(obj, sym.SectionIndex);
        // A symbol defined in a section another object carried first has no address of its own: the
        // reference resolves through the copy that was kept, the same way an undefined one does.
        if (!sym.IsUndefined && !resolution.DroppedSections.Contains((obj, sym.SectionIndex)))
            return sectionAddr(obj, sym.SectionIndex) + sym.Value;
        if (resolution.Defined.TryGetValue(sym.Name, out ElfObject? defObj))
            foreach (ElfSymbol d in defObj.Symbols)
                if (!d.IsUndefined && d.Name == sym.Name)
                    return d.SectionIndex == ShnAbs ? d.Value : sectionAddr(defObj, d.SectionIndex) + d.Value;
        if (importByName.TryGetValue(sym.Name, out Import? imp)) return imp.PltAddress;
        // Routines the linker writes itself rather than taking from an object: the constructor walker
        // and the teardown routine, which only the linker can place.
        if (linkerDefined is not null && linkerDefined.TryGetValue(sym.Name, out ulong provided)) return provided;
        if (TryEncapsulationAddress(resolution, sectionAddr, sym.Name, out ulong enc)) return enc;
        if (sym.IsWeak) return 0;
        throw new ElfLinkException($"Unresolved symbol '{sym.Name}'.");
    }

    // Whether a symbol is settled inside this module, so a reference to it can be written directly
    // instead of going through the global-offset table. An imported one cannot: only the loader knows
    // where it lands. An unresolved weak one cannot either - it reads as absolute zero, which no
    // instruction-relative form can express.
    private static bool ResolvesLocally(
        LinkResolution resolution, Dictionary<string, Import> importByName, ElfSymbol sym)
        => !importByName.ContainsKey(sym.Name)
           && (!sym.IsUndefined || resolution.Defined.ContainsKey(sym.Name));

    // Whether a relaxable table load can become a direct reference. The compiler marks the forms it
    // knows are safe to rewrite; the ones that appear are a load through the table, an indirect call,
    // and an indirect jump. Anything else keeps its slot rather than being rewritten blind. The
    // relocation names the four-byte displacement, so the opcode sits two bytes in front of it.
    private static bool CanRelaxGot(ElfSection section, ElfRelocation r)
    {
        if (r.Type is not (RelType.GotPcRelX or RelType.RexGotPcRelX)) return false;
        if (section.IsNoBits || section.Data is null) return false;
        long at = (long)r.Offset;
        if (at < 2 || at + 4 > section.Data.Length) return false;
        byte op = section.Data[at - 2], modRm = section.Data[at - 1];
        if (op == 0x8B) return true;                                    // load through the table
        if (op == 0xFF && modRm == 0x15) return true;                   // indirect call
        if (op == 0xFF && modRm == 0x25) return at + 5 <= section.Data.Length; // indirect jump
        return false;
    }

    // Rewrites a relaxable table load into a direct reference and writes its displacement.
    //   mov  sym@GOTPCREL(%rip), %reg  ->  lea sym(%rip), %reg
    //   call *sym@GOTPCREL(%rip)       ->  addr32 call sym
    //   jmp  *sym@GOTPCREL(%rip)       ->  jmp sym; nop
    // The jump form is a byte shorter than what it replaces, so its displacement moves back one byte
    // and a no-op fills the tail.
    private static void RelaxGot(byte[] bytes, int at, long value)
    {
        byte op = bytes[at - 2], modRm = bytes[at - 1];
        if (op == 0xFF && modRm == 0x15)
        {
            bytes[at - 2] = 0x67;
            bytes[at - 1] = 0xE8;
            BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(at), checked((int)value));
        }
        else if (op == 0xFF && modRm == 0x25)
        {
            bytes[at - 2] = 0xE9;
            bytes[at + 3] = 0x90;
            BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(at - 1), checked((int)(value + 1)));
        }
        else
        {
            bytes[at - 2] = 0x8D;
            BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(at), checked((int)value));
        }
    }

    // A general- or local-dynamic thread-local sequence is a lea followed by a call __tls_get_addr.
    // Relaxing the lea rewrites both instructions, so the call's relocation is folded away rather than
    // applied on its own - and the helper it names is not called at all, so it takes no linkage-table
    // slot. The call sits eight bytes past a general-dynamic lea's relocation and five past a
    // local-dynamic one.
    private static HashSet<ulong>? FoldedTlsCalls(IReadOnlyList<ElfRelocation> relocations, ModuleKind kind)
    {
        // A library keeps the pair and lets the helper run, so nothing is folded there.
        if (kind == ModuleKind.Library)
            return null;
        HashSet<ulong>? folded = null;
        foreach (ElfRelocation probe in relocations)
        {
            if (probe.Type == RelType.TlsGd) (folded ??= []).Add(probe.Offset + 8);
            else if (probe.Type == RelType.TlsLd) (folded ??= []).Add(probe.Offset + 5);
        }
        return folded;
    }

    // Whether an absolute or GOT-data reference to this symbol emits a load-time dynamic relocation.
    // A symbol defined here or in another included object needs a base-relative fixup, and an imported
    // one resolves through the dynamic symbol table. An unresolved weak reference resolves to absolute
    // zero and emits nothing, so an address-taken weak symbol reads as null rather than the load base.
    // Whether a table slot holding this symbol's address needs a load-time fixup. Everything the link
    // can name an address for does; only a weak name nothing defines is left null, which is what the
    // address-taken idiom tests for.
    //
    // The two the linker settles itself have to be counted here as well. A section-boundary symbol is
    // the clearest case: the runtime reads the start and end of its own compiled code out of this table
    // and registers the range with itself. Left without a fixup the slots stay zero, the range measures
    // nothing, registration is refused, and the module leaves main with a failure before a line of
    // application code - with nothing to say why.
    private static bool ProducesDynReloc(LinkResolution resolution, Dictionary<string, Import> importByName, ElfSymbol sym)
        => !sym.IsUndefined
           || importByName.ContainsKey(sym.Name)
           || resolution.Defined.ContainsKey(sym.Name)
           || Linker.LinkerProvided.Contains(sym.Name)
           || NamesACarriedSection(resolution, sym.Name);

    // Whether the name is a section boundary whose section some included object actually carries. This
    // asks only which sections exist, not where they land, so it can be answered before the layout is
    // settled - which is where the table has to be sized.
    private static bool NamesACarriedSection(LinkResolution resolution, string name)
    {
        string section;
        if (name.StartsWith("__start_", StringComparison.Ordinal)) section = name["__start_".Length..];
        else if (name.StartsWith("__stop_", StringComparison.Ordinal)) section = name["__stop_".Length..];
        else return false;
        if (section.Length == 0)
            return false;
        foreach (ElfObject o in resolution.Included)
            foreach (ElfSection s in o.Sections)
                if (s.IsAlloc && s.Name == section)
                    return true;
        return false;
    }

    // The address of a section-boundary symbol: the lowest start (for __start_) or the highest end (for
    // __stop_) of the allocated sections that carry the named section across every included object.
    private static bool TryEncapsulationAddress(
        LinkResolution resolution, Func<ElfObject, int, ulong> sectionAddr, string name, out ulong addr)
    {
        addr = 0;
        bool isStop;
        string section;
        if (name.StartsWith("__start_", StringComparison.Ordinal)) { isStop = false; section = name["__start_".Length..]; }
        else if (name.StartsWith("__stop_", StringComparison.Ordinal)) { isStop = true; section = name["__stop_".Length..]; }
        else return false;
        if (section.Length == 0)
            return false;

        // The boundary spans the section's start to its end. The section is placed per object rather
        // than coalesced across objects, so a contiguous span is only well defined when a single object
        // carries the section. That is the case for the sections these symbols mark (the compiler emits
        // each in one object); a second contributor would leave the span covering unrelated data in
        // between, so it is refused rather than silently miscomputed.
        bool found = false;
        int contributors = 0;
        ulong min = ulong.MaxValue, max = 0;
        foreach (ElfObject o in resolution.Included)
            for (int i = 0; i < o.Sections.Count; i++)
            {
                ElfSection s = o.Sections[i];
                if (!s.IsAlloc || s.Name != section)
                    continue;
                ulong a = sectionAddr(o, i);
                if (a < min) min = a;
                if (a + s.Size > max) max = a + s.Size;
                found = true;
                contributors++;
            }
        if (!found)
            return false;
        if (contributors > 1)
            throw new ElfLinkException(
                $"Section '{section}' is carried by more than one object; the boundary symbol '{name}' would span unrelated data. Coalescing same-named sections is not supported.");
        addr = isStop ? max : min;
        return true;
    }

    // Finds the object and symbol that define a global name, for resolving an export to its address.
    private static bool TryFindDefinedSymbol(LinkResolution resolution, string name, out ElfObject? obj, out ElfSymbol? sym)
    {
        obj = null;
        sym = null;
        if (!resolution.Defined.TryGetValue(name, out ElfObject? defObj))
            return false;
        foreach (ElfSymbol s in defObj.Symbols)
            if (!s.IsUndefined && s.Name == name)
            {
                obj = defObj;
                sym = s;
                return true;
            }
        return false;
    }

    // The offset of a thread-local symbol within the template: its section's template offset plus the
    // symbol value, resolving a defined symbol through its own object.
    private static bool TryTlsTemplateOffset(
        LinkResolution resolution, Dictionary<(ElfObject, int), ulong> tlsOffset, ElfObject obj, ElfSymbol sym, out ulong result)
    {
        result = 0;
        if (!sym.IsUndefined)
        {
            if (!tlsOffset.TryGetValue((obj, sym.SectionIndex), out ulong o)) return false;
            result = o + sym.Value;
            return true;
        }
        if (resolution.Defined.TryGetValue(sym.Name, out ElfObject? defObj))
            foreach (ElfSymbol d in defObj.Symbols)
                if (!d.IsUndefined && d.Name == sym.Name && tlsOffset.TryGetValue((defObj, d.SectionIndex), out ulong o))
                {
                    result = o + d.Value;
                    return true;
                }
        return false;
    }

    // Writes one 24-byte RELA record: the slot address, the packed symbol/type word, and the addend.
    private static void WriteRela(byte[] table, int index, DynReloc d)
    {
        int b = index * 24;
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b), d.Offset);
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b + 8), ((ulong)d.Sym << 32) | d.Type);
        BinaryPrimitives.WriteUInt64LittleEndian(table.AsSpan(b + 16), d.Addend);
    }

    private static byte[] WriteFile(
        LinkResolution resolution, ModuleKind kind, ulong entry,
        Dictionary<(ElfObject, int), byte[]> sectionData, Func<ElfObject, int, ulong> sectionAddr,
        List<(string Name, ulong Addr, ulong Size, ulong Align, bool Exec, bool Writable, bool NoBits)> outputSections,
        (ulong Addr, ulong Len) text, ulong pltAddr, byte[] plt,
        ulong roAddr, ulong roLen, ulong dynlibAddr, ulong dynsymAddr, byte[] dynsym, ulong dynstrAddr, byte[] dynstr,
        ulong hashAddr, byte[] hash, ulong relaAddr, byte[] rela, ulong relaDynAddr, byte[] relaDyn,
        ulong procAddr, byte[] proc, ulong paramBlocksAddr, byte[] paramBlocks, ulong noteAddr, byte[] note, ulong ehFrameHdrAddr, byte[] ehFrameHdr,
        bool hasTls, ulong tlsAddr, ulong tlsFileLen, ulong tlsMemLen, ulong tlsAlign,
        ulong relroAddr, ulong relroEndAddr, ulong dataAddr, ulong dataLen, ulong dataSegMem,
        ulong gotAddr, byte[] got, ulong dynamicAddr, byte[] dynamic,
        byte[] comment, byte[] versionBlob)
    {
        // Five load segments: [code|plt] execute-only, [rodata|eh_frame] read, [got|procparam]
        // read-write covered by a relro header, [data|bss] read-write, and the dynamic-linking tables
        // in a segment with no protection. Each group starts a page past the one before it. Where a
        // group reserves more memory than it stores the two distances part company, which is fine
        // while both stay page-aligned: the loader maps by offset, address and stored size.
        ulong textFileOff = SegAlign;
        ulong textSegEnd = pltAddr + (ulong)plt.Length;
        ulong roFileOff = textFileOff + Align(textSegEnd, SegAlign);
        ulong roSegEnd = ehFrameHdr.Length > 0 ? ehFrameHdrAddr + (ulong)ehFrameHdr.Length : roAddr + roLen;
        ulong relroFileOff = roFileOff + Align(roSegEnd - roAddr, SegAlign);
        ulong relroLen = relroEndAddr - relroAddr;
        ulong dataFileOff = relroFileOff + Align(relroLen, SegAlign);
        // The linking group follows the writable group's stored bytes in the file, at the first offset
        // that carries the same page offset as its address - the relation every mapped group keeps, held
        // here too even though this group is not mapped. Rounding to the next page instead would put the
        // group on a page of its own and its address past the end of the writable group's memory. The
        // address has to fall inside that memory and the offset must never be page-aligned.
        ulong dataStoredEnd = dataFileOff + dataLen;
        ulong dynlibFileOff = (dataStoredEnd & ~(SegAlign - 1)) + (dynlibAddr & (SegAlign - 1));
        // Past the writable group's last stored byte, and never at its own offset - a group that stores
        // nothing would otherwise start exactly where that one does, and two load segments sharing a file
        // offset overlap.
        if (dynlibFileOff < dataStoredEnd || dynlibFileOff == dataFileOff) dynlibFileOff += SegAlign;
        // The dynamic table closes the linking group, and nothing follows it inside that group. The
        // loader works out the layout of this group from the table itself, so bytes left past the end
        // of the table are bytes the layout does not account for. Every module measured ends the group
        // exactly where its dynamic table ends.
        ulong dynlibSegEnd = dynamicAddr + (ulong)dynamic.Length;

        // The file tail, outside every load segment: the comment, which the container stores as a
        // segment of its own, then the version record, then the reserved note. Each is 16-aligned and
        // the image ends exactly at the extent the last program header records.
        ulong commentFileOff = Align(dynlibFileOff + (dynlibSegEnd - dynlibAddr), 16);
        ulong versionFileOff = Align(commentFileOff + (ulong)comment.Length, 16);
        ulong tailNoteFileOff = Align(versionFileOff + (ulong)versionBlob.Length, 16);

        // The section table, past everything a segment covers. Every module measured carries one, and a
        // module carrying none reads back as a file with no sections at all rather than a stripped one -
        // the container drops the table with the rest of what lies outside the segments, which is why
        // those modules name a table that is past the end of what they carry. Naming the regions also
        // lets the module be read back by the tools that read a built one.
        var shstr = new StringTable();
        var sections = new List<(int Name, uint Type, ulong Flags, ulong Addr, ulong Off, ulong Size, uint Link, uint Info, ulong Align, ulong EntSize)>
        {
            (shstr.Add(""), 0, 0, 0, 0, 0, 0, 0, 0, 0),
        };
        var sectionIndex = new Dictionary<string, int>(StringComparer.Ordinal);
        void AddSection(string name, uint type, ulong flags, ulong addr, ulong off, ulong size,
                        string? link = null, uint info = 0, ulong align = 1, ulong entSize = 0)
        {
            if (size == 0) return;
            sectionIndex[name] = sections.Count;
            sections.Add((shstr.Add(name), type, flags, addr, off, size,
                (uint)(link is not null && sectionIndex.TryGetValue(link, out int l) ? l : 0), info, align, entSize));
        }
        const uint ShtProgBits = 1, ShtStrTab = 3, ShtRela = 4, ShtHash = 5, ShtDynamic = 6,
                   ShtNote = 7, ShtNoBits = 8, ShtDynSym = 11;
        const ulong ShfWrite = 1, ShfAlloc = 2, ShfExec = 4, ShfTls = 0x400;
        // Every output section the objects contributed, named as it ends up.
        foreach ((string nm, ulong addr, ulong size, ulong al, bool ex, bool wr, bool nb) in outputSections)
        {
            ulong groupOff = ex ? textFileOff
                : addr >= relroAddr && addr < relroEndAddr ? relroFileOff
                : wr ? dataFileOff : roFileOff;
            ulong groupBase = ex ? text.Addr
                : addr >= relroAddr && addr < relroEndAddr ? relroAddr
                : wr ? dataAddr : roAddr;
            AddSection(nm, nb ? ShtNoBits : ShtProgBits,
                ShfAlloc | (ex ? ShfExec : 0) | (wr ? ShfWrite : 0),
                addr, groupOff + (addr - groupBase), size, align: al);
        }
        AddSection(".plt", ShtProgBits, ShfAlloc | ShfExec, pltAddr, textFileOff + (pltAddr - text.Addr), (ulong)plt.Length, align: 16);
        AddSection(".eh_frame_hdr", ShtProgBits, ShfAlloc, ehFrameHdrAddr,
            roFileOff + (ehFrameHdrAddr - roAddr), (ulong)ehFrameHdr.Length, align: 8);
        // The reserved head, and the regions the linker builds rather than taking from an object.
        AddSection(".sce_padding", ShtProgBits, ShfAlloc, text.Addr, textFileOff, ImageHeadReserve, align: 1);
        AddSection(".got", ShtProgBits, ShfAlloc | ShfWrite, gotAddr, relroFileOff + (gotAddr - relroAddr),
            (ulong)got.Length, align: 8);
        AddSection(".sce_process_param", ShtProgBits, ShfAlloc | ShfWrite, procAddr,
            relroFileOff + (procAddr - relroAddr), (ulong)proc.Length, align: 8);
        if (hasTls)
        {
            AddSection(".tdata", ShtProgBits, ShfAlloc | ShfWrite | ShfTls, tlsAddr,
                relroFileOff + (tlsAddr - relroAddr), tlsFileLen, align: tlsAlign);
            AddSection(".tbss", ShtNoBits, ShfAlloc | ShfWrite | ShfTls, tlsAddr + tlsFileLen,
                relroFileOff + (tlsAddr + tlsFileLen - relroAddr), tlsMemLen - tlsFileLen, align: tlsAlign);
        }
        // The linking tables, each named where the dynamic table says it is.
        AddSection(".dynstr", ShtStrTab, ShfAlloc, dynstrAddr, dynlibFileOff + (dynstrAddr - dynlibAddr), (ulong)dynstr.Length, align: 16);
        AddSection(".dynsym", ShtDynSym, ShfAlloc, dynsymAddr, dynlibFileOff + (dynsymAddr - dynlibAddr), (ulong)dynsym.Length,
            link: ".dynstr", info: 1, align: 8, entSize: 24);
        AddSection(".hash", ShtHash, ShfAlloc, hashAddr, dynlibFileOff + (hashAddr - dynlibAddr), (ulong)hash.Length,
            link: ".dynsym", align: 4, entSize: 4);
        AddSection(".rela.plt", ShtRela, ShfAlloc, relaAddr, dynlibFileOff + (relaAddr - dynlibAddr), (ulong)rela.Length,
            link: ".dynsym", align: 8, entSize: 24);
        AddSection(".rela.dyn", ShtRela, ShfAlloc, relaDynAddr, dynlibFileOff + (relaDynAddr - dynlibAddr), (ulong)relaDyn.Length,
            link: ".dynsym", align: 8, entSize: 24);
        AddSection(".note.gnu.build-id", ShtNote, ShfAlloc, noteAddr, dynlibFileOff + (noteAddr - dynlibAddr), (ulong)note.Length, align: 4);
        AddSection(".dynamic", ShtDynamic, ShfAlloc | ShfWrite, dynamicAddr, dynlibFileOff + (dynamicAddr - dynlibAddr),
            (ulong)dynamic.Length, link: ".dynstr", align: 8, entSize: 16);
        // The regions past the last segment: the comment, the version records and the tail note. They
        // carry no address, and naming them is what lets a reader show their contents.
        AddSection(".prodg_meta_data", ShtProgBits, 0, 0, commentFileOff, (ulong)comment.Length, align: 16);
        AddSection(".sceversion", ShtProgBits, 0, 0, versionFileOff, (ulong)versionBlob.Length, align: 1);
        AddSection(".note", ShtNote, 0, 0, tailNoteFileOff, TailNoteLen, align: 4);
        int shstrIndex = sections.Count;
        sections.Add((shstr.Add(".shstrtab"), ShtStrTab, 0, 0, 0, 0, 0, 0, 1, 0));
        byte[] shstrBytes = shstr.ToBytes();
        sections[shstrIndex] = sections[shstrIndex] with { Size = (ulong)shstrBytes.Length };

        ulong shstrFileOff = Align(tailNoteFileOff + TailNoteLen, 16);
        ulong shdrFileOff = Align(shstrFileOff + (ulong)shstrBytes.Length, 8);
        byte[] file = new byte[shdrFileOff + (ulong)sections.Count * 0x40];

        // ELF header.
        file[0] = 0x7F; file[1] = (byte)'E'; file[2] = (byte)'L'; file[3] = (byte)'F';
        file[4] = 2; file[5] = 1; file[6] = 1; file[7] = 9; file[8] = 2;
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x10), kind == ModuleKind.Library ? TypeSceDynamic : TypeSceDynExec);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x12), 0x3E);
        BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(0x14), 1);
        BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(0x18), entry);
        BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(0x20), 0x40);
        BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(0x28), shdrFileOff);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x34), 0x40);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x36), 0x38);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x3A), 0x40);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x3C), (ushort)sections.Count);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x3E), (ushort)shstrIndex);
        // The code, writable, parameter and dynamic-linking load segments, the relro header covering
        // the writable group, the dynamic and process-parameter headers, the comment, the version
        // record, the module note and the reserved note - eleven. The read-only segment is added only
        // when it carries something, and the frame index and thread-local template when present.
        // The thread-local header is carried whether or not the module has thread-local data, so the
        // count comes to fourteen either way. Leaving it out on a module with no thread-local data
        // would produce thirteen, and a module with thirteen does not load.
        bool hasRo = roSegEnd > roAddr;
        int phnum = 12 + (hasRo ? 1 : 0) + (ehFrameHdr.Length > 0 ? 1 : 0);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x38), (ushort)phnum);

        // Program headers.
        int ph = 0x40;
        void WritePh(uint type, uint flags, ulong off, ulong va, ulong filesz, ulong memsz, ulong align)
        {
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(ph), type);
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(ph + 4), flags);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 8), off);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 16), va);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 24), va);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 32), filesz);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 40), memsz);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 48), align);
            ph += 0x38;
        }
        // Execute without read. A load segment that asks for both is refused outright, so the code
        // segment carries the execute bit alone; the loader grants the read access the processor needs
        // when it maps the segment. Read-only content belongs in the segment below, not beside code.
        WritePh(PtLoad, PfX, textFileOff, text.Addr, textSegEnd - text.Addr, textSegEnd - text.Addr, SegAlign);
        if (hasRo)
            WritePh(PtLoad, PfR, roFileOff, roAddr, roSegEnd - roAddr, roSegEnd - roAddr, SegAlign);
        WritePh(PtLoad, PfR | PfW, relroFileOff, relroAddr, relroLen, relroLen, SegAlign);
        // The relro header matches the group it covers on offset, address and stored size, and rounds
        // its memory size up to the page. The loader protects whole pages, and the group that follows
        // starts on the next page anyway, so the rounded extent reaches exactly to it and no further.
        WritePh(PtGnuRelro, PfR, relroFileOff, relroAddr, relroLen, Align(relroLen, SegAlign), 1);
        // The writable group stores what it has and reserves the rest; a module's uninitialized data
        // costs no file bytes.
        WritePh(PtLoad, PfR | PfW, dataFileOff, dataAddr, dataLen, dataSegMem, SegAlign);
        WritePh(kind == ModuleKind.Library ? PtSceModuleParam : PtSceProcParam, PfR,
            relroFileOff + (procAddr - relroAddr), procAddr, (ulong)proc.Length, (ulong)proc.Length, 8);
        WritePh(PtDynamic, PfR | PfW, dynlibFileOff + (dynamicAddr - dynlibAddr), dynamicAddr, (ulong)dynamic.Length, (ulong)dynamic.Length, 8);
        WritePh(PtTls, PfR, hasTls ? relroFileOff + (tlsAddr - relroAddr) : relroFileOff,
            hasTls ? tlsAddr : relroAddr, tlsFileLen, tlsMemLen, hasTls ? tlsAlign : 1);
        // The frame-lookup header asks for four, which is what the table's own entries are sized to;
        // eight leaves the header claiming an alignment its entries do not keep.
        if (ehFrameHdr.Length > 0)
            WritePh(PtGnuEhFrame, PfR, roFileOff + (ehFrameHdrAddr - roAddr), ehFrameHdrAddr, (ulong)ehFrameHdr.Length, (ulong)ehFrameHdr.Length, 4);
        // The dynamic-linking segment requests no protection: the loader reads it to bind the module
        // rather than mapping it into the running image.
        WritePh(PtLoad, 0, dynlibFileOff, dynlibAddr, dynlibSegEnd - dynlibAddr, dynlibSegEnd - dynlibAddr, SegAlign);
        // The comment reserves no memory at all, which is what the loader checks it for.
        WritePh(PtSceComment, 0, commentFileOff, 0, (ulong)comment.Length, 0, 0x10);
        WritePh(PtSceVersion, 0, versionFileOff, 0, (ulong)versionBlob.Length, (ulong)versionBlob.Length, 1);
        WritePh(PtNote, 0, dynlibFileOff + (noteAddr - dynlibAddr), noteAddr, (ulong)note.Length, (ulong)note.Length, 4);
        // The tail note carries no load address (memsz 0); it is present in the file only.
        WritePh(PtNote, 0, tailNoteFileOff, 0, TailNoteLen, 0, 4);

        // Segment data.
        void Put(ulong segFileOff, ulong segBase, ulong addr, byte[] bytes)
            => bytes.AsSpan().CopyTo(file.AsSpan((int)(segFileOff + (addr - segBase))));

        // The reserved head of the image is filled with the one-byte trap, as a built module fills it.
        // Nothing should ever run here, and a run of zeros would decode as instructions and carry on into
        // the code instead of stopping.
        file.AsSpan((int)textFileOff, (int)ImageHeadReserve).Fill(0xCC);

        foreach (ElfObject obj in resolution.Included)
            for (int i = 0; i < obj.Sections.Count; i++)
            {
                ElfSection sec = obj.Sections[i];
                if (!sec.IsAlloc || sec.IsNoBits || !sectionData.TryGetValue((obj, i), out byte[]? bytes)) continue;
                if (resolution.DroppedSections.Contains((obj, i))) continue;
                ulong a = sectionAddr(obj, i);
                // Which group a section landed in is settled by where it landed, not by what its own
                // flags ask for. The groups run in increasing address order, and a section can sit in a
                // group its flags do not describe - the guard object asks to be read-only and is placed
                // in the writable group, because this platform never lets a read-only mapping widen.
                (ulong segFileOff, ulong segBase) = sec.IsExecutable ? (textFileOff, text.Addr)
                    : a >= dataAddr ? (dataFileOff, dataAddr)
                    : a >= relroAddr ? (relroFileOff, relroAddr)
                    : (roFileOff, roAddr);
                Put(segFileOff, segBase, a, bytes);
            }
        Put(textFileOff, text.Addr, pltAddr, plt);
        if (ehFrameHdr.Length > 0)
            Put(roFileOff, roAddr, ehFrameHdrAddr, ehFrameHdr);
        Put(relroFileOff, relroAddr, gotAddr, got);
        Put(relroFileOff, relroAddr, procAddr, proc);
        Put(relroFileOff, relroAddr, paramBlocksAddr, paramBlocks);
        comment.AsSpan().CopyTo(file.AsSpan((int)commentFileOff));
        versionBlob.AsSpan().CopyTo(file.AsSpan((int)versionFileOff));
        Put(dynlibFileOff, dynlibAddr, dynsymAddr, dynsym);
        Put(dynlibFileOff, dynlibAddr, dynstrAddr, dynstr);
        Put(dynlibFileOff, dynlibAddr, hashAddr, hash);
        Put(dynlibFileOff, dynlibAddr, relaAddr, rela);
        Put(dynlibFileOff, dynlibAddr, relaDynAddr, relaDyn);
        Put(dynlibFileOff, dynlibAddr, noteAddr, note);
        Put(dynlibFileOff, dynlibAddr, dynamicAddr, dynamic);

        // The section table and the names it points at.
        shstrBytes.AsSpan().CopyTo(file.AsSpan((int)shstrFileOff));
        sections[shstrIndex] = sections[shstrIndex] with { Off = shstrFileOff };
        for (int i = 0; i < sections.Count; i++)
        {
            Span<byte> s = file.AsSpan((int)shdrFileOff + i * 0x40);
            BinaryPrimitives.WriteUInt32LittleEndian(s, (uint)sections[i].Name);
            BinaryPrimitives.WriteUInt32LittleEndian(s[4..], sections[i].Type);
            BinaryPrimitives.WriteUInt64LittleEndian(s[8..], sections[i].Flags);
            BinaryPrimitives.WriteUInt64LittleEndian(s[16..], sections[i].Addr);
            BinaryPrimitives.WriteUInt64LittleEndian(s[24..], sections[i].Off);
            BinaryPrimitives.WriteUInt64LittleEndian(s[32..], sections[i].Size);
            BinaryPrimitives.WriteUInt32LittleEndian(s[40..], sections[i].Link);
            BinaryPrimitives.WriteUInt32LittleEndian(s[44..], sections[i].Info);
            BinaryPrimitives.WriteUInt64LittleEndian(s[48..], sections[i].Align);
            BinaryPrimitives.WriteUInt64LittleEndian(s[56..], sections[i].EntSize);
        }

        // The tail note: the same shape as the one above with a shorter name and identifier. Its
        // descriptor is stamped alongside the other one below.
        BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan((int)tailNoteFileOff), 4);
        BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan((int)tailNoteFileOff + 4), 8);
        BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan((int)tailNoteFileOff + 8), 3);
        Encoding.ASCII.GetBytes(TailNoteName).CopyTo(file, (int)tailNoteFileOff + 12);

        // Stamp the build-id note's descriptor with a content fingerprint of the finished image, so
        // the module carries a real, reproducible identifier rather than a run of zeros. The 20-byte
        // descriptor sits 16 bytes into the note (after its name/size/type header and the "GNU" name);
        // it is hashed while still zero, so the same inputs always yield the same identifier.
        // Only the first sixteen bytes carry the identifier and the last four stay zero, which is the
        // shape the system reads back when it names a module. Filling all twenty leaves an identifier
        // the system does not report.
        int noteFileOff = (int)(dynlibFileOff + (noteAddr - dynlibAddr));
        byte[] buildId = System.Security.Cryptography.SHA1.HashData(file);
        buildId.AsSpan(0, BuildIdBytes).CopyTo(file.AsSpan(noteFileOff + 16, BuildIdBytes));
        // The tail note's identifier is the first eight bytes of the same fingerprint, so the two agree
        // on which build they name.
        buildId.AsSpan(0, 8).CopyTo(file.AsSpan((int)tailNoteFileOff + 16, 8));
        return file;
    }

    private static byte[] BuildDynamic(
        (int SonameOff, int ModuleNameOff, int ModuleId, ushort ModuleVersion)[] modules,
        (int LibraryNameOff, int LibraryId, ushort LibraryVersion, int ModuleId)[] libraries, int moduleInfoName,
        ulong symtab, ulong strtab, ulong strsz, ulong hash, ulong hashsz,
        ulong jmprel, ulong pltrelsz, ulong pltgot, int dynsymSize,
        ulong rela, ulong relasz, int relativeCount,
        bool hasExports, int origFileNameOff, int exportLibNameOff, int exportLibId,
        (ulong Address, ulong Size) initArray, (ulong Address, ulong Size) finiArray,
        ulong init, ulong fini)
    {
        // The order below is the order a module built by the toolchain the format comes from carries:
        // the modules it needs, then what the module is, then the tables, then the setup and teardown
        // routines, then the two sizes that have no ordinary tag. Record value packs are
        // nameOffset | (version << 32) | (id << 48). The module's own info and its export library carry
        // this module's version; each needed record carries the version that module exports, so an
        // import binds to the library the provider actually publishes.
        var e = new List<(long, ulong)>();
        foreach ((int sonameOff, int moduleNameOff, int moduleId, ushort moduleVersion) in modules)
        {
            e.Add((DtNeeded, (uint)sonameOff));
            e.Add((DtSceNeededModule, (ulong)(uint)moduleNameOff | ((ulong)moduleVersion << 32) | ((ulong)(uint)moduleId << 48)));
            // Then every library this module publishes that the image imports from. A module that
            // publishes one produces the pair a reference module carries; one that publishes more
            // produces a record for each, all bound to the module named just above.
            foreach ((int libraryNameOff, int libraryId, ushort libraryVersion, int owningModuleId) in libraries)
            {
                if (owningModuleId != moduleId)
                    continue;
                e.Add((DtSceImportLib, (ulong)(uint)libraryNameOff | ((ulong)libraryVersion << 32) | ((ulong)(uint)libraryId << 48)));
                e.Add((DtSceImportLibAttr, ((ulong)(uint)libraryId << 48) | 0x09));
            }
        }
        e.Add((DtSceModuleInfo, (ulong)(uint)moduleInfoName | ((ulong)StubLibrary.DefaultModuleVersion << 32)));
        e.Add((DtSceModuleAttr, 0));
        // Every module records its own file name, whether or not it exports anything.
        e.Add((DtSceOrigFilename, (uint)origFileNameOff));
        // A module that exports symbols also records the library it publishes them under.
        if (hasExports)
        {
            e.Add((DtSceExportLib, (ulong)(uint)exportLibNameOff | ((ulong)StubLibrary.DefaultLibraryVersion << 32) | ((ulong)(uint)exportLibId << 48)));
            e.Add((DtSceExportLibAttr, ((ulong)(uint)exportLibId << 48) | 0x01));
        }
        // The slot the loader fills in with its own bookkeeping. Every module carries it.
        e.Add((DtDebug, 0));
        // The relocation table is named even when it is empty. The loader checks that the table, its
        // size and its entry size were all named and refuses the module if any is missing, so leaving
        // them out for a module with nothing to relocate would make that module unloadable. An empty
        // table is expressed as a zero size, which the loader accepts.
        e.Add((DtRela, rela));
        e.Add((DtRelaSz, relasz));
        e.Add((DtRelaEnt, 24));
        e.Add((DtRelaCount, (ulong)relativeCount));
        e.Add((DtJmpRel, jmprel)); e.Add((DtPltRelSz, pltrelsz));
        e.Add((DtPltGot, pltgot)); e.Add((DtPltRel, 7 /* DT_RELA */));
        // The tables are named by the ordinary tags. A module whose linking segment carries an address
        // must not also name them through the module-specific aliases: the loader routes an alias and
        // its ordinary tag to the same handler, so the alias reads as a duplicate and the module is
        // refused while its dynamic table is being read. Only the two size tags below have no ordinary
        // equivalent, and both are required.
        e.Add((DtSymTab, symtab)); e.Add((DtSymEnt, 24));
        e.Add((DtStrTab, strtab)); e.Add((DtStrSz, strsz));
        e.Add((DtHash, hash));
        // The constructor and destructor arrays are always named. For an executable they are named
        // empty, so the loader is handed no array to run: the entry runs its constructors itself, and
        // declaring a real array here as well would run every one of them twice. A library keeps the
        // array it carries, because the loader is what runs those.
        e.Add((DtPreInitArray, 0)); e.Add((DtPreInitArraySz, 0));
        e.Add((DtInitArray, initArray.Address)); e.Add((DtInitArraySz, initArray.Size));
        e.Add((DtFiniArray, finiArray.Address)); e.Add((DtFiniArraySz, finiArray.Size));
        // The setup and teardown routines the loader calls around the module's life.
        e.Add((DtInit, init));
        e.Add((DtFini, fini));
        e.Add((DtSceSymTabSz, (ulong)dynsymSize));
        e.Add((DtSceHashSz, hashsz));
        e.Add((DtNull, 0));
        byte[] d = new byte[e.Count * 16];
        for (int i = 0; i < e.Count; i++)
        {
            BinaryPrimitives.WriteInt64LittleEndian(d.AsSpan(i * 16), e[i].Item1);
            BinaryPrimitives.WriteUInt64LittleEndian(d.AsSpan(i * 16 + 8), e[i].Item2);
        }
        return d;
    }

    // The symbol hash table, in the classic layout: the bucket count, the chain count, the buckets,
    // then the chains. A lookup hashes the name, reduces it by the bucket count, and walks the chain
    // from that bucket until a name matches or the walk reaches the undefined entry at index 0.
    // <paramref name="names"/> is the symbol table's names in order, starting with the empty name of
    // the null entry. A module carries one bucket per symbol, which keeps a lookup close to constant;
    // a single bucket would put every symbol on one chain and make each lookup walk the whole table.
    private static byte[] BuildSysVHash(IReadOnlyList<string> names)
    {
        int count = names.Count, nbucket = count;
        byte[] h = new byte[8 + nbucket * 4 + count * 4];
        BinaryPrimitives.WriteUInt32LittleEndian(h, (uint)nbucket);
        BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(4), (uint)count);

        int chainBase = 8 + nbucket * 4;
        // Each bucket holds the first symbol hashing to it and each chain slot the next one after it,
        // ending at the undefined index. Symbols are threaded from the back so every chain comes out
        // in ascending order, which is the order a module carries them in.
        for (int i = count - 1; i >= 1; i--)
        {
            int bucket = (int)(ElfHash(names[i]) % (uint)nbucket);
            BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(chainBase + i * 4),
                BinaryPrimitives.ReadUInt32LittleEndian(h.AsSpan(8 + bucket * 4)));
            BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(8 + bucket * 4), (uint)i);
        }
        return h;
    }

    // The hash a symbol table is indexed by: four bits of shift per character, with the bits that
    // overflow the low 28 folded back down.
    private static uint ElfHash(string name)
    {
        uint h = 0;
        foreach (char c in name)
        {
            h = (h << 4) + (byte)c;
            uint carry = h & 0xF0000000;
            if (carry != 0)
                h ^= carry >> 24;
            h &= ~carry;
        }
        return h;
    }

    // A position-independent routine that runs the global constructors in [initStart, initEnd) - each a
    // relocated function pointer, skipping any left null - and returns. It addresses the array with
    // instruction-relative displacements, so it needs no load-time relocation. See
    // <see cref="EntryStubSize"/> for the byte count.
    //
    // The saved registers are what make each call it makes a properly aligned one, and there have to be
    // an odd number of them. This is entered with the stack eight past a sixteen-byte boundary, because
    // the call that got here left a return address on it; a call it makes has to leave the stack **on**
    // a boundary at the moment the call is made. Two saved registers give back the state this was
    // entered in, so every constructor ran eight bytes out - and a constructor that assumes otherwise,
    // which any that touches a wide value does, faults on its first such access. Three restore it.
    // Keeping a frame pointer as well costs nothing here and leaves a walk back through this routine
    // readable, which the toolchain's own does.
    private static byte[] BuildInitWalker(ulong stubAddr, ulong initStart, ulong initEnd)
    {
        // Byte offsets, used for the branch displacements: loop=21, skip=36, done=42, and the code ends
        // at 47. rbx walks the array, r14 marks its end.
        byte[] c = new byte[EntryStubSize];
        int i = 0;
        c[i++] = 0x55;                                     // 0:  push rbp
        c[i++] = 0x48; c[i++] = 0x89; c[i++] = 0xE5;       // 1:  mov rbp, rsp
        c[i++] = 0x41; c[i++] = 0x56;                      // 4:  push r14
        c[i++] = 0x53;                                     // 6:  push rbx
        c[i++] = 0x48; c[i++] = 0x8D; c[i++] = 0x1D;       // 7:  lea rbx, [rip + (initStart - next)]
        BinaryPrimitives.WriteInt32LittleEndian(c.AsSpan(i), checked((int)((long)initStart - (long)(stubAddr + 14)))); i += 4;
        c[i++] = 0x4C; c[i++] = 0x8D; c[i++] = 0x35;       // 14: lea r14, [rip + (initEnd - next)]
        BinaryPrimitives.WriteInt32LittleEndian(c.AsSpan(i), checked((int)((long)initEnd - (long)(stubAddr + 21)))); i += 4;
        c[i++] = 0x4C; c[i++] = 0x39; c[i++] = 0xF3;       // 21: loop: cmp rbx, r14
        c[i++] = 0x73; c[i++] = 0x10;                      // 24: jae done      (-> 42)
        c[i++] = 0x48; c[i++] = 0x8B; c[i++] = 0x03;       // 26: mov rax, [rbx]
        c[i++] = 0x48; c[i++] = 0x85; c[i++] = 0xC0;       // 29: test rax, rax
        c[i++] = 0x74; c[i++] = 0x02;                      // 32: jz skip       (-> 36, past the call)
        c[i++] = 0xFF; c[i++] = 0xD0;                      // 34: call rax
        c[i++] = 0x48; c[i++] = 0x83; c[i++] = 0xC3; c[i++] = 0x08; // 36: skip: add rbx, 8
        c[i++] = 0xEB; c[i++] = 0xEB;                      // 40: jmp loop      (-> 21)
        c[i++] = 0x5B;                                     // 42: done: pop rbx
        c[i++] = 0x41; c[i++] = 0x5E;                      // 43: pop r14
        c[i++] = 0x5D;                                     // 45: pop rbp
        c[i++] = 0xC3;                                     // 46: ret
        return c;
    }

    // The block is 0x60 bytes and says so, stored and in memory alike. Padding it further makes the
    // segment longer than the length the block itself declares, which reads back as a malformed block.
    private const int ProcParamSize = 0x60, ProcParamDeclaredSize = 0x60;

    // The three parameter blocks the process parameters point at. Every built module carries them, and
    // the pointers to them are filled in at load time rather than written into the image - which is why
    // the block reads as all zeros in a finished module and still is not. Each is a size word followed
    // by fields a module leaves at their defaults; the C library block also states its own revision.
    private const int LibcParamOffset = 0x38, KernelMemParamOffset = 0x40, KernelFsParamOffset = 0x48;
    private const int LibcParamSize = 0xA8, KernelMemParamSize = 0x38, KernelFsParamSize = 0x10;
    private const int MallocReplaceSize = 0x78, NewReplaceSize = 0xC0, TlsMallocReplaceSize = 0x38;
    private const ulong LibcParamRevision = 0x000000010000000E;
    // Where in the C library's block the three replacement tables and the library marker are named.
    private const int MallocReplacePointer = 0x30, NewReplacePointer = 0x38,
                      LibcMarkerPointer = 0x48, TlsMallocReplacePointer = 0x60;
    // Executable process parameters point to three blocks. The C-library backing block carries five
    // pointers for either kind. Its optional imported marker is counted separately.
    private const int ProcParamPointers = 3, ParamBlockPointers = 5;

    /// <summary>The two heap figures stored after the blocks, and where each sits.</summary>
    private const int HeapFigureSize = 16;
    private static int HeapSizeValueOffset => ParamBlockOffsets[^1] + TlsMallocReplaceSize;
    private static int HeapExtendedValueOffset => HeapSizeValueOffset + 8;

    // Where in the C library's block the size of the heap it may grow to is named, and the flag that
    // lets it grow past its own idea of a limit. Both are pointers the module fills in at load time.
    private const int LibcHeapSizePointer = 0x10, LibcHeapExtendedPointer = 0x20;

    /// <summary>
    /// The heap figures the module carries for the C library to point at. A module that names no size
    /// gets the library's built-in one, which is a quarter of a megabyte for the whole process - enough
    /// to start almost nothing, and reached silently, as an allocation simply failing. The value below
    /// is the one the platform's own header gives for "no limit", and it is what most shipping modules
    /// carry; the flag beside it clears the same limit by a second route.
    /// </summary>
    private const ulong LibcHeapNoLimit = 0xFFFFFFFFFFFFFFFF;

    /// <summary>Where each block starts, measured from the first.</summary>
    private static readonly int[] ParamBlockOffsets =
    [
        0,                                                                                  // C library
        LibcParamSize,                                                                      // memory
        LibcParamSize + KernelMemParamSize,                                                 // file system
        LibcParamSize + KernelMemParamSize + KernelFsParamSize,                             // allocation
        LibcParamSize + KernelMemParamSize + KernelFsParamSize + MallocReplaceSize,          // construction
        LibcParamSize + KernelMemParamSize + KernelFsParamSize + MallocReplaceSize + NewReplaceSize,
    ];

    /// <summary>
    /// The six blocks a built module carries, laid end to end. Each opens with its own length; the three
    /// replacement tables also carry a count of the entries they have room for. Everything else is a
    /// field a module leaves at its default, or a pointer filled in at load time.
    /// </summary>
    private static byte[] BuildParamBlocks()
    {
        int[] sizes = [LibcParamSize, KernelMemParamSize, KernelFsParamSize,
                       MallocReplaceSize, NewReplaceSize, TlsMallocReplaceSize];
        ulong[] counts = [LibcParamRevision, 0, 0, 2, 3, 1];
        // The two heap figures are stored past the last block and pointed at from the C library's,
        // which is how a module names them: the block holds addresses, not values.
        byte[] b = new byte[ParamBlockOffsets[^1] + TlsMallocReplaceSize + HeapFigureSize];
        for (int i = 0; i < sizes.Length; i++)
        {
            BinaryPrimitives.WriteUInt64LittleEndian(b.AsSpan(ParamBlockOffsets[i]), (ulong)sizes[i]);
            if (counts[i] != 0)
                BinaryPrimitives.WriteUInt64LittleEndian(b.AsSpan(ParamBlockOffsets[i] + 8), counts[i]);
        }
        BinaryPrimitives.WriteUInt64LittleEndian(b.AsSpan(HeapSizeValueOffset), LibcHeapNoLimit);
        BinaryPrimitives.WriteUInt32LittleEndian(b.AsSpan(HeapExtendedValueOffset), 1);
        return b;
    }

    // What a library carries where an executable carries its process parameters. The two are different
    // records under different headers, and a module carries whichever suits what it is: of the modules
    // measured, every one that is a library carries this one and none carries the other, and every
    // executable is the exact reverse. The shared-object half of the reference startup code carries
    // this block verbatim, which is where its contents come from rather than from inference.
    //
    // The version words are the same pair the process parameters carry, and mean the same thing.
    private static byte[] BuildModuleParam()
    {
        byte[] p = new byte[ModuleParamSize];
        BinaryPrimitives.WriteUInt64LittleEndian(p, ModuleParamSize);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x08), ModuleParamMagic);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x0C), 3);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x10), CompanionSdkVersion);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x14), ModuleSdkVersion);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x18), 1);
        return p;
    }

    private static byte[] BuildProcParam()
    {
        byte[] p = new byte[ProcParamSize];
        BinaryPrimitives.WriteUInt64LittleEndian(p, ProcParamDeclaredSize);
        p[8] = (byte)'O'; p[9] = (byte)'R'; p[10] = (byte)'B'; p[11] = (byte)'I';
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x0C), 5);
        // Two version words read out of the process parameters when the image is activated; the system
        // reports both back as it starts the process. The second states the version the module targets
        // and carries the revision the modules an application links against are built at - a revision
        // no release carries describes a module that could not exist. The first is the companion
        // version that goes with it: the two travel as a pair and are not chosen independently, so
        // changing one without the other describes a combination no module has.
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x10), CompanionSdkVersion);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x14), ModuleSdkVersion);
        BinaryPrimitives.WriteUInt32LittleEndian(p.AsSpan(0x58), 1);
        return p;
    }

    // The comment segment, which the container stores and the loader reads. Its form is a four-byte
    // tag, the length of everything that follows the tag and that length itself, the length of the
    // text, then the text. Both lengths have to describe the bytes that are actually there: the
    // segment is the last content in the file, so a length that overstates it points a reader past
    // the end of the image.
    private static byte[] BuildComment(string moduleFileName)
    {
        // The text length counts the terminator, and the segment is the header plus that text rounded to
        // four - both read off built modules, whose declared lengths and segment sizes agree with each
        // other only under those two rules. The text itself is the module's own name; a built module
        // records the whole path it was written to, which says more about the machine that built it than
        // about the module.
        byte[] text = Encoding.ASCII.GetBytes(moduleFileName + "\0");
        byte[] blob = new byte[AlignInt(12 + text.Length, 4)];
        Encoding.ASCII.GetBytes("PATH").CopyTo(blob, 0);
        BinaryPrimitives.WriteUInt32LittleEndian(blob.AsSpan(4), (uint)(blob.Length - 8));
        BinaryPrimitives.WriteUInt32LittleEndian(blob.AsSpan(8), (uint)text.Length);
        text.CopyTo(blob, 12);
        return blob;
    }

    // The version segment, recording the module's own name and the revision it was built against. A
    // record is a zero, the length of the record body, the width of one version word, the name ending
    // in a colon, then two version words held most-significant byte first, so a reader walking record
    // by record lands exactly on the end of the segment.
    private const int VersionWordSize = 8;

    // The version segment is a run of records and nothing else. A reader takes each record's declared
    // body length and steps to the next, so the segment has to end exactly where the last record does:
    // padding past it reads as a further record declaring no body, which is not a record at all.
    private static byte[] BuildVersion(IReadOnlyList<string> components)
    {
        var blob = new List<byte>();
        foreach (string component in components)
        {
            byte[] name = Encoding.ASCII.GetBytes(component + ":");
            int body = 1 + name.Length + 2 * VersionWordSize;
            byte[] record = new byte[4 + body];
            BinaryPrimitives.WriteUInt16LittleEndian(record.AsSpan(2), (ushort)body);
            record[4] = VersionWordSize;
            name.CopyTo(record, 5);
            for (int i = 0, at = 5 + name.Length; i < 2; i++, at += VersionWordSize)
            {
                BinaryPrimitives.WriteUInt32BigEndian(record.AsSpan(at), ModuleSdkVersion);
                BinaryPrimitives.WriteUInt32BigEndian(record.AsSpan(at + 4), 1);
            }
            blob.AddRange(record);
        }
        return [.. blob];
    }

    private static int AlignInt(int value, int alignment) => (value + alignment - 1) & ~(alignment - 1);

    // The descriptor is 0x14 bytes and the identifier fills the first 16 of them.
    private const int BuildIdBytes = 16;

    private static byte[] BuildNote()
    {
        byte[] n = new byte[0x24];
        BinaryPrimitives.WriteUInt32LittleEndian(n, 4);
        BinaryPrimitives.WriteUInt32LittleEndian(n.AsSpan(4), 0x14);
        BinaryPrimitives.WriteUInt32LittleEndian(n.AsSpan(8), 3);
        n[12] = (byte)'G'; n[13] = (byte)'N'; n[14] = (byte)'U';
        return n;
    }

    private static string Encode(int id)
    {
        if (id == 0) return "A";
        var sb = new StringBuilder();
        while (id > 0) { sb.Insert(0, Alphabet[id % 64]); id /= 64; }
        return sb.ToString();
    }

    private static ulong Align(ulong v, ulong a) => a <= 1 ? v : (v + a - 1) / a * a;

    private sealed class StringTable
    {
        private readonly List<byte> _bytes = [0];
        private readonly Dictionary<string, int> _off = new(StringComparer.Ordinal);
        public int Add(string value)
        {
            if (_off.TryGetValue(value, out int o)) return o;
            int offset = _bytes.Count;
            _bytes.AddRange(Encoding.ASCII.GetBytes(value));
            _bytes.Add(0);
            _off[value] = offset;
            return offset;
        }
        public byte[] ToBytes() => [.. _bytes];
    }
}
