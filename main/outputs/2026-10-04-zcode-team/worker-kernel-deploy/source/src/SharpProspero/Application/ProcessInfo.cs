// SharpProspero - a C# SDK for on-device application modules.
// Copyright (C) 2026 SvenGDK

using SharpProspero.Interop;
using SharpProspero.Interop.Kernel;
using System;
using System.Text;

namespace SharpProspero.Application;

/// <summary>
/// What the running process is and what the system allows it. The descriptor ceiling is the one worth
/// reading before a build decides how many files or sockets to keep open at once; the page size is the
/// granularity every mapping is rounded to.
/// </summary>
public static unsafe class ProcessInfo
{
    /// <summary>The process identifier the system tracks this process by.</summary>
    public static int Id => KernelProcess.getpid();

    /// <summary>The size of a memory page, and the granularity every mapping is rounded to.</summary>
    public static int PageSize => KernelProcess.getpagesize();

    /// <summary>How many descriptors the process may hold open at once.</summary>
    public static int MaximumOpenDescriptors => KernelProcess.getdtablesize();

    /// <summary>
    /// The arguments the process was started with. The first is the module's own path, as it is for
    /// any process.
    /// </summary>
    public static string[] Arguments()
    {
        int count = KernelProcess.getargc();
        if (count <= 0)
            return [];
        byte** argv = KernelProcess.getargv();
        if (argv is null)
            return [];

        var values = new string[count];
        for (int i = 0; i < count; i++)
        {
            byte* arg = argv[i];
            if (arg is null)
            {
                values[i] = string.Empty;
                continue;
            }
            int length = 0;
            while (arg[length] != 0)
                length++;
            values[i] = length == 0 ? string.Empty : Encoding.UTF8.GetString(arg, length);
        }
        return values;
    }

    /// <summary>
    /// A fresh 128-bit identifier drawn by the system, for naming a save slot, a capture or a session
    /// so two runs never collide.
    /// </summary>
    /// <exception cref="ProsperoException">The system refused to draw one.</exception>
    public static Guid NewIdentifier()
    {
        SceKernelUuid uuid = default;
        SceResult.ThrowIfFailed(
            KernelProcess.sceKernelUuidCreate(&uuid), nameof(KernelProcess.sceKernelUuidCreate));
        return ToGuid(uuid);
    }

    /// <summary>
    /// Converts a platform identifier into a <see cref="Guid"/>. Both hold the first three fields in
    /// the processor's own byte order and the rest as plain bytes, so the fields carry across
    /// unchanged.
    /// </summary>
    public static Guid ToGuid(SceKernelUuid uuid)
    {
        Span<byte> node = stackalloc byte[8];
        node[0] = uuid.ClockSequenceHighAndReserved;
        node[1] = uuid.ClockSequenceLow;
        for (int i = 0; i < 6; i++)
            node[2 + i] = uuid.Node[i];
        return new Guid(uuid.TimeLow, uuid.TimeMid, uuid.TimeHighAndVersion,
            node[0], node[1], node[2], node[3], node[4], node[5], node[6], node[7]);
    }
}
