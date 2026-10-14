// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Interop.Image;

/// <summary>The pixel layout of the image handed to the encoder.</summary>
public enum PngEncPixelFormat : ushort
{
    /// <summary>Bytes run red, green, blue, alpha.</summary>
    Rgba8 = 0,

    /// <summary>Bytes run blue, green, red, alpha. Matches the display surface.</summary>
    Bgra8 = 1,
}

/// <summary>The color content the encoder writes into the PNG.</summary>
public enum PngEncColorSpace : ushort
{
    /// <summary>Red, green and blue, no alpha channel.</summary>
    Rgb = 3,

    /// <summary>Red, green, blue and alpha.</summary>
    Rgba = 19,
}

/// <summary>The row filters the encoder may apply. A bitwise combination is allowed.</summary>
[System.Flags]
public enum PngEncFilterType : ushort
{
    /// <summary>No filtering.</summary>
    None = 0,

    /// <summary>Subtract the pixel to the left.</summary>
    Sub = 1,

    /// <summary>Subtract the pixel above.</summary>
    Up = 2,

    /// <summary>Subtract the average of left and above.</summary>
    Avg = 4,

    /// <summary>The Paeth predictor.</summary>
    Paeth = 8,

    /// <summary>Every filter (the encoder picks the best per row).</summary>
    All = 15,
}

/// <summary>Encoder creation parameters.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePngEncCreateParam
{
    /// <summary>Size of this structure in bytes.</summary>
    public uint ThisSize;

    /// <summary>Reserved attribute flags. Zero.</summary>
    public uint Attribute;

    /// <summary>The widest image the encoder will handle, in pixels.</summary>
    public uint MaxImageWidth;

    /// <summary>The most filters the encoder may weigh at once, 0 to 4.</summary>
    public uint MaxFilterNumber;
}

/// <summary>Encode parameters: the source image, the output buffer, and the target format.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScePngEncEncodeParam
{
    /// <summary>Pointer to the source image pixels.</summary>
    public void* ImageMemAddr;

    /// <summary>Pointer to the output buffer the PNG is written into.</summary>
    public void* PngMemAddr;

    /// <summary>Size of the source image buffer in bytes.</summary>
    public uint ImageMemSize;

    /// <summary>Size of the output buffer in bytes.</summary>
    public uint PngMemSize;

    /// <summary>Image width in pixels.</summary>
    public uint ImageWidth;

    /// <summary>Image height in pixels.</summary>
    public uint ImageHeight;

    /// <summary>Source row pitch in bytes.</summary>
    public uint ImagePitch;

    /// <summary><see cref="PngEncPixelFormat"/>.</summary>
    public ushort PixelFormat;

    /// <summary><see cref="PngEncColorSpace"/>.</summary>
    public ushort ColorSpace;

    /// <summary>Bit depth. Must be 8.</summary>
    public ushort BitDepth;

    /// <summary>Palette entry count. Must be zero.</summary>
    public ushort ClutNumber;

    /// <summary><see cref="PngEncFilterType"/>, as a bitwise combination.</summary>
    public ushort FilterType;

    /// <summary>Compression effort, 0 (fastest) to 9 (smallest).</summary>
    public ushort CompressionLevel;
}

/// <summary>What the encoder produced.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct ScePngEncOutputInfo
{
    /// <summary>The size of the encoded PNG in bytes.</summary>
    public uint DataSize;

    /// <summary>The number of image rows encoded.</summary>
    public uint ProcessedHeight;
}

/// <summary>
/// PNG-encode bindings. The flow is: query and allocate the encoder's work memory, create an encoder,
/// encode an image into an output buffer, then delete the encoder. The module must be loaded
/// (<c>SystemModuleId.PngEnc</c>) before these calls.
/// </summary>
public static unsafe partial class PngEnc
{
    private const string Lib = "libScePngEnc";

    /// <summary>Returns the work-memory size an encoder needs, or a negative error code.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngEncQueryMemorySize(ScePngEncCreateParam* param);

    /// <summary>Creates an encoder over <paramref name="memory"/> and writes the handle.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngEncCreate(ScePngEncCreateParam* param, void* memory, uint memorySize, void** handle);

    /// <summary>Encodes the image described by <paramref name="param"/> into its output buffer.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngEncEncode(void* handle, ScePngEncEncodeParam* param, ScePngEncOutputInfo* info);

    /// <summary>Deletes an encoder.</summary>
    [LibraryImport(Lib)]
    public static partial int scePngEncDelete(void* handle);
}
