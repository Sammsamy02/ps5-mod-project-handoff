// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;

namespace SharpProspero.Graphics;

// In-place image effects over a surface's pixels. Each one reads and rewrites the pixels the surface
// points at, so they apply to a decoded image, an off-screen surface, or the back buffer alike. The
// alpha channel is preserved; only the colour is changed unless noted.
public readonly unsafe partial struct Surface
{
    /// <summary>Inverts the red, green and blue of every pixel, keeping its alpha.</summary>
    public void Invert()
    {
        for (int y = 0; y < Height; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            for (int x = 0; x < Width; x++)
            {
                uint p = row[x];
                row[x] = (p & 0xFF000000u) | (~p & 0x00FFFFFFu);
            }
        }
    }

    /// <summary>Converts every pixel to grey by its perceived luminance, keeping its alpha.</summary>
    public void ToGrayscale()
    {
        for (int y = 0; y < Height; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            for (int x = 0; x < Width; x++)
            {
                uint p = row[x];
                uint r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
                // Rec. 601 luma in fixed point: (77 r + 150 g + 29 b) / 256.
                uint l = (77 * r + 150 * g + 29 * b + 128) >> 8;
                if (l > 255) l = 255;
                row[x] = (p & 0xFF000000u) | (l << 16) | (l << 8) | l;
            }
        }
    }

    /// <summary>Adds <paramref name="delta"/> to each colour channel (negative darkens), clamped.</summary>
    public void AdjustBrightness(int delta)
    {
        for (int y = 0; y < Height; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            for (int x = 0; x < Width; x++)
            {
                uint p = row[x];
                byte r = ClampByte((int)((p >> 16) & 0xFF) + delta);
                byte g = ClampByte((int)((p >> 8) & 0xFF) + delta);
                byte b = ClampByte((int)(p & 0xFF) + delta);
                row[x] = (p & 0xFF000000u) | ((uint)r << 16) | ((uint)g << 8) | b;
            }
        }
    }

    /// <summary>
    /// Scales each colour channel around mid-grey by <paramref name="factor"/> (1 leaves it unchanged,
    /// above 1 raises contrast, below 1 lowers it), clamped.
    /// </summary>
    public void AdjustContrast(float factor)
    {
        if (factor < 0f)
            factor = 0f;
        for (int y = 0; y < Height; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            for (int x = 0; x < Width; x++)
            {
                uint p = row[x];
                byte r = ClampByte((int)MathF.Round((((p >> 16) & 0xFF) - 128f) * factor + 128f));
                byte g = ClampByte((int)MathF.Round((((p >> 8) & 0xFF) - 128f) * factor + 128f));
                byte b = ClampByte((int)MathF.Round(((p & 0xFF) - 128f) * factor + 128f));
                row[x] = (p & 0xFF000000u) | ((uint)r << 16) | ((uint)g << 8) | b;
            }
        }
    }

    /// <summary>
    /// Blends every pixel towards <paramref name="color"/> by <paramref name="amount"/> (0 leaves it
    /// unchanged, 1 replaces the colour), keeping its alpha. For a wash of colour over an image.
    /// </summary>
    public void Tint(Color color, float amount)
    {
        amount = Math.Clamp(amount, 0f, 1f);
        int keep = (int)((1f - amount) * 256f + 0.5f);
        int add = 256 - keep;
        int tr = color.R, tg = color.G, tb = color.B;
        for (int y = 0; y < Height; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            for (int x = 0; x < Width; x++)
            {
                uint p = row[x];
                byte r = (byte)((((int)((p >> 16) & 0xFF)) * keep + tr * add) >> 8);
                byte g = (byte)((((int)((p >> 8) & 0xFF)) * keep + tg * add) >> 8);
                byte b = (byte)((((int)(p & 0xFF)) * keep + tb * add) >> 8);
                row[x] = (p & 0xFF000000u) | ((uint)r << 16) | ((uint)g << 8) | b;
            }
        }
    }

    /// <summary>Mirrors the image left to right.</summary>
    public void FlipHorizontal()
    {
        for (int y = 0; y < Height; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            for (int x = 0, e = Width - 1; x < e; x++, e--)
                (row[x], row[e]) = (row[e], row[x]);
        }
    }

    /// <summary>Mirrors the image top to bottom.</summary>
    public void FlipVertical()
    {
        for (int y = 0, e = Height - 1; y < e; y++, e--)
        {
            uint* top = _pixels + (long)y * Stride;
            uint* bottom = _pixels + (long)e * Stride;
            for (int x = 0; x < Width; x++)
                (top[x], bottom[x]) = (bottom[x], top[x]);
        }
    }

    /// <summary>
    /// Blurs the image with a box filter of the given <paramref name="radius"/> in pixels (a larger
    /// radius blurs more; zero does nothing). Alpha is preserved. This allocates a working copy the size
    /// of the image.
    /// </summary>
    public void BoxBlur(int radius)
    {
        if (radius <= 0 || Width == 0 || Height == 0)
            return;

        int w = Width, h = Height;
        uint[] scratch = new uint[w * h];

        // Horizontal pass: surface -> scratch.
        for (int y = 0; y < h; y++)
        {
            uint* row = _pixels + (long)y * Stride;
            int baseIndex = y * w;
            for (int x = 0; x < w; x++)
            {
                int x0 = Math.Max(0, x - radius), x1 = Math.Min(w - 1, x + radius);
                int rs = 0, gs = 0, bs = 0, count = x1 - x0 + 1;
                uint alpha = row[x] & 0xFF000000u;
                for (int sx = x0; sx <= x1; sx++)
                {
                    uint p = row[sx];
                    rs += (int)((p >> 16) & 0xFF);
                    gs += (int)((p >> 8) & 0xFF);
                    bs += (int)(p & 0xFF);
                }
                scratch[baseIndex + x] = alpha
                    | ((uint)(rs / count) << 16) | ((uint)(gs / count) << 8) | (uint)(bs / count);
            }
        }

        // Vertical pass: scratch -> surface.
        for (int x = 0; x < w; x++)
        {
            for (int y = 0; y < h; y++)
            {
                int y0 = Math.Max(0, y - radius), y1 = Math.Min(h - 1, y + radius);
                int rs = 0, gs = 0, bs = 0, count = y1 - y0 + 1;
                uint alpha = scratch[y * w + x] & 0xFF000000u;
                for (int sy = y0; sy <= y1; sy++)
                {
                    uint p = scratch[sy * w + x];
                    rs += (int)((p >> 16) & 0xFF);
                    gs += (int)((p >> 8) & 0xFF);
                    bs += (int)(p & 0xFF);
                }
                _pixels[(long)y * Stride + x] = alpha
                    | ((uint)(rs / count) << 16) | ((uint)(gs / count) << 8) | (uint)(bs / count);
            }
        }
    }

    private static byte ClampByte(int value) => (byte)(value < 0 ? 0 : value > 255 ? 255 : value);
}
