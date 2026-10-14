// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Image;

/// <summary>The pixel layout of the image handed to the JPEG encoder.</summary>
public enum JpegEncPixelFormat : ushort
{
    /// <summary>Bytes run red, green, blue, alpha.</summary>
    Rgba8 = 0,

    /// <summary>Bytes run blue, green, red, alpha. Matches the display surface.</summary>
    Bgra8 = 1,

    /// <summary>Packed YUV 4:2:2 (Y, U, Y, V).</summary>
    Yuv422 = 10,

    /// <summary>A single grayscale channel.</summary>
    Gray8 = 11,
}

/// <summary>The color content the JPEG carries.</summary>
public enum JpegEncColorSpace : ushort
{
    /// <summary>Luminance and two chroma channels, the format for a color photo.</summary>
    Ycc = 1,

    /// <summary>A single grayscale channel.</summary>
    Grayscale = 2,
}

/// <summary>How the chroma channels are subsampled.</summary>
public enum JpegEncSamplingType : byte
{
    /// <summary>Full resolution, the only choice for grayscale.</summary>
    Full = 0,

    /// <summary>Half horizontal chroma resolution.</summary>
    Sub422 = 1,

    /// <summary>Half horizontal and vertical chroma resolution, the smallest output.</summary>
    Sub420 = 2,
}

/// <summary>The encoding mode.</summary>
public enum JpegEncMode : ushort
{
    /// <summary>A single still image.</summary>
    Normal = 0,

    /// <summary>A frame of motion JPEG.</summary>
    MotionJpeg = 1,
}

/// <summary>Encoder creation parameters.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceJpegEncCreateParam
{
    /// <summary>Size of this structure in bytes.</summary>
    public uint ThisSize;

    /// <summary>Reserved attribute flags. Zero.</summary>
    public uint Attribute;
}

/// <summary>Encode parameters: the source image, the output buffer, and the target format.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct SceJpegEncEncodeParam
{
    /// <summary>Pointer to the source image pixels.</summary>
    public void* ImageMemAddr;

    /// <summary>Pointer to the output buffer the JPEG is written into.</summary>
    public void* JpegMemAddr;

    /// <summary>Size of the source image buffer in bytes.</summary>
    public uint ImageMemSize;

    /// <summary>Size of the output buffer in bytes.</summary>
    public uint JpegMemSize;

    /// <summary>Image width in pixels.</summary>
    public uint ImageWidth;

    /// <summary>Image height in pixels.</summary>
    public uint ImageHeight;

    /// <summary>Source row pitch in bytes.</summary>
    public uint ImagePitch;

    /// <summary><see cref="JpegEncPixelFormat"/>.</summary>
    public ushort PixelFormat;

    /// <summary><see cref="JpegEncMode"/>.</summary>
    public ushort EncodeMode;

    /// <summary><see cref="JpegEncColorSpace"/>.</summary>
    public ushort ColorSpace;

    /// <summary><see cref="JpegEncSamplingType"/>.</summary>
    public byte SamplingType;

    /// <summary>Quality trade-off, 0 (best quality, largest) to 255 (lowest quality, smallest).</summary>
    public byte CompressionRatio;

    /// <summary>Restart interval: 0 for none, -1 per row of blocks, or a positive block count.</summary>
    public int RestartInterval;
}

/// <summary>What the encoder produced.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct SceJpegEncOutputInfo
{
    /// <summary>The size of the encoded JPEG in bytes.</summary>
    public uint DataSize;

    /// <summary>The number of image rows encoded.</summary>
    public uint ProcessedHeight;
}

/// <summary>
/// JPEG-encode bindings. The flow mirrors PNG encoding: query and allocate the encoder's work memory,
/// create an encoder, encode an image into an output buffer, then delete the encoder. The module must
/// be loaded (<c>SystemModuleId.JpegEnc</c>) before these calls.
/// </summary>
public static unsafe partial class JpegEnc
{
    private const string Lib = "libSceJpegEnc";

    /// <summary>Returns the work-memory size an encoder needs, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegEncQueryMemorySize(SceJpegEncCreateParam* param);

    /// <summary>Creates an encoder over <paramref name="memory"/> and writes the handle.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegEncCreate(SceJpegEncCreateParam* param, void* memory, uint memorySize, void** handle);

    /// <summary>Encodes the image described by <paramref name="param"/> into its output buffer.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegEncEncode(void* handle, SceJpegEncEncodeParam* param, SceJpegEncOutputInfo* info);

    /// <summary>Deletes an encoder.</summary>
    [LibraryImport(Lib)]
    public static partial int sceJpegEncDelete(void* handle);
}
