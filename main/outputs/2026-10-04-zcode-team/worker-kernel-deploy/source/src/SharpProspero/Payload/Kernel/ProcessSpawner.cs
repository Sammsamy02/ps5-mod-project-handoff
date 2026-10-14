// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Payload.Process;

namespace SharpProspero.Payload.Kernel;

/// <summary>
/// Process spawner. Creates a new process from a donor executable, elevates its
/// privileges, and prepares it for ELF injection.
/// </summary>
public static unsafe class PayloadProcessSpawner
{
    /// <summary>
    /// Spawns a new process, elevates it, and prepares it for code injection.
    /// </summary>
    /// <param name="executablePath">NUL-terminated path to the donor executable.</param>
    /// <param name="processName">NUL-terminated name to assign to the new process.</param>
    /// <returns>The PID of the spawned process, or -1 on failure.</returns>
    public static int Spawn(byte* executablePath, byte* processName)
    {
        int pid;
        int rc = PayloadProcessControl.sceKernelSpawn(&pid, 0, executablePath, null, null);
        if (rc != 0) return -1;

        // Elevate the spawned process.
        ulong rootvnode = PayloadKernel.GetRootVnode();
        if (rootvnode != 0)
            PayloadKernel.JailbreakByPid(pid, rootvnode);

        PayloadKernel.EscalateCredentials(pid);

        // Set the process name.
        if (processName != null)
        {
            // Write the name through kernel memory.
            var io = new PayloadKernelIo();
            ulong proc = PayloadKernel.WalkAllprocForPid(io, pid);
            if (proc != 0)
            {
                io.Write(proc + (ulong)KernelOffsets.ProcComm, processName, 17);
            }
        }

        return pid;
    }
}
