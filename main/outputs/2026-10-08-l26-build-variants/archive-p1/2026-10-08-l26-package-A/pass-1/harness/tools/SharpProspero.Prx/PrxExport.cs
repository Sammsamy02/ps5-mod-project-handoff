// SharpProspero.Prx - module inspection and stub generation.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Prx;

/// <summary>One exported symbol of a module.</summary>
/// <param name="Nid">The 11-character identifier.</param>
/// <param name="LibraryId">The numeric export-library id from the symbol suffix.</param>
/// <param name="ModuleId">The numeric module id from the symbol suffix.</param>
/// <param name="LibraryName">The export-library name, when a matching record was found; otherwise empty.</param>
/// <param name="IsFunction">True when the symbol is a function, false when it is data.</param>
/// <param name="Value">The symbol's virtual address within the module.</param>
public readonly record struct PrxExport(
    string Nid,
    int LibraryId,
    int ModuleId,
    string LibraryName,
    bool IsFunction,
    ulong Value);
