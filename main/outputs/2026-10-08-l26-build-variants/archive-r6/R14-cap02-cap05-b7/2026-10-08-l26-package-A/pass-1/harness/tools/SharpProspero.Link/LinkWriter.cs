// SharpProspero.Link - a linker for module output.
// Copyright (C) 2026 SvenGDK
//
// Writes a self-contained module: a resolved graph with no imported symbols. It shares the module
// writer with the importing path so the layout, the relocation fixups, and the dynamic table are
// produced the same way; a self-contained graph simply has an empty import set.

namespace SharpProspero.Link;

/// <summary>Writes a linked module from a resolved symbol graph that has no imports.</summary>
public static class LinkWriter
{
    /// <summary>
    /// Lays out, relocates, and writes the module for <paramref name="resolution"/>. The entry point
    /// is the address of <paramref name="entrySymbol"/> when given, or the start of the text segment.
    /// </summary>
    /// <exception cref="ElfLinkException">A referenced symbol is unresolved.</exception>
    public static byte[] WriteExecutable(LinkResolution resolution, string? entrySymbol, ModuleKind kind = ModuleKind.Executable)
        => DynamicWriter.Write(resolution, entrySymbol, kind);
}
