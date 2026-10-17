// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Numerics;

/// <summary>
/// Smooth, repeatable gradient noise for procedural content - terrain heights, cloud and marble textures,
/// organic motion. Sampling the same coordinate always returns the same value, and nearby coordinates
/// return nearby values, so a field is coherent rather than random. A seed picks one of many fields.
/// Values are in the range -1 to 1; <see cref="FractalNoise2D"/> layers octaves for detail.
/// </summary>
public sealed class NoiseField
{
    private readonly byte[] _perm = new byte[512];

    /// <summary>Builds a field from a seed. The same seed always builds the same field.</summary>
    public NoiseField(int seed = 0)
    {
        // A shuffled permutation of 0..255, doubled so lookups never wrap-check.
        var p = new byte[256];
        for (int i = 0; i < 256; i++)
            p[i] = (byte)i;
        uint state = (uint)seed * 747796405u + 2891336453u;
        for (int i = 255; i > 0; i--)
        {
            state = state * 747796405u + 2891336453u;
            int j = (int)((state >> 8) % (uint)(i + 1));
            (p[i], p[j]) = (p[j], p[i]);
        }
        for (int i = 0; i < 512; i++)
            _perm[i] = p[i & 255];
    }

    /// <summary>Samples the field at (<paramref name="x"/>, <paramref name="y"/>), in -1 to 1.</summary>
    public float Noise2D(float x, float y)
    {
        int xi = FastFloor(x) & 255;
        int yi = FastFloor(y) & 255;
        float xf = x - FastFloor(x);
        float yf = y - FastFloor(y);
        float u = Fade(xf);
        float v = Fade(yf);

        int aa = _perm[_perm[xi] + yi];
        int ab = _perm[_perm[xi] + yi + 1];
        int ba = _perm[_perm[xi + 1] + yi];
        int bb = _perm[_perm[xi + 1] + yi + 1];

        float x1 = Lerp(Grad2D(aa, xf, yf), Grad2D(ba, xf - 1, yf), u);
        float x2 = Lerp(Grad2D(ab, xf, yf - 1), Grad2D(bb, xf - 1, yf - 1), u);
        return Lerp(x1, x2, v);
    }

    /// <summary>Samples the 3D field at (<paramref name="x"/>, <paramref name="y"/>, <paramref name="z"/>), in -1 to 1.</summary>
    public float Noise3D(float x, float y, float z)
    {
        int xi = FastFloor(x) & 255;
        int yi = FastFloor(y) & 255;
        int zi = FastFloor(z) & 255;
        float xf = x - FastFloor(x);
        float yf = y - FastFloor(y);
        float zf = z - FastFloor(z);
        float u = Fade(xf);
        float v = Fade(yf);
        float w = Fade(zf);

        int a = _perm[xi] + yi, aa = _perm[a] + zi, ab = _perm[a + 1] + zi;
        int b = _perm[xi + 1] + yi, ba = _perm[b] + zi, bb = _perm[b + 1] + zi;

        float x1 = Lerp(Grad3D(_perm[aa], xf, yf, zf), Grad3D(_perm[ba], xf - 1, yf, zf), u);
        float x2 = Lerp(Grad3D(_perm[ab], xf, yf - 1, zf), Grad3D(_perm[bb], xf - 1, yf - 1, zf), u);
        float y1 = Lerp(x1, x2, v);
        x1 = Lerp(Grad3D(_perm[aa + 1], xf, yf, zf - 1), Grad3D(_perm[ba + 1], xf - 1, yf, zf - 1), u);
        x2 = Lerp(Grad3D(_perm[ab + 1], xf, yf - 1, zf - 1), Grad3D(_perm[bb + 1], xf - 1, yf - 1, zf - 1), u);
        float y2 = Lerp(x1, x2, v);
        return Lerp(y1, y2, w);
    }

    /// <summary>
    /// Layers <paramref name="octaves"/> of 2D noise at rising frequency and falling amplitude for detail
    /// (fractal Brownian motion). <paramref name="persistence"/> (0 to 1) sets how fast the amplitude
    /// falls; <paramref name="lacunarity"/> (typically 2) sets how fast the frequency rises. The result is
    /// normalised to -1 to 1.
    /// </summary>
    public float FractalNoise2D(float x, float y, int octaves = 4, float persistence = 0.5f, float lacunarity = 2f)
    {
        if (octaves < 1)
            octaves = 1;
        float sum = 0, amplitude = 1, frequency = 1, total = 0;
        for (int o = 0; o < octaves; o++)
        {
            sum += Noise2D(x * frequency, y * frequency) * amplitude;
            total += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }
        return total > 0 ? sum / total : 0;
    }

    private static int FastFloor(float value) => value >= 0 ? (int)value : (int)value - 1;

    private static float Fade(float t) => t * t * t * (t * (t * 6 - 15) + 10);

    private static float Lerp(float a, float b, float t) => a + t * (b - a);

    private static float Grad2D(int hash, float x, float y)
    {
        // One of eight directions towards the edges and corners of a square.
        return (hash & 3) switch
        {
            0 => x + y,
            1 => -x + y,
            2 => x - y,
            _ => -x - y,
        } * 0.70710678f; // scale so the diagonal gradients match the axis ones in length
    }

    private static float Grad3D(int hash, float x, float y, float z)
    {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h is 12 or 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
}
