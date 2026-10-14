using System.Numerics;
using System.Runtime.InteropServices;

namespace SharpProspero.Graphics;

/// <summary>
/// One mesh vertex: a position, a normal for lighting, a texture coordinate, and a color. The layout is
/// sequential and blittable, so an array of these uploads to a GPU vertex buffer without conversion. The
/// built-in shaders read exactly these fields in this order.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vertex(Vector3 position, Vector3 normal, Vector2 texCoord, Color color)
{
    /// <summary>The position in model space.</summary>
    public Vector3 Position = position;

    /// <summary>The surface normal, used for lighting.</summary>
    public Vector3 Normal = normal;

    /// <summary>The texture coordinate.</summary>
    public Vector2 TexCoord = texCoord;

    /// <summary>The vertex color.</summary>
    public Color Color = color;

    /// <summary>The size of one vertex in bytes: the vertex-buffer stride.</summary>
    public const int SizeInBytes = 3 * 4 + 3 * 4 + 2 * 4 + 4;

    /// <summary>A vertex at a position with a normal, white, at texture origin.</summary>
    public Vertex(Vector3 position, Vector3 normal) : this(position, normal, Vector2.Zero, Color.White) { }
}
