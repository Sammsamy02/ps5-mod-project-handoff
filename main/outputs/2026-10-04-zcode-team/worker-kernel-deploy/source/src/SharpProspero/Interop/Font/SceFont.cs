// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Font;

/// <summary>
/// The block of memory the font engine manages for a library and renderer. The engine sub-allocates
/// from the block given to <see cref="SceFont.sceFontMemoryInit"/>, so no allocation callbacks are
/// needed; the caller sets it up once and the engine fills it.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 64)]
public unsafe struct SceFontMemory
{
    /// <summary>Engine type tag, set by the init call.</summary>
    public ushort Type;

    /// <summary>Attribute flags.</summary>
    public ushort Attr;

    /// <summary>The size of the managed block in bytes.</summary>
    public uint Size;

    /// <summary>The base of the managed block.</summary>
    public void* Address;

    /// <summary>The engine's internal allocator object over the block.</summary>
    public void* MspaceObject;

    /// <summary>The optional memory-interface callbacks; null to let the engine manage the block itself.</summary>
    public void* MemInterface;

    /// <summary>An optional destroy callback.</summary>
    public void* DestroyCallback;

    /// <summary>The destroy callback's object.</summary>
    public void* DestroyObject;

    /// <summary>A caller-owned pointer.</summary>
    public void* UserObject;

    /// <summary>The parent object.</summary>
    public void* ParentObject;
}

/// <summary>How a font file or memory image is opened.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceFontOpenDetail
{
    /// <summary>The detail identifier. 0x0FD2.</summary>
    public ushort DetailId;

    /// <summary>Reserved. Zero.</summary>
    public ushort Reserved;

    /// <summary>Open flags.</summary>
    public uint Flags;

    /// <summary>The sub-font index for a font collection.</summary>
    public uint SubFontIndex;

    /// <summary>A caller-chosen id, or -1.</summary>
    public int UniqueId;

    /// <summary>Reserved. Null.</summary>
    public void* Reserved2;

    /// <summary>Reserved. Null.</summary>
    public void* Reserved1;
}

/// <summary>The target a glyph renders into: its pixels, geometry and clipping.</summary>
[StructLayout(LayoutKind.Sequential, Size = 128)]
public unsafe struct SceFontRenderSurface
{
    /// <summary>The pixel buffer.</summary>
    public void* Buffer;

    /// <summary>The row pitch in bytes.</summary>
    public int WidthByte;

    /// <summary>Bytes per pixel. The next field aligns to offset 16, matching the engine's layout.</summary>
    public sbyte PixelSizeByte;

    /// <summary>Width in pixels.</summary>
    public int Width;

    /// <summary>Height in pixels.</summary>
    public int Height;
}

/// <summary>The metrics of one glyph, in pixels for the current scale.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceFontGlyphMetrics
{
    /// <summary>Glyph width.</summary>
    public float Width;

    /// <summary>Glyph height.</summary>
    public float Height;

    /// <summary>Horizontal left bearing.</summary>
    public float HorizontalBearingX;

    /// <summary>Horizontal top bearing.</summary>
    public float HorizontalBearingY;

    /// <summary>Horizontal advance.</summary>
    public float HorizontalAdvance;

    /// <summary>Vertical left bearing.</summary>
    public float VerticalBearingX;

    /// <summary>Vertical top bearing.</summary>
    public float VerticalBearingY;

    /// <summary>Vertical advance.</summary>
    public float VerticalAdvance;
}

/// <summary>The 8-bit antialiased coverage image the renderer produces for a glyph.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceFontTransImage
{
    /// <summary>The coverage bytes, one per pixel (0 transparent, 255 solid).</summary>
    public byte* Address;

    /// <summary>The row pitch in bytes.</summary>
    public uint WidthByte;

    /// <summary>The image width in pixels.</summary>
    public uint ImageWidth;

    /// <summary>The image height in pixels.</summary>
    public uint ImageHeight;
}

/// <summary>Where a rendered glyph landed in the target surface.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceFontSurfaceImage
{
    /// <summary>The target address the glyph was written to.</summary>
    public byte* Address;

    /// <summary>The row pitch in bytes.</summary>
    public uint WidthByte;

    /// <summary>Bytes per pixel.</summary>
    public byte PixelSizeByte;

    /// <summary>The pixel format.</summary>
    public byte PixelFormat;
}

/// <summary>The pixel placement and advance of a rendered glyph image.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceFontGlyphImageMetrics
{
    /// <summary>Left bearing from the pen, in pixels.</summary>
    public float BearingX;

    /// <summary>Top bearing above the baseline, in pixels.</summary>
    public float BearingY;

    /// <summary>Pen advance, in pixels.</summary>
    public float Advance;

    /// <summary>Row stride.</summary>
    public float Stride;

    /// <summary>Image width in pixels.</summary>
    public uint Width;

    /// <summary>Image height in pixels.</summary>
    public uint Height;
}

/// <summary>How a line of a font stacks: where its baseline sits and how tall a line is.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceFontHorizontalLayout
{
    /// <summary>How far below the top of a line its baseline sits, in pixels.</summary>
    public float BaseLineY;

    /// <summary>The distance from one line to the next, in pixels.</summary>
    public float LineHeight;

    /// <summary>How much taller a line becomes once the font's effects are applied.</summary>
    public float EffectHeight;
}

/// <summary>How a glyph pair is nudged together, in pixels for the scale it was read at.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceFontKerning
{
    /// <summary>Horizontal adjustment to the advance between the two glyphs.</summary>
    public float OffsetX;

    /// <summary>Vertical adjustment to the advance between the two glyphs.</summary>
    public float OffsetY;

    /// <summary>Horizontal adjustment to the second glyph's placement.</summary>
    public float PositionX;

    /// <summary>Vertical adjustment to the second glyph's placement.</summary>
    public float PositionY;
}

/// <summary>What a glyph render produced: where it wrote, the region it changed, and the metrics.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceFontRenderResult
{
    /// <summary>
    /// Always null. The render draws into the surface it was handed and reports where through
    /// <see cref="SurfaceImage"/> and the changed region below; nothing ever fills this in.
    /// </summary>
    public SceFontTransImage* TransImage;

    /// <summary>The write into the target surface.</summary>
    public SceFontSurfaceImage SurfaceImage;

    /// <summary>The changed region's left edge in the surface.</summary>
    public uint UpdateX;

    /// <summary>The changed region's top edge.</summary>
    public uint UpdateY;

    /// <summary>The changed region's width.</summary>
    public uint UpdateW;

    /// <summary>The changed region's height.</summary>
    public uint UpdateH;

    /// <summary>The rendered glyph's placement and advance.</summary>
    public SceFontGlyphImageMetrics ImageMetrics;
}
/// <summary>
/// The routines the font engine allocates and releases through. The engine calls these rather than
/// managing a block itself, and a library or renderer refuses to be created unless at least the first
/// two are present.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceFontMemoryInterface
{
    /// <summary>Allocate, taking the caller's object and a size.</summary>
    public delegate* unmanaged[Cdecl]<void*, uint, void*> Malloc;

    /// <summary>Release, taking the caller's object and a block.</summary>
    public delegate* unmanaged[Cdecl]<void*, void*, void> Free;

    /// <summary>Resize, taking the caller's object, a block and a size.</summary>
    public delegate* unmanaged[Cdecl]<void*, void*, uint, void*> Realloc;

    /// <summary>Allocate and clear, taking the caller's object, a count and a size.</summary>
    public delegate* unmanaged[Cdecl]<void*, uint, uint, void*> Calloc;

    /// <summary>Optional; left unset when the engine is not asked to manage a sub-pool.</summary>
    public void* MspaceCreate;

    /// <summary>Optional; the counterpart to <see cref="MspaceCreate"/>.</summary>
    public void* MspaceDestroy;
}


/// <summary>
/// Font-engine bindings (libSceFont). The engine loads a scalable font, scales it, and renders each
/// glyph to an antialiased coverage image. The flow is: set up a memory block, create a renderer and a
/// library (both from the FreeType backend, see <see cref="SceFontFt"/>), enable external fonts, open a
/// font from memory, bind the renderer, set the pixel size, then render glyphs. Load the module
/// (<c>SystemModuleId.Font</c> and <c>SystemModuleId.FontFt</c>) before these calls.
/// </summary>
public static unsafe partial class SceFont
{
    private const string Lib = "libSceFont";

    /// <summary>The engine edition the library and renderer are created with.</summary>
    public const ulong Edition = 0x0700100000000000UL;

    /// <summary>OpenType (auto). Combine with the TrueType and CFF flavors to accept both.</summary>
    public const uint FormatOpenType = 0x0052;

    /// <summary>OpenType with TrueType outlines.</summary>
    public const uint FormatOpenTypeTt = 0x0050;


    /// <summary>OpenType with CFF outlines.</summary>
    public const uint FormatOpenTypeCff = 0x0042;

    /// <summary>
    /// Prepares the memory the engine works in. The engine keeps whatever it is given here and hands it
    /// on: a library or a renderer created against it reads the routines out of it and refuses to be
    /// created at all unless at least the two that allocate and release are there. Passing none is
    /// accepted here and fails later, at the creation, which is what makes it worth stating.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceFontMemoryInit(SceFontMemory* fontMemory, void* address, uint sizeByte,
        SceFontMemoryInterface* memInterface, void* mspaceObject, void* destroyCallback, void* destroyObject);

    /// <summary>Releases a memory block prepared with <see cref="sceFontMemoryInit"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontMemoryTerm(SceFontMemory* fontMemory);

    /// <summary>Creates the font library over a memory block and a backend selection.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontCreateLibraryWithEdition(SceFontMemory* memory, void* selection, ulong edition, void** pLibrary);

    /// <summary>Destroys a font library.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontDestroyLibrary(void** pLibrary);

    /// <summary>Enables opening external fonts of the given formats, up to <paramref name="fontMax"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontSupportExternalFonts(void* library, uint fontMax, uint formats);

    /// <summary>Opens a font held in memory and writes its handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontOpenFontMemory(void* library, void* fontAddress, uint fontSize,
        SceFontOpenDetail* detail, void** pFontHandle);

    /// <summary>Closes a font handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontCloseFont(void* fontHandle);

    /// <summary>Creates the glyph renderer over a memory block and a backend selection.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontCreateRendererWithEdition(SceFontMemory* memory, void* selection, ulong edition, void** pRenderer);

    /// <summary>Destroys a glyph renderer.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontDestroyRenderer(void** pRenderer);

    /// <summary>Binds a renderer to a font handle so its glyphs can be rendered.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontBindRenderer(void* fontHandle, void* renderer);

    /// <summary>Unbinds the renderer from a font handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontUnbindRenderer(void* fontHandle);

    /// <summary>
    /// Sets the render pixel size for a font handle. A font handle carries two independent scales: this
    /// one drives the glyph rasterizer, and the one <see cref="sceFontSetScalePixel"/> writes drives the
    /// layout queries. Setting one leaves the other where the open left it, so both are set together.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceFontSetupRenderScalePixel(void* fontHandle, float w, float h);

    /// <summary>
    /// Sets the pixel size the layout queries answer at, which is what
    /// <see cref="sceFontGetHorizontalLayout"/> reads.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceFontSetScalePixel(void* fontHandle, float w, float h);

    /// <summary>Reads back the layout scale in pixels.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontGetScalePixel(void* fontHandle, float* w, float* h);

    /// <summary>
    /// Sets the layout scale in points. Points are converted through the resolution
    /// <see cref="sceFontSetResolutionDpi"/> set, so that call comes first.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceFontSetScalePoint(void* fontHandle, float w, float h);

    /// <summary>Sets the dots per inch a point size is resolved against.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontSetResolutionDpi(void* fontHandle, uint hDpi, uint vDpi);

    /// <summary>
    /// Reads the kerning between two characters at the render scale, so it lines up with the advances
    /// <see cref="sceFontGetRenderCharGlyphMetrics"/> reports.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int sceFontGetRenderScaledKerning(void* fontHandle, uint preCode, uint code,
        SceFontKerning* kerning);

    /// <summary>Reads a character's glyph metrics at the current scale.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontGetRenderCharGlyphMetrics(void* fontHandle, uint code, SceFontGlyphMetrics* metrics);

    /// <summary>Renders a character's glyph for horizontal text into <paramref name="surf"/> at (x, y).</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontRenderCharGlyphImageHorizontal(void* fontHandle, uint code,
        SceFontRenderSurface* surf, float x, float y, SceFontGlyphMetrics* metrics, SceFontRenderResult* result);

    /// <summary>Initializes a render surface over a pixel buffer.</summary>
    [LibraryImport(Lib)]
    public static partial void sceFontRenderSurfaceInit(SceFontRenderSurface* surf, void* buffer, int bufWidthByte, int pixelSizeByte, int w, int h);

    /// <summary>Reads how a line of this font stacks: where its baseline sits and how tall a line is.</summary>
    [LibraryImport(Lib)]
    public static partial int sceFontGetHorizontalLayout(void* fontHandle, SceFontHorizontalLayout* layout);

    /// <summary>Sets the clip rectangle a render surface draws within.</summary>
    [LibraryImport(Lib)]
    public static partial void sceFontRenderSurfaceSetScissor(SceFontRenderSurface* surf, int x0, int y0, uint w, uint h);
}
