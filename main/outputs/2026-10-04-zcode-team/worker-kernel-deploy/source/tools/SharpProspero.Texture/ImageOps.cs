// SharpProspero.Texture - builds GNF texture containers for the console from common image files.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Texture;

/// <summary>
/// Operations on a decoded image before it becomes a texture: resize, crop and flip. Each returns a new
/// <see cref="DecodedImage"/> and leaves the input untouched, so an oversized source is scaled to a
/// texture-friendly size, a region is cut out, or the rows are flipped for a target that expects the
/// opposite origin.
/// </summary>
public static class ImageOps
{
    /// <summary>Resizes the image to <paramref name="width"/> by <paramref name="height"/> with bilinear sampling.</summary>
    public static DecodedImage Resize(DecodedImage image, int width, int height)
    {
        ArgumentNullException.ThrowIfNull(image);
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width), "The target size must be positive.");
        if (width == image.Width && height == image.Height)
            return image;

        int srcW = image.Width, srcH = image.Height;
        byte[] src = image.Rgba;
        byte[] dst = new byte[width * height * 4];
        float scaleX = (float)srcW / width;
        float scaleY = (float)srcH / height;
        for (int y = 0; y < height; y++)
        {
            float sy = (y + 0.5f) * scaleY - 0.5f;
            int y0 = (int)MathF.Floor(sy);
            float fy = sy - y0;
            int y0c = Math.Clamp(y0, 0, srcH - 1);
            int y1c = Math.Clamp(y0 + 1, 0, srcH - 1);
            for (int x = 0; x < width; x++)
            {
                float sx = (x + 0.5f) * scaleX - 0.5f;
                int x0 = (int)MathF.Floor(sx);
                float fx = sx - x0;
                int x0c = Math.Clamp(x0, 0, srcW - 1);
                int x1c = Math.Clamp(x0 + 1, 0, srcW - 1);
                int p00 = (y0c * srcW + x0c) * 4, p10 = (y0c * srcW + x1c) * 4;
                int p01 = (y1c * srcW + x0c) * 4, p11 = (y1c * srcW + x1c) * 4;
                int d = (y * width + x) * 4;
                for (int c = 0; c < 4; c++)
                {
                    float top = src[p00 + c] + (src[p10 + c] - src[p00 + c]) * fx;
                    float bottom = src[p01 + c] + (src[p11 + c] - src[p01 + c]) * fx;
                    dst[d + c] = (byte)Math.Clamp(MathF.Round(top + (bottom - top) * fy), 0, 255);
                }
            }
        }
        return new DecodedImage(width, height, dst);
    }

    /// <summary>Cuts the rectangle at (<paramref name="x"/>, <paramref name="y"/>) of the given size out of the image.</summary>
    public static DecodedImage Crop(DecodedImage image, int x, int y, int width, int height)
    {
        ArgumentNullException.ThrowIfNull(image);
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width), "The region must be positive.");
        if (x < 0 || y < 0 || x + width > image.Width || y + height > image.Height)
            throw new ArgumentOutOfRangeException(nameof(x), "The region lies outside the image.");
        byte[] dst = new byte[width * height * 4];
        for (int row = 0; row < height; row++)
            Array.Copy(image.Rgba, ((y + row) * image.Width + x) * 4, dst, row * width * 4, width * 4);
        return new DecodedImage(width, height, dst);
    }

    /// <summary>Flips the image top to bottom.</summary>
    public static DecodedImage FlipVertical(DecodedImage image)
    {
        ArgumentNullException.ThrowIfNull(image);
        int stride = image.Width * 4;
        byte[] dst = new byte[image.Rgba.Length];
        for (int row = 0; row < image.Height; row++)
            Array.Copy(image.Rgba, row * stride, dst, (image.Height - 1 - row) * stride, stride);
        return new DecodedImage(image.Width, image.Height, dst);
    }

    /// <summary>Flips the image left to right.</summary>
    public static DecodedImage FlipHorizontal(DecodedImage image)
    {
        ArgumentNullException.ThrowIfNull(image);
        int w = image.Width, h = image.Height;
        byte[] dst = new byte[image.Rgba.Length];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                Array.Copy(image.Rgba, (y * w + x) * 4, dst, (y * w + (w - 1 - x)) * 4, 4);
        return new DecodedImage(w, h, dst);
    }
}
