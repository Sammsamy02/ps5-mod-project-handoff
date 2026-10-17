// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.IO;

namespace SharpProspero.Texture;

/// <summary>
/// An image decoded to 8-bit red-green-blue-alpha, one byte per channel in memory order
/// (<c>[R, G, B, A]</c> per pixel), rows top to bottom, tightly packed at <see cref="Width"/> pixels per
/// row. This is the form <see cref="GnfWriter"/> packs into a texture.
/// </summary>
public sealed class DecodedImage
{
    /// <summary>Width in pixels.</summary>
    public int Width { get; }

    /// <summary>Height in pixels.</summary>
    public int Height { get; }

    /// <summary>The pixels, four bytes each (<c>R, G, B, A</c>), <c>Width * Height * 4</c> bytes total.</summary>
    public byte[] Rgba { get; }

    /// <summary>Wraps decoded pixels. <paramref name="rgba"/> must hold <c>width * height * 4</c> bytes.</summary>
    public DecodedImage(int width, int height, byte[] rgba)
    {
        ArgumentNullException.ThrowIfNull(rgba);
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width), "Image dimensions must be positive.");
        if ((long)width * height * 4 != rgba.Length)
            throw new ArgumentException("Pixel buffer size does not match the dimensions.", nameof(rgba));
        Width = width;
        Height = height;
        Rgba = rgba;
    }

    /// <summary>
    /// Decodes an image file, choosing the decoder from the file's signature (PNG, QOI, TGA, or BMP).
    /// </summary>
    /// <exception cref="ImageFormatException">The file is not a supported image, or is malformed.</exception>
    public static DecodedImage Load(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        byte[] bytes = File.ReadAllBytes(path);
        return Decode(bytes);
    }

    /// <summary>
    /// Decodes image bytes, choosing the decoder from the signature. Supports PNG, QOI, TGA, and BMP.
    /// </summary>
    /// <exception cref="ImageFormatException">The bytes are not a supported image, or are malformed.</exception>
    public static DecodedImage Decode(ReadOnlySpan<byte> bytes)
    {
        if (PngDecoder.IsPng(bytes))
            return PngDecoder.Decode(bytes);
        if (QoiImage.IsQoi(bytes))
            return QoiImage.Decode(bytes);
        if (BmpDecoder.IsBmp(bytes))
            return BmpDecoder.Decode(bytes);
        // TGA has no signature; try it last (it validates its own header).
        return TgaDecoder.Decode(bytes);
    }
}

/// <summary>Thrown when an image file cannot be decoded because it is malformed or unsupported.</summary>
/// <remarks>Creates the exception with a message.</remarks>
public sealed class ImageFormatException(string message) : Exception(message)
{
}
