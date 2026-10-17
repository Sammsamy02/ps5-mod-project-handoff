// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;
using System.Buffers.Binary;

namespace SharpProspero.Texture;

/// <summary>
/// Decodes a TGA image to 8-bit red-green-blue-alpha. Handles uncompressed and run-length-encoded true
/// colour (24 or 32 bit) and grayscale (8 bit), with either scanline origin.
/// </summary>
public static class TgaDecoder
{
    /// <summary>Decodes a TGA.</summary>
    /// <exception cref="ImageFormatException">The bytes are not a valid or supported TGA.</exception>
    public static DecodedImage Decode(ReadOnlySpan<byte> bytes)
    {
        if (bytes.Length < 18)
            throw new ImageFormatException("File is too small to be a TGA.");

        int idLength = bytes[0];
        int colorMapType = bytes[1];
        int imageType = bytes[2];
        int width = BinaryPrimitives.ReadUInt16LittleEndian(bytes[12..]);
        int height = BinaryPrimitives.ReadUInt16LittleEndian(bytes[14..]);
        int pixelDepth = bytes[16];
        int descriptor = bytes[17];
        bool topOrigin = (descriptor & 0x20) != 0;
        bool rightOrigin = (descriptor & 0x10) != 0;

        if (colorMapType != 0)
            throw new ImageFormatException("Colour-mapped TGA is not supported.");
        if (width <= 0 || height <= 0)
            throw new ImageFormatException("TGA has invalid dimensions.");
        if ((long)width * height * 4 > int.MaxValue)
            throw new ImageFormatException("TGA is too large to decode.");

        bool rle = imageType is 10 or 11;
        bool trueColor = imageType is 2 or 10;
        bool grayscale = imageType is 3 or 11;
        if (!trueColor && !grayscale)
            throw new ImageFormatException($"Unsupported TGA image type {imageType}.");

        int bpp = pixelDepth / 8;
        if (trueColor && pixelDepth is not (24 or 32))
            throw new ImageFormatException("True-colour TGA must be 24 or 32 bit.");
        if (grayscale && pixelDepth != 8)
            throw new ImageFormatException("Grayscale TGA must be 8 bit.");

        int offset = 18 + idLength; // colour map is absent (type 0)
        if (offset > bytes.Length)
            throw new ImageFormatException("Truncated TGA header.");

        int pixelCount = width * height;
        byte[] pixels = new byte[pixelCount * bpp]; // decoded in the file's channel order (B, G, R[, A] / gray)
        if (rle)
            DecodeRle(bytes[offset..], pixels, bpp, pixelCount);
        else
        {
            if (offset + pixels.Length > bytes.Length)
                throw new ImageFormatException("Truncated TGA pixel data.");
            bytes.Slice(offset, pixels.Length).CopyTo(pixels);
        }

        byte[] rgba = new byte[pixelCount * 4];
        for (int y = 0; y < height; y++)
        {
            int srcRow = topOrigin ? y : height - 1 - y;
            for (int x = 0; x < width; x++)
            {
                int srcCol = rightOrigin ? width - 1 - x : x;
                int s = (srcRow * width + srcCol) * bpp;
                int d = (y * width + x) * 4;
                if (grayscale)
                {
                    byte v = pixels[s];
                    rgba[d] = v; rgba[d + 1] = v; rgba[d + 2] = v; rgba[d + 3] = 0xFF;
                }
                else
                {
                    // TGA stores blue, green, red, then optional alpha.
                    rgba[d] = pixels[s + 2];
                    rgba[d + 1] = pixels[s + 1];
                    rgba[d + 2] = pixels[s];
                    rgba[d + 3] = bpp == 4 ? pixels[s + 3] : (byte)0xFF;
                }
            }
        }
        return new DecodedImage(width, height, rgba);
    }

    private static void DecodeRle(ReadOnlySpan<byte> src, byte[] dst, int bpp, int pixelCount)
    {
        int sp = 0, dp = 0, produced = 0;
        while (produced < pixelCount)
        {
            if (sp >= src.Length)
                throw new ImageFormatException("Truncated run-length TGA data.");
            int packet = src[sp++];
            int count = (packet & 0x7F) + 1;
            if (produced + count > pixelCount)
                throw new ImageFormatException("Run-length TGA overruns the image.");
            if ((packet & 0x80) != 0)
            {
                // Run packet: one pixel repeated.
                if (sp + bpp > src.Length)
                    throw new ImageFormatException("Truncated run-length TGA run.");
                for (int i = 0; i < count; i++)
                    for (int c = 0; c < bpp; c++)
                        dst[dp + i * bpp + c] = src[sp + c];
                sp += bpp;
            }
            else
            {
                // Raw packet: count literal pixels.
                int bytesCount = count * bpp;
                if (sp + bytesCount > src.Length)
                    throw new ImageFormatException("Truncated run-length TGA packet.");
                src.Slice(sp, bytesCount).CopyTo(dst.AsSpan(dp, bytesCount));
                sp += bytesCount;
            }
            dp += count * bpp;
            produced += count;
        }
    }
}
