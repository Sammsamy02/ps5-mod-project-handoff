// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Image;
using SharpProspero.Storage;
using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Graphics;

/// <summary>
/// Encodes a drawing surface to PNG bytes, for a screenshot or for exporting a picture. The surface is
/// the display's B8-G8-R8-A8 format, so a framebuffer encodes directly. Load the PNG-encode module
/// (<c>SystemModule.Load(SystemModuleId.PngEnc)</c>) before encoding.
/// </summary>
public static unsafe class PngEncoder
{
    /// <summary>
    /// Encodes <paramref name="surface"/> to the bytes of a PNG file.
    /// </summary>
    /// <param name="surface">The pixels to encode, in the display's B8-G8-R8-A8 format.</param>
    /// <param name="compressionLevel">Compression effort, 0 (fastest) to 9 (smallest). Default 6.</param>
    /// <exception cref="ProsperoException">Sizing, creating the encoder, or encoding failed.</exception>
    public static byte[] Encode(Surface surface, int compressionLevel = 6)
    {
        if (surface.Width <= 0 || surface.Height <= 0)
            throw new ArgumentException("The surface has no pixels to encode.", nameof(surface));
        int level = Math.Clamp(compressionLevel, 0, 9);

        var create = new ScePngEncCreateParam
        {
            ThisSize = (uint)sizeof(ScePngEncCreateParam),
            Attribute = 0,
            MaxImageWidth = (uint)surface.Width,
            MaxFilterNumber = 4,
        };
        int workSize = PngEnc.scePngEncQueryMemorySize(&create);
        SceResult.ThrowIfFailed(workSize, nameof(PngEnc.scePngEncQueryMemorySize));

        // The source rows step by the surface stride in bytes (four per pixel); the buffer spans every
        // row. Compute in 64-bit so the products are exact and cannot wrap the size check.
        ulong pitch = (ulong)surface.Stride * 4;
        ulong imageBytes = pitch * (ulong)surface.Height;
        // A generous upper bound for the encoded output: an incompressible image can grow past its raw
        // size once row filters and the container are added.
        ulong pngCapacity = imageBytes + (imageBytes >> 1) + 0x10000;
        if (imageBytes > uint.MaxValue || pngCapacity > uint.MaxValue)
            throw new ProsperoException("The image is too large to encode.", unchecked((int)0x80690103));

        void* work = NativeMemory.Alloc((nuint)workSize);
        void* handle = null;
        void* pngBuffer = null;
        try
        {
            SceResult.ThrowIfFailed(PngEnc.scePngEncCreate(&create, work, (uint)workSize, &handle), nameof(PngEnc.scePngEncCreate));

            pngBuffer = NativeMemory.Alloc((nuint)pngCapacity);
            var encode = new ScePngEncEncodeParam
            {
                ImageMemAddr = surface.Pixels,
                PngMemAddr = pngBuffer,
                ImageMemSize = (uint)imageBytes,
                PngMemSize = (uint)pngCapacity,
                ImageWidth = (uint)surface.Width,
                ImageHeight = (uint)surface.Height,
                ImagePitch = (uint)pitch,
                PixelFormat = (ushort)PngEncPixelFormat.Bgra8,
                ColorSpace = (ushort)PngEncColorSpace.Rgba,
                BitDepth = 8,
                ClutNumber = 0,
                FilterType = (ushort)PngEncFilterType.All,
                CompressionLevel = (ushort)level,
            };
            ScePngEncOutputInfo info = default;
            SceResult.ThrowIfFailed(PngEnc.scePngEncEncode(handle, &encode, &info), nameof(PngEnc.scePngEncEncode));

            var output = new byte[info.DataSize];
            new ReadOnlySpan<byte>(pngBuffer, (int)info.DataSize).CopyTo(output);
            return output;
        }
        finally
        {
            if (pngBuffer != null)
                NativeMemory.Free(pngBuffer);
            if (handle != null)
                PngEnc.scePngEncDelete(handle);
            NativeMemory.Free(work);
        }
    }

    /// <summary>
    /// Encodes <paramref name="surface"/> and writes it to the file at <paramref name="path"/>, for
    /// saving a screenshot (for example <c>/data/screenshot.png</c>).
    /// </summary>
    /// <exception cref="ProsperoException">Encoding or writing the file failed.</exception>
    public static void Save(Surface surface, string path, int compressionLevel = 6)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        FileSystem.WriteAllBytes(path, Encode(surface, compressionLevel));
    }
}
