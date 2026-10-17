// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Elf;
using SharpProspero.Payload.Kernel;
using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Process;

/// <summary>
/// Mono runtime type definitions for interacting with processes that embed the Mono
/// virtual machine. These structs match the in-memory layout of Mono's internal
/// metadata structures, enabling inspection and manipulation of managed types,
/// methods, and assemblies in a target process through
/// <see cref="PayloadProcessMemory"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoImage
{
    /// <summary>Reference count.</summary>
    public int RefCount;

    private nint _rawDataHandle;

    /// <summary>Raw data pointer.</summary>
    public byte* RawData;

    /// <summary>Raw data length.</summary>
    public uint RawDataLen;

    private fixed byte _pad1[8];

    /// <summary>Image name.</summary>
    public byte* Name;

    /// <summary>Assembly name.</summary>
    public byte* AssemblyName;

    /// <summary>Module name.</summary>
    public byte* ModuleName;
}

/// <summary>Mono assembly structure.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoAssembly
{
    /// <summary>Reference count.</summary>
    public int RefCount;

    private nint _pad0;

    /// <summary>Base directory.</summary>
    public byte* BaseDir;

    /// <summary>Assembly name (MonoAssemblyName inline).</summary>
    public nint AName;

    /// <summary>Image for this assembly.</summary>
    public MonoImage* Image;
}

/// <summary>Mono class structure.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoClass
{
    /// <summary>Element class (for arrays).</summary>
    public MonoClass* ElementClass;

    /// <summary>Cast class (for generics).</summary>
    public MonoClass* CastClass;

    /// <summary>Supertype array.</summary>
    public MonoClass** Supertypes;

    /// <summary>Interface count.</summary>
    public ushort InterfaceCount;

    /// <summary>Interface offsets count.</summary>
    public ushort InterfaceOffsetsCount;

    /// <summary>IDX (various flags packed).</summary>
    public byte IdxSlot;

    /// <summary>Minimum alignment.</summary>
    public byte MinAlign;

    /// <summary>Packing size.</summary>
    public byte PackingSize;

    /// <summary>State flags.</summary>
    public byte StateFlags;
}

/// <summary>Mono method structure.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoMethod
{
    /// <summary>Method flags.</summary>
    public ushort Flags;

    /// <summary>Implementation flags.</summary>
    public ushort ImplFlags;

    /// <summary>Token.</summary>
    public uint Token;

    /// <summary>Declaring class.</summary>
    public MonoClass* Klass;

    /// <summary>Method signature.</summary>
    public nint Signature;

    /// <summary>Method name.</summary>
    public byte* Name;
}

/// <summary>Mono VTable (virtual method table) structure.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoVTable
{
    /// <summary>Class this VTable belongs to.</summary>
    public MonoClass* Klass;

    /// <summary>GC descriptor.</summary>
    public nint GcDescr;

    /// <summary>Domain.</summary>
    public nint Domain;

    /// <summary>Type.</summary>
    public nint Type;

    /// <summary>Interface bitmap.</summary>
    public byte* InterfaceBitmap;

    /// <summary>Max interface ID.</summary>
    public uint MaxInterfaceId;

    /// <summary>Rank.</summary>
    public byte Rank;

    /// <summary>Initialized flag.</summary>
    public byte Initialized;

    private ushort _pad;

    /// <summary>IDX static fields.</summary>
    public uint IdxStaticFields;
}

/// <summary>Mono domain structure.</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct MonoDomain
{
    /// <summary>Domain ID.</summary>
    public int DomainId;

    private fixed byte _pad[12];

    /// <summary>Setup information.</summary>
    public nint Setup;

    /// <summary>Friendly name.</summary>
    public byte* FriendlyName;
}

/// <summary>
/// Mono runtime function resolution for interacting with processes embedding the
/// Mono virtual machine. Resolves Mono API functions by NID from the loaded Mono
/// runtime module.
/// </summary>
public static unsafe class PayloadMonoInterop
{
    /// <summary>
    /// Finds the Mono runtime image in a target process by looking for
    /// <c>libmonosgen-2.0.sprx</c> in the loaded module list.
    /// </summary>
    /// <returns>The module base address, or zero if not found.</returns>
    public static ulong FindMonoRuntime(PayloadKernelIo io, ulong proc)
    {
        return PayloadHijacker.FindModuleBase(io, proc,
            "libmonosgen-2.0.sprx\0"u8, 0x3A8);
    }

    /// <summary>
    /// Resolves a Mono API function address by name in the target process.
    /// </summary>
    public static ulong ResolveMonoFunction(PayloadKernelIo io, ulong monoBase,
        string functionName, int pid)
    {
        ulong nid = PayloadNid.ComputeRaw(System.Text.Encoding.UTF8.GetBytes(functionName));
        return PayloadHijacker.ResolveByNid(io, monoBase, nid, pid);
    }
}
