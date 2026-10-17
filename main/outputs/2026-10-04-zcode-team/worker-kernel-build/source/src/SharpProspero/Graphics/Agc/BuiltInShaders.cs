// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System;
using System.IO;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// The mesh shaders that ship with the SDK, compiled ahead of time and embedded so a 3D application
/// draws without any shader tooling of its own. The vertex program reads a vertex from a structured
/// buffer indexed by the vertex id, transforms it by a model-view-projection matrix from a constant
/// buffer, and passes the world normal and colour on; the pixel program applies a fixed directional
/// light. The renderer loads these to draw a <see cref="MeshData"/>.
/// </summary>
public static class BuiltInShaders
{
    /// <summary>The built-in mesh vertex program.</summary>
    public static ShaderBinary MeshVertex() => Load("SharpProspero.Shaders.mesh_vs.sb");

    /// <summary>The built-in mesh pixel program.</summary>
    public static ShaderBinary MeshPixel() => Load("SharpProspero.Shaders.mesh_ps.sb");

    private static ShaderBinary Load(string resourceName)
    {
        using Stream stream = typeof(BuiltInShaders).Assembly.GetManifestResourceStream(resourceName)
            ?? throw new InvalidOperationException($"The embedded shader '{resourceName}' was not found.");
        byte[] bytes = new byte[stream.Length];
        stream.ReadExactly(bytes);
        return ShaderBinary.Load(bytes);
    }
}
