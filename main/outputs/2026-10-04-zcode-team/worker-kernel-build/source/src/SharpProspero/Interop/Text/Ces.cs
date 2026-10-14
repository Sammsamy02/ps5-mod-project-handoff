// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Text;

/// <summary>
/// Character-encoding conversion for the encodings the managed framework's own do not cover.
/// </summary>
/// <remarks>
/// Two things about these differ from what the names suggest. A call converts a single character,
/// taking one to three bytes of the source, and writes how much it took and how much it produced, so
/// converting a string means calling in a loop. And the profile is required: there is no default and a
/// null one is refused, so build a profile once with the matching routine below and pass it to every
/// call. The module is <c>libSceCesCs</c>, and it converts between these encodings and UTF-8 only, so
/// use the framework's own encodings to go on to anything wider.
/// </remarks>
public static unsafe partial class Ces
{
    private const string Lib = "libSceCesCs";







    /// <summary>Converts one EUC-JP (Japanese) character to UTF-8. The profile is required; build it with
    /// <see cref="sceCesUcsProfileInitEucJpCp51932"/> or one of its siblings.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCesEucJpToUtf8(void* profile, byte* eucBuffer, uint eucMax, uint* eucLength, byte* utf8Buffer, uint utf8Max, uint* utf8Length);

    /// <summary>Converts one EUC-KR (Korean) character to UTF-8. The profile is required; build it with
    /// <see cref="sceCesUcsProfileInitEucKr"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCesEucKrToUtf8(void* profile, byte* eucBuffer, uint eucMax, uint* eucLength, byte* utf8Buffer, uint utf8Max, uint* utf8Length);

    /// <summary>Converts one Big5 (Traditional Chinese) character to UTF-8. The profile is required; build it with
    /// <see cref="sceCesUcsProfileInitBig5Cp950"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCesBig5ToUtf8(void* profile, byte* big5Buffer, uint big5Max, uint* big5Length, byte* utf8Buffer, uint utf8Max, uint* utf8Length);

    /// <summary>Converts one UHC (Korean) character to UTF-8. The profile is required; build it with
    /// <see cref="sceCesUcsProfileInitUhc"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceCesUhcToUtf8(void* profile, byte* uhcBuffer, uint uhcMax, uint* uhcLength, byte* utf8Buffer, uint utf8Max, uint* utf8Length);

    /// <summary>
    /// The block a profile is built into. The caller owns it and it has to outlive every conversion
    /// that uses the profile, so it belongs in a field rather than on the frame of the call that builds
    /// it. Two hundred and fifty-six bytes, and its contents belong to the module.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Size = 256)]
    public struct SceCesUcsProfileSheet;

    /// <summary>Builds a profile for EUC-JP with the JIS X 0208 set. Answers the profile to pass on.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitEucJpX0208(SceCesUcsProfileSheet* sheet);

    /// <summary>Builds a profile for EUC-JP with JIS X 0208 and the half-width katakana set.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitEucJpX0208Ss2(SceCesUcsProfileSheet* sheet);

    /// <summary>Builds a profile for EUC-JP with JIS X 0208, half-width katakana and JIS X 0212.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitEucJpX0208Ss2Ss3(SceCesUcsProfileSheet* sheet);

    /// <summary>
    /// Builds a profile for EUC-JP as code page 51932, which is the flavour to use when nothing else
    /// is asked for.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitEucJpCp51932(SceCesUcsProfileSheet* sheet);

    /// <summary>Builds a profile for EUC-KR.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitEucKr(SceCesUcsProfileSheet* sheet);

    /// <summary>Builds a profile for Big5 as code page 950.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitBig5Cp950(SceCesUcsProfileSheet* sheet);

    /// <summary>Builds a profile for UHC.</summary>
    [LibraryImport(Lib)]
    public static partial void* sceCesUcsProfileInitUhc(SceCesUcsProfileSheet* sheet);
}
