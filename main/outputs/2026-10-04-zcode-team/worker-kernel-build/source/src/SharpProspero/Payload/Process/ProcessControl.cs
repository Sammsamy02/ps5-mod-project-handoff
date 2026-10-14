// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using System.Runtime.InteropServices;

namespace SharpProspero.Payload.Process;

/// <summary>
/// POSIX process control and signal delivery for a payload context. Wraps <c>kill</c>,
/// <c>getpid</c>, <c>getppid</c> from <c>libkernel</c> and <c>sceKernelGetProcessName</c> from
/// <c>libkernel</c>.
/// </summary>
public static unsafe partial class PayloadProcessControl
{
    private const string LibC = "libc";
    private const string LibKernel = "libkernel";
    private const string LibKernelSys = "libkernel_sys";

    // ---- FreeBSD signal constants ----

    /// <summary>Hangup.</summary>
    public const int SigHup = 1;

    /// <summary>Interrupt (Ctrl-C).</summary>
    public const int SigInt = 2;

    /// <summary>Quit (Ctrl-\).</summary>
    public const int SigQuit = 3;

    /// <summary>Illegal instruction.</summary>
    public const int SigIll = 4;

    /// <summary>Trace/breakpoint trap.</summary>
    public const int SigTrap = 5;

    /// <summary>Abort.</summary>
    public const int SigAbrt = 6;

    /// <summary>Floating-point exception.</summary>
    public const int SigFpe = 8;

    /// <summary>Kill (cannot be caught or ignored).</summary>
    public const int SigKill = 9;

    /// <summary>Bus error.</summary>
    public const int SigBus = 10;

    /// <summary>Segmentation fault.</summary>
    public const int SigSegv = 11;

    /// <summary>Bad system call.</summary>
    public const int SigSys = 12;

    /// <summary>Broken pipe.</summary>
    public const int SigPipe = 13;

    /// <summary>Alarm clock.</summary>
    public const int SigAlrm = 14;

    /// <summary>Termination.</summary>
    public const int SigTerm = 15;

    /// <summary>Stop (cannot be caught or ignored).</summary>
    public const int SigStop = 17;

    /// <summary>Continue after stop.</summary>
    public const int SigCont = 19;

    /// <summary>Child process state changed.</summary>
    public const int SigChld = 20;

    /// <summary>User-defined signal 1.</summary>
    public const int SigUsr1 = 30;

    /// <summary>User-defined signal 2.</summary>
    public const int SigUsr2 = 31;

    /// <summary>
    /// Sends signal <paramref name="sig"/> to the process identified by <paramref name="pid"/>.
    /// </summary>
    /// <param name="pid">The target process identifier.</param>
    /// <param name="sig">The signal number (one of the <c>Sig*</c> constants).</param>
    /// <returns>Zero on success, or -1 on error.</returns>
    [LibraryImport(LibKernel)]
    public static partial int kill(int pid, int sig);

    /// <summary>
    /// Returns the process identifier of the calling process.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int getpid();

    /// <summary>
    /// Returns the parent process identifier of the calling process.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int getppid();

    /// <summary>
    /// Reads the process name (up to 32 bytes, NUL-terminated) for <paramref name="pid"/>
    /// into <paramref name="name"/>.
    /// </summary>
    /// <param name="pid">The target process identifier.</param>
    /// <param name="name">A buffer of at least 32 bytes to receive the name.</param>
    /// <returns>Zero on success, or a negative error code.</returns>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelGetProcessName(int pid, byte* name);

    /// <summary>
    /// Sets the command name of the calling process (visible in process listings).
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelSetProcessName(byte* name);

    /// <summary>
    /// Prepares a process for suspension. Call before <see cref="sceKernelSuspendProcess"/>.
    /// Requires <c>libkernel_sys</c>.
    /// </summary>
    [LibraryImport(LibKernelSys)]
    public static partial int sceKernelPrepareToSuspendProcess(int pid);

    /// <summary>
    /// Suspends a running process. The process must first be prepared with
    /// <see cref="sceKernelPrepareToSuspendProcess"/>. Requires <c>libkernel_sys</c>.
    /// </summary>
    [LibraryImport(LibKernelSys)]
    public static partial int sceKernelSuspendProcess(int pid);

    /// <summary>
    /// Prepares a suspended process for resumption. Call before
    /// <see cref="sceKernelResumeProcess"/>. Requires <c>libkernel_sys</c>.
    /// </summary>
    [LibraryImport(LibKernelSys)]
    public static partial int sceKernelPrepareToResumeProcess(int pid);

    /// <summary>
    /// Resumes a previously suspended process. Requires <c>libkernel_sys</c>.
    /// </summary>
    [LibraryImport(LibKernelSys)]
    public static partial int sceKernelResumeProcess(int pid);

    /// <summary>
    /// Terminates a process by its identifier. Requires <c>libkernel_sys</c>.
    /// </summary>
    /// <param name="pid">The process to terminate.</param>
    /// <param name="ret">On success, receives the process exit code.</param>
    [LibraryImport(LibKernelSys)]
    public static partial int sceKernelTerminateProcess(int pid, int* ret);

    /// <summary>
    /// Spawns a new process from an executable path.
    /// </summary>
    /// <param name="pid">On success, receives the new process identifier.</param>
    /// <param name="dbg">Debug flags (0 for default).</param>
    /// <param name="path">A NUL-terminated path to the executable.</param>
    /// <param name="root">A NUL-terminated root directory path, or null.</param>
    /// <param name="argv">A null-terminated array of argument strings, or null.</param>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelSpawn(int* pid, int dbg, byte* path, byte* root, byte** argv);

    /// <summary>
    /// Creates a JIT-capable shared memory region that can hold executable code.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelJitCreateSharedMemory(nuint addr, nuint length, ulong flags, int* fd);

    /// <summary>
    /// Creates a writable alias of a JIT shared memory region. Write code through the alias,
    /// then execute through the original mapping.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial int sceKernelJitCreateAliasOfSharedMemory(int fd, int flags);

    /// <summary>
    /// Returns the process parameter block pointer for the calling process.
    /// </summary>
    [LibraryImport(LibKernel)]
    public static partial nint sceKernelGetProcParam();
}
