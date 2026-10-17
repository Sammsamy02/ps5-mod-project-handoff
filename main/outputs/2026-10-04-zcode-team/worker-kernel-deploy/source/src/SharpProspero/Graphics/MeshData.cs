using SharpProspero.Numerics;
using System;
using System.Collections.Generic;
using System.Numerics;

namespace SharpProspero.Graphics;

/// <summary>
/// A mesh held in system memory: a list of vertices and the indices that join them into triangles. This
/// is the CPU-side shape, produced by hand or by one of the primitive builders; a renderer uploads it to
/// GPU memory to draw. Indices are 32-bit, so a mesh can exceed 65,535 vertices.
/// </summary>
/// <remarks>Creates a mesh from a vertex and index list.</remarks>
public sealed class MeshData(Vertex[] vertices, uint[] indices)
{
    /// <summary>The vertices.</summary>
    public Vertex[] Vertices { get; set; } = vertices;

    /// <summary>Three indices per triangle, referencing <see cref="Vertices"/>.</summary>
    public uint[] Indices { get; set; } = indices;

    /// <summary>The number of triangles.</summary>
    public int TriangleCount => Indices.Length / 3;

    /// <summary>The axis-aligned box around every vertex.</summary>
    public BoundingBox Bounds()
    {
        var points = new Vector3[Vertices.Length];
        for (int i = 0; i < Vertices.Length; i++) points[i] = Vertices[i].Position;
        return BoundingBox.FromPoints(points);
    }

    /// <summary>
    /// Recomputes each vertex normal by averaging the face normals of the triangles that share it, giving
    /// a smooth-shaded surface. Call after building or deforming geometry by hand.
    /// </summary>
    public void RecalculateNormals()
    {
        for (int i = 0; i < Vertices.Length; i++) Vertices[i].Normal = Vector3.Zero;
        for (int i = 0; i + 2 < Indices.Length; i += 3)
        {
            uint ia = Indices[i], ib = Indices[i + 1], ic = Indices[i + 2];
            Vector3 a = Vertices[ia].Position, b = Vertices[ib].Position, c = Vertices[ic].Position;
            Vector3 faceNormal = Vector3.Cross(b - a, c - a);
            Vertices[ia].Normal += faceNormal;
            Vertices[ib].Normal += faceNormal;
            Vertices[ic].Normal += faceNormal;
        }
        for (int i = 0; i < Vertices.Length; i++)
        {
            Vector3 n = Vertices[i].Normal;
            Vertices[i].Normal = n.LengthSquared() > 1e-12f ? Vector3.Normalize(n) : Vector3.UnitY;
        }
    }

    /// <summary>A flat unit quad in the XY plane, facing +Z, spanning -0.5..0.5.</summary>
    public static MeshData Quad(Color color = default)
    {
        Color c = color.Value == 0 ? Color.White : color;
        Vertex[] v =
        [
            new(new(-0.5f, -0.5f, 0), Vector3.UnitZ, new(0, 1), c),
            new(new( 0.5f, -0.5f, 0), Vector3.UnitZ, new(1, 1), c),
            new(new( 0.5f,  0.5f, 0), Vector3.UnitZ, new(1, 0), c),
            new(new(-0.5f,  0.5f, 0), Vector3.UnitZ, new(0, 0), c),
        ];
        return new MeshData(v, [0, 1, 2, 0, 2, 3]);
    }

    /// <summary>A flat plane in the XZ plane, facing +Y, of the given size and subdivisions per side.</summary>
    public static MeshData Plane(float size = 1f, int subdivisions = 1, Color color = default)
    {
        Color c = color.Value == 0 ? Color.White : color;
        int n = Math.Max(1, subdivisions);
        var verts = new List<Vertex>((n + 1) * (n + 1));
        var idx = new List<uint>(n * n * 6);
        float half = size * 0.5f;
        for (int z = 0; z <= n; z++)
            for (int x = 0; x <= n; x++)
            {
                float fx = (float)x / n, fz = (float)z / n;
                verts.Add(new Vertex(new(fx * size - half, 0, fz * size - half), Vector3.UnitY, new(fx, fz), c));
            }
        for (int z = 0; z < n; z++)
            for (int x = 0; x < n; x++)
            {
                uint i0 = (uint)(z * (n + 1) + x);
                uint i1 = i0 + 1;
                uint i2 = (uint)((z + 1) * (n + 1) + x);
                uint i3 = i2 + 1;
                idx.Add(i0); idx.Add(i2); idx.Add(i1);
                idx.Add(i1); idx.Add(i2); idx.Add(i3);
            }
        return new MeshData([.. verts], [.. idx]);
    }

    /// <summary>A unit cube centered on the origin, spanning -0.5..0.5, with per-face normals and UVs.</summary>
    public static MeshData Cube(float size = 1f, Color color = default)
    {
        Color c = color.Value == 0 ? Color.White : color;
        float h = size * 0.5f;
        var verts = new List<Vertex>(24);
        var idx = new List<uint>(36);
        // Six faces, each a quad with its own outward normal so edges stay crisp.
        Span<(Vector3 normal, Vector3 uAxis, Vector3 vAxis)> faces =
        [
            (Vector3.UnitZ, Vector3.UnitX, Vector3.UnitY),
            (-Vector3.UnitZ, -Vector3.UnitX, Vector3.UnitY),
            (Vector3.UnitX, -Vector3.UnitZ, Vector3.UnitY),
            (-Vector3.UnitX, Vector3.UnitZ, Vector3.UnitY),
            (Vector3.UnitY, Vector3.UnitX, -Vector3.UnitZ),
            (-Vector3.UnitY, Vector3.UnitX, Vector3.UnitZ),
        ];
        foreach ((Vector3 normal, Vector3 uAxis, Vector3 vAxis) in faces)
        {
            uint b = (uint)verts.Count;
            Vector3 center = normal * h;
            verts.Add(new Vertex(center - uAxis * h - vAxis * h, normal, new(0, 1), c));
            verts.Add(new Vertex(center + uAxis * h - vAxis * h, normal, new(1, 1), c));
            verts.Add(new Vertex(center + uAxis * h + vAxis * h, normal, new(1, 0), c));
            verts.Add(new Vertex(center - uAxis * h + vAxis * h, normal, new(0, 0), c));
            idx.Add(b); idx.Add(b + 1); idx.Add(b + 2);
            idx.Add(b); idx.Add(b + 2); idx.Add(b + 3);
        }
        return new MeshData([.. verts], [.. idx]);
    }

    /// <summary>A UV sphere of the given radius, with <paramref name="rings"/> latitude bands and <paramref name="segments"/> longitude columns.</summary>
    public static MeshData Sphere(float radius = 0.5f, int rings = 16, int segments = 24, Color color = default)
    {
        Color c = color.Value == 0 ? Color.White : color;
        rings = Math.Max(2, rings);
        segments = Math.Max(3, segments);
        var verts = new List<Vertex>((rings + 1) * (segments + 1));
        var idx = new List<uint>(rings * segments * 6);
        for (int y = 0; y <= rings; y++)
        {
            float v = (float)y / rings;
            float phi = v * MathF.PI;              // 0 at the north pole to PI at the south
            float sinPhi = MathF.Sin(phi), cosPhi = MathF.Cos(phi);
            for (int x = 0; x <= segments; x++)
            {
                float u = (float)x / segments;
                float theta = u * MathF.Tau;
                Vector3 n = new(sinPhi * MathF.Cos(theta), cosPhi, sinPhi * MathF.Sin(theta));
                verts.Add(new Vertex(n * radius, n, new(u, v), c));
            }
        }
        int stride = segments + 1;
        for (int y = 0; y < rings; y++)
            for (int x = 0; x < segments; x++)
            {
                uint i0 = (uint)(y * stride + x);
                uint i1 = i0 + 1;
                uint i2 = (uint)((y + 1) * stride + x);
                uint i3 = i2 + 1;
                // Wound so the faces look outward. The order these were in turned every one of them
                // inward: measured over a sphere of eight rings and twelve segments, all 168 faces
                // that are not degenerate faced inward and none outward. It shows as lighting from the
                // wrong side, and as the sphere disappearing the moment anything asks for back faces
                // to be dropped, which is what makes it easy to miss.
                idx.Add(i0); idx.Add(i1); idx.Add(i2);
                idx.Add(i1); idx.Add(i3); idx.Add(i2);
            }
        return new MeshData([.. verts], [.. idx]);
    }
}
