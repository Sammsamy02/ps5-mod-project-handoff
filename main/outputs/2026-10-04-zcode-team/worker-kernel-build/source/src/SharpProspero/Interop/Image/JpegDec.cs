// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Image;

/// <summary>The pixel layout the JPEG decoder writes.</summary>
public enum JpegPixelFormat : ushort
{
    /// <summary>Bytes run red, green, blue, alpha.</summary>
    Rgba8 = 0,

    /// <summary>Bytes run blue, green, red, alpha. Matches the display surface.</summary>
    Bgra8 = 1,
}

/// <summary>Decoder creation parameters.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceJpegDecCreateParam
{
    /// <summary>Size of this structure in bytes.</summary>
    public uint ThisSize;

    /// <summary>Decoder attributes.</summary>
    public uint Attribute;

    /// <summary>The widest image the decoder will handle, in pixels.</summary>
    public uint MaxImageWidth;
}

/// <summary>Decode parameters: input, output buffer, scratch coefficient buffer, and target format.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceJpegDecDecodeParam
{
    /// <summary>Pointer to the encoded JPEG bytes.</summary>
    public void* JpegMemAddr;

    /// <summary>Pointer to the output image buffer.</summary>
    public void* ImageMemAddr;

    /// <summary>Pointer to a scratch buffer for the transform coefficients, or null.</summary>
    public void* CoefficientMemAddr;

    /// <summary>Length of the encoded JPEG bytes.</summary>
    public uint JpegMemSize;

    /// <summary>Size of the output image buffer in bytes.</summary>
    public uint ImageMemSize;

    /// <summary>Size of the coefficient buffer in bytes.</summary>
    public uint CoefficientMemSize;

    /// <summary>Decode mode. Zero for a normal image.</summary>
    public ushort DecodeMode;

    /// <summary>Downscale factor: 1, 2, 4 or 8.</summary>
    public ushort DownScale;

    /// <summary><see cref="JpegPixelFormat"/>.</summary>
    public ushort PixelFormat;

    /// <summary>Alpha applied to the opaque image, 0 to 255.</summary>
    public ushort AlphaValue;

    /// <summary>Output row pitch in bytes.</summary>
    public uint ImagePitch;
}

/// <summary>Header-parse parameters.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceJpegDecParseParam
{
    /// <summary>Pointer to the encoded JPEG bytes.</summary>
    public void* JpegMemAddr;

    /// <summary>Length of the encoded JPEG bytes.</summary>
    public uint JpegMemSize;

    /// <summary>Decode mode. Zero for a normal image.</summary>
    public ushort DecodeMode;

    /// <summary>Downscale factor: 1, 2, 4 or 8.</summary>
    public ushort DownScale;
}

/// <summary>Image dimensions and format read from the header.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceJpegDecImageInfo
{
    /// <summary>Source width in pixels.</summary>
    public uint ImageWidth;

    /// <summary>Source height in pixels.</summary>
    public uint ImageHeight;

    /// <summary>Source color space.</summary>
    public ushort ColorSpace;

    /// <summary>Number of color components.</summary>
    public ushort ComponentNumber;

    private fixed byte _componentId[4];
    private fixed byte _samplingFactor[4];

    /// <summary>Bytes the coefficient scratch buffer needs for a decode.</summary>
    public uint CoefficientMemSize;

    /// <summary>The color-space conversion attribute the source suits.</summary>
    public uint SuitableCscAttribute;

    /// <summary>Output width in pixels after any downscale.</summary>
    public uint OutputImageWidth;

    /// <summary>Output height in pixels after any downscale.</summary>
    public uint OutputImageHeight;
}

/// <summary>
/// JPEG-decode bindings. The flow mirrors the PNG path but adds a scratch coefficient buffer whose
/// size the header parse reports, and the output dimensions account for the downscale factor. The
/// module must be loaded (<c>SystemModuleId.JpegDec</c>) before these calls.
/// </summary>
public static unsafe partial class JpegDec
{
    private const string Lib = "libSceJpegDec";

    /// <summary>Returns the work-memory size a decoder needs, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegDecQueryMemorySize(SceJpegDecCreateParam* param);

    /// <summary>Creates a decoder over <paramref name="memory"/> and writes the handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegDecCreate(SceJpegDecCreateParam* param, void* memory, uint memorySize, void** handle);

    /// <summary>Decodes the JPEG into the output buffer described by <paramref name="param"/>.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegDecDecode(void* handle, SceJpegDecDecodeParam* param, SceJpegDecImageInfo* info);

    /// <summary>Deletes a decoder.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegDecDelete(void* handle);

    /// <summary>Reads the header into <paramref name="info"/> without decoding.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegDecParseHeader(SceJpegDecParseParam* param, SceJpegDecImageInfo* info);
}
