// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop.Kernel;
using SharpProspero.Memory;
using System;
using System.Buffers.Binary;

namespace SharpProspero.Graphics.Agc;

/// <summary>
/// A compiled shader binary, ready to prepare into a runnable shader. The shader compiler writes the
/// program as a container with two parts: a header block that describes the program (its magic, version,
/// the register values it needs, and the sizes of its pieces) and a code block of graphics-processor
/// microcode. This type reads those two parts out of the container so both can be placed in
/// graphics-readable memory, where the header is then prepared.
/// </summary>
public sealed class ShaderBinary
{
    /// <summary>The magic value at the start of a shader header block.</summary>
    public const uint HeaderMagic = 0x34333231;

    private readonly byte[] _header;
    private readonly byte[] _code;

    private ShaderBinary(byte[] header, byte[] code)
    {
        _header = header;
        _code = code;
    }

    /// <summary>The magic at the start of the header (equals <see cref="HeaderMagic"/> for a valid binary).</summary>
    public uint Magic => BinaryPrimitives.ReadUInt32LittleEndian(_header);

    /// <summary>The shader-binary format version the header declares.</summary>
    public uint Version => BinaryPrimitives.ReadUInt32LittleEndian(_header.AsSpan(4));

    /// <summary>The header block, describing the program. Copied into graphics-readable memory to prepare.</summary>
    public ReadOnlySpan<byte> Header => _header;

    /// <summary>The microcode block, which must be placed in graphics-readable memory to run.</summary>
    public ReadOnlySpan<byte> Code => _code;

    /// <summary>The raw program-type byte from the header (pixel, vertex, compute, and so on).</summary>
    public byte ProgramType => _header[90];

    /// <summary>The number of context registers the program sets.</summary>
    public int ContextRegisterCount => _header[91];

    /// <summary>The number of shader registers the program sets.</summary>
    public int ShaderRegisterCount => _header[92];

    /// <summary>
    /// Reads a shader binary from a compiled container. The container holds the header in a
    /// <c>.shader_header</c> section and the microcode in a <c>.shader_text</c> section; both are copied
    /// out so the caller owns them.
    /// </summary>
    /// <exception cref="ArgumentException">The container is not a valid shader binary.</exception>
    public static ShaderBinary Load(ReadOnlySpan<byte> container)
    {
        if (container.Length < 64 || container[0] != 0x7f || container[1] != (byte)'E' ||
            container[2] != (byte)'L' || container[3] != (byte)'F')
            throw new ArgumentException("Not a shader-binary container.", nameof(container));

        ulong shoff = BinaryPrimitives.ReadUInt64LittleEndian(container[40..]);
        ushort shentsize = BinaryPrimitives.ReadUInt16LittleEndian(container[58..]);
        ushort shnum = BinaryPrimitives.ReadUInt16LittleEndian(container[60..]);
        ushort shstrndx = BinaryPrimitives.ReadUInt16LittleEndian(container[62..]);
        if (shoff == 0 || shnum == 0 || shstrndx >= shnum)
            throw new ArgumentException("Shader-binary container has no section table.", nameof(container));

        // The section-name string table locates each section by name.
        ulong strOff = SectionOffset(container, shoff, shentsize, shstrndx, out _);

        byte[]? header = null, code = null;
        for (int i = 0; i < shnum; i++)
        {
            int rec = (int)shoff + i * shentsize;
            uint nameIndex = BinaryPrimitives.ReadUInt32LittleEndian(container[rec..]);
            ulong off = BinaryPrimitives.ReadUInt64LittleEndian(container[(rec + 24)..]);
            ulong size = BinaryPrimitives.ReadUInt64LittleEndian(container[(rec + 32)..]);
            string name = ReadString(container, (int)strOff + (int)nameIndex);
            if (name == ".shader_header") header = container.Slice((int)off, (int)size).ToArray();
            else if (name == ".shader_text") code = container.Slice((int)off, (int)size).ToArray();
        }

        if (header is null || code is null)
            throw new ArgumentException("Shader-binary container is missing its header or code section.", nameof(container));
        if (header.Length < 96 || BinaryPrimitives.ReadUInt32LittleEndian(header) != HeaderMagic)
            throw new ArgumentException("Shader-binary header is malformed.", nameof(container));
        return new ShaderBinary(header, code);
    }

    /// <summary>
    /// Places both the header and the microcode in graphics-readable memory, prepares the header there,
    /// and creates a runnable shader. The returned object owns both regions; dispose it when the shader
    /// is no longer used.
    /// </summary>
    public unsafe PreparedShader Prepare()
    {
        // Both blocks carry data the graphics processor reads while the shader runs, so both have to sit
        // in memory it can reach. The managed heap is mapped for this program alone, so a header left
        // there is out of the processor's reach however firmly it is pinned. The regions stay writable
        // from here as well, which is what lets the header be prepared in place.
        // The header asks for eight-byte alignment, so the smallest reservation the kernel grants is
        // more than enough; the code asks for far more and keeps the larger default.
        var headerRegion = DirectMemoryRegion.Allocate((nuint)_header.Length, KernelMemory.PageSize);
        try
        {
            var codeRegion = DirectMemoryRegion.Allocate((nuint)_code.Length);
            try
            {
                _header.AsSpan().CopyTo(new Span<byte>(headerRegion.Pointer, _header.Length));
                _code.AsSpan().CopyTo(new Span<byte>(codeRegion.Pointer, _code.Length));
                AgcShader shader = AgcShader.Create(headerRegion.Pointer, codeRegion.Pointer);
                return new PreparedShader(shader, headerRegion, codeRegion, this);
            }
            catch
            {
                codeRegion.Dispose();
                throw;
            }
        }
        catch
        {
            headerRegion.Dispose();
            throw;
        }
    }

    private static ulong SectionOffset(ReadOnlySpan<byte> data, ulong shoff, ushort entsize, int index, out ulong size)
    {
        int rec = (int)shoff + index * entsize;
        size = BinaryPrimitives.ReadUInt64LittleEndian(data[(rec + 32)..]);
        return BinaryPrimitives.ReadUInt64LittleEndian(data[(rec + 24)..]);
    }

    private static string ReadString(ReadOnlySpan<byte> data, int at)
    {
        int end = at;
        while (end < data.Length && data[end] != 0) end++;
        return System.Text.Encoding.ASCII.GetString(data[at..end]);
    }
}

/// <summary>
/// A runnable shader and the memory it depends on: the prepared header and the code, both in
/// graphics-readable memory. The renderer binds <see cref="Shader"/> when drawing. Dispose it after the
/// shader is no longer used by any in-flight frame.
/// </summary>
public sealed class PreparedShader : IDisposable
{
    private readonly ShaderBinary _binary;
    private DirectMemoryRegion? _headerRegion;
    private DirectMemoryRegion? _codeRegion;
    private bool _disposed;

    internal PreparedShader(AgcShader shader, DirectMemoryRegion headerRegion, DirectMemoryRegion codeRegion, ShaderBinary binary)
    {
        Shader = shader;
        _headerRegion = headerRegion;
        _codeRegion = codeRegion;
        _binary = binary;
    }

    /// <summary>The prepared shader the command buffer binds.</summary>
    public AgcShader Shader { get; }

    /// <summary>The number of context registers the program sets.</summary>
    public int ContextRegisterCount => _binary.ContextRegisterCount;

    /// <summary>The number of shader registers the program sets.</summary>
    public int ShaderRegisterCount => _binary.ShaderRegisterCount;

    /// <summary>Releases the header and code regions.</summary>
    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _codeRegion?.Dispose();
        _codeRegion = null;
        _headerRegion?.Dispose();
        _headerRegion = null;
    }
}
