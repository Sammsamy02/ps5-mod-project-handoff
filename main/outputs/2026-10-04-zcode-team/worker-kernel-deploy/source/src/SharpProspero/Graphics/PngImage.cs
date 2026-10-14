// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Image;
using System;
using System.Runtime.InteropServices;

namespace SharpProspero.Graphics;

/// <summary>
/// A decoded PNG image held as B8-G8-R8-A8 pixels, the same layout the display surface uses, so it
/// blits straight onto a framebuffer. Decode once, draw its <see cref="AsSurface"/> as often as
/// needed, and dispose it to release the pixels. Load the PNG-decode module
/// (<c>SystemModule.Load(SystemModuleId.PngDec)</c>) before decoding.
/// </summary>
public sealed unsafe class PngImage : IDisposable
{
    private void* _pixels;
    private bool _disposed;

    private PngImage(void* pixels, int width, int height)
    {
        _pixels = pixels;
        Width = width;
        Height = height;
    }

    /// <summary>Width in pixels.</summary>
    public int Width { get; }

    /// <summary>Height in pixels.</summary>
    public int Height { get; }

    /// <summary>Views the decoded pixels as a drawing surface.</summary>
    public Surface AsSurface() => new((uint*)_pixels, Width, Height);

    /// <summary>Decodes <paramref name="png"/> into a B8-G8-R8-A8 image.</summary>
    /// <exception cref="ProsperoException">Parsing, sizing, or decoding failed.</exception>
    public static PngImage Decode(ReadOnlySpan<byte> png)
    {
        if (png.IsEmpty)
            throw new ArgumentException("The PNG data is empty.", nameof(png));

        fixed (byte* source = png)
        {
            ScePngDecImageInfo info = default;
            var parse = new ScePngDecParseParam { PngMemAddr = source, PngMemSize = (uint)png.Length, Reserved0 = 0 };
            SceResult.ThrowIfFailed(PngDec.scePngDecParseHeader(&parse, &info), nameof(PngDec.scePngDecParseHeader));

            // Compute the RGBA byte count in unsigned 64-bit so the product is exact for every
            // header-representable dimension (each is at most 0x7FFFFFFF) and cannot wrap past the check.
            ulong width = info.ImageWidth;
            ulong height = info.ImageHeight;
            ulong imageBytes = width * 4 * height;
            if (width == 0 || height == 0 || imageBytes > uint.MaxValue)
                throw new ProsperoException(nameof(PngDec.scePngDecParseHeader), unchecked((int)0x80690020));

            // How deep the samples are, taken from the header that was just read rather than assumed.
            // The decoder is created for one depth or the other and refuses a file deeper than the one
            // it was created for, so a picture carrying sixteen bits a channel was refused outright.
            // The same description sizes the working buffer, so it grows to match on its own.
            var create = new ScePngDecCreateParam
            {
                ThisSize = (uint)sizeof(ScePngDecCreateParam),
                Attribute = info.BitDepth > 8 ? 1u : 0u,
                MaxImageWidth = info.ImageWidth,
            };
            int workSize = PngDec.scePngDecQueryMemorySize(&create);
            SceResult.ThrowIfFailed(workSize, nameof(PngDec.scePngDecQueryMemorySize));

            void* work = NativeMemory.Alloc((nuint)workSize);
            void* handle = null;
            void* image = null;
            try
            {
                SceResult.ThrowIfFailed(PngDec.scePngDecCreate(&create, work, (uint)workSize, &handle), nameof(PngDec.scePngDecCreate));

                image = NativeMemory.Alloc((nuint)imageBytes);
                ScePngDecImageInfo outInfo = default;
                var decode = new ScePngDecDecodeParam
                {
                    PngMemAddr = source,
                    ImageMemAddr = image,
                    PngMemSize = (uint)png.Length,
                    ImageMemSize = (uint)imageBytes,
                    PixelFormat = (ushort)PngPixelFormat.Bgra8,
                    AlphaValue = 255,
                    ImagePitch = (uint)(width * 4),
                };
                SceResult.ThrowIfFailed(PngDec.scePngDecDecode(handle, &decode, &outInfo), nameof(PngDec.scePngDecDecode));
            }
            catch
            {
                if (image != null)
                    NativeMemory.Free(image);
                if (handle != null)
                    PngDec.scePngDecDelete(handle);
                NativeMemory.Free(work);
                throw;
            }

            PngDec.scePngDecDelete(handle);
            NativeMemory.Free(work);
            return new PngImage(image, (int)width, (int)height);
        }
    }

    /// <summary>Releases the decoded pixels.</summary>
    public void Dispose()
    {
        if (_disposed)
            return;
        _disposed = true;
        if (_pixels != null)
        {
            NativeMemory.Free(_pixels);
            _pixels = null;
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Releases the pixels if the image was dropped without a <see cref="Dispose"/> call.</summary>
    ~PngImage() => Dispose();
}
