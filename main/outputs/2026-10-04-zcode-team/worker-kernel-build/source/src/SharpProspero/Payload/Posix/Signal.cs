// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Posix;

/// <summary>
/// FreeBSD <c>sigset_t</c> — a signal set for <see cref="FreeBsdSigaction.Mask"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdSigset
{
    /// <summary>Bit array of signal flags (4 x 32-bit words = 128 signals).</summary>
    public fixed uint Bits[4];
}

/// <summary>
/// FreeBSD <c>struct sigaction</c> for <see cref="PayloadSignal.sigaction"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdSigaction
{
    /// <summary>Signal handler function pointer (<c>sa_handler</c> or <c>sa_sigaction</c>).</summary>
    public void* Handler;

    /// <summary>Signal action flags (SA_SIGINFO, SA_RESTART, SA_RESETHAND, etc.).</summary>
    public int Flags;

    /// <summary>Signals to block during handler execution.</summary>
    public FreeBsdSigset Mask;
}

/// <summary>
/// FreeBSD <c>jmp_buf</c> for <see cref="PayloadSignal.setjmp"/> and
/// <see cref="PayloadSignal.longjmp"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdJmpBuf
{
    /// <summary>Saved register context (callee-saved regs + rip + rsp + signal mask).</summary>
    public fixed long Regs[12];
}

/// <summary>
/// FreeBSD <c>struct timeval</c> for <see cref="PayloadSignal.select"/>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct FreeBsdTimeval
{
    /// <summary>Seconds.</summary>
    public long Sec;

    /// <summary>Microseconds.</summary>
    public long Usec;
}

/// <summary>
/// FreeBSD <c>fd_set</c> for <see cref="PayloadSignal.select"/>.
/// FD_SETSIZE = 1024, stored as 16 unsigned longs.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct FreeBsdFdSet
{
    /// <summary>Bit array of file descriptors.</summary>
    public fixed ulong Bits[16];

    /// <summary>Zeroes all bits.</summary>
    public void Zero()
    {
        for (int i = 0; i < 16; i++) Bits[i] = 0;
    }

    /// <summary>Sets a file descriptor in the set.</summary>
    public void Set(int fd)
    {
        if ((uint)fd < 1024)
            Bits[fd / 64] |= 1UL << (fd % 64);
    }

    /// <summary>Clears a file descriptor from the set.</summary>
    public void Clear(int fd)
    {
        if ((uint)fd < 1024)
            Bits[fd / 64] &= ~(1UL << (fd % 64));
    }

    /// <summary>Returns whether a file descriptor is in the set.</summary>
    public readonly bool IsSet(int fd)
    {
        if ((uint)fd >= 1024) return false;
        return (Bits[fd / 64] & (1UL << (fd % 64))) != 0;
    }
}

/// <summary>
/// Signal handling, non-local jumps, I/O multiplexing via select, and environment
/// variable access for a payload context.
/// </summary>
public static unsafe partial class PayloadSignal
{
    private const string Lib = "libc";
    private const string LibKernel = "libkernel";

    /// <summary>Use <c>sa_sigaction</c> handler with siginfo.</summary>
    public const int SaSiginfo = 0x0040;

    /// <summary>Restart interrupted system calls.</summary>
    public const int SaRestart = 0x0002;

    /// <summary>Reset handler to SIG_DFL on delivery.</summary>
    public const int SaResethand = 0x0004;

    /// <summary>Do not generate SIGCHLD on child stop.</summary>
    public const int SaNocldstop = 0x0008;

    /// <summary>
    /// Installs a signal handler with full control over flags and mask.
    /// </summary>
    /// <param name="sig">The signal number (one of the <c>Sig*</c> constants in
    /// <see cref="Process.PayloadProcessControl"/>).</param>
    /// <param name="act">The new action, or null to query the current one.</param>
    /// <param name="oact">On return, the previous action, or null to discard it.</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int sigaction(int sig, FreeBsdSigaction* act, FreeBsdSigaction* oact);

    /// <summary>
    /// Saves the calling environment for later use by <see cref="longjmp"/>.
    /// Returns 0 on the initial call, and the value passed to longjmp on a subsequent
    /// non-local return.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int setjmp(FreeBsdJmpBuf* env);

    /// <summary>
    /// Restores the environment saved by <see cref="setjmp"/>. Execution resumes at the
    /// setjmp call point with <paramref name="val"/> as the return value (or 1 if val is 0).
    /// </summary>
    [LibraryImport(Lib)]
    public static partial void longjmp(FreeBsdJmpBuf* env, int val);

    /// <summary>
    /// Waits for any of the file descriptors in the sets to become ready.
    /// </summary>
    /// <param name="nfds">The highest file descriptor number plus one.</param>
    /// <param name="readfds">Set of descriptors to watch for readability, or null.</param>
    /// <param name="writefds">Set of descriptors to watch for writability, or null.</param>
    /// <param name="exceptfds">Set of descriptors to watch for exceptions, or null.</param>
    /// <param name="timeout">Maximum wait time, or null for infinite.</param>
    /// <returns>The number of ready descriptors, 0 on timeout, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int select(int nfds, FreeBsdFdSet* readfds, FreeBsdFdSet* writefds,
        FreeBsdFdSet* exceptfds, FreeBsdTimeval* timeout);

    /// <summary>
    /// Sets an environment variable. If <paramref name="overwrite"/> is non-zero, an
    /// existing variable is replaced.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int setenv(byte* name, byte* value, int overwrite);

    /// <summary>
    /// Removes an environment variable.
    /// </summary>
    [LibraryImport(Lib)]
    public static partial int unsetenv(byte* name);

    /// <summary>
    /// Creates a new session. The calling process becomes the session leader.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int setsid();

    /// <summary>
    /// Sets the process group identifier of the process <paramref name="pid"/>.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int setpgid(int pid, int pgid);

    /// <summary>
    /// Replaces the current process image with a new one.
    /// </summary>
    /// <param name="path">A NUL-terminated path to the executable.</param>
    /// <param name="argv">A null-terminated array of argument strings.</param>
    /// <param name="envp">A null-terminated array of environment strings.</param>
    [LibraryImport(LibKernel)]
    public static partial int execve(byte* path, byte** argv, byte** envp);
}
