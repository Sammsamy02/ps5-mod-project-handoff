// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Process;
using System;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// x86-64 inline function detour. Calculates a relative jump from the hook point to
/// a trampoline, backing up the original bytes for later restoration.
/// </summary>
public static unsafe class PayloadDetour
{
    /// <summary>Minimum size of the detour jump instruction (5 bytes for JMP rel32).</summary>
    public const int MinDetourSize = 5;

    /// <summary>
    /// Installs a detour at <paramref name="target"/> that redirects to
    /// <paramref name="hook"/>. The original bytes are saved in
    /// <paramref name="backup"/>.
    /// </summary>
    /// <param name="pid">Target process PID.</param>
    /// <param name="target">Address to hook in the target process.</param>
    /// <param name="hook">Address of the hook function.</param>
    /// <param name="backup">Buffer to save the original bytes (at least 14 bytes).</param>
    /// <param name="backupSize">Receives the number of bytes backed up.</param>
    /// <returns><see langword="true"/> on success.</returns>
    public static bool Install(int pid, nint target, nint hook,
        Span<byte> backup, out int backupSize)
    {
        backupSize = 14; // Full 14-byte absolute jump
        if (backup.Length < 14) return false;

        // Save original bytes.
        fixed (byte* p = backup)
        {
            if (PayloadProcessMemory.Read(pid, target, p, 14) != 0)
                return false;
        }

        // Write a 14-byte absolute jump: FF 25 00 00 00 00 [8-byte address]
        byte* jmp = stackalloc byte[14];
        jmp[0] = 0xFF;
        jmp[1] = 0x25;
        jmp[2] = 0x00; jmp[3] = 0x00; jmp[4] = 0x00; jmp[5] = 0x00;
        *(long*)(jmp + 6) = (long)hook;

        return PayloadProcessMemory.Write(pid, jmp, target, 14) == 0;
    }

    /// <summary>
    /// Removes a detour by restoring the original bytes.
    /// </summary>
    public static bool Remove(int pid, nint target, ReadOnlySpan<byte> backup, int size)
    {
        fixed (byte* p = backup)
            return PayloadProcessMemory.Write(pid, p, target, (nuint)size) == 0;
    }
}
