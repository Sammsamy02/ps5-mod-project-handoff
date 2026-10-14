// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Debug;
using SharpProspero.Payload.Elf;
using System;

namespace SharpProspero.Payload.Process;

/// <summary>
/// Reads and writes another process's memory through the mdbg primitive and ptrace.
/// Provides higher-level operations on top of <see cref="PayloadDebug.mdbg_copyout"/>,
/// <see cref="PayloadDebug.mdbg_copyin"/>, and <see cref="PayloadDebug.ptrace"/>.
/// </summary>
public static unsafe class PayloadProcessMemory
{
    /// <summary>
    /// Reads <paramref name="length"/> bytes from address <paramref name="addr"/> in
    /// process <paramref name="pid"/> into <paramref name="buf"/>.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int Read(int pid, nint addr, void* buf, nuint length)
    {
        return PayloadDebug.mdbg_copyout(pid, addr, buf, length);
    }

    /// <summary>
    /// Writes <paramref name="length"/> bytes from <paramref name="buf"/> to address
    /// <paramref name="addr"/> in process <paramref name="pid"/>.
    /// </summary>
    /// <returns>Zero on success, or a negative error code.</returns>
    public static int Write(int pid, void* buf, nint addr, nuint length)
    {
        return PayloadDebug.mdbg_copyin(pid, buf, addr, length);
    }

    /// <summary>
    /// Reads a single 64-bit value from address <paramref name="addr"/> in process
    /// <paramref name="pid"/>.
    /// </summary>
    public static ulong ReadU64(int pid, nint addr)
    {
        ulong value = 0;
        PayloadDebug.mdbg_copyout(pid, addr, &value, 8);
        return value;
    }

    /// <summary>
    /// Reads a single 32-bit value from address <paramref name="addr"/> in process
    /// <paramref name="pid"/>.
    /// </summary>
    public static uint ReadU32(int pid, nint addr)
    {
        uint value = 0;
        PayloadDebug.mdbg_copyout(pid, addr, &value, 4);
        return value;
    }

    /// <summary>
    /// Writes a single 64-bit value to address <paramref name="addr"/> in process
    /// <paramref name="pid"/>.
    /// </summary>
    public static int WriteU64(int pid, nint addr, ulong value)
    {
        return PayloadDebug.mdbg_copyin(pid, &value, addr, 8);
    }

    /// <summary>
    /// Writes a single 32-bit value to address <paramref name="addr"/> in process
    /// <paramref name="pid"/>.
    /// </summary>
    public static int WriteU32(int pid, nint addr, uint value)
    {
        return PayloadDebug.mdbg_copyin(pid, &value, addr, 4);
    }

    /// <summary>
    /// Reads the CPU register set of a stopped process.
    /// </summary>
    /// <returns>Zero on success.</returns>
    public static int GetRegisters(int pid, FreeBsdRegs* regs)
    {
        return PayloadDebug.ptrace(33, pid, regs, 0); // PT_GETREGS = 33
    }

    /// <summary>
    /// Writes the CPU register set of a stopped process.
    /// </summary>
    /// <returns>Zero on success.</returns>
    public static int SetRegisters(int pid, FreeBsdRegs* regs)
    {
        return PayloadDebug.ptrace(34, pid, regs, 0); // PT_SETREGS = 34
    }

    /// <summary>
    /// Reads LWP information after a process stops.
    /// </summary>
    /// <returns>Zero on success.</returns>
    public static int GetLwpInfo(int pid, PtraceLwpinfo* info)
    {
        return PayloadDebug.ptrace(13, pid, info, sizeof(PtraceLwpinfo)); // PT_LWPINFO = 13
    }

    /// <summary>
    /// Attaches to a process for debugging. The process is stopped.
    /// </summary>
    public static int Attach(int pid) => PayloadDebug.ptrace(PayloadDebug.PtAttach, pid, null, 0);

    /// <summary>
    /// Detaches from a process and lets it continue.
    /// </summary>
    public static int Detach(int pid) => PayloadDebug.ptrace(PayloadDebug.PtDetach, pid, null, 0);

    /// <summary>
    /// Continues execution of a stopped process.
    /// </summary>
    public static int Continue(int pid, int signal = 0) =>
        PayloadDebug.ptrace(7, pid, (void*)1, signal); // PT_CONTINUE = 7

    /// <summary>
    /// Executes a single instruction in the stopped process.
    /// </summary>
    public static int Step(int pid) => PayloadDebug.ptrace(9, pid, (void*)1, 0); // PT_STEP = 9

    /// <summary>
    /// Reads process memory using the ptrace PT_IO interface (for processes that cannot
    /// be accessed through the mdbg primitive).
    /// </summary>
    public static int PtraceRead(int pid, nint addr, void* buf, nuint len)
    {
        PtraceIoDesc desc;
        desc.piod_op = PayloadDebug.PiodReadD;
        desc.piod_offs = (void*)addr;
        desc.piod_addr = buf;
        desc.piod_len = len;
        return PayloadDebug.ptrace(PayloadDebug.PtIo, pid, &desc, 0);
    }

    /// <summary>
    /// Writes process memory using the ptrace PT_IO interface.
    /// </summary>
    public static int PtraceWrite(int pid, void* buf, nint addr, nuint len)
    {
        PtraceIoDesc desc;
        desc.piod_op = PayloadDebug.PiodWriteD;
        desc.piod_offs = (void*)addr;
        desc.piod_addr = buf;
        desc.piod_len = len;
        return PayloadDebug.ptrace(PayloadDebug.PtIo, pid, &desc, 0);
    }

    /// <summary>
    /// Scans a memory range in process <paramref name="pid"/> for a byte pattern with
    /// wildcard support. A <c>0xFF</c> byte in <paramref name="mask"/> means the
    /// corresponding pattern byte must match; <c>0x00</c> means any value is accepted.
    /// </summary>
    /// <returns>The address of the first match, or zero if not found.</returns>
    public static nint PatternScan(int pid, nint start, nuint length,
        ReadOnlySpan<byte> pattern, ReadOnlySpan<byte> mask)
    {
        if (pattern.Length != mask.Length || pattern.Length == 0)
            return 0;

        const int ChunkSize = 4096;
        byte* chunk = stackalloc byte[ChunkSize];
        int patLen = pattern.Length;

        for (nuint offset = 0; offset + (nuint)patLen <= length;)
        {
            int readSize = (int)Math.Min(ChunkSize, length - offset);
            if (PayloadDebug.mdbg_copyout(pid, start + (nint)offset, chunk, (nuint)readSize) != 0)
            {
                offset += (nuint)ChunkSize;
                continue;
            }

            for (int i = 0; i <= readSize - patLen; i++)
            {
                bool match = true;
                for (int j = 0; j < patLen; j++)
                {
                    if (mask[j] != 0 && chunk[i + j] != pattern[j])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return start + (nint)(offset + (nuint)i);
            }

            offset += (nuint)(readSize - patLen + 1);
        }

        return 0;
    }
}
