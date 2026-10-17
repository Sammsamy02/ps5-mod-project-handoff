// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Elf;

/// <summary>
/// FreeBSD <c>struct __reg64</c> — CPU register set for ptrace PT_GETREGS/PT_SETREGS.
/// Layout matches FreeBSD x86_64 <c>struct __reg64</c> (26 fields, 176 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct FreeBsdRegs
{
    /// <summary>General-purpose registers in FreeBSD struct reg order.</summary>
    public long R15, R14, R13, R12, R11, R10, R9, R8;

    /// <summary>Destination index.</summary>
    public long Rdi;

    /// <summary>Source index.</summary>
    public long Rsi;

    /// <summary>Frame pointer.</summary>
    public long Rbp;

    /// <summary>Base register.</summary>
    public long Rbx;

    /// <summary>Data register.</summary>
    public long Rdx;

    /// <summary>Counter register.</summary>
    public long Rcx;

    /// <summary>Accumulator.</summary>
    public long Rax;

    /// <summary>Trap number.</summary>
    public uint Trapno;

    /// <summary>FS segment register.</summary>
    public ushort Fs;

    /// <summary>GS segment register.</summary>
    public ushort Gs;

    /// <summary>Error code.</summary>
    public uint Err;

    /// <summary>ES segment register.</summary>
    public ushort Es;

    /// <summary>DS segment register.</summary>
    public ushort Ds;

    /// <summary>Instruction pointer.</summary>
    public long Rip;

    /// <summary>Code segment.</summary>
    public long Cs;

    /// <summary>Flags register.</summary>
    public long Rflags;

    /// <summary>Stack pointer.</summary>
    public long Rsp;

    /// <summary>Stack segment.</summary>
    public long Ss;
}

/// <summary>
/// FreeBSD <c>struct ptrace_lwpinfo</c> — LWP information from ptrace PT_LWPINFO.
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct PtraceLwpinfo
{
    /// <summary>LWP identifier.</summary>
    public int LwpId;

    /// <summary>Event that caused the stop.</summary>
    public int Event;

    /// <summary>LWP flags (PL_FLAG_FORKED, PL_FLAG_EXEC, etc.).</summary>
    public int Flags;

    /// <summary>Signal mask (pl_sigmask, sigset_t = 4 x uint32).</summary>
    public fixed uint Sigmask[4];

    /// <summary>Pending signal list (pl_siglist, sigset_t = 4 x uint32).</summary>
    public fixed uint Siglist[4];

    /// <summary>Signal information (pl_siginfo).</summary>
    public int Siginfo;
}
