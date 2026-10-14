// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Image;

/// <summary>The pixel layout the decoder writes.</summary>
public enum PngPixelFormat : ushort
{
    /// <summary>Bytes run red, green, blue, alpha.</summary>
    Rgba8 = 0,

    /// <summary>Bytes run blue, green, red, alpha. Matches the display surface.</summary>
    Bgra8 = 1,
}

/// <summary>Decoder creation parameters.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePngDecCreateParam
{
    /// <summary>Size of this structure in bytes.</summary>
    public uint ThisSize;

    /// <summary>Zero for images up to 8-bit depth, one for up to 16-bit.</summary>
    public uint Attribute;

    /// <summary>The widest image the decoder will handle, in pixels.</summary>
    public uint MaxImageWidth;
}

/// <summary>Decode parameters: the encoded input, the output buffer, and the target format.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScePngDecDecodeParam
{
    /// <summary>Pointer to the encoded PNG bytes.</summary>
    public void* PngMemAddr;

    /// <summary>Pointer to the output image buffer.</summary>
    public void* ImageMemAddr;

    /// <summary>Length of the encoded PNG bytes.</summary>
    public uint PngMemSize;

    /// <summary>Size of the output image buffer in bytes.</summary>
    public uint ImageMemSize;

    /// <summary><see cref="PngPixelFormat"/>.</summary>
    public ushort PixelFormat;

    /// <summary>Alpha applied where the image has none, 0 to 255.</summary>
    public ushort AlphaValue;

    /// <summary>Output row pitch in bytes.</summary>
    public uint ImagePitch;
}

/// <summary>Header-parse parameters.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScePngDecParseParam
{
    /// <summary>Pointer to the encoded PNG bytes.</summary>
    public void* PngMemAddr;

    /// <summary>Length of the encoded PNG bytes.</summary>
    public uint PngMemSize;

    /// <summary>Reserved. Must be zero.</summary>
    public uint Reserved0;
}

/// <summary>Image dimensions and format read from the header.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePngDecImageInfo
{
    /// <summary>Width in pixels.</summary>
    public uint ImageWidth;

    /// <summary>Height in pixels.</summary>
    public uint ImageHeight;

    /// <summary>Source color space.</summary>
    public ushort ColorSpace;

    /// <summary>Source bit depth.</summary>
    public ushort BitDepth;

    /// <summary>Image flags.</summary>
    public uint ImageFlag;
}

/// <summary>
/// PNG-decode bindings. The flow is: parse the header for the dimensions, query and allocate the
/// decoder's work memory, create a decoder, decode into an output buffer, then delete the decoder.
/// The module must be loaded (<c>SystemModuleId.PngDec</c>) before these calls.
/// </summary>
public static unsafe partial class PngDec
{
    private const string Lib = "libScePngDec";

    /// <summary>Returns the work-memory size a decoder needs, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngDecQueryMemorySize(ScePngDecCreateParam* param);

    /// <summary>Creates a decoder over <paramref name="memory"/> and writes the handle.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngDecCreate(ScePngDecCreateParam* param, void* memory, uint memorySize, void** handle);

    /// <summary>Decodes the PNG into the output buffer described by <paramref name="param"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngDecDecode(void* handle, ScePngDecDecodeParam* param, ScePngDecImageInfo* info);

    /// <summary>Deletes a decoder.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngDecDelete(void* handle);

    /// <summary>Reads the header into <paramref name="info"/> without decoding.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngDecParseHeader(ScePngDecParseParam* param, ScePngDecImageInfo* info);
}
