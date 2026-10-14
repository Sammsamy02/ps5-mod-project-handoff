// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// Kernel IDT (Interrupt Descriptor Table) manipulation. Reads and writes IDT gate
/// descriptors for installing kernel hooks.
/// </summary>
public static unsafe class KernelIdt
{
    /// <summary>Size of one IDT gate descriptor (16 bytes on x86-64).</summary>
    public const int GateSize = 16;

    /// <summary>
    /// Reads an IDT gate descriptor and returns its target address.
    /// </summary>
    public static ulong ReadGateTarget(PayloadKernelIo io, ulong idtBase, int vector)
    {
        ulong entry = idtBase + (ulong)(vector * GateSize);
        ushort offsetLo = io.ReadU16(entry);
        ushort offsetMid = io.ReadU16(entry + 6);
        uint offsetHi = io.ReadU32(entry + 8);
        return (ulong)offsetHi << 32 | (ulong)offsetMid << 16 | offsetLo;
    }

    /// <summary>
    /// Writes a new target address to an IDT gate descriptor.
    /// </summary>
    public static void WriteGateTarget(PayloadKernelIo io, ulong idtBase, int vector,
        ulong target)
    {
        ulong entry = idtBase + (ulong)(vector * GateSize);
        io.WriteU16(entry, (ushort)(target & 0xFFFF));
        io.WriteU16(entry + 6, (ushort)((target >> 16) & 0xFFFF));
        io.WriteU32(entry + 8, (uint)(target >> 32));
    }
}
