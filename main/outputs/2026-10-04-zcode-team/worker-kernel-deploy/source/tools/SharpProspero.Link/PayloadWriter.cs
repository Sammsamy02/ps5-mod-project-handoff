// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK
//
// Writes a position-independent payload: a plain ET_DYN executable an ELF loader maps at a fresh base
// and runs in an existing process. The loader applies base-relative fix-ups from a relocation section it
// reads by section header. The CRT embedded in the payload then reads the dynamic
// section (PT_DYNAMIC) to discover needed libraries, walks the dynamic symbol table for undefined
// references, and resolves each one against the host process's loaded modules.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace SharpProspero.Link;

/// <summary>Writes a position-independent payload from a resolved graph whose outside references resolve at run time.</summary>
public static class PayloadWriter
{
    private const ulong SegAlign = 0x4000;
    private const uint PfX = 1, PfW = 2, PfR = 4;
    private const uint PtLoad = 1, PtDynamic = 2;
    private const uint RRelative = 8;            // R_X86_64_RELATIVE
    private const uint RGlobDat = 6;             // R_X86_64_GLOB_DAT
    private const uint RJumpSlot = 7;            // R_X86_64_JUMP_SLOT
    private const int ShtProgBits = 1, ShtSymTab = 2, ShtStrTab = 3, ShtRela = 4, ShtNoBits = 8;
    private const int ShtDynSym = 11, ShtGnuHash = 0x6FFFFFF6, ShtDynamic = 6;
    private const ulong ShfAlloc = 0x2, ShfWrite = 0x1, ShfExec = 0x4;
    private const ulong ShfMerge = 0x10, ShfStrings = 0x20, ShfInfo = 0x40;
    private const ushort ShnAbs = 0xFFF1, ShnCommon = 0xFFF2;

    private const long DtNull = 0, DtNeeded = 1, DtGnuHash = 0x6FFFFEF5;
    private const long DtPltGot = 3, DtStrTab = 5, DtSymTab = 6;
    private const long DtRela = 7, DtRelaSz = 8, DtRelaEnt = 9, DtStrSz = 10, DtSymEnt = 11;
    private const long DtPltRel = 20, DtDebug = 0x15, DtJmpRel = 23;
    private const long DtInit_Array = 25, DtFini_Array = 26;
    private const long DtInit_ArraySz = 27, DtFini_ArraySz = 28;
    private const long DtPreInit_Array = 32, DtPreInit_ArraySz = 33;
    private const long DtFlags1 = 0x6FFFFFFB;
    private const long DtRelaCount = 0x6FFFFFF9;
    private const long DtPltRelSz = 2;

    private static readonly Dictionary<string, string> PayloadSonameMap = new(StringComparer.Ordinal)
    {
        // The payload host process (hijacked SceSpZeroConfMain) loads libkernel_sys.sprx, not
        // libkernel_web.sprx. Route the generic "libkernel" alias to the sys variant so DT_NEEDED
        // matches the actual runtime module and sceKernelLoadStartModule succeeds against a
        // module already present in the process image.
        ["libkernel.sprx"] = "libkernel_sys.sprx",
    };

    // A writable section the loader is meant to seal once its fix-ups run - the constructor and
    // destructor arrays, and a compiler-marked read-only-after-init region - is placed alongside the
    // read-only content in the middle segment, not out with the runtime data. Known working payloads
    // group these together with the dynamic linking tables so a single load segment carries every
    // region the loader binds; the run-time .data and .bss live in a segment of their own behind
    // them, where mem-size exceeds file-size to cover the uninitialized tail.
    private static readonly HashSet<string> RelroLikeSectionNames = new(StringComparer.Ordinal)
    {
        ".init_array", ".fini_array", ".preinit_array",
        ".data.rel.ro", ".ctors", ".dtors",
    };

    private static bool IsRelroLike(string name)
        => RelroLikeSectionNames.Contains(DynamicWriter.OutputSectionName(name));

    private readonly record struct Relative(ulong Offset, ulong Addend);

    private sealed class Extern
    {
        /// <summary>The name the graph writes into a relocation - the key relocation binding uses.</summary>
        public required string Name { get; init; }

        /// <summary>
        /// The name the resolver looks up at run time. Differs from <see cref="Name"/> when the graph
        /// carries an alias for a published name (a compat routine standing in front of one keeps a name
        /// of its own so its references reach the platform's), and when a reference the linker did not
        /// find any stub for still starts with the alias prefix, in which case the prefix is dropped so
        /// the platform's name is what the resolver sees.
        /// </summary>
        public required string LookupName { get; init; }

        public ulong GotAddress { get; set; }     // the slot the rtld fills through GLOB_DAT
        public ulong PltAddress { get; set; }     // a stub that jumps through the slot
    }

    /// <summary>
    /// Writes the payload for <paramref name="resolution"/>, with <paramref name="entrySymbol"/> as the
    /// entry the loader jumps to. Outside references become dynamic symbol entries the runtime resolves.
    /// </summary>
    /// <param name="resolution">The resolved symbol graph from the linker.</param>
    /// <param name="entrySymbol">The symbol the loader jumps to (typically <c>_start</c>).</param>
    /// <param name="neededSprx">When non-null, the DT_NEEDED SPRX list to emit. Built by
    /// <see cref="PayloadProfile.BuildNeededSprx"/>. When null, DT_NEEDED is derived from the
    /// resolution's imports plus the three hardcoded defaults (backward-compatible path).</param>
    public static byte[] Write(LinkResolution resolution, string? entrySymbol, string[]? neededSprx = null)
    {
        ArgumentNullException.ThrowIfNull(resolution);

        // Outside references, one resolver slot each. An import the graph attributes to a module and a
        // reference nothing defines are the same to a payload: a name resolved at run time. A payload has
        // no dynamic linker, so an unresolved reference is not an error - it is a name for the resolver.
        var externByName = new Dictionary<string, Extern>(StringComparer.Ordinal);
        var externs = new List<Extern>();
        void AddExtern(string name, string? lookupName = null)
        {
            if (name.Length == 0 || externByName.ContainsKey(name) || Linker.LinkerProvided.Contains(name)) return;
            var e = new Extern { Name = name, LookupName = lookupName ?? name };
            externByName[name] = e;
            externs.Add(e);
        }
        foreach (ImportSymbol imp in resolution.Imports)
            AddExtern(imp.Name, imp.PublishedName);
        foreach (string name in resolution.Unresolved)
        {
            string? lookup = name.StartsWith(Linker.DeviceAliasPrefix, StringComparison.Ordinal)
                ? name[Linker.DeviceAliasPrefix.Length..]
                : null;
            AddExtern(name, lookup);
        }

        var sectionNames = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject o in resolution.Included)
            foreach (ElfSection s in o.Sections)
                if (s.IsAlloc && s.Name.Length > 0)
                    sectionNames.Add(s.Name);

        foreach (ElfObject o in resolution.Included)
            foreach (IReadOnlyList<ElfRelocation> relocs in o.Relocations.Values)
                foreach (ElfRelocation r in relocs)
                    if (r.SymbolIndex < o.Symbols.Count)
                    {
                        ElfSymbol s = o.Symbols[(int)r.SymbolIndex];
                        if (s.IsUndefined && s.Name.Length > 0 && !resolution.Defined.ContainsKey(s.Name)
                            && !Linker.IsEncapsulationSymbol(s.Name, sectionNames, out _, out _))
                            AddExtern(s.Name);
                    }

        // Lay out the allocatable sections into three groups matching the corpus segment shape:
        // (1) executable text and PLT stubs, mapped RWX so the loader can rewrite text after mapping;
        // (2) a middle group carrying read-only object content, the writable-but-sealed relro-like
        //     content (init/fini arrays, .data.rel.ro), and the synthesised dynamic tables and GOT;
        // (3) a data group carrying only the run-time .data and .bss so p_memsz > p_filesz there.
        var offsetInGroup = new Dictionary<(ElfObject, int), ulong>();
        ulong textLen = 0, roLen = 0, dataMem = 0, dataFile = 0;
        ulong initStart = 0, initEnd = 0, finiStart = 0, finiEnd = 0;
        bool haveInit = false, haveFini = false;
        void PlaceArray(string name, ref ulong start, ref ulong end, ref bool have)
        {
            // A constructor/destructor array is a run of aligned eight-byte pointers. The membership
            // predicate rejects anything that cannot possibly be one so a mis-declared or zero-filled
            // section can never enlarge the run past the pointer entries it holds. Every accepted
            // section carries file bytes, a positive size, an eight-byte-multiple size, and an
            // alignment no larger than eight. Placement goes into the middle (relro-like) group so
            // the corpus segment layout is preserved: init/fini arrays sit alongside .dynamic in
            // the second LOAD, not out with the runtime data in the third.
            var members = new List<(ElfObject Obj, int Index)>();
            foreach (ElfObject obj in resolution.Included)
                for (int i = 0; i < obj.Sections.Count; i++)
                {
                    ElfSection sec = obj.Sections[i];
                    if (sec is not { IsAlloc: true, IsTls: false, IsWritable: true, IsNoBits: false }
                        || sec.Size == 0 || (sec.Size % 8) != 0 || sec.AddrAlign > 8
                        || DynamicWriter.OutputSectionName(sec.Name) != name) continue;
                    members.Add((obj, i));
                }
            foreach ((ElfObject obj, int i) in members.OrderBy(m => DynamicWriter.ArrayPriority(m.Obj.Sections[m.Index].Name)))
            {
                ElfSection sec = obj.Sections[i];
                ulong o = Align(roLen, sec.AddrAlign);
                offsetInGroup[(obj, i)] = o;
                roLen = o + sec.Size;
                if (!have) { start = o; have = true; }
                end = roLen;
            }
            if (have && ((end - start) & 7) != 0)
                throw new InvalidOperationException($"{name} did not end on an eight-byte boundary (start={start}, end={end})");
        }
        PlaceArray(".init_array", ref initStart, ref initEnd, ref haveInit);
        PlaceArray(".fini_array", ref finiStart, ref finiEnd, ref haveFini);

        foreach (ElfObject obj in resolution.Included)
            for (int i = 0; i < obj.Sections.Count; i++)
            {
                ElfSection sec = obj.Sections[i];
                if (!sec.IsAlloc || sec.IsTls || offsetInGroup.ContainsKey((obj, i))) continue;
                if (sec.IsExecutable) { offsetInGroup[(obj, i)] = textLen = Align(textLen, sec.AddrAlign); textLen += sec.Size; }
                else if (sec.IsWritable && IsRelroLike(sec.Name) && !sec.IsNoBits)
                {
                    // Writable-but-sealed content whose bytes come from the file: goes to the
                    // middle group. A NOBITS relro-like section is nonsensical (a sealed range
                    // needs its data delivered) so falls through to the data group as bss below.
                    ulong o = Align(roLen, sec.AddrAlign);
                    offsetInGroup[(obj, i)] = o; roLen = o + sec.Size;
                }
                else if (sec.IsWritable)
                {
                    // Runtime data (including any NOBITS section): standalone data group.
                    ulong o = Align(dataMem, sec.AddrAlign);
                    offsetInGroup[(obj, i)] = o; dataMem = o + sec.Size;
                    if (!sec.IsNoBits) dataFile = dataMem;
                }
                else
                {
                    // Read-only object content: middle group alongside the relro-like content.
                    ulong o = Align(roLen, sec.AddrAlign);
                    offsetInGroup[(obj, i)] = o; roLen = o + sec.Size;
                }
            }

        // Thread-local template.
        var tlsOffset = new Dictionary<(ElfObject, int), ulong>();
        ulong tlsMemLen = 0, tlsFileLen = 0, tlsAlign = 1;
        for (int pass = 0; pass < 2; pass++)
            foreach (ElfObject obj in resolution.Included)
                for (int i = 0; i < obj.Sections.Count; i++)
                {
                    ElfSection sec = obj.Sections[i];
                    if (!sec.IsAlloc || !sec.IsTls) continue;
                    if ((pass == 0) == sec.IsNoBits) continue;
                    ulong o = Align(tlsMemLen, sec.AddrAlign);
                    tlsOffset[(obj, i)] = o; tlsMemLen = o + sec.Size;
                    if (!sec.IsNoBits) tlsFileLen = tlsMemLen;
                    if (sec.AddrAlign > tlsAlign) tlsAlign = sec.AddrAlign;
                }
        bool hasTls = tlsMemLen > 0;
        ulong tlsAlignedMem = Align(tlsMemLen, tlsAlign);

        ulong tlsTemplateOffsetInData = 0;
        if (hasTls)
        {
            tlsTemplateOffsetInData = Align(dataMem, tlsAlign);
            dataMem = tlsTemplateOffsetInData + tlsMemLen;
            if (tlsFileLen > 0) dataFile = tlsTemplateOffsetInData + tlsFileLen;
        }

        // Exception-frame index.
        var ehFrames = new List<(ElfObject Obj, int Index)>();
        foreach (ElfObject obj in resolution.Included)
            for (int i = 0; i < obj.Sections.Count; i++)
                if (obj.Sections[i] is { IsAlloc: true, IsExecutable: false, IsWritable: false, IsNoBits: false } s
                    && s.Name == ".eh_frame")
                    ehFrames.Add((obj, i));
        int ehFrameCount = 0;
        bool ehFrameOk = ehFrames.Count > 0;
        foreach ((ElfObject obj, int i) in ehFrames)
        {
            var probe = new List<EhFrame.Entry>();
            if (!EhFrame.TryParse(obj.Sections[i].Data, 0, probe)) { ehFrameOk = false; break; }
            ehFrameCount += probe.Count;
        }
        int ehFrameHdrSize = ehFrameOk && ehFrameCount > 0 ? 12 + ehFrameCount * 8 : 0;

        // PLT stubs.
        ulong pltBase = Align(textLen, 16);
        ulong retStubOffset = pltBase + (ulong)externs.Count * 16;
        ulong textSize = retStubOffset + 1;

        // --- Dynamic linking tables (placed in the read-only group) ---
        // Collect unique sonames for DT_NEEDED. When the caller provides a profile-built list,
        // use it directly; otherwise derive from the resolution's imports plus the three
        // hardcoded defaults (backward-compatible path for callers that do not pass a profile).
        var sonames = new List<string>();
        var sonameSet = new HashSet<string>(StringComparer.Ordinal);
        if (neededSprx is not null)
        {
            foreach (string s in neededSprx)
                if (sonameSet.Add(s))
                    sonames.Add(s);
        }
        else
        {
            // Legacy path: derive from imports, mapping known application SDK sonames to the
            // payload host process's actual module names.
            foreach (ImportSymbol imp in resolution.Imports)
            {
                string mapped = PayloadSonameMap.TryGetValue(imp.Soname, out string? replacement) ? replacement : imp.Soname;
                if (sonameSet.Add(mapped))
                    sonames.Add(mapped);
            }
            if (sonameSet.Add("libkernel_sys.sprx")) sonames.Insert(0, "libkernel_sys.sprx");
            if (sonameSet.Add("libSceLibcInternal.sprx")) sonames.Add("libSceLibcInternal.sprx");
            if (sonameSet.Add("libSceNet.sprx")) sonames.Add("libSceNet.sprx");
        }

        // .dynstr: string table for symbol names and sonames.
        var dynstrBuild = new List<byte> { 0 }; // index 0 is always the empty string
        var dynstrOff = new Dictionary<string, int>(StringComparer.Ordinal);
        int DynStrAdd(string s)
        {
            if (dynstrOff.TryGetValue(s, out int existing)) return existing;
            int off = dynstrBuild.Count;
            dynstrBuild.AddRange(Encoding.ASCII.GetBytes(s));
            dynstrBuild.Add(0);
            dynstrOff[s] = off;
            return off;
        }
        int[] sonameStrOff = sonames.Select(DynStrAdd).ToArray();
        int[] externStrOff = new int[externs.Count];
        for (int i = 0; i < externs.Count; i++)
            externStrOff[i] = DynStrAdd(externs[i].LookupName);
        byte[] dynstrBytes = [.. dynstrBuild];

        // .dynsym: NULL + one UND entry per extern.
        int dynsymCount = 1 + externs.Count;
        byte[] dynsymBytes = new byte[dynsymCount * 24];
        for (int i = 0; i < externs.Count; i++)
        {
            int b = (i + 1) * 24;
            BinaryPrimitives.WriteUInt32LittleEndian(dynsymBytes.AsSpan(b), (uint)externStrOff[i]);
            dynsymBytes[b + 4] = (1 << 4) | 2; // STB_GLOBAL | STT_FUNC
        }

        // .gnu.hash: minimal GNU hash table (single bucket, all symbols in one chain).
        int gnuChainCount = externs.Count;
        int hashSize = 28 + gnuChainCount * 4;
        byte[] hashBytes = new byte[hashSize];
        BinaryPrimitives.WriteUInt32LittleEndian(hashBytes.AsSpan(0), 1);   // nbuckets
        BinaryPrimitives.WriteUInt32LittleEndian(hashBytes.AsSpan(4), 1);   // symndx
        BinaryPrimitives.WriteUInt32LittleEndian(hashBytes.AsSpan(8), 1);   // maskwords
        BinaryPrimitives.WriteUInt32LittleEndian(hashBytes.AsSpan(12), 6);  // shift2
        ulong bloomVal = 0;
        uint[] gnuHashes = new uint[externs.Count];
        for (int i = 0; i < externs.Count; i++)
        {
            // The GNU hash indexes .dynstr entries; those entries are the plain C names the
            // known working payloads publish"puts", "signal",
            // "pthread_self", ...). The shim's __sp_dlsym_init probe caches args->sys_dynlib_dlsym
            // as a callable that takes plain names; the on-device dynamic linker matches names
            // through its own module-side NID encoder, so what the payload publishes here must be
            // the source-level C names, not the eleven-character NID form.
            gnuHashes[i] = GnuHash(externs[i].LookupName);
            bloomVal |= 1UL << (int)(gnuHashes[i] % 64);
            bloomVal |= 1UL << (int)((gnuHashes[i] >> 6) % 64);
        }
        BinaryPrimitives.WriteUInt64LittleEndian(hashBytes.AsSpan(16), bloomVal);
        BinaryPrimitives.WriteUInt32LittleEndian(hashBytes.AsSpan(24), externs.Count > 0 ? 1u : 0u);
        for (int i = 0; i < externs.Count; i++)
        {
            bool last = i + 1 >= externs.Count;
            BinaryPrimitives.WriteUInt32LittleEndian(hashBytes.AsSpan(28 + i * 4), (gnuHashes[i] & ~1u) | (last ? 1u : 0u));
        }

        // Middle group (mapped RW): the object read-only content and the writable relro-like
        // content already placed above, then the exception-frame index, the dynamic linking tables
        // (.dynsym, .dynstr, .hash), the global-offset table, the dynamic section itself, and
        // .rela.dyn. Known working payloads keep every binding surface together in this segment so
        // PT_DYNAMIC points inside it; the runtime .data / .bss lives in its own segment behind.
        int internalGotSlots = CountInternalGotSlots(resolution, externByName);
        int totalGotSlots = externs.Count + internalGotSlots;
        // Dynamic table: NEEDED(n) + FLAGS_1 + DEBUG + RELA group(4) + SYMTAB + SYMENT
        //   + STRTAB + STRSZ + GNU_HASH + PREINIT_ARRAY(2) + INIT_ARRAY(2) + FINI_ARRAY(2)
        //   + NULL = n + 18 base. When the output contains JUMP_SLOT relocations, the
        //   JMPREL group (JMPREL, PLTRELSZ, PLTGOT, PLTREL) adds 4 more entries.
        //   All extern references resolve through GLOB_DAT, so the JMPREL group is
        //   omitted, matching the corpus payloads that also lack JUMP_SLOT entries.
        bool emitJmprelGroup = false;
        int dtEntryCount = sonames.Count + 18 + (emitJmprelGroup ? 4 : 0);
        ulong dynamicSize = (ulong)dtEntryCount * 16;
        int maxR64 = 0;
        foreach (ElfObject obj in resolution.Included)
            foreach (IReadOnlyList<ElfRelocation> rl in obj.Relocations.Values)
                foreach (ElfRelocation r in rl)
                    if (r.Type == RelType.R64) maxR64++;
        int maxRelaEntries = maxR64 + internalGotSlots + externs.Count + 8;
        ulong relaReserve = Align((ulong)maxRelaEntries * 24, 8);

        ulong ehFrameHdrOffset = Align(roLen, 8);
        ulong dynsymOffset = Align(ehFrameHdrOffset + (ulong)ehFrameHdrSize, 8);
        ulong dynstrOffset = dynsymOffset + (ulong)dynsymBytes.Length;
        ulong hashOffset = Align(dynstrOffset + (ulong)dynstrBytes.Length, 4);
        ulong gotOffset = Align(hashOffset + (ulong)hashBytes.Length, 8);
        ulong dynamicOffset = Align(gotOffset + (ulong)totalGotSlots * 8, 8);
        ulong relaOffset = Align(dynamicOffset + dynamicSize, 8);
        // roSize includes the .rela.dyn reserve so the middle segment's map covers the actual
        // relocation entries plus a small overshoot budget. The actual .rela.dyn write is smaller
        // (or equal) and any trailing bytes in the map are zero-filled from initial allocation.
        ulong roSize = relaOffset + relaReserve;

        // Data group (mapped RW): only the runtime .data and .bss (plus the TLS template if any).
        // File size is dataFile; mem size is dataMem, so p_memsz can exceed p_filesz for .bss.
        ulong dataFileEnd = dataFile;
        ulong dataMemEnd = dataMem;

        // Segment addresses. The text base VA is 0: the first LOAD segment maps the text from
        // file offset SegAlign to VA 0, matching the standard linker output.
        ulong textAddr = 0;
        ulong roAddr = Align(textAddr + textSize, SegAlign);
        ulong dataAddr = Align(roAddr + roSize, SegAlign);
        ulong gotAddr = roAddr + gotOffset;
        ulong dynamicAddr = roAddr + dynamicOffset;
        ulong relaAddr = roAddr + relaOffset;
        ulong tlsTemplateAddr = hasTls ? dataAddr + tlsTemplateOffsetInData : 0;

        ulong dynsymAddr = roAddr + dynsymOffset;
        ulong dynstrAddr = roAddr + dynstrOffset;
        ulong hashAddr = roAddr + hashOffset;

        for (int i = 0; i < externs.Count; i++)
        {
            externs[i].GotAddress = gotAddr + (ulong)i * 8;
            externs[i].PltAddress = textAddr + pltBase + (ulong)i * 16;
        }

        ulong retStubAddr = textAddr + retStubOffset;
        ulong ehFrameHdrAddr = ehFrameHdrSize > 0 ? roAddr + ehFrameHdrOffset : textAddr;
        ulong imageEndAddr = dataAddr + dataMemEnd;
        // BSS starts after the initialised data in the data segment, aligned to 16.
        // Known working payloads place
        // __bss_start at the first uninitialised byte, not at the image end. When there
        // is no BSS (dataFileEnd == dataMemEnd), the aligned boundary could overshoot
        // imageEndAddr; clamp so __bss_start never exceeds the image boundary.
        ulong bssAddr = Math.Min(Align(dataAddr + dataFileEnd, 16), imageEndAddr);
        // init/fini arrays live in the middle group (RO segment). Empty arrays still
        // get addresses within the RO segment so the .init_array / .fini_array section
        // headers and DT_ tags point into the correct segment, matching the corpus layout.
        ulong initArrayStartAddr, initArrayEndAddr;
        if (haveInit) { initArrayStartAddr = roAddr + initStart; initArrayEndAddr = roAddr + initEnd; }
        else { initArrayStartAddr = initArrayEndAddr = roAddr; }
        ulong finiArrayStartAddr, finiArrayEndAddr;
        if (haveFini) { finiArrayStartAddr = roAddr + finiStart; finiArrayEndAddr = roAddr + finiEnd; }
        else { finiArrayStartAddr = finiArrayEndAddr = initArrayEndAddr; }
        var linkerDefined = new Dictionary<string, ulong>(StringComparer.Ordinal)
        {
            [CompatEmitter.ModuleBaseSymbol] = textAddr,
            [CompatEmitter.TextEndSymbol] = textAddr + textSize,
            [CompatEmitter.FrameIndexSymbol] = ehFrameHdrAddr,
            [CompatEmitter.FrameIndexEndSymbol] = ehFrameHdrSize > 0 ? ehFrameHdrAddr + (ulong)ehFrameHdrSize : textAddr,
            ["_init"] = retStubAddr,
            ["_fini"] = retStubAddr,
            ["__image_start"] = 0,
            ["__image_end"] = imageEndAddr,
            ["__bss_start"] = bssAddr,
            ["__bss_end"] = imageEndAddr,
            ["__init_array_start"] = initArrayStartAddr,
            ["__init_array_end"] = initArrayEndAddr,
            ["__fini_array_start"] = finiArrayStartAddr,
            ["__fini_array_end"] = finiArrayEndAddr,
            ["__preinit_array_start"] = imageEndAddr,
            ["__preinit_array_end"] = imageEndAddr,
            ["_DYNAMIC"] = dynamicAddr,
            ["edata"] = dataAddr + dataFileEnd,
            ["end"] = imageEndAddr,
            ["etext"] = textAddr + textSize,
            ["_edata"] = dataAddr + dataFileEnd,
            ["_end"] = imageEndAddr,
            ["_etext"] = textAddr + textSize,
        };

        ulong SectionAddr(ElfObject o, int i)
        {
            if (i == ShnCommon)
                throw new ElfLinkException($"{o.Origin}: a common (uninitialized global) symbol has no storage. Compile with -fno-common.");
            if ((uint)i >= (uint)o.Sections.Count)
                throw new ElfLinkException($"{o.Origin}: a symbol refers to section index {i}, which the object does not define.");
            ElfSection s = o.Sections[i];
            if (s.IsTls) return tlsTemplateAddr + tlsOffset[(o, i)];
            ulong bas;
            if (s.IsExecutable) bas = textAddr;
            else if (s.IsWritable && IsRelroLike(s.Name) && !s.IsNoBits) bas = roAddr;
            else if (s.IsWritable) bas = dataAddr;
            else bas = roAddr;
            return bas + offsetInGroup[(o, i)];
        }

        // Resolve and fix up.
        var relatives = new List<Relative>();
        var internalGot = new Dictionary<string, ulong>(StringComparer.Ordinal);
        var extraGotSlots = new List<(ulong Slot, ulong Value)>();
        var tlsGotSlots = new List<(ulong Slot, ulong Value)>();
        Dictionary<(ElfObject, int), byte[]> sectionData = Relocate(
            resolution, externByName, SectionAddr, relatives, internalGot, extraGotSlots, tlsGotSlots,
            gotAddr, externs.Count, dynamicAddr, tlsOffset, tlsAlignedMem, linkerDefined);

        // GOT data.
        int totalGot = externs.Count + internalGot.Count;
        byte[] gotData = new byte[totalGot * 8];
        foreach ((ulong slot, ulong value) in extraGotSlots)
        {
            BinaryPrimitives.WriteUInt64LittleEndian(gotData.AsSpan((int)(slot - gotAddr)), value);
            relatives.Add(new Relative(slot, value));
        }
        foreach ((ulong slot, ulong value) in tlsGotSlots)
            BinaryPrimitives.WriteUInt64LittleEndian(gotData.AsSpan((int)(slot - gotAddr)), value);

        // PLT stubs.
        byte[] pltData = new byte[externs.Count * 16];
        for (int i = 0; i < externs.Count; i++)
        {
            int p = i * 16;
            pltData[p] = 0xFF; pltData[p + 1] = 0x25;
            long disp = (long)externs[i].GotAddress - (long)(externs[i].PltAddress + 6);
            BinaryPrimitives.WriteInt32LittleEndian(pltData.AsSpan(p + 2), unchecked((int)disp));
        }

        // Build the .rela.dyn data: RELATIVE entries first (sorted), then GLOB_DAT entries.
        relatives.Sort((a, b) => a.Offset.CompareTo(b.Offset));
        int relativeCount = relatives.Count;
        int totalRelaEntries = relativeCount + externs.Count;
        byte[] relaBytes = new byte[totalRelaEntries * 24];
        for (int i = 0; i < relativeCount; i++)
        {
            int b = i * 24;
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b), relatives[i].Offset);
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b + 8), RRelative);
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b + 16), relatives[i].Addend);
        }
        for (int i = 0; i < externs.Count; i++)
        {
            int b = (relativeCount + i) * 24;
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b), externs[i].GotAddress);
            ulong info = ((ulong)(uint)(i + 1) << 32) | RGlobDat;
            BinaryPrimitives.WriteUInt64LittleEndian(relaBytes.AsSpan(b + 8), info);
            // addend = 0 for GLOB_DAT
        }

        // .rela.dyn lives in the middle group (its offset was reserved up front as relaOffset,
        // and roSize includes the relocation reserve). The address was fixed above at
        // relaAddr = roAddr + relaOffset. If the actual entry count exceeds the reserve, the
        // reserve was under-sized: refuse rather than overrun the RO segment.
        ulong relaSize = (ulong)relaBytes.Length;
        if (relaSize > relaReserve)
            throw new ElfLinkException(
                $"The relocation reserve ({relaReserve} bytes) is smaller than the actual .rela.dyn " +
                $"data ({relaSize} bytes). Grow the maxRelaEntries slack in PayloadWriter.");

        // Build the .dynamic section now that all VAs are known.
        // Tag order follows the standard layout: NEEDED, FLAGS_1, DEBUG, RELA group,
        // JMPREL group, SYMTAB/SYMENT, STRTAB/STRSZ, GNU_HASH, PREINIT_ARRAY,
        // INIT_ARRAY, FINI_ARRAY, NULL.
        byte[] dynamicData = new byte[(int)dynamicSize];
        int di = 0;
        void WriteDt(long tag, ulong val)
        {
            BinaryPrimitives.WriteInt64LittleEndian(dynamicData.AsSpan(di), tag);
            BinaryPrimitives.WriteUInt64LittleEndian(dynamicData.AsSpan(di + 8), val);
            di += 16;
        }
        foreach (int off in sonameStrOff) WriteDt(DtNeeded, (ulong)off);
        WriteDt(DtFlags1, 0x08000000); // DF_1_PIE
        WriteDt(DtDebug, 0);
        WriteDt(DtRela, relaAddr);
        WriteDt(DtRelaSz, relaSize);
        WriteDt(DtRelaEnt, 24);
        WriteDt(DtRelaCount, (ulong)relativeCount);
        // JMPREL group: present only when JUMP_SLOT relocations exist. Corpus payloads
        // without JUMP_SLOT relocs omit these four tags entirely.
        if (emitJmprelGroup)
        {
            WriteDt(DtJmpRel, relaAddr + relaSize);
            WriteDt(DtPltRelSz, 0);
            WriteDt(DtPltGot, gotAddr);
            WriteDt(DtPltRel, 7);
        }
        WriteDt(DtSymTab, dynsymAddr);
        WriteDt(DtSymEnt, 24);
        WriteDt(DtStrTab, dynstrAddr);
        WriteDt(DtStrSz, (ulong)dynstrBytes.Length);
        WriteDt(DtGnuHash, hashAddr);
        WriteDt(DtPreInit_Array, 0);
        WriteDt(DtPreInit_ArraySz, 0);
        // INIT_ARRAY and FINI_ARRAY are ALWAYS present (per the standard layout), even when size=0.
        WriteDt(DtInit_Array, initArrayStartAddr);
        WriteDt(DtInit_ArraySz, haveInit ? initArrayEndAddr - initArrayStartAddr : 0);
        WriteDt(DtFini_Array, finiArrayStartAddr);
        WriteDt(DtFini_ArraySz, haveFini ? finiArrayEndAddr - finiArrayStartAddr : 0);
        WriteDt(DtNull, 0);

        // Exception-frame index.
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

        ulong entry = 0;
        if (!string.IsNullOrEmpty(entrySymbol) && TryFindSymbol(resolution, entrySymbol, out ElfObject? eo, out ElfSymbol? es))
            entry = SectionAddr(eo!, es!.SectionIndex) + es.Value;

        // Build .symtab and .strtab for the output ELF. Known working payloads carry a
        // populated .symtab with LOCAL HIDDEN linker symbols and GLOBAL function symbols.
        // Section indices for each symbol are mapped from their VA address.
        var outStrtab = new List<byte> { 0 }; // index 0 is the empty string
        var outSymtab = new List<byte>(new byte[24]); // NULL entry
        int OutStrtabAdd(string s)
        {
            int off = outStrtab.Count;
            outStrtab.AddRange(Encoding.ASCII.GetBytes(s));
            outStrtab.Add(0);
            return off;
        }
        void OutSymtabAdd(string name, ulong value, ulong size, byte info, byte other, ushort shndx)
        {
            byte[] sym = new byte[24];
            BinaryPrimitives.WriteUInt32LittleEndian(sym, (uint)OutStrtabAdd(name));
            sym[4] = info;
            sym[5] = other;
            BinaryPrimitives.WriteUInt16LittleEndian(sym.AsSpan(6), shndx);
            BinaryPrimitives.WriteUInt64LittleEndian(sym.AsSpan(8), value);
            BinaryPrimitives.WriteUInt64LittleEndian(sym.AsSpan(16), size);
            outSymtab.AddRange(sym);
        }

        // Section index constants matching the section header table.
        const ushort siText = 1, siInitArray = 14, siFiniArray = 15;
        const ushort siDynamic = 16, siData = 17, siBss = 18;

        // Map a virtual address to its output section index.
        ushort VaToShIdx(ulong va)
        {
            if (va >= textAddr && va < textAddr + textSize) return siText;
            if (va == dynamicAddr) return siDynamic;
            if (haveInit && va >= initArrayStartAddr && va <= initArrayEndAddr) return siInitArray;
            if (haveFini && va >= finiArrayStartAddr && va <= finiArrayEndAddr) return siFiniArray;
            if (!haveInit && !haveFini && va == initArrayStartAddr) return siInitArray;
            if (va >= bssAddr) return siBss;
            if (va >= dataAddr) return siData;
            return siText;
        }

        // 9 hidden linker symbols (LOCAL HIDDEN NOTYPE), matching known working payloads.
        // STB_LOCAL=0, STT_NOTYPE=0 -> info=0. STV_HIDDEN=2 -> other=2.
        OutSymtabAdd("__bss_start", bssAddr, 0, 0, 2, siBss);
        OutSymtabAdd("__bss_end", imageEndAddr, 0, 0, 2, siBss);
        OutSymtabAdd("__image_start", 0, 0, 0, 2, siText);
        OutSymtabAdd("__image_end", imageEndAddr, 0, 0, 2, siBss);
        OutSymtabAdd("_DYNAMIC", dynamicAddr, 0, 0, 2, siDynamic);
        OutSymtabAdd("__init_array_end", initArrayEndAddr, 0, 0, 2, siInitArray);
        OutSymtabAdd("__init_array_start", initArrayStartAddr, 0, 0, 2, siInitArray);
        OutSymtabAdd("__fini_array_end", finiArrayEndAddr, 0, 0, 2, siFiniArray);
        OutSymtabAdd("__fini_array_start", finiArrayStartAddr, 0, 0, 2, siFiniArray);
        int localSymCount = outSymtab.Count / 24; // NULL + 9 hidden = 10

        // GLOBAL symbols from all defined objects.
        var emittedGlobals = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject obj in resolution.Included)
            foreach (ElfSymbol sym in obj.Symbols)
            {
                if (sym.IsUndefined || sym.Name.Length == 0) continue;
                byte bind = (byte)(sym.Info >> 4);
                if (bind == 0) continue; // LOCAL symbols already covered above
                if (!emittedGlobals.Add(sym.Name)) continue; // deduplicate
                if (sym.SectionIndex == ShnAbs)
                {
                    OutSymtabAdd(sym.Name, sym.Value, sym.Size, sym.Info, sym.Other, 0xFFF1);
                    continue;
                }
                ulong va = SectionAddr(obj, sym.SectionIndex) + sym.Value;
                OutSymtabAdd(sym.Name, va, sym.Size, sym.Info, sym.Other, VaToShIdx(va));
            }

        byte[] symtabData = [.. outSymtab];
        byte[] strtabData = [.. outStrtab];

        return WriteFile(resolution, entry, sectionData, SectionAddr,
            textAddr, textSize, pltBase, pltData, retStubOffset, roAddr, roSize,
            dynsymOffset, dynsymBytes, dynstrOffset, dynstrBytes, hashOffset, hashBytes,
            ehFrameHdrOffset, ehFrameHdr, gotOffset, gotData,
            dynamicOffset, dynamicData, relaOffset, relaBytes,
            dataAddr, dataFileEnd, dataMemEnd, dynamicAddr,
            initArrayStartAddr, haveInit ? initArrayEndAddr - initArrayStartAddr : 0,
            finiArrayStartAddr, haveFini ? finiArrayEndAddr - finiArrayStartAddr : 0,
            symtabData, strtabData, localSymCount);
    }

    private static Dictionary<(ElfObject, int), byte[]> Relocate(
        LinkResolution resolution, Dictionary<string, Extern> externByName, Func<ElfObject, int, ulong> sectionAddr,
        List<Relative> relatives, Dictionary<string, ulong> internalGot, List<(ulong, ulong)> extraGotSlots,
        List<(ulong, ulong)> tlsGotSlots, ulong gotAddr, int externCount, ulong importTableAddr,
        Dictionary<(ElfObject, int), ulong> tlsOffset, ulong tlsAlignedMem, Dictionary<string, ulong> linkerDefined)
    {
        var result = new Dictionary<(ElfObject, int), byte[]>();
        ulong NextInternalGot() => gotAddr + (ulong)(externCount + internalGot.Count) * 8;

        foreach (ElfObject obj in resolution.Included)
        {
            foreach (KeyValuePair<int, IReadOnlyList<ElfRelocation>> kv in obj.Relocations)
            {
                int idx = kv.Key;
                if (idx >= obj.Sections.Count) continue;
                ElfSection sec = obj.Sections[idx];
                if (!sec.IsAlloc) continue;
                byte[] bytes = sec.IsNoBits ? new byte[sec.Size] : (byte[])sec.Data.Clone();
                ulong secAddr = sectionAddr(obj, idx);

                HashSet<ulong>? foldedTlsCall = null;
                foreach (ElfRelocation probe in kv.Value)
                {
                    if (probe.Type == RelType.TlsGd) (foldedTlsCall ??= []).Add(probe.Offset + 8);
                    else if (probe.Type == RelType.TlsLd) (foldedTlsCall ??= []).Add(probe.Offset + 5);
                }

                foreach (ElfRelocation r in kv.Value)
                {
                    if (r.SymbolIndex >= (uint)obj.Symbols.Count) continue;
                    ElfSymbol sym = obj.Symbols[(int)r.SymbolIndex];
                    ulong place = secAddr + r.Offset;
                    int at = (int)r.Offset;

                    if (foldedTlsCall is not null && foldedTlsCall.Contains(r.Offset))
                        continue;

                    if (r.Type == RelType.TlsGd)
                    {
                        if (at - 4 < 0 || at + 12 > bytes.Length)
                            throw new ElfLinkException($"{obj.Origin}: a thread-local sequence on '{sym.Name}' runs past the section.");
                        if (!TryTlsTemplateOffset(resolution, tlsOffset, obj, sym, out ulong gdTemplateOff))
                            throw new ElfLinkException($"Thread-local symbol '{sym.Name}' has no template section in the payload.");
                        long le = (long)gdTemplateOff - (long)tlsAlignedMem;
                        ReadOnlySpan<byte> localExec = [0x64, 0x48, 0x8B, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x80];
                        localExec.CopyTo(bytes.AsSpan(at - 4));
                        BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(at + 8), checked((int)le));
                        continue;
                    }

                    if (r.Type == RelType.TlsLd)
                    {
                        if (at - 3 < 0 || at + 9 > bytes.Length)
                            throw new ElfLinkException($"{obj.Origin}: a thread-local base sequence on '{sym.Name}' runs past the section.");
                        ReadOnlySpan<byte> threadPointer = [0x66, 0x66, 0x66, 0x64, 0x48, 0x8B, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00];
                        threadPointer.CopyTo(bytes.AsSpan(at - 3));
                        continue;
                    }

                    int width = r.Type is RelType.R64 or RelType.TpOff64 or RelType.Pc64 or RelType.DtpOff64 ? 8 : 4;
                    if (at < 0 || at + width > bytes.Length) continue;

                    if (r.Type == RelType.GotTpOff)
                    {
                        ulong tslot = TlsGotSlotFor(resolution, sectionAddr, obj, sym, internalGot, tlsGotSlots, NextInternalGot, tlsOffset, tlsAlignedMem);
                        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(tslot + (ulong)r.Addend - place));
                        continue;
                    }
                    if (r.Type is RelType.TpOff32 or RelType.TpOff64 or RelType.DtpOff32 or RelType.DtpOff64)
                    {
                        if (!TryTlsTemplateOffset(resolution, tlsOffset, obj, sym, out ulong templateOff))
                            throw new ElfLinkException($"Thread-local symbol '{sym.Name}' has no template section in the payload.");
                        long tp = (long)templateOff - (long)tlsAlignedMem + r.Addend;
                        if (r.Type is RelType.TpOff64 or RelType.DtpOff64) BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at), (ulong)tp);
                        else BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), unchecked((uint)(int)tp));
                        continue;
                    }

                    if (RelType.IsGotPcRel(r.Type))
                    {
                        ulong slot = GotSlotFor(resolution, externByName, sectionAddr, obj, sym, gotAddr, externCount, internalGot, extraGotSlots, NextInternalGot, linkerDefined);
                        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(slot + (ulong)r.Addend - place));
                        continue;
                    }

                    ulong s = SymbolValue(resolution, externByName, sectionAddr, obj, sym, linkerDefined);
                    switch (r.Type)
                    {
                        case RelType.None:
                            break;
                        case RelType.R64:
                            BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at), s + (ulong)r.Addend);
                            if (s + (ulong)r.Addend != 0 && !(sym.IsWeak && sym.IsUndefined && !externByName.ContainsKey(sym.Name) && !resolution.Defined.ContainsKey(sym.Name)))
                                relatives.Add(new Relative(place, s + (ulong)r.Addend));
                            break;
                        case RelType.Pc32:
                        case RelType.Plt32:
                            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(at), (uint)(long)(s + (ulong)r.Addend - place)); break;
                        case RelType.Pc64:
                            BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(at), (ulong)((long)(s + (ulong)r.Addend) - (long)place)); break;
                        case RelType.R32:
                        case RelType.R32S:
                            throw new ElfLinkException(
                                $"A 32-bit absolute relocation on '{sym.Name}' cannot be fixed up in a payload; compile position-independent code.");
                        default:
                            throw new ElfLinkException(
                                $"{obj.Origin}: unsupported relocation type {r.Type} on '{sym.Name}'. A payload resolves absolute, PC-relative, PLT and GOT-relative references.");
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

    private static int CountInternalGotSlots(LinkResolution resolution, Dictionary<string, Extern> externByName)
    {
        var keys = new HashSet<string>(StringComparer.Ordinal);
        foreach (ElfObject obj in resolution.Included)
            foreach (KeyValuePair<int, IReadOnlyList<ElfRelocation>> kv in obj.Relocations)
            {
                if (kv.Key >= obj.Sections.Count || !obj.Sections[kv.Key].IsAlloc) continue;
                foreach (ElfRelocation r in kv.Value)
                {
                    if (r.SymbolIndex >= (uint)obj.Symbols.Count) continue;
                    ElfSymbol sym = obj.Symbols[(int)r.SymbolIndex];
                    string tail = sym.Name.Length > 0 ? sym.Name : $"#{sym.SectionIndex}:{sym.Value}";
                    if (r.Type == RelType.GotTpOff) keys.Add("%tls%" + tail);
                    else if (RelType.IsGotPcRel(r.Type) && !externByName.ContainsKey(sym.Name)) keys.Add(tail);
                }
            }
        return keys.Count;
    }

    private static ulong GotSlotFor(
        LinkResolution resolution, Dictionary<string, Extern> externByName, Func<ElfObject, int, ulong> sectionAddr,
        ElfObject obj, ElfSymbol sym, ulong gotAddr, int externCount, Dictionary<string, ulong> internalGot,
        List<(ulong, ulong)> extraGotSlots, Func<ulong> nextInternalGot, Dictionary<string, ulong> linkerDefined)
    {
        if (externByName.TryGetValue(sym.Name, out Extern? e))
            return e.GotAddress;
        string key = sym.Name.Length > 0 ? sym.Name : $"#{sym.SectionIndex}:{sym.Value}";
        if (internalGot.TryGetValue(key, out ulong slot))
            return slot;
        slot = nextInternalGot();
        internalGot[key] = slot;
        ulong value = SymbolValueDefined(resolution, sectionAddr, obj, sym, linkerDefined);
        extraGotSlots.Add((slot, value));
        return slot;
    }

    private static ulong TlsGotSlotFor(
        LinkResolution resolution, Func<ElfObject, int, ulong> sectionAddr, ElfObject obj, ElfSymbol sym,
        Dictionary<string, ulong> internalGot, List<(ulong, ulong)> tlsGotSlots, Func<ulong> nextInternalGot,
        Dictionary<(ElfObject, int), ulong> tlsOffset, ulong tlsAlignedMem)
    {
        string key = "%tls%" + (sym.Name.Length > 0 ? sym.Name : $"#{sym.SectionIndex}:{sym.Value}");
        if (internalGot.TryGetValue(key, out ulong slot)) return slot;
        slot = nextInternalGot();
        internalGot[key] = slot;
        if (!TryTlsTemplateOffset(resolution, tlsOffset, obj, sym, out ulong templateOffset))
            throw new ElfLinkException($"Thread-local symbol '{sym.Name}' referenced through the GOT has no template section in the payload.");
        tlsGotSlots.Add((slot, (ulong)((long)templateOffset - (long)tlsAlignedMem)));
        return slot;
    }

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

    private static ulong SymbolValue(
        LinkResolution resolution, Dictionary<string, Extern> externByName,
        Func<ElfObject, int, ulong> sectionAddr, ElfObject obj, ElfSymbol sym, Dictionary<string, ulong> linkerDefined)
    {
        if (sym.SectionIndex == ShnAbs) return sym.Value;
        if (sym.Type == SymType.Section) return sectionAddr(obj, sym.SectionIndex);
        if (!sym.IsUndefined) return sectionAddr(obj, sym.SectionIndex) + sym.Value;
        if (linkerDefined.TryGetValue(sym.Name, out ulong provided)) return provided;
        if (resolution.Defined.TryGetValue(sym.Name, out ElfObject? defObj))
            foreach (ElfSymbol d in defObj.Symbols)
                if (!d.IsUndefined && d.Name == sym.Name)
                    return d.SectionIndex == ShnAbs ? d.Value : sectionAddr(defObj, d.SectionIndex) + d.Value;
        if (externByName.TryGetValue(sym.Name, out Extern? e)) return e.PltAddress;
        if (TryEncapsulationAddress(resolution, sectionAddr, sym.Name, out ulong enc)) return enc;
        if (sym.IsWeak) return 0;
        throw new ElfLinkException($"Unresolved symbol '{sym.Name}'.");
    }

    private static ulong SymbolValueDefined(LinkResolution resolution, Func<ElfObject, int, ulong> sectionAddr, ElfObject obj, ElfSymbol sym, Dictionary<string, ulong> linkerDefined)
    {
        if (sym.SectionIndex == ShnAbs) return sym.Value;
        if (sym.Type == SymType.Section) return sectionAddr(obj, sym.SectionIndex);
        if (!sym.IsUndefined) return sectionAddr(obj, sym.SectionIndex) + sym.Value;
        if (linkerDefined.TryGetValue(sym.Name, out ulong provided)) return provided;
        if (resolution.Defined.TryGetValue(sym.Name, out ElfObject? defObj))
            foreach (ElfSymbol d in defObj.Symbols)
                if (!d.IsUndefined && d.Name == sym.Name)
                    return d.SectionIndex == ShnAbs ? d.Value : sectionAddr(defObj, d.SectionIndex) + d.Value;
        if (TryEncapsulationAddress(resolution, sectionAddr, sym.Name, out ulong enc)) return enc;
        return 0;
    }

    private static bool TryEncapsulationAddress(LinkResolution resolution, Func<ElfObject, int, ulong> sectionAddr, string name, out ulong addr)
    {
        addr = 0;
        bool isStop;
        string section;
        if (name.StartsWith("__start_", StringComparison.Ordinal)) { isStop = false; section = name["__start_".Length..]; }
        else if (name.StartsWith("__stop_", StringComparison.Ordinal)) { isStop = true; section = name["__stop_".Length..]; }
        else return false;
        if (section.Length == 0) return false;
        bool found = false;
        ulong min = ulong.MaxValue, max = 0;
        foreach (ElfObject o in resolution.Included)
            for (int i = 0; i < o.Sections.Count; i++)
            {
                ElfSection s = o.Sections[i];
                if (!s.IsAlloc || s.Name != section) continue;
                ulong a = sectionAddr(o, i);
                if (a < min) min = a;
                if (a + s.Size > max) max = a + s.Size;
                found = true;
            }
        if (!found) return false;
        addr = isStop ? max : min;
        return true;
    }

    private static bool TryFindSymbol(LinkResolution resolution, string name, out ElfObject? obj, out ElfSymbol? sym)
    {
        obj = null; sym = null;
        if (!resolution.Defined.TryGetValue(name, out ElfObject? defObj)) return false;
        foreach (ElfSymbol s in defObj.Symbols)
            if (!s.IsUndefined && s.Name == name) { obj = defObj; sym = s; return true; }
        return false;
    }

    private static byte[] WriteFile(
        LinkResolution resolution, ulong entry, Dictionary<(ElfObject, int), byte[]> sectionData, Func<ElfObject, int, ulong> sectionAddr,
        ulong textAddr, ulong textSize, ulong pltBase, byte[] pltData, ulong retStubOffset,
        ulong roAddr, ulong roSize,
        ulong dynsymOffset, byte[] dynsymBytes, ulong dynstrOffset, byte[] dynstrBytes, ulong hashOffset, byte[] hashBytes,
        ulong ehFrameHdrOffset, byte[] ehFrameHdr,
        ulong gotOffset, byte[] gotData,
        ulong dynamicOffset, byte[] dynamicData, ulong relaOffset, byte[] relaBytes,
        ulong dataAddr, ulong dataFileEnd, ulong dataMemEnd, ulong dynamicAddr,
        ulong initArrayAddr, ulong initArraySize, ulong finiArrayAddr, ulong finiArraySize,
        byte[] symtabData, byte[] strtabData, int localSymCount)
    {
        const int phCount = 4; // 3 PT_LOAD + 1 PT_DYNAMIC
        ulong textFileOff = SegAlign;
        ulong roFileOff = textFileOff + Align(textSize, SegAlign);
        ulong dataFileOff = roFileOff + Align(roSize, SegAlign);
        ulong afterData = dataFileOff + dataFileEnd;

        // Section-header string table: 23 section names matching the standard payload layout.
        var shstr = new List<byte> { 0 };
        int AddShName(string s) { int o = shstr.Count; shstr.AddRange(Encoding.ASCII.GetBytes(s)); shstr.Add(0); return o; }
        int nText = AddShName(".text"), nPlt = AddShName(".plt");
        int nEhFrameHdr = AddShName(".eh_frame_hdr"), nEhFrame = AddShName(".eh_frame");
        int nDynsym = AddShName(".dynsym"), nHash = AddShName(".gnu.hash"), nDynstr = AddShName(".dynstr");
        int nRela = AddShName(".rela.dyn"), nRelaPlt = AddShName(".rela.plt");
        int nDataRelRo = AddShName(".data.rel.ro");
        int nGot = AddShName(".got"), nGotPlt = AddShName(".got.plt");
        int nRodata = AddShName(".rodata");
        int nInitArray = AddShName(".init_array"), nFiniArray = AddShName(".fini_array");
        int nDynamic = AddShName(".dynamic");
        int nData = AddShName(".data"), nBss = AddShName(".bss");
        int nComment = AddShName(".comment");
        int nSymtab = AddShName(".symtab");
        int nShStr = AddShName(".shstrtab");
        int nStrtab = AddShName(".strtab");
        ulong shstrFileOff = Align(afterData, 8);
        ulong strtabFileOff = Align(shstrFileOff + (ulong)shstr.Count, 8);
        ulong symtabFileOff = Align(strtabFileOff + (ulong)strtabData.Length, 8);
        ulong shdrFileOff = Align(symtabFileOff + (ulong)symtabData.Length, 8);

        const int shCount = 23;
        ulong fileEnd = shdrFileOff + (ulong)shCount * 64;
        byte[] file = new byte[Align(fileEnd, 16)];

        // ELF header.
        file[0] = 0x7F; file[1] = (byte)'E'; file[2] = (byte)'L'; file[3] = (byte)'F';
        file[4] = 2; file[5] = 1; file[6] = 1; // EI_OSABI = ELFOSABI_NONE (0), ABI version 0
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x10), 3);     // ET_DYN
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x12), 0x3E);  // x86-64
        BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(0x14), 1);
        BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(0x18), entry);
        BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(0x20), 0x40);              // e_phoff
        BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(0x28), shdrFileOff);       // e_shoff
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x34), 0x40);              // e_ehsize
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x36), 0x38);              // e_phentsize
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x38), phCount);
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x3A), 0x40);              // e_shentsize
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x3C), shCount);           // e_shnum
        BinaryPrimitives.WriteUInt16LittleEndian(file.AsSpan(0x3E), 21);               // e_shstrndx (shIdxShStr)

        int ph = 0x40;
        void WritePh(uint type, uint flags, ulong off, ulong va, ulong filesz, ulong memsz)
        {
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(ph), type);
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(ph + 4), flags);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 8), off);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 16), va);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 24), va);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 32), filesz);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 40), memsz);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(ph + 48), SegAlign);
            ph += 0x38;
        }
        // Program headers - corpus shape (matching the standard payload layout.:
        //   [0] LOAD RWE - text + PLT stubs, file offset = 0x4000, VA = 0.
        //   [1] LOAD RW  - middle segment: readonly content + relro-like writable + dyn tables.
        //   [2] LOAD RW  - data segment: pure runtime .data + .bss; p_memsz > p_filesz for bss.
        //   [3] DYNAMIC  - inside [1]; the runtime linker's entry to the dynamic table.
        WritePh(PtLoad, PfR | PfW | PfX, textFileOff, 0, textSize, textSize);
        WritePh(PtLoad, PfR | PfW, roFileOff, roAddr, roSize, roSize);
        WritePh(PtLoad, PfR | PfW, dataFileOff, dataAddr, dataFileEnd, dataMemEnd);
        WritePh(PtDynamic, PfR | PfW, roFileOff + dynamicOffset, dynamicAddr, (ulong)dynamicData.Length, (ulong)dynamicData.Length);

        void Put(ulong segFileOff, ulong segBase, ulong addr, byte[] bytes)
            => bytes.AsSpan().CopyTo(file.AsSpan((int)(segFileOff + (addr - segBase))));

        foreach (ElfObject obj in resolution.Included)
            for (int i = 0; i < obj.Sections.Count; i++)
            {
                ElfSection sec = obj.Sections[i];
                if (!sec.IsAlloc || sec.IsNoBits || !sectionData.TryGetValue((obj, i), out byte[]? bytes)) continue;
                ulong a = sectionAddr(obj, i);
                ulong segFileOff, segBase;
                if (sec.IsExecutable) { segFileOff = textFileOff; segBase = textAddr; }
                else if (sec.IsWritable && IsRelroLike(sec.Name) && !sec.IsNoBits) { segFileOff = roFileOff; segBase = roAddr; }
                else if (sec.IsWritable) { segFileOff = dataFileOff; segBase = dataAddr; }
                else { segFileOff = roFileOff; segBase = roAddr; }
                Put(segFileOff, segBase, a, bytes);
            }
        Put(textFileOff, textAddr, textAddr + pltBase, pltData);
        file[(int)(textFileOff + retStubOffset)] = 0xC3;
        if (ehFrameHdr.Length > 0)
            Put(roFileOff, roAddr, roAddr + ehFrameHdrOffset, ehFrameHdr);
        Put(roFileOff, roAddr, roAddr + dynsymOffset, dynsymBytes);
        Put(roFileOff, roAddr, roAddr + dynstrOffset, dynstrBytes);
        Put(roFileOff, roAddr, roAddr + hashOffset, hashBytes);
        Put(roFileOff, roAddr, roAddr + gotOffset, gotData);
        Put(roFileOff, roAddr, roAddr + dynamicOffset, dynamicData);
        Put(roFileOff, roAddr, roAddr + relaOffset, relaBytes);
        shstr.ToArray().AsSpan().CopyTo(file.AsSpan((int)shstrFileOff));
        strtabData.AsSpan().CopyTo(file.AsSpan((int)strtabFileOff));
        symtabData.AsSpan().CopyTo(file.AsSpan((int)symtabFileOff));

        // Section headers: 23 sections matching the standard payload layout.layout.
        // [ 0] NULL  [ 1] .text  [ 2] .plt  [ 3] .eh_frame_hdr  [ 4] .eh_frame
        // [ 5] .dynsym  [ 6] .gnu.hash  [ 7] .dynstr  [ 8] .rela.dyn  [ 9] .rela.plt
        // [10] .data.rel.ro  [11] .got  [12] .got.plt  [13] .rodata
        // [14] .init_array  [15] .fini_array  [16] .dynamic  [17] .data  [18] .bss
        // [19] .comment  [20] .symtab  [21] .shstrtab  [22] .strtab
        const int shIdxText = 1, shIdxPlt = 2, shIdxEhFrameHdr = 3, shIdxEhFrame = 4;
        const int shIdxDynsym = 5, shIdxHash = 6, shIdxDynstr = 7;
        const int shIdxRela = 8, shIdxRelaPlt = 9;
        const int shIdxDataRelRo = 10, shIdxGot = 11, shIdxGotPlt = 12, shIdxRodata = 13;
        const int shIdxInitArray = 14, shIdxFiniArray = 15, shIdxDynamic = 16;
        const int shIdxData = 17, shIdxBss = 18;
        const int shIdxComment = 19, shIdxSymtab = 20, shIdxShStr = 21, shIdxStrtab = 22;

        void WriteShdr(int index, int nameOff, uint type, ulong flags, ulong addr, ulong off, ulong size, int link, int info, int align, int entsize)
        {
            int b = (int)shdrFileOff + index * 64;
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(b), (uint)nameOff);
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(b + 4), type);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(b + 8), flags);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(b + 16), addr);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(b + 24), off);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(b + 32), size);
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(b + 40), (uint)link);
            BinaryPrimitives.WriteUInt32LittleEndian(file.AsSpan(b + 44), (uint)info);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(b + 48), (ulong)align);
            BinaryPrimitives.WriteUInt64LittleEndian(file.AsSpan(b + 56), (ulong)entsize);
        }

        // Segment 0 sections: .text + .plt
        ulong pltSize = (ulong)pltData.Length;
        WriteShdr(shIdxText, nText, (uint)ShtProgBits, ShfAlloc | ShfExec,
            textAddr, textFileOff, pltBase, 0, 0, (int)SegAlign, 0);
        WriteShdr(shIdxPlt, nPlt, (uint)ShtProgBits, ShfAlloc | ShfExec,
            textAddr + pltBase, textFileOff + pltBase, pltSize, 0, 0, 16, 0);

        // Segment 1 sections (middle / RO + RELRO): order follows the standard layout
        ulong ehfhAddr = roAddr + ehFrameHdrOffset;
        ulong ehfhSize = (ulong)ehFrameHdr.Length;
        WriteShdr(shIdxEhFrameHdr, nEhFrameHdr, (uint)ShtProgBits, ShfAlloc,
            ehfhAddr, roFileOff + ehFrameHdrOffset, ehfhSize, 0, 0, ehfhSize > 0 ? (int)SegAlign : 1, 0);
        // .eh_frame follows .eh_frame_hdr; its exact bounds depend on input objects.
        // When no eh_frame data is present, the section has size 0.
        ulong ehFrameAddr = ehfhSize > 0 ? Align(ehfhAddr + ehfhSize, 8) : ehfhAddr;
        ulong ehFrameSize = dynsymOffset > ehFrameHdrOffset + ehfhSize
            ? dynsymOffset - Align(ehFrameHdrOffset + ehfhSize, 8) : 0;
        ulong ehFrameOff = roFileOff + (ehFrameAddr - roAddr);
        WriteShdr(shIdxEhFrame, nEhFrame, (uint)ShtProgBits, ShfAlloc,
            ehFrameAddr, ehFrameOff, ehFrameSize, 0, 0, 8, 0);

        WriteShdr(shIdxDynsym, nDynsym, (uint)ShtDynSym, ShfAlloc,
            roAddr + dynsymOffset, roFileOff + dynsymOffset, (ulong)dynsymBytes.Length,
            shIdxDynstr, 1, 8, 24);
        WriteShdr(shIdxHash, nHash, (uint)ShtGnuHash, ShfAlloc,
            roAddr + hashOffset, roFileOff + hashOffset, (ulong)hashBytes.Length,
            shIdxDynsym, 0, 8, 0);
        WriteShdr(shIdxDynstr, nDynstr, (uint)ShtStrTab, ShfAlloc,
            roAddr + dynstrOffset, roFileOff + dynstrOffset, (ulong)dynstrBytes.Length, 0, 0, 1, 0);
        WriteShdr(shIdxRela, nRela, (uint)ShtRela, ShfAlloc,
            roAddr + relaOffset, roFileOff + relaOffset, (ulong)relaBytes.Length,
            shIdxDynsym, 0, 8, 24);
        // .rela.plt: follows .rela.dyn; currently empty (no PLT-targeted relocations yet).
        ulong relaPltOff = relaOffset + (ulong)relaBytes.Length;
        WriteShdr(shIdxRelaPlt, nRelaPlt, (uint)ShtRela, ShfAlloc | ShfInfo,
            roAddr + relaPltOff, roFileOff + relaPltOff, 0,
            shIdxDynsym, shIdxGotPlt, 8, 24);
        // .data.rel.ro: relro content in middle segment (between .rela.plt and .got).
        // When no relro content is placed, the section is empty.
        ulong dataRelRoOff = Align(relaPltOff, 16);
        ulong dataRelRoSize = gotOffset > dataRelRoOff ? gotOffset - dataRelRoOff : 0;
        WriteShdr(shIdxDataRelRo, nDataRelRo, (uint)ShtProgBits, ShfAlloc | ShfWrite,
            roAddr + dataRelRoOff, roFileOff + dataRelRoOff, dataRelRoSize, 0, 0, 16, 0);
        WriteShdr(shIdxGot, nGot, (uint)ShtProgBits, ShfAlloc | ShfWrite,
            roAddr + gotOffset, roFileOff + gotOffset, (ulong)gotData.Length, 0, 0, 8, 0);
        // .got.plt: not yet separated from .got; empty for now.
        ulong gotPltOff = gotOffset + (ulong)gotData.Length;
        WriteShdr(shIdxGotPlt, nGotPlt, (uint)ShtProgBits, ShfAlloc | ShfWrite,
            roAddr + gotPltOff, roFileOff + gotPltOff, 0, 0, 0, 8, 0);
        // .rodata: read-only data after .got.plt in the middle segment.
        ulong rodataOff = Align(gotPltOff, 16);
        ulong rodataEnd = dynamicOffset; // rodata extends up to .dynamic
        ulong rodataSize = rodataEnd > rodataOff ? rodataEnd - rodataOff : 0;
        WriteShdr(shIdxRodata, nRodata, (uint)ShtProgBits, ShfAlloc | ShfMerge | ShfStrings,
            roAddr + rodataOff, roFileOff + rodataOff, rodataSize, 0, 0, 16, 0);
        // .init_array and .fini_array: always present, even when size=0 (per the standard layout).
        ulong initFileOff = initArrayAddr >= roAddr ? roFileOff + (initArrayAddr - roAddr) : roFileOff + dynamicOffset;
        ulong finiFileOff = finiArrayAddr >= roAddr ? roFileOff + (finiArrayAddr - roAddr) : initFileOff;
        WriteShdr(shIdxInitArray, nInitArray, (uint)ShtProgBits, ShfAlloc,
            initArrayAddr, initFileOff, initArraySize, 0, 0, 1, 0);
        WriteShdr(shIdxFiniArray, nFiniArray, (uint)ShtProgBits, ShfAlloc,
            finiArrayAddr, finiFileOff, finiArraySize, 0, 0, 1, 0);
        WriteShdr(shIdxDynamic, nDynamic, (uint)ShtDynamic, ShfAlloc | ShfWrite,
            dynamicAddr, roFileOff + dynamicOffset, (ulong)dynamicData.Length,
            shIdxDynstr, 0, (int)SegAlign, 16);

        // Segment 2 sections: .data + .bss
        WriteShdr(shIdxData, nData, (uint)ShtProgBits, ShfAlloc | ShfWrite,
            dataAddr, dataFileOff, dataFileEnd, 0, 0, (int)SegAlign, 0);
        // .bss: uninitialised data following .data; SHT_NOBITS.
        ulong bssAddr = Align(dataAddr + dataFileEnd, 16);
        ulong bssSize = dataMemEnd > (bssAddr - dataAddr) ? dataMemEnd - (bssAddr - dataAddr) : 0;
        WriteShdr(shIdxBss, nBss, (uint)ShtNoBits, ShfAlloc | ShfWrite,
            bssAddr, dataFileOff + dataFileEnd, bssSize, 0, 0, 16, 0);

        // Non-alloc sections: .comment, .symtab, .shstrtab, .strtab
        WriteShdr(shIdxComment, nComment, (uint)ShtProgBits, ShfMerge | ShfStrings,
            0, shstrFileOff, 0, 0, 0, 1, 1);
        WriteShdr(shIdxSymtab, nSymtab, (uint)ShtSymTab, 0,
            0, symtabFileOff, (ulong)symtabData.Length, shIdxStrtab, localSymCount, 8, 24);
        WriteShdr(shIdxShStr, nShStr, (uint)ShtStrTab, 0,
            0, shstrFileOff, (ulong)shstr.Count, 0, 0, 1, 0);
        WriteShdr(shIdxStrtab, nStrtab, (uint)ShtStrTab, 0,
            0, strtabFileOff, (ulong)strtabData.Length, 0, 0, 1, 0);
        return file;
    }

    private static ulong Align(ulong v, ulong a) => a <= 1 ? v : (v + a - 1) / a * a;

    private static uint GnuHash(string name)
    {
        uint h = 5381;
        foreach (byte c in Encoding.ASCII.GetBytes(name))
            h = (h << 5) + h + c;
        return h;
    }
}
