// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Agc;
using System;

namespace SharpProspero.Graphics.Agc;

/// <summary>The kind of resource a shader reads, used to find its slot in the shader's user data.</summary>
public enum ShaderResourceKind
{
    /// <summary>A read-only buffer or texture (a structured/regular buffer, a sampled texture).</summary>
    ReadOnly = 0,
    /// <summary>A read-write buffer or texture.</summary>
    ReadWrite = 1,
    /// <summary>A sampler.</summary>
    Sampler = 2,
    /// <summary>A constant buffer.</summary>
    ConstantBuffer = 3,
}

/// <summary>Which part of the pipeline a shader runs at.</summary>
public enum ShaderKind : byte
{
    /// <summary>Compute shader.</summary>
    Compute = 0,
    /// <summary>Pixel (fragment) shader.</summary>
    Pixel = 1,
    /// <summary>Geometry shader (or a fused vertex-plus-geometry shader).</summary>
    Geometry = 2,
    /// <summary>Hull shader (a fused pair of hull halves).</summary>
    Hull = 3,
    /// <summary>The front half of a geometry shader, to be fused with a back half.</summary>
    GeometryFront = 4,
    /// <summary>The front half of a hull shader, to be fused with a back half.</summary>
    HullFront = 5,
    /// <summary>The back half of a geometry shader, to be fused with a front half.</summary>
    GeometryBack = 6,
    /// <summary>The back half of a hull shader, to be fused with a front half.</summary>
    HullBack = 7,
    /// <summary>Function shader.</summary>
    Function = 8,
}

/// <summary>
/// A shader created from a compiled shader binary. A shader binary is produced ahead of time by the
/// shader compiler and is a header section plus a code section; <see cref="Create"/> reads the header,
/// prepares the shader in place, and returns a handle the command buffer binds. The header and code
/// memory must stay alive - and the code must live in GPU-readable memory - for as long as the shader
/// is in use.
/// </summary>
public sealed unsafe class AgcShader
{
    private AgcShader(void* handle) => Handle = handle;

    /// <summary>The shader handle, as the register-binding and context calls expect it.</summary>
    public void* Handle { get; }

    // The handle points at the prepared shader-program header. Its register arrays and its user-data
    // layout are read here to build a draw. Field offsets are those of the shader-binary program header.
    private byte* Header => (byte*)Handle;

    /// <summary>
    /// The context registers the shader sets, as offset-and-value pairs the command buffer loads. These
    /// go into the draw's combined context state. The records share the layout of <see cref="CxRegister"/>.
    /// </summary>
    public ReadOnlySpan<CxRegister> ContextRegisters
    {
        get { var p = *(CxRegister**)(Header + 24); return new ReadOnlySpan<CxRegister>(p, Header[91]); }
    }

    /// <summary>The shader registers the program sets, as offset-and-value pairs the command buffer loads.</summary>
    public ReadOnlySpan<CxRegister> ShaderRegisters
    {
        get { var p = *(CxRegister**)(Header + 32); return new ReadOnlySpan<CxRegister>(p, Header[92]); }
    }

    /// <summary>
    /// Finds where in the shader's user data a resource of a kind and slot goes: the dword offset the
    /// descriptor is written at, and whether it is a small (four-dword) descriptor. Returns false when the
    /// shader declares no such resource.
    /// </summary>
    public bool TryGetResourceSlot(ShaderResourceKind kind, int slot, out int dwordOffset, out bool small)
    {
        dwordOffset = 0; small = true;
        byte* userData = *(byte**)(Header + 8);        // m_userData (UserDataLayout*)
        if (userData is null) return false;
        ushort* counts = (ushort*)(userData + 46);     // m_sharpResourceCount[4]
        if ((uint)slot >= counts[(int)kind]) return false;
        ushort* sharps = *(ushort**)(userData + 8 + (int)kind * sizeof(void*)); // m_sharpResourceOffset[kind]
        ushort entry = sharps[slot];                   // Sharp: offsetInDwords:15, small:1
        dwordOffset = entry & 0x7FFF;
        small = (entry & 0x8000) != 0;
        return true;
    }

    /// <summary>The base shader-register offset for this shader kind's user-data slot zero.</summary>
    public const uint GsUserDataBaseOffset = 0x008C;

    /// <summary>
    /// Prepares a shader from its compiled binary: <paramref name="header"/> is the binary's header
    /// section (writable, and where the shader is prepared), and <paramref name="gpuCode"/> is the code
    /// section in GPU-readable memory. Both must outlive the shader.
    /// </summary>
    /// <exception cref="ProsperoException">The shader could not be created.</exception>
    public static AgcShader Create(void* header, void* gpuCode)
    {
        void* handle = null;
        SceResult.ThrowIfFailed(
            SceAgc.sceAgcCreateShader(&handle, header, gpuCode),
            nameof(SceAgc.sceAgcCreateShader));
        return new AgcShader(handle);
    }

    /// <summary>
    /// Fuses a front shader half and a back shader half (the two-part geometry or hull pairs) into one
    /// shader written to <paramref name="fusedStorage"/>, using <paramref name="scratch"/> as working
    /// memory. Storage must be at least the size the fused-size query reports.
    /// </summary>
    /// <exception cref="ProsperoException">The halves could not be fused.</exception>
    public static AgcShader Fuse(AgcShader front, AgcShader back, void* fusedStorage, void* scratch)
    {
        SceResult.ThrowIfFailed(
            SceAgc.sceAgcFuseShaderHalves(fusedStorage, front.Handle, back.Handle, scratch),
            nameof(SceAgc.sceAgcFuseShaderHalves));
        return new AgcShader(fusedStorage);
    }
}
