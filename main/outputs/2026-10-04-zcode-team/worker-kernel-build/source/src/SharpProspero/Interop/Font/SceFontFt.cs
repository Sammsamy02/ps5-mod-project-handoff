// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Font;

/// <summary>
/// The FreeType backend for the font engine (libSceFontFt). Its two selectors produce the library and
/// renderer selections passed to <see cref="SceFont.sceFontCreateLibraryWithEdition"/> and
/// <see cref="SceFont.sceFontCreateRendererWithEdition"/>. Pass 0 for the default.
/// </summary>
public static unsafe partial class SceFontFt
{
    private const string Lib = "libSceFontFt";

    /// <summary>Returns the FreeType library selection for <see cref="SceFont.sceFontCreateLibraryWithEdition"/>.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceFontSelectLibraryFt(int value);

    /// <summary>Returns the FreeType renderer selection for <see cref="SceFont.sceFontCreateRendererWithEdition"/>.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceFontSelectRendererFt(int value);
}
